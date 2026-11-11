#include "game/HoverSim.h"
#include <cmath>

namespace fleet {

static const float kPi = 3.14159274101257f;   // 0x5cbb50 (float)

HoverClass HoverClass::From(const HoverOdf& o) {
  float vwh = o.velocCoeffStill, pitchRot = o.pitchRotationSpeed, rollRot = o.rollRotationSpeed;
  (void)vwh;
  float s = o.impulseSpeed;
  float velocHalf = o.velocWhenCoeffHalf, fbi = o.frontBackInertia, maxRev = o.maxReverseSpeed;
  float udi = o.upDownInertia, maxVert = o.maxVerticalSpeed, lri = o.leftRightInertia;
  float maxFwd = 20.0f;   // Vorgabe 0x4a4
  if (s > 0) {            // @0x4557f5: Skalierung mit impulseSpeed
    velocHalf *= s; pitchRot *= s; rollRot *= s; fbi /= s; maxFwd = s; maxRev *= s; udi /= s; maxVert *= s; lri /= s;
  }
  HoverClass c{};
  c.impulse = o.impulseSpeed; c.warp = o.warpSpeed; c.maxFwd = maxFwd;
  c.c45c = static_cast<float>(1.0 / (1.0 - o.velocCoeffStill));
  c.c458 = static_cast<float>((2.0 - c.c45c) / velocHalf);
  double X = 1.0 - 1.0 / (c.c45c + c.c458 * maxFwd);
  c.c464 = pitchRot;
  c.c468 = static_cast<float>(static_cast<double>(pitchRot) * pitchRot * 0.25 / X);
  c.c470 = static_cast<float>(c.c468 * o.maxPitch * kPi * 0.005555555555555556);
  double R = static_cast<double>(kPi * o.maxRoll) * 0.0055555556900799274;   // float 1/180 (0x5cbb7c)
  c.c478 = rollRot;
  c.c47c = rollRot * rollRot * 0.25f;
  c.c484 = static_cast<float>(c.c47c * R / X);
  c.c48c = static_cast<float>(maxFwd / (o.turnRadius * R));
  c.c498 = o.rotationAcceleration * o.maxRotationSpeed;
  c.c4d0 = 2.0f * std::sqrt(o.rotationStiffness);
  c.c4a0 = 1.0f / fbi;
  c.c4ac = c.c4a0 * maxFwd;
  c.c4b0 = c.c4a0 * maxRev;
  c.c4b8 = 1.0f / udi;
  c.c4c0 = c.c4b8 * maxVert;
  c.c4c8 = 1.0f / lri;
  c.shp[0] = o.shp_verticalVelocCoeff; c.shp[1] = o.shp_verticalPosCoeff; c.shp[2] = o.shp_pitchVelocCoeff; c.shp[3] = o.shp_pitchAngleCoeff;
  c.sy[0] = o.sy_rollVelocCoeff; c.sy[1] = o.sy_rollAngleCoeff; c.sy[2] = o.sy_yawVelocCoeff; c.sy[3] = o.sy_yawAngleCoeff; c.sy[4] = o.sy_velocAngleCoeff;
  c.sp[0] = o.sp_pitchVelocCoeff; c.sp[1] = o.sp_pitchAngleCoeff; c.sp[2] = o.sp_velocAngleCoeff;
  c.spl[0] = o.spl_pitchVelocCoeff; c.spl[1] = o.spl_pitchAngleCoeff; c.spl[2] = o.spl_pvelocAngleCoeff;
  c.spl[3] = o.spl_verticalVelocCoeff; c.spl[4] = o.spl_lvelocAngleCoeff;
  c.turn_radius = o.turnRadius;
  c.braccel_k = o.braccelControlStiffness; c.braccel_c = 2.0f * std::sqrt(o.braccelControlStiffness);
  c.too_close_to_turn = o.tooCloseToTurn; c.too_close_for_warp = o.tooCloseForWarp; c.too_slow_to_turn = o.tooSlowToTurn;
  c.heading_ok_rad = static_cast<float>(o.headingCloseEnoughForWarp * static_cast<double>(kPi) * 0.005555555555555556);
  c.warp_ratio = (o.impulseSpeed > 0 && o.warpSpeed > 0) ? o.warpSpeed / o.impulseSpeed : 1.0f;   // Ctor @0x4618c5
  return c;
}

void HoverCraftState::Init(const HoverClass& c) {
  k6a8 = c.c458; k6ac = c.c45c;
  coeff774 = static_cast<float>(1.0 - 1.0 / c.c45c);
  k6b4 = c.c464; k6b8 = c.c468; k6c0 = c.c470; k6c8 = c.c478; k6cc = c.c47c; k6d4 = c.c484; k6dc = c.c48c;
  k6f0 = c.c4a0; k6fc = c.c4ac; k700 = c.c4b0; k708 = c.c4b8; k710 = c.c4c0; k718 = c.c4c8;
  cap_speed = c.impulse;
  k724 = c.shp[0]; k728 = c.shp[1]; k72c = c.shp[2]; k730 = c.shp[3];
  k734 = c.sy[0]; k738 = c.sy[1]; k73c = c.sy[2]; k740 = c.sy[3]; k744 = c.sy[4];
  k748 = c.sp[0]; k74c = c.sp[1]; k750 = c.sp[2];
  k754 = c.spl[0]; k758 = c.spl[1]; k75c = c.spl[2]; k760 = c.spl[3]; k764 = c.spl[4];
  k4b4 = c.braccel_k; k4b8 = c.braccel_c; warp_ratio = c.warp_ratio;
}

void HoverCraftState::SetSpeedCap(float s) { k6fc = s / cap_speed * k6fc; cap_speed = s; }

void HoverCraftState::SetFromMatrix() {   // @0x458ea0
  if (front[1] > 0.999) { pitch = kPi * -0.5f; roll = 0; yaw = std::atan2(-right[2], right[0]); return; }
  if (front[1] < -0.999) { pitch = kPi * 0.5f; roll = 0; yaw = std::atan2(-right[2], right[0]); return; }
  pitch = -std::asin(front[1]);
  yaw = std::atan2(front[0], front[2]);
  roll = std::atan2(right[1], up[1]);
}

void HoverCraftState::BuildMatrix() {   // @0x55d080
  float sr = std::sin(roll), cr = std::cos(roll), sp = std::sin(pitch), cp = std::cos(pitch), sy = std::sin(yaw), cy = std::cos(yaw);
  right[0] = cr * cy + sr * sy * sp; right[1] = cp * sr; right[2] = cy * sr * sp - cr * sy;
  up[0] = cr * sy * sp - cy * sr; up[1] = cp * cr; up[2] = cr * cy * sp + sr * sy;
  front[0] = cp * sy; front[1] = -sp; front[2] = cp * cy;
}

static float Dot(const float* a, const float* b) { return a[2] * b[2] + a[1] * b[1] + a[0] * b[0]; }
static float Len(const float* a) { return std::sqrt(a[2] * a[2] + a[1] * a[1] + a[0] * a[0]); }


static float Clamp1(float v) { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); }
static float WrapPi(float e) {
  if (!(e <= kPi)) e -= 2.0f * kPi;
  if (e < -kPi) e += 2.0f * kPi;
  return e;
}

