#pragma once
#include <string>

#include "../core/App.h"
#include "../lua/Lua.h"

// Runs a single Lua script as a CardOS app. The script may define globals
// `title` and the callbacks on_enter / on_key(code,ch,action) / on_update(dt)
// / on_render(); it draws through the `cardos.*` binding table.
class ScriptApp : public App {
 public:
  explicit ScriptApp(const std::string& path);

  bool ok() const { return !errored_; }
  const std::string& error() const { return error_; }
  const std::string& path() const { return path_; }

  const char* title() const override { return title_.c_str(); }
  void onEnter() override;
  void onExit() override;
  bool handleKey(const KeyEvent& ev) override;
  void update(uint32_t dtMs) override;
  void render(M5Canvas& gfx) override;

 private:
  void registerBindings();
  void fail(const std::string& where, const std::string& msg);

  minlua::Lua lua_;
  std::string path_;
  std::string title_;
  std::string error_;
  bool errored_ = false;
  M5Canvas* canvas_ = nullptr;  // valid only during render()
};
