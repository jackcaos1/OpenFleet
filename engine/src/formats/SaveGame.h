#pragma once
// Spielstand im Originalformat (docs/formats/savegame.md): Karte als Grundlage + Spielzustand. Plattformunabhaengig.
// Annahmen/Unsicherheiten stehen in savegame.md (Kopf-/Fussabschnitte ausser den Objekten sind aus der Exe abgeleitet, nicht gegen einen echten Spielstand geprueft).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "formats/SaveFile.h"

namespace fleet {

struct SaveTeam {
  bool exists = false;
  float scrap = 0, max_scrap = 999999;  // Dilithium
  int max_pilot = 0;
  float crew = 0, max_crew = 99999;
  int color = 0, map_color = 0;
};

struct SaveObject {
  std::string odf;
  std::string cls;       // ODF-classLabel (nur fuer ODFs, die in keiner Karte vorkommen: Feldfolge einer verwandten ODF)
  uint32_t id = 0;       // Handle (EntityID ohne Bit 31)
  int team = 0;
  std::string label;
  float pos[3] = {0, 0, 0};
  float mat[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // 3x3 Drehung + Position, wie in der Kartendatei
  int base_index = -1;   // Objekt der Grundkarte, dessen Werte als Vorgabe dienen (-1: neu)
  bool has_state = false;
  float shield = 0, shield_max = 0;  // GameObject curHealth/maxHealth
  float crew = 0;                    // Craft curCrew
  int sys[5] = {0, 0, 0, 0, 0};      // HitPoints: Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren
  // Vollstaendiger Zustand je System (CraftSystem, Craft::Load_Save 0x462b80): Active, HitPoints, AdjustedHitPercent, ModifiedRepairTime,
  // RepairTimeLeft, DisableTimeLeft, PermanentDisable. Gilt nur mit has_sysx, sonst werden aus sys[] Active und HitPoints abgeleitet.
  struct SysState {
    bool active = false;
    int points = 0;
    float hit_pct = 0, per_point = 0, countdown = 0, disable_timer = 0;
    bool permanent = false;
  };
  SysState sysx[5];
  bool has_sysx = false;
  float shield_delay_left = 0;  // curShieldDelay
};

struct SaveGame {
  std::string desc;           // saveGameDesc
  int campaign = 0, mission = 0, run_type = 0;
  bool single_player = true;
  float start_time = 0;
  std::string race;  // raceName im Kopf (Spielerrasse); leer = Wert der Grundkarte
  std::string map_file;  // Kartendatei (msn_filename), nur beim Lesen gefuellt
  SaveTeam teams[9];
  std::vector<SaveObject> objects;
};

constexpr int kSaveTeams = 9;

// base = Karte im Kartenmodus (ReadSgFile). Liefert false mit err, wenn ein Objekt kein Schema hat.
bool BuildSave(const SgFile& base, const SaveGame& g, SgFile* out, std::string* err);
// Liest einen Spielstand (Spielstandmodus).
// label_of: ODF-Name -> classLabel (fuer ODFs ohne eigenes Schema), darf leer sein.
bool ParseSave(const SgFile& f, SaveGame* g, std::string* err, const std::function<std::string(const std::string&)>& label_of = nullptr);

}  // namespace fleet
