#pragma once
// Original-Oberflaeche des Spiels (HUD) aus den Spieldateien, GL-frei und testbar:
//   misc/gui_<rasse>.cfg   Lage und Groesse der Felder (Bezugsaufloesung 640x480) und ihrer Teilstuecke
//   Sprites/gui_*.spr      Zuordnung Teilstueck -> Textur und Ausschnitt
//   Textures/.../*.tga     Grafiken (Fguia04 ... Fguif04, gb*.tga fuer Schaltflaechen-Symbole)
// Die Felder werden ohne Veraenderung der Bilder skaliert und je nach Bildformat neu angeordnet:
//   Querformat: Minikarte links, Steuerfeld rechts, Info/Geschwindigkeitsleiste/Kinosicht dazwischen, am unteren Rand.
//   Hochformat: unten gestapelt (Minikarte+Info, darunter Steuerfeld+Kinosicht, darunter die Leiste).
// Die Schaltflaechen sind mit Testfunktionen belegt (UiAction), Anzeigen (Info, Minikarte) mit Testwerten.
#include <string>
#include <vector>

#include "formats/GuiCfg.h"
#include "formats/Spr.h"
#include "game/Ui.h"
#include "io/FileSource.h"

namespace fleet {

enum class HudRace { Federation, Klingon, Romulan, Borg };
constexpr int kHudRaceCount = 4;

struct HudDot {
  float nx = 0, ny = 0;   // Lage in der Minikarte, 0..1 (oben links = 0,0)
  float rgb[3] = {1, 1, 1};
};

// Eine Taste des Steuerfelds bzw. der Geschwindigkeitsleiste. sprite = Name in gui_global.spr (z. B. "b_move").
struct HudCmd {
  UiAction action = UiAction::None;
  std::string sprite, label;
  bool active = false, enabled = true;
};

// Ausgewaehltes Objekt fuer das Schiffsfeld (Werte 0..1; -1 = Anzeige ausgegraut).
struct HudShip {
  std::string name, cls, race;
  std::string status;  // Tempo/Warp (leer = Stillstand)
  std::string wire;  // ODF-Name (klein): Drahtgitter-Sprites <wire>w1..w5 aus gui_global.spr
  int crew = 0, crew_max = 0, officers = 0;
  float shield = 1, energy = -1;
  float system[5] = {1, 1, 1, 1, 1};  // Schilde, Antrieb, Lebenserhaltung, Sensoren, Waffen
  float crew_state = 1;               // 1 gruen, 0,5 gelb, 0 rot
  float hull = 1;                     // Huelle 0..1 (faerbt das Drahtgitter)
};

struct HudState {
  std::vector<HudShip> ships;         // nicht leer: Schiffsfeld wie im Original (1 = gross, mehrere = bis zu 8 kleine); sonst info_lines
  bool commands = false;              // true: Steuerfeld/Leiste aus control/rail (leere Eintraege = freie Plaetze) statt der Testbelegung
  std::vector<HudCmd> control;        // bis 12 Plaetze (3 Spalten x 4 Zeilen, zeilenweise)
  std::vector<HudCmd> rail;           // bis 6 Plaetze
  UiState ui;                         // Zustand der Testfunktionen (Hardpoints, Feuer, Ton, Karte)
  std::vector<std::string> info_lines;  // Textzeilen im Infofeld
  bool has_view_quad = false;         // Sichtfeld der Kamera auf der Minikarte (4 Ecken, 0..1)
  float view_quad[8] = {0};
  std::vector<HudDot> dots;           // Punkte der Minikarte
  std::string race_label;             // Beschriftung der Rassen-Taste
};

// Ein gezeichnetes Rechteck in Bildschirmpixeln (Ursprung oben links). tex leer = einfarbige Flaeche (rgba).
struct HudDraw {
  std::string tex;
  float x = 0, y = 0, w = 0, h = 0;
  float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
  float rgba[4] = {1, 1, 1, 1};
  bool additive = false;
};

struct HudText {
  std::string text;
  float x = 0, y = 0;
  int scale = 1;
  float rgb[3] = {1, 1, 1};
};

struct HudButtonSlot {
  UiAction action = UiAction::None;
  UiRect rect;
  bool active = false;
  bool enabled = true;
};

struct HudLayout {
  bool portrait = false;
  float scale = 1;                 // Pixel je Bezugspixel (Hauptfelder)
  UiRect view;                     // freier Bereich fuer die 3D-Ansicht zwischen den Feldern (Pixel, y von oben); das Bild selbst fuellt alles
  std::vector<HudDraw> draws;      // in Zeichenreihenfolge
  std::vector<HudText> texts;      // nach den Grafiken zeichnen
  std::vector<HudButtonSlot> buttons;
  UiRect minimap;                  // Anzeigefeld der Minikarte (Pixel), Breite 0 = keins
  std::vector<UiRect> panels;      // belegte Flaechen (Beruehrung dort dreht nicht die Ansicht)
};

class HudData {
 public:
  // Liest Konfiguration und Sprite-Tabellen der Rasse. false, wenn die Dateien fehlen.
  bool Load(FileSource& src, HudRace race);
  bool loaded() const { return loaded_; }
  HudRace race() const { return race_; }

  const GuiCfg& cfg() const { return cfg_; }
  const SprLibrary& spr() const { return spr_; }

  // Texturausschnitt eines Sprites (normierte UV). false, wenn der Sprite fehlt.
  bool Sprite(const std::string& name, std::string* tex, float uv[4], bool* additive) const;
  // Originalschrift (Sprites/font.spr + Textures/font): Zeichen c liegt bei Glyph c-32, Zellhoehe 10 Pixel im 128er Bezug.
  bool has_font() const { return !font_uv_.empty(); }
  // UV-Ausschnitt und Vorschub (Pixel bei Zellhoehe 10) eines Zeichens; false ohne Schrift. Unbekanntes wird zu '?'.
  // ch = Unicode-Zeichen; die Schrift deckt Latin-1 ab (Umlaute, ss), alles andere wird zu '?'.
  bool Glyph(unsigned ch, float uv[4], float* advance) const;
  // Namen aller Texturen, die die geladenen Sprite-Tabellen nennen (fuer Testdatensatz und Vorladen).
  std::vector<std::string> TextureNames() const;

 private:
  bool loaded_ = false;
  HudRace race_ = HudRace::Federation;
  GuiCfg cfg_;
  SprLibrary spr_;
  std::vector<std::string> textures_;
  struct GlyphDef { float x, y, w; };
  std::vector<GlyphDef> font_uv_;
};

const char* HudRaceName(HudRace race);
const char* HudCfgFile(HudRace race);    // z. B. "gui_fed.cfg"
const char* HudSprFile(HudRace race);    // z. B. "gui_federation.spr"

// Textblock mit der Originalschrift (UTF-8, Umbruch an Leerzeichen/Zeilenenden bei max_w), optional mit Hintergrundflaeche.
// h_px = Zeilenhoehe der Glyphen. Ohne Schrift im HudData bleibt der Block leer (Aufrufer nutzt dann TextRenderer).
struct HudTextBlock {
  std::vector<HudDraw> draws;
  float w = 0, h = 0;  // belegte Flaeche (ohne Rand)
};
HudTextBlock LayoutHudText(const HudData& data, const std::string& utf8, float x, float y, float h_px, float max_w, const float rgb[3],
                           const float panel_rgba[4]);

HudLayout BuildHudLayout(const HudData& data, int width, int height, const HudState& state);
UiAction HudHit(const HudLayout& layout, float x, float y);
bool HudCovers(const HudLayout& layout, float x, float y);

}  // namespace fleet
