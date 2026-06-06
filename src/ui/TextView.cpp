#include "TextView.h"

#include "Theme.h"
#include "scroll.h"
#include "textwrap.h"

void TextView::setText(const std::string& text) {
  text_ = text;
  wrappedWidth_ = -1;
  scroll_ = 0;
}

bool TextView::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return false;
  if (ev.code == KeyCode::Up && scroll_ > 0) { scroll_--; return true; }
  if (ev.code == KeyCode::Down && scroll_ < (int)lines_.size() - 1) {
    scroll_++;
    return true;
  }
  return false;
}

void TextView::render(M5Canvas& gfx, int x, int y, int w, int h) {
  gfx.setFont(theme::font());
  int textW = w - 2 * theme::kPadX - 2;  // leave room for the scrollbar
  if (wrappedWidth_ != textW) {
    lines_ = wrapText(text_, textW,
                      [&](const std::string& s) {
                        return (int)gfx.textWidth(s.c_str());
                      });
    wrappedWidth_ = textW;
    if (scroll_ >= (int)lines_.size()) scroll_ = 0;
  }
  const int lineH = 14;
  int visible = h / lineH;
  gfx.setTextColor(theme::kFg);
  for (int row = 0; row < visible; row++) {
    int idx = scroll_ + row;
    if (idx >= (int)lines_.size()) break;
    gfx.setCursor(x + theme::kPadX, y + row * lineH);
    gfx.print(lines_[idx].c_str());
  }
  if ((int)lines_.size() > visible) {
    int count = (int)lines_.size();
    int barH = h * visible / count;
    int barY = y + (h - barH) * scroll_ / (count - 1);
    gfx.fillRect(x + w - 2, barY, 2, barH, theme::kMuted);
  }
}
