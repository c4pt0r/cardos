#include "Dialog.h"

#include "Theme.h"

namespace {
void paintBox(M5Canvas& gfx, int w, int h, int& x, int& y) {
  x = (gfx.width() - w) / 2;
  y = (gfx.height() - h) / 2;
  gfx.fillRect(x, y, w, h, theme::kBarBg);
  gfx.drawRect(x, y, w, h, theme::kAccent);
}
}  // namespace

namespace dialog {

void paintToast(M5Canvas& gfx, const std::string& msg, bool spinner) {
  int x, y;
  paintBox(gfx, 180, 50, x, y);
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  gfx.setTextDatum(middle_center);
  gfx.drawString(msg.c_str(), gfx.width() / 2, y + (spinner ? 18 : 25));
  gfx.setTextDatum(top_left);
  if (spinner) {
    static uint8_t frame = 0;
    frame = (frame + 1) % 8;
    for (int i = 0; i < 8; i++) {
      uint16_t c = i == frame ? theme::kAccent : theme::kMuted;
      gfx.fillCircle(gfx.width() / 2 - 28 + i * 8, y + 38, 2, c);
    }
  }
}

}  // namespace dialog

void ConfirmDialog::reset(const std::string& msg, const std::string& first,
                          const std::string& second) {
  msg_ = msg;
  first_ = first;
  second_ = second;
  selected_ = 0;
}

ConfirmDialog::Result ConfirmDialog::handleKey(const KeyEvent& ev) {
  switch (ev.code) {
    case KeyCode::Left:
    case KeyCode::Right:
    case KeyCode::Up:
    case KeyCode::Down:
      selected_ = 1 - selected_;
      return Result::None;
    case KeyCode::Enter:
      return selected_ == 0 ? Result::First : Result::Second;
    case KeyCode::Esc:
      return Result::Cancel;
    default:
      return Result::None;
  }
}

void ConfirmDialog::render(M5Canvas& gfx) const {
  int x, y;
  paintBox(gfx, 200, 70, x, y);
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kFg);
  gfx.setTextDatum(middle_center);
  gfx.drawString(msg_.c_str(), gfx.width() / 2, y + 18);
  int cx = gfx.width() / 2;
  const std::string* labels[2] = {&first_, &second_};
  for (int i = 0; i < 2; i++) {
    int bx = cx - 90 + i * 95;
    bool sel = selected_ == i;
    gfx.fillRect(bx, y + 40, 85, 20, sel ? theme::kAccent : theme::kBarBg);
    gfx.drawRect(bx, y + 40, 85, 20, theme::kMuted);
    gfx.setTextColor(sel ? TFT_BLACK : theme::kFg);
    gfx.drawString(labels[i]->c_str(), bx + 42, y + 50);
  }
  gfx.setTextDatum(top_left);
}
