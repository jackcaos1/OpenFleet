#include "formats/Odf.h"

#include <cctype>
#include <cstdlib>

#include "io/FileSource.h"

namespace fleet {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

// Zahl: -?(ziffern[.ziffern]|.ziffern)([eE][+-]?ziffern)?f?  Gibt Art zurueck (Int nur ohne Punkt/Exponent/f).
bool ParseNumber(const std::string& t, OdfToken* tok) {
  size_t i = 0;
  if (i < t.size() && (t[i] == '-' || t[i] == '+')) ++i;
  size_t digits = 0;
  bool is_float = false;
  while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i, ++digits;
  if (i < t.size() && t[i] == '.') {
    is_float = true;
    ++i;
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i, ++digits;
  }
  if (digits == 0) return false;
  if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
    size_t j = i + 1;
    if (j < t.size() && (t[j] == '-' || t[j] == '+')) ++j;
    size_t exp_digits = 0;
    while (j < t.size() && std::isdigit(static_cast<unsigned char>(t[j]))) ++j, ++exp_digits;
    if (exp_digits == 0) return false;
    is_float = true;
    i = j;
  }
  if (i < t.size() && (t[i] == 'f' || t[i] == 'F')) {
    is_float = true;
    ++i;
  }
  if (i != t.size()) return false;
  tok->kind = is_float ? OdfToken::Kind::Float : OdfToken::Kind::Int;
  tok->number = std::strtod(t.c_str(), nullptr);  // strtod ignoriert das f-Suffix nicht: Kopie ohne f
  if (t.back() == 'f' || t.back() == 'F') tok->number = std::strtod(t.substr(0, t.size() - 1).c_str(), nullptr);
  return true;
}

// Zerlegt den Wertteil in Tokens. Gibt false bei offener Zeichenkette zurueck (Rest wird trotzdem als String genommen).
bool Tokenize(const std::string& s, std::vector<OdfToken>* out) {
  bool clean = true;
  size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (std::isspace(static_cast<unsigned char>(c)) || c == ';') {
      ++i;
    } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      break;
    } else if (c == '"') {
      const size_t end = s.find('"', i + 1);
      OdfToken t;
      t.kind = OdfToken::Kind::String;
      if (end == std::string::npos) {
        t.text = s.substr(i + 1);
        clean = false;
        out->push_back(std::move(t));
        break;
      }
      t.text = s.substr(i + 1, end - i - 1);
      out->push_back(std::move(t));
      i = end + 1;
    } else {
      size_t j = i;
      while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j])) && s[j] != ';' && s[j] != '"' &&
             !(s[j] == '/' && j + 1 < s.size() && s[j + 1] == '/'))
        ++j;
      OdfToken t;
      t.text = s.substr(i, j - i);
      if (!ParseNumber(t.text, &t)) t.kind = OdfToken::Kind::Ident;
      out->push_back(std::move(t));
      i = j;
    }
  }
  return clean;
}

bool StartsContinuation(const std::string& s) {
  const char c = s[0];
  return c == '"' || std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '.';
}

}  // namespace

const OdfEntry* OdfObject::Find(const std::string& key) const {
  auto it = index_.find(ToLowerAscii(key));
  return it == index_.end() ? nullptr : &entries_[it->second];
}

void OdfObject::Set(OdfEntry entry) {
  const std::string k = ToLowerAscii(entry.key);
  auto it = index_.find(k);
  if (it == index_.end()) {
    index_.emplace(k, entries_.size());
    entries_.push_back(std::move(entry));
  } else {
    entries_[it->second] = std::move(entry);
  }
}

bool OdfObject::GetString(const std::string& key, std::string* out) const {
  const OdfEntry* e = Find(key);
  if (!e || e->values.empty() || e->values[0].kind != OdfToken::Kind::String) return false;
  *out = e->values[0].text;
  return true;
}

bool OdfObject::GetFloat(const std::string& key, float* out) const {
  const OdfEntry* e = Find(key);
  if (!e || e->values.empty()) return false;
  const OdfToken& t = e->values[0];
  if (t.kind != OdfToken::Kind::Int && t.kind != OdfToken::Kind::Float) return false;
  *out = static_cast<float>(t.number);
  return true;
}

bool OdfObject::GetInt(const std::string& key, int* out) const {
  const OdfEntry* e = Find(key);
  if (!e || e->values.empty()) return false;
  const OdfToken& t = e->values[0];
  if (t.kind != OdfToken::Kind::Int && t.kind != OdfToken::Kind::Float) return false;
  *out = static_cast<int>(t.number);
  return true;
}

bool OdfObject::GetBool(const std::string& key, bool* out) const {
  const OdfEntry* e = Find(key);
  if (!e || e->values.empty()) return false;
  const OdfToken& t = e->values[0];
  if (t.kind == OdfToken::Kind::Int || t.kind == OdfToken::Kind::Float) {
    *out = t.number != 0.0;
    return true;
  }
  const std::string l = ToLowerAscii(t.text);
  if (l == "true") {
    *out = true;
    return true;
  }
  if (l == "false") {
    *out = false;
    return true;
  }
  return false;
}

