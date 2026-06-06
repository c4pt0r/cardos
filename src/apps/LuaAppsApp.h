#pragma once
#include <string>
#include <vector>

#include "../core/App.h"
#include "../ui/MenuList.h"
#include "ScriptHost.h"

// Launcher screen listing installed Lua apps (/flash/apps/*.lua). Enter
// runs the selected one; Esc returns to the main launcher.
class LuaAppsApp : public App {
 public:
  explicit LuaAppsApp(ScriptHost& host) : host_(host) {}
  const char* title() const override { return "Lua Apps"; }
  void onEnter() override;
  bool handleKey(const KeyEvent& ev) override;
  void render(M5Canvas& gfx) override;

 private:
  ScriptHost& host_;
  MenuList menu_;
  std::vector<std::string> files_;
};
