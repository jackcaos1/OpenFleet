#include "render/ModelRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "core/Log.h"
#include "formats/TextureLookup.h"

namespace fleet {

namespace {

const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
uniform mat4 u_nrot;
uniform mat4 u_model;
uniform mat3 u_vrot;
uniform highp vec3 u_origin;
out highp vec3 v_wp;
out vec3 v_normal;
out vec3 v_pos;
out vec2 v_uv;
void main() {
  v_normal = mat3(u_nrot) * a_normal;
  v_pos = mat3(u_nrot) * a_pos;  // nur fuer Ableitungen (Bump), Verschiebung faellt dabei weg
  v_uv = a_uv;
  v_wp = u_vrot * ((u_model * vec4(a_pos, 1.0)).xyz - u_origin);  // Weltlage relativ zum Blickpunkt, in den Raum der Normalen
  gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 v_normal;
in vec3 v_pos;
in vec2 v_uv;
in highp vec3 v_wp;
uniform highp int u_np;
uniform highp vec3 u_ppos[32];  // Lichtlage relativ zum Blickpunkt (Raum der Normalen)
uniform vec3 u_pcol[32];
uniform highp vec2 u_prng[32];  // Abfallbeginn, Abfallbereich
uniform sampler2D u_tex;
uniform vec3 u_color;
uniform float u_use_tex;
uniform float u_alpha_mode;
uniform float u_unlit;
uniform float u_opacity;
uniform vec3 u_amb;
uniform vec3 u_mamb;
uniform vec3 u_mdiff;
uniform vec3 u_mspec;
uniform float u_mpow;
uniform int u_nl;
uniform vec3 u_ldir[4];
uniform vec3 u_lcol[4];
out vec4 o_color;
void main() {
  vec4 t = mix(vec4(1.0), texture(u_tex, v_uv), u_use_tex);
  // u_alpha_mode: 0 ignorieren, 1 Test, 2 Mischen, 3 Alpha=Emission, 4 Emission invertiert, 5 Alpha=Hoehenkarte (Bump)
  if (u_alpha_mode > 0.5 && u_alpha_mode < 1.5 && t.a < 0.5) discard;
  vec3 n = normalize(v_normal);
  if (u_alpha_mode > 4.5) {
    // Bump aus Alpha ohne Tangenten (Ableitungsverfahren): Hoehe h = Alpha
    vec3 dp1 = dFdx(v_pos);
    vec3 dp2 = dFdy(v_pos);
    float dh1 = dFdx(t.a);
    float dh2 = dFdy(t.a);
    vec3 r1 = cross(dp2, n);
    vec3 r2 = cross(n, dp1);
    float det = dot(dp1, r1);
    vec3 grad = sign(det) * (dh1 * r1 + dh2 * r2);
    n = normalize(abs(det) * n - 4.0 * grad);
  }
  vec3 shade;
  vec3 v_spec = vec3(0.0);
  if (u_nl < 0) {
    // ohne Szenenlicht (Modellbetrachter): einfache Festbeleuchtung
    shade = vec3(mix(0.35 + 0.65 * abs(dot(n, normalize(vec3(-0.4, 0.6, 0.7)))), 1.0, u_unlit));
  } else {
    vec3 acc = u_amb + u_mamb;
    vec3 spec = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
      if (i >= u_nl) break;
      float d = dot(n, u_ldir[i]);
      if (d > 0.0) acc += d * u_lcol[i] * u_mdiff;
      if (u_mpow > 0.0) {
        // Glanzlicht (Lichtmodell 2, Armada.exe 0x58dd90): Halbvektor aus Blickrichtung (zur Kamera) und Licht, Schlick-Naeherung
        float t = dot(n, normalize(vec3(0.0, 0.0, 1.0) + u_ldir[i]));
        if (t > 0.0 && d > 0.0) spec += (t / (u_mpow - u_mpow * t + t)) * u_lcol[i] * u_mspec;
      }
    }
    // Punktlichter (Armada.exe 0x58d7f0): nur diffus; voll bis Abfallbeginn, dann linear auf 0 ueber die Abfallreichweite
    for (int i = 0; i < 32; ++i) {
      if (i >= u_np) break;
      highp vec3 rel = u_ppos[i] - v_wp;
      highp float dist = length(rel);
      float reach = u_prng[i].x + u_prng[i].y;
      if (dist >= reach || dist < 1e-4) continue;
      float nd = dot(n, rel / dist);
      if (nd <= 0.0) continue;
      float f = dist < u_prng[i].x ? 1.0 : 1.0 - (dist - u_prng[i].x) / u_prng[i].y;
      acc += nd * f * u_pcol[i] * u_mdiff;
    }
    shade = mix(clamp(acc, 0.0, 1.0), vec3(1.0), u_unlit);
    v_spec = clamp(spec, 0.0, 1.0);
  }
  if (u_alpha_mode > 2.5 && u_alpha_mode < 4.5) {
    float em = u_alpha_mode < 3.5 ? t.a : 1.0 - t.a;
    shade = mix(shade, vec3(1.0), em);
  }
  o_color = vec4(t.rgb * u_color * shade + v_spec * (1.0 - u_unlit), (u_alpha_mode > 1.5 && u_alpha_mode < 2.5 ? t.a : 1.0) * u_opacity);
}
)";

GLuint Compile(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512] = {0};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    FLEET_LOGE("Shader-Fehler (%s): %s", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

bool ModelRenderer::Init() {
  Shutdown();
  GLuint vs = Compile(GL_VERTEX_SHADER, kVertexShader);
  GLuint fs = Compile(GL_FRAGMENT_SHADER, kFragmentShader);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return false;
  }
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512] = {0};
    glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
    FLEET_LOGE("Shader-Link-Fehler: %s", log);
    Shutdown();
    return false;
  }
  u_mvp_ = glGetUniformLocation(program_, "u_mvp");
  u_nrot_ = glGetUniformLocation(program_, "u_nrot");
  u_color_ = glGetUniformLocation(program_, "u_color");
  u_opacity_ = glGetUniformLocation(program_, "u_opacity");
  u_tex_ = glGetUniformLocation(program_, "u_tex");
  u_use_tex_ = glGetUniformLocation(program_, "u_use_tex");
  u_alpha_mode_ = glGetUniformLocation(program_, "u_alpha_mode");
  u_unlit_ = glGetUniformLocation(program_, "u_unlit");
  u_mspec_ = glGetUniformLocation(program_, "u_mspec");
  u_mpow_ = glGetUniformLocation(program_, "u_mpow");
  u_amb_ = glGetUniformLocation(program_, "u_amb");
  u_mamb_ = glGetUniformLocation(program_, "u_mamb");
  u_mdiff_ = glGetUniformLocation(program_, "u_mdiff");
  u_nl_ = glGetUniformLocation(program_, "u_nl");
  u_ldir_ = glGetUniformLocation(program_, "u_ldir");
  u_lcol_ = glGetUniformLocation(program_, "u_lcol");
  u_model_ = glGetUniformLocation(program_, "u_model");
  u_vrot_ = glGetUniformLocation(program_, "u_vrot");
  u_np_ = glGetUniformLocation(program_, "u_np");
  u_origin_ = glGetUniformLocation(program_, "u_origin");
  u_ppos_ = glGetUniformLocation(program_, "u_ppos");
  u_pcol_ = glGetUniformLocation(program_, "u_pcol");
  u_prng_ = glGetUniformLocation(program_, "u_prng");

  const unsigned char white[4] = {255, 255, 255, 255};
  glGenTextures(1, &white_);
  glBindTexture(GL_TEXTURE_2D, white_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  return true;
}

