#pragma once
#include <cstdint>

// Semantic key codes for CardOS. Pure header: no Arduino/M5 includes.
enum class KeyCode : uint8_t {
  None, Up, Down, Left, Right, Enter, Esc, Backspace, Tab, Char
};

// How the key transitioned this frame.
enum class KeyAction : uint8_t { Press, LongPress, Release };

struct KeyEvent {
  KeyCode code = KeyCode::None;
  char ch = 0;       // raw printable char (valid for Char and nav keys)
  bool fn = false;   // Fn modifier held
  KeyAction action = KeyAction::Press;
};

// Map one raw key report to a semantic event. The M5Cardputer keyboard
// reports Enter/Backspace/Tab as separate booleans, printables as chars.
// Nav keys (; . , /) keep their char so a focused TextInput can use them
// as literal input; ` doubles as ESC and is not typeable.
inline KeyEvent mapKey(char raw, bool fn, bool enter, bool del, bool tab) {
  KeyEvent ev;
  ev.fn = fn;
  if (enter) { ev.code = KeyCode::Enter; return ev; }
  if (del)   { ev.code = KeyCode::Backspace; return ev; }
  if (tab)   { ev.code = KeyCode::Tab; return ev; }
  if (!raw) { ev.code = KeyCode::None; return ev; }
  switch (raw) {
    case '`': ev.code = KeyCode::Esc; return ev;
    case ';': ev.code = KeyCode::Up; break;
    case '.': ev.code = KeyCode::Down; break;
    case ',': ev.code = KeyCode::Left; break;
    case '/': ev.code = KeyCode::Right; break;
    default:  ev.code = KeyCode::Char; break;
  }
  ev.ch = raw;
  return ev;
}
