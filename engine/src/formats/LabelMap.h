#pragma once
// label.map: Klartext-Textdatei der Menues (docs/formats/shell-setup.md). Aufbau: `<Abschnitt> {` ... `+ <Schluessel> {Text}` ... `}`, Zeilen mit # sind Kommentar.
#include <map>
#include <string>

namespace fleet {

class LabelMap {
 public:
  // Liest den Dateiinhalt (Latin-1 oder UTF-8 wird unveraendert uebernommen). Rueckgabe: Zahl der gelesenen Texte.
  int Parse(const std::string& text);
  // Text zu Abschnitt/Schluessel, sonst fallback.
  std::string Get(const std::string& section, const std::string& key, const std::string& fallback = std::string()) const;
  bool Has(const std::string& section, const std::string& key) const { return entries_.count(section + "\n" + key) != 0; }
  size_t size() const { return entries_.size(); }

 private:
  std::map<std::string, std::string> entries_;
};

}  // namespace fleet
