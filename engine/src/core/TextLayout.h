#pragma once
// Textumbruch und Zeichenzuordnung fuer die eingebaute Schrift (core/FontData.h). Ohne GL, daher testbar.
#include <string>
#include <vector>

namespace fleet {

// Zerlegt UTF-8 in Zeilen von Atlas-Zeichennummern (Umbruch bei '\n' und bei max_chars; max_chars <= 0 = unbegrenzt).
// Unbekannte Zeichen werden zu '?', ungueltige UTF-8-Bytes als Latin-1 gelesen.
std::vector<std::vector<int>> LayoutText(const std::string& utf8, int max_chars);

}  // namespace fleet
