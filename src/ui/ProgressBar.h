#pragma once
#include <M5GFX.h>

// Horizontal progress bar, 0..100.
class ProgressBar {
 public:
  void setValue(int pct);  // clamped to 0..100
  int value() const { return pct_; }
  void render(M5Canvas& gfx, int x, int y, int w, int h,
              bool showPercent = false) const;

 private:
  int pct_ = 0;
};
