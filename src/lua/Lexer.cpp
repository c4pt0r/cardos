#include "Lexer.h"

#include <cctype>
#include <cstdlib>
#include <unordered_map>

#include "Error.h"

namespace minlua {
namespace {

const std::unordered_map<std::string, Tok>& keywords() {
  static const std::unordered_map<std::string, Tok> kw = {
      {"and", Tok::And}, {"break", Tok::Break}, {"do", Tok::Do},
      {"else", Tok::Else}, {"elseif", Tok::Elseif}, {"end", Tok::End},
      {"false", Tok::False}, {"for", Tok::For}, {"function", Tok::Function},
      {"if", Tok::If}, {"in", Tok::In}, {"local", Tok::Local},
      {"nil", Tok::Nil}, {"not", Tok::Not}, {"or", Tok::Or},
      {"repeat", Tok::Repeat}, {"return", Tok::Return}, {"then", Tok::Then},
      {"true", Tok::True}, {"until", Tok::Until}, {"while", Tok::While},
  };
  return kw;
}

struct Scanner {
  const std::string& s;
  size_t i = 0;
  int line = 1;
  std::vector<Token> out;

  explicit Scanner(const std::string& src) : s(src) {}

  char peek(size_t o = 0) const { return i + o < s.size() ? s[i + o] : '\0'; }
  char advance() { char c = s[i++]; if (c == '\n') line++; return c; }
  bool match(char c) { if (peek() == c) { advance(); return true; } return false; }

  void push(Tok t, const std::string& text = "") {
    out.push_back({t, text, 0, line});
  }

  void skipTrivia() {
    for (;;) {
      char c = peek();
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); continue; }
      if (c == '-' && peek(1) == '-') {
        advance(); advance();
        if (peek() == '[' && peek(1) == '[') {  // block comment --[[ ]]
          advance(); advance();
          while (!(peek() == ']' && peek(1) == ']')) {
            if (peek() == '\0') throw LuaError("unterminated block comment", line);
            advance();
          }
          advance(); advance();
        } else {
          while (peek() != '\n' && peek() != '\0') advance();
        }
        continue;
      }
      break;
    }
  }

  void string(char quote) {
    int startLine = line;
    std::string val;
    while (peek() != quote) {
      char c = peek();
      if (c == '\0' || c == '\n') throw LuaError("unterminated string", startLine);
      if (c == '\\') {
        advance();
        char e = advance();
        switch (e) {
          case 'n': val += '\n'; break;
          case 't': val += '\t'; break;
          case 'r': val += '\r'; break;
          case '\\': val += '\\'; break;
          case '"': val += '"'; break;
          case '\'': val += '\''; break;
          case '0': val += '\0'; break;
          default: val += e; break;
        }
      } else {
        val += advance();
      }
    }
    advance();  // closing quote
    out.push_back({Tok::String, val, 0, startLine});
  }

  void number() {
    size_t start = i;
    bool hex = false;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
      hex = true; advance(); advance();
      while (isxdigit((unsigned char)peek())) advance();
    } else {
      while (isdigit((unsigned char)peek())) advance();
      if (peek() == '.') { advance(); while (isdigit((unsigned char)peek())) advance(); }
      if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (isdigit((unsigned char)peek())) advance();
      }
    }
    std::string lit = s.substr(start, i - start);
    double v = hex ? (double)strtoll(lit.c_str(), nullptr, 16) : strtod(lit.c_str(), nullptr);
    out.push_back({Tok::Number, lit, v, line});
  }

  void name() {
    size_t start = i;
    while (isalnum((unsigned char)peek()) || peek() == '_') advance();
    std::string id = s.substr(start, i - start);
    auto it = keywords().find(id);
    if (it != keywords().end()) out.push_back({it->second, id, 0, line});
    else out.push_back({Tok::Name, id, 0, line});
  }

  void run() {
    for (;;) {
      skipTrivia();
      char c = peek();
      if (c == '\0') break;
      if (isdigit((unsigned char)c) ||
          (c == '.' && isdigit((unsigned char)peek(1)))) { number(); continue; }
      if (isalpha((unsigned char)c) || c == '_') { name(); continue; }
      if (c == '"' || c == '\'') { advance(); string(c); continue; }
      advance();
      switch (c) {
        case '+': push(Tok::Plus); break;
        case '-': push(Tok::Minus); break;
        case '*': push(Tok::Star); break;
        case '/': push(match('/') ? Tok::DSlash : Tok::Slash); break;
        case '%': push(Tok::Percent); break;
        case '^': push(Tok::Caret); break;
        case '#': push(Tok::Hash); break;
        case '(': push(Tok::LParen); break;
        case ')': push(Tok::RParen); break;
        case '{': push(Tok::LBrace); break;
        case '}': push(Tok::RBrace); break;
        case '[': push(Tok::LBracket); break;
        case ']': push(Tok::RBracket); break;
        case ';': push(Tok::Semi); break;
        case ':': push(Tok::Colon); break;
        case ',': push(Tok::Comma); break;
        case '=': push(match('=') ? Tok::Eq : Tok::Assign); break;
        case '~':
          if (match('=')) push(Tok::Ne);
          else throw LuaError("unexpected '~'", line);
          break;
        case '<': push(match('=') ? Tok::Le : Tok::Lt); break;
        case '>': push(match('=') ? Tok::Ge : Tok::Gt); break;
        case '.':
          if (match('.')) push(Tok::Concat);
          else push(Tok::Dot);
          break;
        default: throw LuaError(std::string("unexpected char '") + c + "'", line);
      }
    }
    push(Tok::Eof);
  }
};

}  // namespace

std::vector<Token> lex(const std::string& src) {
  Scanner sc(src);
  sc.run();
  return sc.out;
}

}  // namespace minlua
