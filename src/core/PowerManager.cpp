#include "PowerManager.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_sleep.h>

void PowerManager::begin() {
  M5Cardputer.Display.setBrightness(kBrightActive);
}

bool PowerManager::onInput(uint32_t nowMs) {
  bool swallow = policy_.onInput(nowMs);
  if (applied_ != IdleState::Active) {
    M5Cardputer.Display.setBrightness(kBrightActive);
    applied_ = IdleState::Active;
  }
  return swallow;
}

bool PowerManager::tick(uint32_t nowMs) {
  IdleState s = policy_.state(nowMs);
  if (s == applied_) return false;
  applied_ = s;
  switch (s) {
    case IdleState::Active:
      M5Cardputer.Display.setBrightness(kBrightActive);
      break;
    case IdleState::Dimmed:
      M5Cardputer.Display.setBrightness(kBrightDimmed);
      break;
    case IdleState::SleepPending:
      showSleepNoticeAndSleep();
      // Only reached if the user cancelled within the notice window. The
      // notice painted directly to the display, so the app must repaint.
      policy_.onInput(nowMs);
      applied_ = IdleState::Active;
      M5Cardputer.Display.setBrightness(kBrightActive);
      return true;
  }
  return false;
}

void PowerManager::showSleepNoticeAndSleep() {
  auto& d = M5Cardputer.Display;
  d.setBrightness(kBrightActive);
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::efontCN_16);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(middle_center);
  d.drawString("Sleeping soon...", d.width() / 2, d.height() / 2 - 12);
  d.drawString("Press G0 to wake", d.width() / 2, d.height() / 2 + 12);
  d.setTextDatum(top_left);

  uint32_t start = millis();
  while (millis() - start < 3000) {  // any key cancels
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
      return;
    if (M5Cardputer.BtnA.wasPressed()) return;
    delay(20);
  }

  Serial.println("[power] entering deep sleep");
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  d.sleep();           // display controller off + backlight off
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // G0 pressed = LOW
  esp_deep_sleep_start();
}

bool PowerManager::wokeFromDeepSleep() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}
