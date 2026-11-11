#pragma once
// Legt Quellen uebereinander: die erste, die eine Datei hat, gewinnt (z. B. lose Dateien vor dem ZFS-Archiv).
#include <memory>
#include <vector>

#include "io/FileSource.h"

namespace fleet {

class OverlaySource : public FileSource {
 public:
  void Add(std::shared_ptr<FileSource> s) { layers_.push_back(std::move(s)); }
  bool ReadFile(const std::string& path, std::vector<uint8_t>* out) override {
    for (auto& l : layers_) if (l->ReadFile(path, out)) return true;
    return false;
  }
  bool Exists(const std::string& path) override {
    for (auto& l : layers_) if (l->Exists(path)) return true;
    return false;
  }
  std::vector<std::string> ListDir(const std::string& dir) override {
    std::vector<std::string> r;
    for (auto& l : layers_) {
      for (auto& n : l->ListDir(dir)) {
        bool dup = false;
        for (auto& x : r) if (ToLowerAscii(x) == ToLowerAscii(n)) { dup = true; break; }
        if (!dup) r.push_back(n);
      }
    }
    return r;
  }

 private:
  std::vector<std::shared_ptr<FileSource>> layers_;
};

}  // namespace fleet
