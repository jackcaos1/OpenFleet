#pragma once
// Findet Texturen der Spieldaten: Textures/RGB zuerst, dann Textures/Index8; Name ohne Endung, Schreibweise egal.
#include <string>

#include "formats/Tga.h"
#include "io/FileSource.h"

namespace fleet {

// Gibt false zurueck, wenn nichts gefunden oder nicht lesbar ist (*err dann ggf. gefuellt).
bool LoadGameTexture(FileSource& src, const std::string& name, Image* out, std::string* err);

}  // namespace fleet
