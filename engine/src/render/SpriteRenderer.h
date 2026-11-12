#pragma once
// Zeichnet die Sprite-Knoten eines Modells (Positionslichter usw.) als additive Billboards mit Keyframe-Animation.
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Math.h"
#include "formats/Spr.h"
#include "io/FileSource.h"
#include "model/ModelBuilder.h"
#include "render/Gl.h"

namespace fleet {

class SpriteRenderer {
 public:
  SpriteRenderer() = default;
  SpriteRenderer(const SpriteRenderer&) = delete;
  SpriteRenderer& operator=(const SpriteRenderer&) = delete;
  ~SpriteRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  // Loest die Sprite-Knoten des Modells ueber die Bibliothek auf. Gibt die Zahl der darstellbaren Lichter zurueck.
  // Die Bibliothek muss bis zum naechsten SetNodes/Clear leben.
  int SetNodes(const Model& model, const SprLibrary& lib, FileSource& src);
  void Clear() { lights_.clear(); }
  int count() const { return static_cast<int>(lights_.size()); }
  void SetEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  // view_model: Ansicht*Modell, proj: Projektion. time in Sekunden. scale vergroessert Knotenpositionen und Spritegroessen (Explosionen).
  void Draw(const Mat4& proj, const Mat4& view_model, double time, float scale = 1.0f) const;

 private:
  struct Light {
    float pos[3];
    float size[2];
    float colour[3];
    float uv[4];  // u0 v0 u1 v1
    bool flat = false;  // true: Rechteck in der right/up-Ebene des Knotens (statt zur Kamera gedreht)
    float right[3] = {1, 0, 0}, up[3] = {0, 1, 0};
    GLuint texture = 0;
    const SprAnimation* anim = nullptr;
  };
  GLuint LoadTexture(const std::string& name, FileSource& src);
  void ReleaseTextures();

  GLuint program_ = 0, vao_ = 0;
  GLint u_proj_ = -1, u_center_ = -1, u_size_ = -1, u_colour_ = -1, u_uv_ = -1, u_ax_ = -1, u_ay_ = -1, u_tex_ = -1;
  bool enabled_ = true;
  std::vector<Light> lights_;
  std::unordered_map<std::string, GLuint> textures_;
};

}  // namespace fleet
