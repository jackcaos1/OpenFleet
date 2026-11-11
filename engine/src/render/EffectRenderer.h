#pragma once
// Zeichnet Waffeneffekte (Strahlen, Pulse, Torpedos aus game/FireDemo.h) als additive Flaechen mit Sprite-Texturen.
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Math.h"
#include "game/FireDemo.h"
#include "io/FileSource.h"
#include "render/Gl.h"

namespace fleet {

class EffectRenderer {
 public:
  EffectRenderer() = default;
  EffectRenderer(const EffectRenderer&) = delete;
  EffectRenderer& operator=(const EffectRenderer&) = delete;
  ~EffectRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  // Texturen werden beim ersten Gebrauch aus src geladen (Textures/RGB, Textures/Index8).
  void Draw(const Mat4& proj, const Mat4& view_model, const std::vector<Shot>& shots, FileSource& src);

 private:
  GLuint Texture(const std::string& name, FileSource& src);

  GLuint program_ = 0, vao_ = 0, vbo_ = 0;
  GLint u_proj_ = -1, u_tex_ = -1;
  std::unordered_map<std::string, GLuint> textures_;
};

}  // namespace fleet
