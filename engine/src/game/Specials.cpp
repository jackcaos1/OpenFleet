#include "game/Specials.h"

#include <algorithm>

namespace fleet {

bool LoadSpecialDef(OdfLibrary& lib, const std::string& odf, SpecialDef* out) {
  const OdfObject* o = lib.Get(odf);
  if (!o) return false;
  *out = SpecialDef();
  out->odf = odf;
  std::string v;
  if (o->GetString("wpnName", &v)) out->name = Cp1252ToUtf8(v);
  std::string label;
  o->GetString("classLabel", &label);
  if (label == "regeneration") out->kind = SpecialKind::Regeneration;
  else if (label == "cloakingdevice") out->kind = SpecialKind::Cloak;
  else if (label == "selfdestruct") out->kind = SpecialKind::SelfDestruct;
  o->GetInt("buttonSlot", &out->button_slot);
  o->GetFloat("specialEnergyCost", &out->energy_cost);
  o->GetFloat("regenModifier", &out->regen_modifier);
  o->GetFloat("timer", &out->timer);
  o->GetFloat("radius", &out->radius);
  o->GetFloat("effectTimer", &out->effect_timer);
  return true;
}

bool SpecialPress(SpecialRt* s, SpecialEnergy* e) {
  switch (s->def.kind) {
    case SpecialKind::Regeneration:
      if (s->active) { s->active = false; return true; }
      if (e->cur >= s->def.energy_cost) { s->active = true; return true; }
      return false;
    case SpecialKind::Cloak:
      if (s->active) { s->active = false; return true; }
      if (e->cur >= s->def.energy_cost) { e->Add(-s->def.energy_cost); s->active = true; return true; }
      return false;
    case SpecialKind::SelfDestruct:
      if (s->active) { s->active = false; return true; }
      s->active = true;
      s->fired = false;
      s->timer = s->def.timer;
      return true;
    default: return false;
  }
}

SpecialResult SpecialUpdate(SpecialRt* s, SpecialEnergy* e, float dt, float shield, float shield_max) {
  SpecialResult r;
  switch (s->def.kind) {
    case SpecialKind::Regeneration: {
      if (!s->active) break;
      e->Add(-dt * s->def.energy_cost);
      r.shield_add = dt * s->def.regen_modifier;
      const float after = std::min(shield + r.shield_add, shield_max);
      if (e->cur <= 0 || shield_max - after < 0.01f) s->active = false;
      break;
    }
    case SpecialKind::Cloak: {
      const float len = s->def.effect_timer > 0 ? s->def.effect_timer : 1.0f;
      s->timer += (s->active ? dt : -dt);
      s->timer = std::clamp(s->timer, 0.0f, len);
      break;
    }
    case SpecialKind::SelfDestruct:
      if (!s->active) break;
      s->timer -= dt;
      if (s->timer < 0) { s->active = false; s->fired = true; }
      break;
    default: break;
  }
  return r;
}

float CloakAmount(const SpecialRt& s) {
  const float len = s.def.effect_timer > 0 ? s.def.effect_timer : 1.0f;
  return std::clamp(s.timer / len, 0.0f, 1.0f);
}

}  // namespace fleet