void ModelRenderer::ClearModel() {
  for (GpuBatch& b : batches_) {
    if (b.vbo) glDeleteBuffers(1, &b.vbo);
    if (b.ebo) glDeleteBuffers(1, &b.ebo);
    if (b.vao) glDeleteVertexArrays(1, &b.vao);
  }
  batches_.clear();
  ReleaseTextures();
}

void ModelRenderer::ReleaseTextures() {
  for (auto& kv : textures_) {
    if (kv.second.id) glDeleteTextures(1, &kv.second.id);
  }
  textures_.clear();
}

void ModelRenderer::Shutdown() {
  ClearModel();
  if (white_) glDeleteTextures(1, &white_);
  if (program_) glDeleteProgram(program_);
  white_ = program_ = 0;
}

GLuint ModelRenderer::LoadTexture(const std::string& name, FileSource& src, bool* alpha, ModelLoadStats* stats) {
  const std::string key = ToLowerAscii(name);
  auto it = textures_.find(key);
  if (it != textures_.end()) {
    *alpha = it->second.alpha;
    return it->second.id;
  }
  TexInfo info;
  Image img;
  std::string err;
  if (LoadGameTexture(src, name, &img, &err)) {
    glGenTextures(1, &info.id);
    glBindTexture(GL_TEXTURE_2D, info.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    info.alpha = img.has_alpha;
    ++stats->textures_ok;
  } else {
    ++stats->textures_missing;
    FLEET_LOGW("Textur nicht gefunden: %s%s%s", name.c_str(), err.empty() ? "" : " | ", err.c_str());
  }
  textures_.emplace(key, info);
  *alpha = info.alpha;
  return info.id;
}

ModelLoadStats ModelRenderer::SetModel(const Model& model, FileSource& src) {
  ModelLoadStats stats;
  ClearModel();
  for (int k = 0; k < 3; ++k) ring_pivot_[k] = model.ore_pivot[k];
  for (const ModelBatch& mb : model.batches) {
    GpuBatch b;
    b.index_count = static_cast<GLsizei>(mb.indices.size());
    b.cull_back = mb.cull_back;
    b.ring = mb.ore_ring;
    {
      std::string nl = ToLowerAscii(mb.node);
      if (nl == "outerbeam") b.roll_sign = 1; else if (nl == "beam") b.roll_sign = -1;
      if (nl.size() > 3 && nl.compare(0, 3, "ore") == 0 && nl.find_first_not_of("0123456789", 3) == std::string::npos) b.ore_n = std::atoi(nl.c_str() + 3);
    }
    b.additive = mb.additive;
    for (int i = 0; i < 3; ++i) { b.color[i] = mb.color[i]; b.ambient[i] = mb.ambient[i]; b.specular[i] = mb.specular[i]; }
    b.power = mb.power;
    b.lighting = mb.lighting;
    if (!mb.texture.empty()) b.texture = LoadTexture(mb.texture, src, &b.has_alpha, &stats);
    b.unlit = mb.unlit;
    b.team = mb.team;
    b.cutout = mb.cutout;
    glGenVertexArrays(1, &b.vao);
    glBindVertexArray(b.vao);
    glGenBuffers(1, &b.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mb.vertices.size() * sizeof(float)), mb.vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &b.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mb.indices.size() * sizeof(uint32_t)), mb.indices.data(),
                 GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    batches_.push_back(b);
  }
  return stats;
}

