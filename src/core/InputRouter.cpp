#include "InputRouter.h"

#include <M5Cardputer.h>

std::vector<KeyEvent> InputRouter::poll() {
  std::vector<KeyEvent> out;

  // G0/BtnA acts as Esc when running (it is also the deep-sleep wake pin).
  if (M5Cardputer.BtnA.wasPressed()) {
    KeyEvent ev;
    ev.code = KeyCode::Esc;
    out.push_back(ev);
  }

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())
    return out;

  auto st = M5Cardputer.Keyboard.keysState();
  if (st.enter || st.del || st.tab) {
    out.push_back(mapKey(0, st.fn, st.enter, st.del, st.tab));
    return out;
  }
  for (char c : st.word) out.push_back(mapKey(c, st.fn, false, false, false));
  return out;
}
