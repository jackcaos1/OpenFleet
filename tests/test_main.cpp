// Minimaler Testlauf ohne Fremdbibliothek. Spaeter ggf. Catch2/GoogleTest (Entscheidung offen).
#include <unordered_map>
#include <cmath>
#include <cstdio>
#include <string>

#include <cstring>
#include <vector>

#include "core/Log.h"
#include "core/Math.h"
#include "formats/Sod.h"
#include "formats/Tga.h"
#include "data/UnitDef.h"
#include "core/TextLayout.h"
#include "audio/Mixer.h"
#include "formats/Bzn.h"
#include "formats/SaveGame.h"
#include "formats/SaveSchema.h"
#include "formats/Wav.h"
#include "game/MapScene.h"
#include "game/Ui.h"
#include "game/Hud.h"
#include "game/Boarding.h"
#include "game/Combat.h"
#include "game/Shockwave.h"
#include "game/Teams.h"
#include "game/ShipSim.h"
#include "game/HoverSim.h"
#include "game/HoverShip.h"
#include "game/MenuLayout.h"
#include "game/PathGrid.h"
#include "io/ZipSource.h"
#include "io/Inflate.h"
#include "io/OverlaySource.h"
#include "formats/GuiCfg.h"
#include <map>
#include "formats/Odf.h"
#include "game/FireDemo.h"
#include "formats/Spr.h"
#include "formats/Tt.h"
#include "formats/Zfs.h"
#include "io/OverlaySource.h"
#include "io/ByteReader.h"
#include "model/ModelBuilder.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_failures;                                                              \
      std::printf("FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
    }                                                                            \
  } while (0)

bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
bool Near(fleet::Vec2 a, float x, float y) { return Near(a.x, x) && Near(a.y, y); }

std::string g_last_log;
void CaptureSink(fleet::LogLevel, const char* message) { g_last_log = message; }

void TestMat4() {
  using fleet::Mat4;
  using fleet::Vec2;
  CHECK(Near(Mat4::Identity().TransformPoint({3, -2}), 3, -2));

  const Mat4 ortho = Mat4::Ortho(-2, 2, -1, 1, -1, 1);
  CHECK(Near(ortho.TransformPoint({2, 1}), 1, 1));
  CHECK(Near(ortho.TransformPoint({-2, -1}), -1, -1));
  CHECK(Near(ortho.TransformPoint({0, 0}), 0, 0));

  const float half_pi = 1.57079632679f;
  CHECK(Near(Mat4::RotationZ(half_pi).TransformPoint({1, 0}), 0, 1));

  // Reihenfolge: (T * R) dreht zuerst, verschiebt dann.
  const Mat4 tr = Mat4::Translation(1, 0, 0) * Mat4::RotationZ(half_pi);
  CHECK(Near(tr.TransformPoint({1, 0}), 1, 1));
  const Mat4 rt = Mat4::RotationZ(half_pi) * Mat4::Translation(1, 0, 0);
  CHECK(Near(rt.TransformPoint({1, 0}), 0, 2));

  // Identitaet ist neutral.
  const Mat4 same = Mat4::Identity() * tr;
  for (int i = 0; i < 16; ++i) CHECK(Near(same.m[i], tr.m[i]));
}

void TestLog() {
  fleet::SetLogSink(CaptureSink);
  FLEET_LOGI("wert=%d text=%s", 42, "ok");
  CHECK(g_last_log == "wert=42 text=ok");
  fleet::SetLogSink(nullptr);
}


// ---- Hilfen zum Bauen von Testdaten -------------------------------------------------------------------------------
struct W {
  std::vector<uint8_t> b;
  W& U8(unsigned v) { b.push_back(static_cast<uint8_t>(v)); return *this; }
  W& U16(unsigned v) { U8(v & 255); U8(v >> 8); return *this; }
  W& U32(uint32_t v) { U16(v & 0xffff); U16(v >> 16); return *this; }
  W& F32(float f) { uint32_t u; std::memcpy(&u, &f, 4); return U32(u); }
  W& Str(const std::string& s) { U16(static_cast<unsigned>(s.size())); for (char c : s) U8(static_cast<unsigned char>(c)); return *this; }
  W& Raw(const std::string& s) { for (char c : s) U8(static_cast<unsigned char>(c)); return *this; }
};

void TestByteReader() {
  const uint8_t d[] = {1, 2, 3, 4, 5, 0, 'a', 'b', 0x80};
  fleet::ByteReader r(d, sizeof d);
  CHECK(r.U8() == 1);
  CHECK(r.U16() == 0x0302);
  CHECK(r.U16() == 0x0504);
  CHECK(r.String().empty());  // Stringlaenge 0x6100 ragt ueber das Ende
  CHECK(!r.ok());           // zu lang -> Fehler, kein Absturz
  CHECK(r.U32() == 0);      // nach Fehler nur Nullwerte
  fleet::ByteReader r2(d, sizeof d);
  r2.Skip(5);
  CHECK(r2.U16() == 0x6100);
  CHECK(r2.ok());
}

void TestTga() {
  // 2x2, 24 Bit, unten-links-Ursprung: Zeilen unten zuerst. Pixel BGR.
  W w;
  w.U8(0).U8(0).U8(2).U16(0).U16(0).U8(0).U16(0).U16(0).U16(2).U16(2).U8(24).U8(0);
  w.U8(0).U8(0).U8(255).U8(0).U8(255).U8(0);    // untere Zeile: rot, gruen
  w.U8(255).U8(0).U8(0).U8(255).U8(255).U8(255);  // obere Zeile: blau, weiss
  fleet::Image img;
  std::string err;
  CHECK(fleet::DecodeTga(w.b.data(), w.b.size(), &img, &err));
  CHECK(img.width == 2 && img.height == 2 && !img.has_alpha);
  CHECK(img.rgba[0] == 0 && img.rgba[1] == 0 && img.rgba[2] == 255);      // oben links = blau
  CHECK(img.rgba[(2 * 1 + 0) * 4 + 0] == 255 && img.rgba[(2 * 1 + 0) * 4 + 1] == 0);  // unten links = rot
  CHECK(img.rgba[3] == 255);

  // 2x1 palettiert, oben-links-Ursprung (Bit 5), 3 Eintraege.
  W p;
  p.U8(0).U8(1).U8(1).U16(0).U16(3).U8(24).U16(0).U16(0).U16(2).U16(1).U8(8).U8(0x20);
  p.U8(0).U8(0).U8(0).U8(10).U8(20).U8(30).U8(40).U8(50).U8(60);  // Palette BGR
  p.U8(2).U8(1);
  CHECK(fleet::DecodeTga(p.b.data(), p.b.size(), &img, &err));
  CHECK(img.rgba[0] == 60 && img.rgba[1] == 50 && img.rgba[2] == 40);
  CHECK(img.rgba[4] == 30 && img.rgba[5] == 20 && img.rgba[6] == 10);

  // Abgeschnitten und Muell duerfen nicht abstuerzen.
  CHECK(!fleet::DecodeTga(w.b.data(), w.b.size() - 3, &img, &err));
  CHECK(!fleet::DecodeTga(w.b.data(), 5, &img, &err));
}

void TestZfs() {
  // Ein Block, zwei Dateien. Schluessel 0x04030201, Dateiinhalt "ABCDEFG" (7 Byte: 4 verschluesselt, 3 Klartext).
  const uint32_t key = 0x04030201;
  const std::string plain = "ABCDEFG";
  std::string enc = plain;
  const uint8_t k[4] = {1, 2, 3, 4};
  for (size_t i = 0; i < 4; ++i) enc[i] = static_cast<char>(enc[i] ^ k[i]);
  W w;
  w.Raw("ZFSF").U32(1).U32(16).U32(100).U32(2).U32(key).U32(28).U32(0);
  const uint32_t data0 = 32 + 2 * 36;
  auto entry = [&](const std::string& name, uint32_t off, uint32_t idx, uint32_t size) {
    std::string n = name;
    n.resize(16, '\0');
    w.Raw(n).U32(off).U32(idx).U32(size).U32(0).U32(0);
  };
  entry("Test.odf", data0, 0, 7);
  entry("leer.txt", data0 + 7, 1, 0);
  w.Raw(enc);
  fleet::ZfsArchive z;
  std::string err;
  CHECK(z.Open(w.b, &err));
  CHECK(z.entries().size() == 2);
  const fleet::ZfsEntry* e = z.Find("TEST.ODF");
  CHECK(e != nullptr);
  if (e) {
    const std::vector<uint8_t> out = z.Read(*e);
    CHECK(std::string(out.begin(), out.end()) == plain);
  }
  CHECK(z.Find("fehlt") == nullptr);
  // Eintrag ausserhalb der Datei wird abgelehnt.
  std::vector<uint8_t> bad = w.b;
  bad.resize(bad.size() - 5);
  CHECK(!z.Open(bad, &err));
}

// Minimales SOD v1.4: ein Material, ein Mesh-Knoten (ein Dreieck, Tiefe z=1 im Modell, Knoten um (0,0,2) verschoben), ein Null-Knoten als Elternteil.
std::vector<uint8_t> BuildSod14() {
  W w;
  w.Raw("StarTrekDB").F32(1.4f);
  w.U16(0);  // Legacy-Liste
  w.U16(1).Str("base").F32(0.1f).F32(0.1f).F32(0.1f).F32(0.5f).F32(0.6f).F32(0.7f).F32(1).F32(1).F32(1).F32(8).U8(1);
  w.U16(2);
  w.U16(0).Str("root").Str("");
  for (float f : {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 2.f}) w.F32(f);
  w.U16(1).Str("hull").Str("root");
  for (float f : {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f}) w.F32(f);
  w.Str("tex1").U16(3).U16(3).U16(1);
  for (float f : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}) w.F32(f);
  for (float f : {0.f, 0.f, 1.f, 0.f, 0.f, 1.f}) w.F32(f);
  w.U16(1).Str("base");
  w.U16(0).U16(0).U16(1).U16(1).U16(2).U16(2);
  w.U8(1).U16(0);
  w.U16(0);  // keine Kanaele
  return w.b;
}

void TestSod() {
  std::vector<uint8_t> d = BuildSod14();
  fleet::Sod sod;
  std::string err;
  CHECK(fleet::ParseSod(d.data(), d.size(), &sod, &err));
  CHECK(sod.nodes.size() == 2);
  CHECK(sod.nodes[1].mesh.texture == "tex1");
  CHECK(sod.nodes[1].mesh.groups.size() == 1 && sod.nodes[1].mesh.groups[0].corners.size() == 6);
  const auto world = fleet::SodWorldMatrices(sod);
  CHECK(Near(world[1].pos[0], 1.0f) && Near(world[1].pos[2], 2.0f));  // Kind + Eltern

  fleet::Model m;
  fleet::BuildModel(sod, "t", &m);
  CHECK(m.batches.size() == 1 && m.triangle_count == 1);
  CHECK(m.batches[0].vertices.size() == 3 * 8);
  CHECK(Near(m.batches[0].vertices[2], -2.0f));  // z negiert
  CHECK(m.batches[0].cull_back);

  // Hilfsknoten: Elternteil "damage" blendet das Mesh aus, mit Option aus bleibt es sichtbar.
  fleet::Sod hidden = sod;
  hidden.nodes[0].name = "damage";
  hidden.nodes[1].parent = "damage";
  fleet::Model hm;
  fleet::BuildModel(hidden, "h", &hm);
  CHECK(hm.batches.empty());
  fleet::BuildOptions keep;
  keep.hide_helper_nodes = false;
  fleet::BuildModel(hidden, "h", &hm, keep);
  CHECK(hm.batches.size() == 1);

  // Detailstufen: nur die hoechste bleibt.
  fleet::Sod lods = sod;
  lods.nodes[0].name = "lod1";
  lods.nodes[1].parent = "lod1";
  fleet::SodNode other = sod.nodes[1];
  other.name = "hull2";
  other.parent = "lod2";
  fleet::SodNode lod2 = sod.nodes[0];
  lod2.name = "lod2";
  lods.nodes.push_back(lod2);
  lods.nodes.push_back(other);
  fleet::Model lm;
  fleet::BuildModel(lods, "l", &lm);
  CHECK(lm.batches.size() == 1 && lm.batches[0].node == "hull2");

  // Ansichten: lod1+lod2 -> zwei Detailstufen plus "alles"; Offiziersquartiere nur bei vorhandenem "oq".
  const std::vector<fleet::ModelVariant> vars = fleet::ListModelVariants(lods);
  CHECK(vars.size() == 3 && vars[0].options.lod == 2 && vars[1].options.lod == 1 && !vars[2].options.hide_helper_nodes);
  fleet::Sod with_oq = sod;
  with_oq.nodes[0].name = "oq";
  with_oq.nodes[1].parent = "oq";
  const std::vector<fleet::ModelVariant> vars2 = fleet::ListModelVariants(with_oq);
  CHECK(vars2.size() == 3 && vars2[1].options.show_oq);  // Standard, +Quartiere, alles
  fleet::BuildModel(with_oq, "q", &hm, vars2[0].options);
  CHECK(hm.batches.empty());
  fleet::BuildModel(with_oq, "q", &hm, vars2[1].options);
  CHECK(hm.batches.size() == 1);

  // Borg-Ueberzug: Alpha schneidet aus (cutout), normale Teile nicht; Borg-Glueh-Material ist keine Teamfarbe.
  fleet::Sod borg = sod;
  borg.nodes[0].name = "borg";
  borg.nodes[1].parent = "borg";
  fleet::BuildOptions show_borg;
  show_borg.show_borg = true;
  fleet::BuildModel(borg, "b", &hm, show_borg);
  CHECK(hm.batches.size() == 1 && hm.batches[0].cutout);
  fleet::BuildModel(sod, "n", &hm);
  CHECK(hm.batches.size() == 1 && !hm.batches[0].cutout && !hm.batches[0].team);

  // Zusatzbyte am Ende -> Fehler; abgeschnitten -> Fehler, kein Absturz.
  std::vector<uint8_t> longer = d;
  longer.push_back(0);
  CHECK(!fleet::ParseSod(longer.data(), longer.size(), &sod, &err));
  for (size_t cut = 0; cut < d.size(); cut += 7) CHECK(!fleet::ParseSod(d.data(), cut, &sod, &err));
}

