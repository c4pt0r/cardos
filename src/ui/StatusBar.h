#pragma once
#include <M5GFX.h>

// Framework-owned top bar. WiFi/battery state is pushed in by main.cpp
// (the status bar does not query services itself).
namespace statusbar {

enum class WifiIcon { Off, Connecting, Bars1, Bars2, Bars3 };

void setWifi(WifiIcon icon);
void setBattery(int pct);            // 0..100
bool changedSinceLastPaint();        // true -> AppManager.requestRedraw()
void paint(M5Canvas& gfx, const char* title);

}  // namespace statusbar
