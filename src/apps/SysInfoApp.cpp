#include "SysInfoApp.h"

#include <LittleFS.h>
#include <M5Cardputer.h>
#include <WiFi.h>

#include "../ui/Theme.h"

void SysInfoApp::update(uint32_t dtMs) {
  sinceRefresh_ += dtMs;
  if (sinceRefresh_ >= 1000) {  // refresh once per second
    sinceRefresh_ = 0;
    requestRedraw();
  }
}

void SysInfoApp::render(M5Canvas& gfx) {
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  int y = theme::kStatusBarH + 8;
  auto line = [&](const char* k, const String& v) {
    gfx.setCursor(theme::kPadX, y);
    gfx.setTextColor(theme::kMuted);
    gfx.print(k);
    gfx.setTextColor(theme::kFg);
    gfx.print(v);
    y += 16;
  };
  bool up = WiFi.status() == WL_CONNECTED;
  line("WiFi:    ", up ? WiFi.SSID() : String("not connected"));
  line("IP:      ", up ? WiFi.localIP().toString() : String("-"));
  line("RSSI:    ", up ? String(WiFi.RSSI()) + " dBm" : String("-"));
  line("Heap:    ", String(ESP.getFreeHeap() / 1024) + " KB free");
  uint32_t fsTotal = LittleFS.totalBytes() / 1024;
  uint32_t fsFree = fsTotal - LittleFS.usedBytes() / 1024;
  line("Flash:   ", String(fsFree) + "/" + String(fsTotal) + " KB free");
  line("Battery: ", String(M5Cardputer.Power.getBatteryLevel()) + " %");
  line("Uptime:  ", String(millis() / 1000) + " s");
}
