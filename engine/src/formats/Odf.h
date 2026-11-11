#pragma once
// ODF (Object Definition File): Textdaten fuer Schiffe, Gebaeude, Waffen usw. Grammatik siehe docs/formats/odf-tt.md.
//
//   [Abschnitt]                       optional, hoechstens einmal am Anfang
//   #include "basis.odf"              nur am Dateianfang; Basis zuerst, eigene Zuweisungen ueberschreiben sie
//   schluessel = wert wert ...        Werte: "Text", Ganzzahl, Gleitkomma (mit/ohne f, auch 1e6), Bezeichner
//                                     Fortsetzungszeilen beginnen mit " oder Ziffer/Minus/Punkt, ';' am Ende und //-Kommentare moeglich
// Schluessel sind unabhaengig von Gross-/Kleinschreibung. Mehrfachdefinitionen: die letzte gilt (Annahme).
// Zeichen bleiben als rohe CP1252-Bytes erhalten (Cp1252ToUtf8 zum Anzeigen).
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fleet {

struct OdfToken {
  enum class Kind { String, Int, Float, Ident };
  Kind kind = Kind::Ident;
  std::string text;     // String: Inhalt ohne Anfuehrungszeichen; sonst Originaltext
  double number = 0.0;  // Int/Float
};

struct OdfEntry {
  std::string key;  // Originalschreibweise
  std::vector<OdfToken> values;
  std::string origin;  // Datei, aus der der gueltige Wert stammt
};

class OdfObject {
 public:
  std::string name;     // Dateiname (klein)
  std::string section;  // Inhalt von [..], leer wenn keiner
  std::vector<std::string> includes;
  std::vector<std::string> warnings;  // nicht lesbare Stellen (werden uebersprungen)

  const std::vector<OdfEntry>& entries() const { return entries_; }
  const OdfEntry* Find(const std::string& key) const;
  bool Has(const std::string& key) const { return Find(key) != nullptr; }

  // Erster Wert des Schluessels. Rueckgabe false, wenn der Schluessel fehlt oder der Typ nicht passt (*out bleibt dann unveraendert).
  bool GetString(const std::string& key, std::string* out) const;
  bool GetFloat(const std::string& key, float* out) const;
  bool GetInt(const std::string& key, int* out) const;
  bool GetBool(const std::string& key, bool* out) const;  // true/false oder Zahl != 0
  // Alle Strings eines Schluessels (z. B. possibleCraftNames, weaponHardpoints1).
  std::vector<std::string> GetStrings(const std::string& key) const;

  // Setzt (ersetzt) einen Eintrag. Fuer Parser und Vererbung.
  void Set(OdfEntry entry);

 private:
  std::vector<OdfEntry> entries_;
  std::unordered_map<std::string, size_t> index_;  // kleiner Schluessel -> Position
};

// Parst eine einzelne Datei ohne Includes aufzuloesen (sie stehen in out->includes).
void ParseOdfText(const std::string& text, const std::string& name, OdfObject* out);

// CP1252 -> UTF-8 (fuer Anzeige).
std::string Cp1252ToUtf8(const std::string& s);

// Laedt ODFs ueber eine Lesefunktion und loest #include auf. Ergebnisse werden zwischengespeichert.
class OdfLibrary {
 public:
  using Reader = std::function<bool(const std::string& file_name, std::vector<uint8_t>* out)>;
  explicit OdfLibrary(Reader reader) : reader_(std::move(reader)) {}

  // name mit oder ohne ".odf", Schreibweise egal. nullptr bei Fehler (*err gefuellt: Datei fehlt, Include-Schleife).
  const OdfObject* Get(const std::string& name, std::string* err = nullptr);

 private:
  const OdfObject* Load(const std::string& key, std::vector<std::string>* stack, std::string* err);

  Reader reader_;
  std::unordered_map<std::string, std::unique_ptr<OdfObject>> cache_;
};

}  // namespace fleet
