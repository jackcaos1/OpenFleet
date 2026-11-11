#include "render/HudRenderer.h"

#include <algorithm>

#include "core/Log.h"
#include "formats/TextureLookup.h"

namespace fleet {

namespace {

const char* kVs = R"(#version 300 es
layout(location = 0) in vec2 a_pos;  // Pixel
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
uniform vec2 u_screen;
out vec2 v_uv;
out vec4 v_color;
void main() {
  v_uv = a_uv;
  v_color = a_color;
  gl_Position = vec4(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0, 0.0, 1.0);
}
)";

const char* kFs = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
in vec4 v_color;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  o_color = texture(u_tex, v_uv) * v_color;
}
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
    FLEET_LOGE("HUD-Shader-Fehler: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

bool HudRenderer::Init() {
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
  u_screen_ = glGetUniformLocation(program_, "u_screen");
  u_tex_ = glGetUniformLocation(program_, "u_tex");
  const uint8_t white[4] = {255, 255, 255, 255};
  glGenTextures(1, &white_);
  glBindTexture(GL_TEXTURE_2D, white_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei stride = 8 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(2 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(4 * sizeof(float)));
  glBindVertexArray(0);
  return true;
}

void HudRenderer::ReleaseTextures() {
  for (auto& kv : textures_)
    if (kv.second) glDeleteTextures(1, &kv.second);
  textures_.clear();
}

void HudRenderer::Shutdown() {
  ReleaseTextures();
  if (white_) glDeleteTextures(1, &white_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  white_ = vbo_ = vao_ = program_ = 0;
}

GLuint HudRenderer::Texture(const std::string& name, FileSource& src) {
  const std::string key = ToLowerAscii(name);
  auto it = textures_.find(key);
  if (it != textures_.end()) return it->second;
  GLuint tex = 0;
  Image img;
  std::string err;
  if (LoadGameTexture(src, name, &img, &err) && img.width > 0 && img.height > 0) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    FLEET_LOGW("HUD-Textur fehlt: %s", name.c_str());
  }
  textures_[key] = tex;
  return tex;
}

void HudRenderer::Draw(const std::vector<HudDraw>& draws, FileSource& src, int screen_w, int screen_h) {
  if (!program_ || draws.empty()) return;
  glUseProgram(program_);
  glUniform2f(u_screen_, static_cast<float>(screen_w), static_cast<float>(screen_h));
  glUniform1i(u_tex_, 0);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);

  std::vector<float> batch;
  GLuint cur_tex = 0;
  bool cur_add = false, have = false;
  auto flush = [&]() {
    if (batch.empty()) return;
    glBindTexture(GL_TEXTURE_2D, cur_tex);
    glBlendFunc(GL_SRC_ALPHA, cur_add ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(batch.size() * sizeof(float)), batch.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.size() / 8));
    batch.clear();
  };
  for (const HudDraw& d : draws) {
    GLuint tex = d.tex.empty() ? white_ : Texture(d.tex, src);
    if (!tex || d.w <= 0 || d.h <= 0) continue;
    if (!have || tex != cur_tex || d.additive != cur_add) {
      flush();
      cur_tex = tex;
      cur_add = d.additive;
      have = true;
    }
    const float x0 = d.x, y0 = d.y, x1 = d.x + d.w, y1 = d.y + d.h;
    const float u0 = d.tex.empty() ? 0.5f : d.u0, v0 = d.tex.empty() ? 0.5f : d.v0, u1 = d.tex.empty() ? 0.5f : d.u1, v1 = d.tex.empty() ? 0.5f : d.v1;
    const float* c = d.rgba;
    const float v[6][8] = {{x0, y0, u0, v0, c[0], c[1], c[2], c[3]}, {x1, y0, u1, v0, c[0], c[1], c[2], c[3]},
                           {x1, y1, u1, v1, c[0], c[1], c[2], c[3]}, {x0, y0, u0, v0, c[0], c[1], c[2], c[3]},
                           {x1, y1, u1, v1, c[0], c[1], c[2], c[3]}, {x0, y1, u0, v1, c[0], c[1], c[2], c[3]}};
    for (const auto& row : v) batch.insert(batch.end(), row, row + 8);
  }
  flush();
  glBindVertexArray(0);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

}  // namespace fleet
