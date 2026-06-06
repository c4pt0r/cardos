#pragma once
#include "Ast.h"
#include "Lexer.h"

namespace minlua {

// Parse a token stream into a top-level block. Throws LuaError on syntax
// errors.
Block parse(const std::vector<Token>& tokens);

}  // namespace minlua