void TestOdf() {
  fleet::OdfObject o;
  fleet::ParseOdfText("[ShipClass]\r\n// Kommentar\r\nclassLabel = \"cruiser\"\r\nhull = 1200\r\nmass=2.5f; // x\r\n"
                      "big = 1e3\r\nflag = true\r\nnames = \"a\" \"b\"\r\n\"c\"\r\nMuell ohne gleich\r\n", "T.ODF", &o);
  CHECK(o.section == "ShipClass" && o.name == "t.odf");
  std::string s; int i = 0; float f = 0; bool b = false;
  CHECK(o.GetString("CLASSLABEL", &s) && s == "cruiser");
  CHECK(o.GetInt("hull", &i) && i == 1200);
  CHECK(o.GetFloat("mass", &f) && f == 2.5f);
  CHECK(o.GetFloat("big", &f) && f == 1000.0f);
  CHECK(o.GetBool("flag", &b) && b);
  CHECK(!o.GetInt("classLabel", &i) && !o.Has("nope"));
  CHECK(o.GetStrings("names").size() == 3 && o.GetStrings("names")[2] == "c");
  CHECK(o.warnings.size() == 1);

  // Vererbung: Basis zuerst, eigene Werte ueberschreiben, Schleifen werden erkannt.
  std::unordered_map<std::string, std::string> files = {
      {"base.odf", "hull = 100\nspeed = 5\n"},
      {"mid.odf", "#include \"base.odf\"\nspeed = 7\n"},
      {"leaf.odf", "#include \"MID.odf\"\nhull = 300\n"},
      {"loop1.odf", "#include \"loop2.odf\"\n"},
      {"loop2.odf", "#include \"loop1.odf\"\n"}};
  fleet::OdfLibrary lib([&](const std::string& n, std::vector<uint8_t>* out) {
    auto it = files.find(n);
    if (it == files.end()) return false;
    out->assign(it->second.begin(), it->second.end());
    return true;
  });
  std::string err;
  const fleet::OdfObject* leaf = lib.Get("Leaf", &err);
  CHECK(leaf && leaf->GetInt("hull", &i) && i == 300);
  CHECK(leaf->GetInt("speed", &i) && i == 7);
  CHECK(lib.Get("loop1", &err) == nullptr && !err.empty());
  CHECK(lib.Get("fehlt", &err) == nullptr);

  CHECK(fleet::Cp1252ToUtf8("a\xE4\x80") == "a\xC3\xA4\xE2\x82\xAC");

  std::vector<std::string> w;
  const auto tt = fleet::ParseTt("A.ODF 2 b.odf C.odf\nd.odf 0\nkaputt 3 x\n", &w);
  CHECK(tt.size() == 2 && tt[0].odf == "a.odf" && tt[0].prerequisites.size() == 2 && tt[0].prerequisites[1] == "c.odf");
  CHECK(w.size() == 1);

  // Overlay: erste Quelle gewinnt.
  struct Mem : fleet::FileSource {
    std::string v;
    bool ReadFile(const std::string&, std::vector<uint8_t>* o) override { if (v.empty()) return false; o->assign(v.begin(), v.end()); return true; }
    bool Exists(const std::string&) override { return !v.empty(); }
    std::vector<std::string> ListDir(const std::string&) override { return {}; }
  };
  auto m1 = std::make_shared<Mem>(), m2 = std::make_shared<Mem>();
  m2->v = "zwei";
  fleet::OverlaySource ov;
  ov.Add(m1); ov.Add(m2);
  std::vector<uint8_t> got;
  CHECK(ov.ReadFile("x", &got) && got.size() == 4);
  m1->v = "eins";
  CHECK(ov.ReadFile("x", &got) && got.size() == 4 && got[0] == 'e');
}

void TestSpr() {
  fleet::SprLibrary lib;
  lib.AddText("@reference=64\n@tmaterial=additive\nBallflare xlite1 0 0 32 32\nSmallStar xlite1 32 0 32 32\n"
              "@sprite_node redstrobe SmallStar strobe (.5,.5) (1,.8,0) billboard\n"
              "@sprite_node bad Ballflare const (1,1)) (0,1,0) //billboard\n"
              "@animation strobe\ndraw 4 2.0 step\n@keyframes\n0.0 1\n0.1 0\n0.2 1\n0.3 0\n"
              "@animation fade\ncolour 2 1.0 linear\n@keyframes\n0.0 (0,0,0)\n1.0 (1,0.5,0)\n");
  CHECK(lib.sprite_count() == 2 && lib.animation_count() == 2 && lib.node_count() == 2);
  {
    fleet::SprLibrary l2;
    l2.AddText("@reference=128\nwphoton wphoton 0 0 32 32 @anim=photanim\n@animation photanim\noffset 4 0.5 step\n@auto=column\n@keyframes\n"
               "@animation sq\noffset 12 3.0 step\n@auto=square\n@keyframes\n@auto=row\n");
    CHECK(l2.FindSprite("wphoton") && l2.FindSprite("wphoton")->anim == "photanim");
    int c, r;
    const fleet::SprAnimation* pa = l2.FindAnimation("photanim");
    CHECK(pa && pa->count == 4 && pa->auto_mode == "column");
    pa->FrameCell(0.0f, &c, &r); CHECK(c == 0 && r == 0);
    pa->FrameCell(0.3f, &c, &r); CHECK(c == 0 && r == 2);
    pa->FrameCell(0.55f, &c, &r); CHECK(c == 0 && r == 0);  // Schleife
    const fleet::SprAnimation* sq = l2.FindAnimation("sq");
    CHECK(sq && sq->auto_mode == "square");  // spaeteres @auto ausserhalb des Animationskopfes wird ignoriert
    sq->FrameCell(2.9f, &c, &r); CHECK(c == 3 && r == 2);  // Bild 11 von 12 im 4x4-Raster
  }
  const fleet::SprSprite* sp = lib.FindSprite("smallstar");
  CHECK(sp && sp->file == "xlite1" && sp->u == 32 && sp->ref_w == 64 && sp->material == "additive");
  const fleet::SprNodeDef* n = lib.ResolveNode("RedStrobe_3_12");
  CHECK(n && n->billboard && n->w == 0.5f && n->colour[1] == 0.8f && n->animation == "strobe");
  CHECK(lib.ResolveNode("redstrobe_x") == nullptr && lib.ResolveNode("unbekannt_1") == nullptr);
  const fleet::SprNodeDef* b = lib.FindNode("bad");
  CHECK(b && !b->billboard && b->colour[1] == 1.0f);
  const fleet::SprAnimation* a = lib.FindAnimation("strobe");
  float v[3];
  CHECK(a && a->keys.size() == 4 && a->length == 2.0f);
  a->Eval(0.05f, v); CHECK(v[0] == 1);
  a->Eval(0.15f, v); CHECK(v[0] == 0);
  a->Eval(0.25f, v); CHECK(v[0] == 1);
  a->Eval(1.5f, v);  CHECK(v[0] == 0);
  a->Eval(2.05f, v); CHECK(v[0] == 1);  // Schleife
  lib.FindAnimation("fade")->Eval(0.5f, v);
  CHECK(std::fabs(v[0] - 0.5f) < 1e-5f && std::fabs(v[1] - 0.25f) < 1e-5f);
}

void TestUnitDef() {
  std::unordered_map<std::string, std::string> files = {
      {"craft.odf", "classLabel = \"wingman\"\nshieldDelay = 3.0f\nenginesHitPoints = 10\nphysicsFile = \"phys.odf\"\n"},
      {"ship.odf", "#include \"craft.odf\"\nunitName = \"Test Cl\x92ss\"\nrace = \"federation\"\nmaxHealth = 300\ncrewCost = 200\n"
                   "weapon1 = \"Wphas\"\nweaponHardpoints1 = \"hp01\" \"hp02\"\nweapon3 = \"wtorp\"\nweaponHardpoints3 = \"hp05\"\n"
                   "enginesTargetHardpoints = \"hp20\"\n"},
      {"phys.odf", "impulseSpeed = 20\nwarpSpeed = 80\n"},
      {"wphas.odf", "wpnName = \"Phaser\"\nrange = 300.0f\nshotDelay = 2.0f\nclassLabel = \"cannon\"\n"},
      {"wtorp.odf", "special = 1\n"}};
  fleet::OdfLibrary lib([&](const std::string& n, std::vector<uint8_t>* out) {
    auto it = files.find(n);
    if (it == files.end()) return false;
    out->assign(it->second.begin(), it->second.end());
    return true;
  });
  fleet::UnitDef u;
  CHECK(fleet::LoadUnitDef(lib, "SHIP", &u));
  CHECK(u.odf == "ship" && u.class_label == "wingman" && u.race == "federation" && u.max_health == 300 && u.crew_cost == 200);
  CHECK(u.unit_name == "Test Cl\xE2\x80\x99ss");  // CP1252 0x92 = Apostroph
  CHECK(u.shield_delay == 3.0f && u.engines.hit_points == 10 && u.engines.target_hardpoints.size() == 1);
  CHECK(u.weapons.size() == 2 && u.weapons[0].odf == "wphas" && u.weapons[0].hardpoints.size() == 2 && u.weapons[1].odf == "wtorp");
  fleet::WeaponDef w;
  CHECK(fleet::LoadWeaponDef(lib, "wphas", &w) && w.name == "Phaser" && w.range == 300 && w.shot_delay == 2);
  CHECK(fleet::LoadWeaponDef(lib, "wtorp", &w) && w.special);
  files["wphas.odf"] += "hitChance = 0.7 \"bbattle.odf\" 1.0 \"fente.odf\" 0.5\nordName = \"wphaso\"\n";
  files["wphaso.odf"] = "classLabel = \"phaser\"\ndamageBase = 24 \"zmama.odf\" 2\ndamageVariance = 2\nlifeSpan = 1.6\nshotSpeed = 1e6\nRadius = 4.0\n"
                        "ignoreShield = 1\nlightColor = 0.9 0.8 0.3\nSprite = \"fphaser\"\n";
  fleet::OdfLibrary lib2([&](const std::string& n, std::vector<uint8_t>* out) {  // frisch, da lib die alten Inhalte zwischengespeichert hat
    auto it = files.find(n);
    if (it == files.end()) return false;
    out->assign(it->second.begin(), it->second.end());
    return true;
  });
  CHECK(fleet::LoadWeaponDef(lib2, "wphas", &w) && w.hit_chance.base == 0.7f && w.hit_chance.overrides.size() == 2);
  CHECK(w.hit_chance.For("BBattle") == 1.0f && w.hit_chance.For("fente.odf") == 0.5f && w.hit_chance.For("kbattle") == 0.7f);
  fleet::OrdnanceDef od;
  CHECK(fleet::LoadOrdnanceDef(lib2, w.ordnance, &od) && od.class_label == "phaser" && od.damage.base == 24 && od.damage.For("zmama") == 2);
  CHECK(od.damage_variance == 2 && od.life_span == 1.6f && od.shot_speed == 1e6f && od.radius == 4.0f && od.ignore_shield && od.has_light);
  CHECK(od.light_color[1] == 0.8f && od.sprite == "fphaser");
  fleet::PhysicsDef ph;
  CHECK(fleet::LoadPhysicsDef(lib, u.physics_file, &ph) && ph.warp_speed == 80);
  CHECK(!fleet::LoadUnitDef(lib, "gibtsnicht", &u));
  const std::string text = fleet::DescribeUnit(lib, [&] { fleet::UnitDef d; fleet::LoadUnitDef(lib, "ship", &d); return d; }());
  CHECK(text.find("Phaser x2") != std::string::npos && text.find("Warp 80") != std::string::npos);
}

void TestTextLayout() {
  auto l = fleet::LayoutText("Ab\nC", 0);
  CHECK(l.size() == 2 && l[0].size() == 2 && l[0][0] == 'A' - 32 && l[0][1] == 'b' - 32 && l[1][0] == 'C' - 32);
  l = fleet::LayoutText("abcdef", 4);
  CHECK(l.size() == 2 && l[0].size() == 4 && l[1].size() == 2);
  l = fleet::LayoutText("\xC3\xA4\xE2\x80\x99\xE4", 0);  // ä, ’ (UTF-8), dann ein einzelnes Latin-1-ä (ungueltige UTF-8-Folge)
  CHECK(l[0].size() == 3 && l[0][0] == 95 && l[0][1] == 95 + 8 && l[0][2] != 'z' - 32);
  l = fleet::LayoutText("\xE2\x82", 0);  // abgeschnittene Folge darf nicht ueber das Ende lesen
  CHECK(l[0].size() == 2);
  l = fleet::LayoutText("\xF0\x9F\x98\x80", 0);  // unbekanntes Zeichen -> ?
  CHECK(l[0].size() == 1 && l[0][0] == '?' - 32);
  CHECK(fleet::LayoutText("", 0).size() == 1 && fleet::LayoutText("", 0)[0].empty());
}

void TestFireDemo() {
  std::unordered_map<std::string, std::string> files = {
      {"u.odf", "unitName = \"U\"\nweapon1 = \"wb\"\nweaponHardpoints1 = \"hp01\" \"hp02\"\nweapon2 = \"wp\"\nweaponHardpoints2 = \"hp03\"\n"
                "weapon3 = \"wsp\"\nweaponHardpoints3 = \"hp03\"\nweapon4 = \"wph\"\nweaponHardpoints4 = \"hp09\"\n"},
      {"wb.odf", "wpnName = \"Beam\"\nshotDelay = 2.0\nrange = 300\nordName = \"ob\"\n"},
      {"ob.odf", "shotSpeed = 1e6\nlifeSpan = 1.6\nRadius = 4\nSprite = \"sb\"\n"},
      {"wp.odf", "wpnName = \"Pulse\"\nshotDelay = 1.0\nordName = \"op\"\n"},
      {"op.odf", "shotSpeed = 300\nlifeSpan = 4\nRadius = 8\nLength = 50\nSprite = \"sp\"\n"},
      {"wsp.odf", "special = 1\nshotDelay = 1.0\nordName = \"ob\"\n"},
      {"wph.odf", "shotDelay = 1.0\nordName = \"ob\"\n"}};
  fleet::OdfLibrary lib([&](const std::string& n, std::vector<uint8_t>* out) {
    auto it = files.find(n);
    if (it == files.end()) return false;
    out->assign(it->second.begin(), it->second.end());
    return true;
  });
  fleet::SprLibrary spr;
  spr.AddText("@reference=128\nsb wt 0 0 128 21\nsp wp 0 0 128 32\n");
  fleet::Model m;
  for (int i = 1; i <= 3; ++i) {
    fleet::ModelHardpoint h;
    h.name = "hp0" + std::to_string(i);
    h.pos[0] = static_cast<float>(i);
    h.dir[2] = -1;
    m.hardpoints.push_back(h);
  }
  fleet::UnitDef u;
  CHECK(fleet::LoadUnitDef(lib, "u", &u));
  fleet::FireDemo fd;
  fd.Setup(m, u, lib, spr, 20.0f);
  CHECK(fd.active_slots() == 2 && fd.skipped_slots() == 2);  // Sonderwaffe und Waffe ohne Hardpoint im Modell uebersprungen
  fd.Update(0.1f);
  CHECK(fd.shots().empty());  // feuert erst nach SetFiring
  fd.SetFiring(true);
  fd.Update(0.01f);
  CHECK(fd.shots().size() == 3);  // 2 Strahlen + 1 Puls
  CHECK(fd.shots()[0].kind == fleet::Shot::Kind::Beam && fd.shots()[0].length == 160.0f);  // Reichweite 300 auf 8*Radius begrenzt
  CHECK(fd.shots()[2].kind == fleet::Shot::Kind::Bolt && fd.shots()[2].length == 50.0f);
  const float z0 = fd.shots()[2].pos[2];
  fd.Update(0.5f);
  CHECK(fd.shots()[2].pos[2] < z0 - 30.0f);  // 300 * 0.25 * 0.5 = 37.5 in Richtung -z
  CHECK(fd.shots()[0].pos[0] == 1.0f);       // Strahl bleibt am Hardpoint
  for (int i = 0; i < 40; ++i) fd.Update(0.25f);
  CHECK(fd.shots().size() < 40);  // alte Schuesse verschwinden
  CHECK(fd.Describe().find("Beam x2") != std::string::npos);
}

