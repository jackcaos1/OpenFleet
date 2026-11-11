#pragma once
// Typisierte Sicht auf die ODF-Spielwerte (Schiffe, Gebaeude, Waffen, Physik). Siehe docs/formats/odf-tt.md.
// Fehlende Schluessel lassen den Standardwert stehen (die Spiel-ODFs erben Werte ueber #include, nicht jede Klasse hat jeden Wert).
// Einheiten (Sekunden, Entfernungen usw.) sind aus den Zahlen abgeleitet, nicht belegt.
#include <string>
#include <utility>
#include <vector>

#include "formats/Odf.h"

namespace fleet {

struct SubsystemDef {
  int hit_points = 0;
  float hit_percent = 0;  // Anteil der Treffer, die dieses Teilsystem treffen (Prozent)
  float repair_time = 0;
  float crew_loss = 0;
  std::vector<std::string> target_hardpoints;
};

struct WeaponSlot {
  std::string odf;                    // Waffen-ODF (z. B. fc1phas)
  std::vector<std::string> hardpoints;  // Namen von Hardpoint-Knoten im SOD (hp01 ...)
};

// Wert mit Ausnahmen je Zielart: "24 \"zmama.odf\" 2" = Standardwert 24, gegen zmama 2 (gleiches Muster bei hitChance).
struct PerTarget {
  float base = 0;
  std::vector<std::pair<std::string, float>> overrides;  // Ziel-ODF (klein) -> Wert
  float For(const std::string& target_odf) const;
};

struct WeaponDef {
  std::string odf;
  std::string name;  // wpnName
  std::string class_label;
  std::string ordnance;  // ordName: ODF des Geschosses
  std::string fire_sound;
  float shot_delay = 0;
  float range = 0;
  PerTarget hit_chance;
  float to_hit_warp = 0, to_hit_stop = 0, to_hit_slow = 0;  // Modifikatoren (toHitWarp/Stop/SlowModifier)
  bool special = false;  // Spezialwaffe mit Energiekosten (special = 1), z. B. Chain Reaction Pulsar
};

// Geschoss/Strahl (ODF aus "ordName" einer Waffe). Nicht jede Klasse nutzt jeden Wert; Spezialklassen (Minen, Traktorstrahl, ...) haben
// weitere Schluessel, die hier nicht erfasst sind.
struct OrdnanceDef {
  std::string odf;
  std::string class_label;  // phaser, pphaser, photontorpedo, plasmacannon, chainpulsar, tractor_bm, ...
  PerTarget damage;         // damageBase
  float damage_variance = 0, damage_threshold = 0;
  float shield_crew_modifier = 0, hull_crew_modifier = 0;
  float special_energy_cost = 0;
  float life_span = 0, shot_speed = 0, shot_accel = 0, omega_turn = 0, seek_time = 0;
  float radius = 0, length = 0;
  bool ignore_shield = false;
  std::string sprite, shot_geometry, hit_sound;  // Sprite-Name (Sprites/*.spr) bzw. SOD-Name des Geschossmodells
  float sprite_duration = 0;
  float light_color[3] = {0, 0, 0}, light_falloff_start = 0, light_falloff_range = 0;
  bool has_light = false;
};

struct PhysicsDef {
  std::string odf;
  float impulse_speed = 0, warp_speed = 0, yaw_rate = 0, turn_radius = 0, max_roll = 0;
  float max_pitch = 0, pitch_rate = 0, roll_rate = 0, front_back_inertia = 0, side_inertia = 0, max_reverse = 0;
  float veloc_coeff_still = 0, veloc_half = 0, heading_ok_for_warp = 0, too_close_for_warp = 0, too_close_to_turn = 0;
  float max_rotation_speed = 0;  // Borg
};

struct UnitDef {
  std::string odf;         // Dateiname ohne .odf, klein; entspricht dem SOD-Modellnamen
  std::string class_label;  // wingman = Schiff, starbase/shipyard/... = Gebaeude
  std::string unit_name, tooltip, verbose_tooltip, race;
  float build_time = 0;
  int officer_cost = 0, crew_cost = 0, dilithium_cost = 0;
  float max_health = 0, shield_rate = 0, shield_delay = 0;
  float max_special_energy = 0, special_energy_rate = 0;
  float range_scan = 0, damaged_scan = 0, attack_power = 0;
  bool is_starbase = false;
  std::string physics_file;
  SubsystemDef engines, life_support, weapons_sys, shield_generator, sensors, hull;
  float critical_percent = 0.5f, life_support_loss = 2.0f;
  std::string fireball;  // Explosions-Effekt (ODF-Name, z. B. xfirebmd)
  std::vector<WeaponSlot> weapons;
  std::vector<std::string> possible_names;
};

struct RaceDef {
  float repair_strength = 1.0f, crew_yellow = 0.4f, crew_red = 0.2f, repair_yellow = 1.5f, repair_red = 2.0f, weapon_yellow = 1.0f, weapon_red = 2.0f;
};
bool LoadRaceDef(OdfLibrary& lib, const std::string& race, RaceDef* out);

bool LoadUnitDef(OdfLibrary& lib, const std::string& name, UnitDef* out, std::string* err = nullptr);
bool LoadWeaponDef(OdfLibrary& lib, const std::string& name, WeaponDef* out, std::string* err = nullptr);
bool LoadOrdnanceDef(OdfLibrary& lib, const std::string& name, OrdnanceDef* out, std::string* err = nullptr);
bool LoadPhysicsDef(OdfLibrary& lib, const std::string& name, PhysicsDef* out, std::string* err = nullptr);

// Mehrzeilige Kurzbeschreibung (UTF-8) fuer die Anzeige im Betrachter. Fehlende Waffen-/Physik-ODFs werden ausgelassen.
std::string DescribeUnit(OdfLibrary& lib, const UnitDef& u);

}  // namespace fleet
