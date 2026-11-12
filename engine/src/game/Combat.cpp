#include "game/Combat.h"

#include <algorithm>
#include <cmath>

namespace fleet {

namespace {

// Ein Teilsystem lebt, wenn es eingeschaltet ist oder nur zeitweise ausgeschaltet ist und noch Punkte hat (0x467620/0x4676d0).
bool Alive(const SystemBlock& b) { return b.active || (b.disable_timer > 0 && b.points != 0); }

// Besatzung nach einem Verlust: Prozent der aktuellen Besatzung plus ein fester Anteil (0x4652c1, 0x4655e6, 0x467cb3, 0x465fbb).
void LoseCrew(Health* h, float percent_of_current, float flat) {
  if (h->derelict || h->crew_max <= 0) return;
  SetCrew(h, h->crew - (h->crew * percent_of_current * 0.01f + flat));
}

// Schaltet ein ausgeschaltetes System wieder ein (0x467d40); true, wenn es dabei angegangen ist.
bool Reenable(SystemBlock* b, float dt) {
  if (b->disable_timer > 0) b->disable_timer -= dt;
  if (b->script_disabled) return false;
  if (b->disable_timer > 0) return false;
  b->disable_timer = 0;
  if (b->points < b->max_points) return false;
  if (b->max_points == 0) return false;
  b->active = true;
  return true;
}

// Faellt ein System aus, geben seine Trefferanteile an die lebenden Systeme weiter (0x467620).
void Redistribute(Health* h, SystemBlock* dead) {
  if (dead->active) return;
  int n = 0;
  for (int i : kBlockOrder)
    if (Alive(h->sys[i])) ++n;
  if (n == 0) {
    h->crit_pct += dead->hit_pct;  // kein System uebrig: der Anteil geht an den kritischen Treffer (0x4676b6)
    return;
  }
  const float share = dead->hit_pct / static_cast<float>(n);
  for (int i : kBlockOrder)
    if (Alive(h->sys[i])) h->sys[i].hit_pct += share;
}

// Ein wieder eingeschaltetes System nimmt seinen Anteil zurueck (0x4676d0): jedes lebende System bekommt Original + gleichen Anteil.
void RestoreShares(Health* h, SystemBlock* back_p) {
  const SystemBlock& back = *back_p;
  if (!back.active) return;
  int n = 0;
  float acc = 0;
  for (int i : kBlockOrder) {  // Summationsreihenfolge wie im Original (Gleitkomma)
    const SystemBlock& b = h->sys[i];
    if (Alive(b)) {
      acc += b.hit_pct - b.hit_pct_orig;
      ++n;
    }
  }
  if (n <= 1) {  // nur das reparierte System lebt: es bekommt den gesammelten kritischen Anteil (0x467775)
    back_p->hit_pct = h->crit_pct - h->crit_pct_orig;
    h->crit_pct = h->crit_pct_orig;
    return;
  }
  const float share = (acc - back.hit_pct) / static_cast<float>(n);
  for (int i : kBlockOrder)
    if (Alive(h->sys[i])) h->sys[i].hit_pct = share + h->sys[i].hit_pct_orig;
}

// Zeit je Punkt (0x468060): Zeit je Punkt laut ODF x Besatzungsfaktor x Rassenstaerke. Der Teiler ist ein Spezialeffekt (TimedHyper), den es
// hier nicht gibt, also 1,0.
float PerPointTime(const Health& h, const SystemBlock& b) {
  const float frac = h.crew_max != 0 ? h.crew / h.crew_max : 1.0f;
  float f = 1.0f;
  if (h.race.crew_yellow >= frac) f = h.race.repair_yellow;
  if (h.race.crew_red >= frac) f = h.race.repair_red;
  return f * h.race.repair_strength * b.repair_per_point;
}

// Reparatur eines Systems (0x4683d0): true, wenn es dabei vollstaendig repariert und wieder eingeschaltet wurde.
bool Repair(Health* h, SystemBlock* b, float dt) {
  if (b->points >= b->max_points) return false;
  if (b->disable_timer > 0) return false;
  if (b->script_disabled) return false;
  const float nw = PerPointTime(*h, *b);
  if (nw != b->per_point) {
    if (b->per_point != 0) b->countdown = nw / b->per_point * b->countdown;
    b->per_point = nw;
  }
  b->countdown -= dt;
  if (b->countdown > 0) return false;
  for (;;) {
    b->countdown += b->per_point;
    ++b->points;
    if (b->points >= b->max_points) {
      b->countdown = 0;
      Reenable(b, 0);
      if (b->hit_pct <= 0) {
        b->hit_pct = b->hit_pct_orig;
        RestoreShares(h, b);
      }
      return true;
    }
    if (b->countdown > 0) return false;
  }
}

// Ein System faellt aus (0x467b70, Punkte <= 0): ausschalten, Reparaturzeit setzen, Anteile verteilen, Besatzung verlieren.
void Destroy(Health* h, SystemBlock* b) {
  if (b->disable_timer < 0) b->disable_timer = 0;
  b->active = false;
  b->points = 0;
  b->per_point = b->countdown = PerPointTime(*h, *b);
  Redistribute(h, b);
  b->hit_pct = 0;
  LoseCrew(h, b->crew_loss, 1.0f);
}

// Punkte abziehen (0x467b70).
void DamageSystem(Health* h, SystemBlock* b, int pts) {
  if (pts == 0) return;
  b->points -= pts;
  if (b->points > 0) return;
  Destroy(h, b);
}

}  // namespace

CombatConfig& CombatCfg() {
  static CombatConfig cfg;
  return cfg;
}

float DifficultyDamageScale(int mode) {
  const CombatConfig& c = CombatCfg();
  return mode == 1 ? c.easy_damage : (mode == 3 ? c.hard_damage : 1.0f);
}

bool Health::damaged() const {
  for (const SystemBlock& b : sys)
    if (b.points < b.max_points) return true;
  return false;
}

void InitHealth(Health* h, const HealthSpec& spec, const RaceRules& race) {
  *h = Health();
  h->shield_max = h->shield = std::max(0.0f, spec.shield_max);
  h->shield_rate = spec.shield_rate;
  h->shield_delay = spec.shield_delay;
  h->crew_max = h->crew = std::max(0.0f, spec.crew_max);
  h->life_support_loss = spec.life_support_loss;
  h->hull_pct = spec.hull_pct;
  h->crit_pct_orig = h->crit_pct = spec.critical_pct;
  h->race = race;
  const CombatConfig& cfg = CombatCfg();
  for (int i = 0; i < kSysCount; ++i) {
    const SystemSpec& s = spec.sys[i];
    SystemBlock& b = h->sys[i];
    int pts = s.points;
    if (cfg.use_system_hitpoints && pts > 0) pts += cfg.system_hitpoints_bonus;  // 0x460f95
    b.max_points = b.points = pts;
    b.active = pts != 0;
    b.hit_pct_orig = b.hit_pct = s.hit_pct;
    b.crew_loss = s.crew_loss;
    b.repair_per_point = pts > 0 ? s.repair_time / static_cast<float>(pts) : s.repair_time;  // 0x46103e
    b.per_point = b.repair_per_point;
    b.countdown = 0;
  }
}

int RollHitSystem(const Health& h, Rng* r) { return HitSystemForRoll(h, r->Roll(100)); }

int HitSystemForRoll(const Health& h, int roll) {
  float v = static_cast<float>(roll);
  for (int k = 0; k < kSysCount; ++k) {
    v -= h.sys[kRollOrder[k]].hit_pct;
    if (v < 0) return kRollOrder[k];
  }
  v -= h.hull_pct;
  if (v < 0) return kHitHull;
  return kHitCritical;
}

void SetCrew(Health* h, float crew) {
  h->crew = crew;
  if (h->crew < 1.0f && h->crew_max > 0) {
    if (h->derelict) return;
    h->crew = 0;
    if (h->self_destruct) {
      h->exploded = true;  // 0x466dc3: Zerstoerung statt Derelict, solange ein Countdown laeuft
      return;
    }
    h->derelict = true;
    for (SystemBlock& b : h->sys) {
      if (b.disable_timer < 0) b.disable_timer = 0;
      b.active = false;
    }
    h->shield = 0;  // vtable+0x54(0): SetHealth(0)
    return;
  }
  h->derelict = false;
  for (int i : kBlockOrder)
    if (!h->sys[i].active) Reenable(&h->sys[i], 0);
}

int ApplyHit(Health* h, Rng* r, const HitInfo& hit) {
  if (!h->alive() || !CombatCfg().use_system_hitpoints) return kSysCount;
  return ApplyHitTo(h, RollHitSystem(*h, r), hit);  // der Wurf geschieht vor dem Schaden, auch wenn der Schild alles aufnimmt
}

int ApplyHitTo(Health* h, int idx, const HitInfo& hit) {
  if (!h->alive() || !CombatCfg().use_system_hitpoints) return kSysCount;
  const float dmg = hit.damage * hit.damage_scale;
  if (dmg <= 0) return kSysCount;
  const float s_part = CombatCfg().shield_damage_protection * dmg;  // Anteil fuer den Schild
  float sys_dmg = dmg - s_part;                                      // Anteil fuer die Systeme
  if (!hit.ignore_shield) {
    if (h->shield > 0 && hit.threshold > 0) {
      const float excess = s_part - hit.threshold;
      if (excess > 0) LoseCrew(h, hit.shield_crew_mod * excess, std::min(excess, 1.0f));  // 0x4652c1
    }
    h->shield = std::min(h->shield + (-s_part), h->shield_max);  // AddHealth(-S): darf unter 0 fallen
    if (h->shield < 0) sys_dmg -= h->shield;                     // Ueberlauf geht an die Systeme
  } else {
    sys_dmg = dmg;
  }
  int pts = 0;
  if (idx != kHitHull) {
    const float t = sys_dmg + h->carry;
    pts = static_cast<int>(t);  // _ftol: abschneiden
    h->carry = (sys_dmg - static_cast<float>(pts)) + h->carry;  // Reihenfolge der Rechnung wie in 0x4653dc..0x4653e8
  }
  int destroyed = kSysCount;
  switch (idx) {
    case kHitHull:
      LoseCrew(h, hit.hull_crew_mod * sys_dmg, std::min(sys_dmg, 1.0f));  // 0x4655e6
      break;
    case kHitCritical:
      if (pts != 0) {  // 0x465667: Explode und sofort zurueck, ohne den Schildabschluss
        h->exploded = true;
        return destroyed;
      }
      break;
    default: {
      SystemBlock& b = h->sys[idx];
      const bool was_alive = b.points > 0;
      DamageSystem(h, &b, pts);
      if (was_alive && b.points <= 0) destroyed = idx;
      break;
    }
  }
  if (h->shield <= 0) {  // 0x465719: Schild leer -> auf 0 setzen, Aufladeverzoegerung starten
    h->shield = 0;
    h->delay_left = h->shield_delay;
  }
  return destroyed;
}

void StepHealth(Health* h, float dt) {
  if (!h->alive() || h->derelict) return;
  for (int i : kBlockOrder)
    if (!h->sys[i].active) Reenable(&h->sys[i], dt);
  for (int i : kBlockOrder) Repair(h, &h->sys[i], dt);
  if (!h->sys[kLife].active && h->crew_max > 0) LoseCrew(h, h->life_support_loss * dt, std::min(dt, 1.0f));  // 0x465f6f
  // Schildaufladung (0x466b00)
  if (h->sys[kShield].active && h->shield < h->shield_max && h->shield_rate > 0) {
    float t = dt;
    bool wait = false;
    if (h->delay_left != 0) {
      h->delay_left -= dt;
      if (h->delay_left > 0) {
        wait = true;
      } else {
        t = -h->delay_left;
        h->delay_left = 0;
      }
    }
    if (!wait) h->shield = std::min(h->shield + t * h->shield_rate, h->shield_max);
  }
}

Dying MakeDying(float speed, bool hero) {
  Dying d;
  d.left = (speed > 1.0f || hero) ? 5.0f : 3.0f;
  return d;
}

float DyingFxSize(float radius) { return std::max(0.14f * radius, 4.0f); }

int StepDying(Dying* d, float dt, bool* done) {
  d->left -= dt;
  d->acc += dt;
  int n = 0;
  while (d->acc >= kDyingFxStep) {  // wie 0x466320: je angesammelte 0,05 s ein Effekt
    d->acc -= kDyingFxStep;
    ++n;
  }
  *done = d->left <= 0;
  return n;
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
