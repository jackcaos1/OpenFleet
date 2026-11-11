#pragma once
// TGA-Leser fuer die Spieltexturen: Typ 1 (palettiert, 8 Bit), Typ 2 (24/32 Bit), jeweils unkomprimiert.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct Image {
  int width = 0;
  int height = 0;
  bool has_alpha = false;
  std::vector<uint8_t> rgba;  // width*height*4, Zeile 0 = oben
};

// Dekodiert nach RGBA8. Bei Fehler false und *err gefuellt.
bool DecodeTga(const uint8_t* data, size_t size, Image* out, std::string* err);

}  // namespace fleet
