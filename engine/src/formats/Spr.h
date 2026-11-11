#pragma once
// Sprite-Beschreibungen (Sprites/*.spr): Sprite-Tabelle, Sprite-Knoten (z. B. Positionslichter) und Keyframe-Animationen.
// Format siehe docs/formats/spr.md. Mehrere Dateien werden in eine Bibliothek zusammengefuehrt (Namen ohne Beachtung der Schreibweise).
#include <string>
#include <unordered_map>
#include <vector>

namespace fleet {

struct SprSprite {
  std::string name;
  std::string file;  // Texturname (ohne Endung)
  float u = 0, v = 0, w = 0, h = 0;  // Pixel im Bezugsformat
  float ref_w = 0, ref_h = 0;         // @reference (bzw. @referenceWidth/@referenceHeight)
  std::string material;               // @tmaterial, z. B. "additive"
  std::string anim;                   // @anim=<animation> (Bildfolge/Flipbook), leer = Standbild
};

struct SprKeyframe {
  float t = 0;
  float v[3] = {0, 0, 0};
};

struct SprAnimation {
  std::string id;
  std::string field;   // draw, colour, offset, width
  float length = 0;    // Dauer eines Durchlaufs in Sekunden (die Animation wiederholt sich)
  bool linear = false; // sonst "step"
  int count = 0;       // Anzahl Schluessel/Bilder laut Kopfzeile
  std::string auto_mode;  // @auto=square|row|column: Bildfolge, Bilder ueber das Raster der Textur verteilt
  std::vector<SprKeyframe> keys;

  // Wert zur Zeit t (Schleife ueber length). Bei "step" gilt der letzte Schluessel mit Zeit <= t.
  void Eval(float t, float out[3]) const;
  // Bildfolge (offset + @auto): Zelle (Spalte, Zeile) des Bildes zur Zeit t. Ohne @auto immer (0, 0).
  // square: Raster mit ceil(sqrt(count)) Spalten, zeilenweise; row: nebeneinander; column: untereinander.
  void FrameCell(float t, int* col, int* row) const;
};

struct SprNodeDef {
  std::string name;
  std::string sprite;
  std::string animation;  // "const" = immer an, keine Animation
  float w = 1, h = 1;
  float colour[3] = {1, 1, 1};
  bool billboard = false;
  bool table = false;  // Flag "sprite_table"
};

class SprLibrary {
 public:
  // Fuegt den Inhalt einer .spr-Datei hinzu. Unlesbare Zeilen erhoehen skipped().
  void AddText(const std::string& text);

  const SprSprite* FindSprite(const std::string& name) const;
  const SprAnimation* FindAnimation(const std::string& id) const;
  const SprNodeDef* FindNode(const std::string& name) const;
  // Loest einen Knotennamen aus einem Modell auf: erst exakt, dann mit abgeschnittenen Endungen "_<Zahl>" (whitestrobe_1_2 -> whitestrobe).
  const SprNodeDef* ResolveNode(const std::string& model_node_name) const;

  size_t sprite_count() const { return sprites_.size(); }
  size_t animation_count() const { return animations_.size(); }
  size_t node_count() const { return nodes_.size(); }
  int skipped() const { return skipped_; }

 private:
  std::unordered_map<std::string, SprSprite> sprites_;
  std::unordered_map<std::string, SprAnimation> animations_;
  std::unordered_map<std::string, SprNodeDef> nodes_;
  int skipped_ = 0;
};

}  // namespace fleet
