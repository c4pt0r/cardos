#include <M5Cardputer.h>

#include "apps/LauncherApp.h"
#include "apps/SysInfoApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "ui/StatusBar.h"

namespace {
AppManager apps;
InputRouter input;
LauncherApp launcher;
SysInfoApp sysinfo;
uint32_t lastMs = 0;
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  Serial.begin(115200);
  Serial.println("[cardos] boot");

  launcher.addEntry("System Info", &sysinfo);
  apps.begin(M5Cardputer.Display, statusbar::paint);
  apps.push(&launcher);
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
