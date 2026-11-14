#include "model/ModelBuilder.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <cstring>
#include <map>
#include <unordered_map>

#include "io/FileSource.h"

namespace fleet {

namespace {

struct V3 {
  float x, y, z;
};

V3 Sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

}  // namespace

namespace {

bool StartsWith(const std::string& s, const char* prefix) { return s.compare(0, std::strlen(prefix), prefix) == 0; }

// "lod" + Ziffern -> Zahl, sonst -1.
int LodLevel(const std::string& lower_name) {
  if (lower_name.size() < 4 || !StartsWith(lower_name, "lod")) return -1;
  int v = 0;
  for (size_t i = 3; i < lower_name.size(); ++i) {
    if (lower_name[i] < '0' || lower_name[i] > '9') return -1;
    v = v * 10 + (lower_name[i] - '0');
  }
  return v;
}

int MaxLod(const Sod& sod) {
  int m = -1;
  for (const SodNode& n : sod.nodes) m = std::max(m, LodLevel(ToLowerAscii(n.name)));
  return m;
}

struct Flags {
  bool oq = false, borg = false, damage = false, ore = false;
  int lod = -1;
};

// Besondere Vorfahren (oder der Knoten selbst) eines Knotens.
Flags AncestorFlags(const Sod& sod, const std::unordered_map<std::string, size_t>& by_name, size_t i) {
  Flags f;
  size_t cur = i;
  for (int depth = 0; depth < 64; ++depth) {
    const std::string lower = ToLowerAscii(sod.nodes[cur].name);
    if (lower == "oq") f.oq = true;
    if (StartsWith(lower, "borg")) f.borg = true;
    if (StartsWith(lower, "damage")) f.damage = true;
    if (lower == "oreglow" && cur != i) f.ore = true;  // Nachfahren (der Mond hat einen gleichnamigen Sprite-Knoten, der sichtbar bleibt)
    const int lod = LodLevel(lower);
    if (lod >= 0) f.lod = lod;
    auto it = by_name.find(sod.nodes[cur].parent);
    if (it == by_name.end() || it->second == cur) break;
    cur = it->second;
  }
  return f;
}

std::unordered_map<std::string, size_t> NameIndex(const Sod& sod) {
  std::unordered_map<std::string, size_t> by_name;
  for (size_t i = 0; i < sod.nodes.size(); ++i) by_name.emplace(sod.nodes[i].name, i);
  return by_name;
}

// Welche Knoten gezeichnet werden (gleiche Reihenfolge wie sod.nodes).
std::vector<char> VisibleNodes(const Sod& sod, const BuildOptions& o) {
  std::vector<char> visible(sod.nodes.size(), 1);
  if (!o.hide_helper_nodes) return visible;
  const auto by_name = NameIndex(sod);
  const int lod = o.lod >= 0 ? o.lod : MaxLod(sod);
  for (size_t i = 0; i < sod.nodes.size(); ++i) {
    const Flags f = AncestorFlags(sod, by_name, i);
    bool show = true;
    if (f.lod >= 0 && f.lod != lod) show = false;
    if (f.oq && !o.show_oq) show = false;
    if (f.ore && !o.show_ore) show = false;
    if (f.borg && !o.show_borg) show = false;
    if (f.damage && !f.borg && !o.show_damage) show = false;
    if (f.damage && f.borg && !o.show_borg) show = false;
    visible[i] = show ? 1 : 0;
  }
  return visible;
}

}  // namespace

void BuildModel(const Sod& sod, const std::string& name, Model* out, BuildOptions options) {
  *out = Model();
  out->name = name;
  const std::vector<Mat3x4> world = SodWorldMatrices(sod);
  const std::vector<char> visible = VisibleNodes(sod, options);
  const auto by_name = NameIndex(sod);

  const float inf = std::numeric_limits<float>::infinity();
  float mn[3] = {inf, inf, inf}, mx[3] = {-inf, -inf, -inf};
  double signed_volume = 0;

  for (size_t ni = 0; ni < sod.nodes.size(); ++ni) {
    const SodNode& nd = sod.nodes[ni];
    if (nd.type != kNodeNull || nd.name.size() < 3) continue;
    const std::string l = [&] { std::string r = nd.name; for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return r; }();
    const bool named = l == "build" || l == "dock";  // Hardpoints der Werften (buildHardpoint) und Bergbaustationen (dockingHardpoint)
    if (!named) {
      if (l[0] != 'h' || l[1] != 'p') continue;
      bool digits = true;
      for (size_t i = 2; i < l.size(); ++i) digits = digits && std::isdigit(static_cast<unsigned char>(l[i]));
      if (!digits) continue;
    }
    ModelHardpoint hp;
    hp.name = l;
    for (int k = 0; k < 3; ++k) {
      hp.pos[k] = world[ni].pos[k];
      hp.dir[k] = world[ni].front[k];
    }
    hp.pos[2] = -hp.pos[2];
    hp.dir[2] = -hp.dir[2];
    out->hardpoints.push_back(hp);
  }

  // Lichter an ausgeblendeten Teilen (z. B. Offiziersquartiere der Basis) mit ausblenden: Die Sprite-Knoten haengen nicht unter
  // dem Teil, sondern unter "lights". Annahme: Liegt ein Licht innerhalb (2 Einheiten Rand) der Huellbox eines ausgeblendeten
  // oq-Netzes, gehoert es zu diesem Teil.
  struct Box {
    float mn[3], mx[3];
  };
  std::vector<Box> hidden_boxes;
  if (options.hide_helper_nodes) {
    for (size_t ni = 0; ni < sod.nodes.size(); ++ni) {
      const SodNode& node = sod.nodes[ni];
      if (node.type != kNodeMesh || visible[ni] || !AncestorFlags(sod, by_name, ni).oq) continue;
      const Mat3x4& w = world[ni];
      Box b = {{inf, inf, inf}, {-inf, -inf, -inf}};
      for (size_t i = 0; i + 2 < node.mesh.positions.size(); i += 3) {
        const float x = node.mesh.positions[i], y = node.mesh.positions[i + 1], z = node.mesh.positions[i + 2];
        float p[3] = {x * w.right[0] + y * w.up[0] + z * w.front[0] + w.pos[0], x * w.right[1] + y * w.up[1] + z * w.front[1] + w.pos[1],
                      -(x * w.right[2] + y * w.up[2] + z * w.front[2] + w.pos[2])};
        for (int k = 0; k < 3; ++k) {
          b.mn[k] = std::min(b.mn[k], p[k]);
          b.mx[k] = std::max(b.mx[k], p[k]);
        }
      }
      if (b.mn[0] <= b.mx[0]) hidden_boxes.push_back(b);
    }
  }
  for (size_t ni = 0; ni < sod.nodes.size(); ++ni) {
    if (sod.nodes[ni].type != kNodeSprite || !visible[ni]) continue;
    ModelSpriteNode sn;
    sn.name = sod.nodes[ni].name;
    for (int k = 0; k < 3; ++k) {
      sn.pos[k] = world[ni].pos[k];
      sn.right[k] = world[ni].right[k];
      sn.up[k] = world[ni].up[k];
    }
    sn.pos[2] = -sn.pos[2];
    sn.right[2] = -sn.right[2];
    sn.up[2] = -sn.up[2];
    bool hide = false;
    for (const Box& b : hidden_boxes) {
      const float m = 2.0f;
      if (sn.pos[0] >= b.mn[0] - m && sn.pos[0] <= b.mx[0] + m && sn.pos[1] >= b.mn[1] - m && sn.pos[1] <= b.mx[1] + m &&
          sn.pos[2] >= b.mn[2] - m && sn.pos[2] <= b.mx[2] + m)
        hide = true;
    }
    if (!hide) out->sprite_nodes.push_back(sn);
  }

  // Erster Durchlauf: Batches mit Weltpositionen und Rohdreiecken.
  std::vector<std::vector<uint32_t>> batch_vpos;   // je Batch und Eckpunkt: Positionsindex im Mesh (Normalen werden je Position ueber alle Gruppen summiert)
  std::vector<size_t> batch_node;
  for (size_t ni = 0; ni < sod.nodes.size(); ++ni) {
    const SodNode& node = sod.nodes[ni];
    if (node.type != kNodeMesh || !visible[ni]) continue;
    const SodMesh& mesh = node.mesh;
    const bool borg_overlay = AncestorFlags(sod, by_name, ni).borg;
    const Mat3x4& w = world[ni];
    const size_t nv = mesh.positions.size() / 3;
    std::vector<V3> wp(nv);
    for (size_t i = 0; i < nv; ++i) {
      const float x = mesh.positions[i * 3], y = mesh.positions[i * 3 + 1], z = mesh.positions[i * 3 + 2];
      V3 p = {x * w.right[0] + y * w.up[0] + z * w.front[0] + w.pos[0], x * w.right[1] + y * w.up[1] + z * w.front[1] + w.pos[1],
              x * w.right[2] + y * w.up[2] + z * w.front[2] + w.pos[2]};
      p.z = -p.z;  // linkshaendig (Direct3D) -> rechtshaendig (GL)
      wp[i] = p;
    }
    for (const SodGroup& g : mesh.groups) {
      ModelBatch b;
      b.node = node.name;
      b.texture = mesh.texture;
      b.material = g.material;
      b.cull_back = mesh.cull != 0;
      // Himmelswuerfel (Material BACKGROUND): Die Windungsrichtung wird fuer das ganze Modell aus dem Volumen bestimmt, bei
      // offenen Flaechen (Wuerfelseiten von innen gesehen) ist das nicht verlaesslich, daher nie ausblenden.
      { const std::string lm = ToLowerAscii(g.material); if (lm.compare(0, 10, "background") == 0) b.cull_back = false; }  // auch BACKGROUND_1 (Sternenkugel in MbgStars)
      b.color[0] = b.color[1] = b.color[2] = 1.0f;
      for (const SodMaterial& m : sod.materials) {
        if (m.name == g.material) {
          b.color[0] = m.diffuse[0];
          b.color[1] = m.diffuse[1];
          b.color[2] = m.diffuse[2];
          for (int i = 0; i < 3; ++i) { b.ambient[i] = m.ambient[i]; b.specular[i] = m.specular[i]; }
          b.power = m.power;
          b.lighting = m.lighting;
          b.unlit = m.lighting == 0;
          break;
        }
      }
      const std::string lm = ToLowerAscii(g.material);
      b.team = StartsWith(lm, "team") && lm != "team_bglow";
      b.cutout = borg_overlay;

      std::map<std::pair<uint16_t, uint16_t>, uint32_t> unique;
      std::vector<uint32_t> vpos;
      const size_t ntri = g.corners.size() / 6;
      for (size_t t = 0; t < ntri; ++t) {
        uint32_t idx[3];
        for (int c = 0; c < 3; ++c) {
          const uint16_t vi = g.corners[t * 6 + c * 2], ui = g.corners[t * 6 + c * 2 + 1];
          auto it = unique.find({vi, ui});
          if (it == unique.end()) {
            const uint32_t id = static_cast<uint32_t>(b.vertices.size() / 8);
            const V3 p = wp[vi];
            const float u = mesh.uvs.empty() ? 0.0f : mesh.uvs[static_cast<size_t>(ui) * 2];
            const float v = mesh.uvs.empty() ? 0.0f : mesh.uvs[static_cast<size_t>(ui) * 2 + 1];
            b.vertices.insert(b.vertices.end(), {p.x, p.y, p.z, 0.0f, 0.0f, 0.0f, u, v});
            vpos.push_back(vi);
            it = unique.emplace(std::make_pair(vi, ui), id).first;
            for (int k = 0; k < 3; ++k) {
              const float val = k == 0 ? p.x : k == 1 ? p.y : p.z;
              mn[k] = std::min(mn[k], val);
              mx[k] = std::max(mx[k], val);
            }
          }
          idx[c] = it->second;
        }
        b.indices.insert(b.indices.end(), {idx[0], idx[1], idx[2]});
        const float* a = &b.vertices[idx[0] * 8];
        const float* bb = &b.vertices[idx[1] * 8];
        const float* cc = &b.vertices[idx[2] * 8];
        signed_volume += Dot(V3{a[0], a[1], a[2]}, Cross(V3{bb[0], bb[1], bb[2]}, V3{cc[0], cc[1], cc[2]})) / 6.0;
      }
      out->triangle_count += ntri;
      if (!b.indices.empty()) {
        out->batches.push_back(std::move(b));
        batch_vpos.push_back(std::move(vpos));
        batch_node.push_back(ni);
      }
    }
  }

  // Windung: Ein ausgewogenes Modell hat positives Volumen, wenn die Vorderseiten gegen den Uhrzeigersinn liegen.
  out->winding_flipped = signed_volume < 0;
  for (ModelBatch& b : out->batches) {
    if (out->winding_flipped) {
      for (size_t i = 0; i + 2 < b.indices.size(); i += 3) std::swap(b.indices[i + 1], b.indices[i + 2]);
    }
  }
  // Normalen wie im Original (Armada.exe 0x58faf0/0x5a2f40/0x55e240): Einheitsnormale je Dreieck, je Positionsindex des Mesh ueber
  // alle Gruppen summiert und normiert (nicht flaechengewichtet, an UV-Nahtstellen geteilt).
  {
    std::map<std::pair<size_t, uint32_t>, V3> acc;
    for (size_t bi = 0; bi < out->batches.size(); ++bi) {
      const ModelBatch& b = out->batches[bi];
      for (size_t i = 0; i + 2 < b.indices.size(); i += 3) {
        const float* p[3] = {&b.vertices[b.indices[i] * 8], &b.vertices[b.indices[i + 1] * 8], &b.vertices[b.indices[i + 2] * 8]};
        V3 n = Cross(Sub({p[1][0], p[1][1], p[1][2]}, {p[0][0], p[0][1], p[0][2]}), Sub({p[2][0], p[2][1], p[2][2]}, {p[0][0], p[0][1], p[0][2]}));
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len <= 1e-12f) continue;
        n = {n.x / len, n.y / len, n.z / len};
        for (int c = 0; c < 3; ++c) {
          V3& a = acc[{batch_node[bi], batch_vpos[bi][b.indices[i + c]]}];
          a.x += n.x; a.y += n.y; a.z += n.z;
        }
      }
    }
    for (size_t bi = 0; bi < out->batches.size(); ++bi) {
      ModelBatch& b = out->batches[bi];
      for (size_t v = 0; v * 8 < b.vertices.size(); ++v) {
        const V3& a = acc[{batch_node[bi], batch_vpos[bi][v]}];
        float* n = &b.vertices[v * 8 + 3];
        const float len = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        if (len > 1e-12f) { n[0] = a.x / len; n[1] = a.y / len; n[2] = a.z / len; }
        else { n[0] = 0; n[1] = 1; n[2] = 0; }
      }
    }
  }
  if (out->batches.empty()) {
    std::fill(mn, mn + 3, 0.0f);
    std::fill(mx, mx + 3, 0.0f);
  }
  std::copy(mn, mn + 3, out->bounds_min);
  std::copy(mx, mx + 3, out->bounds_max);
}

}  // namespace fleet

