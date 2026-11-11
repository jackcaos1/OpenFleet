#include "render/SpriteRenderer.h"

#include <algorithm>

#include "core/Log.h"
#include "formats/TextureLookup.h"

namespace fleet {

namespace {

const char* kVs = R"(#version 300 es
uniform mat4 u_proj;
uniform vec3 u_center;  // Ansichtsraum
uniform vec2 u_size;
uniform vec3 u_ax;  // Achsen des Rechtecks im Ansichtsraum
uniform vec3 u_ay;
uniform vec4 u_uv;
out vec2 v_uv;
void main() {
  vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
  vec3 p = u_center + u_ax * ((c.x - 0.5) * u_size.x) + u_ay * ((c.y - 0.5) * u_size.y);
  v_uv = vec2(mix(u_uv.x, u_uv.z, c.x), mix(u_uv.w, u_uv.y, c.y));
  gl_Position = u_proj * vec4(p, 1.0);
}
)";

const char* kFs = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec3 u_colour;
out vec4 o_color;
void main() {
  o_color = vec4(texture(u_tex, v_uv).rgb * u_colour, 1.0);
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
    FLEET_LOGE("Sprite-Shader-Fehler: %s", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

bool SpriteRenderer::Init() {
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
    FLEET_LOGE("Sprite-Programm konnte nicht gelinkt werden");
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }
  u_proj_ = glGetUniformLocation(program_, "u_proj");
  u_center_ = glGetUniformLocation(program_, "u_center");
  u_size_ = glGetUniformLocation(program_, "u_size");
  u_colour_ = glGetUniformLocation(program_, "u_colour");
  u_uv_ = glGetUniformLocation(program_, "u_uv");
  u_ax_ = glGetUniformLocation(program_, "u_ax");
  u_ay_ = glGetUniformLocation(program_, "u_ay");
  u_tex_ = glGetUniformLocation(program_, "u_tex");
  glGenVertexArrays(1, &vao_);
  return true;
}

void SpriteRenderer::ReleaseTextures() {
  for (auto& kv : textures_) {
    if (kv.second) glDeleteTextures(1, &kv.second);
  }
  textures_.clear();
}

void SpriteRenderer::Shutdown() {
  lights_.clear();
  ReleaseTextures();
  if (vao_) glDeleteVertexArrays(1, &vao_);
  vao_ = 0;
  if (program_) glDeleteProgram(program_);
  program_ = 0;
}

GLuint SpriteRenderer::LoadTexture(const std::string& name, FileSource& src) {
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
    FLEET_LOGW("Sprite-Textur nicht gefunden: %s", name.c_str());
  }
  textures_.emplace(key, id);
  return id;
}

int SpriteRenderer::SetNodes(const Model& model, const SprLibrary& lib, FileSource& src) {
  lights_.clear();
  for (const ModelSpriteNode& n : model.sprite_nodes) {
    const SprNodeDef* d = lib.ResolveNode(n.name);
    if (!d) continue;
    const SprSprite* sp = lib.FindSprite(d->sprite);
    if (!sp || sp->ref_w <= 0 || sp->ref_h <= 0) continue;
    Light l;
    for (int k = 0; k < 3; ++k) {
      l.pos[k] = n.pos[k];
      l.colour[k] = d->colour[k];
    }
    l.size[0] = d->w;
    l.size[1] = d->h;
    // Nicht-quadratische Flaechen ohne billboard-Flag (z. B. der 7x18-Lichtfleck whiteconstbig) liegen in der Ebene des Knotens.
    l.flat = !d->billboard && d->w != d->h;
    for (int k = 0; k < 3; ++k) {
      l.right[k] = n.right[k];
      l.up[k] = n.up[k];
    }
    l.uv[0] = sp->u / sp->ref_w;
    l.uv[1] = sp->v / sp->ref_h;
    l.uv[2] = (sp->u + sp->w) / sp->ref_w;
    l.uv[3] = (sp->v + sp->h) / sp->ref_h;
    l.texture = LoadTexture(sp->file, src);
    if (!l.texture) continue;
    l.anim = d->animation == "const" ? nullptr : lib.FindAnimation(d->animation);
    lights_.push_back(l);
  }
  return count();
}

void SpriteRenderer::Draw(const Mat4& proj, const Mat4& vm, double time) const {
  if (!enabled_ || !program_ || lights_.empty()) return;
  glUseProgram(program_);
  glUniformMatrix4fv(u_proj_, 1, GL_FALSE, proj.m);
  glUniform1i(u_tex_, 0);
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glBindVertexArray(vao_);
  const float* m = vm.m;
  for (const Light& l : lights_) {
    float colour[3] = {l.colour[0], l.colour[1], l.colour[2]};
    if (l.anim) {
      float v[3];
      l.anim->Eval(static_cast<float>(time), v);
      if (l.anim->field == "draw") {
        if (v[0] < 0.5f) continue;
      } else if (l.anim->field == "colour") {
        // Farb-Kanal des Originals (Armada.exe 0x577760): die Animation ERSETZT die Knotenfarbe (auf 0..1 begrenzt), sie multipliziert nicht
        for (int k = 0; k < 3; ++k) colour[k] = std::min(1.0f, std::max(0.0f, v[k]));
      }
    }
    const float cx = m[0] * l.pos[0] + m[4] * l.pos[1] + m[8] * l.pos[2] + m[12];
    const float cy = m[1] * l.pos[0] + m[5] * l.pos[1] + m[9] * l.pos[2] + m[13];
    const float cz = m[2] * l.pos[0] + m[6] * l.pos[1] + m[10] * l.pos[2] + m[14];
    glUniform3f(u_center_, cx, cy, cz);
    if (l.flat) {
      // Um 90 Grad gedreht: die Breite (x) laeuft entlang der Knoten-Achse "up", die Hoehe (y) entlang "right"
      // (so liegt der Lichtfleck quer zur Schiffslaenge und beleuchtet die Untertasse gleichmaessig).
      glUniform3f(u_ax_, m[0] * l.up[0] + m[4] * l.up[1] + m[8] * l.up[2], m[1] * l.up[0] + m[5] * l.up[1] + m[9] * l.up[2],
                  m[2] * l.up[0] + m[6] * l.up[1] + m[10] * l.up[2]);
      glUniform3f(u_ay_, m[0] * l.right[0] + m[4] * l.right[1] + m[8] * l.right[2], m[1] * l.right[0] + m[5] * l.right[1] + m[9] * l.right[2],
                  m[2] * l.right[0] + m[6] * l.right[1] + m[10] * l.right[2]);
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-2.0f, -2.0f);
    } else {
      glUniform3f(u_ax_, 1, 0, 0);
      glUniform3f(u_ay_, 0, 1, 0);
    }
    glUniform2f(u_size_, l.size[0], l.size[1]);
    glUniform3fv(u_colour_, 1, colour);
    glUniform4fv(u_uv_, 1, l.uv);
    glBindTexture(GL_TEXTURE_2D, l.texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (l.flat) glDisable(GL_POLYGON_OFFSET_FILL);
  }
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
}

}  // namespace fleet
