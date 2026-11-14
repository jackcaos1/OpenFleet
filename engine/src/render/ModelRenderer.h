#pragma once
// Zeichnet ein CPU-Modell (model/ModelBuilder.h) mit seinen Texturen. GL-Kontext muss aktuell sein.
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Math.h"
#include "io/FileSource.h"
#include "model/ModelBuilder.h"
#include "render/Gl.h"

namespace fleet {

struct ModelLoadStats {
  int textures_ok = 0;
  int textures_missing = 0;
};

class ModelRenderer {
 public:
  ModelRenderer() = default;
  ModelRenderer(const ModelRenderer&) = delete;
  ModelRenderer& operator=(const ModelRenderer&) = delete;
  ~ModelRenderer() { Shutdown(); }

  bool Init();
  void Shutdown();
  // Laedt Eckpunktdaten und Texturen auf die GPU. Ersetzt das bisherige Modell.
  ModelLoadStats SetModel(const Model& model, FileSource& src);
  void ClearModel();
  bool has_model() const { return !batches_.empty(); }
  // Alpha der 32-Bit-Texturen (Bedeutung ungeklaert): 0 ignorieren, 1 Alpha-Test (<0.5 verwerfen), 2 Mischen (transparent),
  // 3 Emission (hell wo Alpha hoch), 4 Emission invertiert, 5 Hoehenkarte/Bump.
  void SetAlphaMode(int mode) { alpha_mode_ = mode; }
  // Alle Flaechen additiv mischen (ohne Tiefenschreiben): Himmelskasten, der einen dahinterliegenden Sternenhimmel ergaenzt.
  void SetForceAdditive(bool on) { force_additive_ = on; }
  void SetTeamColor(float r, float g, float b) { team_[0] = r; team_[1] = g; team_[2] = b; }
  // Szenenlicht des Originals (Armada.exe 0x5a4410): Eckpunktfarbe = Karten-Ambient + Material-Ambient + Summe je Richtungslicht
  // max(L.n,0) * Lichtfarbe * Material-Diffus, je Kanal auf 0..1 begrenzt. dir = Richtung ZUM Licht (Weltraum, GL), view_rot = Kameradrehung.
  struct SceneLighting {
    bool enabled = false;
    float ambient[3] = {0, 0, 0};
    int count = 0;
    float dir[4][3] = {};
    float color[4][3] = {};
    int pcount = 0;             // Punktlichter (Geschosse), Weltkoordinaten
    float origin[3] = {0, 0, 0};  // Blickpunkt (Bezug der Punktlichtlagen, Genauigkeit)
    float ppos[32][3] = {};
    float pcol[32][3] = {};
    float prng[32][2] = {};      // Abfallbeginn, Abfallbereich
  };
  static void SetSceneLighting(const SceneLighting& l, const Mat4& view_rot);
  void SetOpacity(float a) { opacity_ = a; }   // < 1: all opaque faces are blended (cloaked ships)
  void SetLit(bool on) { lit_ = on; }   // false: Himmelskasten usw. ohne Szenenlicht
  // mvp: Projektion*Ansicht*Modell, normal_rot: nur die Drehung des Modells.
  void Draw(const Mat4& mvp, const Mat4& normal_rot, const Mat4* world = nullptr) const;  // world: Modell->Welt (fuer Punktlichter)

 private:
  struct GpuBatch {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei index_count = 0;
    GLuint texture = 0;
    bool has_alpha = false;
    bool unlit = false;
    bool team = false;
    bool cutout = false;
    bool cull_back = false;
    bool additive = false;
    float color[3] = {1, 1, 1};
    float ambient[3] = {0, 0, 0};
    float specular[3] = {0, 0, 0};
    float power = 0;
    int lighting = 1;
  };
  GLuint LoadTexture(const std::string& name, FileSource& src, bool* alpha, ModelLoadStats* stats);
  void ReleaseTextures();

  GLuint program_ = 0;
  GLint u_mvp_ = -1, u_nrot_ = -1, u_color_ = -1, u_tex_ = -1, u_use_tex_ = -1, u_alpha_mode_ = -1, u_unlit_ = -1;
  int alpha_mode_ = 0;
  bool lit_ = true;
  float opacity_ = 1.0f;
  GLint u_opacity_ = -1;
  GLint u_mspec_ = -1, u_mpow_ = -1, u_amb_ = -1, u_mamb_ = -1, u_mdiff_ = -1, u_nl_ = -1, u_ldir_ = -1, u_lcol_ = -1, u_model_ = -1, u_vrot_ = -1, u_np_ = -1, u_origin_ = -1, u_ppos_ = -1, u_pcol_ = -1, u_prng_ = -1;
  bool force_additive_ = false;
  float team_[3] = {1, 1, 1};
  GLuint white_ = 0;
  std::vector<GpuBatch> batches_;
  struct TexInfo {
    GLuint id = 0;
    bool alpha = false;
  };
  std::unordered_map<std::string, TexInfo> textures_;
};

}  // namespace fleet