namespace fleet {

std::vector<ModelVariant> ListModelVariants(const Sod& sod) {
  std::vector<ModelVariant> out;
  const auto by_name = NameIndex(sod);
  std::vector<int> lods;
  bool has_oq = false, has_borg = false, has_damage = false;
  for (size_t i = 0; i < sod.nodes.size(); ++i) {
    const int l = LodLevel(ToLowerAscii(sod.nodes[i].name));
    if (l >= 0 && std::find(lods.begin(), lods.end(), l) == lods.end()) lods.push_back(l);
    if (sod.nodes[i].type != kNodeMesh) continue;
    const Flags f = AncestorFlags(sod, by_name, i);
    has_oq = has_oq || f.oq;
    has_borg = has_borg || f.borg;
    has_damage = has_damage || (f.damage && !f.borg);
  }
  std::sort(lods.begin(), lods.end(), [](int a, int b) { return a > b; });

  BuildOptions base;
  if (lods.empty()) {
    out.push_back({"Standard", base});
  } else {
    for (size_t k = 0; k < lods.size(); ++k) {
      BuildOptions o = base;
      o.lod = lods[k];
      out.push_back({"Detailstufe " + std::to_string(lods[k]) + (k == 0 ? " (hoechste)" : ""), o});
    }
  }
  if (has_oq) {
    BuildOptions o = base;
    o.show_oq = true;
    out.push_back({"hoechste Stufe + Offiziersquartiere", o});
  }
  if (has_borg) {
    BuildOptions o = base;
    o.show_borg = true;
    out.push_back({"Borg-Variante", o});
  }
  if (has_damage) {
    BuildOptions o = base;
    o.show_damage = true;
    out.push_back({"Schadenszustand", o});
  }
  BuildOptions all;
  all.hide_helper_nodes = false;
  out.push_back({"alles (ungefiltert)", all});
  return out;
}

}  // namespace fleet
