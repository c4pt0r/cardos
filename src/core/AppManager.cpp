#include "AppManager.h"

void AppManager::begin(M5GFX& display, StatusBarPainter painter) {
  display_ = &display;
  statusBar_ = painter;
  canvas_.setColorDepth(16);
  if (!canvas_.createSprite(display.width(), display.height()))
    Serial.println("[cardos] FATAL: canvas alloc failed");  // render() skips
}

void AppManager::push(App* app) {
  app->attach(this);
  stack_.push_back(app);
  app->onEnter();
  forceRedraw_ = true;
}

void AppManager::pop() {
  if (stack_.size() <= 1) return;  // launcher stays at the bottom
  stack_.back()->onExit();
  stack_.pop_back();
  forceRedraw_ = true;
}

void AppManager::dispatch(const KeyEvent& ev) {
  App* app = top();
  if (!app) return;
  if (!app->handleKey(ev) && ev.code == KeyCode::Esc &&
      ev.action == KeyAction::Press)
    pop();
}

bool AppManager::onStack(App* app) const {
  for (App* a : stack_)
    if (a == app) return true;
  return false;
}

void AppManager::update(uint32_t dtMs) {
  if (App* app = top()) app->update(dtMs);
}

void AppManager::requestRedraw() { forceRedraw_ = true; }

void AppManager::render() {
  App* app = top();
  if (!app || !canvas_.getBuffer()) return;
  bool dirty = app->consumeDirty();
  if (!forceRedraw_ && !dirty) return;
  forceRedraw_ = false;
  canvas_.fillSprite(TFT_BLACK);
  app->render(canvas_);
  if (statusBar_) statusBar_(canvas_, app->title());
  canvas_.pushSprite(display_, 0, 0);
}
