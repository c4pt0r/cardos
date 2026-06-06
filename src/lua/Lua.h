#pragma once
#include <functional>
#include <string>

#include "Interp.h"
#include "Value.h"

namespace minlua {

// Facade over the lexer/parser/interpreter plus a trimmed standard library.
// All error paths are caught and returned as Result{ok=false, error}.
class Lua {
 public:
  struct Result { bool ok = true; std::string error; };

  Lua();  // installs the standard library

  // Lex + parse + execute a chunk at global scope.
  Result run(const std::string& src);

  Interp& interp() { return in_; }
  void setGlobal(const std::string& n, Value v) { in_.setGlobal(n, std::move(v)); }
  Value getGlobal(const std::string& n) { return in_.getGlobal(n); }
  void registerFn(const std::string& n, NativeFn f) {
    in_.setGlobal(n, Value::makeNative(std::move(f)));
  }
  bool hasFunction(const std::string& n) { return getGlobal(n).isCallable(); }

  // Call a global function; results (if any) returned via `out`.
  Result callGlobal(const std::string& name, const ValueList& args,
                    ValueList* out = nullptr);

  // print() sink; defaults to appending "<line>\n" to output().
  std::function<void(const std::string&)> onPrint;
  const std::string& output() const { return out_; }
  void clearOutput() { out_.clear(); }

 private:
  Interp in_;
  std::string out_;
};

}  // namespace minlua
