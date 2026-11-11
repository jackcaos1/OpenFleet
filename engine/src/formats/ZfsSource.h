#pragma once
// FileSource ueber ein ZFS-Archiv (flach, Name ohne Pfad; ein fuehrender Pfadanteil wird ignoriert).
#include <memory>

#include "formats/Zfs.h"
#include "io/FileSource.h"

namespace fleet {

class ZfsSource : public FileSource {
 public:
  explicit ZfsSource(std::shared_ptr<ZfsArchive> a) : a_(std::move(a)) {}
  bool ReadFile(const std::string& path, std::vector<uint8_t>* out) override {
    const ZfsEntry* e = a_->Find(Base(path));
    if (!e) return false;
    *out = a_->Read(*e);
    return true;
  }
  bool Exists(const std::string& path) override { return a_->Find(Base(path)) != nullptr; }
  std::vector<std::string> ListDir(const std::string& dir) override {
    std::vector<std::string> r;
    if (!dir.empty() && dir != "." && dir != "/") return r;
    for (const ZfsEntry& e : a_->entries()) r.push_back(e.name);
    return r;
  }

 private:
  static std::string Base(const std::string& p) {
    const size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
  }
  std::shared_ptr<ZfsArchive> a_;
};

}  // namespace fleet
