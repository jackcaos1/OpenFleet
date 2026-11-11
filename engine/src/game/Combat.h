#pragma once
// Kampfregeln ohne Darstellung, nach den Original-ODFs (craft.odf/battle.odf, Rassen-ODFs, Ordnance-ODFs):
//  - Der Schild (= maxHealth) nimmt Schaden zuerst auf. Schaden ueber damageThreshold bei vorhandenem Schild kostet Besatzung.
//  - Was durchschlaegt, trifft per Wuerfel (hitPercent) ein Teilsystem (je 10 Punkte), die Huelle (Besatzung stirbt) oder kritisch (Schiff explodiert).
//  - Zerstoerte Systeme kosten Besatzung, ohne Lebenserhaltung stirbt die Besatzung weiter, Reparatur dauert je Punkt repairTime.
//  - Ohne Besatzung ist das Schiff ein herrenloses Wrack (derelict).
// Offene Annahmen (CLAUDE.md): 1 Systempunkt je durchgeschlagenem Treffer; Objekte ohne Besatzung arbeiten weiter, bis ihre Systeme ausfallen oder ein kritischer Treffer sie zerstoert (keine Huellenpunkte).
#include <cstdint>

namespace fleet {

enum Sys { kShield = 0, kEngines = 1, kLife = 2, kSensors = 3, kWeapons = 4, kSysCount = 5 };

// Kleiner deterministischer Zufallsgenerator (xorshift32), damit Gefechte nachvollziehbar sind.
struct Rng {
  uint32_t s = 2463534242u;
  uint32_t U32() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  float F01() { return static_cast<float>(U32() >> 8) / 16777216.0f; }
};

// Regeln einer Rasse (aus fed.odf, kling.odf, ...).
struct RaceRules {
  float repair_strength = 1.0f;
  float crew_yellow = 0.4f, crew_red = 0.2f;
  float repair_yellow = 1.5f, repair_red = 2.0f;
  float weapon_yellow = 1.0f, weapon_red = 2.0f;
};

// Regeln einer Schiffsklasse (aus craft.odf/battle.odf).
struct ShipRules {
  float hit_pct[kSysCount] = {7.5f, 2.0f, 2.5f, 7.5f, 10.0f};  // Wahrscheinlichkeit je System (Schild, Antrieb, Leben, Sensoren, Waffen)
  float hull_pct = 70.0f, critical_pct = 0.5f;
  float sys_points = 10.0f;
  float repair_time[kSysCount] = {60, 5, 10, 60, 60};  // Sekunden je Punkt
  float crew_loss[kSysCount] = {2, 5, 3, 2, 2};         // Prozent der Besatzung beim Ausfall
  float life_loss = 2.0f;                               // Prozent der Besatzung je Sekunde ohne Lebenserhaltung
};

struct Health {
  float shield = 0, shield_max = 0;
  float shield_rate = 0;
  float shield_delay = 3.0f;  // gilt, nachdem der Schild auf 0 war
  float since_hit = 1e9f;
  bool shield_was_zero = false;
  float crew = 0, crew_max = 0;
  float sys[kSysCount] = {10, 10, 10, 10, 10};
  float sys_max = 10;
  float repair_acc = 0;
  float hull = 1, hull_max = 1;  // unbenutzt (Huellentreffer kosten nur Besatzung)
  bool exploded = false;         // kritischer Treffer
  ShipRules rules;
  RaceRules race;

  bool derelict() const { return crew_max > 0 && crew <= 0; }
  bool alive() const { return !exploded; }
  bool sys_ok(int i) const { return sys[i] > 0; }
  float sys_frac(int i) const { return sys_max > 0 ? sys[i] / sys_max : 0.0f; }
  float shield_frac() const { return shield_max > 0 ? shield / shield_max : 0.0f; }
  float hull_frac() const { return hull_max > 0 ? hull / hull_max : 0.0f; }
  float crew_frac() const { return crew_max > 0 ? crew / crew_max : 1.0f; }
  // 0 gruen, 1 gelb, 2 rot
  int crew_status() const { return crew_frac() <= race.crew_red ? 2 : (crew_frac() <= race.crew_yellow ? 1 : 0); }
  float weapon_delay_mult() const { const int c = crew_status(); return c == 2 ? race.weapon_red : (c == 1 ? race.weapon_yellow : 1.0f); }
  float repair_mult() const { const int c = crew_status(); return c == 2 ? race.repair_red : (c == 1 ? race.repair_yellow : 1.0f); }
  bool damaged() const;
};

void InitHealth(Health* h, float shield_max, float shield_rate, float shield_delay, float hull_max, float crew_max, const ShipRules& rules, const RaceRules& race);

struct HitInfo {
  float damage = 0, threshold = 0, shield_crew_mod = 0, hull_crew_mod = 0;
  bool ignore_shield = false;
};
// Wendet einen Treffer an. Rueckgabe: Index des ausgefallenen Systems (kSysCount = keins).
int ApplyHit(Health* h, Rng* r, const HitInfo& hit);

// Schildaufladung, Lebenserhaltung (Besatzung stirbt), Reparatur.
void StepHealth(Health* h, float dt);

// Besatzung aendern (Prozent der Maximalbesatzung bzw. absolut), nie unter 0 / ueber crew_max.
void LoseCrewPercent(Health* h, float percent);

float RollDamage(Rng* r, float base, float variance);
bool RollHit(Rng* r, float chance);

}  // namespace fleet
