#include "core/Math.h"

#include <cmath>

namespace fleet {

Mat4 Mat4::Identity() { return Mat4(); }

Mat4 Mat4::Translation(float x, float y, float z) {
  Mat4 r;
  r.m[12] = x;
  r.m[13] = y;
  r.m[14] = z;
  return r;
}

Mat4 Mat4::RotationX(float radians) {
  Mat4 r;
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  return r;
}

Mat4 Mat4::RotationY(float radians) {
  Mat4 r;
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

Mat4 Mat4::Perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) {
  Mat4 r;
  const float f = 1.0f / std::tan(fov_y_radians * 0.5f);
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
  r.m[11] = -1.0f;
  r.m[14] = 2.0f * far_plane * near_plane / (near_plane - far_plane);
  r.m[15] = 0.0f;
  return r;
}

Mat4 Mat4::RotationZ(float radians) {
  Mat4 r;
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  r.m[0] = c;
  r.m[1] = s;
  r.m[4] = -s;
  r.m[5] = c;
  return r;
}

Mat4 Mat4::Ortho(float left, float right, float bottom, float top, float near_plane, float far_plane) {
  Mat4 r;
  r.m[0] = 2.0f / (right - left);
  r.m[5] = 2.0f / (top - bottom);
  r.m[10] = -2.0f / (far_plane - near_plane);
  r.m[12] = -(right + left) / (right - left);
  r.m[13] = -(top + bottom) / (top - bottom);
  r.m[14] = -(far_plane + near_plane) / (far_plane - near_plane);
  return r;
}

Mat4 Mat4::operator*(const Mat4& rhs) const {
  Mat4 out;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += m[k * 4 + row] * rhs.m[col * 4 + k];
      }
      out.m[col * 4 + row] = sum;
    }
  }
  return out;
}

Vec2 Mat4::TransformPoint(Vec2 p) const {
  return Vec2{m[0] * p.x + m[4] * p.y + m[12], m[1] * p.x + m[5] * p.y + m[13]};
}

}  // namespace fleet
