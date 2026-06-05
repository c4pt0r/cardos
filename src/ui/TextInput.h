#pragma once
#include <M5GFX.h>

#include <string>

#include "../core/KeyEvent.h"

// Single-line text input. Plaintext by default; Tab toggles masking.
class TextInput {
 public:
  enum class Result { None, Submit, Cancel };

  void reset(const std::string& prompt);
  Result handleKey(const KeyEvent& ev);
  const std::string& text() const { return text_; }
  void render(M5Canvas& gfx, int x, int y, int w) const;

 private:
  std::string prompt_;
  std::string text_;
  bool masked_ = false;
};
