#pragma once
// BMP-Leser fuer die Menue-Grafiken (bitmaps/*.bmp): unkomprimiert, 24 Bit (BI_RGB), Zeilen von unten nach oben.
#include <cstdint>
#include <string>

#include "formats/Tga.h"

namespace fleet {

bool DecodeBmp(const uint8_t* data, size_t size, Image* out, std::string* err);

}  // namespace fleet
