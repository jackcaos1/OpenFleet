#include "io/ZipSource.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "io/Inflate.h"

namespace fleet {

namespace {

uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint64_t U64(const uint8_t* p) { return static_cast<uint64_t>(U32(p)) | (static_cast<uint64_t>(U32(p + 4)) << 32); }

std::string NormDir(const std::string& s) {
  std::string r;
  for (char c : s) r += c == '\\' ? '/' : c;
  while (!r.empty() && r.front() == '/') r.erase(0, 1);
  while (!r.empty() && r.back() == '/') r.pop_back();
  return ToLowerAscii(r);
}

}  // namespace

std::shared_ptr<ZipSource> ZipSource::Open(ReadAt read, uint64_t size, std::string* err) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return std::shared_ptr<ZipSource>();
  };
  if (size < 22) return fail("zu klein");
  const size_t tail_n = static_cast<size_t>(std::min<uint64_t>(size, 22 + 65535));
  std::vector<uint8_t> tail(tail_n);
  if (!read(size - tail_n, tail.data(), tail_n)) return fail("nicht lesbar");
  int eocd = -1;
  for (int i = static_cast<int>(tail_n) - 22; i >= 0; --i)
    if (U32(&tail[static_cast<size_t>(i)]) == 0x06054b50u) {
      eocd = i;
      break;
    }
  if (eocd < 0) return fail("keine ZIP-Datei");
  const uint8_t* e = &tail[static_cast<size_t>(eocd)];
  uint64_t total = U16(e + 10), cd_size = U32(e + 12), cd_off = U32(e + 16);
  if (total == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu) {  // ZIP64
    const uint64_t eocd_abs = size - tail_n + static_cast<uint64_t>(eocd);
    uint8_t loc[20];
    if (eocd_abs < 20 || !read(eocd_abs - 20, loc, 20) || U32(loc) != 0x07064b50u) return fail("ZIP64 unlesbar");
    uint8_t z[56];
    if (!read(U64(loc + 8), z, 56) || U32(z) != 0x06064b50u) return fail("ZIP64 unlesbar");
    total = U64(z + 32);
    cd_size = U64(z + 40);
    cd_off = U64(z + 48);
  }
  if (cd_off + cd_size > size || cd_size > (1ull << 28)) return fail("Verzeichnis ungueltig");
  std::vector<uint8_t> cd(static_cast<size_t>(cd_size));
  if (!read(cd_off, cd.data(), cd.size())) return fail("Verzeichnis nicht lesbar");

  std::shared_ptr<ZipSource> z(new ZipSource());
  z->read_ = read;
  struct Item { std::string name; Entry e; };
  std::vector<Item> items;
  size_t p = 0;
  while (p + 46 <= cd.size() && U32(&cd[p]) == 0x02014b50u) {
    const uint8_t* h = &cd[p];
    const uint16_t flags = U16(h + 8), nlen = U16(h + 28), elen = U16(h + 30), clen = U16(h + 32);
    if (p + 46 + nlen + elen + clen > cd.size()) break;
    Item it;
    it.e.method = U16(h + 10);
    it.e.comp_size = U32(h + 20);
    it.e.size = U32(h + 24);
    it.e.local_offset = U32(h + 42);
    it.name.assign(reinterpret_cast<const char*>(h + 46), nlen);
    // ZIP64-Zusatzfeld: ersetzt der Reihe nach alle Felder, die 0xFFFFFFFF tragen
    const uint8_t* x = h + 46 + nlen;
    for (size_t q = 0; q + 4 <= elen;) {
      const uint16_t id = U16(x + q), sz = U16(x + q + 2);
      if (q + 4 + sz > elen) break;
      if (id == 1) {
        size_t o = q + 4;
        if (it.e.size == 0xFFFFFFFFu && o + 8 <= q + 4 + sz) { it.e.size = U64(x + o); o += 8; }
        if (it.e.comp_size == 0xFFFFFFFFu && o + 8 <= q + 4 + sz) { it.e.comp_size = U64(x + o); o += 8; }
        if (it.e.local_offset == 0xFFFFFFFFu && o + 8 <= q + 4 + sz) { it.e.local_offset = U64(x + o); o += 8; }
      }
      q += 4 + sz;
    }
    p += 46 + nlen + elen + clen;
    for (char& c : it.name) if (c == '\\') c = '/';
    while (it.name.size() > 2 && it.name[0] == '.' && it.name[1] == '/') it.name.erase(0, 2);
    if (it.name.empty() || it.name.back() == '/' || (flags & 1)) continue;  // Ordnereintrag oder verschluesselt
    items.push_back(std::move(it));
  }
  if (items.empty()) return fail("leer");

  // gemeinsamen Oberordner weglassen, wenn er kein Spielordner ist
  size_t strip = 0;
  {
    static const std::set<std::string> kKnown = {"sod", "textures", "sprites", "bzn", "misc", "sounds", "wav", "animations", "3d_paths", "ai",
                                                 "missions", "cursors", "bitmaps", "bmp", "help", "addon", "scrnthms", "giddi", "assets.zfs"};
    std::string first;
    bool same = true;
    for (const Item& it : items) {
      const size_t s = it.name.find('/');
      if (s == std::string::npos) { same = false; break; }
      const std::string f = ToLowerAscii(it.name.substr(0, s));
      if (first.empty()) first = f;
      else if (first != f) { same = false; break; }
    }
    if (same && !first.empty() && !kKnown.count(first)) strip = first.size() + 1;
  }
  for (Item& it : items) {
    const std::string path = it.name.substr(strip);
    if (path.empty()) continue;
    z->files_[ToLowerAscii(path)] = it.e;
    std::string dir;
    size_t start = 0;
    for (;;) {
      const size_t s = path.find('/', start);
      const std::string part = path.substr(start, s == std::string::npos ? std::string::npos : s - start);
      if (!part.empty()) z->dirs_[dir][ToLowerAscii(part)] = part;
      if (s == std::string::npos) break;
      dir += (dir.empty() ? "" : "/") + ToLowerAscii(part);
      start = s + 1;
    }
  }
  return z;
}

bool ZipSource::Exists(const std::string& path) {
  const std::string k = NormDir(path);
  return files_.count(k) > 0 || dirs_.count(k) > 0;
}

std::vector<std::string> ZipSource::ListDir(const std::string& dir) {
  std::vector<std::string> r;
  auto it = dirs_.find(NormDir(dir));
  if (it == dirs_.end()) return r;
  for (const auto& kv : it->second) r.push_back(kv.second);
  return r;
}

bool ZipSource::ReadFile(const std::string& path, std::vector<uint8_t>* out) {
  auto it = files_.find(NormDir(path));
  if (it == files_.end()) return false;
  const Entry& e = it->second;
  if (e.size > (1ull << 30) || e.comp_size > (1ull << 30)) return false;
  uint8_t lh[30];
  if (!read_(e.local_offset, lh, 30) || U32(lh) != 0x04034b50u) return false;
  const uint64_t data = e.local_offset + 30 + U16(lh + 26) + U16(lh + 28);
  std::vector<uint8_t> comp(static_cast<size_t>(e.comp_size));
  if (!comp.empty() && !read_(data, comp.data(), comp.size())) return false;
  if (e.method == 0) {
    if (e.comp_size != e.size) return false;
    *out = std::move(comp);
    return true;
  }
  if (e.method != 8) return false;
  out->assign(static_cast<size_t>(e.size), 0);
  return e.size == 0 || InflateRaw(comp.data(), comp.size(), out->data(), out->size());
}

}  // namespace fleet
