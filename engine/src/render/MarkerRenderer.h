#pragma once
// Hilfsdarstellung: farbige Punkte und Linien im Modellraum (Hardpoints mit Richtung), ohne Tiefentest.
#include <vector>

#include "core/Math.h"
#include "render/Gl.h"

namespace fleet {

class MarkerRenderer {
 public:
  struct Marker {
    float pos[3];
    float dir[3];  // Richtung (Einheitsvektor); Linie der Laenge line_length
    float color[3];
  };
  MarkerRenderer() = default;
  MarkerRenderer(const MarkerRenderer&) = delete;
  MarkerRenderer& operator=(const MarkerRenderer&) = delete;
  ~MarkerRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  void Set(const std::vector<Marker>& markers, float line_length);
  void Clear() { point_count_ = 0; }
  int count() const { return point_count_; }
  void Draw(const Mat4& mvp, float point_size_px, bool depth_test = false) const;

 private:
  GLuint program_ = 0, vao_ = 0, vbo_ = 0;
  GLint u_mvp_ = -1, u_size_ = -1;
  int point_count_ = 0;
};

}  // namespace fleet
