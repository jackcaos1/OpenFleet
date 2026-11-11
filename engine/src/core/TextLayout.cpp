#include "core/TextLayout.h"

#include <cstdint>

#include "core/FontData.h"

namespace fleet {

namespace {

int GlyphIndex(uint32_t cp) {
  if (cp >= 32 && cp <= 126) return static_cast<int>(cp - 32);
  for (int i = 0; i < kFontExtraCount; ++i) {
    if (kFontExtra[i] == cp) return 95 + i;
  }
  return '?' - 32;
}

uint32_t NextCodepoint(const std::string& s, size_t* i) {
  const unsigned char c = static_cast<unsigned char>(s[*i]);
  int extra = 0;
  uint32_t cp = c;
  if (c >= 0xF0) { extra = 3; cp = c & 0x07; }
  else if (c >= 0xE0) { extra = 2; cp = c & 0x0F; }
  else if (c >= 0xC0) { extra = 1; cp = c & 0x1F; }
  if (extra == 0 || *i + static_cast<size_t>(extra) >= s.size()) {
    ++*i;
    return extra == 0 ? c : c;  // ungueltig/abgeschnitten: als Latin-1
  }
  for (int k = 1; k <= extra; ++k) {
    const unsigned char n = static_cast<unsigned char>(s[*i + static_cast<size_t>(k)]);
    if ((n & 0xC0) != 0x80) {
      ++*i;
      return c;
    }
    cp = (cp << 6) | (n & 0x3F);
  }
  *i += static_cast<size_t>(extra) + 1;
  return cp;
}

}  // namespace

std::vector<std::vector<int>> LayoutText(const std::string& utf8, int max_chars) {
  std::vector<std::vector<int>> lines(1);
  size_t i = 0;
  while (i < utf8.size()) {
    if (utf8[i] == '\r') { ++i; continue; }
    if (utf8[i] == '\n') { lines.emplace_back(); ++i; continue; }
    const uint32_t cp = NextCodepoint(utf8, &i);
    if (max_chars > 0 && static_cast<int>(lines.back().size()) >= max_chars) lines.emplace_back();
    lines.back().push_back(GlyphIndex(cp));
  }
  return lines;
}

}  // namespace fleet
