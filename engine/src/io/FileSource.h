#pragma once
// Lesezugriff auf die Spielverzeichnisse des Nutzers. Pfade relativ zur Spielwurzel, Trenner '/', Gross-/Kleinschreibung egal
// (das Spiel stammt von Windows, Android-Dateisysteme unterscheiden sie).
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fleet {

class FileSource {
 public:
  virtual ~FileSource() = default;
  virtual bool ReadFile(const std::string& path, std::vector<uint8_t>* out) = 0;
  virtual bool Exists(const std::string& path) = 0;
  // Namen (ohne Pfad) der Eintraege eines Ordners, in Originalschreibweise. Leer, wenn der Ordner fehlt.
  virtual std::vector<std::string> ListDir(const std::string& dir) = 0;
};

std::string ToLowerAscii(const std::string& s);

// Datei- oder Ordnerquelle ueber das normale Dateisystem (Desktop, Tests, Android mit echtem Pfad).
std::unique_ptr<FileSource> OpenDirectory(const std::string& root);

}  // namespace fleet
