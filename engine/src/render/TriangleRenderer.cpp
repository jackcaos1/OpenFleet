#include "render/TriangleRenderer.h"

#include "core/Log.h"

namespace fleet {

namespace {

const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
out vec3 v_color;
void main() {
  v_color = a_color;
  gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 v_color;
uniform vec3 u_tint;
out vec4 o_color;
void main() {
  o_color = vec4(v_color * u_tint, 1.0);
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

// x, y, r, g, b
const GLfloat kVertices[] = {
    0.0f,  0.8f,  1.0f, 0.2f, 0.2f,
    -0.7f, -0.4f, 0.2f, 1.0f, 0.2f,
    0.7f,  -0.4f, 0.2f, 0.4f, 1.0f,
};

}  // namespace

bool TriangleRenderer::Init() {
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
  mvp_location_ = glGetUniformLocation(program_, "u_mvp");
  tint_location_ = glGetUniformLocation(program_, "u_tint");

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  const GLsizei stride = 5 * sizeof(GLfloat);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
  glBindVertexArray(0);
  return true;
}

void TriangleRenderer::Shutdown() {
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  vbo_ = vao_ = program_ = 0;
  mvp_location_ = tint_location_ = -1;
}

void TriangleRenderer::Draw(const Mat4& mvp, const float tint[3]) const {
  if (!program_) return;
  glUseProgram(program_);
  glUniformMatrix4fv(mvp_location_, 1, GL_FALSE, mvp.m);
  glUniform3fv(tint_location_, 1, tint);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

}  // namespace fleet
