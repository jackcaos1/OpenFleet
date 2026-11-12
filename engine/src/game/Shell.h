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
enum class ShellScreen { Main, Single, Multi, Skirmish, SkirmishMap };

struct ShellButton {
  std::string id;  // "single", "multi", "replay", "credits", "options", "quit", "skirmish", "campaign", "back" ...
  UiRect rect;     // Trefferflaeche in Pixeln
  bool enabled = true;
};

struct ShellLayout {
  std::vector<HudDraw> draws;
  std::vector<ShellButton> buttons;
};

// Anzeigedaten des Spiel-Einrichtungsbildschirms (bitmaps/multiset/mpsBG.bmp; Rechtecke aus dem Aufbaucode des Originals, docs/formats/shell-setup.md).
struct SkirmishSlotView {
  int state = 0;  // 0 nicht vorhanden (Karte hat zu wenige Startplaetze), 1 aktiv
  std::string name, race, team;
};
struct SkirmishOptionView {
  int id = 0;  // Index der Option (game/Skirmish.h), Aktions-ID "opt:<id>"
  float x = 0, y = 0, w = 0, h = 0;
  std::string text;
  bool toggle = false;  // Schalter (tg-Grafik) statt Auswahlliste (pd-Grafik)
  bool enabled = true;
};
struct SkirmishView {
  SkirmishSlotView slot[8];
  std::vector<SkirmishOptionView> options;
  std::string title, col_name, col_side, col_team, col_game, label_messages;
  std::string map_name;                  // Kartenname unter der Vorschau
  std::string preview;                   // Textur der Kartenvorschau ("" = keine)
  std::string btn_prev, btn_launch, btn_main, btn_select_map;
  std::vector<std::string> info;         // Textzeilen im Meldungsfeld links unten
  // Kartenauswahl (ShellScreen::SkirmishMap)
  std::vector<std::string> maps;         // sichtbarer Ausschnitt (bis 13)
  int selected = -1;                     // Index im Ausschnitt
  bool can_up = false, can_down = false;
  std::string filter_text, btn_select, btn_cancel;
};

// pressed = id der gerade gedrueckten Taste (gelb). font darf nullptr sein (dann ohne Beschriftung der Platten).
ShellLayout BuildShell(ShellScreen screen, int width, int height, const std::string& pressed, const HudData* font, const SkirmishView* sk = nullptr);
// id der aktiven Taste an dieser Stelle, sonst leer.
std::string ShellHit(const ShellLayout& layout, float x, float y);

}  // namespace fleet
