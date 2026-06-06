#include "LauncherApp.h"

#include "../core/AppManager.h"
#include "../ui/Theme.h"

void LauncherApp::addEntry(const char* label, App* app) {
  entries_.push_back({label, app});
}

void LauncherApp::onEnter() { rebuild(); }

void LauncherApp::rebuild() {
  std::vector<MenuItem> items;
  for (auto& e : entries_) items.push_back({e.first, "", 0});
  menu_.setItems(std::move(items));
  requestRedraw();
}

bool LauncherApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;
  if (menu_.handleKey(ev)) { requestRedraw(); return true; }
  if (ev.code == KeyCode::Enter && !entries_.empty()) {
    mgr_->push(entries_[menu_.selected()].second);
    return true;
  }
  return ev.code == KeyCode::Esc;  // swallow Esc at the root
}

void LauncherApp::render(M5Canvas& gfx) {
  menu_.render(gfx, 0, theme::kStatusBarH, gfx.width(),
               gfx.height() - theme::kStatusBarH);
}
