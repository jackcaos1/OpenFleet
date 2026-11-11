#pragma once

#include "core/Math.h"
#include "render/Gl.h"

namespace fleet {

// Zeichnet ein farbiges Dreieck. Dient in M0 nur dazu, die Kette Fenster -> GL-Kontext -> Shader -> Bild zu pruefen.
class TriangleRenderer {
 public:
  TriangleRenderer() = default;
  TriangleRenderer(const TriangleRenderer&) = delete;
  TriangleRenderer& operator=(const TriangleRenderer&) = delete;
  ~TriangleRenderer() { Shutdown(); }

  // GL-Kontext muss aktuell sein. Gibt false zurueck und loggt, wenn Shader nicht bauen.
  bool Init();
  void Shutdown();
  void Draw(const Mat4& mvp, const float tint[3]) const;

 private:
  GLuint program_ = 0;
  GLuint vao_ = 0;
  GLuint vbo_ = 0;
  GLint mvp_location_ = -1;
  GLint tint_location_ = -1;
};

}  // namespace fleet
