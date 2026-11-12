#include "formats/LabelMap.h"

#include <cctype>

namespace fleet {

namespace {
std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
}  // namespace

int LabelMap::Parse(const std::string& text) {
  entries_.clear();
  std::string section;
  int n = 0;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t e = text.find('\n', pos);
    if (e == std::string::npos) e = text.size();
    const std::string line = Trim(text.substr(pos, e - pos));
    pos = e + 1;
    if (line.empty() || line[0] == '#') continue;
    if (line == "}") { section.clear(); continue; }
    if (line.back() == '{' && line[0] != '+') {  // "<Abschnitt> {"
      section = Trim(line.substr(0, line.size() - 1));
      continue;
    }
    if (line[0] == '+' && !section.empty()) {  // "+ <Schluessel> {Text}"
      const size_t ob = line.find('{');
      const size_t cb = line.rfind('}');
      if (ob == std::string::npos || cb == std::string::npos || cb < ob) continue;
      const std::string key = Trim(line.substr(1, ob - 1));
      if (key.empty()) continue;
      entries_[section + "\n" + key] = line.substr(ob + 1, cb - ob - 1);
      ++n;
    }
  }
  return n;
}

std::string LabelMap::Get(const std::string& section, const std::string& key, const std::string& fallback) const {
  const auto it = entries_.find(section + "\n" + key);
  return it == entries_.end() ? fallback : it->second;
}

}  // namespace fleet
