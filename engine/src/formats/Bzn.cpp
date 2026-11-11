#include "formats/Bzn.h"

#include <cstdlib>
#include <cstring>

namespace fleet {
namespace {

struct Elem {
  uint8_t type;
  const uint8_t* d;
  uint32_t size;
};

uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
float F32(const uint8_t* p) {
  const uint32_t u = U32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
std::string CStr(const Elem& e) {
  size_t n = 0;
  while (n < e.size && e.d[n]) ++n;
  return std::string(reinterpret_cast<const char*>(e.d), n);
}

bool IsObjStart(const std::vector<Elem>& T, size_t k) {
  if (k + 6 >= T.size()) return false;
  return T[k].type == 7 && T[k + 1].type == 4 && T[k + 1].size == 4 && (T[k + 1].d[3] & 0x80) && T[k + 2].type == 9 && T[k + 2].size == 12 &&
         T[k + 3].type == 4 && T[k + 3].size == 4 && T[k + 4].type == 2 && T[k + 4].size == 40 && T[k + 5].type == 8 && T[k + 6].type == 11 &&
         T[k + 6].size == 48;
}

bool IsFooter(const std::vector<Elem>& T, size_t k) {
  return k + 4 < T.size() && T[k].type == 2 && T[k].size == 40 && T[k + 1].type == 8 && T[k + 2].type == 2 && T[k + 2].size == 64 &&
         T[k + 3].type == 2 && T[k + 3].size == 64 && T[k + 4].type == 0 && T[k + 4].size == 64;
}

bool Fail(std::string* err, const std::string& m) {
  if (err) *err = m;
  return false;
}

}  // namespace

bool ParseBzn(const uint8_t* data, size_t size, BznMap* out, std::string* err) {
  static const char kTrue[] = "true\r\n";
  size_t m = 0;
  for (; m + 6 <= size; ++m)
    if (std::memcmp(data + m, kTrue, 6) == 0) break;
  if (m + 6 > size) return Fail(err, "Kopf ohne 'true'");
  BznMap r;
  {  // Textkopf: Zeile 2 = Version, Zeile 3 = "saveGameDesc = <hex>"
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < m + 6; ++i) {
      if (data[i] == '\n') {
        if (!cur.empty() && cur.back() == '\r') cur.pop_back();
        lines.push_back(cur);
        cur.clear();
      } else {
        cur.push_back(static_cast<char>(data[i]));
      }
    }
    if (lines.size() < 4) return Fail(err, "Textkopf zu kurz");
    r.version = std::atoi(lines[1].c_str());
    const size_t eq = lines[2].find('=');
    std::string hex = eq == std::string::npos ? "" : lines[2].substr(eq + 1);
    auto hv = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; };
    for (size_t i = 0; i < hex.size(); ++i) {
      if (hv(hex[i]) < 0) continue;  // Leerzeichen
      if (i + 1 >= hex.size()) break;
      const char c = static_cast<char>(hv(hex[i]) * 16 + hv(hex[i + 1]));
      if (c == 0) break;
      r.description.push_back(c);
      ++i;
    }
  }
  std::vector<Elem> T;
  for (size_t i = m + 6; i < size;) {
    if (i + 8 > size) return Fail(err, "abgeschnittener Elementkopf");
    const uint32_t sz = U32(data + i + 4);
    if (sz > size - i - 8) return Fail(err, "Element ueberragt Dateiende");
    T.push_back({data[i], data + i + 8, sz});
    i += 8 + sz;
  }
  if (T.size() < 17 || T[16].type != 4 || T[16].size != 4) return Fail(err, "Kopf zu kurz");
  r.file = CStr(T[0]);
  r.name = CStr(T[2]);
  for (int i = 0; i < 6; ++i) {
    if (T[3 + i].type != 5 || T[3 + i].size != 4) return Fail(err, "Kopf: Grenzwert fehlt");
    r.bounds[i] = F32(T[3 + i].d);
  }
  for (int i = 0; i < 3; ++i)
    if (T[10 + i].type == 5 && T[10 + i].size == 4) r.ambient[i] = F32(T[10 + i].d);
  r.background = CStr(T[13]);
  r.tech_tree = CStr(T[14]);
  r.race = CStr(T[15]);
  const int count = static_cast<int>(U32(T[16].d));
  size_t last = 0;
  for (size_t k = 17; k < T.size(); ++k) {
    if (!IsObjStart(T, k)) continue;
    BznObject o;
    o.odf = CStr(T[k]);
    o.handle = U32(T[k + 1].d);
    for (int i = 0; i < 3; ++i) o.pos[i] = F32(T[k + 2].d + 4 * i);
    o.team = static_cast<int>(U32(T[k + 3].d));
    o.label = CStr(T[k + 4]);
    for (int i = 0; i < 9; ++i) o.rot[i] = F32(T[k + 6].d + 4 * i);
    for (int i = 0; i < 3; ++i)
      if (k + 9 < T.size() && T[k + 7 + i].type == 5 && T[k + 7 + i].size == 4) o.light_color[i] = F32(T[k + 7 + i].d);
    r.objects.push_back(o);
    last = k;
  }
  if (static_cast<int>(r.objects.size()) != count) return Fail(err, "Objektzahl stimmt nicht mit dem Kopf ueberein");
  size_t fk = T.size();
  for (size_t k = r.objects.empty() ? 17 : last + 7; k < T.size(); ++k)
    if (IsFooter(T, k)) {
      fk = k;
      break;
    }
  if (fk == T.size()) return Fail(err, "Fussteil nicht gefunden");
  r.mission = CStr(T[fk]);
  r.title = CStr(T[fk + 4]);
  size_t k = fk + 5;
  if (k + 1 >= T.size() || T[k + 1].type != 4) return Fail(err, "Pfadanzahl fehlt");
  const int npaths = static_cast<int>(U32(T[k + 1].d));
  k += 2;
  for (int p = 0; p < npaths; ++p) {
    if (k + 6 >= T.size() || T[k].type != 0 || T[k + 1].type != 4 || T[k + 2].type != 2 || T[k + 3].type != 4 || T[k + 4].type != 9 ||
        T[k + 5].type != 0 || T[k + 6].type != 5)
      return Fail(err, "Pfadelement unerwartet");
    const uint32_t len = U32(T[k + 1].d), n = U32(T[k + 3].d);
    if (len != T[k + 2].size || n * 12u != T[k + 4].size) return Fail(err, "Pfadlaengen passen nicht");
    BznPath path;
    path.name = CStr(T[k + 2]);
    for (uint32_t i = 0; i < n * 3; ++i) path.points.push_back(F32(T[k + 4].d + 4 * i));
    path.kind = static_cast<int>(U32(T[k + 5].d));
    path.speed = F32(T[k + 6].d);
    r.paths.push_back(std::move(path));
    k += 7;
  }
  if (k != T.size()) return Fail(err, "Daten nach den Pfaden");
  *out = std::move(r);
  return true;
}

}  // namespace fleet
