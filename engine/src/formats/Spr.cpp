#include "formats/Spr.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "io/FileSource.h"

namespace fleet {

namespace {

std::vector<std::string> Words(std::string s) {
  const size_t c = s.find("//");
  if (c != std::string::npos) s.resize(c);
  for (char& ch : s) {
    if (ch == '(' || ch == ')' || ch == ',' || ch == '\t' || ch == '\r') ch = ' ';
  }
  std::vector<std::string> w;
  std::istringstream is(s);
  for (std::string t; is >> t;) w.push_back(t);
  return w;
}

bool ToNum(const std::string& s, float* out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const double d = std::strtod(s.c_str(), &end);
  if (*end != '\0') return false;
  *out = static_cast<float>(d);
  return true;
}

}  // namespace

void SprAnimation::Eval(float t, float out[3]) const {
  out[0] = out[1] = out[2] = 0;
  if (keys.empty()) return;
  if (length > 0) t = t - length * std::floor(t / length);
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].t <= t) ++i;
  if (!linear || i + 1 >= keys.size() || t < keys[i].t) {
    for (int k = 0; k < 3; ++k) out[k] = keys[i].v[k];
    return;
  }
  const float span = keys[i + 1].t - keys[i].t;
  const float f = span > 0 ? (t - keys[i].t) / span : 0;
  for (int k = 0; k < 3; ++k) out[k] = keys[i].v[k] + (keys[i + 1].v[k] - keys[i].v[k]) * f;
}

void SprAnimation::FrameCell(float t, int* col, int* row) const {
  *col = *row = 0;
  if (auto_mode.empty() || count < 1 || length <= 0) return;
  t = t - length * std::floor(t / length);
  int f = static_cast<int>(t / length * static_cast<float>(count));
  if (f >= count) f = count - 1;
  if (auto_mode == "row") {
    *col = f;
  } else if (auto_mode == "column") {
    *row = f;
  } else {
    int n = 1;
    while (n * n < count) ++n;
    *col = f % n;
    *row = f / n;
  }
}

void SprLibrary::AddText(const std::string& text) {
  std::istringstream in(text);
  std::string line;
  float ref_w = 0, ref_h = 0;
  std::string material;
  SprAnimation* cur = nullptr;
  bool in_keys = false, want_header = false, anim_ctx = false;
  while (std::getline(in, line)) {
    auto w = Words(line);
    // Trailing "#comment" columns (race files add qualifier/screen columns) are not data.
    for (size_t k = 1; k < w.size(); ++k)
      if (w[k][0] == '#') { w.resize(k); break; }
    if (w.empty() || w[0][0] == '#') {
      if (w.empty() && !in_keys) {}  // Leerzeile beendet Keyframes nicht zwingend
      continue;
    }
    const std::string& h = w[0];
    if (h[0] == '@') {
      in_keys = false;
      if (h.rfind("@reference=", 0) == 0) {
        ref_w = ref_h = static_cast<float>(std::atof(h.c_str() + 11));
      } else if (h.rfind("@referenceWidth=", 0) == 0) {
        ref_w = static_cast<float>(std::atof(h.c_str() + 16));
      } else if (h.rfind("@referenceHeight=", 0) == 0) {
        ref_h = static_cast<float>(std::atof(h.c_str() + 17));
      } else if (h.rfind("@tmaterial=", 0) == 0) {
        material = h.substr(11);
      } else if (h == "@animation" && w.size() >= 2) {
        SprAnimation a;
        a.id = ToLowerAscii(w[1]);
        cur = &(animations_[a.id] = a);
        anim_ctx = false;
        want_header = true;
      } else if (h.rfind("@auto=", 0) == 0) {
        if (cur && anim_ctx) cur->auto_mode = ToLowerAscii(h.substr(6));
      } else if (h == "@keyframes") {
        in_keys = true;
        anim_ctx = false;
      } else if (h == "@sprite_node" && w.size() >= 4) {
        SprNodeDef d;
        d.name = ToLowerAscii(w[1]);
        d.sprite = ToLowerAscii(w[2]);
        d.animation = ToLowerAscii(w[3]);
        size_t i = 4;
        float v;
        if (i + 1 < w.size() && ToNum(w[i], &d.w) && ToNum(w[i + 1], &d.h)) i += 2;
        if (i + 2 < w.size() && ToNum(w[i], &d.colour[0]) && ToNum(w[i + 1], &d.colour[1]) && ToNum(w[i + 2], &d.colour[2])) i += 3;
        for (; i < w.size(); ++i) {
          if (w[i] == "billboard") d.billboard = true;
          else if (w[i] == "sprite_table") d.table = true;
          else if (!ToNum(w[i], &v)) {}  // unbekanntes Flag: ignorieren
        }
        nodes_[d.name] = d;
      }
      // @include, @auto, @skip, @emitter u. a. sind hier nicht von Belang
      continue;
    }
    if (cur && want_header) {
      // "<feld> <anzahl> <laenge> <interpolation>"
      float len = 0;
      if (w.size() >= 4 && ToNum(w[2], &len)) {
        cur->field = ToLowerAscii(w[0]);
        cur->count = std::atoi(w[1].c_str());
        cur->length = len;
        cur->linear = ToLowerAscii(w[3]) == "linear";
        want_header = false;
        anim_ctx = true;
        continue;
      }
      ++skipped_;
      continue;
    }
    if (cur && in_keys) {
      SprKeyframe k;
      bool ok = ToNum(w[0], &k.t) && w.size() >= 2;
      for (size_t i = 1; ok && i < w.size() && i <= 3; ++i) ok = ToNum(w[i], &k.v[i - 1]);
      if (ok) cur->keys.push_back(k);
      else ++skipped_;
      continue;
    }
    // Sprite-Tabellenzeile: Name Datei U V W H
    float u, v, ww, hh;
    if ((w.size() == 6 || (w.size() >= 7 && w[6].rfind("@anim=", 0) == 0)) && ToNum(w[2], &u) && ToNum(w[3], &v) && ToNum(w[4], &ww) && ToNum(w[5], &hh)) {
      SprSprite s;
      s.name = ToLowerAscii(w[0]);
      s.file = w[1];
      s.u = u; s.v = v; s.w = ww; s.h = hh;
      s.ref_w = ref_w; s.ref_h = ref_h;
      s.material = material;
      if (w.size() >= 7) s.anim = ToLowerAscii(w[6].substr(6));
      sprites_[s.name] = s;
      continue;
    }
    ++skipped_;
  }
}

const SprSprite* SprLibrary::FindSprite(const std::string& n) const {
  auto it = sprites_.find(ToLowerAscii(n));
  return it == sprites_.end() ? nullptr : &it->second;
}
const SprAnimation* SprLibrary::FindAnimation(const std::string& n) const {
  auto it = animations_.find(ToLowerAscii(n));
  return it == animations_.end() ? nullptr : &it->second;
}
const SprNodeDef* SprLibrary::FindNode(const std::string& n) const {
  auto it = nodes_.find(ToLowerAscii(n));
  return it == nodes_.end() ? nullptr : &it->second;
}

const SprNodeDef* SprLibrary::ResolveNode(const std::string& name) const {
  std::string n = ToLowerAscii(name);
  for (;;) {
    if (const SprNodeDef* d = FindNode(n)) return d;
    const size_t us = n.find_last_of('_');
    if (us == std::string::npos || us + 1 >= n.size()) return nullptr;
    for (size_t i = us + 1; i < n.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(n[i]))) return nullptr;
    }
    n.resize(us);
  }
}

}  // namespace fleet
