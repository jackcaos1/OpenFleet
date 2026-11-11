#pragma once
// Bruecke zwischen dem Spiel (ShipState, GL-Welt) und der Original-Flugphysik (HoverSim, Koordinaten der Originaldateien: z = -z_GL).
#include <string>
#include <vector>
#include "formats/Odf.h"
#include "game/HoverSim.h"
#include "game/PathGrid.h"
#include "game/ShipSim.h"

namespace fleet {

// Liest alle Physik-Werte aus einer Physik-ODF (z. B. battphys); fehlende Schluessel behalten die Standardwerte von HoverOdf.
bool LoadHoverOdf(OdfLibrary& lib, const std::string& name, HoverOdf* out, std::string* err = nullptr);

bool HoverOdfFromObject(const OdfObject& obj, HoverOdf* out);

struct HoverShip {
  bool ok = false;
  HoverClass cls{};
  HoverCraftState st;
  float last_gl[3] = {0, 0, 0};
  // Wegpunkte um gesperrte Rasterfelder (Originalsystem x,z); path_goal = Ziel, fuer das sie berechnet wurden
  std::vector<PathPoint> path;
  size_t path_idx = 0;
  float path_goal[3] = {1e30f, 0, 0};
  void Init(const HoverOdf& odf, const ShipState& s);   // Lage/Kurs aus dem ShipState, Geschwindigkeit 0
};

// Hindernis aus GL-Weltwerten (Position, Geschwindigkeit, Radius); mobile = Einheit (Flag 8).
HoverObstacle MakeHoverObstacle(const float pos_gl[3], const float vel_gl[3], float radius, bool mobile);

// Ein Schritt. Mit Ziel (ss->has_target) fliegt das Schiff per Originalsteuerung dorthin (inkl. Hindernisvermeidung),
// ohne Ziel oder ohne Antrieb wirkt kein Schub. Schreibt Lage, Kurs, Tempo und die Ausrichtung (Basis) in ss zurueck.
void HoverShipStep(HoverShip* h, ShipState* ss, bool can_fly, float dt, const std::vector<HoverObstacle>* obstacles, const PathGrid* grid = nullptr);

}  // namespace fleet
