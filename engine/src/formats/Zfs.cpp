#include "formats/Zfs.h"

#include "io/ByteReader.h"
#include <algorithm>

#include "io/FileSource.h"

namespace fleet {

namespace {
bool Fail(std::string* err, const std::string& msg) {
  if (err) *err = msg;
  return false;
}
}  // namespace

bool ZfsArchive::Open(std::vector<uint8_t> data, std::string* err) {
  data_ = std::move(data);
  entries_.clear();
  ByteReader r(data_.data(), data_.size());
  const uint8_t* magic = r.Bytes(4);
  if (!magic || magic[0] != 'Z' || magic[1] != 'F' || magic[2] != 'S' || magic[3] != 'F') return Fail(err, "ZFS: Kennung fehlt");
  const uint32_t version = r.U32();
  const uint32_t name_len = r.U32();
  const uint32_t per_block = r.U32();
  const uint32_t total = r.U32();
  key_ = r.U32();
  r.U32();  // 0x18, Bedeutung unbekannt
  uint32_t next = r.U32();
  if (!r.ok() || version != 1 || name_len != 16 || per_block == 0) return Fail(err, "ZFS: unbekannte Kopfdaten");
  if (total > 1000000) return Fail(err, "ZFS: unplausible Dateizahl");

  size_t pos = r.pos();
  uint32_t seen = 0;
  while (seen < total) {
    const uint32_t n = std::min(per_block, total - seen);
    r.Seek(pos);
    for (uint32_t i = 0; i < n; ++i) {
      const uint8_t* nm = r.Bytes(16);
      ZfsEntry e;
      e.offset = r.U32();
      r.U32();  // laufender Index
      e.size = r.U32();
      e.timestamp = r.U32();
      r.U32();
      if (!r.ok() || !nm) return Fail(err, "ZFS: Verzeichnis zu kurz");
      size_t len = 0;
      while (len < 16 && nm[len] != 0) ++len;
      e.name.assign(reinterpret_cast<const char*>(nm), len);
      if (static_cast<uint64_t>(e.offset) + e.size > data_.size()) return Fail(err, "ZFS: Eintrag ausserhalb der Datei");
      entries_.push_back(std::move(e));
    }
    seen += n;
    if (seen < total) {
      if (!next) return Fail(err, "ZFS: Blockkette endet zu frueh");
      r.Seek(next);
      const uint32_t following = r.U32();
      if (!r.ok()) return Fail(err, "ZFS: Blockzeiger ungueltig");
      pos = r.pos();
      next = following;
    }
  }
  return true;
}

const ZfsEntry* ZfsArchive::Find(const std::string& name) const {
  const std::string want = ToLowerAscii(name);
  for (const ZfsEntry& e : entries_) {
    if (ToLowerAscii(e.name) == want) return &e;
  }
  return nullptr;
}

std::vector<uint8_t> ZfsArchive::Read(const ZfsEntry& e) const {
  std::vector<uint8_t> out(data_.begin() + e.offset, data_.begin() + e.offset + e.size);
  if (key_ != 0) {
    const uint8_t k[4] = {static_cast<uint8_t>(key_), static_cast<uint8_t>(key_ >> 8), static_cast<uint8_t>(key_ >> 16),
                          static_cast<uint8_t>(key_ >> 24)};
    const size_t full = out.size() - out.size() % 4;
    for (size_t i = 0; i < full; ++i) out[i] ^= k[i & 3];
  }
  return out;
}

}  // namespace fleet
