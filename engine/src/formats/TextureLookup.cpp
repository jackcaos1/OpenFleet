#include "formats/Bmp.h"
#include "formats/TextureLookup.h"

namespace fleet {

bool LoadGameTexture(FileSource& src, const std::string& name, Image* out, std::string* err) {
  if (name.empty()) return false;
  if (name.find('/') != std::string::npos) {  // Menue-Grafik: voller Pfad, z. B. "bitmaps/ms/MSsingl1.bmp"
    std::vector<uint8_t> b;
    return src.ReadFile(name, &b) && DecodeBmp(b.data(), b.size(), out, err);
  }
  static const char* const kDirs[] = {"Textures/RGB/", "Textures/Index8/"};
  static const char* const kExts[] = {".tga", ""};  // Namen mit Punkt (z. B. "Mbg01.1") gibt es auch ohne ".tga"
  std::vector<uint8_t> bytes;
  for (const char* dir : kDirs) {
    for (const char* ext : kExts) {
      const std::string path = std::string(dir) + name + ext;
      if (!src.ReadFile(path, &bytes)) continue;
      std::string e;
      if (DecodeTga(bytes.data(), bytes.size(), out, &e)) return true;
      if (err) *err = path + ": " + e;
    }
  }
  return false;
}

}  // namespace fleet
