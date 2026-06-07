#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../core/App.h"
#include "../lua/Lua.h"

// Interactive Lua prompt over a persistent session (globals survive
// across lines; the session resets on each app entry). Exposes the
// standard library plus the shared cardos platform bindings — no
// drawing functions.
class LuaReplApp : public App {
 public:
  const char* title() const override { return "Lua REPL"; }
  void onEnter() override;
  void onExit() override;
  bool handleKey(const KeyEvent& ev) override;
  void render(M5Canvas& gfx) override;

 private:
  struct Line { std::string text; uint16_t color; };
  void pushLine(const std::string& text, uint16_t color);
  void submit();

  std::unique_ptr<minlua::Lua> lua_;
  std::vector<Line> lines_;            // scrollback, capped
  int scroll_ = 0;                     // rows scrolled up from the tail
  std::string edit_;                   // input line under construction
  std::vector<std::string> history_;   // submitted lines, capped
  int histPos_ = -1;                   // -1 = editing a fresh line
};