// BZN: synthetische Datei im Originalaufbau (Textkopf + typisierte Elemente), ohne Spieldaten.
void BznElem(std::vector<uint8_t>& b, uint8_t type, const void* d, uint32_t n) {
  const uint8_t head[4] = {type, 0xcd, 0xcd, 0xcd};
  b.insert(b.end(), head, head + 4);
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(n >> (8 * i)));
  b.insert(b.end(), static_cast<const uint8_t*>(d), static_cast<const uint8_t*>(d) + n);
}
void BznStr(std::vector<uint8_t>& b, uint8_t type, const std::string& s, uint32_t size) {
  std::vector<uint8_t> v(size, 0);
  for (size_t i = 0; i < s.size() && i < size; ++i) v[i] = static_cast<uint8_t>(s[i]);
  BznElem(b, type, v.data(), size);
}
void BznF(std::vector<uint8_t>& b, float f) { BznElem(b, 5, &f, 4); }
void BznI(std::vector<uint8_t>& b, int32_t i) { BznElem(b, 4, &i, 4); }
void BznV(std::vector<uint8_t>& b, std::vector<float> v) { BznElem(b, 9, v.data(), static_cast<uint32_t>(v.size() * 4)); }

void TestBzn() {
  const std::string text = "version [1] =\r\n2051\r\nsaveGameDesc = 54657374\r\nbinarySave [1] =\r\ntrue\r\n";
  std::vector<uint8_t> b(text.begin(), text.end());
  BznStr(b, 2, "untitled.bzn", 16);
  const uint8_t one = 1;
  BznElem(b, 1, &one, 1);
  BznStr(b, 2, "untitled.bzn", 100);
  for (float f : {0.0f, 6400.0f, -28.5f, 105.5f, 0.0f, 6400.0f}) BznF(b, f);
  const uint8_t zero1 = 0;
  BznElem(b, 1, &zero1, 1);
  for (int i = 0; i < 3; ++i) BznF(b, 0.0f);
  BznStr(b, 2, "mbgdom1.sod", 100);
  BznStr(b, 2, "tech1.tt", 128);
  BznStr(b, 2, "borg", 128);
  const size_t count_at = b.size();
  BznI(b, 1);  // Objektzahl
  BznStr(b, 7, "bbase", 8);
  BznI(b, static_cast<int32_t>(0x80008307u));
  BznV(b, {1, 2, 3});
  BznI(b, 4);
  BznStr(b, 2, "bbase13", 40);
  const uint32_t ptr = 0x1234;
  BznElem(b, 8, &ptr, 4);
  const float mat[12] = {0, 0, -1, 0, 1, 0, 1, 0, 0, 1, 2, 3};
  BznElem(b, 11, mat, 48);
  BznF(b, 7.0f);  // klassenspezifische Zusatzdaten werden uebersprungen
  BznStr(b, 2, "EmptyMission", 40);
  BznElem(b, 8, &ptr, 4);
  BznStr(b, 2, "", 64);
  BznStr(b, 2, "", 64);
  BznStr(b, 0, "Mein Titel", 64);
  BznI(b, 0);
  BznI(b, 1);  // ein Pfad
  const uint32_t zero = 0;
  BznElem(b, 0, &ptr, 4);
  BznI(b, 4);
  BznStr(b, 2, "belt", 4);
  BznI(b, 2);
  BznV(b, {1, 2, 3, 4, 5, 6});
  BznElem(b, 0, &zero, 4);
  BznF(b, 0.5f);

  fleet::BznMap m;
  std::string err;
  CHECK(fleet::ParseBzn(b.data(), b.size(), &m, &err));
  if (m.objects.size() != 1 || m.paths.size() != 1) return;
  CHECK(m.version == 2051 && m.description == "Test");
  CHECK(m.background == "mbgdom1.sod" && m.tech_tree == "tech1.tt" && m.race == "borg");
  CHECK(m.bounds[1] == 6400.0f && m.bounds[2] == -28.5f);
  CHECK(m.objects.size() == 1);
  CHECK(m.objects[0].odf == "bbase" && m.objects[0].label == "bbase13" && m.objects[0].team == 4);
  CHECK(m.objects[0].handle == 0x80008307u && m.objects[0].pos[2] == 3.0f && m.objects[0].rot[2] == -1.0f);
  CHECK(m.title == "Mein Titel" && m.mission == "EmptyMission");
  CHECK(m.paths.size() == 1 && m.paths[0].name == "belt" && m.paths[0].points.size() == 6 && m.paths[0].speed == 0.5f);

  // kaputte Eingaben duerfen nicht abstuerzen und liefern Fehler
  CHECK(!fleet::ParseBzn(b.data(), b.size() - 3, &m, &err));
  std::vector<uint8_t> c = b;
  c.push_back(5);
  CHECK(!fleet::ParseBzn(c.data(), c.size(), &m, &err));
  const std::string junk = "keine bzn";
  CHECK(!fleet::ParseBzn(reinterpret_cast<const uint8_t*>(junk.data()), junk.size(), &m, &err));
  std::vector<uint8_t> d = b;
  d[count_at + 8] = 2;  // Objektzahl im Kopf falsch
  CHECK(!fleet::ParseBzn(d.data(), d.size(), &m, &err));
}

void TestMapScene() {
  using namespace fleet;
  BznObject o;
  o.odf = "bbase";
  o.pos[0] = 10; o.pos[1] = 20; o.pos[2] = 30;
  const Mat4 m = BznObjectMatrix(o);  // Einheitsdrehung: nur Verschiebung, z gespiegelt
  CHECK(Near(m.m[12], 10) && Near(m.m[13], 20) && Near(m.m[14], -30) && Near(m.m[10], 1));
  // Modellpunkt vorne (GL z = +1 entspricht Spiel-z = -1) landet bei Spiel-z 29, also GL-z -29
  CHECK(Near(m.m[10] * 1.0f + m.m[14], -29));
  // 90 Grad um y: rechts = (0,0,-1)? vorne = (1,0,0): Modell-Vorne zeigt im Spiel nach +x, im GL-Modell ist vorne -z
  BznObject q;
  q.pos[0] = 0; q.pos[1] = 0; q.pos[2] = 0;
  const float rot[9] = {0, 0, -1, 0, 1, 0, 1, 0, 0};
  for (int i = 0; i < 9; ++i) q.rot[i] = rot[i];
  const Mat4 mq = BznObjectMatrix(q);
  // lokaler Punkt (0,0,-1) (GL, "vorne") -> Welt (GL)
  const float wx = mq.m[8] * -1.0f, wy = mq.m[9] * -1.0f, wz = mq.m[10] * -1.0f;
  CHECK(Near(wx, 1) && Near(wy, 0) && Near(wz, 0));

  BznMap map;
  for (int i = 0; i < 3; ++i) {
    BznObject x;
    x.odf = i == 2 ? "mafield" : "BBase";
    x.team = i == 2 ? 0 : 1;
    x.pos[0] = 100.0f * static_cast<float>(i);
    map.objects.push_back(x);
  }
  const MapScene sc = BuildMapScene(map, [](const BznObject& o, MapObjectInfo* info) {
    if (o.odf == "BBase") {
      info->sod = "bbase";
      info->scale = 2.0f;
      info->below_grid = 5.0f;
      return true;
    }
    if (o.odf == "mafield") {
      info->field_min = 10;
      info->field_max = 15;
      info->field_radius = 100;
      info->field_models = {"master1", "master2"};
      info->scale = 1.5f;
      return true;
    }
    return false;
  });
  CHECK(sc.models.size() == 3 && sc.models[0] == "bbase");
  CHECK(sc.unmodeled.empty());
  size_t asteroids = 0;
  for (const MapInstance& mi : sc.instances) if (mi.object == 2) ++asteroids;
  CHECK(asteroids >= 10 && asteroids <= 15 && sc.instances.size() == 2 + asteroids);
  CHECK(Near(sc.instances[0].transform.m[0], 2.0f) && Near(sc.instances[0].transform.m[13], -5.0f) && Near(sc.instances[0].rotation.m[0], 1.0f));
  for (const MapInstance& mi : sc.instances) {  // Asteroiden liegen innerhalb der Feldkugel um (200,0,0)
    if (mi.object != 2) continue;
    const float dx = mi.transform.m[12] - 200.0f, dy = mi.transform.m[13], dz = mi.transform.m[14];
    CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) <= 100.5f);
  }
  const MapScene sc2 = BuildMapScene(map, [](const BznObject& o, MapObjectInfo* info) {
    if (o.odf == "mafield") { info->field_min = 10; info->field_max = 15; info->field_radius = 100; info->field_models = {"master1", "master2"}; return true; }
    info->sod = "bbase";
    return true;
  });
  CHECK(sc2.instances.size() == 2 + asteroids);  // gleiches Handle -> gleiche Anzahl
  CHECK(Near(sc.center[0], 50) && sc.extent >= 500);
}

std::vector<uint8_t> MakeWav(int rate, int ch, int bits, const std::vector<int>& values) {
  std::vector<uint8_t> b;
  auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); };
  auto u16 = [&](uint16_t v) { b.push_back(static_cast<uint8_t>(v)); b.push_back(static_cast<uint8_t>(v >> 8)); };
  const uint32_t data = static_cast<uint32_t>(values.size() * static_cast<size_t>(bits / 8));
  b.insert(b.end(), {'R', 'I', 'F', 'F'});
  u32(36 + data);
  b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  u32(16); u16(1); u16(static_cast<uint16_t>(ch)); u32(static_cast<uint32_t>(rate)); u32(static_cast<uint32_t>(rate * ch * bits / 8));
  u16(static_cast<uint16_t>(ch * bits / 8)); u16(static_cast<uint16_t>(bits));
  b.insert(b.end(), {'d', 'a', 't', 'a'});
  u32(data);
  for (int v : values) {
    if (bits == 8) b.push_back(static_cast<uint8_t>(v));
    else u16(static_cast<uint16_t>(v));
  }
  return b;
}

void TestAudio() {
  using namespace fleet;
  PcmClip c;
  const auto w8 = MakeWav(22050, 1, 8, {128, 255, 0});
  CHECK(ParseWav(w8.data(), w8.size(), &c));
  CHECK(c.sample_rate == 22050 && c.channels == 1 && c.frames() == 3 && c.samples[0] == 0 && c.samples[1] == 127 * 256 && c.samples[2] == -128 * 256);
  const auto w16 = MakeWav(44100, 2, 16, {1000, -1000, 2000, -2000});
  CHECK(ParseWav(w16.data(), w16.size(), &c) && c.channels == 2 && c.frames() == 2 && c.samples[3] == -2000);
  CHECK(!ParseWav(w16.data(), 20, &c));  // abgeschnitten
  std::vector<uint8_t> bad = w16;
  bad[20] = 2;  // Format-Tag 2 = ADPCM
  CHECK(!ParseWav(bad.data(), bad.size(), &c));
  const std::string junk = "nope nope nope nope";
  CHECK(!ParseWav(reinterpret_cast<const uint8_t*>(junk.data()), junk.size(), &c));

  auto clip = std::make_shared<PcmClip>();
  clip->sample_rate = 44100;
  clip->channels = 1;
  clip->samples.assign(100, 16384);  // 0,5
  Mixer mx(44100);
  const auto id = mx.Play(clip, 1.0f, -1.0f, false);  // ganz links
  CHECK(id != Mixer::kNone && mx.IsPlaying(id));
  int16_t out[400];
  mx.Mix(out, 100);
  CHECK(out[0] > 22000 && out[0] < 24000 && std::abs(out[1]) < 100);  // links laut, rechts still
  CHECK(!mx.IsPlaying(id));  // zu Ende gespielt und entfernt
  const auto lid = mx.Play(clip, 0.5f, 0.0f, true);
  mx.Mix(out, 200);  // laenger als der Clip: Schleife
  CHECK(mx.IsPlaying(lid) && out[398] != 0 && out[398] == out[399]);
  mx.Stop(lid);
  CHECK(mx.active_voices() == 0);
  // Resampling: halbe Abtastrate dehnt den Clip auf doppelte Laenge
  auto slow = std::make_shared<PcmClip>();
  slow->sample_rate = 22050;
  slow->channels = 1;
  slow->samples.assign(50, 8000);
  mx.Play(slow, 1.0f, 0.0f, false);
  mx.Mix(out, 100);
  CHECK(mx.active_voices() == 0);
  // Zwei Stimmen addieren sich, hartes Begrenzen
  mx.Play(clip, 1.0f, 0.0f, false);
  mx.Play(clip, 1.0f, 0.0f, false);
  mx.Mix(out, 10);
  CHECK(out[0] > 20000);
}

