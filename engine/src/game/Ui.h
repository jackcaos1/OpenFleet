#pragma once
// Bedienleiste (GL-frei, testbar): Schaltflaechen unten am Bildschirm, Hoch- und Querformat.
// Hochformat: 4 Spalten, mehrere Zeilen; Querformat: 6 Spalten. Das 3D-Bild belegt den Platz ueber der Leiste.
#include <string>
#include <vector>

namespace fleet {

enum class UiAction {
  None,
  PrevModel, NextModel, Variant, Info, Hardpoints, Fire, Map, Mute, Folder, Team, Alpha,  // Modellansicht
  PrevMap, NextMap, MapInfo, ModelView,                                                   // Kartenansicht (Mute/Folder gelten auch hier)
  ZoomIn, ZoomOut, ResetView, Race,                                                       // Ansicht und HUD-Rasse
  Menu, Move, Attack, Orders, Back, AlertGreen, AlertYellow, AlertRed, Repair, Guard, Patrol, Scout, SearchDestroy,  // Original-Befehle
  Special0, Special1, Special2, Special3, Special4, Special5,                              // Spezialwaffen der Geschwindigkeitsleiste
  Spawn, MapMenu, Stop, Delete, Transport, StandDown, SelfDestruct,
  Harvest, Recrew, Rally,  // Frachter abbauen (cmd 0x14), Besatzung auffuellen (cmd 7), Sammelpunkt (cmd 0x15)
  Queue0, Queue1, Queue2, Queue3, Queue4, Queue5, BuildMenu, Build0, Build1, Build2, Build3, Build4, Build5, Build6, Build7, Build8, Build9, CancelBuild,  // Bauen (docs/formats/economy.md)
  Focus0, Focus1, Focus2, Focus3, Focus4, Focus5, Focus6, Focus7,  // Tipp auf das Drahtgittermodell im Schiffsfeld: Kamera zum i-ten ausgewaehlten Schiff                                                            // Objekte setzen, Karte waehlen, anhalten, entfernen
};

struct UiRect {
  float x = 0, y = 0, w = 0, h = 0;
  bool Contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct UiButton {
  UiAction action = UiAction::None;
  std::string label;
  UiRect rect;
  bool active = false;  // z. B. Hardpoints/Feuer eingeschaltet
};

struct UiState {
  bool map_mode = false;
  bool hardpoints = false;
  bool firing = false;
  bool muted = false;
};

struct UiLayout {
  std::vector<UiButton> buttons;
  float bar_y = 0;       // obere Kante der Leiste in Pixeln
  float bar_height = 0;
  int text_scale = 1;    // ganzzahliger Schriftfaktor fuer die Beschriftungen
};

UiLayout BuildUiLayout(int width, int height, const UiState& state);
// Schaltflaeche an dieser Stelle; None ausserhalb der Leiste oder zwischen den Tasten.
UiAction UiHit(const UiLayout& layout, float x, float y);
// Schriftfaktor fuer Meldungen: mindestens ~40 Zeichen pro Zeile, hoechstens passend zur Hoehe.
int UiMessageScale(int width, int height);

}  // namespace fleet
