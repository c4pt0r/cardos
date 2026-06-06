#include "Parser.h"

#include "Error.h"

namespace minlua {
namespace {

struct P {
  const std::vector<Token>& t;
  size_t i = 0;
  explicit P(const std::vector<Token>& toks) : t(toks) {}

  const Token& peek(size_t o = 0) const {
    size_t k = i + o;
    return k < t.size() ? t[k] : t.back();  // back() is Eof
  }
  const Token& advance() { return t[i++]; }
  bool check(Tok k) const { return peek().type == k; }
  bool match(Tok k) { if (check(k)) { i++; return true; } return false; }
  const Token& expect(Tok k, const char* what) {
    if (!check(k)) throw LuaError(std::string("expected ") + what, peek().line);
    return advance();
  }
  std::string expectName() { return expect(Tok::Name, "name").text; }

  // ---- expressions ----

  static int leftBp(Tok t) {
    switch (t) {
      case Tok::Or: return 1;
      case Tok::And: return 2;
      case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge:
      case Tok::Ne: case Tok::Eq: return 3;
      case Tok::Concat: return 9;
      case Tok::Plus: case Tok::Minus: return 10;
      case Tok::Star: case Tok::Slash: case Tok::DSlash: case Tok::Percent:
        return 11;
      case Tok::Caret: return 14;
      default: return 0;
    }
  }
  static bool rightAssoc(Tok t) { return t == Tok::Concat || t == Tok::Caret; }

  ExprP expr(int minbp = 0) {
    ExprP left = unary();
    for (;;) {
      Tok op = peek().type;
      int lbp = leftBp(op);
      if (lbp == 0 || lbp <= minbp) break;
      int line = advance().line;
      int rbp = rightAssoc(op) ? lbp - 1 : lbp;
      ExprP right = expr(rbp);
      EK k = (op == Tok::And) ? EK::And : (op == Tok::Or) ? EK::Or : EK::Binary;
      auto e = mkExpr(k, line);
      e->op = op; e->a = left; e->b = right;
      left = e;
    }
    return left;
  }

  ExprP unary() {
    Tok op = peek().type;
    if (op == Tok::Not || op == Tok::Minus || op == Tok::Hash) {
      int line = advance().line;
      auto e = mkExpr(EK::Unary, line);
      e->op = op;
      e->a = expr(12);  // unary binds tighter than * but looser than ^
      return e;
    }
    return suffixed();
  }

  std::vector<ExprP> callArgs() {
    std::vector<ExprP> args;
    if (check(Tok::String)) {  // f"str"
      auto e = mkExpr(EK::Str, peek().line); e->str = advance().text;
      args.push_back(e); return args;
    }
    if (check(Tok::LBrace)) { args.push_back(table()); return args; }  // f{...}
    expect(Tok::LParen, "'('");
    if (!check(Tok::RParen)) {
      args.push_back(expr());
      while (match(Tok::Comma)) args.push_back(expr());
    }
    expect(Tok::RParen, "')'");
    return args;
  }

  ExprP suffixed() {
    ExprP e = primary();
    for (;;) {
      if (match(Tok::Dot)) {
        int line = peek().line;
        auto idx = mkExpr(EK::Str, line); idx->str = expectName();
        auto n = mkExpr(EK::Index, line); n->a = e; n->b = idx; e = n;
      } else if (match(Tok::LBracket)) {
        int line = peek().line;
        ExprP k = expr();
        expect(Tok::RBracket, "']'");
        auto n = mkExpr(EK::Index, line); n->a = e; n->b = k; e = n;
      } else if (check(Tok::LParen) || check(Tok::String) || check(Tok::LBrace)) {
        auto n = mkExpr(EK::Call, peek().line);
        n->a = e; n->list = callArgs(); e = n;
      } else if (match(Tok::Colon)) {
        auto n = mkExpr(EK::Method, peek().line);
        n->str = expectName();
        n->a = e; n->list = callArgs(); e = n;
      } else {
        break;
      }
    }
    return e;
  }