void TestUi() {
  using namespace fleet;
  for (const auto& dim : {std::pair<int, int>{1080, 2400}, {2400, 1080}, {800, 480}, {480, 800}}) {
    for (int map = 0; map < 2; ++map) {
      UiState st;
      st.map_mode = map == 1;
      const UiLayout L = BuildUiLayout(dim.first, dim.second, st);
      CHECK(!L.buttons.empty() && L.bar_height > 0 && L.bar_y + L.bar_height <= static_cast<float>(dim.second) + 0.5f);
      CHECK(L.bar_height < 0.4f * static_cast<float>(dim.second));  // Bild behaelt den groessten Teil
      bool inside = true, overlap = false;
      for (size_t i = 0; i < L.buttons.size(); ++i) {
        const UiRect& r = L.buttons[i].rect;
        inside = inside && r.x >= 0 && r.y >= L.bar_y && r.x + r.w <= static_cast<float>(dim.first) + 0.5f && r.y + r.h <= static_cast<float>(dim.second) + 0.5f;
        for (size_t j = i + 1; j < L.buttons.size(); ++j) {
          const UiRect& q = L.buttons[j].rect;
          if (r.x < q.x + q.w && q.x < r.x + r.w && r.y < q.y + q.h && q.y < r.y + r.h) overlap = true;
        }
        // Beschriftung passt in die Taste
        CHECK(static_cast<float>(L.buttons[i].label.size() * 11 * static_cast<size_t>(L.text_scale)) <= r.w);
        // Treffer in der Tastenmitte liefert die Aktion
        CHECK(UiHit(L, r.x + r.w / 2, r.y + r.h / 2) == L.buttons[i].action);
      }
      CHECK(inside && !overlap);
      CHECK(UiHit(L, 10, L.bar_y - 5) == UiAction::None);  // ueber der Leiste
    }
  }
  UiState on;
  on.firing = true;
  on.muted = true;
  const UiLayout L = BuildUiLayout(1080, 2400, on);
  int active = 0;
  for (const UiButton& b : L.buttons) if (b.active) ++active;
  CHECK(active == 2);
  CHECK(BuildUiLayout(1080, 2400, UiState()).buttons.size() == 11 && BuildUiLayout(1080, 2400, UiState()).buttons.size() > BuildUiLayout(1080, 2400, [] { UiState m; m.map_mode = true; return m; }()).buttons.size());
  CHECK(UiMessageScale(1080, 2400) >= 1 && 1080 / (11 * UiMessageScale(1080, 2400)) >= 40);
  CHECK(UiMessageScale(100, 100) == 1);
}

static const unsigned char kZipA[] = {80,75,3,4,20,0,0,0,8,0,0,0,33,0,60,227,107,235,72,1,0,0,218,17,0,0,9,0,0,0,83,79,68,47,97,46,115,111,100,141,215,189,77,67,49,24,134,209,85,60,1,242,191,63,151,236,144,9,64,65,4,20,2,132,8,198,167,164,67,167,127,171,227,43,95,63,135,231,99,250,184,157,30,95,211,195,231,229,251,45,61,93,126,210,125,122,185,157,223,175,41,167,175,211,249,120,189,75,135,127,70,69,70,85,70,77,70,93,70,67,70,83,70,75,70,33,163,77,152,70,78,230,133,208,11,169,23,98,47,228,94,8,190,144,124,33,250,66,246,149,236,171,125,239,100,95,201,190,146,125,37,251,74,246,149,236,43,217,87,178,111,100,223,200,190,217,101,67,246,141,236,27,217,55,178,111,100,223,200,190,145,125,39,251,78,246,157,236,187,221,244,100,223,201,190,147,125,39,251,78,246,157,236,7,217,15,178,31,100,63,200,126,216,111,150,236,7,217,15,178,31,100,63,200,126,146,253,36,251,73,246,147,236,39,217,79,123,227,144,253,36,251,73,246,147,236,23,217,47,178,95,100,191,200,126,145,253,34,251,101,15,76,178,95,100,191,200,62,200,62,200,62,200,62,200,62,200,62,200,62,200,62,236,117,79,246,65,246,155,236,55,217,111,178,223,100,191,201,126,147,253,38,251,77,246,219,210,10,219,202,226,42,91,93,101,203,171,108,125,149,45,176,178,21,86,182,196,202,214,88,217,34,43,219,41,104,226,218,41,96,228,98,229,98,230,98,231,98,232,98,233,98,234,254,181,238,47,80,75,3,4,20,0,0,0,0,0,0,0,33,0,160,145,71,97,11,0,0,0,11,0,0,0,18,0,0,0,84,101,120,116,117,114,101,115,47,82,71,66,47,66,46,116,103,97,115,116,111,114,101,100,45,100,97,116,97,80,75,3,4,20,0,0,0,8,0,0,0,33,0,203,34,2,103,62,1,0,0,0,20,0,0,11,0,0,0,83,79,68,47,122,122,122,46,115,111,100,99,96,100,98,102,97,101,99,231,224,228,226,230,225,229,227,23,16,20,18,22,17,21,19,151,144,148,146,150,145,149,147,87,80,84,82,86,81,85,83,215,208,212,210,214,209,213,211,55,48,52,50,54,49,53,51,183,176,180,178,182,177,181,179,119,112,116,114,118,113,117,115,247,240,244,242,246,241,245,243,15,8,12,10,14,9,13,11,143,136,140,138,142,137,141,139,79,72,76,74,78,73,77,75,207,200,204,202,206,201,205,203,47,40,44,42,46,41,45,43,175,168,172,170,174,169,173,171,111,104,108,106,110,105,109,107,239,232,236,234,238,233,237,235,159,48,113,210,228,41,83,167,77,159,49,115,214,236,57,115,231,205,95,176,112,209,226,37,75,151,45,95,177,114,213,234,53,107,215,173,223,176,113,211,230,45,91,183,109,223,177,115,215,238,61,123,247,237,63,112,240,208,225,35,71,143,29,63,113,242,212,233,51,103,207,157,191,112,241,210,229,43,87,175,93,191,113,243,214,237,59,119,239,221,127,240,240,209,227,39,79,159,61,127,241,242,213,235,55,111,223,189,255,240,241,211,231,47,95,191,125,255,241,243,215,239,63,127,255,253,103,24,245,255,168,255,71,253,63,234,255,81,255,143,250,127,212,255,163,254,31,245,255,168,255,71,253,63,234,255,81,255,143,250,127,212,255,163,254,31,245,255,168,255,135,177,255,1,80,75,1,2,20,3,20,0,0,0,8,0,0,0,33,0,60,227,107,235,72,1,0,0,218,17,0,0,9,0,0,0,0,0,0,0,0,0,0,0,128,1,0,0,0,0,83,79,68,47,97,46,115,111,100,80,75,1,2,20,3,20,0,0,0,0,0,0,0,33,0,160,145,71,97,11,0,0,0,11,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,128,1,111,1,0,0,84,101,120,116,117,114,101,115,47,82,71,66,47,66,46,116,103,97,80,75,1,2,20,3,20,0,0,0,8,0,0,0,33,0,203,34,2,103,62,1,0,0,0,20,0,0,11,0,0,0,0,0,0,0,0,0,0,0,128,1,170,1,0,0,83,79,68,47,122,122,122,46,115,111,100,80,75,5,6,0,0,0,0,3,0,3,0,176,0,0,0,17,3,0,0,0,0};
static const unsigned char kZipB[] = {80,75,3,4,20,0,0,0,8,0,0,0,33,0,233,25,252,111,72,1,0,0,218,17,0,0,16,0,0,0,65,114,109,97,100,97,47,83,79,68,47,97,46,115,111,100,141,215,189,77,67,49,24,134,209,85,60,1,242,191,63,183,204,144,9,64,65,4,20,2,132,8,198,167,164,67,167,127,171,227,43,95,63,135,231,99,250,184,157,30,95,211,195,231,229,251,45,61,93,126,210,125,122,185,157,223,175,41,167,175,211,249,120,189,75,135,127,70,69,70,85,70,77,70,93,70,67,70,83,70,75,70,33,163,77,152,70,78,230,133,208,11,169,23,98,47,228,94,8,190,144,124,33,250,66,246,149,236,171,125,239,100,95,201,190,146,125,37,251,74,246,149,236,43,217,87,178,111,100,223,200,190,217,101,67,246,141,236,27,217,55,178,111,100,223,200,190,145,125,39,251,78,246,157,236,187,221,244,100,223,201,190,147,125,39,251,78,246,157,236,7,217,15,178,31,100,63,200,126,216,111,150,236,7,217,15,178,31,100,63,200,126,146,253,36,251,73,246,147,236,39,217,79,123,227,144,253,36,251,73,246,147,236,23,217,47,178,95,100,191,200,126,145,253,34,251,101,15,76,178,95,100,191,200,62,200,62,200,62,200,62,200,62,200,62,200,62,200,62,236,117,79,246,65,246,155,236,55,217,111,178,223,100,191,201,126,147,253,38,251,77,246,219,210,10,219,202,226,42,91,93,101,203,171,108,125,149,45,176,178,21,86,182,196,202,214,88,217,34,43,219,41,104,226,218,41,96,228,98,229,98,230,98,231,98,232,98,233,98,234,254,181,238,47,80,75,3,4,20,0,0,0,0,0,0,0,33,0,187,170,173,147,3,0,0,0,3,0,0,0,16,0,0,0,65,114,109,97,100,97,47,98,122,110,47,109,46,98,122,110,109,97,112,80,75,1,2,20,3,20,0,0,0,8,0,0,0,33,0,233,25,252,111,72,1,0,0,218,17,0,0,16,0,0,0,0,0,0,0,0,0,0,0,128,1,0,0,0,0,65,114,109,97,100,97,47,83,79,68,47,97,46,115,111,100,80,75,1,2,20,3,20,0,0,0,0,0,0,0,33,0,187,170,173,147,3,0,0,0,3,0,0,0,16,0,0,0,0,0,0,0,0,0,0,0,128,1,118,1,0,0,65,114,109,97,100,97,47,98,122,110,47,109,46,98,122,110,80,75,5,6,0,0,0,0,2,0,2,0,124,0,0,0,167,1,0,0,0,0};

std::string ZipText(const char* tag) {
  std::string s;
  for (int i = 0; i < 120; ++i) s += std::string("The quick brown fox ") + tag + " jumps " + std::to_string(i) + " times. ";
  return s;
}

std::shared_ptr<fleet::ZipSource> OpenMemZip(const unsigned char* d, size_t n) {
  std::string err;
  auto z = fleet::ZipSource::Open([d, n](uint64_t off, void* buf, size_t len) {
    if (off + len > n) return false;
    std::memcpy(buf, d + off, len);
    return true;
  }, n, &err);
  return z;
}

void TestZip() {
  using namespace fleet;
  auto a = OpenMemZip(kZipA, sizeof kZipA), b = OpenMemZip(kZipB, sizeof kZipB);
  CHECK(a && b);
  std::vector<uint8_t> o;
  CHECK(a->ReadFile("SOD/a.sod", &o) && std::string(o.begin(), o.end()) == ZipText("A"));  // Deflate
  CHECK(a->ReadFile("sod/A.SOD", &o) && o.size() == ZipText("A").size());                  // Schreibweise egal
  CHECK(a->ReadFile("Textures/RGB/b.tga", &o) && std::string(o.begin(), o.end()) == "stored-data");  // gespeichert
  CHECK(a->ReadFile("SOD/zzz.sod", &o) && o.size() == 5120 && o[255] == 255 && o[256] == 0);
  CHECK(!a->ReadFile("SOD/fehlt.sod", &o));
  CHECK(a->Exists("SOD") && a->Exists("textures/rgb") && !a->Exists("bzn"));
  std::vector<std::string> l = a->ListDir("SOD");
  CHECK(l.size() == 2);
  CHECK(a->ListDir("").size() == 2);  // SOD, Textures
  // Oberordner "Armada" wird weggelassen
  CHECK(b->ReadFile("SOD/a.sod", &o) && std::string(o.begin(), o.end()) == ZipText("B"));
  CHECK(b->ReadFile("bzn/m.bzn", &o) && std::string(o.begin(), o.end()) == "map");
  // Reihenfolge: die weiter oben stehende ZIP gewinnt
  OverlaySource ab, ba;
  ab.Add(a); ab.Add(b);
  ba.Add(b); ba.Add(a);
  CHECK(ab.ReadFile("SOD/a.sod", &o) && std::string(o.begin(), o.end()) == ZipText("A"));
  CHECK(ba.ReadFile("SOD/a.sod", &o) && std::string(o.begin(), o.end()) == ZipText("B"));
  CHECK(ab.ReadFile("bzn/m.bzn", &o) && ab.ListDir("SOD").size() == 2);
  // kaputte Daten
  const unsigned char junk[40] = {1, 2, 3};
  CHECK(!OpenMemZip(junk, sizeof junk));
  uint8_t out[4];
  const uint8_t bad[3] = {0xFF, 0xFF, 0xFF};
  CHECK(!InflateRaw(bad, 3, out, 4));
}

void TestShockwave() {
  using namespace fleet;
  ShockwaveDef d;
  d.rate = 110;
  d.radius_factor = 10;
  d.fade_factor = 0.3f;
  d.rays = 16;
  Shockwave s;
  s.scale = 50;  // Maximum 500
  s.age = 1.0f;  // aussen 220
  CHECK(ShockwaveOuterRadius(d, s) == 220.0f && ShockwaveMaxRadius(d, s) == 500.0f);
  CHECK(!ShockwaveDone(d, s) && std::fabs(ShockwaveFade(d, s) - 0.8f) < 1e-4f);  // 220 > 150: (500-220)/350
  s.age = 0.5f;
  CHECK(ShockwaveFade(d, s) == 1.0f);  // aussen 110 < 150
  s.age = 5.0f;
  CHECK(ShockwaveDone(d, s) && ShockwaveFade(d, s) == 0.0f);
  s.age = 1.0f;
  std::vector<float> mesh;
  BuildShockwaveMesh(d, s, &mesh);
  CHECK(mesh.size() == 16u * 3u * 6u * 5u);
  CHECK(Near(mesh[0], 0.0f) && Near(mesh[1], 0.0f) && Near(mesh[2], 220.0f) && mesh[3] == 0.0f && mesh[4] == 1.0f);  // Strahl 0, Eckpunkt 1: aussen
  bool in_range = true;
  for (size_t i = 0; i < mesh.size(); i += 5) in_range = in_range && mesh[i + 1] >= -2.0f && mesh[i + 1] <= 2.0f && std::fabs(mesh[i + 2]) <= 220.01f;
  CHECK(in_range);
  // Schaden: ohne damageBase keiner; mit Wert (base + Zufall x Varianz) x (1 - d/R) x dt, Ausnahme je Zielart
  HitInfo hit;
  CHECK(!ShockwaveHit(d, s, 10.0f, "zbattle", 0.5f, 0.1f, &hit));
  d.damage_base.base = 380;
  d.damage_base.overrides.emplace_back("zmama.odf", 2.0f);
  d.damage_variance.base = 10;
  d.damage_threshold.base = 605;
  d.hull_crew_modifier.base = 0.05f;
  s.age = 1.0f;  // R = 220
  CHECK(ShockwaveHit(d, s, 110.0f, "zbattle", 0.5f, 0.1f, &hit));
  CHECK(std::fabs(hit.damage - (380.0f + 5.0f) * 0.5f * 0.1f) < 1e-3f && hit.threshold == 605.0f && std::fabs(hit.hull_crew_mod - 0.05f) < 1e-6f);
  CHECK(ShockwaveHit(d, s, 110.0f, "zmama", 0.0f, 1.0f, &hit) && std::fabs(hit.damage - 2.0f * 0.5f) < 1e-3f);
  CHECK(!ShockwaveHit(d, s, 230.0f, "zbattle", 0.5f, 0.1f, &hit));  // ausserhalb des Radius
}

