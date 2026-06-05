#pragma once
#include <M5GFX.h>

#include <string>
#include <vector>

#include "../core/KeyEvent.h"

// Scrollable, selectable vertical list. Up/Down moves the selection;
// the caller reacts to Enter itself (via selected()).
struct MenuItem {
  std::string label;
  std::string note;      // right-aligned annotation (signal bars, lock, ...)
  uint16_t noteColor = 0xFFFF;
};

class MenuList {
 public:
  void setItems(std::vector<MenuItem> items);
  bool handleKey(const KeyEvent& ev);  // true if consumed (selection moved)
  int selected() const { return selected_; }
  void setSelected(int idx);
  // Draws into the given rect; rows of theme::kRowH, ~6 visible rows.
  void render(M5Canvas& gfx, int x, int y, int w, int h) const;

 private:
  std::vector<MenuItem> items_;
  int selected_ = 0;
};
