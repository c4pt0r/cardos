#include "Interp.h"

#include <cmath>
#include <cstdio>

#include "Error.h"

namespace minlua {

void Interp::error(const std::string& msg, int line) {
  throw LuaError(msg, line);
}

std::string Interp::tostr(const Value& v) {
  switch (v.type) {
    case Type::Nil: return "nil";
    case Type::Bool: return v.b ? "true" : "false";
    case Type::String: return v.str;
    case Type::Number: {
      double d = v.num;
      if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 1e15) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)d);
        return buf;
      }
      char buf[40]; snprintf(buf, sizeof(buf), "%.14g", d);
      return buf;
    }
    case Type::Table: { char b[32]; snprintf(b, sizeof(b), "table: %p", (void*)v.table.get()); return b; }
    case Type::Function:
    case Type::Native: { char b[32]; snprintf(b, sizeof(b), "function: %p",
                          v.type == Type::Function ? (void*)v.fn.get() : (void*)v.native.get()); return b; }
  }
  return "nil";
}

bool Interp::tonum(const Value& v, double& out) {
  if (v.type == Type::Number) { out = v.num; return true; }
  if (v.type == Type::String) {
    const char* s = v.str.c_str();
    char* end = nullptr;
    double d = strtod(s, &end);
    if (end == s) return false;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return false;
    out = d; return true;
  }
  return false;
}

Value Interp::index(const Value& obj, const Value& key, int line) {
  if (obj.type != Type::Table)
    error(std::string("attempt to index a ") + obj.typeName() + " value", line);
  return obj.table->get(key);
}

void Interp::setIndex(const Value& obj, const Value& key, const Value& val, int line) {
  if (obj.type != Type::Table)
    error(std::string("attempt to index a ") + obj.typeName() + " value", line);
  if (key.isNil()) error("table index is nil", line);
  obj.table->set(key, val);
}

Value Interp::unop(Tok op, const Value& a, int line) {
  switch (op) {
    case Tok::Minus: {
      double d; if (!tonum(a, d)) error("attempt to perform arithmetic on a " + std::string(a.typeName()) + " value", line);
      return Value::number(-d);
    }
    case Tok::Not: return Value::boolean(!a.truthy());
    case Tok::Hash:
      if (a.type == Type::String) return Value::number((double)a.str.size());
      if (a.type == Type::Table) return Value::number((double)a.table->length());
      error("attempt to get length of a " + std::string(a.typeName()) + " value", line);
    default: error("bad unary op", line);
  }
}

Value Interp::binop(Tok op, const Value& a, const Value& b, int line) {
  auto arith = [&](double x, double y) -> double {
    switch (op) {
      case Tok::Plus: return x + y;
      case Tok::Minus: return x - y;
      case Tok::Star: return x * y;
      case Tok::Slash: return x / y;
      case Tok::Percent: return x - std::floor(x / y) * y;
      case Tok::Caret: return std::pow(x, y);
      case Tok::DSlash: return std::floor(x / y);
      default: return 0;
    }
  };
  switch (op) {
    case Tok::Plus: case Tok::Minus: case Tok::Star: case Tok::Slash:
    case Tok::Percent: case Tok::Caret: case Tok::DSlash: {
      double x, y;
      if (!tonum(a, x) || !tonum(b, y))
        error("attempt to perform arithmetic on a " +
              std::string((a.type == Type::Number || a.type == Type::String) ? b.typeName() : a.typeName()) + " value", line);
      return Value::number(arith(x, y));
    }
    case Tok::Concat: {
      if ((a.type == Type::Table) || (b.type == Type::Table) ||
          a.type == Type::Nil || b.type == Type::Nil ||
          a.type == Type::Bool || b.type == Type::Bool)
        error("attempt to concatenate a " +
              std::string(a.type == Type::Number || a.type == Type::String ? b.typeName() : a.typeName()) + " value", line);
      return Value::string(tostr(a) + tostr(b));
    }
    case Tok::Eq: return Value::boolean(a.equals(b));
    case Tok::Ne: return Value::boolean(!a.equals(b));
    case Tok::Lt: case Tok::Le: case Tok::Gt: case Tok::Ge: {
      if (a.type == Type::Number && b.type == Type::Number) {
        double x = a.num, y = b.num;
        switch (op) { case Tok::Lt: return Value::boolean(x < y);
          case Tok::Le: return Value::boolean(x <= y);
          case Tok::Gt: return Value::boolean(x > y);
          default: return Value::boolean(x >= y); }
      }
      if (a.type == Type::String && b.type == Type::String) {
        const std::string& x = a.str; const std::string& y = b.str;
        switch (op) { case Tok::Lt: return Value::boolean(x < y);
          case Tok::Le: return Value::boolean(x <= y);
          case Tok::Gt: return Value::boolean(x > y);
          default: return Value::boolean(x >= y); }
      }
      error("attempt to compare " + std::string(a.typeName()) + " with " + b.typeName(), line);
    }
    default: error("bad binary op", line);
  }
}