void TestDying() {
  using namespace fleet;
  Dying d;
  bool done = false;
  int fx = 0;
  float t = 0;
  while (!done && t < 10.0f) {  // ~3 s in 0,1-s-Schritten: etwa 60 Effekte (alle 0,05 s), dann fertig
    fx += StepDying(&d, 0.1f, &done);
    t += 0.1f;
  }
  CHECK(done && t > 2.9f && t < 3.2f);  // Standarddauer 3 s
  CHECK(fx >= 58 && fx <= 63);
  CHECK(MakeDying(0.5f, false).left == 3.0f && MakeDying(0.5f, true).left == 5.0f && MakeDying(20.0f, false).left == 5.0f);
  CHECK(DyingFxSize(10.0f) == 4.0f && DyingFxSize(100.0f) > 13.9f && DyingFxSize(100.0f) < 14.1f);
  bool done2 = false;
  Dying e;
  CHECK(StepDying(&e, 0.01f, &done2) == 0 && !done2);  // unter 0,05 s noch kein Effekt
}

namespace {
// Werte der GOG-Installation (RTS_CFG.h): Schild nimmt allen Schaden, kein Bonus auf die Systempunkte
struct CfgScope {
  fleet::CombatConfig old;
  CfgScope() : old(fleet::CombatCfg()) { fleet::CombatCfg() = fleet::CombatConfig{true, 0, 1.0f}; }
  ~CfgScope() { fleet::CombatCfg() = old; }
};

// battle.odf: Schild 7.5/10, Antrieb 2/10, Waffen 10/10, Lebenserhaltung 2.5/10, Sensoren 7.5/10, Rumpf 70, kritisch 0.5
fleet::HealthSpec BattleSpec() {
  using namespace fleet;
  HealthSpec s;
  s.shield_max = 100;
  s.shield_rate = 2.0f;
  s.shield_delay = 3.0f;
  s.crew_max = 400;
  s.hull_pct = 70;
  const float pct[kSysCount] = {7.5f, 2.0f, 2.5f, 7.5f, 10.0f};  // Reihenfolge Sys: Schild, Antrieb, Leben, Sensoren, Waffen
  const float loss[kSysCount] = {2, 5, 3, 2, 2};
  const float rep[kSysCount] = {60, 5, 10, 60, 60};
  for (int i = 0; i < kSysCount; ++i) {
    s.sys[i].points = 10;
    s.sys[i].hit_pct = pct[i];
    s.sys[i].crew_loss = loss[i];
    s.sys[i].repair_time = rep[i];
  }
  return s;
}
}  // namespace

void TestCombat() {
  using namespace fleet;
  CfgScope cfg;
  RaceRules race;
  race.repair_strength = 1.0f;

  // Trefferwurf (0x464dd0): ganzzahlig 0..99, Anteile in der Reihenfolge Antrieb, Leben, Waffen, Schild, Sensoren, Rumpf, Rest = kritisch
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    int n[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int roll = 0; roll < 100; ++roll) ++n[HitSystemForRoll(h, roll)];
    CHECK(n[kEngines] == 2 && n[kLife] == 3 && n[kWeapons] == 10 && n[kShield] == 7 && n[kSensors] == 8 && n[kHitHull] == 70);
    CHECK(n[kHitCritical] == 0);  // Summe 99,5: der ganzzahlige Wurf erreicht den kritischen Rest nie
    HealthSpec st;                // Station: Schild 25, Rumpf 70, kritisch 5
    st.sys[kShield].hit_pct = 25;
    st.hull_pct = 70;
    for (int i = 0; i < kSysCount; ++i) st.sys[i].points = i == kShield ? 1 : 0;
    Health b;
    InitHealth(&b, st, race);
    int m[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int roll = 0; roll < 100; ++roll) ++m[HitSystemForRoll(b, roll)];
    CHECK(m[kShield] == 25 && m[kHitHull] == 70 && m[kHitCritical] == 5);
    CHECK(!b.sys_ok(kEngines) && b.sys_ok(kShield));  // Punkte 0 = System fehlt
    Rng r;
    int seen[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 20000; ++i) ++seen[RollHitSystem(h, &r)];
    CHECK(seen[kHitHull] > 13000 && seen[kHitHull] < 15000 && seen[kEngines] > 300 && seen[kEngines] < 500);
  }

  // Schadenspaket (0x465100): Schild nimmt den Schutzanteil, der Rest und der Ueberlauf gehen an die Systeme
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    HitInfo hit;
    hit.damage = 60;
    hit.threshold = 1000;
    ApplyHitTo(&h, kEngines, hit);
    CHECK(h.shield == 40.0f && h.crew == 400.0f && h.sys[kEngines].points == 10);
    // Schwelle: Schaden 60 bei Schwelle 50 und Faktor 0,5: Besatzung - (Besatzung x 0,5 x 10 x 0,01 + min(10,1)) = 400 - 21
    hit.threshold = 50;
    hit.shield_crew_mod = 0.5f;
    h.shield = 100;
    ApplyHitTo(&h, kEngines, hit);
    CHECK(Near(h.crew, 379.0f) && h.shield == 40.0f);
    // Schild 0 nach dem Treffer: Aufladeverzoegerung startet
    ApplyHitTo(&h, kHitHull, hit);  // 40 - 60: Schild leer, 20 Ueberlauf
    CHECK(h.shield == 0.0f && Near(h.delay_left, 3.0f));
  }
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 10;
    HitInfo hit;
    hit.damage = 25;  // 10 Schild, 15 Ueberlauf
    ApplyHitTo(&h, kWeapons, hit);
    CHECK(h.shield == 0.0f && h.sys[kWeapons].points == 0 + (10 - 15 > 0 ? 1 : 0) && !h.sys_ok(kWeapons));
    // Ausfall: Besatzung - (Besatzung x 2 % + 1) = 400 - 9
    CHECK(Near(h.crew, 391.0f));
  }
  {  // Bruchteile der Systempunkte werden uebertragen (0x4653c9)
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 0.6f;
    ApplyHitTo(&h, kSensors, hit);
    CHECK(h.sys[kSensors].points == 10 && Near(h.carry, 0.6f));
    ApplyHitTo(&h, kSensors, hit);
    CHECK(h.sys[kSensors].points == 9 && Near(h.carry, 0.2f));
  }
  {  // ignoreShield: Schild bleibt, alles geht an die Systeme; Rumpftreffer kostet Besatzung
    Health h;
    InitHealth(&h, BattleSpec(), race);
    HitInfo hit;
    hit.damage = 10;
    hit.ignore_shield = true;
    hit.hull_crew_mod = 1.0f;
    ApplyHitTo(&h, kHitHull, hit);
    CHECK(h.shield == 100.0f && Near(h.crew, 400.0f - (400.0f * 10 * 0.01f + 1.0f)));
    ApplyHitTo(&h, kEngines, hit);
    CHECK(h.sys[kEngines].points == 0 && !h.sys_ok(kEngines));
  }
  {  // kritischer Treffer: nur mit mindestens einem ganzen Punkt
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 0.5f;
    ApplyHitTo(&h, kHitCritical, hit);
    CHECK(h.alive());
    hit.damage = 1.0f;
    ApplyHitTo(&h, kHitCritical, hit);
    CHECK(!h.alive());
  }
  {  // Schwierigkeitsfaktor und ausgeschaltete Systempunkte
    Health h;
    InitHealth(&h, BattleSpec(), race);
    HitInfo hit;
    hit.damage = 10;
    hit.damage_scale = 2.0f;
    ApplyHitTo(&h, kHitHull, hit);
    CHECK(h.shield == 80.0f);
    CombatCfg().use_system_hitpoints = false;
    ApplyHitTo(&h, kHitHull, hit);
    CHECK(h.shield == 80.0f);  // USE_SYSTEM_HITPOINTS = 0: kein Schaden
    CombatCfg().use_system_hitpoints = true;
  }
  {  // SYSTEM_HITPOINTS_BONUS gilt nur fuer vorhandene Systeme
    CombatCfg().system_hitpoints_bonus = 200;
    Health h;
    HealthSpec s = BattleSpec();
    s.sys[kSensors].points = 0;
    InitHealth(&h, s, race);
    CHECK(h.sys[kEngines].max_points == 210 && h.sys[kSensors].max_points == 0 && !h.sys_ok(kSensors));
    CombatCfg().system_hitpoints_bonus = 0;
  }

  // Trefferanteile ausgefallener Systeme gehen zu gleichen Teilen an die lebenden (0x467620) und kommen bei der Reparatur zurueck (0x4676d0)
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 10;
    ApplyHitTo(&h, kEngines, hit);  // 10 Punkte -> Antrieb faellt aus (2 % auf die vier anderen)
    CHECK(!h.sys_ok(kEngines) && h.sys[kEngines].hit_pct == 0.0f);
    CHECK(Near(h.sys[kShield].hit_pct, 7.5f + 0.5f) && Near(h.sys[kLife].hit_pct, 2.5f + 0.5f) && Near(h.sys[kWeapons].hit_pct, 10.5f));
    float sum = h.hull_pct;
    for (const SystemBlock& b : h.sys) sum += b.hit_pct;
    CHECK(Near(sum, 99.5f));
    // alle anderen ausfallen lassen: Rest sind Rumpf 70 und kritisch 30
    for (int i : {kLife, kWeapons, kShield, kSensors}) ApplyHitTo(&h, i, hit);
    int n[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int roll = 0; roll < 100; ++roll) ++n[HitSystemForRoll(h, roll)];
    CHECK(n[kHitHull] == 70 && n[kHitCritical] == 30);
    // Antrieb reparieren (5 s ganz): Anteile kehren zurueck
    for (int i = 0; i < 700; ++i) StepHealth(&h, 0.1f);
    CHECK(!h.derelict);
  }
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 10;
    ApplyHitTo(&h, kEngines, hit);
    for (int i = 0; i < 49; ++i) StepHealth(&h, 0.1f);
    CHECK(!h.sys_ok(kEngines) && h.sys[kEngines].points < 10);
    for (int i = 0; i < 20; ++i) StepHealth(&h, 0.1f);
    CHECK(h.sys_ok(kEngines) && h.sys[kEngines].points == 10);
    CHECK(Near(h.sys[kEngines].hit_pct, 2.0f) && Near(h.sys[kShield].hit_pct, 7.5f) && Near(h.sys[kWeapons].hit_pct, 10.0f));
  }

  // Reparatur (0x4683d0): Zeit je Punkt = Reparaturzeit / Punkte x Besatzungsfaktor x Rassenstaerke; alle Systeme gleichzeitig; erst wenn
  // alle Punkte da sind, laeuft das System wieder
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 10;
    ApplyHitTo(&h, kWeapons, hit);  // Waffen ausgefallen: 60 s / 10 Punkte = 6 s je Punkt
    ApplyHitTo(&h, kEngines, hit);  // Antrieb: 0,5 s je Punkt
    StepHealth(&h, 5.9f);
    CHECK(h.sys[kWeapons].points == 0 && h.sys[kEngines].points == 10 && h.sys_ok(kEngines));  // Antrieb fertig (5 s), Waffen noch nicht
    StepHealth(&h, 0.2f);
    CHECK(h.sys[kWeapons].points == 1 && !h.sys_ok(kWeapons));
    for (int i = 0; i < 540; ++i) StepHealth(&h, 0.1f);
    CHECK(h.sys[kWeapons].points == 10 && h.sys_ok(kWeapons));
  }
  {  // Eigenheit des Originals: ein Treffer, der das System nicht ausfallen laesst, wird beim naechsten Bild sofort um einen Punkt repariert
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    HitInfo hit;
    hit.damage = 1;
    ApplyHitTo(&h, kShield, hit);
    CHECK(h.sys[kShield].points == 9);
    StepHealth(&h, 0.01f);
    CHECK(h.sys[kShield].points == 10);
  }
  {  // Rasse und Besatzungsstatus: gelb = Faktor repairYellow, mal Rassenstaerke
    RaceRules rr;
    rr.repair_strength = 1.5f;
    rr.crew_yellow = 0.4f;
    rr.crew_red = 0.2f;
    rr.repair_yellow = 1.5f;
    rr.repair_red = 2.0f;
    Health h;
    InitHealth(&h, BattleSpec(), rr);
    h.shield = 0;
    h.crew = 100;  // 25 % = gelb
    HitInfo hit;
    hit.damage = 10;
    ApplyHitTo(&h, kWeapons, hit);
    // je Punkt 6 s x 1,5 (Staerke) x 1,5 (gelb) = 13,5 s; der Besatzungsverlust beim Ausfall (2 % + 1) aendert nichts mehr daran, weil
    // die Zeit beim Ausfall gesetzt wird; danach gilt der Faktor der aktuellen Besatzung
    h.crew = 100;
    StepHealth(&h, 13.4f);
    CHECK(h.sys[kWeapons].points == 0);
    StepHealth(&h, 0.2f);
    CHECK(h.sys[kWeapons].points == 1);
  }

  // Lebenserhaltung aus (0x465f6f): Besatzung - (Besatzung x Verlust x dt x 0,01 + min(dt,1)), unter 1 wird das Schiff Derelict
  {
    Health h;
    HealthSpec s = BattleSpec();
    s.crew_max = 100;
    s.sys[kLife].repair_time = 1e9f;
    InitHealth(&h, s, race);
    h.sys[kLife].points = 0;
    h.sys[kLife].active = false;
    StepHealth(&h, 1.0f);
    CHECK(Near(h.crew, 100.0f - (100.0f * 2.0f * 1.0f * 0.01f + 1.0f)));
    for (int i = 0; i < 200 && !h.derelict; ++i) StepHealth(&h, 1.0f);
    CHECK(h.derelict && h.crew == 0.0f);
    for (int i = 0; i < kSysCount; ++i) CHECK(!h.sys_ok(i));  // alle Systeme aus
    CHECK(h.shield == 0.0f);
  }
  {  // Besatzung zurueck: Systeme mit allen Punkten laufen wieder, beschaedigte bleiben aus
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.sys[kWeapons].points = 4;
    SetCrew(&h, 0.5f);
    CHECK(h.derelict && h.crew == 0.0f && !h.sys_ok(kEngines));
    SetCrew(&h, 20.0f);
    CHECK(!h.derelict && h.sys_ok(kEngines) && h.sys_ok(kShield) && !h.sys_ok(kWeapons));
  }
  {  // Selbstzerstoerung laeuft: ohne Besatzung explodiert das Schiff
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.self_destruct = true;
    SetCrew(&h, 0.2f);
    CHECK(!h.alive() && !h.derelict);
  }
  {  // Objekte ohne Besatzung werden nie Derelict
    Health h;
    HealthSpec s = BattleSpec();
    s.crew_max = 0;
    InitHealth(&h, s, race);
    SetCrew(&h, 0.0f);
    CHECK(!h.derelict);
  }

  // Schildaufladung (0x466b00): erst nach der Verzoegerung, der Rest des Bildes zaehlt
  {
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.shield = 0;
    h.delay_left = 3.0f;
    StepHealth(&h, 2.0f);
    CHECK(h.shield == 0.0f && Near(h.delay_left, 1.0f));
    StepHealth(&h, 2.0f);  // 1 s Verzoegerung, dann 1 s Aufladen x 2
    CHECK(Near(h.shield, 2.0f) && h.delay_left == 0.0f);
    h.sys[kShield].active = false;  // Schildsystem ausgefallen (Punkte weg, noch nicht repariert): keine Aufladung
    h.sys[kShield].points = 0;
    h.sys[kShield].countdown = h.sys[kShield].per_point = 6.0f;
    StepHealth(&h, 5.0f);
    CHECK(Near(h.shield, 2.0f));
  }
  {  // Zeitweise ausgeschaltet (Spezialwaffe): zaehlt nur bei ausgeschaltetem System, erst danach wieder an
    Health h;
    InitHealth(&h, BattleSpec(), race);
    h.sys[kShield].active = false;
    h.sys[kShield].disable_timer = 5.0f;
    StepHealth(&h, 2.0f);
    CHECK(!h.sys_ok(kShield) && Near(h.sys[kShield].disable_timer, 3.0f));
    StepHealth(&h, 3.1f);
    CHECK(h.sys_ok(kShield));
  }

  // Besatzungsstatus
  {
    Health s;
    HealthSpec sp;
    sp.crew_max = 100;
    InitHealth(&s, sp, race);
    s.crew = 30;
    CHECK(s.crew_status() == 1 && s.weapon_delay_mult() == 1.0f);
    s.crew = 10;
    CHECK(s.crew_status() == 2 && s.weapon_delay_mult() == 2.0f);
  }

  Rng r;
  int hits = 0;
  for (int i = 0; i < 4000; ++i) hits += RollHit(&r, 0.25f) ? 1 : 0;
  CHECK(hits > 800 && hits < 1200);
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < 1000; ++i) {
    const float d = RollDamage(&r, 24, 2);
    lo = std::min(lo, d);
    hi = std::max(hi, d);
  }
  CHECK(lo >= 22.0f && hi <= 26.0f && hi - lo > 3.0f);
  CHECK(RollHit(&r, 1.0f) && !RollHit(&r, 0.0f));
}

