#pragma once
#include <M5GFX.h>

#include <string>
#include <vector>

#include "../core/KeyEvent.h"

// Scrollable multi-line text display. Wraps on first render (or width
// change); ;/. scroll by line.
class TextView {
 public:
  void setText(const std::string& text);
  bool handleKey(const KeyEvent& ev);  // consumes Up/Down (Press only)
  void render(M5Canvas& gfx, int x, int y, int w, int h);

 private:
  std::string text_;
  std::vector<std::string> lines_;
  int wrappedWidth_ = -1;  // width lines_ was computed for; -1 = dirty
  int scroll_ = 0;         // first visible line
};
