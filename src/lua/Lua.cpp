#include "Lua.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "Error.h"
#include "Lexer.h"
#include "Parser.h"

namespace minlua {
namespace {

Value arg(const ValueList& a, size_t i) {
  return i < a.size() ? a[i] : Value::nil();
}
double argNum(Interp& in, const ValueList& a, size_t i, const char* fn) {
  double d;
  if (!Interp::tonum(arg(a, i), d)) in.error(std::string(fn) + ": expected number");
  return d;
}
std::string argStr(Interp& in, const ValueList& a, size_t i, const char* fn) {
  Value v = arg(a, i);
  if (v.type == Type::String) return v.str;
  if (v.type == Type::Number) return Interp::tostr(v);
  in.error(std::string(fn) + ": expected string");
}

// next(t, key): walk the ordered map (used by pairs).
ValueList builtinNext(Interp& in, const ValueList& a) {
  Value tv = arg(a, 0);
  if (tv.type != Type::Table) in.error("next: expected table");
  auto& m = tv.table->entries;
  Value key = arg(a, 1);
  if (key.isNil()) {
    if (m.empty()) return {Value::nil()};
    return {m.begin()->first, m.begin()->second};
  }
  auto it = m.find(key);
  if (it == m.end()) return {Value::nil()};
  ++it;
  if (it == m.end()) return {Value::nil()};
  return {it->first, it->second};
}

std::string formatOne(const std::string& spec, Interp& in, const Value& v) {
  char out[128];
  char conv = spec.back();
  if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'X' ||
      conv == 'o' || conv == 'c') {
    double d; if (!Interp::tonum(v, d)) in.error("format: expected number");
    std::string s = spec; s.insert(s.size() - 1, "ll");  // long long
    snprintf(out, sizeof(out), s.c_str(), (long long)d);
  } else if (conv == 'f' || conv == 'g' || conv == 'G' || conv == 'e' || conv == 'E') {
    double d; if (!Interp::tonum(v, d)) in.error("format: expected number");
    snprintf(out, sizeof(out), spec.c_str(), d);
  } else if (conv == 's') {
    snprintf(out, sizeof(out), spec.c_str(), Interp::tostr(v).c_str());
  } else {
    in.error(std::string("format: unsupported %") + conv);
  }
  return out;
}

void installString(Interp& in) {
  auto t = std::make_shared<Table>();
  auto set = [&](const char* n, NativeFn f) { t->set(Value::string(n), Value::makeNative(std::move(f))); };

  set("len", [](Interp& in, const ValueList& a) -> ValueList {
    return {Value::number((double)argStr(in, a, 0, "len").size())};
  });
  set("upper", [](Interp& in, const ValueList& a) -> ValueList {
    std::string s = argStr(in, a, 0, "upper");
    for (char& c : s) c = toupper((unsigned char)c);
    return {Value::string(s)};
  });
  set("lower", [](Interp& in, const ValueList& a) -> ValueList {
    std::string s = argStr(in, a, 0, "lower");
    for (char& c : s) c = tolower((unsigned char)c);
    return {Value::string(s)};
  });
  set("rep", [](Interp& in, const ValueList& a) -> ValueList {
    std::string s = argStr(in, a, 0, "rep");
    int n = (int)argNum(in, a, 1, "rep");
    std::string out;
    for (int i = 0; i < n; i++) out += s;
    return {Value::string(out)};
  });
  set("sub", [](Interp& in, const ValueList& a) -> ValueList {
    std::string s = argStr(in, a, 0, "sub");
    int len = (int)s.size();
    int i = (int)argNum(in, a, 1, "sub");
    int j = a.size() > 2 && !arg(a, 2).isNil() ? (int)argNum(in, a, 2, "sub") : -1;
    if (i < 0) i = len + i + 1; if (i < 1) i = 1;
    if (j < 0) j = len + j + 1; if (j > len) j = len;
    if (i > j) return {Value::string("")};
    return {Value::string(s.substr(i - 1, j - i + 1))};
  });
  set("find", [](Interp& in, const ValueList& a) -> ValueList {
    std::string s = argStr(in, a, 0, "find");
    std::string pat = argStr(in, a, 1, "find");
    int init = a.size() > 2 && !arg(a, 2).isNil() ? (int)argNum(in, a, 2, "find") : 1;
    if (init < 1) init = 1;
    size_t pos = s.find(pat, init - 1);   // plain substring only
    if (pos == std::string::npos) return {Value::nil()};
    return {Value::number((double)(pos + 1)), Value::number((double)(pos + pat.size()))};
  });
  set("format", [](Interp& in, const ValueList& a) -> ValueList {
    std::string f = argStr(in, a, 0, "format");
    std::string out;
    size_t argi = 1;
    for (size_t i = 0; i < f.size(); i++) {
      if (f[i] != '%') { out += f[i]; continue; }
      if (f[i + 1] == '%') { out += '%'; i++; continue; }
      size_t j = i + 1;
      while (j < f.size() && !strchr("diuxXofgGeEsc", f[j])) j++;
      std::string spec = f.substr(i, j - i + 1);
      out += formatOne(spec, in, arg(a, argi++));
      i = j;
    }
    return {Value::string(out)};
  });

  in.setGlobal("string", Value::makeTable(t));
}

void installMath(Interp& in) {
  auto t = std::make_shared<Table>();
  auto set = [&](const char* n, NativeFn f) { t->set(Value::string(n), Value::makeNative(std::move(f))); };
  auto un = [&](const char* n, double (*f)(double)) {
    set(n, [f, n](Interp& in, const ValueList& a) -> ValueList {
      return {Value::number(f(argNum(in, a, 0, n)))};
    });
  };
  un("floor", [](double d) { return std::floor(d); });
  un("ceil", [](double d) { return std::ceil(d); });
  un("abs", [](double d) { return std::fabs(d); });
  un("sqrt", [](double d) { return std::sqrt(d); });
  un("sin", [](double d) { return std::sin(d); });
  un("cos", [](double d) { return std::cos(d); });
  set("max", [](Interp& in, const ValueList& a) -> ValueList {
    double m = argNum(in, a, 0, "max");
    for (size_t i = 1; i < a.size(); i++) m = std::fmax(m, argNum(in, a, i, "max"));
    return {Value::number(m)};
  });
  set("min", [](Interp& in, const ValueList& a) -> ValueList {
    double m = argNum(in, a, 0, "min");
    for (size_t i = 1; i < a.size(); i++) m = std::fmin(m, argNum(in, a, i, "min"));
    return {Value::number(m)};
  });
  set("fmod", [](Interp& in, const ValueList& a) -> ValueList {
    return {Value::number(std::fmod(argNum(in, a, 0, "fmod"), argNum(in, a, 1, "fmod")))};
  });
  set("random", [](Interp& in, const ValueList& a) -> ValueList {
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    if (a.empty()) return {Value::number(r)};
    int lo = 1, hi;
    if (a.size() == 1) { hi = (int)argNum(in, a, 0, "random"); }
    else { lo = (int)argNum(in, a, 0, "random"); hi = (int)argNum(in, a, 1, "random"); }
    return {Value::number((double)(lo + (int)(r * (hi - lo + 1))))};
  });
  set("randomseed", [](Interp& in, const ValueList& a) -> ValueList {
    srand((unsigned)(a.empty() ? 0 : argNum(in, a, 0, "randomseed")));
    return {};
  });
  t->set(Value::string("pi"), Value::number(M_PI));
  t->set(Value::string("huge"), Value::number(HUGE_VAL));
  in.setGlobal("math", Value::makeTable(t));
}

void installTable(Interp& in) {
  auto t = std::make_shared<Table>();
  auto set = [&](const char* n, NativeFn f) { t->set(Value::string(n), Value::makeNative(std::move(f))); };
  set("insert", [](Interp& in, const ValueList& a) -> ValueList {
    Value tv = arg(a, 0);
    if (tv.type != Type::Table) in.error("insert: expected table");
    int n = tv.table->length();
    if (a.size() <= 2) {
      tv.table->set(Value::number(n + 1), arg(a, 1));
    } else {
      int pos = (int)argNum(in, a, 1, "insert");
      for (int i = n; i >= pos; i--)
        tv.table->set(Value::number(i + 1), tv.table->get(Value::number(i)));
      tv.table->set(Value::number(pos), arg(a, 2));
    }
    return {};
  });
  set("remove", [](Interp& in, const ValueList& a) -> ValueList {
    Value tv = arg(a, 0);
    if (tv.type != Type::Table) in.error("remove: expected table");
    int n = tv.table->length();
    if (n == 0) return {Value::nil()};
    int pos = a.size() > 1 ? (int)argNum(in, a, 1, "remove") : n;
    Value removed = tv.table->get(Value::number(pos));
    for (int i = pos; i < n; i++)
      tv.table->set(Value::number(i), tv.table->get(Value::number(i + 1)));
    tv.table->set(Value::number(n), Value::nil());
    return {removed};
  });
  set("concat", [](Interp& in, const ValueList& a) -> ValueList {
    Value tv = arg(a, 0);
    if (tv.type != Type::Table) in.error("concat: expected table");
    std::string sep = a.size() > 1 && !arg(a, 1).isNil() ? argStr(in, a, 1, "concat") : "";
    int i = a.size() > 2 ? (int)argNum(in, a, 2, "concat") : 1;
    int j = a.size() > 3 ? (int)argNum(in, a, 3, "concat") : tv.table->length();
    std::string out;
    for (int k = i; k <= j; k++) {
      if (k > i) out += sep;
      out += Interp::tostr(tv.table->get(Value::number(k)));
    }
    return {Value::string(out)};
  });
  in.setGlobal("table", Value::makeTable(t));
}

}  // namespace

