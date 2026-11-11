#include "formats/Sod.h"

#include <cmath>
#include <unordered_map>

#include "io/ByteReader.h"

namespace fleet {

namespace {

bool Fail(std::string* err, const std::string& msg, size_t pos) {
  if (err) *err = msg + " bei Offset " + std::to_string(pos);
  return false;
}

void ReadFloats(ByteReader& r, size_t n, std::vector<float>* out) {
  out->resize(n);
  for (size_t i = 0; i < n; ++i) (*out)[i] = r.F32();
}

bool AllFinite(const std::vector<float>& v) {
  for (float f : v) {
    if (!std::isfinite(f)) return false;
  }
  return true;
}

bool ReadMesh(ByteReader& r, float version, SodMesh* m, std::string* err) {
  m->material = version >= 1.7f - 1e-3f ? r.String() : "default";
  m->texture = r.String();
  const uint16_t nv = r.U16(), nuv = r.U16(), ng = r.U16();
  if (!r.ok()) return Fail(err, "Mesh-Kopf zu kurz", r.pos());
  ReadFloats(r, static_cast<size_t>(nv) * 3, &m->positions);
  ReadFloats(r, static_cast<size_t>(nuv) * 2, &m->uvs);
  for (float& f : m->uvs) f = f > 1.0f ? 1.0f : (f < 0.0f ? 0.0f : f);   // Original (0x590c31): UVs beim Laden auf 0..1 geklemmt
  if (!r.ok() || !AllFinite(m->positions)) return Fail(err, "Eckpunkte ungueltig", r.pos());
  m->groups.resize(ng);
  for (SodGroup& g : m->groups) {
    const uint16_t nf = r.U16();
    g.material = r.String();
    g.corners.resize(static_cast<size_t>(nf) * 6);
    for (size_t i = 0; i < g.corners.size(); i += 2) {
      g.corners[i] = r.U16();
      g.corners[i + 1] = r.U16();
      if (r.ok() && (g.corners[i] >= nv || g.corners[i + 1] >= nuv)) return Fail(err, "Index ausserhalb", r.pos());
    }
    if (!r.ok()) return Fail(err, "Gruppe ungueltig", r.pos());
  }
  m->cull = r.U8();
  const uint16_t extra = r.U16();
  r.Skip(static_cast<size_t>(extra) * 2);
  if (!r.ok()) return Fail(err, "Meshende ungueltig", r.pos());
  return true;
}

bool ReadMeshV12(ByteReader& r, SodMesh* m, std::string* err) {
  m->texture = r.String();
  m->material = r.String();
  const uint16_t nv = r.U16(), npoly = r.U16(), nuv = r.U16();
  if (!r.ok()) return Fail(err, "Mesh-Kopf (v1.2) zu kurz", r.pos());
  ReadFloats(r, static_cast<size_t>(nv) * 3, &m->positions);
  ReadFloats(r, static_cast<size_t>(nuv) * 2, &m->uvs);
  for (float& f : m->uvs) f = f > 1.0f ? 1.0f : (f < 0.0f ? 0.0f : f);   // Original (0x590c31): UVs beim Laden auf 0..1 geklemmt
  if (!r.ok() || !AllFinite(m->positions)) return Fail(err, "Eckpunkte (v1.2) ungueltig", r.pos());
  SodGroup g;
  g.material = m->material;
  for (uint16_t p = 0; p < npoly; ++p) {
    const uint16_t n = r.U16();
    std::vector<uint16_t> pr(static_cast<size_t>(n) * 2);
    for (uint16_t& v : pr) v = r.U16();
    if (!r.ok() || n < 3) return Fail(err, "Polygon ungueltig", r.pos());
    for (size_t i = 0; i < pr.size(); i += 2) {
      if (pr[i] >= nv || pr[i + 1] >= nuv) return Fail(err, "Polygonindex ausserhalb", r.pos());
    }
    for (uint16_t i = 1; i + 1 < n; ++i) {  // Dreiecksfaecher
      for (int k : {0, static_cast<int>(i), static_cast<int>(i) + 1}) {
        g.corners.push_back(pr[static_cast<size_t>(k) * 2]);
        g.corners.push_back(pr[static_cast<size_t>(k) * 2 + 1]);
      }
    }
  }
  m->groups.push_back(std::move(g));
  m->cull = 1;
  return true;
}

}  // namespace

bool ParseSod(const uint8_t* data, size_t size, Sod* out, std::string* err) {
  *out = Sod();
  ByteReader r(data, size);
  const uint8_t* magic = r.Bytes(10);
  if (!magic) return Fail(err, "Datei zu kurz", 0);
  const std::string m(reinterpret_cast<const char*>(magic), 10);
  if (m != "Storm3D_SW" && m != "StarTrekDB") return Fail(err, "unbekannte Kennung", 0);
  const float raw_version = r.F32();
  const int v100 = static_cast<int>(std::lround(raw_version * 100.0f));
  if (v100 != 120 && v100 != 140 && v100 != 150 && v100 != 160 && v100 != 170 && v100 != 180)
    return Fail(err, "Version nicht unterstuetzt: " + std::to_string(v100 / 100.0), 10);
  const float ver = v100 / 100.0f;
  out->version = ver;

  if (v100 <= 160) {  // Legacy-Liste, Bedeutung unbekannt
    const uint16_t n = r.U16();
    for (uint16_t i = 0; i < n && r.ok(); ++i) {
      r.String();
      r.String();
      r.Skip(7);
    }
  }
  const uint16_t nmat = r.U16();
  for (uint16_t i = 0; i < nmat && r.ok(); ++i) {
    SodMaterial mat;
    mat.name = r.String();
    for (float& f : mat.ambient) f = r.F32();
    for (float& f : mat.diffuse) f = r.F32();
    for (float& f : mat.specular) f = r.F32();
    mat.power = r.F32();
    mat.lighting = r.U8();
    out->materials.push_back(std::move(mat));
  }
  if (!r.ok()) return Fail(err, "Materialien ungueltig", r.pos());

  const uint16_t nnode = r.U16();
  for (uint16_t i = 0; i < nnode && r.ok(); ++i) {
    SodNode n;
    n.type = r.U16();
    n.name = r.String();
    n.parent = r.String();
    for (float& f : n.right) f = r.F32();
    for (float& f : n.up) f = r.F32();
    for (float& f : n.front) f = r.F32();
    for (float& f : n.pos) f = r.F32();
    if (!r.ok()) return Fail(err, "Knoten ungueltig", r.pos());
    if (n.type == kNodeEmitter) {
      n.emitter = r.String();
    } else if (n.type == kNodeMesh) {
      if (v100 < 140 ? !ReadMeshV12(r, &n.mesh, err) : !ReadMesh(r, ver, &n.mesh, err)) return false;
    } else if (n.type != kNodeNull && n.type != kNodeSprite && n.type != kNodeLod) {
      return Fail(err, "unbekannter Knotentyp " + std::to_string(n.type), r.pos());
    }
    if (!r.ok()) return Fail(err, "Knotenende ungueltig", r.pos());
    out->nodes.push_back(std::move(n));
  }

  if (v100 >= 140) {  // Animationskanaele (gelesen, noch nicht ausgewertet)
    const uint16_t nch = r.U16();
    out->channel_count = nch;
    for (uint16_t i = 0; i < nch && r.ok(); ++i) {
      r.String();
      const uint16_t nk = r.U16();
      r.F32();
      const uint16_t kind = r.U16();
      r.Skip(static_cast<size_t>(nk) * (kind == 5 ? 4 : 48));
    }
  }
  if (v100 >= 160) {  // Texturreferenzen
    const uint16_t nref = r.U16();
    out->texref_count = nref;
    for (uint16_t i = 0; i < nref && r.ok(); ++i) {
      r.U8();
      r.String();
      r.String();
      if (v100 >= 180) r.F32();
    }
  }
  if (!r.ok()) return Fail(err, "Animationsteil ungueltig", r.pos());
  if (!r.at_end()) return Fail(err, std::to_string(r.remaining()) + " Byte uebrig", r.pos());
  return true;
}

std::vector<Mat3x4> SodWorldMatrices(const Sod& sod) {
  std::unordered_map<std::string, size_t> by_name;
  for (size_t i = 0; i < sod.nodes.size(); ++i) by_name.emplace(sod.nodes[i].name, i);  // erster Treffer gewinnt
  std::vector<Mat3x4> world(sod.nodes.size());
  std::vector<char> done(sod.nodes.size(), 0);

  auto local = [&](size_t i) {
    const SodNode& n = sod.nodes[i];
    Mat3x4 m;
    for (int k = 0; k < 3; ++k) {
      m.right[k] = n.right[k];
      m.up[k] = n.up[k];
      m.front[k] = n.front[k];
      m.pos[k] = n.pos[k];
    }
    return m;
  };
  // Lokal dann Eltern (Zeilenvektor).
  auto mul = [](const Mat3x4& a, const Mat3x4& b) {
    Mat3x4 r;
    const float* ar[3] = {a.right, a.up, a.front};
    float* rr[3] = {r.right, r.up, r.front};
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) rr[i][j] = ar[i][0] * b.right[j] + ar[i][1] * b.up[j] + ar[i][2] * b.front[j];
    }
    for (int j = 0; j < 3; ++j) r.pos[j] = a.pos[0] * b.right[j] + a.pos[1] * b.up[j] + a.pos[2] * b.front[j] + b.pos[j];
    return r;
  };

  for (size_t i = 0; i < sod.nodes.size(); ++i) {
    // Kette zur Wurzel sammeln (Tiefe begrenzt wie im Python-Parser), dann von oben nach unten multiplizieren.
    std::vector<size_t> chain;
    size_t cur = i;
    while (chain.size() <= 64) {
      chain.push_back(cur);
      if (done[cur]) break;
      auto it = by_name.find(sod.nodes[cur].parent);
      if (it == by_name.end() || it->second == cur) break;
      cur = it->second;
    }
    // Wurzelende der Kette: entweder fertig berechnet oder lokale Matrix.
    size_t k = chain.size();
    Mat3x4 acc{};
    bool have = false;
    while (k-- > 0) {
      const size_t idx = chain[k];
      if (done[idx]) {
        acc = world[idx];
        have = true;
        continue;
      }
      acc = have ? mul(local(idx), acc) : local(idx);
      have = true;
      world[idx] = acc;
      done[idx] = 1;
    }
  }
  return world;
}

}  // namespace fleet
