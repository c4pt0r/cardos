#include "ScriptApp.h"

#include <M5Cardputer.h>

#include "../sdk/Audio.h"
#include "../sdk/Fs.h"
#include "../sdk/Http.h"
#include "../ui/Theme.h"

using namespace minlua;

namespace {
std::string baseName(const std::string& path) {
  size_t s = path.find_last_of('/');
  return s == std::string::npos ? path : path.substr(s + 1);
}
uint16_t colorArg(const ValueList& a, size_t i, uint16_t def) {
  return (i < a.size() && a[i].type == Type::Number) ? (uint16_t)a[i].num : def;
}
int numArg(const ValueList& a, size_t i) {
  return (i < a.size() && a[i].type == Type::Number) ? (int)a[i].num : 0;
}
}  // namespace

ScriptApp::ScriptApp(const std::string& path) : path_(path) {
  title_ = baseName(path);
  registerBindings();

  std::string src = cardos::fs::readFile(path);
  if (src.empty()) {
    fail("load", "empty or unreadable: " + path);
    return;
  }
  Lua::Result r = lua_.run(src);
  if (!r.ok) { fail("parse", r.error); return; }

  Value t = lua_.getGlobal("title");
  if (t.type == Type::String && !t.str.empty()) title_ = t.str;
}

void ScriptApp::fail(const std::string& where, const std::string& msg) {
  errored_ = true;
  error_ = where + ": " + msg;
  Serial.printf("[lua] %s\n", error_.c_str());
}

void ScriptApp::registerBindings() {
  auto tbl = std::make_shared<Table>();
  auto set = [&](const char* n, NativeFn f) {
    tbl->set(Value::string(n), Value::makeNative(std::move(f)));
  };
  auto color = [&](const char* n, uint16_t c) {
    tbl->set(Value::string(n), Value::number((double)c));
  };

  // ---- drawing (valid only during render) ----
  set("text", [this](Interp&, const ValueList& a) -> ValueList {
    if (canvas_) {
      canvas_->setFont(theme::font());
      canvas_->setTextColor(colorArg(a, 3, theme::kFg));
      canvas_->setCursor(numArg(a, 0), numArg(a, 1));
      canvas_->print(a.size() > 2 ? Interp::tostr(a[2]).c_str() : "");
    }
    return {};
  });
  set("rect", [this](Interp&, const ValueList& a) -> ValueList {
    if (canvas_)
      canvas_->drawRect(numArg(a, 0), numArg(a, 1), numArg(a, 2), numArg(a, 3),
                        colorArg(a, 4, theme::kFg));
    return {};
  });
  set("fillrect", [this](Interp&, const ValueList& a) -> ValueList {
    if (canvas_)
      canvas_->fillRect(numArg(a, 0), numArg(a, 1), numArg(a, 2), numArg(a, 3),
                        colorArg(a, 4, theme::kFg));
    return {};
  });
  set("line", [this](Interp&, const ValueList& a) -> ValueList {
    if (canvas_)
      canvas_->drawLine(numArg(a, 0), numArg(a, 1), numArg(a, 2), numArg(a, 3),
                        colorArg(a, 4, theme::kFg));
    return {};
  });
  set("width", [this](Interp&, const ValueList&) -> ValueList {
    return {Value::number(canvas_ ? canvas_->width() : 240)};
  });
  set("height", [this](Interp&, const ValueList&) -> ValueList {
    return {Value::number(canvas_ ? canvas_->height() : 135)};
  });
  color("BLACK", theme::kBg); color("WHITE", theme::kFg);
  color("GRAY", theme::kMuted); color("CYAN", theme::kAccent);
  color("RED", theme::kDanger); color("GREEN", theme::kOk);
  color("YELLOW", 0xFFE0); color("ORANGE", 0xFD20);

  // ---- platform services ----
  set("log", [](Interp&, const ValueList& a) -> ValueList {
    Serial.printf("[lua] %s\n", a.empty() ? "" : Interp::tostr(a[0]).c_str());
    return {};
  });
  set("millis", [](Interp&, const ValueList&) -> ValueList {
    return {Value::number((double)millis())};
  });
  set("redraw", [this](Interp&, const ValueList&) -> ValueList {
    requestRedraw(); return {};
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

  lua_.setGlobal("cardos", Value::makeTable(tbl));
}

static const char* actionName(KeyAction a) {
  switch (a) {
    case KeyAction::Press: return "press";
    case KeyAction::LongPress: return "long";
    default: return "release";
  }
}
static const char* codeName(KeyCode c) {
  switch (c) {
    case KeyCode::Up: return "up"; case KeyCode::Down: return "down";
    case KeyCode::Left: return "left"; case KeyCode::Right: return "right";
    case KeyCode::Enter: return "enter"; case KeyCode::Esc: return "esc";
    case KeyCode::Backspace: return "backspace"; case KeyCode::Tab: return "tab";
    case KeyCode::Char: return "char"; default: return "none";
  }
}

void ScriptApp::onEnter() {
  if (errored_ || !lua_.hasFunction("on_enter")) return;
  Lua::Result r = lua_.callGlobal("on_enter", {});
  if (!r.ok) fail("on_enter", r.error);
}

void ScriptApp::onExit() {
  // Lets a script release resources (e.g. stop the mic) when popped.
  if (errored_ || !lua_.hasFunction("on_exit")) return;
  lua_.callGlobal("on_exit", {});
}

bool ScriptApp::handleKey(const KeyEvent& ev) {
  if (errored_) return false;  // let Esc pop a broken app
  if (!lua_.hasFunction("on_key")) return false;
  char chbuf[2] = {ev.ch, 0};
  minlua::ValueList out;
  Lua::Result r = lua_.callGlobal(
      "on_key",
      {minlua::Value::string(codeName(ev.code)),
       minlua::Value::string(ev.ch ? chbuf : ""),
       minlua::Value::string(actionName(ev.action))},
      &out);
  if (!r.ok) { fail("on_key", r.error); requestRedraw(); return false; }
  return !out.empty() && out[0].truthy();
}

void ScriptApp::update(uint32_t dtMs) {
  if (errored_ || !lua_.hasFunction("on_update")) return;
  Lua::Result r = lua_.callGlobal("on_update", {minlua::Value::number((double)dtMs)});
  if (!r.ok) { fail("on_update", r.error); requestRedraw(); }
}

void ScriptApp::render(M5Canvas& gfx) {
  if (errored_) {
    gfx.setFont(theme::font());
    gfx.setTextColor(theme::kDanger);
    gfx.setCursor(theme::kPadX, theme::kStatusBarH + 6);
    gfx.print("Lua error:");
    gfx.setTextColor(theme::kFg);
    gfx.setCursor(theme::kPadX, theme::kStatusBarH + 24);
    gfx.print(error_.c_str());
    return;
  }
  if (!lua_.hasFunction("on_render")) return;
  canvas_ = &gfx;
  Lua::Result r = lua_.callGlobal("on_render", {});
  canvas_ = nullptr;
  if (!r.ok) fail("on_render", r.error);
}
