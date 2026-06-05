#pragma once
#include <M5GFX.h>

#include <string>

#include "../core/KeyEvent.h"

// Centered overlays drawn on top of whatever the app rendered.
namespace dialog {

// Non-interactive toast (message + optional spinner). Caller manages
// how long it stays up.
void paintToast(M5Canvas& gfx, const std::string& msg, bool spinner);

}  // namespace dialog

// Two-option modal confirm. Left/Right (or Up/Down) switches the option,
// Enter confirms, Esc cancels.
class ConfirmDialog {
 public:
  enum class Result { None, First, Second, Cancel };

  void reset(const std::string& msg, const std::string& first,
             const std::string& second);
  Result handleKey(const KeyEvent& ev);
  void render(M5Canvas& gfx) const;

 private:
  std::string msg_, first_, second_;
  int selected_ = 0;
};
