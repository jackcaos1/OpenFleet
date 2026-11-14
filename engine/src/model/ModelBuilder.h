#pragma once
// Macht aus einem Sod ein zeichenfertiges CPU-Modell: Eckpunkte in Modellraum (Weltmatrizen angewendet), Normalen, Indizes.
// Koordinaten sind bereits ins rechtshaendige GL-System gedreht (z negiert), die Windungsrichtung ist so gewaehlt,
// dass die Vorderseiten gegen den Uhrzeigersinn liegen.
#include <cstdint>
#include <string>
#include <vector>

#include "formats/Sod.h"

namespace fleet {

struct ModelBatch {
  std::string node;
  std::string texture;      // leer = nur Materialfarbe
  std::string material;     // Gruppenmaterial
  float color[3] = {1, 1, 1};  // Diffusfarbe des Materials
  float ambient[3] = {0, 0, 0};  // Ambientfarbe des Materials
  float specular[3] = {0, 0, 0};
  float power = 0;          // Glanzexponent (Armada.exe 0x58dd90: Schlick-Naeherung t/(p-p*t+t))
  int lighting = 1;         // Lichtmodell des Materials: 0 Diffus fest, 1 Ambient+Diffus, 2 zusaetzlich Glanzlicht (Tabelle [renderer+0x80..0x88])
  bool cull_back = false;
  bool additive = false;    // additiv gemischte Effektflaechen (Strahlenkranz "oreglow" der Bergbaustation: Textur wbeams ohne Alpha, schwarz = durchsichtig)
  bool ore_ring = false;    // Nachfahre des Knotens oreglow: dreht sich waehrend des Entladens um dessen Ursprung (Animationskanal des Knotens, eine Drehung je 2,7 s)
  bool unlit = false;       // Lichtmodell 0 im Material (glow_*, warp_*, impulse_*): ohne Beleuchtung zeichnen
  bool team = false;        // Material team_*: Textur wird mit der Spielerfarbe eingefaerbt (nicht team_Bglow, das ist Borg-Gruen)
  bool cutout = false;      // Borg-Ueberzug: Alpha der Textur schneidet das Gitter aus (Alpha 0 = durchsichtig)
  std::vector<float> vertices;     // je Eckpunkt: x y z nx ny nz u v
  std::vector<uint32_t> indices;   // Dreiecke
};

// Sprite-Knoten (Positionslichter, Leuchtflaechen): Name wie im Modell, Position im Modellraum (z negiert).
struct ModelSpriteNode {
  std::string name;
  float pos[3] = {0, 0, 0};
  float right[3] = {1, 0, 0};  // Achsen des Knotens (z negiert); bei nicht-billboard Flaechen liegt das Rechteck in right/up
  float up[3] = {0, 1, 0};
};

// Hardpoint (Null-Knoten hpNN): Waffen- und Zielpunkte. dir = Knoten-Achse "front" (Feuer-/Blickrichtung, z negiert).
struct ModelHardpoint {
  std::string name;  // klein, z. B. "hp01"
  float pos[3] = {0, 0, 0};
  float dir[3] = {0, 0, 1};
};

struct Model {
  std::string name;
  std::vector<ModelBatch> batches;
  float bounds_min[3] = {0, 0, 0};
  float bounds_max[3] = {0, 0, 0};
  std::vector<ModelSpriteNode> sprite_nodes;
  std::vector<ModelHardpoint> hardpoints;
  float ore_pivot[3] = {0, 0, 0};   // Ursprung des Knotens oreglow (GL)
  size_t triangle_count = 0;
  bool winding_flipped = false;
};

// Welche Teile eines Modells gezeichnet werden. Annahmen aus der Datenanalyse (Engine-Verhalten nicht belegt, siehe
// docs/formats/sod.md): Knoten "lodN" sind Detailstufen (hoehere Zahl = mehr Dreiecke), "oq" sind die Offiziersquartiere
// (Erweiterungen der Basis), "borg*" die Borg-Uebernahmevariante, "damage*" Schadenszustaende.
struct BuildOptions {
  bool hide_helper_nodes = true;  // false = alles zeichnen (Vergleich mit der Python-Referenz)
  int lod = -1;                   // -1 = hoechste Stufe, sonst genau diese Stufe
  bool show_oq = false;
  bool show_borg = false;
  bool show_damage = false;
  bool show_ore = false;  // Bergbaustation: Knoten unter "oreglow" (Strahlenkranz) nur sichtbar, solange ein Frachter angedockt ist (0x45a7d0)
};

struct ModelVariant {
  std::string name;
  BuildOptions options;
};

// Sinnvolle Ansichten dieses Modells; Eintrag 0 ist die Standardansicht (hoechste Detailstufe).
std::vector<ModelVariant> ListModelVariants(const Sod& sod);

void BuildModel(const Sod& sod, const std::string& name, Model* out, BuildOptions options = BuildOptions());

}  // namespace fleet