float HoverSetThrottle(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, float accel, bool warp) {
  (void)c;
  float maxc = warp ? s.warp_ratio : 1.0f;
  float v = Dot(s.front, s.vel) * s.k6f0 + accel;
  v = (v > 0) ? v / s.k6fc : v / s.k700;
  if (v < -maxc) v = -maxc; else if (!(v <= maxc)) v = maxc;
  ctl->c3 = v;
  return v;
}

void HoverSetVertical(const HoverCraftState& s, HoverControls* ctl, float ty) { ctl->c2 = Clamp1(ty - s.pos[1]); }

void HoverServoHeightPitch(const HoverCraftState& s, HoverControls* ctl, float ty) {
  float v = (s.pos[1] - ty) * s.k728 + s.k72c * s.angvel[0] + s.k724 * s.vel[1] + s.k730 * s.pitch;
  ctl->c1 = Clamp1(v);
}

void HoverServoYaw(const HoverCraftState& s, HoverControls* ctl, float ty) {
  double e = static_cast<double>(s.yaw) - ty;
  if (!(e <= kPi)) e -= 2.0 * kPi;
  if (e < -kPi) e += 2.0 * kPi;
  float v = s.k73c * s.angvel[1] + s.k738 * s.roll + s.k734 * s.angvel[2] + s.k740 * static_cast<float>(e);
  ctl->c0 = Clamp1(v);
}

