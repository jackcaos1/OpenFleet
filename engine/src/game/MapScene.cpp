#include "game/MapScene.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "io/FileSource.h"

namespace fleet {

namespace {

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
  uint32_t Next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  float Unit() { return static_cast<float>(Next() >> 8) / 16777216.0f; }  // [0,1)
};

Mat4 Scaled(Mat4 m, float k) {
  for (int i : {0, 1, 2, 4, 5, 6, 8, 9, 10}) m.m[i] *= k;
  return m;
}

}  // namespace

Mat4 BznObjectMatrix(const BznObject& o) {
  const float* r = o.rot;  // rechts = r[0..2], oben = r[3..5], vorne = r[6..8]
  Mat4 m;
  m.m[0] = r[0];  m.m[1] = r[1];  m.m[2] = -r[2];  m.m[3] = 0;
  m.m[4] = r[3];  m.m[5] = r[4];  m.m[6] = -r[5];  m.m[7] = 0;
  m.m[8] = -r[6]; m.m[9] = -r[7]; m.m[10] = r[8];  m.m[11] = 0;
  m.m[12] = o.pos[0]; m.m[13] = o.pos[1]; m.m[14] = -o.pos[2]; m.m[15] = 1;
  return m;
}

MapScene BuildMapScene(const BznMap& map, const MapResolver& resolve) {
  MapScene s;
  std::map<std::string, int> index;
  auto model_index = [&](const std::string& name) {
    auto it = index.find(name);
    if (it == index.end()) {
      it = index.emplace(name, static_cast<int>(s.models.size())).first;
      s.models.push_back(name);
    }
    return it->second;
  };
  for (size_t i = 0; i < map.objects.size(); ++i) {
    const BznObject& o = map.objects[i];
    MapObjectInfo info;
    if (!resolve(o, &info)) {
      s.unmodeled.push_back(static_cast<int>(i));
      continue;
    }
    Mat4 base = BznObjectMatrix(o);
    base.m[13] -= info.below_grid;
    if (info.field_radius > 0 && !info.field_models.empty()) {
      Rng rng(o.handle * 2654435761u + 12345u);
      const int span = std::max(0, info.field_max - info.field_min);
      const int n = info.field_min + (span > 0 ? static_cast<int>(rng.Next() % static_cast<uint32_t>(span + 1)) : 0);
      for (int k = 0; k < n; ++k) {
        float v[3];
        do {
          for (float& c : v) c = 2.0f * rng.Unit() - 1.0f;
        } while (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] > 1.0f);
        MapInstance inst;
        inst.model = model_index(info.field_models[rng.Next() % info.field_models.size()]);
        inst.team = o.team;
        inst.object = static_cast<int>(i);
        inst.rotation = Mat4::RotationY(rng.Unit() * 6.2831853f) * Mat4::RotationX(rng.Unit() * 6.2831853f);
        Mat4 t = Mat4::Translation(base.m[12] + v[0] * info.field_radius, base.m[13] + v[1] * info.field_radius,
                                   base.m[14] + v[2] * info.field_radius);
        inst.transform = t * Scaled(inst.rotation, info.scale);
        s.instances.push_back(inst);
      }
      continue;
    }
    MapInstance inst;
    inst.model = model_index(info.sod);
    inst.team = o.team;
    inst.object = static_cast<int>(i);
    inst.transform = Scaled(base, info.scale);
    inst.rotation = base;
    inst.rotation.m[12] = inst.rotation.m[13] = inst.rotation.m[14] = 0;
    s.instances.push_back(inst);
  }
  // Blickpunkt: Mitte der Objekte mit Team (Schiffe, Stationen), sonst aller Objekte.
  std::vector<const BznObject*> focus;
  for (const BznObject& o : map.objects)
    if (o.team != 0) focus.push_back(&o);
  if (focus.empty())
    for (const BznObject& o : map.objects) focus.push_back(&o);
  if (!focus.empty()) {
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const BznObject* o : focus) {
      const float p[3] = {o->pos[0], o->pos[1], -o->pos[2]};
      for (int k = 0; k < 3; ++k) {
        lo[k] = std::min(lo[k], p[k]);
        hi[k] = std::max(hi[k], p[k]);
      }
    }
    for (int k = 0; k < 3; ++k) s.center[k] = 0.5f * (lo[k] + hi[k]);
    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    s.extent = std::max(500.0f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return s;
}

}  // namespace fleet