  ExprP table() {
    int line = expect(Tok::LBrace, "'{'").line;
    auto e = mkExpr(EK::Table, line);
    while (!check(Tok::RBrace)) {
      Field f;
      if (match(Tok::LBracket)) {                 // [expr] = expr
        f.key = expr();
        expect(Tok::RBracket, "']'");
        expect(Tok::Assign, "'='");
        f.val = expr();
      } else if (check(Tok::Name) && peek(1).type == Tok::Assign) {  // name = expr
        auto key = mkExpr(EK::Str, peek().line); key->str = advance().text;
        advance();  // '='
        f.key = key; f.val = expr();
      } else {                                    // positional
        f.val = expr();
      }
      e->fields.push_back(f);
      if (!match(Tok::Comma) && !match(Tok::Semi)) break;
    }
    expect(Tok::RBrace, "'}'");
    return e;
  }

  std::vector<std::string> paramList() {
    std::vector<std::string> ps;
    expect(Tok::LParen, "'('");
    if (!check(Tok::RParen)) {
      ps.push_back(expectName());
      while (match(Tok::Comma)) ps.push_back(expectName());
    }
    expect(Tok::RParen, "')'");
    return ps;
  }

  ExprP funcBody(int line, bool method) {
    auto e = mkExpr(EK::Func, line);
    e->params = paramList();
    if (method) e->params.insert(e->params.begin(), "self");
    e->body = block();
    expect(Tok::End, "'end'");
    return e;
  }

  ExprP primary() {
    const Token& tk = peek();
    switch (tk.type) {
      case Tok::Nil: advance(); return mkExpr(EK::Nil, tk.line);
      case Tok::True: advance(); return mkExpr(EK::True, tk.line);
      case Tok::False: advance(); return mkExpr(EK::False, tk.line);
      case Tok::Number: { advance(); auto e = mkExpr(EK::Num, tk.line); e->num = tk.num; return e; }
      case Tok::String: { advance(); auto e = mkExpr(EK::Str, tk.line); e->str = tk.text; return e; }
      case Tok::Name: { advance(); auto e = mkExpr(EK::Name, tk.line); e->str = tk.text; return e; }
      case Tok::LParen: {
        advance(); ExprP e = expr(); expect(Tok::RParen, "')'"); return e;
      }
      case Tok::LBrace: return table();
      case Tok::Function: { advance(); return funcBody(tk.line, false); }
      default:
        throw LuaError("unexpected token in expression", tk.line);
    }
  }

  // ---- statements ----

  bool blockEnd() const {
    switch (peek().type) {
      case Tok::End: case Tok::Else: case Tok::Elseif: case Tok::Until:
      case Tok::Eof: return true;
      default: return false;
    }
  }

  Block block() {
    Block b;
    while (!blockEnd()) {
      if (check(Tok::Return)) { b.push_back(returnStat()); break; }
      StmtP s = statement();
      if (s) b.push_back(s);
    }
    return b;
  }

  StmtP returnStat() {
    int line = advance().line;  // 'return'
    auto s = mkStmt(SK::Return, line);
    if (!blockEnd() && !check(Tok::Semi)) {
      s->exprs.push_back(expr());
      while (match(Tok::Comma)) s->exprs.push_back(expr());
    }
    match(Tok::Semi);
    return s;
  }

  StmtP statement() {
    const Token& tk = peek();
    switch (tk.type) {
      case Tok::Semi: advance(); return nullptr;
      case Tok::Break: advance(); return mkStmt(SK::Break, tk.line);
      case Tok::Do: {
        advance(); auto s = mkStmt(SK::Do, tk.line);
        s->body = block(); expect(Tok::End, "'end'"); return s;
      }
      case Tok::While: return whileStat();
      case Tok::Repeat: return repeatStat();
      case Tok::If: return ifStat();
      case Tok::For: return forStat();
      case Tok::Function: return funcStat();
      case Tok::Local: return localStat();
      default: return exprStat();
    }
  }

  StmtP whileStat() {
    int line = advance().line;
    auto s = mkStmt(SK::While, line);
    s->e1 = expr();
    expect(Tok::Do, "'do'");
    s->body = block();
    expect(Tok::End, "'end'");
    return s;
  }

  StmtP repeatStat() {
    int line = advance().line;
    auto s = mkStmt(SK::Repeat, line);
    s->body = block();
    expect(Tok::Until, "'until'");
    s->e1 = expr();
    return s;
  }

