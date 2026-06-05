#include <M5Cardputer.h>

#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "ui/MenuList.h"
#include "ui/StatusBar.h"
#include "ui/Theme.h"

namespace {

// Temporary menu demo, replaced by LauncherApp in a later task.
class EchoApp : public App {
 public:
  const char* title() const override { return "CardOS"; }
  void onEnter() override {
    menu_.setItems({{"WiFi Settings", "", 0},
                    {"System Info", "", 0},
                    {"中文条目测试", "ok", theme::kOk},
                    {"Item 4", "", 0},
                    {"Item 5", "", 0},
                    {"Item 6", "", 0},
                    {"Item 7", "", 0},
                    {"Item 8", "", 0}});
  }
  bool handleKey(const KeyEvent& ev) override {
    if (menu_.handleKey(ev)) { requestRedraw(); return true; }
    return ev.code != KeyCode::Esc;
  }
  void render(M5Canvas& gfx) override {
    menu_.render(gfx, 0, theme::kStatusBarH, gfx.width(),
                 gfx.height() - theme::kStatusBarH);
  }
 private:
  MenuList menu_;
};

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
  apps.begin(M5Cardputer.Display, statusbar::paint);
  apps.push(&echo);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();
  for (const KeyEvent& ev : input.poll()) apps.dispatch(ev);
  apps.update(now - lastMs);
  static uint32_t lastBatteryMs = 0;
  if (now - lastBatteryMs > 5000) {
    lastBatteryMs = now;
    statusbar::setBattery(M5Cardputer.Power.getBatteryLevel());
  }
  if (statusbar::changedSinceLastPaint()) apps.requestRedraw();
  apps.render();
  lastMs = now;
  delay(5);
}
