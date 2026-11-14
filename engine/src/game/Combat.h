#pragma once
// Gesundheits- und Schadensmodell ohne Darstellung, nach Armada.exe (Craft::TakeDamage 0x465100, Trefferwurf 0x464dd0,
// Systemschaden 0x467b70, Reparatur 0x4683d0, Wiedereinschalten 0x467d40, Verteilung der Trefferanteile 0x467620/0x4676d0,
// Besatzungsbehandlung 0x466ca0/0x466d80, Schild 0x466b00). Jede Regel ist in docs/formats/ship-health.md belegt;
// was dort als UNBEKANNT steht, ist hier nicht nachgebaut.
#include <cstdint>

namespace fleet {

enum Sys { kShield = 0, kEngines = 1, kLife = 2, kSensors = 3, kWeapons = 4, kSysCount = 5 };

// Ergebnis des Trefferwurfs: 0..4 = Teilsystem (Sys), kHitHull = Rumpf, kHitCritical = kritischer Treffer
constexpr int kHitHull = 5;
constexpr int kHitCritical = 6;
// Reihenfolge, in der der Wurf die Anteile abzieht (0x464dd0): Antrieb, Lebenserhaltung, Waffen, Schild, Sensoren, dann Rumpf.
constexpr int kRollOrder[kSysCount] = {kEngines, kLife, kWeapons, kShield, kSensors};
// Reihenfolge der Bloecke im Objekt (Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren): in dieser Reihenfolge laufen die Schleifen des Originals
// (Wiedereinschalten, Reparatur); sie ist bei mehreren fertigen Systemen in einem Bild fuer die Trefferanteile wichtig.
constexpr int kBlockOrder[kSysCount] = {kShield, kEngines, kWeapons, kLife, kSensors};

// Zufallsgenerator des Originals (0x5014d0: s = (2273 s + 31) mod 0x19981595, Wurf = s mod n) fuer die Wuerfel; daneben xorshift
// fuer alles, was nicht nachgebildet werden muss.
struct Rng {
  uint32_t s = 2463534242u;
  uint32_t lcg = 12345u;  // Startwert des Originals wird zur Laufzeit gesetzt, hier fest fuer Wiederholbarkeit
  uint32_t U32() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  float F01() { return static_cast<float>(U32() >> 8) / 16777216.0f; }
  int Roll(int n) {
    lcg = (2273u * lcg + 31u) % 0x19981595u;
    return static_cast<int>(lcg % static_cast<uint32_t>(n));
  }
};

// Werte aus RTS_CFG.h (Vorgaben der Exe, wenn die Datei fehlt: 0x5fbc00..0x5fbc08)
struct CombatConfig {
  bool use_system_hitpoints = true;       // USE_SYSTEM_HITPOINTS; aus: kein Schaden (0x465119)
  int system_hitpoints_bonus = 200;       // SYSTEM_HITPOINTS_BONUS, wird zu jedem vorhandenen System addiert (0x460f9c)
  float shield_damage_protection = 0.8f;  // SHIELD_DAMAGE_PROTECTION: Anteil des Schadens, den der Schild aufnimmt
  // Schwierigkeit (0x465173): Modus 1 = leicht, 3 = schwer, sonst normal; der Faktor gilt fuer Schaden an den Schiffen des Spielers (EASY_DAMAGE 0.5,
  // HARD_DAMAGE 2.0 in RTS_CFG.h). Der Client kennt noch keine Auswahl, Vorgabe ist normal (Faktor 1).
  float easy_damage = 0.5f, hard_damage = 2.0f;
};
// Faktor fuer den Schaden an Schiffen des Spielers: mode 1 -> easy_damage, 3 -> hard_damage, sonst 1.
float DifficultyDamageScale(int mode);
CombatConfig& CombatCfg();

struct SystemSpec {  // je Teilsystem aus der Einheiten-ODF (Vorgaben der Schablone 0x460ef0: 1 Punkt, Reparaturzeit 1)
  int points = 1;
  float hit_pct = 0;
  float crew_loss = 0;
  float repair_time = 1.0f;  // Zeit fuer alle Punkte; je Punkt = repair_time / points
};

struct HealthSpec {
  float shield_max = 0, shield_rate = 0, shield_delay = 3.0f;  // maxHealth, shieldRate, shieldDelay
  float crew_max = 0;                                          // crewCost
  float life_support_loss = 2.0f;                              // lifeSupportLoss
  float hull_pct = 0;                                          // hullHitPercent
  float critical_pct = 0;                                      // criticalHitPercent (Sammelbecken fuer Anteile, siehe Health::crit_pct)
  SystemSpec sys[kSysCount];
};

struct RaceRules {
  float repair_strength = 1.0f;               // repairStrength: Faktor auf die Reparaturzeit je Punkt (0x468060)
  float crew_yellow = 0.4f, crew_red = 0.2f;  // crewYellowStatus / crewRedStatus
  float repair_yellow = 1.5f, repair_red = 2.0f;
  float weapon_yellow = 1.0f, weapon_red = 2.0f;
  float boarding_strength = 1.0f;             // boardingStrength: Staerke als Entermannschaft und gegen sie
};

// Ein Teilsystem (Block von 0x38 Byte im Original)
struct SystemBlock {
  bool active = false;
  int max_points = 0, points = 0;
  float hit_pct_orig = 0, hit_pct = 0;  // Trefferanteil laut ODF / aktuell (Anteile ausgefallener Systeme werden verteilt)
  float crew_loss = 0;                  // Prozent der Besatzung beim Ausfall
  float repair_per_point = 0;           // repairTime / maxPoints
  float per_point = 0;                  // aktuelle Zeit je Punkt (Rassen- und Besatzungsfaktor)
  float countdown = 0;                  // Zeit bis zum naechsten reparierten Punkt
  float disable_timer = 0;              // zeitweiser Ausfall (Spezialwaffen), zaehlt nur bei ausgeschaltetem System
  bool script_disabled = false;
};

struct Health {
  float shield = 0, shield_max = 0, shield_rate = 0, shield_delay = 3.0f;
  float delay_left = 0;  // Aufladeverzoegerung (nach einem Treffer, der den Schild auf 0 bringt)
  float crew = 0, crew_max = 0;
  float life_support_loss = 2.0f;
  float hull_pct = 0;
  // Kritischer Anteil laut ODF und aktuell: wird nicht gewuerfelt (der Wurf nimmt den Rest), sondern sammelt die Anteile, wenn kein System
  // mehr lebt, um sie dem ersten wieder reparierten System zurueckzugeben (0x4676b6, 0x467775).
  float crit_pct_orig = 0, crit_pct = 0;
  SystemBlock sys[kSysCount];
  float carry = 0;  // Bruchteil der Systempunkte, der zum naechsten Treffer uebertragen wird (0x465100)
  bool exploded = false;
  bool derelict = false;
  bool self_destruct = false;  // Selbstzerstoerungs-Countdown laeuft: ohne Besatzung explodiert das Schiff statt Derelict zu werden
  RaceRules race;

