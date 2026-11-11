#pragma once
// Menue-Anordnungen als Daten. Jede Anordnung ("Variante") gilt fuer einen Seitenverhaeltnis-Bereich; die Originalanordnung (4:3, aus
// Armada.exe gelesen) ist die Vorgabe und gilt, wenn keine andere Variante passt. Weitere Anordnungen (Breitbild, Hochformat usw.) werden
// als zusaetzliche MenuLayout-Eintraege in MenuLayout.cpp ergaenzt, ohne Shell.cpp anzufassen.
#include <string>
#include <vector>

namespace fleet {

// Bezug der Lage: Frame = innerhalb des 640x480-Rahmens (Original); Left/Right (Top/Bottom) = an der linken/rechten (oberen/unteren) Bildschirmkante
// statt an der Rahmenkante, damit Breitbild- oder Hochformat-Varianten Elemente an den Rand ruecken koennen.
enum class MenuAnchor { Frame, Left, Right };
enum class MenuAnchorV { Frame, Top, Bottom };

struct MenuElement {
  const char* id;      // z. B. "single", "version"
  float x, y, w, h;    // Bezug 640x480 (Original)
  MenuAnchor ax = MenuAnchor::Frame;
  MenuAnchorV ay = MenuAnchorV::Frame;
};

struct MenuLayout {
  const char* screen;   // "main"
  const char* name;     // "original", "wide", ...
  float min_aspect;     // Breite/Hoehe von (inklusive)
  float max_aspect;     // bis (ausschliesslich)
  std::vector<MenuElement> elements;
  const char* source;   // Herkunft der Werte (Beleg)
};

// Passende Anordnung fuer Bildschirm und Seitenverhaeltnis; nie nullptr fuer bekannte Bildschirme (Vorgabe = "original").
const MenuLayout* SelectMenuLayout(const std::string& screen, float aspect);
// Element suchen (nullptr, wenn die Anordnung es nicht hat).
const MenuElement* FindMenuElement(const MenuLayout& layout, const std::string& id);

}  // namespace fleet