ValueList Interp::call(const Value& fn, const ValueList& args, int line) {
  if (fn.type == Type::Native) return (*fn.native)(*this, args);
  if (fn.type != Type::Function)
    error("attempt to call a " + std::string(fn.typeName()) + " value", line);
  auto env = std::make_shared<Env>(fn.fn->env);
  const auto& params = fn.fn->params;
  for (size_t i = 0; i < params.size(); i++)
    env->define(params[i], i < args.size() ? args[i] : Value::nil());
  Signal sig = exec(fn.fn->body, env);
  if (sig.kind == Signal::Return) return sig.ret;
  return {};
}

ValueList Interp::evalMulti(const ExprP& e, const EnvP& env) {
  if (e->kind == EK::Call || e->kind == EK::Method) {
    Value fn; ValueList args;
    if (e->kind == EK::Method) {
      Value obj = eval(e->a, env);
      fn = index(obj, Value::string(e->str), e->line);
      args.push_back(obj);
    } else {
      fn = eval(e->a, env);
    }
    ValueList rest = evalList(e->list, env);
    for (auto& v : rest) args.push_back(v);
    return call(fn, args, e->line);
  }
  return {eval(e, env)};
}

ValueList Interp::evalList(const std::vector<ExprP>& exprs, const EnvP& env) {
  ValueList out;
  for (size_t i = 0; i < exprs.size(); i++) {
    if (i + 1 == exprs.size()) {
      ValueList tail = evalMulti(exprs[i], env);
      for (auto& v : tail) out.push_back(v);
    } else {
      out.push_back(eval(exprs[i], env));
    }
  }
  return out;
}

Value Interp::eval(const ExprP& e, const EnvP& env) {
  switch (e->kind) {
    case EK::Nil: return Value::nil();
    case EK::True: return Value::boolean(true);
    case EK::False: return Value::boolean(false);
    case EK::Num: return Value::number(e->num);
    case EK::Str: return Value::string(e->str);
    case EK::Name: {
      if (Value* slot = env->find(e->str)) return *slot;
      return globals->get(Value::string(e->str));
    }
    case EK::Index: {
      Value obj = eval(e->a, env);
      Value key = eval(e->b, env);
      return index(obj, key, e->line);
    }
    case EK::Call: case EK::Method: {
      ValueList r = evalMulti(e, env);
      return r.empty() ? Value::nil() : r[0];
    }
    case EK::Func: {
      auto f = std::make_shared<Function>();
      f->params = e->params; f->body = e->body; f->env = env;
      return Value::makeFunction(f);
    }
    case EK::Table: {
      auto t = std::make_shared<Table>();
      int arrayIdx = 1;
      for (size_t i = 0; i < e->fields.size(); i++) {
        const Field& f = e->fields[i];
        if (f.key) {
          t->set(eval(f.key, env), eval(f.val, env));
        } else if (i + 1 == e->fields.size()) {  // last positional expands
          for (auto& v : evalMulti(f.val, env))
            t->set(Value::number(arrayIdx++), v);
        } else {
          t->set(Value::number(arrayIdx++), eval(f.val, env));
        }
      }
      return Value::makeTable(t);
    }
    case EK::Unary: return unop(e->op, eval(e->a, env), e->line);
    case EK::And: { Value a = eval(e->a, env); return a.truthy() ? eval(e->b, env) : a; }
    case EK::Or: { Value a = eval(e->a, env); return a.truthy() ? a : eval(e->b, env); }
    case EK::Binary: return binop(e->op, eval(e->a, env), eval(e->b, env), e->line);
  }
  return Value::nil();
}

void Interp::assign(const ExprP& target, const Value& val, const EnvP& env) {
  if (target->kind == EK::Name) {
    if (Value* slot = env->find(target->str)) { *slot = val; return; }
    globals->set(Value::string(target->str), val);
    return;
  }
  if (target->kind == EK::Index) {
    Value obj = eval(target->a, env);
    Value key = eval(target->b, env);
    setIndex(obj, key, val, target->line);
    return;
  }
  error("cannot assign to this expression", target->line);
}

