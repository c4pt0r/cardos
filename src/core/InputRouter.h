#pragma once
#include <vector>

#include "KeyEvent.h"
#include "KeyTracker.h"

// Polls the Cardputer keyboard matrix and G0 button every frame, feeding
// the full held-key snapshot into KeyTracker to derive Press / LongPress /
// Release events.
class InputRouter {
 public:
  // Call once per loop after M5Cardputer.update().
  std::vector<KeyEvent> poll();

 private:
  KeyTracker tracker_;
};
