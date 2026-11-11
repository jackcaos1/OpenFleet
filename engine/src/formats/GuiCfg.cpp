#include "formats/GuiCfg.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace fleet {

namespace {

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

}  // namespace

void GuiCfg::AddText(const std::string& text, const IncludeResolver& include, int depth) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    const size_t cm = line.find("//");
    if (cm != std::string::npos) line.erase(cm);
    line = Trim(line);
    if (line.empty()) continue;
    if (line[0] == '#') {
      if (line.compare(0, 8, "#include") == 0 && include && depth < 4) {
        const size_t q1 = line.find('"');
        const size_t q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
        if (q2 != std::string::npos) {
          std::string inc;
          if (include(line.substr(q1 + 1, q2 - q1 - 1), &inc)) AddText(inc, include, depth + 1);
        }
      }
      continue;
    }
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = Lower(Trim(line.substr(0, eq)));
    const std::string val = Trim(line.substr(eq + 1));
    if (key.empty()) continue;
    Entry e;
    if (!val.empty() && val[0] == '"') {
      const size_t q = val.find('"', 1);
      e.str = val.substr(1, q == std::string::npos ? std::string::npos : q - 1);
    } else {
      std::istringstream vs(val);
      std::string tok;
      while (vs >> tok) {
        char* end = nullptr;
        const float f = std::strtof(tok.c_str(), &end);
        if (end == tok.c_str()) break;  // kein Zahlenwert (z. B. Makro): Rest ignorieren
        e.nums.push_back(f);
      }
    }
    entries_[key] = std::move(e);
  }
}

bool GuiCfg::Has(const std::string& key) const { return entries_.count(Lower(key)) != 0; }

bool GuiCfg::Numbers(const std::string& key, int n, float* out) const {
  auto it = entries_.find(Lower(key));
  if (it == entries_.end() || static_cast<int>(it->second.nums.size()) < n) return false;
  for (int i = 0; i < n; ++i) out[i] = it->second.nums[static_cast<size_t>(i)];
  return true;
}

std::string GuiCfg::Str(const std::string& key) const {
  auto it = entries_.find(Lower(key));
  return it == entries_.end() ? std::string() : it->second.str;
}

int GuiCfg::Int(const std::string& key, int def) const {
  float v = 0;
  return Numbers(key, 1, &v) ? static_cast<int>(v) : def;
}

}  // namespace fleet
