#pragma once
// SOD-Modelle (Storm3D), eigene Implementierung nach docs/formats/sod.md. Unterstuetzt Versionen 1.2 und 1.4 bis 1.8.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct SodMaterial {
  std::string name;
  float ambient[3] = {0, 0, 0};
  float diffuse[3] = {0, 0, 0};
  float specular[3] = {0, 0, 0};
  float power = 0;
  uint8_t lighting = 0;
};

struct SodGroup {
  std::string material;
  // Pro Dreiecksecke ein Paar (Eckpunktindex, UV-Index); drei Paare je Dreieck.
  std::vector<uint16_t> corners;
};

struct SodMesh {
  std::string material;  // ab v1.7, sonst "default"
  std::string texture;   // Dateiname ohne Endung, leer = Farbflaeche
  std::vector<float> positions;  // xyz
  std::vector<float> uvs;        // uv
  std::vector<SodGroup> groups;
  uint8_t cull = 0;  // 0 beidseitig, 1 Rueckseiten verwerfen
};

enum SodNodeType : uint16_t { kNodeNull = 0, kNodeMesh = 1, kNodeSprite = 3, kNodeLod = 11, kNodeEmitter = 12 };

struct SodNode {
  uint16_t type = 0;
  std::string name;
  std::string parent;  // "" = Wurzel
  float right[3] = {1, 0, 0};
  float up[3] = {0, 1, 0};
  float front[3] = {0, 0, 1};
  float pos[3] = {0, 0, 0};
  std::string emitter;
  SodMesh mesh;  // nur bei type == kNodeMesh gefuellt
};

struct Sod {
  float version = 0;
  std::vector<SodMaterial> materials;
  std::vector<SodNode> nodes;
  uint32_t channel_count = 0;
  uint32_t texref_count = 0;
};

// Strenges Parsen: die Datei muss exakt bis zum letzten Byte gelesen werden.
bool ParseSod(const uint8_t* data, size_t size, Sod* out, std::string* err);

struct Mat3x4 {
  float right[3], up[3], front[3], pos[3];
};

// Weltmatrix je Knoten (gleiche Reihenfolge wie sod.nodes). Zeilenvektor: v' = v.right*x + up*y + front*z + pos.
std::vector<Mat3x4> SodWorldMatrices(const Sod& sod);

}  // namespace fleet