static float PathAngleErr(const HoverCraftState& s, float tp) {
  if (s.speed < 0.0001) return 0.0f;
  float r = s.vel[1] / s.speed; if (r > 1) r = 1; if (r < -1) r = -1;
  return std::asin(r) - tp;
}

void HoverServoPitch(const HoverCraftState& s, HoverControls* ctl, float tp) {
  float X = PathAngleErr(s, tp);
  float v = (-s.pitch - tp) * s.k74c + s.k748 * s.angvel[0] + (s.k6b8 / s.k6c0) * tp + s.k750 * X;
  ctl->c1 = Clamp1(v);
}

void HoverServoPitchLift(const HoverCraftState& s, HoverControls* ctl, float tp) {
  float X = PathAngleErr(s, tp);
  float v = (-s.pitch - tp) * s.k758 + s.k754 * s.angvel[0] + (s.k6b8 / s.k6c0) * tp + s.k75c * X;
  ctl->c1 = Clamp1(v);
  ctl->c2 = Clamp1(Dot(s.up, s.vel) * s.k760 + s.k764 * X);
}

void HoverGotoPD(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, const float t[3], const float tv[3], bool warp) {
  (void)c;
  float dv[3] = {tv[0] - s.vel[0], tv[1] - s.vel[1], tv[2] - s.vel[2]};
  float dp[3] = {t[0] - s.pos[0], t[1] - s.pos[1], t[2] - s.pos[2]};
  float a = Dot(s.front, dp) * s.k4b4 + Dot(s.front, dv) * s.k4b8;
  HoverSetThrottle(c, s, ctl, a, warp);
}

void HoverAimDirection(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, const float d[3], float warp_scale) {
  (void)c;
  float len = Len(d);
  float use;
  if (len < 0.001) { len = 0.001f; use = len; }
  else if (len < -1.0f) use = -1.0f;
  else use = len <= 1.0f ? len : 1.0f;
  ctl->c3 = use * warp_scale;
  float r = d[1] / len; if (r > 1) r = 1; if (r < -1) r = -1;
  float pitch_t = std::asin(r);
  float yaw_t = std::atan2(d[0], d[2]);
  HoverServoPitch(s, ctl, pitch_t);
  HoverServoYaw(s, ctl, yaw_t);
  ctl->c2 = 0;
}


