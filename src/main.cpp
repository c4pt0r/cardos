#include <M5Cardputer.h>

#include "core/AppManager.h"
#include "core/InputRouter.h"

namespace {

// Temporary key-echo app, replaced by LauncherApp in a later task.
class EchoApp : public App {
 public:
  const char* title() const override { return "CardOS"; }
  bool handleKey(const KeyEvent& ev) override {
    last_ = ev;
    requestRedraw();
    return ev.code != KeyCode::Esc;  // keep Esc from popping the root
  }
  void render(M5Canvas& gfx) override {
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_WHITE);
    gfx.setCursor(10, 40);
    switch (last_.code) {
      case KeyCode::None: gfx.print("press a key"); break;
      case KeyCode::Up: gfx.print("UP"); break;
      case KeyCode::Down: gfx.print("DOWN"); break;
      case KeyCode::Enter: gfx.print("ENTER"); break;
      case KeyCode::Esc: gfx.print("ESC"); break;
      case KeyCode::Backspace: gfx.print("BACKSPACE"); break;
      default: gfx.printf("char: %c", last_.ch); break;
    }
  }
 private:
  KeyEvent last_;
};

void paintStatusBar(M5Canvas& gfx, const char* title) {
  gfx.fillRect(0, 0, gfx.width(), 16, TFT_NAVY);
  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE);
  gfx.setCursor(4, 4);
  gfx.print(title);
}

AppManager apps;
InputRouter input;
EchoApp echo;
uint32_t lastMs = 0;

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  Serial.begin(115200);
  Serial.println("[cardos] boot");
  apps.begin(M5Cardputer.Display, paintStatusBar);
  apps.push(&echo);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();
  for (const KeyEvent& ev : input.poll()) apps.dispatch(ev);
  apps.update(now - lastMs);
  apps.render();
  lastMs = now;
  delay(5);
}
