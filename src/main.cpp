#include <M5Cardputer.h>

#include "apps/LauncherApp.h"
#include "apps/SysInfoApp.h"
#include "apps/WiFiApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "core/PowerManager.h"
#include "services/NvsStorage.h"
#include "services/WiFiService.h"
#include "ui/StatusBar.h"

namespace {
AppManager apps;
InputRouter input;
LauncherApp launcher;
SysInfoApp sysinfo;
NvsStorage nvs;
WiFiStore wifiStore(nvs);
WiFiService wifiService;
WiFiApp wifiApp(wifiService, wifiStore);
PowerManager power;
uint32_t lastMs = 0;
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(200);
  Serial.begin(115200);
  Serial.println("[cardos] boot");
  power.begin();
  if (PowerManager::wokeFromDeepSleep())
    Serial.println("[cardos] woke from deep sleep");

  wifiStore.load();
  wifiService.begin(&wifiStore);
  wifiService.autoConnect();
  launcher.addEntry("WiFi Settings", &wifiApp);
  launcher.addEntry("System Info", &sysinfo);
  apps.begin(M5Cardputer.Display, statusbar::paint);
  apps.push(&launcher);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();
  wifiService.tick(now);
  power.keepAwake(wifiService.busy());
  power.tick(now);
  for (const KeyEvent& ev : input.poll()) {
    if (!power.onInput(now)) apps.dispatch(ev);
  }
  apps.update(now - lastMs);

  static uint32_t lastStatusMs = 0;
  if (now - lastStatusMs > 2000) {
    lastStatusMs = now;
    statusbar::setBattery(M5Cardputer.Power.getBatteryLevel());
    if (wifiService.state() == WifiState::Connected) {
      int r = wifiService.rssi();
      statusbar::setWifi(r >= -55 ? statusbar::WifiIcon::Bars3
                         : r >= -70 ? statusbar::WifiIcon::Bars2
                                    : statusbar::WifiIcon::Bars1);
    } else if (wifiService.busy()) {
      statusbar::setWifi(statusbar::WifiIcon::Connecting);
    } else {
      statusbar::setWifi(statusbar::WifiIcon::Off);
    }
  }
  if (statusbar::changedSinceLastPaint()) apps.requestRedraw();

  apps.render();
  lastMs = now;
  delay(5);
}
