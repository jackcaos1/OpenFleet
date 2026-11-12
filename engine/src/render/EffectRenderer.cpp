#include "render/EffectRenderer.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "core/Log.h"
#include "formats/TextureLookup.h"

namespace fleet {

namespace {

const char* kVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;  // Ansichtsraum
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_fade;
uniform mat4 u_proj;
out vec2 v_uv;
out float v_fade;
void main() {
  v_uv = a_uv;
  v_fade = a_fade;
  gl_Position = u_proj * vec4(a_pos, 1.0);
}
)";

const char* kFs = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
in float v_fade;
uniform sampler2D u_tex;
uniform vec3 u_tint;
out vec4 o_color;
void main() { o_color = vec4(texture(u_tex, v_uv).rgb * v_fade * u_tint, 1.0); }
)";

GLuint Compile(GLenum type, const char* source) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &source, nullptr);
  glCompileShader(s);
  GLint ok = GL_FALSE;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512] = {0};
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    FLEET_LOGE("Effekt-Shader-Fehler: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

struct V3 {
  float x, y, z;
};
V3 Xf(const float* m, const float* p) {
  return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
          m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}
}  // namespace

bool EffectRenderer::Init() {
  Shutdown();
  GLuint vs = Compile(GL_VERTEX_SHADER, kVs), fs = Compile(GL_FRAGMENT_SHADER, kFs);
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
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }
  u_proj_ = glGetUniformLocation(program_, "u_proj");
  u_tex_ = glGetUniformLocation(program_, "u_tex");
  u_tint_ = glGetUniformLocation(program_, "u_tint");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(5 * sizeof(float)));
  glBindVertexArray(0);
  return true;
}

void EffectRenderer::Shutdown() {
  for (auto& kv : textures_) {
    if (kv.second) glDeleteTextures(1, &kv.second);
  }
  textures_.clear();
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  vbo_ = vao_ = program_ = 0;
}

GLuint EffectRenderer::Texture(const std::string& name, FileSource& src) {
  const std::string key = ToLowerAscii(name);
  auto it = textures_.find(key);
  if (it != textures_.end()) return it->second;
  GLuint id = 0;
  Image img;
  std::string err;
  if (LoadGameTexture(src, name, &img, &err)) {
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    FLEET_LOGW("Effekt-Textur nicht gefunden: %s", name.c_str());
  }
  textures_.emplace(key, id);
  return id;
}

void EffectRenderer::Draw(const Mat4& proj, const Mat4& vm, const std::vector<Shot>& shots, FileSource& src) {
  if (!program_ || shots.empty()) return;
  // Je Textur eine Liste von Dreiecken (x y z u v fade).
  std::map<GLuint, std::vector<float>> batches;
  const float* m = vm.m;
  for (const Shot& s : shots) {
    if (!s.sprite || s.sprite->ref_w <= 0 || s.sprite->ref_h <= 0) continue;
    const GLuint tex = Texture(s.sprite->file, src);
    if (!tex) continue;
    int col = 0, row = 0;
    if (s.anim) s.anim->FrameCell(s.age, &col, &row);
    const float u0 = (s.sprite->u + static_cast<float>(col) * s.sprite->w) / s.sprite->ref_w;
    const float v0 = (s.sprite->v + static_cast<float>(row) * s.sprite->h) / s.sprite->ref_h;
    const float u1 = u0 + s.sprite->w / s.sprite->ref_w, v1 = v0 + s.sprite->h / s.sprite->ref_h;
    const float rest = 1.0f - s.age / s.life;
    const float fade = std::min(1.0f, rest * 5.0f);  // kurzes Ausblenden am Ende
    V3 a, b, c, d;  // Ecken: a=Anfang/links-unten, in Reihenfolge (u0,v1) (u1,v1) (u0,v0) (u1,v0)
    float uv[4][2] = {{u0, v1}, {u1, v1}, {u0, v0}, {u1, v0}};
    if (s.kind == Shot::Kind::Ball) {
      const V3 c0 = Xf(m, s.pos);
      const float h = s.width;  // Durchmesser = width, halbe Kante
      a = {c0.x - h, c0.y - h, c0.z};
      b = {c0.x + h, c0.y - h, c0.z};
      c = {c0.x - h, c0.y + h, c0.z};
      d = {c0.x + h, c0.y + h, c0.z};
    } else {
      float tail[3], head[3];
      if (s.kind == Shot::Kind::Beam) {
        for (int k = 0; k < 3; ++k) {
          tail[k] = s.pos[k];
          head[k] = s.pos[k] + s.dir[k] * s.length;
        }
      } else {
        for (int k = 0; k < 3; ++k) {
          head[k] = s.pos[k];
          tail[k] = s.pos[k] - s.dir[k] * s.length;
        }
      }
      const V3 t = Xf(m, tail), h = Xf(m, head);
      V3 dv = {h.x - t.x, h.y - t.y, h.z - t.z};
      // Seitenvektor senkrecht zur Richtung und zur Blickachse (zur Kamera gedreht)
      V3 side = {dv.y * 1.0f - 0.0f, -dv.x * 1.0f, 0.0f};  // cross(dv, (0,0,1))
      float len = std::sqrt(side.x * side.x + side.y * side.y);
      if (len < 1e-5f) side = {1, 0, 0}, len = 1;
      const float hw = 0.5f * s.width / len;
      side = {side.x * hw, side.y * hw, 0};
      a = {t.x - side.x, t.y - side.y, t.z};
      b = {h.x - side.x, h.y - side.y, h.z};
      c = {t.x + side.x, t.y + side.y, t.z};
      d = {h.x + side.x, h.y + side.y, h.z};
    }
    const V3 p[4] = {a, b, c, d};
    std::vector<float>& v = batches[tex];
    for (int idx : {0, 1, 2, 1, 3, 2}) v.insert(v.end(), {p[idx].x, p[idx].y, p[idx].z, uv[idx][0], uv[idx][1], fade});
  }
  glUseProgram(program_);
  glUniformMatrix4fv(u_proj_, 1, GL_FALSE, proj.m);
  glUniform1i(u_tex_, 0);
  glUniform3f(u_tint_, 1.0f, 1.0f, 1.0f);
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  for (auto& kv : batches) {
    glBindTexture(GL_TEXTURE_2D, kv.first);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kv.second.size() * sizeof(float)), kv.second.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(kv.second.size() / 6));
  }
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
}

void EffectRenderer::DrawMesh(const Mat4& proj, const Mat4& vm, const std::vector<float>& vertices, const std::string& texture, const float tint[3], float fade, FileSource& src) {
  if (!program_ || vertices.empty() || fade <= 0) return;
  const GLuint tex = Texture(texture, src);
  if (!tex) return;
  std::vector<float> v;  // x y z u v fade, im Ansichtsraum
  v.reserve(vertices.size() / 5 * 6);
  for (size_t i = 0; i + 4 < vertices.size(); i += 5) {
    const V3 p = Xf(vm.m, &vertices[i]);
    v.insert(v.end(), {p.x, p.y, p.z, vertices[i + 3], vertices[i + 4], fade});
  }
  glUseProgram(program_);
  glUniformMatrix4fv(u_proj_, 1, GL_FALSE, proj.m);
  glUniform1i(u_tex_, 0);
  glUniform3f(u_tint_, tint[0], tint[1], tint[2]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);  // Original-Windung unbekannt: beide Seiten
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);  // additiv (Annahme nach der Zeichenkette "additive" im Ladecode)
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size() / 6));
  glUniform3f(u_tint_, 1.0f, 1.0f, 1.0f);
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
}

}  // namespace fleet
