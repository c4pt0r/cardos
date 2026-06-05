#include "StatusBar.h"

#include "Theme.h"

namespace statusbar {
namespace {
WifiIcon wifi_ = WifiIcon::Off;
int batteryPct_ = 100;
bool dirty_ = true;
uint8_t spinnerFrame_ = 0;
}  // namespace

void setWifi(WifiIcon icon) {
  if (wifi_ != icon) { wifi_ = icon; dirty_ = true; }
  if (icon == WifiIcon::Connecting) dirty_ = true;  // animate spinner
}

void setBattery(int pct) {
  // Repaint only when the segment count (same ceil formula as paint())
  // or the low-battery color threshold changes.
  bool segChanged = (pct + 25) / 26 != (batteryPct_ + 25) / 26;
  bool lowChanged = (pct <= 20) != (batteryPct_ <= 20);
  if (segChanged || lowChanged) dirty_ = true;
  batteryPct_ = pct;
}

bool changedSinceLastPaint() { return dirty_; }

void paint(M5Canvas& gfx, const char* title) {
  dirty_ = false;
  gfx.fillRect(0, 0, gfx.width(), theme::kStatusBarH, theme::kBarBg);
  gfx.setFont(theme::fontBold());
  gfx.setTextColor(theme::kFg);
  gfx.setCursor(theme::kPadX, 2);
  gfx.print(title);

  // Battery: 4-segment bar at the right edge.
  int bx = gfx.width() - 26, by = 4;
  gfx.drawRect(bx, by, 20, 8, theme::kFg);
  gfx.fillRect(bx + 20, by + 2, 2, 4, theme::kFg);
  int seg = (batteryPct_ + 25) / 26;  // 0..4
  for (int i = 0; i < seg && i < 4; i++)
    gfx.fillRect(bx + 2 + i * 4, by + 2, 3, 4,
                 batteryPct_ <= 20 ? theme::kDanger : theme::kOk);

  // WiFi: 3 ascending bars, gray when off; spinner dot when connecting.
  int wx = gfx.width() - 48, wy = 12;
  if (wifi_ == WifiIcon::Connecting) {
    spinnerFrame_ = (spinnerFrame_ + 1) % 4;
    gfx.fillCircle(wx + 4 + spinnerFrame_ * 3, wy - 4, 1, theme::kAccent);
  } else {
    int bars = wifi_ == WifiIcon::Bars1 ? 1
             : wifi_ == WifiIcon::Bars2 ? 2
             : wifi_ == WifiIcon::Bars3 ? 3 : 0;
    for (int i = 0; i < 3; i++) {
      uint16_t c = i < bars ? theme::kFg : theme::kMuted;
      int h = 3 + i * 3;
      gfx.fillRect(wx + i * 5, wy - h, 3, h, c);
    }
  }
}

}  // namespace statusbar
