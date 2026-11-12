#include "game/Shockwave.h"

#include <algorithm>
#include <cmath>

#include "formats/Odf.h"

namespace fleet {

bool LoadShockwaveDef(OdfLibrary& lib, const std::string& fireball_odf, ShockwaveDef* out) {
  std::string err, name;
  const OdfObject* f = lib.Get(fireball_odf + ".odf", &err);
  if (!f || !f->GetString("shockwave", &name) || name.empty()) return false;
  const OdfObject* o = lib.Get(name + ".odf", &err);
  if (!o) return false;
  ShockwaveDef d;
  o->GetInt("rays", &d.rays);
  o->GetFloat("rate", &d.rate);
  o->GetFloat("radiusFactor", &d.radius_factor);
  o->GetFloat("fadeFactor", &d.fade_factor);
  o->GetString("texture", &d.texture);
  if (const OdfEntry* c = o->Find("color")) {
    for (size_t k = 0; k < 3 && k < c->values.size(); ++k) d.colour[k] = static_cast<float>(c->values[k].number);
  }
  ReadPerTarget(*o, "damageBase", &d.damage_base);
  ReadPerTarget(*o, "damageVariance", &d.damage_variance);
  ReadPerTarget(*o, "damageThreshold", &d.damage_threshold);
  ReadPerTarget(*o, "shieldCrewModifier", &d.shield_crew_modifier);
  ReadPerTarget(*o, "hullCrewModifier", &d.hull_crew_modifier);
  f->GetFloat("shockwaveDelay", &d.delay);
  f->GetFloat("shockwaveMinimum", &d.minimum);
  d.rays = std::max(3, d.rays);
  *out = d;
  return true;
}

float ShockwaveOuterRadius(const ShockwaveDef& d, const Shockwave& s) { return 2.0f * d.rate * s.age; }
float ShockwaveMaxRadius(const ShockwaveDef& d, const Shockwave& s) { return s.scale * d.radius_factor; }
bool ShockwaveDone(const ShockwaveDef& d, const Shockwave& s) { return ShockwaveOuterRadius(d, s) > ShockwaveMaxRadius(d, s); }

float ShockwaveFade(const ShockwaveDef& d, const Shockwave& s) {
  const float r = ShockwaveOuterRadius(d, s), mx = ShockwaveMaxRadius(d, s);
  if (r <= d.fade_factor * mx) return 1.0f;
  const float span = (1.0f - d.fade_factor) * mx;
  return span <= 0 ? 0.0f : std::max(0.0f, (mx - r) / span);
}

bool ShockwaveHit(const ShockwaveDef& d, const Shockwave& s, float distance, const std::string& target_odf, float rand01, float dt, HitInfo* out) {
  const float r = ShockwaveOuterRadius(d, s);
  if (r <= 0) return false;
  const float falloff = 1.0f - distance / r;
  if (falloff < 0) return false;
  const float base = d.damage_base.For(target_odf);
  if (base <= 0) return false;
  HitInfo h;
  h.damage = (base + rand01 * d.damage_variance.For(target_odf)) * falloff * dt;
  h.threshold = d.damage_threshold.For(target_odf);
  h.shield_crew_mod = d.shield_crew_modifier.For(target_odf);
  h.hull_crew_mod = d.hull_crew_modifier.For(target_odf);
  if (h.damage <= 0) return false;
  *out = h;
  return true;
}

void BuildShockwaveMesh(const ShockwaveDef& d, const Shockwave& s, std::vector<float>* out) {
  // Eckpunkte je Strahl (Original: Tabellen bei 0x67fab4/0x67facc): Radiusfaktor 1,2,1,2,1,2 und Hoehe 0,0,2,-2,-2,2.
  static const float kRadius[6] = {1, 2, 1, 2, 1, 2};
  static const float kHeight[6] = {0, 0, 2, -2, -2, 2};
  // Dreiecke je Strahlenpaar: (a, b) = (T1[k], T2[k]) mit T1 = 1,3,5 und T2 = 0,2,4.
  static const int kT1[3] = {1, 3, 5}, kT2[3] = {0, 2, 4};
  const int n = d.rays;
  const float t = s.age;
  auto vertex = [&](int ray, int j, float* v) {
    const float a = 2.0f * 3.14159265f * static_cast<float>(ray) / static_cast<float>(n);
    const float r = d.rate * t * kRadius[j];
    v[0] = std::sin(a) * r;
    v[1] = kHeight[j];
    v[2] = std::cos(a) * r;
    v[3] = static_cast<float>(ray & 1);
    v[4] = static_cast<float>(j & 1);
  };
  out->clear();
  out->reserve(static_cast<size_t>(n) * 3 * 6 * 5);
  for (int i = 0; i < n; ++i) {
    const int nx = (i + 1) % n;
    for (int k = 0; k < 3; ++k) {
      float v[4][5];
      vertex(i, kT1[k], v[0]);   // base + T1
      vertex(i, kT2[k], v[1]);   // base + T2
      vertex(nx, kT2[k], v[2]);  // next + T2
      vertex(nx, kT1[k], v[3]);  // next + T1
      for (int idx : {0, 1, 2, 0, 2, 3}) out->insert(out->end(), v[idx], v[idx] + 5);
    }
  }
}

}  // namespace fleet
