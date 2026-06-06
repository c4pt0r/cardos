#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Ast.h"
#include "Value.h"

namespace minlua {

// Lexical scope. Locals/upvalues live here; globals live in a separate
// table so closures capture by reference to the defining Env.
struct Env {
  std::unordered_map<std::string, Value> vars;
  std::shared_ptr<Env> parent;
  explicit Env(std::shared_ptr<Env> p = nullptr) : parent(std::move(p)) {}

  Value* find(const std::string& n) {
    for (Env* e = this; e; e = e->parent.get()) {
      auto it = e->vars.find(n);
      if (it != e->vars.end()) return &it->second;
    }
    return nullptr;
  }
  void define(const std::string& n, Value v) { vars[n] = std::move(v); }
};
using EnvP = std::shared_ptr<Env>;

// A Lua closure: parameter names, body, and the scope it was defined in.
struct Function {
  std::vector<std::string> params;
  Block body;
  EnvP env;
};

class Interp {
 public:
  std::shared_ptr<Table> globals = std::make_shared<Table>();

  void setGlobal(const std::string& n, Value v) {
    globals->set(Value::string(n), std::move(v));
  }
  Value getGlobal(const std::string& n) {
    return globals->get(Value::string(n));
  }

  void run(const Block& b);                                   // global scope
  ValueList call(const Value& fn, const ValueList& args, int line = 0);

  [[noreturn]] void error(const std::string& msg, int line = 0);

  static std::string tostr(const Value& v);
  static bool tonum(const Value& v, double& out);

 private:
  struct Signal {
    enum K { None, Break, Return } kind = None;
    ValueList ret;
  };

  Signal exec(const Block&, const EnvP&);
  Signal execStmt(const StmtP&, const EnvP&);
  Value eval(const ExprP&, const EnvP&);
  ValueList evalMulti(const ExprP&, const EnvP&);
  ValueList evalList(const std::vector<ExprP>&, const EnvP&);
  void assign(const ExprP& target, const Value&, const EnvP&);

  Value index(const Value& obj, const Value& key, int line);
  void setIndex(const Value& obj, const Value& key, const Value& val, int line);
  Value binop(Tok op, const Value&, const Value&, int line);
  Value unop(Tok op, const Value&, int line);
};

}  // namespace minlua
