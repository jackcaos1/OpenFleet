#include "formats/SaveGame.h"

#include <cstring>

#include "formats/SaveSchema.h"

namespace fleet {
namespace {

std::string Low(const std::string& s) {
  std::string r = s;
  for (char& c : r)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return r;
}

uint8_t TypeOf(const std::string& kind) {
  if (kind == "bool") return 1;
  if (kind == "str") return 2;
  if (kind == "int") return 4;
  if (kind == "float") return 5;
  if (kind == "prjid") return 7;
  if (kind == "ptr") return 8;
  if (kind == "vec3") return 9;
  if (kind == "matrix") return 11;
  return 0;
}

SgElem Raw(uint8_t type, size_t size) {
  SgElem e;
  e.type = type;
  e.data.assign(size, 0);
  return e;
}

// Vorgabewert eines Feldes (Nullen); ptr/str mit der angegebenen Groesse
void Default(const SgField& f, std::vector<SgElem>* out) {
  if (f.kind == "euler") {
    out->push_back(SgFloat(0));
    out->push_back(SgFloat(0));
    for (int i = 0; i < 3; ++i) out->push_back(Raw(9, 12));
    return;
  }
  size_t n = static_cast<size_t>(f.size);
  if (f.kind == "bool") n = 1;
  else if (f.kind == "int" || f.kind == "float" || f.kind == "ptr") n = 4;
  else if (f.kind == "vec3") n = 12;
  else if (f.kind == "matrix") n = 48;
  else if (f.kind == "prjid") n = 8;
  out->push_back(Raw(TypeOf(f.kind), n));
}

const SgLayout* PickLayout(const std::vector<SgLayout>* ls, const SgObject& o) {
  if (!ls) return nullptr;
  for (const SgLayout& l : *ls)
    if (static_cast<int>(o.e.size()) == kSgObjectHeadElems + l.elems) return &l;
  return ls->size() == 1 ? &(*ls)[0] : nullptr;
}

void SetFloat(SgObject* o, const SgLayout& l, const char* name, float v) {
  if (SgElem* e = SgFieldMut(o, l, name))
    if (e->type == 5 && e->data.size() == 4) std::memcpy(e->data.data(), &v, 4);
}
void SetInt(SgObject* o, const SgLayout& l, const char* name, int32_t v) {
  if (SgElem* e = SgFieldMut(o, l, name))
    if (e->type == 4 && e->data.size() == 4) std::memcpy(e->data.data(), &v, 4);
}
void SetBool(SgObject* o, const SgLayout& l, const char* name, bool v) {
  if (SgElem* e = SgFieldMut(o, l, name))
    if (e->type == 1 && e->data.size() == 1) e->data[0] = v ? 1 : 0;
}

const char* const kSysNames[5] = {"shieldGenerator", "engines", "weapons", "lifeSupport", "sensors"};

// Objekt im Spielstandmodus bauen: Werte gleichnamiger Felder aus dem Kartenobjekt, sonst Nullen
bool MakeFull(const SgObject* base, bool base_mission_layout_ok, const SaveObject& so, SgObject* out, std::string* err) {
  const std::string low = Low(so.odf);
  const auto* fl = SgLayoutsFor(low, so.cls, false);
  if (!fl || fl->empty()) {
    if (err) *err = "kein Spielstandschema fuer " + so.odf;
    return false;
  }
  const SgLayout& full = (*fl)[0];
  const SgLayout* ml = nullptr;
  if (base && base_mission_layout_ok) ml = PickLayout(SgLayouts(low, true), *base);
  // mafield: Variante nach have_belt der Karte
  const SgLayout* fl2 = &full;
  if (fl->size() > 1 && base && base->e.size() > kSgObjectHeadElems) {
    const SgElem& hb = base->e[kSgObjectHeadElems];
    fl2 = &(*fl)[hb.data.size() == 1 && hb.data[0] && fl->size() > 1 ? 1 : 0];
  }
  SgObject o;
  o.odf = so.odf;
  // Kopf: PrjID, EntityID, pos, team, label, Zeiger, Matrix
  if (base && base->e.size() >= static_cast<size_t>(kSgObjectHeadElems)) {
    o.e.assign(base->e.begin(), base->e.begin() + kSgObjectHeadElems);
  } else {
    o.e.push_back(Raw(7, 8));
    std::memcpy(o.e[0].data.data(), so.odf.data(), so.odf.size() < 8 ? so.odf.size() : 8);
    o.e.push_back(SgInt(0));
    o.e.push_back(Raw(9, 12));
    o.e.push_back(SgInt(0));
    o.e.push_back(SgStr("", 40));
    o.e.push_back(Raw(8, 4));
    o.e.push_back(Raw(11, 48));
  }
  o.e[1] = SgInt(static_cast<int32_t>(so.id | 0x80000000u));
  o.e[2] = SgVec3(so.pos);
  o.e[3] = SgInt(so.team);
  o.e[4] = SgStr(so.label, 40);
  o.e[6].data.resize(48);
  std::memcpy(o.e[6].data.data(), so.mat, 48);
  for (const SgField& f : fl2->fields) {
    std::vector<SgElem> v;
    const SgField* mf = nullptr;
    if (ml) {
      auto it = ml->by_name.find(f.name);
      if (it != ml->by_name.end() && ml->fields[it->second].kind == f.kind && ml->fields[it->second].elems == f.elems) mf = &ml->fields[it->second];
    }
    // Namen, die in der Vollvariante mehrfach vorkommen (ArgType/what/who/where/param), nur beim ersten Auftreten aus der Karte
    if (mf && base) {
      for (int i = 0; i < f.elems; ++i) v.push_back(base->e[kSgObjectHeadElems + mf->index + i]);
    } else {
      Default(f, &v);
    }
    for (SgElem& e : v) o.e.push_back(std::move(e));
  }
  // Variablelange Listen ohne Eintraege: Zaehler 0 -> die Elemente der Schleife entfallen (siehe savegame.md)
  *out = std::move(o);
  return true;
}

void ApplyState(SgObject* o, const SgLayout& l, const SaveObject& so) {
  SetFloat(o, l, "curHealth", so.shield);
  SetFloat(o, l, "maxHealth", so.shield_max);
  SetFloat(o, l, "healthRatio", so.shield_max > 0 ? so.shield / so.shield_max : 1.0f);
  SetFloat(o, l, "curCrew", so.crew);
  for (int i = 0; i < 5; ++i) {
    std::string n = kSysNames[i];
    SetInt(o, l, (n + "HitPoints").c_str(), so.sys[i]);
    SetBool(o, l, (n + "Active").c_str(), so.sys[i] > 0);
  }
}


SgElem Zero(uint8_t t, size_t n) { return Raw(t, n); }

void TeamElems(const SaveTeam& t, std::vector<SgElem>* out) {
  out->push_back(SgFloat(t.scrap));
  out->push_back(SgFloat(t.max_scrap));
  out->push_back(SgInt(t.max_pilot));
  out->push_back(SgFloat(t.crew));
  out->push_back(SgFloat(t.max_crew));
  out->push_back(SgInt(t.color));
  out->push_back(SgInt(t.map_color));
}

}  // namespace

bool BuildSave(const SgFile& base, const SaveGame& g, SgFile* out, std::string* err) {
  if (base.head.size() < 17) {
    if (err) *err = "Grundkarte ohne Kopf";
    return false;
  }
  SgFile f;
  f.version = base.version;
  // Textkopf: saveGameDesc als Hex (wie das Original), Rest wie die Karte
  {
    std::string hex;
    static const char* H = "0123456789ABCDEF";
    for (unsigned char c : g.desc) {
      hex.push_back(H[c >> 4]);
      hex.push_back(H[c & 15]);
    }
    hex += "00";
    f.head_text = "version [1] =\r\n" + std::to_string(f.version) + "\r\nsaveGameDesc = " + hex + "\r\nbinarySave [1] =\r\ntrue\r\n";
    f.desc = g.desc;
  }
  std::vector<SgElem>& h = f.head;
  h.push_back(base.head[0]);       // msn_filename
  h.push_back(SgBool(false));      // missionSave
  h.push_back(SgInt(g.campaign));
  h.push_back(SgInt(g.mission));
  h.push_back(SgInt(g.run_type));
  h.push_back(SgBool(g.single_player));
  for (int t = 0; t < kSaveTeams; ++t) {  // Spielerstatistik (0x52a570), Werte ausser Existenz/Name unbenutzt
    h.push_back(SgBool(g.teams[t].exists));
    h.push_back(SgFloat(0));
    h.push_back(SgFloat(0));
    for (int i = 0; i < 6; ++i) h.push_back(SgInt(0));
    h.push_back(SgInt(0));
    h[h.size() - 1].type = 2;  // teamColor ist ein Text von 4 Byte
    h.push_back(SgBool(false));
    h.push_back(SgBool(false));
    h.push_back(SgBool(false));
    h.push_back(SgBool(false));
    h.push_back(SgBool(g.teams[t].exists));  // human
    h.push_back(SgInt(t));
    h.push_back(SgStr("", 20));
  }
  h.push_back(base.head[2]);  // TerrainName
  for (int i = 3; i <= 8; ++i) h.push_back(base.head[i]);  // Grenzen
  h.push_back(base.head[9]);
  for (int i = 0; i < 6; ++i) h.push_back(SgFloat(0));  // 0x44ade7..: nur im Spielstand
  for (int i = 10; i <= 12; ++i) h.push_back(base.head[i]);  // Umgebungslicht
  h.push_back(base.head[13]);  // BackgroundArt
  h.push_back(SgFloat(g.start_time));
  h.push_back(base.head[14]);  // techTree
  h.push_back(g.race.empty() ? base.head[15] : SgStr(g.race, 128));  // race
  h.push_back(SgInt(static_cast<int32_t>(g.objects.size())));
  for (const SaveObject& so : g.objects) {
    const SgObject* b = so.base_index >= 0 && static_cast<size_t>(so.base_index) < base.objects.size() ? &base.objects[static_cast<size_t>(so.base_index)] : nullptr;
    SgObject o;
    if (!MakeFull(b, true, so, &o, err)) return false;
    if (so.has_state) {
      const auto* fl = SgLayoutsFor(Low(so.odf), so.cls, false);
      const SgLayout* l = PickLayout(fl, o);
      if (l) ApplyState(&o, *l, so);
    }
    f.objects.push_back(std::move(o));
  }
  // Fuss: Teamtabelle (7 Elemente je Team), danach die Gesamtfelder, dann der Rest der Karte, dann die nur im Spielstand vorhandenen Listen
  std::vector<SgElem>& t = f.tail;
  for (int i = 0; i < kSaveTeams; ++i) TeamElems(g.teams[i], &t);
  {
    SgElem rel = Zero(2, 36);
    t.push_back(rel);  // teamRelation
    SgElem a = Zero(4, 36);
    t.push_back(a);  // allyShipAttacks
    t.push_back(a);  // allyStationAttacks
    SgElem tm = Zero(5, 36);
    t.push_back(tm);  // timeOfLastAllyAttack
    t.push_back(Zero(2, 324));  // giveViewRef
  }
  size_t k = static_cast<size_t>(kSaveTeams) * 7;
  for (; k < base.tail.size(); ++k) t.push_back(base.tail[k]);
  t.push_back(SgInt(0));  // 0x4ef000 ListSize
  t.push_back(SgInt(0));  // 0x51a530 ListSize
  t.push_back(SgInt(0));  // 0x55ea70 EffectListSize
  t.push_back(SgInt(0));  // 0x447d70 TextA
  t.push_back(Zero(2, 0));  // ShroudData
  t.push_back(SgInt(0));  // SpecialFlagNextHandle
  t.push_back(SgInt(0));  // NumScripts
  t.push_back(SgInt(0));  // NumGhosts
  t.push_back(SgInt(0));  // difficultySetting
  t.push_back(SgBool(false));
  t.push_back(SgInt(0));
  t.push_back(SgStr("", 40));   // groupNum
  t.push_back(SgStr("", 320));  // groupList
  t.push_back(SgStr("", 20));   // objectivesFile
  *out = std::move(f);
  return true;
}

bool ParseSave(const SgFile& f, SaveGame* g, std::string* err, const std::function<std::string(const std::string&)>& label_of) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return false;
  };
  SaveGame r;
  r.desc = f.desc;
  const std::vector<SgElem>& h = f.head;
  // 6 Kopf, 9 Teams x 17, Terrain, 6 Grenzen, Flag, 6 Zusatz, 3 Licht, Hintergrund, Startzeit, Technikbaum, Rasse, Objektzahl
  const size_t kStart = 6 + 9 * 17 + 1 + 6 + 1 + 6 + 3 + 1;
  const size_t kHead = kStart + 4;
  if (h.size() != kHead || h[1].data.size() != 1 || h[1].data[0] != 0) return fail("kein Spielstand (Kopf)");
  r.campaign = SgGetInt(h[2]);
  r.mission = SgGetInt(h[3]);
  r.run_type = SgGetInt(h[4]);
  r.single_player = h[5].data.size() == 1 && h[5].data[0];
  for (int t = 0; t < kSaveTeams; ++t) r.teams[t].exists = h[6 + t * 17].data.size() == 1 && h[6 + t * 17].data[0];
  r.start_time = SgGetFloat(h[kStart]);
  r.race = SgGetStr(h[kStart + 2]);
  r.map_file = SgGetStr(h[0]);
  if (f.tail.size() < static_cast<size_t>(kSaveTeams) * 7) return fail("kein Spielstand (Fuss)");
  for (int i = 0; i < kSaveTeams; ++i) {
    SaveTeam& t = r.teams[i];
    const size_t b = static_cast<size_t>(i) * 7;
    t.scrap = SgGetFloat(f.tail[b]);
    t.max_scrap = SgGetFloat(f.tail[b + 1]);
    t.max_pilot = SgGetInt(f.tail[b + 2]);
    t.crew = SgGetFloat(f.tail[b + 3]);
    t.max_crew = SgGetFloat(f.tail[b + 4]);
    t.color = SgGetInt(f.tail[b + 5]);
    t.map_color = SgGetInt(f.tail[b + 6]);
  }
  for (const SgObject& o : f.objects) {
    SaveObject so;
    so.odf = o.odf;
    so.id = static_cast<uint32_t>(SgGetInt(o.e[1])) & 0x7fffffffu;
    std::memcpy(so.pos, o.e[2].data.data(), 12);
    so.team = SgGetInt(o.e[3]);
    so.label = SgGetStr(o.e[4]);
    if (o.e[6].data.size() == 48) std::memcpy(so.mat, o.e[6].data.data(), 48);
    const SgLayout* l = PickLayout(SgLayoutsFor(Low(o.odf), label_of ? label_of(o.odf) : std::string(), false), o);
    if (l) {
      auto num = [&](const char* n, float* v) {
        if (const SgElem* e = SgField_(o, *l, n))
          if (e->type == 5) *v = SgGetFloat(*e);
      };
      float v = 0;
      so.has_state = SgField_(o, *l, "curCrew") != nullptr;
      num("curHealth", &so.shield);
      num("maxHealth", &so.shield_max);
      num("curCrew", &so.crew);
      for (int i = 0; i < 5; ++i) {
        if (const SgElem* e = SgField_(o, *l, (std::string(kSysNames[i]) + "HitPoints").c_str())) so.sys[i] = SgGetInt(*e);
      }
      (void)v;
    }
    r.objects.push_back(std::move(so));
  }
  *g = std::move(r);
  return true;
}

}  // namespace fleet
