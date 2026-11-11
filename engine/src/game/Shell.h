#pragma once
// Menues vor dem Spiel (Hauptmenue, Einzelspieler) im Stil des Originals, GL-frei und testbar.
// Grafiken: bitmaps/main, bitmaps/ms (Tasten), bitmaps/single, bitmaps/sp. Tastenpositionen aus originalFiles/shell_cfg.h
// (Bezugsflaeche 640x480, Tasten bei x=460, y=267..432). Die 4:3-Flaeche wird ganz gezeigt; der Rest des Bildschirms
// bekommt denselben Hintergrund vergroessert und abgedunkelt.
#include <string>
#include <vector>

#include "game/Hud.h"
#include "game/Ui.h"

namespace fleet {

// Single = Einzelspieler (Voelkerportraets, Load Game), Multi = "Multiplayer Connection", Skirmish = "Game Setup" (Instant Action, erreichbar ueber Multiplayer)
enum class ShellScreen { Main, Single, Multi, Skirmish };

struct ShellButton {
  std::string id;  // "single", "multi", "replay", "credits", "options", "quit", "skirmish", "campaign", "back" ...
  UiRect rect;     // Trefferflaeche in Pixeln
  bool enabled = true;
};

struct ShellLayout {
  std::vector<HudDraw> draws;
  std::vector<ShellButton> buttons;
};

// Anzeigedaten des Skirmish-Einstellungsbildschirms (bitmaps/multiset/mpsBG.bmp, Felder an den Originalpositionen).
struct SkirmishSlotView {
  int state = 0;  // 0 nicht vorhanden (Karte hat zu wenige Startplaetze), 1 aktiv
  std::string name, race, team;
};
struct SkirmishView {
  SkirmishSlotView slot[8];
  std::vector<std::string> maps;  // sichtbarer Ausschnitt der Kartenliste (bis 12)
  int selected = -1;              // Index im Ausschnitt, -1 = keiner sichtbar
  bool can_up = false, can_down = false;
  std::string title;              // Zeile oben rechts
  std::string preview;            // Textur der Kartenvorschau ("" = keine)
  std::vector<std::string> info;  // Textzeilen im grossen schwarzen Feld links unten
  std::string res_label;
};

// pressed = id der gerade gedrueckten Taste (gelb). font darf nullptr sein (dann ohne Beschriftung der Platten).
ShellLayout BuildShell(ShellScreen screen, int width, int height, const std::string& pressed, const HudData* font, const SkirmishView* sk = nullptr);
// id der aktiven Taste an dieser Stelle, sonst leer.
std::string ShellHit(const ShellLayout& layout, float x, float y);

}  // namespace fleet
