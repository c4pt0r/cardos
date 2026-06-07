#include "LuaReplApp.h"

#include <algorithm>

#include "../ui/Theme.h"
#include "LuaBindings.h"
#include "ReplEval.h"

namespace {
constexpr int kMaxLines = 50;
constexpr int kMaxHistory = 10;
constexpr int kLineH = 14;
constexpr int kInputH = 18;
constexpr int kVisibleRows =
    (135 - theme::kStatusBarH - kInputH - 4) / kLineH;  // 6 on 240x135
constexpr size_t kWrapCols = 37;  // ~6px/char over 228px of width
}  // namespace

void LuaReplApp::onEnter() {
  lua_.reset(new minlua::Lua());
  auto tbl = std::make_shared<minlua::Table>();
  cardos::installPlatformBindings(tbl);
  lua_->setGlobal("cardos", minlua::Value::makeTable(tbl));
  lua_->onPrint = [this](const std::string& s) { pushLine(s, theme::kFg); };
  lines_.clear();
  history_.clear();
  histPos_ = -1;
  scroll_ = 0;
  edit_.clear();
  pushLine("Lua REPL  Fn+;.=history Fn+,/=page", theme::kMuted);
  requestRedraw();
}

void LuaReplApp::onExit() {
  lua_.reset();  // free the session
}

void LuaReplApp::pushLine(const std::string& text, uint16_t color) {
  // Split on newlines and hard-wrap so one giant value can't hide the
  // rest of the scrollback.
  size_t start = 0;
  while (start <= text.size()) {
    size_t nl = text.find('\n', start);
    std::string row = text.substr(
        start, nl == std::string::npos ? std::string::npos : nl - start);
    for (size_t off = 0;; off += kWrapCols) {
      lines_.push_back({row.substr(off, kWrapCols), color});
      if (off + kWrapCols >= row.size()) break;
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  if ((int)lines_.size() > kMaxLines)
    lines_.erase(lines_.begin(), lines_.end() - kMaxLines);
  requestRedraw();
}

void LuaReplApp::submit() {
  if (edit_.empty()) return;
  pushLine("> " + edit_, theme::kAccent);
  history_.push_back(edit_);
  if ((int)history_.size() > kMaxHistory) history_.erase(history_.begin());
  histPos_ = -1;
  auto r = cardos::replEval(*lua_, edit_);  // print() lands via onPrint
  if (!r.ok)
    pushLine(r.text, theme::kDanger);
  else if (!r.text.empty())
    pushLine(r.text, theme::kFg);
  edit_.clear();
  scroll_ = 0;  // snap to the tail
  requestRedraw();
}

bool LuaReplApp::handleKey(const KeyEvent& ev) {
  if (ev.action != KeyAction::Press) return true;  // standard guard

  if (ev.fn) {  // meta layer: history + paging
    if (ev.code == KeyCode::Up && !history_.empty()) {
      histPos_ = histPos_ < 0 ? (int)history_.size() - 1
                              : std::max(0, histPos_ - 1);
      edit_ = history_[histPos_];
      requestRedraw();
    } else if (ev.code == KeyCode::Down && histPos_ >= 0) {
      if (++histPos_ >= (int)history_.size()) { histPos_ = -1; edit_.clear(); }
      else edit_ = history_[histPos_];
      requestRedraw();
    } else if (ev.code == KeyCode::Left) {
      int maxScroll = std::max(0, (int)lines_.size() - kVisibleRows);
      scroll_ = std::min(maxScroll, scroll_ + kVisibleRows);
      requestRedraw();
    } else if (ev.code == KeyCode::Right) {
      scroll_ = std::max(0, scroll_ - kVisibleRows);
      requestRedraw();
    }
    return true;
  }

  switch (ev.code) {
    case KeyCode::Esc:
      return false;  // pop back to the launcher
    case KeyCode::Enter:
      submit();
      return true;
    case KeyCode::Backspace:
      if (!edit_.empty()) { edit_.pop_back(); requestRedraw(); }
      return true;
    default:
      // Nav keys carry their literal char (; . , /) — Lua needs them.
      if (ev.ch >= 0x20 && ev.ch < 0x7F && edit_.size() < 120) {
        edit_.push_back(ev.ch);
        requestRedraw();
      }
      return true;
  }
}

void LuaReplApp::render(M5Canvas& gfx) {
  gfx.setFont(theme::font());
  int top = theme::kStatusBarH + 2;

  int total = (int)lines_.size();
  int first = std::max(0, total - kVisibleRows - scroll_);
  int y = top;
  for (int i = first; i < total && i < first + kVisibleRows; i++) {
    gfx.setTextColor(lines_[i].color);
    gfx.setCursor(theme::kPadX, y);
    gfx.print(lines_[i].text.c_str());
    y += kLineH;
  }

  int inputY = gfx.height() - kInputH;
  gfx.drawFastHLine(0, inputY, gfx.width(), theme::kMuted);
  gfx.setCursor(theme::kPadX, inputY + 4);
  gfx.setTextColor(theme::kAccent);
  gfx.print("> ");
  gfx.setTextColor(theme::kFg);
  // Keep the cursor visible: show the tail when the line overflows.
  std::string shown = edit_.size() > kWrapCols - 3
                          ? edit_.substr(edit_.size() - (kWrapCols - 3))
                          : edit_;
  gfx.print(shown.c_str());
  gfx.print('_');
}
