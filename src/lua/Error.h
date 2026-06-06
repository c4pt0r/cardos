#pragma once
#include <stdexcept>
#include <string>

namespace minlua {

// Thrown internally by the lexer/parser/interpreter; caught at the facade
// (Lua::run) and turned into a {ok=false, error} result. Also produced by
// the Lua `error()` builtin.
struct LuaError : std::runtime_error {
  int line;
  explicit LuaError(const std::string& msg, int line_ = 0)
      : std::runtime_error(msg), line(line_) {}
};

}  // namespace minlua
