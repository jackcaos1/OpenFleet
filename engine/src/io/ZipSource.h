#pragma once
// Spielverzeichnis aus einer ZIP-Datei (Methoden "gespeichert" und Deflate; ZIP64 wird gelesen, Verschluesselung nicht).
// Die ZIP bleibt ausserhalb des Speichers: der Aufrufer liefert nur eine Leseroutine (z. B. pread auf einen Dateideskriptor),
// einzelne Dateien werden beim Zugriff entpackt. Liegen alle Dateien in einem gemeinsamen Oberordner (z. B. "Armada/SOD/..."),
// der kein bekannter Spielordner ist, wird er weggelassen.
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "io/FileSource.h"

namespace fleet {

class ZipSource : public FileSource {
 public:
  using ReadAt = std::function<bool(uint64_t offset, void* buffer, size_t size)>;
  // nullptr bei Fehlern (err wird gefuellt).
  static std::shared_ptr<ZipSource> Open(ReadAt read, uint64_t size, std::string* err);

  bool ReadFile(const std::string& path, std::vector<uint8_t>* out) override;
  bool Exists(const std::string& path) override;
  std::vector<std::string> ListDir(const std::string& dir) override;
  size_t file_count() const { return files_.size(); }

 private:
  struct Entry {
    uint64_t local_offset = 0, comp_size = 0, size = 0;
    int method = 0;
  };
  ReadAt read_;
  std::map<std::string, Entry> files_;                            // kleingeschriebener Pfad -> Eintrag
  std::map<std::string, std::map<std::string, std::string>> dirs_;  // kleingeschriebener Ordner -> (klein -> Originalname)
};

}  // namespace fleet
