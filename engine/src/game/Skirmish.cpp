#include "game/Skirmish.h"

#include <cmath>
#include <cstdlib>

namespace fleet {

const std::vector<SkOptDef>& SkOptDefs() {
  static const std::vector<SkOptDef> k = {
      {kOptGameType, 524, 60, 111, 20, false, {"normal_game_type", "head_to_head_type"}},
      {kOptMaxPlayers, 524, 84, 111, 20, false, {"maximum_players_8", "maximum_players_7", "maximum_players_6", "maximum_players_5", "maximum_players_4", "maximum_players_3", "maximum_players_2"}},
      {kOptPing, 524, 108, 111, 20, false, {"ping_filter_off", "ping_filter_200", "ping_filter_400", "ping_filter_600"}},
      {kOptLock, 524, 132, 111, 20, true, {"lock_game_off", "lock_game_on"}},
      {kOptDilithium, 524, 156, 111, 20, false, {"sd_value_1", "sd_value_2", "sd_value_3", "sd_value_4", "sd_value_5"}},
      {kOptCrew, 524, 180, 111, 20, false, {"sc_value_1", "sc_value_2", "sc_value_3", "sc_value_4", "sc_value_5"}},
      {kOptOfficers, 524, 204, 111, 20, false, {"mo_value_1", "mo_value_2", "mo_value_3", "mo_value_4", "mo_value_5"}},
      {kOptUnits, 524, 228, 111, 20, false, {"default_units_map", "default_units_minimal", "default_units_standard", "default_units_super"}},
      {kOptTech, 524, 252, 111, 20, false, {"tech_level_one", "tech_level_two", "tech_level_three", "tech_level_four", "tech_level_five", "tech_level_six", "tech_level_seven"}},
      {kOptAlliance, 524, 276, 111, 20, true, {"alliances_teams_off", "alliances_teams_on"}},
      {kOptGiveUnits, 524, 300, 111, 20, true, {"give_units_on", "give_units_off"}},
      {kOptMoons, 524, 324, 111, 20, true, {"infinite_moons_on", "infinite_moons_off"}},
      {kOptFerengi, 524, 348, 111, 20, true, {"ferengi_on", "ferengi_off"}},
      {kOptDirectors, 524, 372, 111, 20, true, {"directors_on", "directors_off"}},
      {kOptHideMinimap, 385, 295, 128, 20, true, {"hide_minimap_on", "hide_minimap_off"}},
      {kOptFixedPlacement, 385, 318, 128, 20, true, {"fixed_placement_on", "fixed_placement_off"}},
      {kOptVisibility, 385, 341, 128, 20, false, {"show_all", "show_available", "show_allies", "show_none"}},
      {kOptShroudFog, 385, 364, 128, 20, false, {"shroud_fog_on_on", "shroud_fog_off_on", "shroud_fog_on_off", "shroud_fog_off_off"}},
  };
  return k;
}

SkirmishSettings::SkirmishSettings() {
  for (int& x : v) x = 0;
  // Konstruktor 0x4bd200: Standardeinheiten 1 (minimal), Tech-Stufe 0; Dilithium / Besatzung stehen im Original auf Werten ausserhalb der Liste (3500 / 4000) und
  // fallen beim Gastgeber auf den ersten Eintrag; Offiziere 100 = erster Eintrag. Die uebrigen Vorgaben folgen den Byte-Feldern +0x4c..+0x57 und den Zahlen
  // +0x68..+0x84 (Zuordnung zu den Schaltern aus dem Aufbaucode). Schalterzustand 0 zeigt den zuerst registrierten Text, Zustand 1 den zweiten (0x525330). Feste Platzierung: Zustand 0
  // (0x539dbb). "Maximale Spieler" wird mit der Karte gesetzt (Index 8 - Spielerzahl, 0x539e44); Sichtbarkeit: Vorgabe +0x74 = 3.
  v[kOptUnits] = 1;
  v[kOptFerengi] = 1;
  v[kOptHideMinimap] = 1;  // +0x53 = 1 -> Zustand 1 = zweiter Text (hide_minimap_off, "Minimap Hidden"; Zustand 0 = erster registrierter Text, 0x525330)
  v[kOptGameType] = -1;  // Vorgabe +0x78 = 2 (ladder) ist nicht in der Liste: das Feld bleibt leer (0x539e01)
  v[kOptVisibility] = 3;
}

std::string SkOptText(const LabelMap& labels, const SkirmishSettings& s, SkOpt opt) {
  const SkOptDef& d = SkOptDefs()[static_cast<size_t>(opt)];
  const int i = s.v[opt];
  if (i < 0 || i >= static_cast<int>(d.keys.size())) return std::string();
  return labels.Get("multiplayer_setup", d.keys[static_cast<size_t>(i)], d.keys[static_cast<size_t>(i)]);
}

void SkOptCycle(SkirmishSettings* s, SkOpt opt) {
  const SkOptDef& d = SkOptDefs()[static_cast<size_t>(opt)];
  s->v[opt] = (s->v[opt] + 1) % static_cast<int>(d.keys.size());
}

int SkResourceValue(const LabelMap& labels, SkOpt opt, int index) {
  const SkOptDef& d = SkOptDefs()[static_cast<size_t>(opt)];
  if (index < 0 || index >= static_cast<int>(d.keys.size())) return 0;
  return std::atoi(labels.Get("multiplayer_setup", d.keys[static_cast<size_t>(index)], "0").c_str());
}

std::vector<std::string> SkStartUnits(const RaceDef& race, int mode) {
  switch (mode) {
    case 1: return race.minimal_units;
    case 2: return race.standard_units;
    case 3: return race.super_units;
    default: return {};
  }
}

void SkSpiralPlace(float cx, float cz, const std::function<bool(float, float)>& ok, float* x, float* z) {
  float r = 1.0f;
  while (r <= 40.0f) {
    const float a = 2.0f * r * 3.14159274f;
    const float px = cx + std::sin(a) * r * 120.0f, pz = cz + std::cos(a) * r * 120.0f;
    r += 0.0833333358f;
    if (ok(px, pz)) { *x = px; *z = pz; return; }
  }
  *x = cx;
  *z = cz;
}

}  // namespace fleet