  bool alive() const { return !exploded; }
  bool sys_ok(int i) const { return sys[i].active; }
  float sys_frac(int i) const { return sys[i].max_points > 0 ? static_cast<float>(sys[i].points) / static_cast<float>(sys[i].max_points) : 0.0f; }
  float shield_frac() const { return shield_max > 0 ? shield / shield_max : 0.0f; }
  float crew_frac() const { return crew_max > 0 ? crew / crew_max : 1.0f; }
  // 0 gruen, 1 gelb, 2 rot
  int crew_status() const { return crew_frac() <= race.crew_red ? 2 : (crew_frac() <= race.crew_yellow ? 1 : 0); }
  float weapon_delay_mult() const { const int c = crew_status(); return c == 2 ? race.weapon_red : (c == 1 ? race.weapon_yellow : 1.0f); }
  bool damaged() const;
};

void InitHealth(Health* h, const HealthSpec& spec, const RaceRules& race);

struct HitInfo {
  float damage = 0, threshold = 0, shield_crew_mod = 0, hull_crew_mod = 0;
  bool ignore_shield = false;
  float damage_scale = 1.0f;  // Schwierigkeitsfaktor fuer die Schiffe des Spielers im Einzelspiel (EASY_DAMAGE/HARD_DAMAGE), sonst 1
};
// Trefferwurf: ganzzahlig 0..99, Anteile werden in der Reihenfolge kRollOrder, dann Rumpf abgezogen; der Rest ist kritisch (0x464dd0).
int RollHitSystem(const Health& h, Rng* r);
int HitSystemForRoll(const Health& h, int roll);  // der Wurf selbst, roll = 0..99
// Wendet einen Treffer an (0x465100): erst der Wurf, dann der Schaden. Rueckgabe: Index des dabei ausgefallenen Systems (kSysCount = keins).
int ApplyHit(Health* h, Rng* r, const HitInfo& hit);
// Der Schaden mit schon gewuerfeltem Ziel (idx wie von RollHitSystem), wie das Original ihn mit dem Paketfeld "Ziel" aufruft.
int ApplyHitTo(Health* h, int idx, const HitInfo& hit);

// Ein Bild: Wiedereinschalten, Reparatur aller Systeme (gleichzeitig), Lebenserhaltung, Schildaufladung (0x465e80). Derelict und zerstoerte
// Schiffe tun nichts.
void StepHealth(Health* h, float dt);
// 0x4685c0 (docked repair): the shield recharge and the system repairs once more with the scaled step (dt x 15, 0x5ca8a8); no life support, no re-enabling.
void StepDockedRepair(Health* h, float dt);

// Besatzung setzen und die Folgen ziehen (0x466ca0): unter 1,0 mit Maximalbesatzung > 0 wird das Schiff Derelict (alle Systeme aus, Schild 0)
// oder explodiert bei laufendem Selbstzerstoerungs-Countdown; ab 1,0 gilt es wieder als bemannt und die Systeme werden wieder eingeschaltet.
void SetCrew(Health* h, float crew);
// 0x467e90 (called every frame by the regeneration weapon): every system that is not script-disabled gets all its points back at once, its timers cleared;
// the health step switches a repaired system on again.
void RepairAllSystems(Health* h);

float RollDamage(Rng* r, float base, float variance);
bool RollHit(Rng* r, float chance);

// Sterbephase eines zerstoerten Schiffs (Original: Craft-Update 0x465cc0, Handler 0x466090/0x466320, docs/formats/ship-health.md).
// Das Schiff fliegt weiter, solange der Zaehler laeuft; alle kDyingFxStep Sekunden entsteht ein kleiner Explosionseffekt, am Ende die
// grosse Explosion und das Entfernen. Dauer im Original: 5 s bei Geschwindigkeit ueber 1,0 oder fuer Helden, sonst 3 s.
constexpr float kDyingFxStep = 0.05f;
constexpr int kDyingFxKinds = 7;  // systemexpl1..7
struct Dying {
  float left = 3.0f;
  float acc = 0;
};
Dying MakeDying(float speed, bool hero);
// Skalierung eines kleinen Explosionseffekts: max(0,14 x Objektradius, 4,0) als halbe Kantenlaenge in Welteinheiten.
float DyingFxSize(float radius);
// Rueckgabe: Anzahl der in diesem Schritt zu erzeugenden Effekte; *done wird true, sobald der Zaehler abgelaufen ist.
int StepDying(Dying* d, float dt, bool* done);

}  // namespace fleet
