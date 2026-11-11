#include "formats/Tga.h"

#include "io/ByteReader.h"

namespace fleet {

namespace {
bool Fail(std::string* err, const char* msg) {
  if (err) *err = msg;
  return false;
}
}  // namespace

bool DecodeTga(const uint8_t* data, size_t size, Image* out, std::string* err) {
  ByteReader r(data, size);
  const uint8_t id_len = r.U8();
  const uint8_t cmap_type = r.U8();
  const uint8_t type = r.U8();
  const uint16_t cmap_first = r.U16();
  const uint16_t cmap_len = r.U16();
  const uint8_t cmap_depth = r.U8();
  r.U16();  // x-Ursprung
  r.U16();  // y-Ursprung
  const uint16_t w = r.U16();
  const uint16_t h = r.U16();
  const uint8_t bpp = r.U8();
  const uint8_t desc = r.U8();
  if (!r.ok()) return Fail(err, "TGA: Kopf zu kurz");
  if (w == 0 || h == 0 || w > 8192 || h > 8192) return Fail(err, "TGA: unplausible Groesse");
  r.Skip(id_len);

  std::vector<uint8_t> palette;  // RGB je Eintrag
  if (cmap_type == 1) {
    if (cmap_depth != 24 && cmap_depth != 32) return Fail(err, "TGA: Palettentiefe nicht unterstuetzt");
    const size_t bytes_per = cmap_depth / 8;
    const uint8_t* p = r.Bytes(static_cast<size_t>(cmap_len) * bytes_per);
    if (!p) return Fail(err, "TGA: Palette zu kurz");
    palette.assign(static_cast<size_t>(cmap_first + cmap_len) * 3, 0);
    for (size_t i = 0; i < cmap_len; ++i) {
      const uint8_t* e = p + i * bytes_per;  // BGR(A)
      uint8_t* d = &palette[(cmap_first + i) * 3];
      d[0] = e[2];
      d[1] = e[1];
      d[2] = e[0];
    }
  } else if (cmap_type != 0) {
    return Fail(err, "TGA: unbekannter Palettentyp");
  }

  const bool paletted = type == 1;
  if (!(paletted && bpp == 8 && cmap_type == 1) && !(type == 2 && (bpp == 24 || bpp == 32)))
    return Fail(err, "TGA: Typ/Tiefe nicht unterstuetzt");

  const size_t px_bytes = bpp / 8;
  const uint8_t* px = r.Bytes(static_cast<size_t>(w) * h * px_bytes);
  if (!px) return Fail(err, "TGA: Pixeldaten zu kurz");

  out->width = w;
  out->height = h;
  out->has_alpha = (!paletted && bpp == 32);
  out->rgba.assign(static_cast<size_t>(w) * h * 4, 255);
  const bool top_origin = (desc & 0x20) != 0;
  for (int y = 0; y < h; ++y) {
    const int src_y = top_origin ? y : (h - 1 - y);
    const uint8_t* row = px + static_cast<size_t>(src_y) * w * px_bytes;
    uint8_t* dst = &out->rgba[static_cast<size_t>(y) * w * 4];
    for (int x = 0; x < w; ++x) {
      if (paletted) {
        const size_t idx = row[x] * 3u;
        if (idx + 2 < palette.size()) {
          dst[x * 4 + 0] = palette[idx];
          dst[x * 4 + 1] = palette[idx + 1];
          dst[x * 4 + 2] = palette[idx + 2];
        }
      } else {
        const uint8_t* s = row + static_cast<size_t>(x) * px_bytes;
        dst[x * 4 + 0] = s[2];
        dst[x * 4 + 1] = s[1];
        dst[x * 4 + 2] = s[0];
        dst[x * 4 + 3] = bpp == 32 ? s[3] : 255;
      }
    }
  }
  return true;
}

}  // namespace fleet
