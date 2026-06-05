#pragma once
#include <M5Cardputer.h>

#include <vector>

#include "App.h"
#include "KeyEvent.h"

// Scene stack + render loop. Owns the off-screen canvas; composes the
// status bar (drawn by a callback so core/ does not depend on ui/).
class AppManager {
 public:
  using StatusBarPainter = void (*)(M5Canvas& gfx, const char* title);

  void begin(M5GFX& display, StatusBarPainter painter);
  void push(App* app);   // apps are statically allocated; not owned
  void pop();
  App* top() const { return stack_.empty() ? nullptr : stack_.back(); }

  void dispatch(const KeyEvent& ev);
  void update(uint32_t dtMs);
  void render();              // redraws only when the top app is dirty
  void requestRedraw();       // force a redraw (e.g. status bar changed)

 private:
  std::vector<App*> stack_;
  M5Canvas canvas_;
  M5GFX* display_ = nullptr;
  StatusBarPainter statusBar_ = nullptr;
  bool forceRedraw_ = true;
};