namespace {
float g_vrot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
ModelRenderer::SceneLighting g_light;   // Dirs bereits in den Raum der Normalen (Kameradrehung) gedreht
}

void ModelRenderer::SetSceneLighting(const SceneLighting& l, const Mat4& r) {
  g_light = l;
  for (int k = 0; k < 9; ++k) g_vrot[k] = 0;
  for (int c = 0; c < 3; ++c)
    for (int rr = 0; rr < 3; ++rr) g_vrot[c * 3 + rr] = r.m[c * 4 + rr];  // Spalte c; (mat3 spaltenweise)
  for (int i = 0; i < l.count && i < 4; ++i) {
    const float* d = l.dir[i];
    g_light.dir[i][0] = r.m[0] * d[0] + r.m[4] * d[1] + r.m[8] * d[2];
    g_light.dir[i][1] = r.m[1] * d[0] + r.m[5] * d[1] + r.m[9] * d[2];
    g_light.dir[i][2] = r.m[2] * d[0] + r.m[6] * d[1] + r.m[10] * d[2];
  }
  for (int i = 0; i < l.pcount && i < 32; ++i) {  // Punktlichter: relativ zum Blickpunkt, in den Raum der Normalen gedreht
    const float d[3] = {l.ppos[i][0] - l.origin[0], l.ppos[i][1] - l.origin[1], l.ppos[i][2] - l.origin[2]};
    g_light.ppos[i][0] = r.m[0] * d[0] + r.m[4] * d[1] + r.m[8] * d[2];
    g_light.ppos[i][1] = r.m[1] * d[0] + r.m[5] * d[1] + r.m[9] * d[2];
    g_light.ppos[i][2] = r.m[2] * d[0] + r.m[6] * d[1] + r.m[10] * d[2];
  }
}

