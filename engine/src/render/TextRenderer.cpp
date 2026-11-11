#include "render/TextRenderer.h"

#include "core/Log.h"
#include <algorithm>

#include "core/FontData.h"
#include "core/TextLayout.h"

namespace fleet {

namespace {

const char* kVs = R"(#version 300 es
layout(location = 0) in vec2 a_pos;  // Pixel
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_bg;  // 1 = Hintergrundflaeche
uniform vec2 u_screen;
out vec2 v_uv;
out float v_bg;
void main() {
  v_uv = a_uv;
  v_bg = a_bg;
  gl_Position = vec4(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0, 0.0, 1.0);
}
)";

const char* kFs = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
in float v_bg;
uniform sampler2D u_tex;
uniform vec4 u_bg;
uniform vec3 u_fg;
out vec4 o_color;
void main() {
  if (v_bg > 0.5) {
    o_color = u_bg;
  } else {
    float a = texture(u_tex, v_uv).r;
    o_color = vec4(u_fg, a);
  }
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
    FLEET_LOGE("Text-Shader-Fehler: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

void TextRenderer::Measure(const std::string& utf8, int scale, float max_width_px, float* w, float* h) {
  const int max_chars = max_width_px > 0 ? static_cast<int>(max_width_px / static_cast<float>(kFontCellW * scale)) : 0;
  const auto lines = LayoutText(utf8, max_chars < 1 && max_width_px > 0 ? 1 : max_chars);
  size_t widest = 0;
  for (const auto& l : lines) widest = std::max(widest, l.size());
  *w = static_cast<float>(widest) * static_cast<float>(kFontCellW * scale);
  *h = static_cast<float>(lines.size()) * static_cast<float>(kFontCellH * scale);
}

bool TextRenderer::Init() {
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
  u_bg_ = glGetUniformLocation(program_, "u_bg");
  u_fg_ = glGetUniformLocation(program_, "u_fg");
  glGenTextures(1, &tex_);
  glBindTexture(GL_TEXTURE_2D, tex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kFontCols * kFontCellW, kFontRows * kFontCellH, 0, GL_RED, GL_UNSIGNED_BYTE, kFontAtlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei stride = 5 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(2 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(4 * sizeof(float)));
  glBindVertexArray(0);
  return true;
}

void TextRenderer::Shutdown() {
  if (tex_) glDeleteTextures(1, &tex_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  tex_ = vbo_ = vao_ = program_ = 0;
}

void TextRenderer::Draw(const std::string& utf8, float x, float y, int scale, int screen_w, int screen_h, float max_width_px) const {
  static const float kBg[4] = {0.0f, 0.0f, 0.0f, 0.55f}, kFg[3] = {1.0f, 1.0f, 1.0f};
  DrawStyled(utf8, x, y, scale, screen_w, screen_h, max_width_px, true, kBg, kFg);
}

void TextRenderer::FillRect(float x, float y, float w, float h, const float rgba[4], int screen_w, int screen_h) const {
  if (!program_) return;
  const float q[6][5] = {{x, y, 0, 0, 1}, {x + w, y, 0, 0, 1}, {x, y + h, 0, 0, 1}, {x + w, y, 0, 0, 1}, {x + w, y + h, 0, 0, 1}, {x, y + h, 0, 0, 1}};
  glUseProgram(program_);
  glUniform2f(u_screen_, static_cast<float>(screen_w), static_cast<float>(screen_h));
  glUniform1i(u_tex_, 0);
  glUniform4fv(u_bg_, 1, rgba);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

void TextRenderer::DrawStyled(const std::string& utf8, float x, float y, int scale, int screen_w, int screen_h, float max_width_px, bool panel,
                              const float bg[4], const float fg[3]) const {
  if (!program_ || utf8.empty()) return;
  if (scale < 1) scale = 1;
  const float cw = static_cast<float>(kFontCellW * scale), ch = static_cast<float>(kFontCellH * scale);
  int max_chars = max_width_px > 0 ? static_cast<int>(max_width_px / cw) : 0;
  if (max_width_px > 0 && max_chars < 1) max_chars = 1;
  const auto lines = LayoutText(utf8, max_chars);
  size_t widest = 0;
  for (const auto& l : lines) widest = std::max(widest, l.size());
  std::vector<float> v;
  auto quad = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float bg) {
    const float q[6][5] = {{x0, y0, u0, v0, bg}, {x1, y0, u1, v0, bg}, {x0, y1, u0, v1, bg},
                           {x1, y0, u1, v0, bg}, {x1, y1, u1, v1, bg}, {x0, y1, u0, v1, bg}};
    for (const auto& p : q) v.insert(v.end(), p, p + 5);
  };
  const float pad = 4.0f * static_cast<float>(scale);
  if (panel) quad(x - pad, y - pad, x + static_cast<float>(widest) * cw + pad, y + static_cast<float>(lines.size()) * ch + pad, 0, 0, 0, 0, 1.0f);
  const float aw = static_cast<float>(kFontCols * kFontCellW), ah = static_cast<float>(kFontRows * kFontCellH);
  for (size_t r = 0; r < lines.size(); ++r) {
    for (size_t c = 0; c < lines[r].size(); ++c) {
      const int g = lines[r][c];
      const float u0 = static_cast<float>((g % kFontCols) * kFontCellW) / aw, v0 = static_cast<float>((g / kFontCols) * kFontCellH) / ah;
      const float px = x + static_cast<float>(c) * cw, py = y + static_cast<float>(r) * ch;
      quad(px, py, px + cw, py + ch, u0, v0, u0 + static_cast<float>(kFontCellW) / aw, v0 + static_cast<float>(kFontCellH) / ah, 0.0f);
    }
  }
  glUseProgram(program_);
  glUniform2f(u_screen_, static_cast<float>(screen_w), static_cast<float>(screen_h));
  glUniform1i(u_tex_, 0);
  glUniform4fv(u_bg_, 1, bg);
  glUniform3fv(u_fg_, 1, fg);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size() / 5));
  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

}  // namespace fleet
