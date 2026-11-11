#pragma once
// Feldfolgen der Objektklassen (aus der Armada.exe gewonnen, siehe docs/formats/savegame.md).
#include <map>
#include <string>
#include <vector>

#include "formats/SaveFile.h"

namespace fleet {

struct SgField {
  std::string kind;  // bool int float str vec3 euler ptr prjid matrix
  std::string name;
  int size = 0;
  int elems = 1;  // Anzahl Container-Elemente (euler = 5)
  int index = 0;  // erstes Element relativ zum Objektanfang (nach dem 7-teiligen Kopf)
};

struct SgLayout {
  std::string cls;
  std::vector<SgField> fields;
  int elems = 0;  // Summe der Elemente (ohne den 7-teiligen Kopf)
  std::map<std::string, int> by_name;
};

constexpr int kSgObjectHeadElems = 7;

// Alle Varianten einer ODF (z.B. mafield hat 2); leer, wenn unbekannt. odf klein geschrieben.
const std::vector<SgLayout>* SgLayouts(const std::string& odf, bool mission);

// Wie SgLayouts; unbekannte ODF -> Feldfolge einer ODF mit derselben Klassenbezeichnung (ODF-classLabel, beliebige Schreibweise).
const std::vector<SgLayout>* SgLayoutsFor(const std::string& odf, const std::string& label, bool mission);

// Zugriff auf Felder eines Objekts nach Namen
const SgElem* SgField_(const SgObject& o, const SgLayout& l, const std::string& name);
SgElem* SgFieldMut(SgObject* o, const SgLayout& l, const std::string& name);

}  // namespace fleet
