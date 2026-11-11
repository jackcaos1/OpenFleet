#include "render/MarkerRenderer.h"

#include "core/Log.h"

namespace fleet {

namespace {

const char* kVs = R"(#version 300 es
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_col;
uniform mat4 u_mvp;
uniform float u_size;
out vec3 v_col;
void main() {
  v_col = a_col;
  gl_PointSize = u_size;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

const char* kFs = R"(#version 300 es
precision mediump float;
in vec3 v_col;
out vec4 o_color;
void main() { o_color = vec4(v_col, 1.0); }
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
    FLEET_LOGE("Marker-Shader-Fehler: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

bool MarkerRenderer::Init() {
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
  u_mvp_ = glGetUniformLocation(program_, "u_mvp");
  u_size_ = glGetUniformLocation(program_, "u_size");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
  glBindVertexArray(0);
  return true;
}

void MarkerRenderer::Shutdown() {
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  vbo_ = vao_ = program_ = 0;
  point_count_ = 0;
}

void MarkerRenderer::Set(const std::vector<Marker>& markers, float line_length) {
  point_count_ = static_cast<int>(markers.size());
  if (!vbo_ || markers.empty()) return;
  // Zuerst alle Punkte, dann je Marker zwei Linienenden.
  std::vector<float> v;
  v.reserve(markers.size() * 18);
  for (const Marker& m : markers) v.insert(v.end(), {m.pos[0], m.pos[1], m.pos[2], m.color[0], m.color[1], m.color[2]});
  for (const Marker& m : markers) {
    v.insert(v.end(), {m.pos[0], m.pos[1], m.pos[2], m.color[0], m.color[1], m.color[2]});
    v.insert(v.end(), {m.pos[0] + m.dir[0] * line_length, m.pos[1] + m.dir[1] * line_length, m.pos[2] + m.dir[2] * line_length, m.color[0],
                       m.color[1], m.color[2]});
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
}

void MarkerRenderer::Draw(const Mat4& mvp, float point_size_px) const {
  if (!program_ || point_count_ == 0) return;
  glUseProgram(program_);
  glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
  glUniform1f(u_size_, point_size_px);
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, point_count_);
  glDrawArrays(GL_LINES, point_count_, point_count_ * 2);
  glBindVertexArray(0);
}

}  // namespace fleet
