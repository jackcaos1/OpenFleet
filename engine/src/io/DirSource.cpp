#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <unordered_map>

#include "io/FileSource.h"

namespace fleet {

namespace fs = std::filesystem;

std::string ToLowerAscii(const std::string& s) {
  std::string r = s;
  for (char& c : r) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return r;
}

namespace {

class DirSource : public FileSource {
 public:
  explicit DirSource(std::string root) : root_(std::move(root)) {}

  bool ReadFile(const std::string& path, std::vector<uint8_t>* out) override {
    std::string real;
    if (!Resolve(path, &real)) return false;
    std::FILE* f = std::fopen(real.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) {
      std::fclose(f);
      return false;
    }
    out->resize(static_cast<size_t>(n));
    const size_t got = n > 0 ? std::fread(out->data(), 1, static_cast<size_t>(n), f) : 0;
    std::fclose(f);
    return got == static_cast<size_t>(n);
  }

  bool Exists(const std::string& path) override {
    std::string real;
    return Resolve(path, &real);
  }

  std::vector<std::string> ListDir(const std::string& dir) override {
    std::string real;
    std::vector<std::string> names;
    if (!Resolve(dir, &real)) return names;
    for (const auto& kv : Listing(real)) names.push_back(kv.second);
    std::sort(names.begin(), names.end());
    return names;
  }

 private:
  using Entries = std::unordered_map<std::string, std::string>;  // klein -> Original

  const Entries& Listing(const std::string& real_dir) {
    auto it = cache_.find(real_dir);
    if (it != cache_.end()) return it->second;
    Entries e;
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(real_dir, ec)) {
      const std::string name = de.path().filename().string();
      e.emplace(ToLowerAscii(name), name);
    }
    return cache_.emplace(real_dir, std::move(e)).first->second;
  }

  // Schrittweise Aufloesung jedes Pfadteils ohne Beachtung der Schreibweise.
  bool Resolve(const std::string& path, std::string* real) {
    std::string cur = root_;
    size_t i = 0;
    while (i <= path.size()) {
      size_t j = path.find('/', i);
      if (j == std::string::npos) j = path.size();
      const std::string part = path.substr(i, j - i);
      i = j + 1;
      if (part.empty() || part == ".") continue;
      if (part == "..") return false;
      const Entries& e = Listing(cur);
      auto it = e.find(ToLowerAscii(part));
      if (it == e.end()) return false;
      cur += "/" + it->second;
    }
    *real = cur;
    return true;
  }

  std::string root_;
  std::unordered_map<std::string, Entries> cache_;
};

}  // namespace

std::unique_ptr<FileSource> OpenDirectory(const std::string& root) {
  std::string r = root;
  while (r.size() > 1 && (r.back() == '/' || r.back() == '\\')) r.pop_back();
  return std::make_unique<DirSource>(r);
}

}  // namespace fleet
