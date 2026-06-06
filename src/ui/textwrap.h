#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Pure greedy word-wrap with an injected width-measure function so the
// algorithm is host-testable (the device passes gfx.textWidth).
// UTF-8 aware: multi-byte sequences are never split. Wraps at spaces when
// possible, otherwise at any glyph (CJK text wraps per glyph naturally).
using MeasureFn = std::function<int(const std::string&)>;

inline std::vector<std::string> wrapText(const std::string& text, int width,
                                         const MeasureFn& measure) {
  std::vector<std::string> lines;
  std::string line;
  size_t lastSpace = std::string::npos;  // byte index in `line`

  auto flush = [&]() {
    lines.push_back(line);
    line.clear();
    lastSpace = std::string::npos;
  };

  for (size_t i = 0; i < text.size();) {
    if (text[i] == '\n') {
      flush();
      i++;
      continue;
    }
    // Glyph = one UTF-8 sequence.
    size_t glyphLen = 1;
    uint8_t lead = (uint8_t)text[i];
    if (lead >= 0xF0) glyphLen = 4;
    else if (lead >= 0xE0) glyphLen = 3;
    else if (lead >= 0xC0) glyphLen = 2;
    std::string glyph = text.substr(i, glyphLen);

    if (measure(line + glyph) > width && !line.empty()) {
      if (glyph == " ") {  // wrap point itself: drop the space, new line
        flush();
        i += glyphLen;
        continue;
      }
      if (lastSpace != std::string::npos) {
        // Break at the last space; carry the partial word to the next line.
        std::string carry = line.substr(lastSpace + 1);
        line.resize(lastSpace + 1);
        flush();
        line = carry;
      } else {
        flush();
      }
    }
    if (glyph == " ") lastSpace = line.size();
    line += glyph;
    i += glyphLen;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}