std::vector<std::string> OdfObject::GetStrings(const std::string& key) const {
  std::vector<std::string> r;
  const OdfEntry* e = Find(key);
  if (!e) return r;
  for (const OdfToken& t : e->values) {
    if (t.kind == OdfToken::Kind::String) r.push_back(t.text);
  }
  return r;
}

void ParseOdfText(const std::string& text, const std::string& name, OdfObject* out) {
  *out = OdfObject();
  out->name = ToLowerAscii(name);
  OdfEntry cur;
  bool have = false;
  auto flush = [&]() {
    if (have) {
      cur.origin = out->name;
      out->Set(std::move(cur));
    }
    cur = OdfEntry();
    have = false;
  };

  size_t pos = 0;
  int line_no = 0;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) nl = text.size();
    std::string line = Trim(text.substr(pos, nl - pos));
    pos = nl + 1;
    ++line_no;
    if (line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) continue;
    auto warn = [&](const char* what) { out->warnings.push_back("Zeile " + std::to_string(line_no) + ": " + what + ": " + line.substr(0, 60)); };

    if (line[0] == '#') {
      flush();
      if (ToLowerAscii(line.substr(0, 8)) == "#include") {
        std::vector<OdfToken> toks;
        Tokenize(line.substr(8), &toks);
        if (toks.size() >= 1 && toks[0].kind == OdfToken::Kind::String) {
          out->includes.push_back(toks[0].text);
        } else {
          warn("#include ohne Dateiname");
        }
      } else {
        warn("unbekannte Direktive");
      }
      continue;
    }
    if (line[0] == '[') {
      flush();
      const size_t close = line.find(']');
      if (close == std::string::npos) {
        warn("Abschnitt ohne ]");
      } else {
        out->section = Trim(line.substr(1, close - 1));
      }
      continue;
    }
    if (IsIdentStart(line[0])) {
      size_t k = 0;
      while (k < line.size() && IsIdentChar(line[k])) ++k;
      size_t eq = k;
      while (eq < line.size() && (line[eq] == ' ' || line[eq] == '\t')) ++eq;
      if (eq < line.size() && line[eq] == '=') {
        flush();
        cur.key = line.substr(0, k);
        have = true;
        if (!Tokenize(line.substr(eq + 1), &cur.values)) warn("offene Zeichenkette");
        continue;
      }
    }
    if (have && StartsContinuation(line)) {
      if (!Tokenize(line, &cur.values)) warn("offene Zeichenkette");
      continue;
    }
    warn("Zeile nicht lesbar");
  }
  flush();
}

std::string Cp1252ToUtf8(const std::string& s) {
  // 0x80..0x9F weichen von Latin-1 ab; nur die haeufig genutzten Zeichen werden abgebildet, Rest wie Latin-1.
  static const uint16_t kHigh[32] = {0x20AC, 0x81,   0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                                     0x2039, 0x0152, 0x8D,   0x017D, 0x8F,   0x90,   0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                                     0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D,   0x017E, 0x0178};
  std::string out;
  for (unsigned char c : s) {
    uint32_t cp = c;
    if (c >= 0x80 && c < 0xA0) cp = kHigh[c - 0x80];
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

const OdfObject* OdfLibrary::Get(const std::string& name, std::string* err) {
  std::string key = ToLowerAscii(name);
  if (key.size() < 4 || key.compare(key.size() - 4, 4, ".odf") != 0) key += ".odf";
  std::vector<std::string> stack;
  return Load(key, &stack, err);
}

const OdfObject* OdfLibrary::Load(const std::string& key, std::vector<std::string>* stack, std::string* err) {
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second.get();
  for (const std::string& s : *stack) {
    if (s == key) {
      if (err) *err = "Include-Schleife bei " + key;
      return nullptr;
    }
  }
  std::vector<uint8_t> bytes;
  if (!reader_(key, &bytes)) {
    if (err) *err = "ODF nicht gefunden: " + key;
    return nullptr;
  }
  OdfObject own;
  ParseOdfText(std::string(bytes.begin(), bytes.end()), key, &own);

  auto merged = std::make_unique<OdfObject>();
  merged->name = key;
  merged->section = own.section;
  merged->includes = own.includes;
  merged->warnings = own.warnings;
  stack->push_back(key);
  for (const std::string& inc : own.includes) {
    std::string k = ToLowerAscii(inc);
    if (k.size() < 4 || k.compare(k.size() - 4, 4, ".odf") != 0) k += ".odf";
    const OdfObject* base = Load(k, stack, err);
    if (!base) {
      stack->pop_back();
      return nullptr;
    }
    if (merged->section.empty()) merged->section = base->section;
    for (const OdfEntry& e : base->entries()) merged->Set(e);
  }
  stack->pop_back();
  for (const OdfEntry& e : own.entries()) merged->Set(e);
  return (cache_[key] = std::move(merged)).get();
}

}  // namespace fleet
