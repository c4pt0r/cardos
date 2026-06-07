#include "LuaBindings.h"

#include <Arduino.h>

#include "../lua/Interp.h"
#include "../sdk/Audio.h"
#include "../sdk/Fs.h"
#include "../sdk/Http.h"

using namespace minlua;

namespace cardos {

void installPlatformBindings(const std::shared_ptr<Table>& tbl) {
  auto set = [&](const char* n, NativeFn f) {
    tbl->set(Value::string(n), Value::makeNative(std::move(f)));
  };

  set("log", [](Interp&, const ValueList& a) -> ValueList {
    Serial.printf("[lua] %s\n", a.empty() ? "" : Interp::tostr(a[0]).c_str());
    return {};
  });
  set("millis", [](Interp&, const ValueList&) -> ValueList {
    return {Value::number((double)millis())};
  });
  set("fs_read", [](Interp&, const ValueList& a) -> ValueList {
    if (a.empty() || a[0].type != Type::String) return {Value::nil()};
    std::string s = cardos::fs::readFile(a[0].str);
    return {Value::string(s)};
  });
  set("fs_write", [](Interp&, const ValueList& a) -> ValueList {
    if (a.size() < 2 || a[0].type != Type::String) return {Value::boolean(false)};
    return {Value::boolean(cardos::fs::writeFile(a[0].str, Interp::tostr(a[1])))};
  });
  set("fs_list", [](Interp&, const ValueList& a) -> ValueList {
    auto t = std::make_shared<Table>();
    if (!a.empty() && a[0].type == Type::String) {
      int i = 1;
      for (auto& e : cardos::fs::list(a[0].str))
        t->set(Value::number(i++), Value::string(e.name));
    }
    return {Value::makeTable(t)};
  });
  set("http_get", [](Interp&, const ValueList& a) -> ValueList {
    if (a.empty() || a[0].type != Type::String)
      return {Value::number(-1), Value::string("bad url")};
    cardos::http::Response r = cardos::http::get(a[0].str);
    return {Value::number(r.status), Value::string(r.status > 0 ? r.body : r.error)};
  });
  set("record_start", [](Interp&, const ValueList& a) -> ValueList {
    if (a.empty() || a[0].type != Type::String) return {Value::boolean(false)};
    return {Value::boolean(cardos::audio::startToWav(a[0].str))};
  });
  set("record_stop", [](Interp&, const ValueList&) -> ValueList {
    cardos::audio::stop(); return {};
  });
  set("level", [](Interp&, const ValueList&) -> ValueList {
    return {Value::number(cardos::audio::level())};
  });
}

}  // namespace cardos
