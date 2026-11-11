#pragma once
// Zeichnet die Rechtecke eines HudLayout (Texturausschnitte und einfarbige Flaechen) in Bildschirmpixeln.
#include <string>
#include <unordered_map>
#include <vector>

#include "game/Hud.h"
#include "io/FileSource.h"
#include "render/Gl.h"

namespace fleet {

class HudRenderer {
 public:
  HudRenderer() = default;
  HudRenderer(const HudRenderer&) = delete;
  HudRenderer& operator=(const HudRenderer&) = delete;
  ~HudRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  // Zeichnet alle Rechtecke in Reihenfolge; Texturen werden bei Bedarf aus src geladen (Name -> Textures/...).
  void Draw(const std::vector<HudDraw>& draws, FileSource& src, int screen_w, int screen_h);
  // Verwirft die geladenen Texturen (Kontextverlust, Wechsel der Rasse).
  void ReleaseTextures();

 private:
  GLuint Texture(const std::string& name, FileSource& src);

  GLuint program_ = 0, vao_ = 0, vbo_ = 0, white_ = 0;
  GLint u_screen_ = -1, u_tex_ = -1;
  std::unordered_map<std::string, GLuint> textures_;  // 0 = nicht ladbar
};

}  // namespace fleet
