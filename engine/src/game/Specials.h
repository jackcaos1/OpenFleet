// Special weapons that are plain state machines on the ship: regeneration, cloaking device, self destruct.
// Armada.exe: Weapon vtable slot 4 is the per-frame process. State byte +0x2c = "button pressed", +0x2d = active.
// Regeneration 0x508ad0, self destruct 0x508fc0, cloaking device 0x507770. Details and open points: docs/formats/specials.md.
#pragma once

#include <string>

#include "formats/Odf.h"

namespace fleet {

enum class SpecialKind { Unsupported, Regeneration, Cloak, SelfDestruct };

struct SpecialDef {
  std::string odf, name;
  SpecialKind kind = SpecialKind::Unsupported;
  int button_slot = 0;           // buttonSlot: place in the speed rail (0 = not on the rail)
  float energy_cost = 0;         // specialEnergyCost
  float regen_modifier = 0;      // regenModifier: shield points per second
  float timer = 0;               // selfdestruct timer (s)
  float radius = 0;              // selfdestruct radius
  float effect_timer = 0;        // cloak: time to fade in / out
};

bool LoadSpecialDef(OdfLibrary& lib, const std::string& odf, SpecialDef* out);

// Special energy of a craft (+0x47c current, +0x478 maximum; +0x58 adds and clamps).
struct SpecialEnergy {
  float cur = 0, max = 0, rate = 0;
  void Add(float d) {
    if (max <= 0) return;
    cur += d;
    if (cur > max) cur = max;
    if (cur < 0) cur = 0;
  }
  float Frac() const { return max > 0 ? cur / max : 0.0f; }
};

struct SpecialRt {
  SpecialDef def;
  bool active = false;
  float timer = 0;      // selfdestruct countdown / cloak fade progress (0..effect_timer)
  bool fired = false;   // selfdestruct: countdown reached zero
};

// The button was pressed. Returns true when the state changed.
//  - regeneration / cloak: switch off when active, switch on only with energy >= cost (cloak takes the cost once, regeneration drains it per second)
//  - self destruct: first press starts the countdown, a second press cancels it
bool SpecialPress(SpecialRt* s, SpecialEnergy* e);

struct SpecialResult {
  float shield_add = 0;      // regeneration: shield points gained this step
};
// One frame. Regeneration drains cost * dt, adds regenModifier * dt shield and stops when the energy is gone or the shield is full (0x508ad0).
SpecialResult SpecialUpdate(SpecialRt* s, SpecialEnergy* e, float dt, float shield, float shield_max);

// Cloak fade 0..1 (1 = fully cloaked).
float CloakAmount(const SpecialRt& s);

}  // namespace fleet
