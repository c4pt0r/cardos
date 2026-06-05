#pragma once
#include <cstdint>

// Idle state machine. Pure header: takes time as a parameter, no millis().
enum class IdleState : uint8_t { Active, Dimmed, SleepPending };

class IdlePolicy {
 public:
  IdlePolicy(uint32_t dimAfterMs, uint32_t sleepAfterMs)
      : dimAfterMs_(dimAfterMs), sleepAfterMs_(sleepAfterMs) {}

  // Record user input at `now`. Returns true if the key should be swallowed
  // (it only woke the dimmed screen and must not reach the app).
  bool onInput(uint32_t now) {
    bool swallow = state(now) == IdleState::Dimmed;
    lastInputMs_ = now;
    return swallow;
  }

  // Suppresses SleepPending (e.g. WiFi connecting); dimming still applies.
  void keepAwake(bool on) { keepAwake_ = on; }

  IdleState state(uint32_t now) const {
    uint32_t idle = now - lastInputMs_;
    if (!keepAwake_ && idle >= sleepAfterMs_) return IdleState::SleepPending;
    if (idle >= dimAfterMs_) return IdleState::Dimmed;
    return IdleState::Active;
  }

 private:
  uint32_t dimAfterMs_, sleepAfterMs_;
  uint32_t lastInputMs_ = 0;
  bool keepAwake_ = false;
};
