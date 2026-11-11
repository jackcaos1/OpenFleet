#pragma once
// BZN: Karten und Missionen (originalFiles/bzn/*.bzn). Format siehe docs/formats/bzn.md.
// Gelesen werden Kopf, Objektliste (ODF-Name, Handle, Position, Team, Name, Drehung) und Wegpunkt-Pfade.
// Klassenspezifische Zusatzdaten der Objekte (Gesundheit, Untersysteme, ...) werden uebersprungen.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct BznObject {
  std::string odf;     // ODF-Name ohne Endung (gleich dem SOD-Namen)
  std::string label;   // Instanzname, z. B. "zferscav3"
  uint32_t handle = 0;
  float pos[3] = {0, 0, 0};
  int team = 0;        // 0 = neutral, 1..8 = Spieler-/Teamnummer
  float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // Zeilen: rechts, oben, vorne (wie die Knoten im SOD)
  float light_color[3] = {1, 1, 1};  // nur dlight: lightColorRed/Green/Blue (die drei Floats nach der Matrix)
};

struct BznPath {
  std::string name;
  std::vector<float> points;  // x,y,z je Wegpunkt
  int kind = 0;
  float speed = 0;
};

struct BznMap {
  int version = 0;
  std::string description;  // saveGameDesc
  std::string file, name, background, tech_tree, race;
  float bounds[6] = {0, 0, 0, 0, 0, 0};  // x min/max, y min/max, z min/max (vermutet)
  float ambient[3] = {0, 0, 0};  // Kopf-Token 10..12 (AmbientRed/Green/Blue, vermutet: Wertebereich 0..0.4 passt)
  std::string mission, title;
  std::vector<BznObject> objects;
  std::vector<BznPath> paths;
};

bool ParseBzn(const uint8_t* data, size_t size, BznMap* out, std::string* err);

}  // namespace fleet
