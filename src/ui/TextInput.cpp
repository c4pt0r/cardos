#include "TextInput.h"

#include "Theme.h"

void TextInput::reset(const std::string& prompt) {
  prompt_ = prompt;
  text_.clear();
  masked_ = false;
}

TextInput::Result TextInput::handleKey(const KeyEvent& ev) {
  switch (ev.code) {
    case KeyCode::Enter:
      return Result::Submit;
    case KeyCode::Esc:
      return Result::Cancel;
    case KeyCode::Backspace:
      if (!text_.empty()) text_.pop_back();
      return Result::None;
    case KeyCode::Tab:
      masked_ = !masked_;
      return Result::None;
    default:
      // Nav keys carry their literal char; accept any printable.
      if (ev.ch >= 0x20 && ev.ch < 0x7F && text_.size() < 63)
        text_.push_back(ev.ch);
      return Result::None;
  }
}

void TextInput::render(M5Canvas& gfx, int x, int y, int w) const {
  gfx.setFont(theme::font());
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(x, y);
  gfx.print(prompt_.c_str());

  int by = y + 16;
  gfx.drawRect(x, by, w, 20, theme::kAccent);
  gfx.setTextColor(theme::kFg);
  gfx.setCursor(x + 4, by + 4);
  if (masked_) {
    for (size_t i = 0; i < text_.size(); i++) gfx.print('*');
  } else {
    gfx.print(text_.c_str());
  }
  gfx.print('_');  // cursor
  gfx.setTextColor(theme::kMuted);
  gfx.setCursor(x, by + 26);
  gfx.print("Enter=OK  Esc=Cancel  Tab=Mask");
}