Lua::Lua() {
  Interp& in = in_;

  registerFn("print", [this](Interp& in, const ValueList& a) -> ValueList {
    std::string line;
    for (size_t i = 0; i < a.size(); i++) {
      if (i) line += "\t";
      line += Interp::tostr(a[i]);
    }
    if (onPrint) onPrint(line);
    else { out_ += line; out_ += "\n"; }
    return {};
  });
  registerFn("tostring", [](Interp& in, const ValueList& a) -> ValueList {
    return {Value::string(Interp::tostr(arg(a, 0)))};
  });
  registerFn("tonumber", [](Interp& in, const ValueList& a) -> ValueList {
    double d; if (Interp::tonum(arg(a, 0), d)) return {Value::number(d)};
    return {Value::nil()};
  });
  registerFn("type", [](Interp& in, const ValueList& a) -> ValueList {
    return {Value::string(arg(a, 0).typeName())};
  });
  registerFn("assert", [](Interp& in, const ValueList& a) -> ValueList {
    if (!arg(a, 0).truthy())
      in.error(a.size() > 1 ? Interp::tostr(a[1]) : "assertion failed!");
    return a;
  });
  registerFn("error", [](Interp& in, const ValueList& a) -> ValueList {
    in.error(arg(a, 0).type == Type::String ? arg(a, 0).str : Interp::tostr(arg(a, 0)));
  });
  registerFn("next", builtinNext);
  registerFn("pairs", [](Interp& in, const ValueList& a) -> ValueList {
    return {Value::makeNative(builtinNext), arg(a, 0), Value::nil()};
  });
  registerFn("ipairs", [](Interp& in, const ValueList& a) -> ValueList {
    if (arg(a, 0).type != Type::Table) in.error("ipairs: expected table");
    NativeFn iter = [](Interp& in, const ValueList& a) -> ValueList {
      Value tv = arg(a, 0);
      if (tv.type != Type::Table) in.error("ipairs: expected table");
      int i = (int)arg(a, 1).num + 1;
      Value v = tv.table->get(Value::number(i));
      if (v.isNil()) return {Value::nil()};
      return {Value::number(i), v};
    };
    return {Value::makeNative(iter), arg(a, 0), Value::number(0)};
  });
  registerFn("pcall", [](Interp& in, const ValueList& a) -> ValueList {
    if (a.empty() || !a[0].isCallable()) return {Value::boolean(false), Value::string("not callable")};
    ValueList callArgs(a.begin() + 1, a.end());
    try {
      ValueList r = in.call(a[0], callArgs);
      ValueList out = {Value::boolean(true)};
      for (auto& v : r) out.push_back(v);
      return out;
    } catch (const LuaError& e) {
      return {Value::boolean(false), Value::string(e.what())};
    }
  });

  installString(in);
  installMath(in);
  installTable(in);
}

Lua::Result Lua::run(const std::string& src) {
  try {
    Block b = parse(lex(src));
    in_.run(b);
    return {true, ""};
  } catch (const LuaError& e) {
    std::string msg = e.what();
    if (e.line) msg = "line " + std::to_string(e.line) + ": " + msg;
    return {false, msg};
  }
}

Lua::Result Lua::callGlobal(const std::string& name, const ValueList& args, ValueList* out) {
  Value fn = getGlobal(name);
  if (!fn.isCallable()) return {false, name + " is not a function"};
  try {
    ValueList r = in_.call(fn, args);
    if (out) *out = r;
    return {true, ""};
  } catch (const LuaError& e) {
    return {false, std::string(e.what())};
  }
}

}  // namespace minlua
