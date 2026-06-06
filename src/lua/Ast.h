#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Lexer.h"

namespace minlua {

struct Expr;
struct Stmt;
using ExprP = std::shared_ptr<Expr>;
using StmtP = std::shared_ptr<Stmt>;
using Block = std::vector<StmtP>;

enum class EK {
  Nil, True, False, Num, Str, Name,
  Index,    // a[b]  (a.f is Index with b = Str "f")
  Call,     // a(list...)
  Method,   // a:str(list...)
  Func,     // function(params) body end
  Table,    // {fields}
  Unary,    // op a   (Minus / Not / Hash)
  Binary,   // a op b (arithmetic / compare / concat)
  And, Or,  // short-circuit
};

struct Field {
  ExprP key;   // null => positional array item
  ExprP val;
};

struct Expr {
  EK kind;
  int line = 0;
  Tok op = Tok::Eof;            // Unary/Binary operator
  double num = 0;              // Num
  std::string str;            // Str / Name / Method name
  ExprP a, b;                 // operands / object / index
  std::vector<ExprP> list;    // Call/Method args
  std::vector<std::string> params;  // Func
  Block body;                 // Func
  std::vector<Field> fields;  // Table
};

enum class SK {
  Local, Assign, ExprStat, If, While, NumFor, GenFor, Repeat, Return, Break, Do,
};

struct Stmt {
  SK kind;
  int line = 0;
  std::vector<std::string> names;   // Local / GenFor names; NumFor var = names[0]
  std::vector<ExprP> targets;       // Assign targets (Name or Index)
  std::vector<ExprP> exprs;         // rhs / Return exprs / GenFor iterator exprs
  ExprP e1, e2, e3;                 // While cond | NumFor start/stop/step | Repeat cond
  std::vector<ExprP> conds;         // If conditions
  std::vector<Block> blocks;        // If branch bodies (one per cond, +1 if hasElse)
  bool hasElse = false;
  Block body;                       // While/For/Repeat/Do
};

inline ExprP mkExpr(EK k, int line) {
  auto e = std::make_shared<Expr>();
  e->kind = k; e->line = line; return e;
}
inline StmtP mkStmt(SK k, int line) {
  auto s = std::make_shared<Stmt>();
  s->kind = k; s->line = line; return s;
}

}  // namespace minlua