// ---------------------------------------------------------------------------------------------
// Hindernisvermeidung (Kontext-Steuerung mit 10 Kandidatenrichtungen), Original @0x42ef70
namespace {
struct ApproachResult { float t, miss2, dd; };

// @0x424860: Annaeherung von craft (Wunschgeschwindigkeit vA) an Objekt o. t = Zeitpunkt der kleinsten Distanz.
ApproachResult ClosestApproach(const HoverCraftState& a, const float vA[3], const HoverObstacle& o) {
  float rel[3] = {a.pos[0] - o.pos[0], a.pos[1] - o.pos[1], a.pos[2] - o.pos[2]};
  float v[3] = {vA[0], vA[1], vA[2]};
  if (o.mobile && !o.no_dodge) {
    if (o.has_parent && o.parent_active) {
      float q[3] = {o.parent_pos[0] - o.pos[0], o.parent_pos[1] - o.pos[1], o.parent_pos[2] - o.pos[2]};
      double q2 = static_cast<double>(q[2]) * q[2] + static_cast<double>(q[1]) * q[1] + static_cast<double>(q[0]) * q[0];
      if (q2 > 1.0) {                                // Objekt steuert auf sein Elternobjekt zu
        float tmp[3] = {o.vel[0] * 0.2f, o.vel[1] * 0.2f, o.vel[2] * 0.2f};
        float inv = static_cast<float>(1.0 / std::sqrt(q2));
        float f = static_cast<float>((1.0 - static_cast<double>(0.2f)) * o.speed);
        for (int i = 0; i < 3; ++i) v[i] = v[i] - ((q[i] * inv) * f + tmp[i]);
      }
    } else {
      for (int i = 0; i < 3; ++i) v[i] -= o.vel[i];
    }
  }
  double vv = static_cast<double>(v[2]) * v[2] + static_cast<double>(v[1]) * v[1] + static_cast<double>(v[0]) * v[0];
  if (vv < 0.001) {
    float r2 = rel[2] * rel[2] + rel[1] * rel[1] + rel[0] * rel[0];
    return {0.0f, r2, 0.0f};
  }
  double dd = -(static_cast<double>(v[2]) * rel[2] + static_cast<double>(v[1]) * rel[1] + static_cast<double>(v[0]) * rel[0]);
  double t = dd / vv;
  float m[3] = {static_cast<float>(rel[0] + v[0] * t), static_cast<float>(rel[1] + v[1] * t), static_cast<float>(rel[2] + v[2] * t)};
  float m2 = m[2] * m[2] + m[1] * m[1] + m[0] * m[0];
  return {static_cast<float>(t), m2, static_cast<float>(dd)};
}

struct AvoidCtx {
  float score[10];
  float dir[10][3];
  float vel[10][3];
  bool init = false;
  AvoidCtx() { for (float& x : score) x = 0; }
};

// @0x424ba0: 10 Richtungen um die Wunschrichtung d (Einheitsvektor): 0 = d, 1/2 = +-15 Grad, 3/4 = +-30, 5/6 = +-60 (Kurs),
// 7 = Vorausrichtung, 8/9 = Neigung +-15 Grad. Startwert Score = 100*(dir.d).
void BuildRosette(AvoidCtx* c, const HoverCraftState& cr, const float d[3]) {
  const float a = static_cast<float>(static_cast<double>(kPi) * 0.08333333333333334);
  struct SC { float s, c; };
  auto sincosf_ = [](float x) { return SC{static_cast<float>(std::sin(static_cast<double>(x))), static_cast<float>(std::cos(static_cast<double>(x)))}; };
  SC p1 = sincosf_(a), p2 = sincosf_(a + a), p4 = sincosf_(a * 4.0f);
  c->init = true;
  for (int i = 0; i < 3; ++i) { c->dir[0][i] = d[i]; c->dir[7][i] = cr.front[i]; }
  const SC ang[3] = {p1, p2, p4};
  for (int k = 0; k < 3; ++k) {
    float kk = 1.0f - ang[k].c;
    float ky = kk * d[1];
    float Mx = d[0] * ang[k].c;
    float My = static_cast<float>(d[1] * ang[k].c) + ky;
    float Mz = d[2] * ang[k].c;
    float wx = ang[k].s * d[2], wz = -(ang[k].s * d[0]);
    float* m = c->dir[1 + 2 * k]; m[0] = Mx - wx; m[1] = My; m[2] = Mz - wz;
    float* p = c->dir[2 + 2 * k]; p[0] = Mx + wx; p[1] = My; p[2] = Mz + wz;
  }
  // Neigung: u = (-dx*dy, dx^2+dz^2, -dz*dy); dir = d*cos(a) +- sin(a)*u
  float ux = -(d[0] * d[1]), uy = d[0] * d[0] - (-d[2] * d[2]), uz = -d[2] * d[1];
  float us[3] = {ux * p1.s, uy * p1.s, uz * p1.s};
  float pd = ((d[0] * d[2] + 0.0f * d[1]) + (-d[2] * d[0])) * (1.0f - p1.c);
  float pv[3] = {-d[2] * pd, 0.0f * pd, d[0] * pd};
  for (int i = 0; i < 3; ++i) {
    float R = pv[i] + d[i] * p1.c;
    c->dir[8][i] = us[i] + R;
    c->dir[9][i] = R - us[i];
  }
  for (int i = 0; i < 10; ++i)
    c->score[i] = (c->dir[i][0] * d[0] + c->dir[i][2] * d[2] + c->dir[i][1] * d[1]) * 100.0f;
  float m = cr.speed < 10.0f ? 10.0f : cr.speed;
  for (int i = 0; i < 10; ++i) for (int k = 0; k < 3; ++k) c->vel[i][k] = m * c->dir[i][k];
}

// @0x4250e0: Gefahr durch Objekt o in alle Richtungen eintragen
void Accumulate(AvoidCtx* c, const HoverCraftState& cr, const HoverObstacle& o) {
  for (int i = 0; i < 10; ++i) {
    ApproachResult r = ClosestApproach(cr, c->vel[i], o);
    float term = 0.0f;
    if (!(r.t < 0.0f)) {
      float sr = (o.radius + cr.radius) * 1.1f;
      float cc = (o.speed < 1.0f) ? 2.0f : 1.0f;
      if (sr * sr > r.miss2) term = (-100.0f * cc * sr * sr) / r.miss2;
    }
    c->score[i] += term;
  }
  if (o.no_dodge) c->score[8] = c->score[9] = -100000.0f;   // Original: nur wenn dynamic_cast zur Zielklasse klappt (hier angenommen)
}

// @0x424b50: Vorliebe fuer die bisherige Richtung
void WeightPrev(AvoidCtx* c, const float v[3]) {
  for (int i = 0; i < 10; ++i) {
    float dot = c->dir[i][0] * v[0] + c->dir[i][2] * v[2] + c->dir[i][1] * v[1];
    if (dot > 0.0f) c->score[i] += 50.0f * dot;
  }
}

// @0x425200: beste Richtung (hoechster Score); ist selbst der beste Score negativ, nur 75 % Schub
void Choose(const AvoidCtx& c, float out[3]) {
  float best = -1.0000000150474662e+30f; int idx = 0;
  for (int i = 0; i < 10; ++i) if (best < c.score[i]) { best = c.score[i]; idx = i; }
  float f = best < 0.0f ? 0.75f : 1.0f;
  for (int k = 0; k < 3; ++k) out[k] = f * c.dir[idx][k];
}
}  // namespace

