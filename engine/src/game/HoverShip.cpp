#include "game/HoverShip.h"
#include <cmath>

namespace fleet {

bool LoadHoverOdf(OdfLibrary& lib, const std::string& name, HoverOdf* out, std::string* err) {
  const OdfObject* o = lib.Get(name, err);
  if (!o) return false;
  return HoverOdfFromObject(*o, out);
}

bool HoverOdfFromObject(const OdfObject& obj, HoverOdf* out) {
  const OdfObject* o = &obj;
  *out = HoverOdf();
  HoverOdf& h = *out;
  struct Kv { const char* key; float* dst; };
  const Kv kv[] = {
      {"impulseSpeed", &h.impulseSpeed}, {"warpSpeed", &h.warpSpeed}, {"velocCoeffStill", &h.velocCoeffStill}, {"velocWhenCoeffHalf", &h.velocWhenCoeffHalf},
      {"pitchRotationSpeed", &h.pitchRotationSpeed}, {"maxPitch", &h.maxPitch}, {"rollRotationSpeed", &h.rollRotationSpeed}, {"maxRoll", &h.maxRoll},
      {"turnRadius", &h.turnRadius}, {"maxRotationSpeed", &h.maxRotationSpeed}, {"rotationAcceleration", &h.rotationAcceleration},
      {"frontBackInertia", &h.frontBackInertia}, {"maxReverseSpeed", &h.maxReverseSpeed}, {"upDownInertia", &h.upDownInertia},
      {"maxVerticalSpeed", &h.maxVerticalSpeed}, {"leftRightInertia", &h.leftRightInertia}, {"rotationStiffness", &h.rotationStiffness},
      {"shp_verticalVelocCoeff", &h.shp_verticalVelocCoeff}, {"shp_verticalPosCoeff", &h.shp_verticalPosCoeff},
      {"shp_pitchVelocCoeff", &h.shp_pitchVelocCoeff}, {"shp_pitchAngleCoeff", &h.shp_pitchAngleCoeff},
      {"sy_rollVelocCoeff", &h.sy_rollVelocCoeff}, {"sy_rollAngleCoeff", &h.sy_rollAngleCoeff}, {"sy_yawVelocCoeff", &h.sy_yawVelocCoeff},
      {"sy_yawAngleCoeff", &h.sy_yawAngleCoeff}, {"sy_velocAngleCoeff", &h.sy_velocAngleCoeff},
      {"sp_pitchVelocCoeff", &h.sp_pitchVelocCoeff}, {"sp_pitchAngleCoeff", &h.sp_pitchAngleCoeff}, {"sp_velocAngleCoeff", &h.sp_velocAngleCoeff},
      {"spl_pitchVelocCoeff", &h.spl_pitchVelocCoeff}, {"spl_pitchAngleCoeff", &h.spl_pitchAngleCoeff}, {"spl_pvelocAngleCoeff", &h.spl_pvelocAngleCoeff},
      {"spl_verticalVelocCoeff", &h.spl_verticalVelocCoeff}, {"spl_lvelocAngleCoeff", &h.spl_lvelocAngleCoeff},
      {"braccelControlStiffness", &h.braccelControlStiffness}, {"tooCloseToTurn", &h.tooCloseToTurn}, {"tooCloseForWarp", &h.tooCloseForWarp},
      {"tooSlowToTurn", &h.tooSlowToTurn}, {"headingCloseEnoughForWarp", &h.headingCloseEnoughForWarp},
  };
  for (const Kv& e : kv) o->GetFloat(e.key, e.dst);
  if (!o->GetFloat("warpSpeed", &h.warpSpeed)) h.warpSpeed = 0;   // ohne Eintrag kein Warp
  return true;
}

void HoverShip::Init(const HoverOdf& odf, const ShipState& s) {
  cls = HoverClass::From(odf);
  st = HoverCraftState();
  st.Init(cls);
  st.pos[0] = s.pos[0]; st.pos[1] = s.pos[1]; st.pos[2] = -s.pos[2];
  st.pitch = 0; st.roll = 0;
  st.yaw = std::atan2(std::sin(s.yaw), -std::cos(s.yaw));   // Blick (sin y, 0, cos y)_GL = (sin y, 0, -cos y) in Originalkoordinaten
  st.BuildMatrix();
  for (int k = 0; k < 3; ++k) last_gl[k] = s.pos[k];
  ok = true;
}

HoverObstacle MakeHoverObstacle(const float p[3], const float v[3], float radius, bool mobile) {
  HoverObstacle o;
  o.pos[0] = p[0]; o.pos[1] = p[1]; o.pos[2] = -p[2];
  o.vel[0] = v[0]; o.vel[1] = v[1]; o.vel[2] = -v[2];
  o.speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  o.radius = radius;
  o.mobile = mobile;
  return o;
}

void HoverShipStep(HoverShip* h, ShipState* ss, bool can_fly, float dt, const std::vector<HoverObstacle>* obstacles, const PathGrid* grid) {
  HoverCraftState& s = h->st;
  // Wurde das Schiff von aussen versetzt (Start, Ankerpunkt, Laden), Lage uebernehmen
  const float jx = ss->pos[0] - h->last_gl[0], jy = ss->pos[1] - h->last_gl[1], jz = ss->pos[2] - h->last_gl[2];
  if (jx * jx + jy * jy + jz * jz > 1e-4f) { s.pos[0] = ss->pos[0]; s.pos[1] = ss->pos[1]; s.pos[2] = -ss->pos[2]; }
  s.powered = can_fly;
  if (!ss->has_target) h->path_goal[0] = 1e30f;
  HoverControls ctl;
  if (ss->has_target && can_fly) {
    const float goal[3] = {ss->target[0], ss->target[1], -ss->target[2]};
    float t[3] = {goal[0], goal[1], goal[2]};
    // Weg um gesperrte Felder (Asteroidenfelder): neu berechnen, wenn das Ziel wechselt; Wegpunkt weiter, wenn nahe genug
    if (grid && !grid->Empty()) {
      const bool new_goal = std::fabs(goal[0] - h->path_goal[0]) + std::fabs(goal[2] - h->path_goal[2]) > 1e-3f;
      if (new_goal) {
        h->path_goal[0] = goal[0]; h->path_goal[1] = goal[1]; h->path_goal[2] = goal[2];
        grid->FindPath({s.pos[0], s.pos[2]}, {goal[0], goal[2]}, &h->path);
        h->path_idx = 0;
      }
      constexpr float kPathLead = 150.0f;   // pathLeadDistance der Klassen-ODF (Vorgabe 150)
      while (h->path_idx + 1 < h->path.size()) {
        const float wx = h->path[h->path_idx].x - s.pos[0], wz = h->path[h->path_idx].z - s.pos[2];
        if (wx * wx + wz * wz >= kPathLead * kPathLead) break;
        ++h->path_idx;
      }
      if (h->path_idx < h->path.size()) { t[0] = h->path[h->path_idx].x; t[2] = h->path[h->path_idx].z; }
    }
    HoverAvoidArgs av;
    if (obstacles && !obstacles->empty()) { av.obstacles = obstacles->data(); av.count = static_cast<int>(obstacles->size()); }
    HoverGotoSteer(h->cls, s, &ctl, t, h->cls.warp > h->cls.impulse, &av);
    const float dx = goal[0] - s.pos[0], dy = goal[1] - s.pos[1], dz = goal[2] - s.pos[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) <= h->cls.too_close_to_turn && s.speed < 0.5f) ss->has_target = false;   // angekommen
  }
  HoverSimulate(h->cls, &s, ctl, dt);
  // zurueck ins Spiel (GL: z gespiegelt)
  ss->pos[0] = s.pos[0]; ss->pos[1] = s.pos[1]; ss->pos[2] = -s.pos[2];
  for (int k = 0; k < 3; ++k) h->last_gl[k] = ss->pos[k];
  ss->vel[0] = s.vel[0]; ss->vel[1] = s.vel[1]; ss->vel[2] = -s.vel[2];
  ss->speed = s.speed;
  ss->warp = ctl.c3 > 1.0f + 1e-3f;
  ss->b_front[0] = s.front[0]; ss->b_front[1] = s.front[1]; ss->b_front[2] = -s.front[2];
  ss->b_up[0] = s.up[0]; ss->b_up[1] = s.up[1]; ss->b_up[2] = -s.up[2];
  ss->has_basis = true;
  ss->yaw = std::atan2(ss->b_front[0], ss->b_front[2]);
  ss->pitch = std::asin(std::fmax(-1.0f, std::fmin(1.0f, ss->b_front[1])));
  ss->roll = 0;
}

}  // namespace fleet