  StmtP ifStat() {
    int line = advance().line;
    auto s = mkStmt(SK::If, line);
    s->conds.push_back(expr());
    expect(Tok::Then, "'then'");
    s->blocks.push_back(block());
    while (check(Tok::Elseif)) {
      advance();
      s->conds.push_back(expr());
      expect(Tok::Then, "'then'");
      s->blocks.push_back(block());
    }
    if (match(Tok::Else)) {
      s->hasElse = true;
      s->blocks.push_back(block());
    }
    expect(Tok::End, "'end'");
    return s;
  }

  StmtP forStat() {
    int line = advance().line;  // 'for'
    std::string first = expectName();
    if (check(Tok::Assign)) {  // numeric for
      advance();
      auto s = mkStmt(SK::NumFor, line);
      s->names.push_back(first);
      s->e1 = expr();
      expect(Tok::Comma, "','");
      s->e2 = expr();
      if (match(Tok::Comma)) s->e3 = expr();
      expect(Tok::Do, "'do'");
      s->body = block();
      expect(Tok::End, "'end'");
      return s;
    }
    // generic for: namelist 'in' explist
    auto s = mkStmt(SK::GenFor, line);
    s->names.push_back(first);
    while (match(Tok::Comma)) s->names.push_back(expectName());
    expect(Tok::In, "'in'");
    s->exprs.push_back(expr());
    while (match(Tok::Comma)) s->exprs.push_back(expr());
    expect(Tok::Do, "'do'");
    s->body = block();
    expect(Tok::End, "'end'");
    return s;
  }

  // function Name{.Name}[:Name] funcbody
  StmtP funcStat() {
    int line = advance().line;
    auto target = mkExpr(EK::Name, line);
    target->str = expectName();
    bool method = false;
    while (check(Tok::Dot)) {
      advance();
      auto idx = mkExpr(EK::Str, line); idx->str = expectName();
      auto n = mkExpr(EK::Index, line); n->a = target; n->b = idx; target = n;
    }
    if (match(Tok::Colon)) {
      method = true;
      auto idx = mkExpr(EK::Str, line); idx->str = expectName();
      auto n = mkExpr(EK::Index, line); n->a = target; n->b = idx; target = n;
    }
    ExprP fn = funcBody(line, method);
    auto s = mkStmt(SK::Assign, line);
    s->targets.push_back(target);
    s->exprs.push_back(fn);
    return s;
  }

  StmtP localStat() {
    int line = advance().line;  // 'local'
    if (match(Tok::Function)) {
      std::string name = expectName();
      ExprP fn = funcBody(line, false);
      auto s = mkStmt(SK::Local, line);
      s->names.push_back(name);
      s->exprs.push_back(fn);
      s->hasElse = true;  // reuse flag: predeclare name before RHS (recursion)
      return s;
    }
    auto s = mkStmt(SK::Local, line);
    s->names.push_back(expectName());
    while (match(Tok::Comma)) s->names.push_back(expectName());
    if (match(Tok::Assign)) {
      s->exprs.push_back(expr());
      while (match(Tok::Comma)) s->exprs.push_back(expr());
    }
    return s;
  }

  StmtP exprStat() {
    int line = peek().line;
    ExprP e = suffixed();
    if (check(Tok::Assign) || check(Tok::Comma)) {  // assignment
      auto s = mkStmt(SK::Assign, line);
      s->targets.push_back(e);
      while (match(Tok::Comma)) s->targets.push_back(suffixed());
      expect(Tok::Assign, "'='");
      s->exprs.push_back(expr());
      while (match(Tok::Comma)) s->exprs.push_back(expr());
      return s;
    }
    if (e->kind != EK::Call && e->kind != EK::Method)
      throw LuaError("syntax error (expected statement)", line);
    auto s = mkStmt(SK::ExprStat, line);
    s->exprs.push_back(e);
    return s;
  }
};

}  // namespace

Block parse(const std::vector<Token>& tokens) {
  P p(tokens);
  Block b = p.block();
  if (!p.check(Tok::Eof)) throw LuaError("unexpected trailing tokens", p.peek().line);
  return b;
}

}  // namespace minlua
