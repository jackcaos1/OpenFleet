#pragma once
// Kleine Mathe-Bausteine. Matrizen sind spaltenweise abgelegt (wie OpenGL), Punkte sind Spaltenvektoren.

namespace fleet {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  static Mat4 Identity();
  static Mat4 Translation(float x, float y, float z);
  static Mat4 RotationX(float radians);
  static Mat4 RotationY(float radians);
  static Mat4 RotationZ(float radians);
  static Mat4 Perspective(float fov_y_radians, float aspect, float near_plane, float far_plane);
  static Mat4 Ortho(float left, float right, float bottom, float top, float near_plane, float far_plane);

  // (a * b) wirkt auf einen Punkt zuerst mit b, dann mit a.
  Mat4 operator*(const Mat4& rhs) const;

  // Wendet die Matrix auf (x, y, 0, 1) an und gibt x, y zurueck (keine Perspektivdivision).
  Vec2 TransformPoint(Vec2 p) const;
};

}  // namespace fleet
