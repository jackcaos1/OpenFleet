#include "game/Ai.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>

namespace fleet {

double Aip::Get(const char* name, double fallback) const {
  auto it = params.find(name);
  return it == params.end() ? fallback : it->second;
}

namespace {

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string StripComments(const std::string& t) {
  std::string o;
  o.reserve(t.size());
  for (size_t i = 0; i < t.size(); ++i) {
    if (t[i] == '/' && i + 1 < t.size() && t[i + 1] == '/') {
      while (i < t.size() && t[i] != '\n') ++i;
      o += '\n';
    } else if (t[i] == '/' && i + 1 < t.size() && t[i + 1] == '*') {
      i += 2;
      while (i + 1 < t.size() && !(t[i] == '*' && t[i + 1] == '/')) ++i;
      ++i;
    } else {
      o += t[i];
    }
  }
  return o;
}

}  // namespace

bool ParseAip(const std::string& raw, Aip* out) {
  const std::string text = StripComments(raw);
  bool any = false;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    const std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    const bool is_int = line.compare(i, 4, "int ") == 0, is_dbl = line.compare(i, 7, "double ") == 0;
    if (!is_int && !is_dbl) continue;
    i += is_int ? 4 : 7;
    size_t j = i;
    while (j < line.size() && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_')) ++j;
    const std::string name = Lower(line.substr(i, j - i));
    const size_t eq = line.find('=', j);
    if (name.empty() || eq == std::string::npos || line.find('[', j) < eq) continue;  // arrays are handled below
    char* end = nullptr;
    const double v = std::strtod(line.c_str() + eq + 1, &end);
    if (end == line.c_str() + eq + 1) continue;
    out->params[name] = v;
    any = true;
  }
  // the_build_list[...] = { "name", n[, base], ... };
  const size_t bl = text.find("the_build_list");
  if (bl != std::string::npos) {
    const size_t open = text.find('{', bl), close = text.find('}', bl);
    if (open != std::string::npos && close != std::string::npos && close > open) {
      const bool better = text.rfind("Better_Build_List_Element", bl) != std::string::npos && text.rfind("Better_Build_List_Element", bl) + 40 > bl;
      std::vector<AipElement> list;
      size_t k = open + 1;
      while (k < close) {
        const size_t q0 = text.find('"', k);
        if (q0 == std::string::npos || q0 >= close) break;
        const size_t q1 = text.find('"', q0 + 1);
        if (q1 == std::string::npos) break;
        AipElement e;
        e.unit = Lower(text.substr(q0 + 1, q1 - q0 - 1));
        char* end = nullptr;
        size_t p = q1 + 1;
        while (p < close && (text[p] == ',' || std::isspace(static_cast<unsigned char>(text[p])))) ++p;
        double v = std::strtod(text.c_str() + p, &end);
        if (end != text.c_str() + p) {
          e.count = static_cast<int>(v);
          p = static_cast<size_t>(end - text.c_str());
          if (better) {
            while (p < close && (text[p] == ',' || std::isspace(static_cast<unsigned char>(text[p])))) ++p;
            v = std::strtod(text.c_str() + p, &end);
            if (end != text.c_str() + p) { e.base = static_cast<int>(v); p = static_cast<size_t>(end - text.c_str()); }
          }
        }
        list.push_back(e);
        k = p;
      }
      if (!list.empty()) { out->build_list = list; any = true; }
    }
  }
  return any;
}

void MergeAip(Aip* base, const Aip& over) {
  for (const auto& kv : over.params) base->params[kv.first] = kv.second;
  if (!over.build_list.empty()) base->build_list = over.build_list;
}

void BuildPlanner::Reset(const std::vector<AipElement>& list) {
  list_ = list;
  index_ = 0;
  built_ = 0;
}

bool BuildPlanner::Peek(AipElement* out) const {
  if (Done()) return false;
  *out = list_[index_];
  return true;
}

void BuildPlanner::Advance() {
  if (Done()) return;
  if (++built_ >= std::max(1, list_[index_].count)) { ++index_; built_ = 0; }
}

void BuildPlanner::Skip() {
  if (Done()) return;
  ++index_;
  built_ = 0;
}

std::vector<int> CumulativeTargets(const std::vector<AipElement>& list) {
  std::map<std::string, int> total;
  std::vector<int> out;
  for (const AipElement& e : list) {
    total[e.unit] += std::max(1, e.count);
    out.push_back(total[e.unit]);
  }
  return out;
}

Goal EvaluateGoal(GoalKind kind, const GoalCell& c, const Aip& aip) {
  Goal g;
  g.kind = kind;
  g.cell = c;
  const double home = aip.Get("distance_from_home_priority_modifier", 0), enemy = aip.Get("distance_from_enemy_priority_modifier", 0);
  const double troops = aip.Get("attack_troops_priority", 0), perim = c.perimeter ? aip.Get("perimeter_priority", 0) : 0.0;
  const double common = c.dist_home * home + c.dist_enemy * enemy + c.threat * troops + perim;
  switch (kind) {
    case GoalKind::Attack:  // invalid when the cell has enemy stations or no threat
      g.valid = c.enemy_stations == 0 && c.threat > 0;
      g.raw = common;
      break;
    case GoalKind::Defend:  // invalid when the cell has no friendly station
      g.valid = c.own_stations > 0;
      g.raw = common + c.own_stations * aip.Get("defend_priority", aip.Get("defend_buildings_priority", 0));
      break;
    case GoalKind::AttackBase:  // invalid when the cell has no enemy station
      g.valid = c.enemy_stations > 0;
      g.raw = common + c.enemy_stations * aip.Get("attack_enemy_base_priority", 0);
      break;
  }
  return g;
}

double MatchUtility(const Goal& g, float ux, float uz, const Aip& aip) {
  const double d = std::sqrt(static_cast<double>(g.cell.x - ux) * (g.cell.x - ux) + static_cast<double>(g.cell.z - uz) * (g.cell.z - uz));
  return d * aip.Get("distance_from_goal_match_modifier", 0) + g.raw;
}

std::vector<std::vector<int>> AssignGoals(const std::vector<Goal>& goals, const std::vector<SquadUnit>& units, const Aip& aip) {
  std::vector<std::vector<int>> out(goals.size());
  std::set<int> used;
  // goal order: by the best match any unit has with it
  struct Cand { size_t goal; double best; };
  std::vector<Cand> order;
  for (size_t gi = 0; gi < goals.size(); ++gi) {
    if (!goals[gi].valid) continue;
    double best = -1e30;
    for (const SquadUnit& u : units) best = std::max(best, MatchUtility(goals[gi], u.x, u.z, aip));
    if (!units.empty()) order.push_back({gi, best});
  }
  std::stable_sort(order.begin(), order.end(), [](const Cand& a, const Cand& b) { return a.best > b.best; });
  for (const Cand& c : order) {
    const Goal& g = goals[c.goal];
    std::vector<const SquadUnit*> free_units;
    for (const SquadUnit& u : units)
      if (!used.count(u.id)) free_units.push_back(&u);
    std::stable_sort(free_units.begin(), free_units.end(), [&](const SquadUnit* a, const SquadUnit* b) { return MatchUtility(g, a->x, a->z, aip) > MatchUtility(g, b->x, b->z, aip); });
    float have = 0;
    std::vector<int> chosen;
    for (const SquadUnit* u : free_units) {
      chosen.push_back(u->id);
      have += u->shield;
      if (have > g.cell.enemy_shield) break;
    }
    if (!chosen.empty() && have > g.cell.enemy_shield) {  // step 8: enough strength, otherwise rolled back
      for (int id : chosen) used.insert(id);
      out[c.goal] = chosen;
    }
  }
  return out;
}

}  // namespace fleet