void HoverAvoid(HoverCraftState* sp, const HoverAvoidArgs& a, float dir[3]) {
  HoverCraftState& s = *sp;
  const float d[3] = {dir[0], dir[1], dir[2]};
  const float R = s.speed * 4.0f;
  float lim = R;
  if (a.target_pos) {
    float dx = a.target_pos[0] - s.pos[0], dy = a.target_pos[1] - s.pos[1], dz = a.target_pos[2] - s.pos[2];
    float d2 = dx * dx + dy * dy + dz * dz;
    float dist = d2 <= 0 ? 0.0f : std::sqrt(d2);
    float v = dist - s.radius;
    lim = v < R ? v : R;
  }
  AvoidCtx ctx;
  const float myvel[3] = {d[0] * s.speed, d[1] * s.speed, d[2] * s.speed};
  for (int i = 0; i < a.count; ++i) {
    const HoverObstacle& o = a.obstacles[i];
    if (!o.collidable || &o == a.target_obj || !o.active || o.hidden) continue;
    if (o.special) continue;                                   // 0x8000-Objekte: @0x42ed20 nicht portiert
    if (o.priority < s.priority && !(o.speed <= 1.0f)) continue;  // niedrigere Prioritaet nur wenn (fast) ruhend
    float d2 = (o.pos[0] - s.pos[0]) * (o.pos[0] - s.pos[0]) + (o.pos[1] - s.pos[1]) * (o.pos[1] - s.pos[1]) + (o.pos[2] - s.pos[2]) * (o.pos[2] - s.pos[2]);
    float dist = d2 <= 0 ? 0.0f : std::sqrt(d2);
    if ((dist - s.radius) - o.radius > lim) continue;
    ApproachResult r = ClosestApproach(s, myvel, o);
    if (r.t <= 0.0f || r.t > 4.0f) continue;
    if (o.mobile && a.proc_group != a.invalid_group && o.has_parent) {
      if (a.proc_group == o.group || a.proc_group == o.parent_group) continue;
      if (o.parent_group == s.group) continue;
    }
    if (!ctx.init) {
      double l2 = static_cast<double>(d[2]) * d[2] + static_cast<double>(d[1]) * d[1] + static_cast<double>(d[0]) * d[0];
      float inv = static_cast<float>(1.0 / std::sqrt(l2));
      float dn[3] = {d[0] * inv, d[1] * inv, d[2] * inv};
      BuildRosette(&ctx, s, dn);
    }
    Accumulate(&ctx, s, o);
  }
  if (ctx.init) {
    float out[3];
    double l2 = static_cast<double>(s.prev_dir[2]) * s.prev_dir[2] + static_cast<double>(s.prev_dir[1]) * s.prev_dir[1] + static_cast<double>(s.prev_dir[0]) * s.prev_dir[0];
    float inv = static_cast<float>(1.0 / std::sqrt(l2));
    float pv[3] = {s.prev_dir[0] * inv, s.prev_dir[1] * inv, s.prev_dir[2] * inv};
    WeightPrev(&ctx, pv);
    Choose(ctx, out);
    for (int k = 0; k < 3; ++k) dir[k] = out[k];
  }
  for (int k = 0; k < 3; ++k) s.prev_dir[k] = dir[k];
  if (s.pos[1] > HoverWorld().ceiling && dir[1] > 0) dir[1] = 0;
  if (s.pos[1] < HoverWorld().floor && dir[1] < 0) dir[1] = 0;
}

