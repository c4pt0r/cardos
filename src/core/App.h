#pragma once
#include <M5GFX.h>

#include "KeyEvent.h"

class AppManager;

// One screen/scene. Apps are pushed onto AppManager's stack; the top app
// receives input and renders into the content area below the status bar.
class App {
 public:
  virtual ~App() = default;
  virtual const char* title() const = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  // Return true if the key was consumed. Unconsumed Esc pops the app.
  virtual bool handleKey(const KeyEvent& ev) { (void)ev; return false; }
  virtual void update(uint32_t dtMs) { (void)dtMs; }
  virtual void render(M5Canvas& gfx) = 0;  // gfx is the full-screen canvas

  void requestRedraw() { dirty_ = true; }
  bool consumeDirty() { bool d = dirty_; dirty_ = false; return d; }
  void attach(AppManager* mgr) { mgr_ = mgr; }

 protected:
  AppManager* mgr_ = nullptr;
  bool dirty_ = true;
};
