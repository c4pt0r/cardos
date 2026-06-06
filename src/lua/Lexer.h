#pragma once
#include <string>
#include <vector>

namespace minlua {

enum class Tok {
  // literals / names
  Number, String, Name,
  // keywords
  And, Break, Do, Else, Elseif, End, False, For, Function, If, In, Local,
  Nil, Not, Or, Repeat, Return, Then, True, Until, While,
  // symbols
  Plus, Minus, Star, Slash, DSlash, Percent, Caret, Hash,
  Eq, Ne, Le, Ge, Lt, Gt, Assign,
  LParen, RParen, LBrace, RBrace, LBracket, RBracket,
  Semi, Colon, Comma, Dot, Concat,
  Eof,
};

struct Token {
  Tok type;
  std::string text;   // Name/String value, or raw for symbols
  double num = 0;     // Number value
  int line = 1;
};

// Throws LuaError (from Lua.h) on malformed input.
std::vector<Token> lex(const std::string& src);

}  // namespace minlua
