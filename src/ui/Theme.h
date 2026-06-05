#pragma once
#include <M5GFX.h>

// All UI constants in one place.
namespace theme {
constexpr int kStatusBarH = 16;
constexpr int kRowH = 18;             // menu row height
constexpr int kPadX = 6;

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kFg = TFT_WHITE;
constexpr uint16_t kMuted = 0x8410;   // mid gray
constexpr uint16_t kAccent = 0x05FF;  // cyan-ish
constexpr uint16_t kBarBg = 0x0926;   // dark navy
constexpr uint16_t kDanger = TFT_RED;
constexpr uint16_t kOk = TFT_GREEN;

// CJK-capable font for SSIDs and labels.
inline const lgfx::IFont* font() { return &fonts::efontCN_12; }
inline const lgfx::IFont* fontBold() { return &fonts::efontCN_12_b; }
}  // namespace theme
