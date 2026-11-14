#include "data/UnitDef.h"

#include "io/FileSource.h"

#include <cstdio>
#include <cstdlib>
#include <map>

namespace fleet {

namespace {

void Sub(const OdfObject& o, const std::string& prefix, SubsystemDef* s) {
  o.GetInt(prefix + "HitPoints", &s->hit_points);
  o.GetFloat(prefix + "HitPercent", &s->hit_percent);
  o.GetFloat(prefix + "RepairTime", &s->repair_time);
  o.GetFloat(prefix + "CrewLoss", &s->crew_loss);
  s->target_hardpoints = o.GetStrings(prefix + "TargetHardpoints");
}

std::string Num(float v) {
  char b[32];
  if (v == static_cast<int>(v)) std::snprintf(b, sizeof(b), "%d", static_cast<int>(v));
  else std::snprintf(b, sizeof(b), "%.2f", v);
  return b;
}

}  // namespace

bool ReadPerTarget(const OdfObject& o, const char* key, PerTarget* out) {
  const OdfEntry* e = o.Find(key);
  if (!e || e->values.empty()) return false;
  const OdfToken& t0 = e->values[0];
  if (t0.kind != OdfToken::Kind::Int && t0.kind != OdfToken::Kind::Float) return false;
  out->base = static_cast<float>(t0.number);
  out->overrides.clear();
  for (size_t i = 1; i + 1 < e->values.size(); i += 2) {
    const OdfToken& a = e->values[i];
    const OdfToken& b = e->values[i + 1];
    if (a.kind != OdfToken::Kind::String || (b.kind != OdfToken::Kind::Int && b.kind != OdfToken::Kind::Float)) break;
    out->overrides.emplace_back(ToLowerAscii(a.text), static_cast<float>(b.number));
  }
  return true;
}

float PerTarget::For(const std::string& target_odf) const {
  std::string t = ToLowerAscii(target_odf);
  if (t.size() < 4 || t.compare(t.size() - 4, 4, ".odf") != 0) t += ".odf";
  for (const auto& kv : overrides) {
    if (kv.first == t) return kv.second;
  }
  return base;
}

bool LoadUnitDef(OdfLibrary& lib, const std::string& name, UnitDef* out, std::string* err) {
  const OdfObject* o = lib.Get(name, err);
  if (!o) return false;
  *out = UnitDef();
  out->odf = o->name.substr(0, o->name.size() - 4);
  auto S = [&](const char* k, std::string* d) {
    std::string v;
    if (o->GetString(k, &v)) *d = Cp1252ToUtf8(v);
  };
  S("classLabel", &out->class_label);
  S("unitName", &out->unit_name);
  S("tooltip", &out->tooltip);
  S("verboseTooltip", &out->verbose_tooltip);
  S("race", &out->race);
  S("physicsFile", &out->physics_file);
  o->GetFloat("buildTime", &out->build_time);
  o->GetInt("officerCost", &out->officer_cost);
  o->GetInt("crewCost", &out->crew_cost);
  o->GetInt("dilithiumCost", &out->dilithium_cost);
  o->GetInt("officerGain", &out->officer_gain);
  o->GetFloat("dilithiumTransferRate", &out->dilithium_transfer_rate);
  o->GetFloat("maxScrap", &out->max_scrap);
  o->GetFloat("dockingBuffer", &out->docking_buffer);
  for (int i = 0; i < 12; ++i) {
    std::string v;
    if (o->GetString("buildItem" + std::to_string(i), &v) && !v.empty()) out->build_items.push_back(ToLowerAscii(v));
  }
  o->GetFloat("maxHealth", &out->max_health);
  o->GetFloat("shieldRate", &out->shield_rate);
  o->GetFloat("shieldDelay", &out->shield_delay);
  o->GetFloat("maxSpecialEnergy", &out->max_special_energy);
  o->GetFloat("specialEnergyRate", &out->special_energy_rate);
  o->GetFloat("rangeScan", &out->range_scan);
  o->GetFloat("damagedScan", &out->damaged_scan);
  o->GetFloat("attackPower", &out->attack_power);
  o->GetBool("is_starbase", &out->is_starbase);
  o->GetBool("isHero", &out->is_hero);
  Sub(*o, "engines", &out->engines);
  Sub(*o, "lifeSupport", &out->life_support);
  Sub(*o, "weapons", &out->weapons_sys);
  Sub(*o, "shieldGenerator", &out->shield_generator);
  Sub(*o, "sensors", &out->sensors);
  Sub(*o, "hull", &out->hull);
  o->GetFloat("criticalHitPercent", &out->critical_percent);
  o->GetFloat("lifeSupportLoss", &out->life_support_loss);
  S("fireball", &out->fireball);
  for (const OdfEntry& e : o->entries()) {  // weaponN / weaponHardpointsN (N = 1, 2, ...), auch ueber 9 hinaus
    const std::string k = ToLowerAscii(e.key);
    if (k.rfind("weapon", 0) != 0 || k.size() <= 6 || k[6] < '0' || k[6] > '9') continue;
    const int n = std::atoi(k.c_str() + 6);
    if (n < 1 || e.values.empty() || e.values[0].kind != OdfToken::Kind::String) continue;
    if (static_cast<int>(out->weapons.size()) < n) out->weapons.resize(static_cast<size_t>(n));
    out->weapons[static_cast<size_t>(n - 1)].odf = ToLowerAscii(e.values[0].text);
    out->weapons[static_cast<size_t>(n - 1)].hardpoints = o->GetStrings("weaponHardpoints" + std::to_string(n));
  }
  std::vector<WeaponSlot> packed;
  for (WeaponSlot& w : out->weapons) {
    if (!w.odf.empty()) packed.push_back(std::move(w));
  }
  out->weapons = std::move(packed);
  for (const std::string& n : o->GetStrings("possibleCraftNames")) out->possible_names.push_back(Cp1252ToUtf8(n));
  return true;
}

bool LoadRaceDef(OdfLibrary& lib, const std::string& race, RaceDef* out) {
  *out = RaceDef();
  std::string err;
  std::string r = ToLowerAscii(race);
  if (r == "federation") r = "fed";
  else if (r == "klingon") r = "kling";
  else if (r == "romulan") r = "rom";
  const OdfObject* o = r.empty() ? nullptr : lib.Get(r + ".odf", &err);
  if (!o) o = lib.Get("fed.odf", &err);
  if (!o) return false;
  o->GetFloat("repairStrength", &out->repair_strength);
  o->GetFloat("crewYellowStatus", &out->crew_yellow);
  o->GetFloat("crewRedStatus", &out->crew_red);
  o->GetFloat("repairYellow", &out->repair_yellow);
  o->GetFloat("repairRed", &out->repair_red);
  o->GetFloat("weaponYellow", &out->weapon_yellow);
  o->GetFloat("weaponRed", &out->weapon_red);
  o->GetFloat("boardingStrength", &out->boarding_strength);
  o->GetFloat("crewAccumulationRate", &out->crew_accumulation_rate);
  o->GetFloat("planetCrewAccumulationModifier", &out->planet_crew_modifier);
  const struct { const char* key; std::vector<std::string>* list; } kLists[3] = {
      {"minimalUnits", &out->minimal_units}, {"standardUnits", &out->standard_units}, {"superUnits", &out->super_units}};
  for (const auto& l : kLists)
    for (int i = 1; i <= 9; ++i) {
      std::string v;
      if (o->GetString(std::string(l.key) + std::to_string(i), &v) && !v.empty()) l.list->push_back(v);
    }
  return true;
}

bool LoadWeaponDef(OdfLibrary& lib, const std::string& name, WeaponDef* out, std::string* err) {
  const OdfObject* o = lib.Get(name, err);
  if (!o) return false;
  *out = WeaponDef();
  out->odf = o->name.substr(0, o->name.size() - 4);
  std::string v;
  if (o->GetString("wpnName", &v)) out->name = Cp1252ToUtf8(v);
  o->GetString("classLabel", &out->class_label);
  o->GetString("ordName", &out->ordnance);
  o->GetString("fireSound", &out->fire_sound);
  o->GetFloat("shotDelay", &out->shot_delay);
  o->GetFloat("range", &out->range);
  ReadPerTarget(*o, "hitChance", &out->hit_chance);
  o->GetFloat("toHitWarpModifier", &out->to_hit_warp);
  o->GetFloat("toHitStopModifier", &out->to_hit_stop);
  o->GetFloat("toHitSlowModifier", &out->to_hit_slow);
  o->GetBool("special", &out->special);
  return true;
}

bool LoadOrdnanceDef(OdfLibrary& lib, const std::string& name, OrdnanceDef* out, std::string* err) {
  const OdfObject* o = lib.Get(name, err);
  if (!o) return false;
  *out = OrdnanceDef();
  out->odf = o->name.substr(0, o->name.size() - 4);
  o->GetString("classLabel", &out->class_label);
  ReadPerTarget(*o, "damageBase", &out->damage);
  o->GetFloat("damageVariance", &out->damage_variance);
  o->GetFloat("damageThreshold", &out->damage_threshold);
  o->GetFloat("shieldCrewModifier", &out->shield_crew_modifier);
  o->GetFloat("hullCrewModifier", &out->hull_crew_modifier);
  o->GetFloat("specialEnergyCost", &out->special_energy_cost);
  o->GetFloat("lifeSpan", &out->life_span);
  o->GetFloat("shotSpeed", &out->shot_speed);
  o->GetFloat("shotAccel", &out->shot_accel);
  o->GetFloat("omegaTurn", &out->omega_turn);
  o->GetFloat("seekTime", &out->seek_time);
  o->GetFloat("radius", &out->radius);
  o->GetFloat("length", &out->length);
  o->GetBool("ignoreShield", &out->ignore_shield);
  o->GetString("sprite", &out->sprite);
  o->GetString("shotGeometry", &out->shot_geometry);
  o->GetString("hitSound", &out->hit_sound);
  o->GetFloat("spriteDuration", &out->sprite_duration);
  if (const OdfEntry* e = o->Find("lightColor")) {
    if (e->values.size() >= 3) {
      for (int k = 0; k < 3; ++k) out->light_color[k] = static_cast<float>(e->values[static_cast<size_t>(k)].number);
      out->has_light = true;
    }
  }
  o->GetFloat("lightFalloffStart", &out->light_falloff_start);
  o->GetFloat("lightFalloffRange", &out->light_falloff_range);
  return true;
}

bool LoadPhysicsDef(OdfLibrary& lib, const std::string& name, PhysicsDef* out, std::string* err) {
  const OdfObject* o = lib.Get(name, err);
  if (!o) return false;
  *out = PhysicsDef();
  out->odf = o->name.substr(0, o->name.size() - 4);
  o->GetFloat("impulseSpeed", &out->impulse_speed);
  o->GetFloat("warpSpeed", &out->warp_speed);
  o->GetFloat("yawRotationRate", &out->yaw_rate);
  o->GetFloat("turnRadius", &out->turn_radius);
  o->GetFloat("maxRoll", &out->max_roll);
  o->GetFloat("maxPitch", &out->max_pitch);
  o->GetFloat("pitchRotationSpeed", &out->pitch_rate);
  o->GetFloat("rollRotationSpeed", &out->roll_rate);
  o->GetFloat("frontBackInertia", &out->front_back_inertia);
  o->GetFloat("leftRightInertia", &out->side_inertia);
  o->GetFloat("maxReverseSpeed", &out->max_reverse);
  o->GetFloat("velocCoeffStill", &out->veloc_coeff_still);
  o->GetFloat("velocWhenCoeffHalf", &out->veloc_half);
  o->GetFloat("headingCloseEnoughForWarp", &out->heading_ok_for_warp);
  o->GetFloat("tooCloseForWarp", &out->too_close_for_warp);
  o->GetFloat("tooCloseToTurn", &out->too_close_to_turn);
  o->GetFloat("maxRotationSpeed", &out->max_rotation_speed);
  return true;
}

std::string DescribeUnit(OdfLibrary& lib, const UnitDef& u) {
  std::string s = (u.unit_name.empty() ? u.odf : u.unit_name) + " [" + u.odf + ", " + (u.class_label.empty() ? "?" : u.class_label) +
                  (u.race.empty() ? "" : ", " + u.race) + "]\n";
  s += "Schild " + Num(u.max_health) + " (+" + Num(u.shield_rate) + "/s)  Besatzung " + std::to_string(u.crew_cost) + "  Offiziere " +
       std::to_string(u.officer_cost) + "  Dilithium " + std::to_string(u.dilithium_cost) + "  Bauzeit " + Num(u.build_time) + "\n";
  PhysicsDef ph;
  if (!u.physics_file.empty() && LoadPhysicsDef(lib, u.physics_file, &ph)) {
    s += "Impuls " + Num(ph.impulse_speed) + "  Warp " + Num(ph.warp_speed) + "  Drehrate " + Num(ph.yaw_rate) + "\n";
  }
  if (!u.weapons.empty()) {
    s += "Waffen:";
    for (const WeaponSlot& w : u.weapons) {
      WeaponDef d;
      std::string n = w.odf;
      std::string extra;
      if (LoadWeaponDef(lib, w.odf, &d)) {
        if (!d.name.empty()) n = d.name;
        extra = " Reichw. " + Num(d.range) + " Takt " + Num(d.shot_delay);
        OrdnanceDef od;
        if (!d.ordnance.empty() && LoadOrdnanceDef(lib, d.ordnance, &od) && od.damage.base > 0) {
          extra += " Schaden " + Num(od.damage.base);
          if (od.damage_variance > 0) extra += "+-" + Num(od.damage_variance);
        }
      }
      s += "\n  " + n + " x" + std::to_string(w.hardpoints.size()) + extra;
    }
  }
  return s;
}

}  // namespace fleet
