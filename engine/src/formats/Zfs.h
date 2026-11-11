#pragma once
// ZFS-Archiv (siehe docs/formats/zfs.md). Haelt die Archivdaten im Speicher, Dateien werden beim Lesen entschluesselt.
#include <cstdint>
#include <string>
#include <vector>

namespace fleet {

struct ZfsEntry {
  std::string name;
  uint32_t offset = 0;
  uint32_t size = 0;
  uint32_t timestamp = 0;
};

class ZfsArchive {
 public:
  bool Open(std::vector<uint8_t> data, std::string* err);
  const std::vector<ZfsEntry>& entries() const { return entries_; }
  uint32_t key() const { return key_; }
  // Suche ohne Beachtung der Schreibweise; nullptr wenn nicht vorhanden.
  const ZfsEntry* Find(const std::string& name) const;
  // Entschluesselter Inhalt eines Eintrags.
  std::vector<uint8_t> Read(const ZfsEntry& e) const;

 private:
  std::vector<uint8_t> data_;
  std::vector<ZfsEntry> entries_;
  uint32_t key_ = 0;
};

}  // namespace fleet