HoverWorldCfg& HoverWorld() { static HoverWorldCfg w; return w; }

void HoverGotoSteer(const HoverClass& c, HoverCraftState& s, HoverControls* ctl, const float t[3], bool allow_warp, const HoverAvoidArgs* avoid) {
  const float zero[3] = {0, 0, 0};
  float dx = t[0] - s.pos[0], dy = t[1] - s.pos[1], dz = t[2] - s.pos[2];
  float d2 = dx * dx + dy * dy + dz * dz;
  if (d2 < c.too_close_for_warp * c.too_close_for_warp) allow_warp = false;
  float dist = std::sqrt(d2);
  if (dist <= c.too_close_to_turn) {                 // zu nah: nicht mehr wenden, nur Schub regeln
    ctl->c0 = ctl->c1 = ctl->c2 = 0;
    HoverGotoPD(c, s, ctl, t, zero, allow_warp);
    return;
  }
  // Wendekreis-Test (@0x42e8e6)
  float fx = s.front[0], fz = s.front[2];
  float inv = 1.0f / std::sqrt(fz * fz + fx * fx);
  float nx = fz * inv, nz = -fx * inv;
  float R = c.turn_radius;
  float side = nx * dx + dz * nz;
  float ex, ez;
  if (side > 0) { ex = dx - nx * R; ez = dz - nz * R; } else { ex = dx + nx * R; ez = dz + nz * R; }
  if (R * R > ex * ex + ez * ez) {                    // Ziel liegt im Wendekreis: geradeaus/abbremsen
    ctl->c0 = ctl->c1 = ctl->c2 = 0;
    HoverGotoPD(c, s, ctl, t, zero, allow_warp);
    return;
  }
  // Richtung (@0x42ebb0): Einheitsvektor, bei Entfernung < 20 m anteilig; Hoehengrenzen (Boden/Decke aus RTS_CFG.h)
  float dir[3];
  if (dist < 0.001) dir[0] = dir[1] = dir[2] = 0;
  else { float k = dist > 20.0f ? dist : 20.0f; dir[0] = dx / k; dir[1] = dy / k; dir[2] = dz / k; }
  if (avoid && HoverWorld().obstacle_avoidance) HoverAvoid(&s, *avoid, dir);   // enthaelt auch die Hoehengrenzen
  else {                                                                       // @0x42f66c: nur Hoehengrenzen
    if (s.pos[1] > HoverWorld().ceiling && dir[1] > 0) dir[1] = 0;
    if (s.pos[1] < HoverWorld().floor && dir[1] < 0) dir[1] = 0;
  }
  // @0x42f9b0: Warp nur wenn der Kurs ungefaehr stimmt
  float yaw_t = std::atan2(dir[0], dir[2]);
  float scale = 1.0f;
  if (allow_warp) {
    float err = std::fabs(yaw_t - s.yaw);
    if (!(err <= 2.0f * kPi)) err -= 2.0f * kPi;
    if (!(err <= kPi)) err = 2.0f * kPi - err;
    if (err < c.heading_ok_rad) scale = s.warp_ratio;
  }
  HoverAimDirection(c, s, ctl, dir, scale);
  // Liegt das Ziel innerhalb von 45 Grad voraus, regelt der PD-Regler den Schub (@0x42eb46)
  float dl = std::sqrt(dx * dx + dz * dz), fl = std::sqrt(fx * fx + fz * fz);
  if (fx * dx + fz * dz > 0.707 * fl * dl) HoverGotoPD(c, s, ctl, t, zero, allow_warp);
}

