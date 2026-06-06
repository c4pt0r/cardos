#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Minimal Lua-compatible value model. Pure C++17 — no Arduino/M5 includes,
// so the whole interpreter is host-testable.
namespace minlua {

struct Table;
struct Function;
class Interp;
struct Value;

using ValueList = std::vector<Value>;
// A native (C++) function: receives its arguments, returns its results.
using NativeFn = std::function<ValueList(Interp&, const ValueList&)>;

enum class Type : uint8_t { Nil, Bool, Number, String, Table, Function, Native };

struct Value {
  Type type = Type::Nil;
  bool b = false;
  double num = 0.0;
  std::string str;                       // for String
  std::shared_ptr<Table> table;          // for Table
  std::shared_ptr<Function> fn;          // for Function (Lua closure)
  std::shared_ptr<NativeFn> native;      // for Native

  Value() = default;
  static Value nil() { return Value(); }
  static Value boolean(bool v) { Value x; x.type = Type::Bool; x.b = v; return x; }
  static Value number(double v) { Value x; x.type = Type::Number; x.num = v; return x; }
  static Value string(std::string v) {
    Value x; x.type = Type::String; x.str = std::move(v); return x;
  }
  static Value makeTable(std::shared_ptr<Table> t) {
    Value x; x.type = Type::Table; x.table = std::move(t); return x;
  }
  static Value makeFunction(std::shared_ptr<Function> f) {
    Value x; x.type = Type::Function; x.fn = std::move(f); return x;
  }
  static Value makeNative(NativeFn f) {
    Value x; x.type = Type::Native;
    x.native = std::make_shared<NativeFn>(std::move(f));
    return x;
  }

  bool isNil() const { return type == Type::Nil; }
  bool isCallable() const { return type == Type::Function || type == Type::Native; }
  // Lua truthiness: only nil and false are falsey.
  bool truthy() const { return !(type == Type::Nil || (type == Type::Bool && !b)); }

  // Raw equality (no metamethods). Tables/functions compare by identity.
  bool equals(const Value& o) const {
    if (type != o.type) return false;
    switch (type) {
      case Type::Nil: return true;
      case Type::Bool: return b == o.b;
      case Type::Number: return num == o.num;
      case Type::String: return str == o.str;
      case Type::Table: return table == o.table;
      case Type::Function: return fn == o.fn;
      case Type::Native: return native == o.native;
    }
    return false;
  }

  // Total order so Value can key a std::map (orders by type, then value;
  // reference types by pointer identity).
  bool operator<(const Value& o) const {
    if (type != o.type) return (uint8_t)type < (uint8_t)o.type;
    switch (type) {
      case Type::Nil: return false;
      case Type::Bool: return b < o.b;
      case Type::Number: return num < o.num;
      case Type::String: return str < o.str;
      case Type::Table: return table.get() < o.table.get();
      case Type::Function: return fn.get() < o.fn.get();
      case Type::Native: return native.get() < o.native.get();
    }
    return false;
  }

  const char* typeName() const {
    switch (type) {
      case Type::Nil: return "nil";
      case Type::Bool: return "boolean";
      case Type::Number: return "number";
      case Type::String: return "string";
      case Type::Table: return "table";
      case Type::Function:
      case Type::Native: return "function";
    }
    return "nil";
  }
};

// Lua table: ordered map keyed by Value. Integer-keyed entries from 1..n
// form the "array" used by # and ipairs.
struct Table {
  std::map<Value, Value> entries;

  Value get(const Value& key) const {
    auto it = entries.find(key);
    return it == entries.end() ? Value::nil() : it->second;
  }
  void set(const Value& key, const Value& val) {
    if (key.isNil()) return;
    if (val.isNil()) entries.erase(key);
    else entries[key] = val;
  }
  // Border length: largest n such that t[1..n] are all non-nil.
  int length() const {
    int n = 0;
    while (entries.count(Value::number(n + 1))) n++;
    return n;
  }
};

}  // namespace minlua
