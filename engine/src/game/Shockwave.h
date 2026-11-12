#pragma once
// Schockwelle (Ring) nach grossen Explosionen, GL-frei. Nach dem Original (Klasse shockwavexplode, Armada.exe 0x51d2a0/0x51d570/0x51d0b5,
// docs/formats/ship-health.md): Der aeussere Radius waechst als 2 x rate x Zeit, der innere als rate x Zeit; das Netz hat je Strahl sechs
// Eckpunkte (Querschnitt aus drei Baendern), Maximalradius = Objektmassstab x radiusFactor, ab fadeFactor x Maximalradius wird linear ausgeblendet.
#include <string>
#include <vector>

#include "data/UnitDef.h"
#include "game/Combat.h"

namespace fleet {

class OdfLibrary;

struct ShockwaveDef {
  int rays = 10;
  float rate = 10.0f;
  float radius_factor = 4.0f;
  float fade_factor = 0.8f;
  float colour[3] = {0.5f, 0.5f, 0.5f};
  std::string texture;
  // Schaden (Original 0x51d2a0): jeder Wert kann je Zielart Ausnahmen haben; ohne damageBase richtet die Welle nichts an.
  PerTarget damage_base, damage_variance, damage_threshold, shield_crew_modifier, hull_crew_modifier;
  // Aus der fireball-ODF
  float delay = 0.0f;    // shockwaveDelay: Sekunden nach dem Fireball-Start
  float minimum = 0.0f;  // shockwaveMinimum: Objektmassstab, ab dem es die Welle gibt (nur groessere)
};

// Liest die Welle, die die fireball-ODF nennt (shockwave, shockwaveDelay, shockwaveMinimum). false = keine Welle.
bool LoadShockwaveDef(OdfLibrary& lib, const std::string& fireball_odf, ShockwaveDef* out);

struct Shockwave {
  float pos[3] = {0, 0, 0};
  float scale = 1;  // Objektmassstab (Schiffsradius)
  float age = 0;
};

float ShockwaveOuterRadius(const ShockwaveDef& d, const Shockwave& s);
float ShockwaveMaxRadius(const ShockwaveDef& d, const Shockwave& s);
bool ShockwaveDone(const ShockwaveDef& d, const Shockwave& s);
// 1 bis fadeFactor x Maximum, danach linear auf 0 am Maximum.
float ShockwaveFade(const ShockwaveDef& d, const Shockwave& s);
// Schaden an einem Ziel im Abstand distance (Mitte zu Mitte): (base + Zufall x Varianz) x (1 - Abstand / Radius) x dt, mit den Werten der
// Zielart. false = ausserhalb des Radius oder kein Schaden. Annahme: das Original fragt vorher ein Klassenfeld (+0x60 > 0,1) ab, dessen
// Bedeutung offen ist; hier genuegt damageBase > 0.
bool ShockwaveHit(const ShockwaveDef& d, const Shockwave& s, float distance, const std::string& target_odf, float rand01, float dt, HitInfo* out);

// Dreiecke (je Eckpunkt x y z u v, relativ zur Position der Welle), 6 Dreiecke je Strahl.
void BuildShockwaveMesh(const ShockwaveDef& d, const Shockwave& s, std::vector<float>* out);

}  // namespace fleet
