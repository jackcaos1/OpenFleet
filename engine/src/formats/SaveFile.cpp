#include "formats/SaveFile.h"

#include <cstring>

#include "formats/SaveSchema.h"

namespace fleet {
namespace {

uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }

bool IsObjStart(const std::vector<SgElem>& T, size_t k) {
  if (k + 6 >= T.size()) return false;
  return T[k].type == 7 && T[k + 1].type == 4 && T[k + 1].data.size() == 4 && (T[k + 1].data[3] & 0x80) && T[k + 2].type == 9 && T[k + 2].data.size() == 12 &&
         T[k + 3].type == 4 && T[k + 3].data.size() == 4 && T[k + 4].type == 2 && T[k + 4].data.size() == 40 && T[k + 5].type == 8 && T[k + 6].type == 11 &&
         T[k + 6].data.size() == 48;
}

bool IsFooter(const std::vector<SgElem>& T, size_t k) {
  return k + 4 < T.size() && T[k].type == 2 && T[k].data.size() == 40 && T[k + 1].type == 8 && T[k + 2].type == 2 && T[k + 2].data.size() == 64 &&
         T[k + 3].type == 2 && T[k + 3].data.size() == 64 && T[k + 4].type == 0 && T[k + 4].data.size() == 64;
}

bool Fail(std::string* err, const std::string& m) {
  if (err) *err = m;
  return false;
}

std::string CStr(const SgElem& e) {
  size_t n = 0;
  while (n < e.data.size() && e.data[n]) ++n;
  return std::string(reinterpret_cast<const char*>(e.data.data()), n);
}

}  // namespace

bool ReadSgFile(const uint8_t* data, size_t size, SgFile* out, std::string* err) {
  static const char kTrue[] = "true\r\n";
  size_t m = 0;
  for (; m + 6 <= size; ++m)
    if (std::memcmp(data + m, kTrue, 6) == 0) break;
  if (m + 6 > size) return Fail(err, "Kopf ohne 'true'");
  SgFile f;
  f.head_text.assign(reinterpret_cast<const char*>(data), m + 6);
  {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : f.head_text) {
      if (c == '\n') {
        if (!cur.empty() && cur.back() == '\r') cur.pop_back();
        lines.push_back(cur);
        cur.clear();
      } else {
        cur.push_back(c);
      }
    }
    if (lines.size() < 4) return Fail(err, "Textkopf zu kurz");
    f.version = std::atoi(lines[1].c_str());
    const size_t eq = lines[2].find('=');
    std::string hex = eq == std::string::npos ? "" : lines[2].substr(eq + 1);
    auto hv = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; };
    for (size_t i = 0; i < hex.size(); ++i) {
      if (hv(hex[i]) < 0) continue;
      if (i + 1 >= hex.size()) break;
      const char c = static_cast<char>(hv(hex[i]) * 16 + hv(hex[i + 1]));
      if (c == 0) break;
      f.desc.push_back(c);
      ++i;
    }
  }
  std::vector<SgElem> T;
  for (size_t i = m + 6; i < size;) {
    if (i + 8 > size) return Fail(err, "abgeschnittener Elementkopf");
    const uint32_t sz = U32(data + i + 4);
    if (sz > size - i - 8) return Fail(err, "Element ueberragt Dateiende");
    SgElem e;
    e.type = data[i];
    std::memcpy(e.junk, data + i + 1, 3);
    e.data.assign(data + i + 8, data + i + 8 + sz);
    T.push_back(std::move(e));
    i += 8 + sz;
  }
  std::vector<size_t> starts;
  for (size_t k = 8; k < T.size(); ++k)
    if (IsObjStart(T, k)) starts.push_back(k);
  if (starts.empty()) return Fail(err, "keine Objekte");
  f.head.assign(T.begin(), T.begin() + starts[0]);
  if (f.head.size() < 3) return Fail(err, "Kopf zu kurz");
  size_t fk = T.size();
  for (size_t k = starts.back() + 7; k < T.size(); ++k)
    if (IsFooter(T, k)) {
      fk = k;
      break;
    }
  if (fk == T.size()) return Fail(err, "Fussteil nicht gefunden");
  const bool mission = !f.head.empty() && f.head[1].data.size() == 1 && f.head[1].data[0] != 0;
  for (size_t s = 0; s < starts.size(); ++s) {
    SgObject o;
    o.odf = CStr(T[starts[s]]);
    size_t end;
    if (s + 1 < starts.size()) {
      end = starts[s + 1];
    } else {  // letztes Objekt: endet vor der Teamtabelle (9 x 7 Elemente, im Spielstand dazu 5 Gesamtfelder) vor "EmptyMission"
      const size_t before = static_cast<size_t>(mission ? 63 : 68);
      if (fk < starts[s] + kSgObjectHeadElems + before) return Fail(err, "Fuss zu kurz");
      end = fk - before;
    }
    o.e.assign(T.begin() + starts[s], T.begin() + end);
    f.objects.push_back(std::move(o));
  }
  f.tail.assign(T.begin() + starts.back() + f.objects.back().e.size(), T.end());
  *out = std::move(f);
  return true;
}

std::vector<uint8_t> WriteSgFile(const SgFile& f) {
  std::vector<uint8_t> out(f.head_text.begin(), f.head_text.end());
  auto put = [&](const SgElem& e) {
    out.push_back(e.type);
    out.insert(out.end(), e.junk, e.junk + 3);
    const uint32_t n = static_cast<uint32_t>(e.data.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(n >> (8 * i)));
    out.insert(out.end(), e.data.begin(), e.data.end());
  };
  for (const SgElem& e : f.head) put(e);
  for (const SgObject& o : f.objects)
    for (const SgElem& e : o.e) put(e);
  for (const SgElem& e : f.tail) put(e);
  return out;
}

SgElem SgFloat(float v) {
  SgElem e;
  e.type = 5;
  e.data.resize(4);
  std::memcpy(e.data.data(), &v, 4);
  return e;
}
SgElem SgInt(int32_t v) {
  SgElem e;
  e.type = 4;
  e.data.resize(4);
  std::memcpy(e.data.data(), &v, 4);
  return e;
}
SgElem SgBool(bool v) {
  SgElem e;
  e.type = 1;
  e.data.assign(1, v ? 1 : 0);
  return e;
}
SgElem SgVec3(const float v[3]) {
  SgElem e;
  e.type = 9;
  e.data.resize(12);
  std::memcpy(e.data.data(), v, 12);
  return e;
}
SgElem SgStr(const std::string& s, size_t size) {
  SgElem e;
  e.type = 2;
  e.data.assign(size, 0);
  std::memcpy(e.data.data(), s.data(), s.size() < size ? s.size() : size);
  return e;
}
float SgGetFloat(const SgElem& e, size_t index) {
  float f = 0;
  if (e.data.size() >= 4 * (index + 1)) std::memcpy(&f, e.data.data() + 4 * index, 4);
  return f;
}
int32_t SgGetInt(const SgElem& e) {
  int32_t v = 0;
  if (e.data.size() >= 4) std::memcpy(&v, e.data.data(), 4);
  return v;
}
std::string SgGetStr(const SgElem& e) { return CStr(e); }

}  // namespace fleet
