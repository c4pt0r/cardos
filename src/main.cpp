#include <M5Cardputer.h>

#include "apps/AppUploaderApp.h"
#include "apps/HttpDemoApp.h"
#include "apps/LauncherApp.h"
#include "apps/LuaAppsApp.h"
#include "apps/RecorderApp.h"
#include "apps/ScriptHost.h"
#include "apps/VoiceMemoApp.h"
#include "apps/SysInfoApp.h"
#include "apps/WiFiApp.h"
#include "core/AppManager.h"
#include "core/InputRouter.h"
#include "core/PowerManager.h"
#include "core/SerialControl.h"
#include "services/NvsStorage.h"
#include "sdk/Audio.h"
#include "sdk/Fs.h"
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
HttpDemoApp httpDemo;
RecorderApp recorder;
VoiceMemoApp voiceMemo;
ScriptHost scriptHost;
LuaAppsApp luaApps(scriptHost);
AppUploaderApp appUploader;
SerialControl serialControl;
PowerManager power;
uint32_t lastMs = 0;
}  // namespace

// The Lua interpreter recurses on the loop task; give it a roomier stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

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
  cardos::fs::begin();

  wifiStore.load();
  wifiService.begin(&wifiStore);
  wifiService.autoConnect();
  launcher.addEntry("WiFi Settings", &wifiApp);
  launcher.addEntry("HTTP Demo", &httpDemo);
  launcher.addEntry("Recorder", &recorder);
  launcher.addEntry("Voice Memo", &voiceMemo);
  launcher.addEntry("Lua Apps", &luaApps);
  launcher.addEntry("App Uploader", &appUploader);
  launcher.addEntry("System Info", &sysinfo);
  scriptHost.begin(&apps);
  serialControl.begin(&scriptHost);
  apps.begin(M5Cardputer.Display, statusbar::paint);
  apps.push(&launcher);
  lastMs = millis();
}

void loop() {
  M5Cardputer.update();
  serialControl.tick();  // host app-management over USB serial
  uint32_t now = millis();
  wifiService.tick(now);
  cardos::audio::tick();
  power.keepAwake(wifiService.busy());
  if (power.tick(now)) apps.requestRedraw();  // repaint after canceled notice
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
