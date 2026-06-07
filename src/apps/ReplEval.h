#pragma once
#include <string>

#include "../lua/Error.h"
#include "../lua/Lua.h"
#include "../lua/Parser.h"

// One REPL line against a persistent Lua session. Pure: no Arduino/UI
// includes, so it unit-tests in the native environment.
namespace cardos {

struct ReplResult {
  bool ok = true;
  std::string text;  // expression value ("" for nil/statements) or error
};

inline ReplResult replEval(minlua::Lua& lua, const std::string& line) {
  // Decide expression-vs-statement by parsing only — the trial must not
  // execute anything, or statement side effects would run twice.
  std::string wrapped = "__r = (" + line + ")";
  bool isExpr = true;
  try {
    minlua::parse(minlua::lex(wrapped));
  } catch (const minlua::LuaError&) {
    isExpr = false;
  }
  if (!isExpr) {
    auto r = lua.run(line);
    return {r.ok, r.ok ? "" : r.error};
  }
  auto r = lua.run(wrapped);
  if (!r.ok) return {false, r.error};
  minlua::Value v = lua.getGlobal("__r");
  if (v.type == minlua::Type::Nil) return {true, ""};
  return {true, minlua::Interp::tostr(v)};
}

}  // namespace cardos