void TestTeamsAndDifficulty() {
  using namespace fleet;
  CfgScope cfg;
  // Beziehungstabelle: Team 0 neutral, gleiche Gruppe verbuendet, sonst Feind
  CHECK(TeamRelation(0, 3) == kNeutral && TeamRelation(2, 0) == kNeutral && TeamRelation(2, 2) == kAlly && TeamRelation(1, 2) == kEnemy);
  CHECK(IsEnemyTeam(1, 2) && !IsEnemyTeam(1, 1) && !IsEnemyTeam(0, 1));
  // Schwierigkeit: 1 leicht, 3 schwer, sonst normal
  CombatCfg().easy_damage = 0.5f;
  CombatCfg().hard_damage = 2.0f;
  CHECK(DifficultyDamageScale(1) == 0.5f && DifficultyDamageScale(3) == 2.0f && DifficultyDamageScale(2) == 1.0f && DifficultyDamageScale(0) == 1.0f);
  Health h;
  HealthSpec sp;
  sp.shield_max = 100;
  InitHealth(&h, sp, RaceRules());
  HitInfo hi;
  hi.damage = 10;
  hi.damage_scale = DifficultyDamageScale(3);
  ApplyHitTo(&h, kHitHull, hi);
  CHECK(h.shield == 80.0f);  // schwer: doppelter Schaden
}

void TestBoarding() {
  using namespace fleet;
  CfgScope cfg;
  RaceRules race;
  auto make = [&](float crew, float crew_max) {
    HealthSpec sp;
    sp.crew_max = crew_max;
    sp.shield_max = 100;
    sp.shield_delay = 3.0f;
    for (int i = 0; i < kSysCount; ++i) sp.sys[i].points = 10;
    Health h;
    InitHealth(&h, sp, race);
    h.crew = crew;
    return h;
  };
  // Besatzung hinzufuegen: der Ueberschuss kommt zurueck (0x467570)
  {
    Health h = make(90, 100);
    CHECK(Near(AddCrew(&h, 25.0f), 15.0f) && h.crew == 100.0f);
    CHECK(AddCrew(&h, 1.0f) == 1.0f);
  }
  // Besatzung abziehen: zurueck kommt hoechstens die alte Besatzung (0x4674d0), darunter wird das Schiff Derelict
  {
    Health h = make(3, 100);
    CHECK(Near(RemoveCrew(&h, 5.0f), 3.0f) && h.derelict && h.crew == 0.0f);
    Health g = make(50, 100);
    CHECK(RemoveCrew(&g, 5.0f) == 5.0f && g.crew == 45.0f);
    Health z = make(0, 0);
    CHECK(RemoveCrew(&z, 5.0f) == 0.0f);
  }
  // Transporter-Schritt (0x467050)
  {
    Health src = make(100, 100), tgt = make(50, 100);
    CHECK(TransporterStep(&src, &tgt, 301.0f, false).stop && src.crew == 100.0f);  // zu weit
    Health full = make(100, 100);
    CHECK(TransporterStep(&src, &full, 100.0f, false).stop && src.crew == 100.0f);  // Freund ohne Platz
    BeamStep st = TransporterStep(&src, &tgt, 100.0f, false);
    CHECK(!st.stop && st.created && st.party_crew == 5.0f && src.crew == 95.0f && !st.enemy);
    // Feind: der Schild muss (fast) unten sein, danach bleibt er unten
    Health foe = make(50, 100);
    CHECK(TransporterStep(&src, &foe, 100.0f, true).stop && src.crew == 95.0f);
    foe.shield = 0.5f;  // 0,5 %
    st = TransporterStep(&src, &foe, 100.0f, true);
    CHECK(!st.stop && st.created && st.enemy && Near(foe.delay_left, 3.0f) && src.crew == 90.0f);
    // Rote Besatzung der Quelle: nach dem Schritt waere die naechste Runde im roten Bereich -> Ende (die Mannschaft ist aber gebildet)
    Health weak = make(24, 100);
    st = TransporterStep(&weak, &tgt, 100.0f, false);
    CHECK(st.created && st.stop && weak.crew == 19.0f);
  }
  // Ankunft (0x55f750)
  {
    Health t = make(90, 100);  // Freund: Besatzung dazu, Rest bleibt in der Mannschaft
    BoardResult r = ArriveBoardingParty(&t, 1, 1.0f, 20.0f, 1.0f, 1, false);
    CHECK(Near(r.party_left, 10.0f) && t.crew == 100.0f && !r.owner_changed);
    Health e = make(10, 100);  // Feind: Staerke 1,5 gegen 1,0, Mannschaft 10 -> 15 Mann Kampfkraft; Ziel hat 10
    r = ArriveBoardingParty(&e, 2, 1.0f, 10.0f, 1.5f, 1, true);
    CHECK(r.owner_changed && Near(e.crew, 5.0f / 1.5f) && !e.derelict && e.crew_max == 100.0f);
    Health e2 = make(50, 100);  // Ziel hat genug Leute: es verliert 15, die Mannschaft ist weg, kein Besitzerwechsel
    r = ArriveBoardingParty(&e2, 2, 1.0f, 10.0f, 1.5f, 1, true);
    CHECK(!r.owner_changed && Near(e2.crew, 35.0f) && r.party_left == 0.0f);
    Health e3 = make(10, 100);  // Kampf ohne Rest: Ziel faellt aus (Derelict), wird aber nicht uebernommen
    r = ArriveBoardingParty(&e3, 2, 1.0f, 10.0f, 1.0f, 1, true);
    CHECK(!r.owner_changed && e3.derelict && e3.crew == 0.0f);
    Health d = make(0, 100);  // Derelict desselben Teams: Besatzung dazu, Rasse der Mannschaft
    d.derelict = true;
    r = ArriveBoardingParty(&d, 1, 1.0f, 5.0f, 1.0f, 1, false);
    CHECK(r.race_changed && !r.owner_changed && d.crew == 5.0f);
    Health d2 = make(0, 100);  // Derelict eines Feindes: uebernommen
    d2.derelict = true;
    r = ArriveBoardingParty(&d2, 2, 1.0f, 5.0f, 1.0f, 1, true);
    CHECK(r.owner_changed && d2.crew == 5.0f && !d2.derelict);
    Health d3 = make(0, 100);  // Derelict eines Verbuendeten (anderes Team, kein Feind): Besatzung dazu, aber kein Wechsel
    d3.derelict = true;
    r = ArriveBoardingParty(&d3, 2, 1.0f, 5.0f, 1.0f, 1, false);
    CHECK(!r.owner_changed && !r.race_changed && d3.crew == 5.0f);
  }
}

void TestShipSim() {
  using namespace fleet;
  PhysicsDef pd;
  CHECK(!FlightFromPhysics(pd).valid);
  pd.impulse_speed = 20; pd.warp_speed = 80; pd.turn_radius = 30; pd.max_roll = 40; pd.front_back_inertia = 20;
  FlightParams fp = FlightFromPhysics(pd);
  CHECK(fp.valid && fp.impulse == 20 && fp.warp == 80);
  ShipState s;
  float t[3] = {400, 0, 300};
  SetShipTarget(&s, t);
  float maxspeed = 0;
  for (int i = 0; i < 6000 && s.has_target; ++i) {
    StepShip(fp, &s, 0.05f);
    maxspeed = std::max(maxspeed, s.speed);
    CHECK(std::isfinite(s.pos[0]) && std::isfinite(s.yaw));
  }
  CHECK(!s.has_target);
  CHECK(maxspeed > fp.impulse);  // Warp genutzt
  for (int i = 0; i < 400; ++i) StepShip(fp, &s, 0.05f);
  CHECK(std::fabs(s.speed) < 0.1f);
  float dx = s.pos[0] - t[0], dz = s.pos[2] - t[2];
  CHECK(std::sqrt(dx * dx + dz * dz) < 120);
  CHECK(std::fabs(s.roll) < 0.2f);
  float f[3];
  ShipState u;
  u.yaw = 0;
  ShipForward(u, f);
  CHECK(std::fabs(f[2] - 1) < 1e-5f);
  Mat4 m, r;
  ShipMatrices(u, 2.0f, &m, &r);
  // Modell-Bug (0,0,-1) muss auf die Blickrichtung (0,0,1) zeigen
  CHECK(std::fabs(m.m[10] * -1 - (-2.0f)) < 1e-4f || std::fabs(m.m[10] + 2.0f) < 1e-4f);
}

