#pragma once
// Einstellungen des Spiel-Einrichtungsbildschirms (Armada.exe 0x536250, docs/formats/shell-setup.md) und Startobjekte (0x442fd0, docs/formats/session-start.md).
// GL-frei und testbar.
#include <functional>
#include <string>
#include <vector>

#include "data/UnitDef.h"
#include "formats/LabelMap.h"

namespace fleet {

enum SkOpt {
  kOptGameType, kOptMaxPlayers, kOptPing, kOptLock, kOptDilithium, kOptCrew, kOptOfficers, kOptUnits, kOptTech, kOptAlliance, kOptGiveUnits, kOptMoons,
  kOptFerengi, kOptDirectors, kOptHideMinimap, kOptFixedPlacement, kOptVisibility, kOptShroudFog, kOptCount
};

struct SkOptDef {
  SkOpt id;
  float x, y, w, h;                 // Rechteck im 640x480-Bezug (Konstanten im Aufbaucode des Originals)
  bool toggle;                      // Schalter (zwei Zustaende) statt Auswahlliste
  std::vector<const char*> keys;    // label.map-Schluessel der Eintraege (Abschnitt multiplayer_setup) in der Reihenfolge des Originals
};

const std::vector<SkOptDef>& SkOptDefs();

struct SkirmishSettings {
  int v[kOptCount];
  SkirmishSettings();  // Vorgaben des Originals (Konstruktor 0x4bd200 und die Listenauswahl 0x539a71..)
};

// Anzeigetext des aktuellen Eintrags
std::string SkOptText(const LabelMap& labels, const SkirmishSettings& s, SkOpt opt);
// naechster Eintrag (Antippen)
void SkOptCycle(SkirmishSettings* s, SkOpt opt);
// Zahlenwert eines Ressourceneintrags (Startdilithium, Besatzung, Offiziere) aus label.map (sd_value_N, sc_value_N, mo_value_N)
int SkResourceValue(const LabelMap& labels, SkOpt opt, int index);

// Startobjekte (ODF-Namen) einer Rasse fuer die Einstellung "Standardeinheiten": 0 Karteneinheiten (keine), 1 minimal, 2 Standard, 3 Super Side
std::vector<std::string> SkStartUnits(const RaceDef& race, int mode);

// Spirale des Originals (0x44318b): Radiuszaehler r = 1, 1 + 1/12, ... bis 40, Winkel 2 pi r, Abstand 120 r (x: sin, z: cos). Der erste von ok akzeptierte Punkt
// gilt; wird keiner akzeptiert, gilt der Mittelpunkt.
void SkSpiralPlace(float cx, float cz, const std::function<bool(float, float)>& ok, float* x, float* z);

}  // namespace fleet