Interp::Signal Interp::exec(const Block& block, const EnvP& env) {
  for (const StmtP& s : block) {
    Signal sig = execStmt(s, env);
    if (sig.kind != Signal::None) return sig;
  }
  return {};
}

Interp::Signal Interp::execStmt(const StmtP& s, const EnvP& env) {
  switch (s->kind) {
    case SK::ExprStat: evalMulti(s->exprs[0], env); return {};
    case SK::Local: {
      if (s->hasElse) {  // local function: predeclare for recursion
        env->define(s->names[0], Value::nil());
        env->vars[s->names[0]] = eval(s->exprs[0], env);
        return {};
      }
      ValueList vals = evalList(s->exprs, env);
      for (size_t i = 0; i < s->names.size(); i++)
        env->define(s->names[i], i < vals.size() ? vals[i] : Value::nil());
      return {};
    }
    case SK::Assign: {
      ValueList vals = evalList(s->exprs, env);
      for (size_t i = 0; i < s->targets.size(); i++)
        assign(s->targets[i], i < vals.size() ? vals[i] : Value::nil(), env);
      return {};
    }
    case SK::Do: {
      auto inner = std::make_shared<Env>(env);
      return exec(s->body, inner);
    }
    case SK::If: {
      for (size_t i = 0; i < s->conds.size(); i++) {
        if (eval(s->conds[i], env).truthy()) {
          auto inner = std::make_shared<Env>(env);
          return exec(s->blocks[i], inner);
        }
      }
      if (s->hasElse) {
        auto inner = std::make_shared<Env>(env);
        return exec(s->blocks.back(), inner);
      }
      return {};
    }
    case SK::While: {
      while (eval(s->e1, env).truthy()) {
        auto inner = std::make_shared<Env>(env);
        Signal sig = exec(s->body, inner);
        if (sig.kind == Signal::Break) break;
        if (sig.kind == Signal::Return) return sig;
      }
      return {};
    }
    case SK::Repeat: {
      for (;;) {
        auto inner = std::make_shared<Env>(env);
        Signal sig = exec(s->body, inner);
        if (sig.kind == Signal::Break) break;
        if (sig.kind == Signal::Return) return sig;
        if (eval(s->e1, inner).truthy()) break;  // until sees loop locals
      }
      return {};
    }
    case SK::NumFor: {
      double a, b, st = 1;
      Value va = eval(s->e1, env), vb = eval(s->e2, env);
      if (!tonum(va, a) || !tonum(vb, b)) error("'for' initial/limit must be numbers", s->line);
      if (s->e3) { Value vc = eval(s->e3, env); if (!tonum(vc, st)) error("'for' step must be a number", s->line); }
      if (st == 0) error("'for' step is zero", s->line);
      for (double i = a; (st > 0) ? (i <= b) : (i >= b); i += st) {
        auto inner = std::make_shared<Env>(env);
        inner->define(s->names[0], Value::number(i));
        Signal sig = exec(s->body, inner);
        if (sig.kind == Signal::Break) break;
        if (sig.kind == Signal::Return) return sig;
      }
      return {};
    }
    case SK::GenFor: {
      ValueList it = evalList(s->exprs, env);
      Value iter = it.size() > 0 ? it[0] : Value::nil();
      Value state = it.size() > 1 ? it[1] : Value::nil();
      Value ctrl = it.size() > 2 ? it[2] : Value::nil();
      for (;;) {
        ValueList res = call(iter, {state, ctrl}, s->line);
        if (res.empty() || res[0].isNil()) break;
        ctrl = res[0];
        auto inner = std::make_shared<Env>(env);
        for (size_t i = 0; i < s->names.size(); i++)
          inner->define(s->names[i], i < res.size() ? res[i] : Value::nil());
        Signal sig = exec(s->body, inner);
        if (sig.kind == Signal::Break) break;
        if (sig.kind == Signal::Return) return sig;
      }
      return {};
    }
    case SK::Return: { Signal sig; sig.kind = Signal::Return; sig.ret = evalList(s->exprs, env); return sig; }
    case SK::Break: { Signal sig; sig.kind = Signal::Break; return sig; }
  }
  return {};
}

void Interp::run(const Block& b) {
  auto env = std::make_shared<Env>();
  exec(b, env);
}

}  // namespace minlua
