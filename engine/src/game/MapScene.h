#pragma once
// Kartenszene (GL-frei): macht aus einer BznMap Platzierungen fuer die SOD-Modelle.
// Modelle liegen nach model/ModelBuilder im rechtshaendigen GL-System (z negiert); die BZN-Daten stehen im Spielsystem.
#include <functional>
#include <string>
#include <vector>

#include "core/Math.h"
#include "formats/Bzn.h"

namespace fleet {

struct MapInstance {
  int model = 0;   // Index in MapScene::models
  int team = 0;
  int object = 0;  // Index in BznMap::objects
  Mat4 transform;  // Modellraum (GL) -> Welt (GL), mit Skalierung und Versatz
  Mat4 rotation;   // nur die Drehung (fuer die Beleuchtung)
};

// Wie ein Kartenobjekt dargestellt wird (aus der ODF des Objekts abgeleitet, siehe Game.cpp).
struct MapObjectInfo {
  std::string sod;                        // SOD-Name (klein), z. B. aus baseName
  float scale = 1.0f;
  float below_grid = 0.0f;                // Versatz nach unten (distanceBelowGrid)
  // Asteroidenfeld: ohne eigenes Modell, stattdessen min..max Einzelmodelle in einer Kugel um die Objektposition
  int field_min = 0, field_max = 0;
  float field_radius = 0;
  std::vector<std::string> field_models;  // Auswahl fuer die Einzelmodelle (klein)
};

struct MapScene {
  std::vector<std::string> models;       // verschiedene SOD-Namen (klein), die mindestens eine Instanz haben
  std::vector<MapInstance> instances;
  std::vector<int> unmodeled;            // Objekte ohne darstellbares Modell (Lichter, ...): nur als Punkte
  float center[3] = {0, 0, 0};           // Mittelpunkt der Teamobjekte (sonst aller Objekte), GL-System
  float extent = 1000;                   // Radius um center, der alle Teamobjekte umfasst (mindestens 500)
};

// Matrix eines Objekts: Modell (GL, z negiert) -> Welt (GL). Die Drehung steht zeilenweise rechts/oben/vorne wie im SOD.
Mat4 BznObjectMatrix(const BznObject& o);

// resolve(objekt, &info): false = kein Modell (Punkt). Fuer Asteroidenfelder (field_radius > 0) entstehen die Einzelmodelle
// deterministisch aus dem Handle des Objekts.
using MapResolver = std::function<bool(const BznObject&, MapObjectInfo*)>;
MapScene BuildMapScene(const BznMap& map, const MapResolver& resolve);

}  // namespace fleet
