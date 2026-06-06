#pragma once
#include <cstdint>
#include <vector>

#include "KeyEvent.h"

// Per-key hold tracker. Fed the set of currently-held key ids each frame,
// emits Press / LongPress (once per hold) / Release transitions.
// Pure header: time is a parameter; no Arduino includes.
// Ids: printable keys use their char value; special keys use the
// constants below (outside the char range).
class KeyTracker {
 public:
  static constexpr uint32_t kLongPressMs = 600;
  static constexpr size_t kMaxKeys = 8;  // extra concurrent keys are ignored
  static constexpr uint16_t kEnterId = 0x100;
  static constexpr uint16_t kBackspaceId = 0x101;
  static constexpr uint16_t kTabId = 0x102;

  struct Out {
    uint16_t id;
    KeyAction action;
  };

  std::vector<Out> update(const uint16_t* held, size_t count, uint32_t now) {
    std::vector<Out> out;
    // Releases: slots whose id is no longer held.
    for (auto& s : slots_) {
      if (!s.used) continue;
      bool still = false;
      for (size_t i = 0; i < count; i++)
        if (held[i] == s.id) { still = true; break; }
      if (!still) {
        out.push_back({s.id, KeyAction::Release});
        s.used = false;
      }
    }
    // Presses and long-presses.
    for (size_t i = 0; i < count; i++) {
      Slot* slot = find(held[i]);
      if (!slot) {
        if (Slot* free = alloc()) {
          *free = {held[i], now, false, true};
          out.push_back({held[i], KeyAction::Press});
        }
        continue;  // full: ignore overflow keys
      }
      if (!slot->longFired && now - slot->since >= kLongPressMs) {
        slot->longFired = true;
        out.push_back({slot->id, KeyAction::LongPress});
      }
    }
    return out;
  }

 private:
  struct Slot {
    uint16_t id = 0;
    uint32_t since = 0;
    bool longFired = false;
    bool used = false;
  };
  Slot* find(uint16_t id) {
    for (auto& s : slots_)
      if (s.used && s.id == id) return &s;
    return nullptr;
  }
  Slot* alloc() {
    for (auto& s : slots_)
      if (!s.used) return &s;
    return nullptr;
  }
  Slot slots_[kMaxKeys];
};
