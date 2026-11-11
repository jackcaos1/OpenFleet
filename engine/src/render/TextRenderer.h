#pragma once
// Einblendung von Text (UTF-8) ueber dem Bild: eingebauter Monospace-Atlas (render/FontData.h), halbtransparenter Hintergrund.
#include <string>
#include <vector>

#include "render/Gl.h"

namespace fleet {

class TextRenderer {
 public:
  TextRenderer() = default;
  TextRenderer(const TextRenderer&) = delete;
  TextRenderer& operator=(const TextRenderer&) = delete;
  ~TextRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  // Zeichnet den Text mit der linken oberen Ecke bei (x, y) Pixeln. scale >= 1 (ganzzahlig fuer scharfe Kanten).
  // Zeilen enden bei '\n' oder, wenn max_width_px > 0, am Bildschirmrand (Zeichenumbruch).
  void Draw(const std::string& utf8, float x, float y, int scale, int screen_w, int screen_h, float max_width_px) const;
  // Wie Draw, aber mit wahlbarer Textfarbe; panel = halbtransparente Flaeche hinter dem Text in der Farbe bg (RGBA).
  void DrawStyled(const std::string& utf8, float x, float y, int scale, int screen_w, int screen_h, float max_width_px, bool panel,
                  const float bg[4], const float fg[3]) const;
  // Gefuelltes Rechteck (Pixel, Ursprung oben links) mit Farbe rgba.
  void FillRect(float x, float y, float w, float h, const float rgba[4], int screen_w, int screen_h) const;
  // Masse des Textblocks in Pixeln (wie Draw sie belegt).
  static void Measure(const std::string& utf8, int scale, float max_width_px, float* w, float* h);

 private:
  GLuint program_ = 0, vao_ = 0, vbo_ = 0, tex_ = 0;
  GLint u_screen_ = -1, u_tex_ = -1, u_bg_ = -1, u_fg_ = -1;
};

}  // namespace fleet
