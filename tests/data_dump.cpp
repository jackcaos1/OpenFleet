// Pruefwerkzeug: liest die echten Spieldaten (nur lokal vorhanden) und gibt Kennzahlen aus, die mit
// tools/data_dump_ref.py (Python-Referenz) verglichen werden: tools/compare_dumps.py.
//   fleet_data_dump <spielordner>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <memory>

#include "data/UnitDef.h"
#include "formats/Bzn.h"
#include "formats/Odf.h"
#include "formats/Sod.h"
#include "formats/Spr.h"
#include "formats/Tt.h"
#include "formats/ZfsSource.h"
#include "formats/TextureLookup.h"
#include "formats/Zfs.h"
#include "io/FileSource.h"
#include "model/ModelBuilder.h"

using namespace fleet;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Aufruf: %s <spielordner>\n", argv[0]);
    return 2;
  }
  auto src = OpenDirectory(argv[1]);

  for (const char* zname : {"assets.zfs", "objtxt.zfs"}) {
    std::vector<uint8_t> bytes;
    if (!src->ReadFile(zname, &bytes)) continue;
    ZfsArchive z;
    std::string err;
    if (!z.Open(std::move(bytes), &err)) {
      std::printf("ZFS %s FEHLER %s\n", zname, err.c_str());
      continue;
    }
    unsigned long sum = 0;
    for (const ZfsEntry& e : z.entries()) {
      for (uint8_t b : z.Read(e)) sum += b;
    }
    std::printf("ZFS %s n=%zu key=%08x bytesum=%lu\n", zname, z.entries().size(), z.key(), sum);
  }

  // ODF/TT aus assets.zfs. Kennzahlen: aufgeloeste Eintraege, Summe aller Zahlen, Anzahl Strings, Anzahl Werte, Includes.
  std::shared_ptr<ZfsArchive> odf_zfs = std::make_shared<ZfsArchive>();
  ZfsSource odf_source(odf_zfs);
  OdfLibrary odf_lib([&](const std::string& n, std::vector<uint8_t>* o) { return odf_source.ReadFile(n, o); });
  {
    std::vector<uint8_t> bytes;
    auto z = odf_zfs;
    std::string err;
    if (src->ReadFile("assets.zfs", &bytes) && z->Open(std::move(bytes), &err)) {
      ZfsSource& zs = odf_source;
      OdfLibrary& lib = odf_lib;
      std::vector<std::string> files;
      for (const ZfsEntry& e : z->entries()) files.push_back(ToLowerAscii(e.name));
      std::sort(files.begin(), files.end());
      for (const std::string& f : files) {
        auto ends = [&](const char* x) { const std::string t = x; return f.size() > t.size() && f.compare(f.size() - t.size(), t.size(), t) == 0; };
        if (ends(".odf")) {
          std::string e;
          const OdfObject* o = lib.Get(f, &e);
          if (!o) { std::printf("ODF %s FEHLER\n", f.c_str()); continue; }
          double sum = 0; size_t strs = 0, vals = 0;
          for (const OdfEntry& en : o->entries())
            for (const OdfToken& t : en.values) {
              ++vals;
              if (t.kind == OdfToken::Kind::String) ++strs;
              else if (t.kind != OdfToken::Kind::Ident) sum += t.number;
            }
          std::printf("ODF %s entries=%zu vals=%zu strs=%zu includes=%zu numsum=%.4f\n", f.c_str(), o->entries().size(), vals, strs,
                      o->includes.size(), sum);
        } else if (ends(".tt")) {
          std::vector<uint8_t> b;
          zs.ReadFile(f, &b);
          const auto tt = ParseTt(std::string(b.begin(), b.end()));
          size_t pre = 0;
          for (const TtEntry& t : tt) pre += t.prerequisites.size();
          std::printf("TT %s lines=%zu prereqs=%zu\n", f.c_str(), tt.size(), pre);
        }
      }
    }
  }

  // Waffen-ODFs (mit ordName) und deren Geschosse
  {
    std::vector<std::string> files;
    for (const ZfsEntry& e : odf_zfs->entries()) {
      const std::string f = ToLowerAscii(e.name);
      if (f.size() > 4 && f.compare(f.size() - 4, 4, ".odf") == 0) files.push_back(f);
    }
    std::sort(files.begin(), files.end());
    std::vector<std::string> ords;
    for (const std::string& f : files) {
      const OdfObject* o = odf_lib.Get(f);
      std::string ord;
      if (!o || !o->GetString("ordName", &ord)) continue;
      WeaponDef w;
      LoadWeaponDef(odf_lib, f, &w);
      std::printf("WPN %s ord=%s range=%.2f delay=%.2f hit=%.2f hitov=%zu\n", f.c_str(), ToLowerAscii(ord).c_str(), w.range, w.shot_delay,
                  w.hit_chance.base, w.hit_chance.overrides.size());
      ords.push_back(ToLowerAscii(ord));
    }
    std::sort(ords.begin(), ords.end());
    ords.erase(std::unique(ords.begin(), ords.end()), ords.end());
    for (const std::string& n : ords) {
      OrdnanceDef d;
      if (!LoadOrdnanceDef(odf_lib, n, &d)) { std::printf("ORD %s FEHLT\n", n.c_str()); continue; }
      std::printf("ORD %s class=%s dmg=%.2f var=%.2f thr=%.2f dmgov=%zu life=%.2f speed=%.2f radius=%.2f ign=%d light=%d\n", n.c_str(),
                  d.class_label.c_str(), d.damage.base, d.damage_variance, d.damage_threshold, d.damage.overrides.size(), d.life_span,
                  d.shot_speed, d.radius, d.ignore_shield ? 1 : 0, d.has_light ? 1 : 0);
    }
  }

  // Sprites/*.spr: Bibliothek (Sprites, Animationen, Knoten, Schluesselbilder, Pegel der Animation "strobe")
  SprLibrary spr;
  {
    std::vector<std::string> files = src->ListDir("Sprites");
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
    for (const std::string& n : files) {
      const std::string l = ToLowerAscii(n);
      std::vector<uint8_t> b;
      if (l.size() > 4 && l.compare(l.size() - 4, 4, ".spr") == 0 && src->ReadFile("Sprites/" + n, &b)) spr.AddText(std::string(b.begin(), b.end()));
    }
    size_t keys = 0;
    std::printf("SPR sprites=%zu animations=%zu nodes=%zu\n", spr.sprite_count(), spr.animation_count(), spr.node_count());
    for (const char* id : {"photanim", "tex4x4", "tex4x4.12", "tex1x4", "tex4x1", "tex2x2", "tex3x3"}) {
      const SprAnimation* a = spr.FindAnimation(id);
      if (!a) { std::printf("SPRANIM %s fehlt\n", id); continue; }
      std::printf("SPRANIM %s field=%s count=%d auto=%s len=%.3f", id, a->field.c_str(), a->count, a->auto_mode.c_str(), a->length);
      for (float t : {0.0f, 0.37f, 1.9f, 4.23f}) { int c, r; a->FrameCell(t, &c, &r); std::printf(" %d,%d", c, r); }
      std::printf("\n");
    }
    for (const char* sn : {"wphoton", "kphoton", "fphaser", "pphaser"}) {
      const SprSprite* sp = spr.FindSprite(sn);
      std::printf("SPRITE %s %s\n", sn, sp ? (sp->file + " " + std::to_string(static_cast<int>(sp->u)) + " " + std::to_string(static_cast<int>(sp->v)) + " " + std::to_string(static_cast<int>(sp->w)) + " " + std::to_string(static_cast<int>(sp->h)) + " anim=" + sp->anim).c_str() : "fehlt");
    }
    for (const char* id : {"strobe", "strobe1", "strobe2", "flicker"}) {
      const SprAnimation* a = spr.FindAnimation(id);
      if (!a) { std::printf("ANIM %s fehlt\n", id); continue; }
      keys = a->keys.size();
      float acc = 0;
      for (int i = 0; i < 40; ++i) { float v[3]; a->Eval(i * 0.137f, v); acc += v[0] + 2 * v[1] + 3 * v[2]; }
      std::printf("ANIM %s field=%s keys=%zu len=%.3f linear=%d acc=%.3f\n", id, a->field.c_str(), keys, a->length, a->linear ? 1 : 0, acc);
    }
  }

  std::vector<std::string> names = src->ListDir("SOD");
  std::sort(names.begin(), names.end());
  for (const std::string& n : names) {
    if (ToLowerAscii(n).size() < 5 || ToLowerAscii(n).substr(n.size() - 4) != ".sod") continue;
    std::vector<uint8_t> bytes;
    src->ReadFile("SOD/" + n, &bytes);
    Sod sod;
    std::string err;
    if (!ParseSod(bytes.data(), bytes.size(), &sod, &err)) {
      std::printf("SOD %s FEHLER\n", ToLowerAscii(n).c_str());
      continue;
    }
    const auto world = SodWorldMatrices(sod);
    size_t meshes = 0, verts = 0, tris = 0, tex_ok = 0, tex_missing = 0, tex_empty = 0;
    double sx = 0, sy = 0, sz = 0;
    for (size_t i = 0; i < sod.nodes.size(); ++i) {
      const SodNode& nd = sod.nodes[i];
      if (nd.type != kNodeMesh) continue;
      ++meshes;
      const Mat3x4& w = world[i];
      for (size_t k = 0; k + 2 < nd.mesh.positions.size() + 0 && k + 2 <= nd.mesh.positions.size() - 1; k += 3) {
        const double x = nd.mesh.positions[k], y = nd.mesh.positions[k + 1], z = nd.mesh.positions[k + 2];
        sx += x * w.right[0] + y * w.up[0] + z * w.front[0] + w.pos[0];
        sy += x * w.right[1] + y * w.up[1] + z * w.front[1] + w.pos[1];
        sz += x * w.right[2] + y * w.up[2] + z * w.front[2] + w.pos[2];
      }
      verts += nd.mesh.positions.size() / 3;
      for (const SodGroup& g : nd.mesh.groups) tris += g.corners.size() / 6;
      if (nd.mesh.texture.empty()) {
        ++tex_empty;
      } else {
        Image img;
        if (LoadGameTexture(*src, nd.mesh.texture, &img, nullptr)) ++tex_ok; else ++tex_missing;
      }
    }
    Model model;
    BuildOptions all;
    all.hide_helper_nodes = false;  // Python-Referenz kennt keine Ausblendung
    BuildModel(sod, n, &model, all);
    const size_t model_sprite_nodes = model.sprite_nodes.size();
    {
      // Einheit mit gleichnamiger ODF: Waffen, Hardpoints, fehlende Hardpoints im Modell, Kennzahlen
      UnitDef ud;
      const std::string base = ToLowerAscii(n).substr(0, ToLowerAscii(n).size() - 4);
      if (odf_zfs->Find(base + ".odf") && LoadUnitDef(odf_lib, base, &ud)) {
        size_t hps = 0, missing = 0;
        for (const WeaponSlot& w : ud.weapons) {
          for (const std::string& h : w.hardpoints) {
            ++hps;
            bool found = false;
            for (const SodNode& nd : sod.nodes) if (ToLowerAscii(nd.name) == ToLowerAscii(h)) found = true;
            if (!found) ++missing;
          }
        }
        std::printf("UNIT %s class=%s weapons=%zu hardpoints=%zu hp_missing=%zu health=%.2f crew=%d\n", base.c_str(), ud.class_label.c_str(),
                    ud.weapons.size(), hps, missing, ud.max_health, ud.crew_cost);
      }
    }
    {
      size_t sn = 0, resolved = 0;
      for (const SodNode& nd : sod.nodes) {
        if (nd.type != kNodeSprite) continue;
        ++sn;
        if (spr.ResolveNode(nd.name)) ++resolved;
      }
      std::printf("LIGHTS %s sprite_nodes=%zu resolved=%zu modelnodes=%zu hardpoints=%zu\n", ToLowerAscii(n).c_str(), sn, resolved, model_sprite_nodes, model.hardpoints.size());
    }
    std::printf("SOD %s v=%.1f nodes=%zu meshes=%zu verts=%zu tris=%zu modeltris=%zu tex_ok=%zu tex_missing=%zu tex_empty=%zu sum=%.2f,%.2f,%.2f\n",
                ToLowerAscii(n).c_str(), sod.version, sod.nodes.size(), meshes, verts, tris, model.triangle_count, tex_ok, tex_missing,
                tex_empty, sx, sy, sz);
  }

  {
    std::vector<std::string> bz = src->ListDir("bzn");
    std::sort(bz.begin(), bz.end());
    for (const std::string& n : bz) {
      if (ToLowerAscii(n).size() < 5 || ToLowerAscii(n).substr(n.size() - 4) != ".bzn") continue;
      std::vector<uint8_t> bytes;
      src->ReadFile("bzn/" + n, &bytes);
      BznMap map;
      std::string err;
      if (!ParseBzn(bytes.data(), bytes.size(), &map, &err)) {
        std::printf("BZN %s FEHLER %s\n", ToLowerAscii(n).c_str(), err.c_str());
        continue;
      }
      auto us = [](std::string s) { for (char& c : s) if (c == ' ') c = '_'; return s.empty() ? std::string("-") : s; };
      double px = 0, py = 0, pz = 0, rot = 0, ptsum = 0;
      unsigned long ids = 0, labels = 0;
      long teams[9] = {0};
      for (const BznObject& o : map.objects) {
        px += o.pos[0]; py += o.pos[1]; pz += o.pos[2];
        for (float v : o.rot) rot += v;
        ids += o.handle;
        labels += o.label.size() + o.odf.size();
        if (o.team >= 0 && o.team < 9) ++teams[o.team];
      }
      size_t pts = 0;
      for (const BznPath& p : map.paths) {
        for (size_t i = 0; i < p.points.size(); ++i) ptsum += p.points[i] * static_cast<double>(1 + i % 3);
        ptsum += p.kind + p.speed + p.name.size();
        pts += p.points.size() / 3;
      }
      std::printf("BZN %s v=%d desc=%s file=%s name=%s bg=%s tt=%s race=%s mission=%s title=%s objs=%zu paths=%zu points=%zu bounds=sum=%.3f,%.3f,%.3f ids=%lu text=%lu "
                  "teams=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld pos=sum=%.3f,%.3f,%.3f rot=numsum=%.4f path=acc=%.3f\n",
                  ToLowerAscii(n).c_str(), map.version, us(map.description).c_str(), us(map.file).c_str(), us(map.name).c_str(), us(map.background).c_str(),
                  us(map.tech_tree).c_str(), us(map.race).c_str(), us(map.mission).c_str(), us(map.title).c_str(), map.objects.size(), map.paths.size(), pts,
                  map.bounds[0] + map.bounds[1], map.bounds[2] + map.bounds[3], map.bounds[4] + map.bounds[5], ids, labels, teams[0], teams[1], teams[2],
                  teams[3], teams[4], teams[5], teams[6], teams[7], teams[8], px, py, pz, rot, ptsum);
    }
  }
  return 0;
}
