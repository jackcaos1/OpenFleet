#include "formats/Bmp.h"

#include <cstring>

namespace fleet {
namespace {
uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
}  // namespace

bool DecodeBmp(const uint8_t* d, size_t size, Image* out, std::string* err) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return false;
  };
  if (size < 54 || d[0] != 'B' || d[1] != 'M') return fail("kein BMP");
  const uint32_t off = U32(d + 10), hs = U32(d + 14);
  if (hs < 40) return fail("BMP-Kopf nicht unterstuetzt");
  const int32_t w = static_cast<int32_t>(U32(d + 18)), h0 = static_cast<int32_t>(U32(d + 22));
  const uint16_t bpp = U16(d + 28);
  const uint32_t comp = U32(d + 30);
  if (comp != 0 || (bpp != 24 && bpp != 32)) return fail("nur unkomprimiertes 24/32-Bit-BMP");
  const bool bottom_up = h0 > 0;
  const int h = h0 > 0 ? h0 : -h0;
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return fail("BMP-Groesse");
  const size_t stride = ((static_cast<size_t>(w) * bpp / 8) + 3) & ~static_cast<size_t>(3);
  if (off + stride * static_cast<size_t>(h) > size) return fail("BMP abgeschnitten");
  out->width = w;
  out->height = h;
  out->has_alpha = false;
  out->rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 255);
  for (int y = 0; y < h; ++y) {
    const uint8_t* row = d + off + stride * static_cast<size_t>(bottom_up ? h - 1 - y : y);
    uint8_t* o = &out->rgba[static_cast<size_t>(y) * static_cast<size_t>(w) * 4];
    for (int x = 0; x < w; ++x) {
      const uint8_t* p = row + static_cast<size_t>(x) * (bpp / 8);
      o[x * 4] = p[2];
      o[x * 4 + 1] = p[1];
      o[x * 4 + 2] = p[0];
      if (p[2] == 255 && p[1] == 0 && p[0] == 255) {  // Farbschluessel Magenta (255, 0, 255) = durchsichtig (Ecken der Tastengrafiken)
        o[x * 4 + 3] = 0;
        out->has_alpha = true;
      }
    }
  }
  return true;
}

}  // namespace fleet
