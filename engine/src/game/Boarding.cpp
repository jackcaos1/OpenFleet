#include "game/Boarding.h"

#include <algorithm>

namespace fleet {

float RemoveCrew(Health* h, float amount) {
  const float old = h->crew;
  if (!h->derelict && h->crew_max > 0) SetCrew(h, h->crew - amount);
  return old < amount ? old : amount;
}

float AddCrew(Health* h, float amount) {
  float left = 0;
  h->crew += amount;
  if (!(h->crew <= h->crew_max)) {
    left = h->crew - h->crew_max;
    h->crew = h->crew_max;
  }
  SetCrew(h, h->crew);
  return left;
}

BeamStep TransporterStep(Health* source, Health* target, float distance, bool enemy) {
  BeamStep r;
  if (distance * distance > kBeamRange * kBeamRange) {
    r.stop = true;
    return r;
  }
  if (enemy) {
    if (target->shield_frac() > 0.01f) {
      r.stop = true;
      return r;
    }
    target->delay_left = target->shield_delay;  // 0x467156: der Schild bleibt unten
  } else if (!(target->crew_max > target->crew)) {
    r.stop = true;
    return r;
  }
  r.created = true;
  r.enemy = enemy;
  r.party_crew = RemoveCrew(source, static_cast<float>(kBeamCrew));
  const float next = (source->crew - static_cast<float>(kBeamCrew)) / source->crew_max;
  if (!(source->race.crew_red < next)) r.stop = true;  // 0x4672e7: naechste Runde waere im roten Bereich
  return r;
}

BoardResult ArriveBoardingParty(Health* t, int t_team, float t_strength, float party_crew, float p_strength, int p_team, bool enemy) {
  BoardResult r;
  if (t->crew_max < 1.0f) return r;  // 0x55f78a
  if (t->derelict) {
    AddCrew(t, party_crew);  // 0x55f7c9, noch vor der Teampruefung
    if (t_team == p_team) r.race_changed = true;
    else if (enemy) r.owner_changed = true;
    return r;
  }
  if (!enemy) {
    r.party_left = AddCrew(t, party_crew);
    return r;
  }
  const float x = party_crew * (p_strength / t_strength);
  r.party_left = t->crew < x ? (x - t->crew) * (t_strength / p_strength) : 0.0f;
  RemoveCrew(t, x);  // 0x467390
  if (t->crew > 0) return r;
  if (!(r.party_left > 0)) return r;
  AddCrew(t, r.party_left);  // 0x55fa09: Rest der Mannschaft uebernimmt das Schiff
  r.owner_changed = true;
  r.party_left = 0;
  return r;
}

}  // namespace fleet
