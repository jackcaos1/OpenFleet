#include "game/FireDemo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "io/FileSource.h"

namespace fleet {

void FireDemo::Clear() {
  slots_.clear();
  shots_.clear();
  sounds_.clear();
  skipped_ = 0;
}

void FireDemo::Setup(const Model& model, const UnitDef& unit, OdfLibrary& lib, const SprLibrary& spr, float model_radius) {
  Clear();
  max_dist_ = std::max(1.0f, model_radius * 8.0f);
  for (const WeaponSlot& ws : unit.weapons) {
    WeaponDef w;
    OrdnanceDef o;
    if (!LoadWeaponDef(lib, ws.odf, &w) || w.ordnance.empty() || !LoadOrdnanceDef(lib, w.ordnance, &o) || w.special ||
        o.special_energy_cost > 0 || o.sprite.empty() || w.shot_delay <= 0) {
      ++skipped_;
      continue;
    }
    const SprSprite* sp = spr.FindSprite(o.sprite);
    if (!sp) {
      ++skipped_;
      continue;
    }
    Slot s;
    s.name = w.name.empty() ? w.odf : w.name;
    s.sound = w.fire_sound;
    s.delay = w.shot_delay;
    s.timer = 0;
    for (const std::string& h : ws.hardpoints) {
      for (const ModelHardpoint& mh : model.hardpoints) {
        if (mh.name == ToLowerAscii(h)) s.points.push_back(mh);
      }
    }
    if (s.points.empty()) {
      ++skipped_;
      continue;
    }
    Shot& p = s.proto;
    p.sprite = sp;
    p.anim = sp->anim.empty() ? nullptr : spr.FindAnimation(sp->anim);
    p.width = o.radius > 0 ? o.radius : 2.0f;
    if (o.shot_speed >= 1e5f) {  // praktisch sofort: Strahl vom Hardpoint, Laenge = Reichweite, im Betrachter begrenzt
      p.kind = Shot::Kind::Beam;
      p.length = std::min(w.range > 0 ? w.range : max_dist_, max_dist_);
      p.life = std::min(o.life_span > 0 ? o.life_span : s.delay, s.delay);
    } else {
      p.kind = o.length > 0 ? Shot::Kind::Bolt : Shot::Kind::Ball;
      p.length = o.length;
      p.speed = o.shot_speed * kDemoSpeedScale;
      p.life = o.life_span > 0 ? o.life_span : 4.0f;
    }
    slots_.push_back(std::move(s));
  }
}

void FireDemo::Update(float dt) {
  for (Slot& s : slots_) {
    if (!firing_) {
      s.timer = 0;
      continue;
    }
    s.timer -= dt;
    while (s.timer <= 0) {
      if (!s.sound.empty() && sounds_.size() < 32) sounds_.push_back(s.sound);
      for (const ModelHardpoint& h : s.points) {
        Shot shot = s.proto;
        for (int k = 0; k < 3; ++k) {
          shot.pos[k] = h.pos[k];
          shot.dir[k] = h.dir[k];
        }
        shots_.push_back(shot);
      }
      s.timer += s.delay;
    }
  }
  for (Shot& sh : shots_) {
    sh.age += dt;
    if (sh.kind != Shot::Kind::Beam) {
      for (int k = 0; k < 3; ++k) sh.pos[k] += sh.dir[k] * sh.speed * dt;
    }
  }
  const float md2 = max_dist_ * max_dist_ * 4;
  shots_.erase(std::remove_if(shots_.begin(), shots_.end(),
                              [&](const Shot& sh) {
                                const float d2 = sh.pos[0] * sh.pos[0] + sh.pos[1] * sh.pos[1] + sh.pos[2] * sh.pos[2];
                                return sh.age >= sh.life || d2 > md2;
                              }),
               shots_.end());
}

std::string FireDemo::Describe() const {
  std::string s = "Feuer: ";
  for (size_t i = 0; i < slots_.size(); ++i) {
    char b[96];
    std::snprintf(b, sizeof(b), "%s%s x%zu alle %.1f s", i ? "; " : "", slots_[i].name.c_str(), slots_[i].points.size(), slots_[i].delay);
    s += b;
  }
  if (slots_.empty()) s += "keine feuernden Waffen";
  if (skipped_ > 0) s += " (" + std::to_string(skipped_) + " Sonderwaffen/ohne Darstellung uebersprungen)";
  return s;
}

}  // namespace fleet