void TestHud() {
  using namespace fleet;
  GuiCfg g;
  g.AddText("// Kommentar\nFooRect = 1 2 3 4 // Ende\nname = \"abc\"\n#include \"x.cfg\"\nbar = .5\n", [](const std::string& n, std::string* t) { *t = n == "x.cfg" ? "inc = 7\n" : ""; return n == "x.cfg"; });
  float r[4] = {0, 0, 0, 0};
  CHECK(g.Rect("foorect", r) && r[0] == 1 && r[3] == 4);
  CHECK(g.Str("NAME") == "abc" && g.Int("inc", 0) == 7 && !g.Has("fehlt") && g.Int("bar", 9) == 0);
  CHECK(!g.Rect("bar", r));

  struct Mem : FileSource {
    std::map<std::string, std::string> f;
    bool ReadFile(const std::string& p, std::vector<uint8_t>* o) override {
      auto it = f.find(p);
      if (it == f.end()) return false;
      o->assign(it->second.begin(), it->second.end());
      return true;
    }
    bool Exists(const std::string& p) override { return f.count(p) != 0; }
    std::vector<std::string> ListDir(const std::string&) override { return {}; }
  } mem;
  std::string cfg;
  auto panel = [&](const char* n, const char* area, const char* bg) {
    cfg += std::string(n) + "PanelArea = " + area + "\n" + n + "BackgroundPanel = \"" + bg + "\"\n" + n + "BackgroundPanelSize = 1\n" + n + "BackgroundPanel_0 = 0 0 10 10\n";
  };
  panel("minimap", "0 334 146 146", "map_panel");
  panel("info", "146 367 247 113", "info_panel");
  panel("control", "524 334 116 146", "control_panel");
  panel("cinematic", "393 356 131 124", "cinematic_panel");
  panel("speed", "146 327 247 40", "speed_panel");
  panel("officer", "559 0 81 16", "officer_panel");
  panel("crew", "477 0 81 16", "crew_panel");
  panel("dilithium", "395 0 81 16", "dilithium_panel");
  cfg += "speedSingleButtonArea = 15 4 32 32\nspeedSingleButtonGap = 3\ncinematicMenuButtonArea = 69 14 48 15\ncinematicCommButtonArea = 14 14 48 15\n";
  const int bx[12] = {9, 42, 75, 9, 42, 75, 9, 42, 75, 9, 42, 75}, by[12] = {8, 8, 8, 41, 41, 41, 74, 74, 74, 107, 107, 107};
  for (int i = 0; i < 12; ++i) cfg += "controlButton" + std::to_string(i + 1) + " = " + std::to_string(bx[i]) + " " + std::to_string(by[i]) + " 32 32\n";
  mem.f["misc/gui_fed.cfg"] = cfg;
  mem.f["Sprites/gui_federation.spr"] = "@reference=256\n@tmaterial=interface\nmap_panel.0 fguia04 0 0 128 9\ninfo_panel.0 fguib04 0 0 256 18\nGhost.x foo 1 2 3 4\n";
  HudData hd;
  CHECK(hd.Load(mem, HudRace::Federation) && hd.loaded());
  std::string tex;
  float uv[4];
  bool add = true;
  CHECK(hd.Sprite("MAP_PANEL.0", &tex, uv, &add) && tex == "fguia04" && !add && uv[2] == 0.5f && Near(uv[3], 9.0f / 256.0f));
  CHECK(!hd.Sprite("fehlt", &tex, uv, &add));
  CHECK(hd.TextureNames().size() == 3);
  {  // Originalschrift: 96 Glyphen, 21 je Zeile im Abstand 6; Breite = Abstand zum naechsten Glyph - 1
    std::string fs = "sprite_table\n@animation font_uv\noffset 96 0.0 step\n@keyframes\n";
    for (int i = 0; i < 96; ++i) fs += std::to_string((i % 21) * 6) + " " + std::to_string((i / 21) * 11) + " #g\n";
    fs += "@animation font_w\nwidth 95 0.0 step\n@keyframes\n5\n";
    mem.f["Sprites/font.spr"] = fs;
    HudData fh;
    CHECK(fh.Load(mem, HudRace::Federation) && fh.has_font());
    float guv[4], adv = 0;
    CHECK(fh.Glyph('!', guv, &adv) && Near(adv, 6.0f) && guv[0] > 6.0f / 128.0f && guv[2] < 11.0f / 128.0f);  // '!' = Glyph 1 bei x=6
    CHECK(fh.Glyph(200, guv, &adv));  // ausserhalb: Ersatzzeichen
    const HudLayout FL = BuildHudLayout(fh, 1080, 2400, HudState());
    int glyphs = 0;
    for (const HudDraw& d : FL.draws) if (d.tex == "font") ++glyphs;
    CHECK(glyphs > 20 && FL.texts.empty());  // Beschriftungen als Glyph-Rechtecke statt eingebauter Schrift
    CHECK(fh.TextureNames().back() == "font");
    CHECK(fh.Glyph(0xE4, guv, &adv) && fh.Glyph(0x20AC, guv, &adv));  // ae-Umlaut vorhanden, Euro -> Ersatzzeichen
    const float white[3] = {1, 1, 1}, panel[4] = {0, 0, 0, 0.5f};
    const HudTextBlock tb = LayoutHudText(fh, "Zwei W\xC3\xB6rter\nund dritte Zeile", 10, 10, 20, 100, white, panel);
    CHECK(!tb.draws.empty() && tb.draws[0].tex.empty() && tb.h > 20 && tb.w <= 100.5f);  // Panel + Glyphen, Umbruch
    CHECK(LayoutHudText(HudData(), "x", 0, 0, 10, 100, white, nullptr).draws.empty());
  }
  HudData none;
  Mem empty;
  CHECK(!none.Load(empty, HudRace::Borg));

  for (const auto& dim : {std::pair<int, int>{1080, 2400}, {2400, 1080}, {800, 480}, {480, 800}}) {
    const float W = static_cast<float>(dim.first), H = static_cast<float>(dim.second);
    HudState st;
    st.info_lines = {"Test", "Zeile"};
    for (int map = 0; map < 2; ++map) {
      st.ui.map_mode = map == 1;
      const HudLayout L = BuildHudLayout(hd, dim.first, dim.second, st);
      CHECK(L.portrait == (H > W) && L.scale >= 0.5f);
      CHECK(L.view.h > 0.5f * H && L.view.h < H && L.view.y >= 0);  // freier Bereich behaelt den groessten Teil
      CHECK(L.buttons.size() >= 20);
      int overlap = 0, outside = 0;
      for (size_t i = 0; i < L.panels.size(); ++i) {
        const UiRect& a = L.panels[i];
        if (a.x < -0.5f || a.y < -0.5f || a.x + a.w > W + 0.5f || a.y + a.h > H + 0.5f) ++outside;
        // Felder liegen ausserhalb des freien 3D-Bereichs (oben bei Hochformat: nur die obere Reihe, y = 0)
        if (a.y > 0.5f && a.y < L.view.y + L.view.h - 0.5f && a.y + a.h > L.view.y + 0.5f) ++outside;
        for (size_t j = i + 1; j < L.panels.size(); ++j) {
          const UiRect& b = L.panels[j];
          if (a.x < b.x + b.w - 0.5f && b.x < a.x + a.w - 0.5f && a.y < b.y + b.h - 0.5f && b.y < a.y + a.h - 0.5f) ++overlap;
        }
      }
      CHECK(overlap == 0 && outside == 0);
      int bad = 0;
      for (const HudButtonSlot& b : L.buttons) {
        if (b.rect.x < 0 || b.rect.y < 0 || b.rect.x + b.rect.w > W + 0.5f || b.rect.y + b.rect.h > H + 0.5f) ++bad;
        if (b.rect.w < 0.04f * std::min(W, H)) ++bad;  // Tasten gross genug zum Tippen
        if (b.enabled) {
          CHECK(HudHit(L, b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2) == b.action);
          CHECK(HudCovers(L, b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2));
        }
      }
      CHECK(bad == 0);
      CHECK(HudHit(L, 5, 5) == UiAction::None && !HudCovers(L, W / 2, L.view.y + 0.5f * L.view.h));
    }
  }
  HudState on;
  on.ui.firing = true;
  on.ui.muted = true;
  int active = 0;
  for (const HudButtonSlot& b : BuildHudLayout(hd, 1080, 2400, on).buttons) if (b.active) ++active;
  CHECK(active == 3);  // Feuer, Ton (Leiste) und Ton (Kinosicht)
  // Befehlsmodus: Steuerfeld/Leiste aus HudState, Menue-Knopf im Kinofeld
  HudState cm;
  cm.commands = true;
  cm.control.assign(12, HudCmd());
  cm.control[0].action = UiAction::Move; cm.control[0].sprite = "b_move";
  cm.control[2].action = UiAction::Stop; cm.control[2].sprite = "b_stop"; cm.control[2].enabled = false;
  cm.control[4].action = UiAction::Orders; cm.control[4].sprite = "b_order";
  cm.rail.assign(6, HudCmd());
  cm.rail[1].action = UiAction::Special1; cm.rail[1].sprite = "b_gcloak";
  for (const auto& dim : {std::make_pair(1080, 2400), std::make_pair(2400, 1080)}) {
    const HudLayout CL = BuildHudLayout(hd, dim.first, dim.second, cm);
    int has_move = 0, has_orders = 0, has_stop_enabled = 0, has_special = 0, has_menu = 0, has_old = 0;
    for (const HudButtonSlot& b : CL.buttons) {
      has_move += b.action == UiAction::Move && b.enabled;
      has_orders += b.action == UiAction::Orders && b.enabled;
      has_stop_enabled += b.action == UiAction::Stop && b.enabled;
      has_special += b.action == UiAction::Special1 && b.enabled;
      has_menu += b.action == UiAction::Menu && b.enabled;
      has_old += b.action == UiAction::Info || b.action == UiAction::Race || b.action == UiAction::Folder || b.action == UiAction::Mute;
    }
    CHECK(has_move == 1 && has_orders == 1 && has_stop_enabled == 0 && has_special == 1 && has_menu == 1 && has_old == 0);
  }
}

}  // namespace


fleet::SgElem SaveZero(const fleet::SgField& f) {
  fleet::SgElem e;
  e.type = f.kind == "bool" ? 1 : f.kind == "int" ? 4 : f.kind == "float" ? 5 : f.kind == "vec3" ? 9 : f.kind == "ptr" ? 8 : f.kind == "prjid" ? 7 : f.kind == "matrix" ? 11 : 2;
  size_t n = f.kind == "bool" ? 1 : f.kind == "int" || f.kind == "float" || f.kind == "ptr" ? 4 : f.kind == "vec3" ? 12 : f.kind == "matrix" ? 48 : f.kind == "prjid" ? 8 : static_cast<size_t>(f.size);
  e.data.assign(n, 0);
  return e;
}

void TestSaveGame() {
  using namespace fleet;
  // Grundkarte von Hand: Kopf (17 Elemente), ein Schiff im Kartenmodus, Fuss (Teamtabelle, EmptyMission, Listen)
  SgFile base;
  base.head_text = "version [1] =\r\n2051\r\nsaveGameDesc = 00\r\nbinarySave [1] =\r\ntrue\r\n";
  base.head.push_back(SgStr("test.bzn", 16));
  base.head.push_back(SgBool(true));
  base.head.push_back(SgStr("terrain", 100));
  for (int i = 0; i < 6; ++i) base.head.push_back(SgFloat(float(i)));
  base.head.push_back(SgBool(false));
  for (int i = 0; i < 3; ++i) base.head.push_back(SgFloat(0.5f));
  base.head.push_back(SgStr("bg", 100));
  base.head.push_back(SgStr("tech", 128));
  base.head.push_back(SgStr("fed", 128));
  base.head.push_back(SgInt(1));
  const auto* ml = SgLayouts("fgalaxy", true);
  CHECK(ml && !ml->empty());
  SgObject o;
  o.odf = "fgalaxy";
  SgElem pr;
  pr.type = 7;
  pr.data.assign(8, 0);
  std::memcpy(pr.data.data(), "fgalaxy", 7);
  o.e.push_back(pr);
  o.e.push_back(SgInt(static_cast<int32_t>(0x80000005u)));
  float p0[3] = {1, 2, 3};
  o.e.push_back(SgVec3(p0));
  o.e.push_back(SgInt(2));
  o.e.push_back(SgStr("fgalaxy1", 40));
  SgElem ptr;
  ptr.type = 8;
  ptr.data.assign(4, 0);
  o.e.push_back(ptr);
  SgElem mat;
  mat.type = 11;
  mat.data.assign(48, 0);
  o.e.push_back(mat);
  for (const SgField& f : (*ml)[0].fields) {
    SgElem z = SaveZero(f);
    if (f.kind == "euler") {
      o.e.push_back(SgFloat(0));
      o.e.push_back(SgFloat(0));
      for (int k = 0; k < 3; ++k) o.e.push_back(SaveZero(SgField{"vec3", "", 12, 1, 0}));
    } else {
      o.e.push_back(z);
    }
  }
  base.objects.push_back(o);
  for (int i = 0; i < 63; ++i) base.tail.push_back(SgFloat(0));
  base.tail.push_back(SgStr("EmptyMission", 40));
  base.tail.push_back(ptr);
  base.tail.push_back(SgStr("", 64));
  base.tail.push_back(SgStr("", 64));
  SgElem title;
  title.type = 0;
  title.data.assign(64, 0);
  base.tail.push_back(title);
  base.tail.push_back(SgInt(0));
  auto bytes = WriteSgFile(base);
  SgFile again;
  std::string err;
  CHECK(ReadSgFile(bytes.data(), bytes.size(), &again, &err));
  CHECK(WriteSgFile(again) == bytes);
  CHECK(again.objects.size() == 1);

  SaveGame g;
  g.desc = "Mission 1 Nov 10";
  g.teams[1].exists = true;
  g.teams[1].scrap = 3500;
  g.teams[1].crew = 4000;
  SaveObject so;
  so.odf = "fgalaxy";
  so.id = 77;
  so.team = 1;
  so.label = "Enterprise";
  so.base_index = 0;
  so.pos[0] = 10;
  so.pos[1] = 20;
  so.pos[2] = 30;
  so.has_state = true;
  so.shield = 150;
  so.shield_max = 400;
  so.crew = 321;
  for (int i = 0; i < 5; ++i) so.sys[i] = 10 - i;
  g.objects.push_back(so);
  SgFile out;
  CHECK(BuildSave(base, g, &out, &err));
  auto sb = WriteSgFile(out);
  SgFile back;
  CHECK(ReadSgFile(sb.data(), sb.size(), &back, &err));
  CHECK(WriteSgFile(back) == sb);
  SaveGame g2;
  CHECK(ParseSave(back, &g2, &err));
  CHECK(g2.desc == g.desc);
  CHECK(g2.objects.size() == 1);
  if (g2.objects.size() == 1) {
    const SaveObject& r = g2.objects[0];
    CHECK(r.odf == "fgalaxy" && r.id == 77 && r.team == 1 && r.label == "Enterprise");
    CHECK(r.pos[0] == 10 && r.pos[2] == 30);
    CHECK(r.has_state && r.shield == 150 && r.shield_max == 400 && r.crew == 321 && r.sys[0] == 10 && r.sys[4] == 6);
  }
  CHECK(g2.teams[1].scrap == 3500 && g2.teams[1].crew == 4000 && g2.teams[1].exists);
  // Kartenmodus-Datei ist kein Spielstand
  CHECK(!ParseSave(base, &g2, &err));
}