void HoverSimulate(const HoverClass& c, HoverCraftState* st, const HoverControls& ctl, float dt) {
  HoverCraftState& s = *st;
  (void)c;
  if (s.dead || s.borg) return;   // Borg-Zweig (0x459a99) noch nicht portiert
  // Teilschritte
  int n = 1;
  {
    float wl = Len(s.angvel);
    float a = 0.1f / wl, b = 4.0f / s.speed;
    float m = b < a ? b : a;
    if (m < 0.05f) m = 0.05f;
    if (dt > m) { n = static_cast<int>(std::ceil(dt / m)); dt = dt / static_cast<float>(n); }
  }
  bool yaw_from_roll = s.powered && !s.hold && !s.borg;
  const float dtl = dt * 0.5f;
  for (int it = 0; it < n; ++it) {
    float invdt = (dt < 0.001) ? 1000.0f : 1.0f / dt;
    if (!s.dead && s.powered && !s.hold) {
      // Wendigkeit
      float d = Dot(s.up, s.vel);
      float vp[3] = {s.vel[0] - d * s.up[0], s.vel[1] - d * s.up[1], s.vel[2] - d * s.up[2]};
      float lp = Len(vp);
      s.coeff774 = static_cast<float>(1.0 - 1.0 / (lp * s.k6a8 + s.k6ac));
      float gp = (dt * s.k6b4 < 1.0f) ? s.k6b4 : invdt;
      s.angacc[0] = -(gp * s.angvel[0]);
      s.angacc[0] = s.angacc[0] - ctl.c1 * s.k6c0 * s.coeff774;
      s.angacc[0] = s.angacc[0] - s.k6b8 * s.coeff774 * s.pitch;
      float gr = (dt * s.k6c8 < 1.0f) ? s.k6c8 : invdt;
      s.angacc[2] = -(gr * s.angvel[2]);
      s.angacc[1] = 0;
      s.angacc[2] = s.angacc[2] + s.k6d4 * ctl.c0 * s.coeff774;
      s.angacc[2] = s.angacc[2] - s.k6cc * s.roll;
      float p = Dot(s.front, s.vel);
      float m;
      if (ctl.c3 > 0) m = s.k6fc;
      else if (p > 0) m = s.k700;
      else m = 0;
      m *= s.speed_mult;
      m *= ctl.c3;
      float x;
      if (s.scale_b * s.k6f0 < m * s.scale_a) {
        if (s.scale_b < 0.01) s.scale_b = 0.01f;
        x = m / s.scale_b;
      } else {
        x = s.k6f0 / s.scale_a;
      }
      float a = m - x * p;
      if (-(a * dt) > p) a = -(p * invdt);
      float lat = Dot(s.right, s.vel) * s.k718;
      float vv = ctl.c2 * s.k710 - Dot(s.up, s.vel) * s.k708;
      for (int i = 0; i < 3; ++i) s.accel[i] = (s.front[i] * a + s.up[i] * vv) - s.right[i] * lat;
    } else {
      // abgeschaltet: Winkelbeschleunigung 0, lineares Abbremsen mit 2 m/s^2
      s.angacc[0] = s.angacc[1] = s.angacc[2] = 0;
      float l2 = s.vel[0] * s.vel[0] + s.vel[1] * s.vel[1] + s.vel[2] * s.vel[2];
      if (l2 < 0.01) { s.accel[0] = s.accel[1] = s.accel[2] = 0; }
      else { float inv = 1.0f / std::sqrt(l2); for (int i = 0; i < 3; ++i) s.accel[i] = s.vel[i] * inv * -2.0f; }
    }
    for (int i = 0; i < 3; ++i) s.angvel[i] += dtl * s.angacc[i];
    if (yaw_from_roll) s.angvel[1] = -(s.k6dc * s.roll);
    s.pitch += dt * s.angvel[0]; s.yaw += dt * s.angvel[1]; s.roll += dt * s.angvel[2];
    for (int i = 0; i < 3; ++i) s.angvel[i] += dtl * s.angacc[i];   // zweiter Halbschritt (@0x459df1)
    for (int i = 0; i < 3; ++i) s.vel[i] += dtl * s.accel[i];
    if (s.pitch < -1.5) s.pitch = -1.5f;
    if (s.pitch > 1.5) s.pitch = 1.5f;
    if (!(-kPi <= s.roll)) s.roll = 2.0f * kPi + s.roll;
    if (!(s.roll <= kPi)) s.roll = s.roll - 2.0f * kPi;
    float np[3] = {dt * s.vel[0] + s.pos[0], dt * s.vel[1] + s.pos[1], dt * s.vel[2] + s.pos[2]};
    s.BuildMatrix();
    for (int i = 0; i < 3; ++i) s.pos[i] = np[i];
    for (int i = 0; i < 3; ++i) s.vel[i] += dtl * s.accel[i];
    s.speed = Len(s.vel);
  }
  if (!(-kPi <= s.yaw)) s.yaw = 2.0f * kPi + s.yaw;
  if (!(s.yaw <= kPi)) s.yaw = s.yaw - 2.0f * kPi;
}

}  // namespace fleet
