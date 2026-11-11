#include "game/Combat.h"

#include <algorithm>

namespace fleet {

bool Health::damaged() const {
  for (int i = 0; i < kSysCount; ++i)
    if (sys[i] < sys_max) return true;
  return false;
}

void InitHealth(Health* h, float shield_max, float shield_rate, float shield_delay, float hull_max, float crew_max, const ShipRules& rules, const RaceRules& race) {
  h->shield_max = h->shield = std::max(0.0f, shield_max);
  h->shield_rate = shield_rate;
  h->shield_delay = shield_delay;
  h->since_hit = 1e9f;
  h->shield_was_zero = false;
  h->crew_max = h->crew = std::max(0.0f, crew_max);
  h->rules = rules;
  h->race = race;
  h->sys_max = std::max(1.0f, rules.sys_points);
  for (float& s : h->sys) s = h->sys_max;
  h->hull_max = h->hull = std::max(1.0f, hull_max);
  h->exploded = false;
  h->repair_acc = 0;
}

void LoseCrewPercent(Health* h, float percent) {
  if (h->crew_max <= 0 || percent <= 0) return;
  h->crew = std::max(0.0f, h->crew - h->crew_max * percent / 100.0f);
}

int ApplyHit(Health* h, Rng* r, const HitInfo& hit) {
  if (hit.damage <= 0 || !h->alive()) return kSysCount;
  h->since_hit = 0.0f;
  float leak = hit.damage;
  const bool shielded = h->shield_max > 0 && h->shield > 0 && h->sys_ok(kShield) && !hit.ignore_shield;
  if (shielded) {
    const float take = std::min(h->shield, hit.damage);
    h->shield -= take;
    leak = hit.damage - take;
    if (h->shield <= 0) h->shield_was_zero = true;
    if (hit.damage > hit.threshold) LoseCrewPercent(h, (hit.damage - hit.threshold) * hit.shield_crew_mod);
    if (leak <= 0) return kSysCount;
  }
  const ShipRules& q = h->rules;
  float total = q.hull_pct + q.critical_pct;
  for (float p : q.hit_pct) total += p;
  float roll = r->F01() * total;
  if ((roll -= q.critical_pct) < 0) {
    h->exploded = true;
    return kSysCount;
  }
  if ((roll -= q.hull_pct) < 0) {
    LoseCrewPercent(h, leak * hit.hull_crew_mod);
    return kSysCount;
  }
  for (int i = 0; i < kSysCount; ++i) {
    if ((roll -= q.hit_pct[i]) < 0) {
      if (h->sys[i] <= 0) return kSysCount;
      h->sys[i] = std::max(0.0f, h->sys[i] - 1.0f);
      if (h->sys[i] <= 0) {
        LoseCrewPercent(h, q.crew_loss[i]);
        return i;
      }
      return kSysCount;
    }
  }
  return kSysCount;
}

void StepHealth(Health* h, float dt) {
  if (!h->alive()) return;
  h->since_hit += dt;
  const float wait = h->shield_was_zero ? h->shield_delay : 0.0f;
  if (h->since_hit >= wait && h->shield < h->shield_max && h->sys_ok(kShield) && !h->derelict()) {
    h->shield = std::min(h->shield_max, h->shield + h->shield_rate * dt);
    if (h->shield >= h->shield_max * 0.05f) h->shield_was_zero = false;
  }
  if (h->crew_max <= 0 || h->crew <= 0) return;
  if (!h->sys_ok(kLife)) LoseCrewPercent(h, h->rules.life_loss * dt);
  // Reparatur: ein Punkt je repairTime Sekunden (je Rasse und Besatzungsstatus), Lebenserhaltung und Antrieb zuerst
  if (h->damaged() && h->crew > 0) {
    static const int order[kSysCount] = {kLife, kEngines, kWeapons, kShield, kSensors};
    int pick = -1;
    for (int k = 0; k < kSysCount && pick < 0; ++k)
      if (h->sys[order[k]] < h->sys_max) pick = order[k];
    if (pick >= 0) {
      const float per = h->rules.repair_time[pick] * h->repair_mult() / std::max(0.1f, h->race.repair_strength);
      h->repair_acc += dt;
      if (h->repair_acc >= per) {
        h->repair_acc = 0;
        h->sys[pick] = std::min(h->sys_max, h->sys[pick] + 1.0f);
      }
    }
  }
}

float RollDamage(Rng* r, float base, float variance) {
  const float v = variance > 0 ? (r->F01() * 2.0f - 1.0f) * variance : 0.0f;
  return std::max(0.0f, base + v);
}

bool RollHit(Rng* r, float chance) {
  if (chance >= 1.0f) return true;
  if (chance <= 0.0f) return false;
  return r->F01() < chance;
}

}  // namespace fleet
