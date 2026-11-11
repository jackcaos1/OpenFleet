#pragma once
// Spielstand-/Karten-Container im Originalformat (siehe docs/formats/savegame.md): Textkopf (version, saveGameDesc, binarySave), danach ein Binaerstrom
// aus Elementen (Typ, 3 Fuellbytes, Groesse u32, Daten). Die Fuellbytes (im Original unbelegter Speicher) bleiben beim Lesen erhalten, damit sich
// Dateien byteweise rekonstruieren lassen; neue Elemente bekommen 0xCD.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct SgElem {
  uint8_t type = 0;
  uint8_t junk[3] = {0xCD, 0xCD, 0xCD};
  std::vector<uint8_t> data;
};

struct SgObject {
  std::string odf;
  std::vector<SgElem> e;  // alle Elemente des Objekts (Kopf: PrjID, Handle, Position, Team, Label, Zeiger, Matrix, dann klassenabhaengig)
};

struct SgFile {
  int version = 2051;
  std::string head_text;        // roher Textkopf inkl. Zeilenenden (wird 1:1 zurueckgeschrieben)
  std::string desc;             // saveGameDesc
  std::vector<SgElem> head;     // Elemente 0..16 (Dateiname, missionSave, Terrain, Grenzen, ..., Objektanzahl)
  std::vector<SgObject> objects;
  std::vector<SgElem> tail;     // Fuss: alles nach dem letzten Objekt
};

bool ReadSgFile(const uint8_t* data, size_t size, SgFile* out, std::string* err);
std::vector<uint8_t> WriteSgFile(const SgFile& f);

// Element-Helfer
SgElem SgFloat(float v);
SgElem SgInt(int32_t v);
SgElem SgBool(bool v);
SgElem SgVec3(const float v[3]);
SgElem SgStr(const std::string& s, size_t size);  // nullgefuellt, feste Laenge
float SgGetFloat(const SgElem& e, size_t index = 0);
int32_t SgGetInt(const SgElem& e);
std::string SgGetStr(const SgElem& e);

}  // namespace fleet