static void TestHoverSim() {
  using namespace fleet;
  HoverOdf o; o.impulseSpeed = 20; o.warpSpeed = 80; o.velocCoeffStill = 0.5f; o.velocWhenCoeffHalf = 0.7f;
  o.pitchRotationSpeed = 0.0667f; o.maxPitch = 45; o.rollRotationSpeed = 0.5f; o.maxRoll = 40; o.turnRadius = 30;
  o.frontBackInertia = 50; o.maxReverseSpeed = 4; o.upDownInertia = 7.5f; o.leftRightInertia = 7.5f; o.maxVerticalSpeed = 0.667f;
  o.rotationAcceleration = 0; o.rotationStiffness = 1;
  HoverClass c = HoverClass::From(o);
  CHECK(std::fabs(c.c4a0 - 1.0f / 2.5f) < 1e-6f);      // 1/(fbi/impulse)
  CHECK(std::fabs(c.c4ac - 8.0f) < 1e-5f);              // 1/fbi' * maxFwd
  HoverCraftState s; s.Init(c);
  HoverControls ctl; ctl.c3 = 1;
  for (int i = 0; i < 600; ++i) HoverSimulate(c, &s, ctl, 0.05f);
  CHECK(std::fabs(s.speed - 20.0f) < 0.05f);            // Gleichgewicht = Impulsgeschwindigkeit
  CHECK(std::fabs(s.pos[0]) < 1e-3f && s.pos[2] > 100);  // geradeaus nach +z
  // Kurve: Lenken -> Querneigung -> Gierrate
  ctl.c0 = 1;
  for (int i = 0; i < 400; ++i) HoverSimulate(c, &s, ctl, 0.05f);
  CHECK(std::fabs(s.roll) > 0.1f);
  CHECK(std::fabs(s.angvel[1]) > 0.1f);
  // Matrix-Konsistenz: Euler -> Matrix -> Euler
  float p0 = s.pitch, y0 = s.yaw, r0 = s.roll; s.SetFromMatrix();
  CHECK(std::fabs(s.pitch - p0) < 1e-4f && std::fabs(s.roll - r0) < 1e-4f && std::fabs(std::sin(s.yaw - y0)) < 1e-4f);

  // Hindernisvermeidung: ohne Hindernis unveraendert, mit ruhendem Hindernis voraus weicht die Richtung aus
  {
    HoverCraftState sa; sa.Init(HoverClass::From(o)); sa.pos[1] = 10; sa.speed = 20; sa.radius = 3;
    float d0[3] = {0, 0, 1}, d1[3] = {0, 0, 1};
    HoverAvoidArgs none; HoverAvoid(&sa, none, d0);
    CHECK(d0[0] == 0 && d0[1] == 0 && d0[2] == 1);
    HoverObstacle ob; ob.pos[0] = 0; ob.pos[1] = 10; ob.pos[2] = 60; ob.radius = 10;
    HoverAvoidArgs a1; a1.obstacles = &ob; a1.count = 1;
    sa.prev_dir[0] = sa.prev_dir[1] = sa.prev_dir[2] = 0;
    HoverAvoid(&sa, a1, d1);
    CHECK(std::fabs(d1[0]) > 0.1f);                       // seitlich ausgewichen
    { float l2 = d1[0] * d1[0] + d1[1] * d1[1] + d1[2] * d1[2]; CHECK(l2 > 0.5f && l2 < 1.1f); }   // 75 % oder voller Schub
  }
  // HoverShip: GL-Welt (z gespiegelt) - Ziel rechts/vorn in GL, Kurs und Basis muessen zum Ziel zeigen
  {
    HoverOdf g = o; g.warpSpeed = 80; g.tooCloseToTurn = 50; g.tooCloseForWarp = 10; g.headingCloseEnoughForWarp = 30;
    ShipState ss; ss.yaw = 0;                                  // Blick +z (GL)
    HoverShip hs; hs.Init(g, ss);
    CHECK(std::fabs(hs.st.front[2] + 1.0f) < 1e-5f);           // Originalkoordinaten: -z
    ss.has_target = true; ss.target[0] = 300; ss.target[1] = 0; ss.target[2] = 300;
    float bx = 0, bz = 0; float mind = 1e9f;
    for (int i = 0; i < 3000; ++i) {
      HoverShipStep(&hs, &ss, true, 0.05f, nullptr);
      float dx = ss.target[0] - ss.pos[0], dz = ss.target[2] - ss.pos[2];
      mind = std::fmin(mind, std::sqrt(dx * dx + dz * dz));
      if (i == 200) { bx = ss.b_front[0]; bz = ss.b_front[2]; }
    }
    CHECK(mind < 60.0f);                                       // Ziel in GL erreicht (nicht gespiegelt)
    CHECK(ss.has_target == false);                             // angekommen
    CHECK(ss.has_basis && std::fabs(std::sqrt(ss.b_front[0] * ss.b_front[0] + ss.b_front[1] * ss.b_front[1] + ss.b_front[2] * ss.b_front[2]) - 1.0f) < 1e-3f);
    Mat4 mm, rr; ShipMatrices(ss, 1.0f, &mm, &rr);
    // Modell-Bug (-z) muss auf b_front zeigen
    CHECK(std::fabs(-rr.m[8] - ss.b_front[0]) < 1e-4f && std::fabs(-rr.m[10] - ss.b_front[2]) < 1e-4f);
  }
  // Warp: weites Ziel voraus -> Warp (Tempo deutlich ueber Impuls); ohne warpSpeed nie
  for (int variant = 0; variant < 2; ++variant) {
    HoverOdf g = o; g.warpSpeed = variant == 0 ? 80.0f : 0.0f; g.tooCloseToTurn = 50; g.tooCloseForWarp = 10; g.headingCloseEnoughForWarp = 30;
    ShipState ss; ss.yaw = 0;
    HoverShip hs; hs.Init(g, ss);
    ss.has_target = true; ss.target[0] = 0; ss.target[1] = 0; ss.target[2] = 3000;
    bool warped = false; float vmax = 0;
    for (int i = 0; i < 2400; ++i) { HoverShipStep(&hs, &ss, true, 0.05f, nullptr); warped = warped || ss.warp; vmax = std::fmax(vmax, ss.speed); }
    if (variant == 0) CHECK(warped && vmax > 40.0f);
    else CHECK(!warped && vmax < 24.0f);
  }
  // Zielfahrt: Schiff fliegt zum Ziel und bleibt innerhalb von tooCloseToTurn stehen
  {
    HoverOdf g = o; g.warpSpeed = 80; g.tooCloseToTurn = 50; g.tooCloseForWarp = 10; g.headingCloseEnoughForWarp = 30;
    HoverClass gc = HoverClass::From(g);
    HoverCraftState sh; sh.Init(gc);
    const float tgt[3] = {200, 0, 600};
    HoverControls cc;
    float maxd = 1e9f; bool finite = true;
    for (int i = 0; i < 2400; ++i) {
      HoverGotoSteer(gc, sh, &cc, tgt, true);
      HoverSimulate(gc, &sh, cc, 0.05f);
      if (!std::isfinite(sh.pos[0] + sh.pos[1] + sh.pos[2] + sh.speed)) finite = false;
      float dx = tgt[0] - sh.pos[0], dz = tgt[2] - sh.pos[2];
      float d = std::sqrt(dx * dx + dz * dz); if (d < maxd) maxd = d;
    }
    CHECK(finite);
    CHECK(maxd < 60.0f);
    CHECK(sh.speed < 5.0f);
  }
}

static void TestNoPathStops() {
  using namespace fleet;
  HoverOdf o;
  o.impulseSpeed = 40; o.frontBackInertia = 3.0f; o.upDownInertia = 3.0f; o.leftRightInertia = 3.0f;
  ShipState ss; ss.yaw = 0;
  HoverShip hs; hs.Init(o, ss);
  PathGrid g; g.Init(-1500, -1500, 1500, 1500, 50); g.StampField(0, 0);
  ss.has_target = true; ss.target[0] = 0; ss.target[1] = 0; ss.target[2] = 0;   // Ziel mitten im gesperrten Feld
  ss.pos[0] = -600; ss.pos[2] = 0;
  HoverShipStep(&hs, &ss, true, 0.05f, nullptr, &g);
  CHECK(!ss.has_target);  // kein Weg: Befehl verworfen (Original 0x42e0c3)
  ss.has_target = true; ss.target[0] = 600; ss.target[2] = 0;                    // Ziel hinter dem Feld: Weg existiert
  HoverShipStep(&hs, &ss, true, 0.05f, nullptr, &g);
  CHECK(ss.has_target);
  ss.target[0] = 0; ss.target[2] = 0;
  HoverShipStep(&hs, &ss, true, 0.05f, nullptr, &g);
  CHECK(!ss.has_target);
}

static void TestPathGrid() {
  using namespace fleet;
  PathGrid g;
  CHECK(g.Empty());
  g.Init(-1500, -1500, 1500, 1500, 50);
  CHECK(!g.Empty());
  CHECK(!g.Blocked(0, 0));
  { PathGrid f; f.Init(-1500, -1500, 1500, 1500, 50); f.StampField(0, 0);
    CHECK(f.Blocked(0, 0)); CHECK(!f.Blocked(300, 0));
    std::vector<PathPoint> q; CHECK(f.FindPath({-600, 0}, {600, 0}, &q)); }
  {  // Unstamp 0x409a80 (Emulator-Probe): Stempel mit Standardkosten 100 -> hebt Kosten 2 auf 100 an, Kosten 198 bleiben
    auto path = [](int32_t cost, bool un) {
      PathGrid u; u.Init(-1500, -1500, 1500, 1500, 50);
      u.StampBox(-100, -300, 100, 300, 0.0f, cost);
      if (un) u.UnstampBox(-100, -300, 100, 300);
      std::vector<PathPoint> q; u.FindPath({-300, -150}, {300, 150}, &q);
      return q.size();
    };
    CHECK(path(2, false) > path(2, true));
    CHECK(path(198, true) == path(198, false));
    CHECK(path(2, true) <= 2);
  }
  g.StampSquare(0, 0, 75);
  CHECK(g.Blocked(0, 0));
  CHECK(g.Blocked(70, -70));
  CHECK(!g.Blocked(300, 0));
  CHECK(g.Blocked(-1700, 0));   // Randstreifen
  std::vector<PathPoint> p;
  // Soll-Werte aus der Emulation des Original-Pfadfinders (tools/oracle_rub.json): Weg ohne Start, mit Ziel
  struct Sc { float sx, sz, gx, gz; int n; float w[6][2]; };
  const Sc scs[] = {
    {-600, 0, 600, 0, 3, {{-200, 100}, {200, 100}, {600, 0}}},
    {-600, -300, 600, 300, 4, {{-400, 0}, {-200, 100}, {400, 200}, {600, 300}}},
    {-100, 0, 100, 0, 4, {{0, 100}, {100, 100}, {100, 100}, {100, 0}}},   // Doppelpunkt: Eigenheit des Originals
    {-600, 200, 600, 200, 1, {{600, 200}}},
    {0, -600, 0, 600, 3, {{100, -200}, {100, 200}, {0, 600}}},
    {-300, -300, 300, 300, 4, {{-100, -200}, {100, -100}, {200, 0}, {300, 300}}},   // nicht optimal, wie im Original
  };
  for (const Sc& c : scs) {
    CHECK(g.FindPath({c.sx, c.sz}, {c.gx, c.gz}, &p));
    CHECK(static_cast<int>(p.size()) == c.n);
    for (int i = 0; i < c.n && i < static_cast<int>(p.size()); ++i) CHECK(std::fabs(p[static_cast<size_t>(i)].x - c.w[i][0]) < 0.6f && std::fabs(p[static_cast<size_t>(i)].z - c.w[i][1]) < 0.6f);
  }
  // Ziel im Feld / ausserhalb der Karte: kein Weg
  CHECK(!g.FindPath({-600, 0}, {0, 0}, &p));
  CHECK(!g.FindPath({-600, 0}, {5000, 0}, &p));
  {  // zwei Felder
    PathGrid g2; g2.Init(-1500, -1500, 1500, 1500, 50);
    g2.StampSquare(-200, 0, 75); g2.StampSquare(200, 0, 75);
    CHECK(g2.FindPath({0, -500}, {0, 500}, &p));
    CHECK(p.size() == 1);   // zwischen den Feldern hindurch, gerade
  }
  // Schiff fliegt um das gesperrte Feld herum und kommt an
  {
    HoverOdf odf; odf.impulseSpeed = 20; odf.warpSpeed = 80; odf.velocCoeffStill = 0.5f; odf.velocWhenCoeffHalf = 0.7f;
    odf.pitchRotationSpeed = 0.0667f; odf.maxPitch = 45; odf.rollRotationSpeed = 0.5f; odf.maxRoll = 40; odf.turnRadius = 30;
    odf.frontBackInertia = 50; odf.maxReverseSpeed = 4; odf.upDownInertia = 7.5f; odf.leftRightInertia = 7.5f; odf.maxVerticalSpeed = 0.667f;
    odf.rotationAcceleration = 0; odf.rotationStiffness = 1;
    odf.tooCloseToTurn = 50; odf.tooCloseForWarp = 10; odf.headingCloseEnoughForWarp = 30;
    HoverShip hs;
    ShipState ss; ss.yaw = 1.5707963f;
    ss.pos[0] = -600; ss.pos[1] = 30; ss.pos[2] = 0;
    ss.target[0] = 600; ss.target[1] = 30; ss.target[2] = 0; ss.has_target = true;
    hs.Init(odf, ss);
    float minr = 1e9f;
    for (int i = 0; i < 20 * 240 && ss.has_target && i < 1000; ++i) {
      HoverShipStep(&hs, &ss, true, 0.05f, nullptr, &g);
      minr = std::min(minr, std::max(std::fabs(ss.pos[0]), std::fabs(ss.pos[2])));
    }
    CHECK(std::fabs(ss.pos[0] - 600) + std::fabs(ss.pos[2]) < 90.0f && ss.speed < 1.0f);   // am Ziel zum Stehen gekommen (Wendekreis: nicht auf den Meter)
    CHECK(minr > 60.0f);   // nie im gesperrten Quadrat (Halbkante 75, Zellen aussen)
  }
}

void TestMenuLayout() {
  for (float aspect : {1.333f, 1.777f, 2.2f, 0.5f}) {
    const fleet::MenuLayout* l = fleet::SelectMenuLayout("main", aspect);
    CHECK(l != nullptr);
    if (!l) continue;
    const fleet::MenuElement* e = fleet::FindMenuElement(*l, "quit");
    CHECK(e != nullptr);
    if (e) CHECK(e->x == 544 && e->y == 415 && e->w == 67 && e->h == 16);
    CHECK(fleet::FindMenuElement(*l, "version") != nullptr);
  }
  CHECK(fleet::SelectMenuLayout("gibtsnicht", 1.0f) == nullptr);
}

int main() {
  TestMat4();
  TestLog();
  TestByteReader();
  TestTga();
  TestZfs();
  TestSod();
  TestOdf();
  TestSpr();
  TestUnitDef();
  TestTextLayout();
  TestFireDemo();
  TestBzn();
  TestHoverSim();
  TestPathGrid();
  TestMenuLayout();
  TestSaveGame();
  TestMapScene();
  TestAudio();
  TestUi();
  TestHud();
  TestShipSim();
  TestCombat();
  TestDying();
  TestShockwave();
  TestNoPathStops();
  TestBoarding();
  TestTeamsAndDifficulty();
  TestZip();
  std::printf("%d Pruefungen, %d Fehler\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
