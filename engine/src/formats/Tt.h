#pragma once
// TT (Tech Tree): je Zeile "<odf-datei> <anzahl> <voraussetzung>...". Siehe docs/formats/odf-tt.md.
#include <string>
#include <vector>

namespace fleet {

struct TtEntry {
  std::string odf;                        // Kleinschreibung
  std::vector<std::string> prerequisites;  // Kleinschreibung
};

// Zeilen, die nicht passen (falsche Anzahl), werden uebersprungen und in *warnings gemeldet.
std::vector<TtEntry> ParseTt(const std::string& text, std::vector<std::string>* warnings = nullptr);

}  // namespace fleet