void ModelRenderer::Draw(const Mat4& mvp, const Mat4& normal_rot, const Mat4* world) const {
  if (!program_ || batches_.empty()) return;
  glUseProgram(program_);
  glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
  glUniformMatrix4fv(u_nrot_, 1, GL_FALSE, normal_rot.m);
  glUniform1i(u_tex_, 0);
  glUniform1f(u_opacity_, opacity_);
  const bool use_light = lit_ && g_light.enabled;
  glUniform1i(u_nl_, use_light ? g_light.count : -1);
  if (use_light) {
    glUniform3fv(u_amb_, 1, g_light.ambient);
    glUniform3fv(u_ldir_, 4, &g_light.dir[0][0]);
    glUniform3fv(u_lcol_, 4, &g_light.color[0][0]);
  }
  const int np = (use_light && world) ? g_light.pcount : 0;
  glUniform1i(u_np_, np);
  if (np > 0) {
    glUniformMatrix4fv(u_model_, 1, GL_FALSE, world->m);
    glUniformMatrix3fv(u_vrot_, 1, GL_FALSE, g_vrot);
    glUniform3fv(u_origin_, 1, g_light.origin);
    glUniform3fv(u_ppos_, 32, &g_light.ppos[0][0]);
    glUniform3fv(u_pcol_, 32, &g_light.pcol[0][0]);
    glUniform2fv(u_prng_, 32, &g_light.prng[0][0]);
  }
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_DEPTH_TEST);
  glFrontFace(GL_CCW);
  std::vector<GLsizei> draw_count(batches_.size());
  {
    int left = tri_limit_;
    for (size_t i = 0; i < batches_.size(); ++i) {
      const int tris = static_cast<int>(batches_[i].index_count / 3);
      if (left < 0) { draw_count[i] = batches_[i].index_count; continue; }
      const int use = std::min(tris, left);
      draw_count[i] = static_cast<GLsizei>(use * 3);
      left -= use;
    }
  }
  // Durchgang 0: undurchsichtig. Durchgang 1 (nur bei Alpha-Modus "Mischen"): Flaechen mit Alphatextur, ohne Tiefenschreiben.
  // Durchgang 2: additive Effektflaechen (derzeit ungenutzt).
  for (int pass = 0; pass < 3; ++pass) {
    if (pass == 1 && alpha_mode_ != 2) continue;
    if (pass == 0 && opacity_ < 1.0f) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (pass == 1) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    } else if (pass == 2) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      glDepthMask(GL_FALSE);
    }
    for (size_t bi = 0; bi < batches_.size(); ++bi) {
      const GpuBatch& b = batches_[bi];
      const bool transparent = alpha_mode_ == 2 && b.has_alpha && !b.cutout;
      const int my_pass = (b.additive || force_additive_) ? 2 : (transparent ? 1 : 0);
      if (my_pass != pass) continue;
      if (ore_fill_ >= 0.0f && b.ore_n > 0 && static_cast<float>(b.ore_n * 10) > ore_fill_) continue;   // 0x4583e0: not yet filled
      if (b.cull_back) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
      } else {
        glDisable(GL_CULL_FACE);
      }
      glBindTexture(GL_TEXTURE_2D, b.texture ? b.texture : white_);
      glUniform1f(u_use_tex_, b.texture ? 1.0f : 0.0f);
      // Borg-Ueberzuege schneiden immer per Alpha aus (Modus 1), alles andere folgt dem Umschalter.
      glUniform1f(u_alpha_mode_, !b.has_alpha ? 0.0f : (b.cutout ? 1.0f : static_cast<float>(alpha_mode_)));
      glUniform1f(u_unlit_, b.unlit ? 1.0f : 0.0f);
      static const float kWhite3[3] = {1, 1, 1};
      // Beleuchtet: Diffusfarbe geht ueber das Szenenlicht ein; unbeleuchtet (Lichtmodell 0) und ohne Szenenlicht wie bisher als Farbe
      static const float kOre[3] = {0.0f, 0.6f, 1.0f};
      glUniform3fv(u_color_, 1, (ore_fill_ >= 0.0f && b.ore_n > 0) ? kOre : (b.team ? team_ : ((use_light && !b.unlit) ? kWhite3 : b.color)));
      glUniform3fv(u_mamb_, 1, b.ambient);
      glUniform3fv(u_mdiff_, 1, b.color);
      glUniform3fv(u_mspec_, 1, b.specular);
      glUniform1f(u_mpow_, (b.lighting == 2 && !b.team) ? std::max(b.power, 1.0f) : 0.0f);
      if (b.ring && ring_angle_ != 0.0f) {
        const Mat4 rg = mvp * Mat4::Translation(ring_pivot_[0], ring_pivot_[1], ring_pivot_[2]) * Mat4::RotationY(ring_angle_) * Mat4::Translation(-ring_pivot_[0], -ring_pivot_[1], -ring_pivot_[2]);
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, rg.m);
      } else if (b.roll_sign != 0 && beam_roll_ != 0.0f) {
        const Mat4 rz = mvp * Mat4::RotationZ(b.roll_sign * beam_roll_);
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, rz.m);
      } else {
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
      }
      glBindVertexArray(b.vao);
      if (draw_count[bi] > 0) glDrawElements(GL_TRIANGLES, draw_count[bi], GL_UNSIGNED_INT, nullptr);
    }
  }
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
}

}  // namespace fleet
