#include "game/MenuLayout.h"

namespace fleet {
namespace {

const std::vector<MenuLayout>& All() {
  static const std::vector<MenuLayout> kAll = {
      // Hauptmenue wie im Original: Steuerelemente in Armada.exe 0x546150..0x546731 (Konstruktor 0x5239a0, Argumente x, y, w, h);
      // Versionsfeld 0x5255a0 (rot, Text im Original fest "v1.2", String 0x610764).
      {"main", "original", 0.0f, 1e9f,
       {{"single", 22, 10, 136, 16},   {"multi", 488, 10, 137, 16},  {"options", 22, 452, 100, 16},
        {"replay", 439, 451, 140, 16}, {"credits", 513, 433, 84, 15}, {"quit", 544, 415, 67, 16},
        {"version", 600, 466, 40, 14}},
       "Armada.exe 0x546150..0x546731"},
  };
  return kAll;
}

}  // namespace

const MenuLayout* SelectMenuLayout(const std::string& screen, float aspect) {
  const MenuLayout* fallback = nullptr;
  const MenuLayout* best = nullptr;
  for (const MenuLayout& l : All()) {
    if (screen != l.screen) continue;
    if (std::string(l.name) == "original") fallback = &l;
    else if (aspect >= l.min_aspect && aspect < l.max_aspect) best = &l;   // spezifische Variante schlaegt die Originalanordnung
  }
  return best ? best : fallback;
}

const MenuElement* FindMenuElement(const MenuLayout& layout, const std::string& id) {
  for (const MenuElement& e : layout.elements)
    if (id == e.id) return &e;
  return nullptr;
}

}  // namespace fleet
