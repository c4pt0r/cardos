#include "LuaAppsApp.h"

#include "../ui/Theme.h"

void LuaAppsApp::onEnter() {
  host_.clearOwned();  // safe: we are back at root, no script app is stacked
  files_ = host_.listFiles();
  std::vector<MenuItem> items;
  for (const auto& f : files_) items.push_back({f, "", theme::kMuted});
  if (items.empty())
    items.push_back({"(no apps - push via tools/cardos-app.py)", "", theme::kMuted});
  menu_.setItems(std::move(items));
  requestRedraw();
}

bool LuaAppsApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;
  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter && !files_.empty()) {
    host_.launch(files_[menu_.selected()]);
    return true;
  }
  return false;  // Esc -> back to launcher
}

void LuaAppsApp::render(M5Canvas& gfx) {
  int top = theme::kStatusBarH;
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(theme::kPadX, top + 2);
  gfx.print("Installed Lua apps:");
  menu_.render(gfx, 0, top + 18, gfx.width(), gfx.height() - top - 18);
}
