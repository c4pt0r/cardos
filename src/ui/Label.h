#pragma once
#include <M5GFX.h>

#include <string>

#include "Theme.h"

// One-line text helper.
namespace label {

inline void draw(M5Canvas& gfx, const std::string& text, int x, int y,
                 uint16_t color = theme::kFg,
                 textdatum_t datum = top_left) {
  gfx.setFont(theme::font());
  gfx.setTextColor(color);
  gfx.setTextDatum(datum);
  gfx.drawString(text.c_str(), x, y);
  gfx.setTextDatum(top_left);
}

}  // namespace label
