#include "ProgressBar.h"

#include <string>

#include "Theme.h"

void ProgressBar::setValue(int pct) {
  pct_ = pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

void ProgressBar::render(M5Canvas& gfx, int x, int y, int w, int h,
                         bool showPercent) const {
  gfx.drawRect(x, y, w, h, theme::kMuted);
  int fill = (w - 4) * pct_ / 100;
  if (fill > 0) gfx.fillRect(x + 2, y + 2, fill, h - 4, theme::kAccent);
  if (showPercent) {
    gfx.setFont(theme::font());
    gfx.setTextDatum(middle_center);
    gfx.setTextColor(theme::kFg);
    gfx.drawString((std::to_string(pct_) + "%").c_str(), x + w / 2,
                   y + h / 2);
    gfx.setTextDatum(top_left);
  }
}
