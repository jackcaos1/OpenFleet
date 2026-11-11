#include "formats/SaveSchema.h"

#include <cstdlib>
#include <cstring>

namespace fleet {
namespace {

struct Row {
  const char *odf, *cls, *label;
  int variant, count;
  const char *mission, *full;
};
const Row kRows[] = {
#include "formats/SaveSchema.inc"
};

// drop_lists: Zaehllisten (odfRestrict, handleRestrict) werden mit Zaehler 0 geschrieben, ihre Schleifenelemente entfallen
void Parse(const char* s, SgLayout* l, bool drop_lists) {
  int idx = 0;
  while (*s) {
    const char* e = std::strchr(s, ';');
    std::string item = e ? std::string(s, e) : std::string(s);
    const size_t a = item.find(':'), b = item.rfind(':');
    SgField f;
    f.kind = item.substr(0, a);
    const std::string nm0 = item.substr(a + 1, b - a - 1);
    if (drop_lists && (nm0 == "odfRestrict" || nm0 == "handleRestrict")) {
      if (!e) break;
      s = e + 1;
      continue;
    }
    f.name = item.substr(a + 1, b - a - 1);
    f.size = std::atoi(item.c_str() + b + 1);
    f.elems = f.kind == "euler" ? 5 : 1;
    f.index = idx;
    idx += f.elems;
    l->by_name[f.name] = static_cast<int>(l->fields.size());
    l->fields.push_back(f);
    if (!e) break;
    s = e + 1;
  }
  l->elems = idx;
}

struct Tables {
  std::map<std::string, std::vector<SgLayout>> mission, full;
  std::map<std::string, std::string> by_label;  // Klassenbezeichnung -> erste ODF mit dieser Bezeichnung
  Tables() {
    for (const Row& r : kRows) {
      SgLayout m, f;
      m.cls = f.cls = r.cls;
      Parse(r.mission, &m, false);
      Parse(r.full, &f, true);
      mission[r.odf].push_back(std::move(m));
      full[r.odf].push_back(std::move(f));
      if (r.variant == 0 && r.label[0] && !by_label.count(r.label)) by_label[r.label] = r.odf;
    }
  }
};

const Tables& TablesRef() {
  static const Tables t;
  return t;
}

}  // namespace

const std::vector<SgLayout>* SgLayouts(const std::string& odf, bool mission) {
  const Tables& t = TablesRef();
  const auto& m = mission ? t.mission : t.full;
  auto it = m.find(odf);
  return it == m.end() ? nullptr : &it->second;
}

const std::vector<SgLayout>* SgLayoutsFor(const std::string& odf, const std::string& label, bool mission) {
  if (const auto* l = SgLayouts(odf, mission)) return l;
  const Tables& t = TablesRef();
  std::string lo;
  for (char c : label) lo.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c));
  auto it = t.by_label.find(lo);
  return it == t.by_label.end() ? nullptr : SgLayouts(it->second, mission);
}

const SgElem* SgField_(const SgObject& o, const SgLayout& l, const std::string& name) {
  auto it = l.by_name.find(name);
  if (it == l.by_name.end()) return nullptr;
  const size_t i = kSgObjectHeadElems + l.fields[it->second].index;
  return i < o.e.size() ? &o.e[i] : nullptr;
}
SgElem* SgFieldMut(SgObject* o, const SgLayout& l, const std::string& name) {
  return const_cast<SgElem*>(SgField_(*o, l, name));
}

}  // namespace fleet
