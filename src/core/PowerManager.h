#pragma once
#include <cstdint>

#include "IdlePolicy.h"

// Device-side power control driven by IdlePolicy: brightness tiers, the
// pre-sleep notice, and the deep-sleep sequence (G0/EXT0 wake).
class PowerManager {
 public:
  static constexpr uint32_t kDimAfterMs = 60000;
  static constexpr uint32_t kSleepAfterMs = 300000;
  static constexpr uint8_t kBrightActive = 200;
  static constexpr uint8_t kBrightDimmed = 40;

  void begin();
  // Forward every key press here BEFORE dispatching to apps. Returns true
  // if the key must be swallowed (it only woke the dimmed screen).
  bool onInput(uint32_t nowMs);
  void keepAwake(bool on) { policy_.keepAwake(on); }
  // Call once per loop. May not return (deep sleep).
  void tick(uint32_t nowMs);
  static bool wokeFromDeepSleep();

 private:
  void showSleepNoticeAndSleep();  // 3s notice, any key cancels
  IdlePolicy policy_{kDimAfterMs, kSleepAfterMs};
  IdleState applied_ = IdleState::Active;
};
