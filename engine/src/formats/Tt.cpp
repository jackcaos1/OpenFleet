#include "formats/Tt.h"

#include <cstdlib>
#include <sstream>

#include "io/FileSource.h"

namespace fleet {

std::vector<TtEntry> ParseTt(const std::string& text, std::vector<std::string>* warnings) {
  std::vector<TtEntry> out;
  std::istringstream in(text);
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    ++n;
    std::istringstream ls(line);
    std::vector<std::string> t;
    for (std::string w; ls >> w;) t.push_back(w);
    if (t.empty() || t[0].compare(0, 2, "//") == 0) continue;
    char* end = nullptr;
    const long count = t.size() >= 2 ? std::strtol(t[1].c_str(), &end, 10) : -1;
    if (t.size() < 2 || *end != '\0' || count < 0 || t.size() != static_cast<size_t>(2 + count)) {
      if (warnings) warnings->push_back("Zeile " + std::to_string(n) + " nicht lesbar: " + line.substr(0, 60));
      continue;
    }
    TtEntry e;
    e.odf = ToLowerAscii(t[0]);
    for (size_t i = 2; i < t.size(); ++i) e.prerequisites.push_back(ToLowerAscii(t[i]));
    out.push_back(std::move(e));
  }
  return out;
}

}  // namespace fleet
