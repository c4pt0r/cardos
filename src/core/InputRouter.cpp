#include "InputRouter.h"

#include <M5Cardputer.h>

namespace {
KeyEvent escEvent(KeyAction action) {
  KeyEvent ev;
  ev.code = KeyCode::Esc;
  ev.action = action;
  return ev;
}
}  // namespace

std::vector<KeyEvent> InputRouter::poll() {
  std::vector<KeyEvent> out;

  // G0/BtnA: M5Unified tracks press/hold/release for us.
  auto& btn = M5Cardputer.BtnA;
  if (btn.wasPressed()) out.push_back(escEvent(KeyAction::Press));
  if (btn.wasHold()) out.push_back(escEvent(KeyAction::LongPress));
  if (btn.wasReleased()) out.push_back(escEvent(KeyAction::Release));

  // Keyboard matrix: full held-key snapshot every frame.
  uint16_t held[KeyTracker::kMaxKeys];
  size_t n = 0;
  bool fn = false;
  if (M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();
    fn = st.fn;
    if (st.enter && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kEnterId;
    if (st.del && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kBackspaceId;
    if (st.tab && n < KeyTracker::kMaxKeys) held[n++] = KeyTracker::kTabId;
    for (char c : st.word) {
      if (n >= KeyTracker::kMaxKeys) break;
      held[n++] = (uint16_t)(uint8_t)c;
    }
  }

  for (const auto& o : tracker_.update(held, n, millis())) {
    KeyEvent ev;
    if (o.id == KeyTracker::kEnterId) ev = mapKey(0, fn, true, false, false);
    else if (o.id == KeyTracker::kBackspaceId) ev = mapKey(0, fn, false, true, false);
    else if (o.id == KeyTracker::kTabId) ev = mapKey(0, fn, false, false, true);
    else ev = mapKey((char)o.id, fn, false, false, false);
    ev.action = o.action;
    out.push_back(ev);
  }
  return out;
}
