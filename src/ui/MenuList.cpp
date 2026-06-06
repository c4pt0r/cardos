#include "MenuList.h"

#include "Theme.h"
#include "scroll.h"

void MenuList::setItems(std::vector<MenuItem> items) {
  items_ = std::move(items);
  if (selected_ >= (int)items_.size()) selected_ = 0;
}

void MenuList::setSelected(int idx) {
  if (idx >= 0 && idx < (int)items_.size()) selected_ = idx;
}

bool MenuList::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return false;
  if (items_.empty()) return false;
  if (ev.code == KeyCode::Up && selected_ > 0) { selected_--; return true; }
  if (ev.code == KeyCode::Down && selected_ < (int)items_.size() - 1) {
    selected_++;
    return true;
  }
  return false;
}

void MenuList::render(M5Canvas& gfx, int x, int y, int w, int h) const {
  gfx.setFont(theme::font());
  int visible = h / theme::kRowH;
  int first = scrollFirstVisible(selected_, (int)items_.size(), visible);
  for (int row = 0; row < visible; row++) {
    int idx = first + row;
    if (idx >= (int)items_.size()) break;
    int ry = y + row * theme::kRowH;
    bool sel = idx == selected_;
    if (sel) gfx.fillRect(x, ry, w, theme::kRowH, theme::kAccent);
    int nw = items_[idx].note.empty()
                 ? 0
                 : gfx.textWidth(items_[idx].note.c_str());
    // Clip the label so long text (e.g. 32-char SSIDs) can't bleed into
    // the right-aligned note.
    gfx.setClipRect(x, ry, w - nw - 2 * theme::kPadX, theme::kRowH);
    gfx.setTextColor(sel ? TFT_BLACK : theme::kFg);
    gfx.setCursor(x + theme::kPadX, ry + 3);
    gfx.print(items_[idx].label.c_str());
    gfx.clearClipRect();
    if (nw > 0) {
      gfx.setTextColor(sel ? TFT_BLACK : items_[idx].noteColor);
      gfx.setCursor(x + w - nw - theme::kPadX, ry + 3);
      gfx.print(items_[idx].note.c_str());
    }
  }
  // Scroll hint: thin bar on the right edge.
  if ((int)items_.size() > visible) {
    int barH = h * visible / (int)items_.size();
    int barY = y + (h - barH) * first / ((int)items_.size() - visible);
    gfx.fillRect(x + w - 2, barY, 2, barH, theme::kMuted);
  }
}
