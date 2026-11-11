#include "game/ShipSim.h"

#include <algorithm>
#include <cmath>

namespace fleet {

namespace {

const float kPi = 3.14159265358979f;

float Wrap(float a) {
  while (a > kPi) a -= 2 * kPi;
  while (a < -kPi) a += 2 * kPi;
  return a;
}

float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Annaeherung mit Zeitkonstante tau (exakt, unabhaengig von dt).
float Approach(float cur, float want, float tau, float dt) {
  if (tau <= 1e-4f) return want;
  return cur + (want - cur) * (1.0f - std::exp(-dt / tau));
}

}  // namespace

FlightParams FlightFromPhysics(const PhysicsDef& p) {
  FlightParams f;
  if (p.impulse_speed <= 0) return f;
  f.valid = true;
  f.impulse = p.impulse_speed;
  f.warp = p.warp_speed > 0 ? p.warp_speed : p.impulse_speed;
  if (p.turn_radius > 0) f.turn_radius = p.turn_radius;
  if (p.max_roll > 0) f.max_roll = p.max_roll;
  if (p.max_pitch > 0) f.max_pitch = p.max_pitch;
  if (p.pitch_rate > 0) f.pitch_rate = p.pitch_rate;
  if (p.roll_rate > 0) f.roll_rate = p.roll_rate;
  if (p.front_back_inertia > 0) f.front_back_inertia = p.front_back_inertia;
  if (p.side_inertia > 0) f.side_inertia = p.side_inertia;
  if (p.max_reverse > 0) f.max_reverse = p.max_reverse;
  if (p.veloc_coeff_still > 0) f.veloc_coeff_still = p.veloc_coeff_still;
  if (p.veloc_half > 0) f.veloc_half = p.veloc_half;
  if (p.heading_ok_for_warp > 0) f.heading_ok_for_warp = p.heading_ok_for_warp;
  if (p.too_close_for_warp > 0) f.too_close_for_warp = p.too_close_for_warp;
  if (p.too_close_to_turn > 0) f.too_close_to_turn = p.too_close_to_turn;
  if (p.max_rotation_speed > 0 && p.turn_radius <= 0) f.turn_radius = f.impulse / p.max_rotation_speed;  // Borg: Drehrate statt Radius
  return f;
}

void ShipForward(const ShipState& s, float out[3]) {
  const float cp = std::cos(s.pitch);
  out[0] = std::sin(s.yaw) * cp;
  out[1] = std::sin(s.pitch);
  out[2] = std::cos(s.yaw) * cp;
}

void SetShipTarget(ShipState* s, const float p[3]) {
  s->has_target = true;
  for (int k = 0; k < 3; ++k) s->target[k] = p[k];
}

void StopShip(ShipState* s) { s->has_target = false; }

void ShipFromForward(const float fwd[3], ShipState* s) {
  s->yaw = std::atan2(fwd[0], fwd[2]);
  s->pitch = 0;
  s->roll = 0;
}

void StepShip(const FlightParams& fp, ShipState* s, float dt) {
  if (dt <= 0) return;
  const float tau_fb = fp.front_back_inertia * kInertiaSeconds;
  const float tau_side = fp.side_inertia * kInertiaSeconds;
  const float omega_max = fp.impulse / std::max(1.0f, fp.turn_radius);  // rad/s bei Impulsgeschwindigkeit
  float want_speed = 0, want_yaw_rate = 0, want_pitch = 0;
  s->warp = false;
  if (s->has_target) {
    const float dx = s->target[0] - s->pos[0], dy = s->target[1] - s->pos[1], dz = s->target[2] - s->pos[2];
    const float dh = std::sqrt(dx * dx + dz * dz), dist = std::sqrt(dh * dh + dy * dy);
    if (dist < fp.too_close_to_turn) {
      s->has_target = false;  // angekommen: nicht mehr wenden, ausrollen
    } else {
      const float err = Wrap(std::atan2(dx, dz) - s->yaw);
      // Wendigkeit waechst mit der Geschwindigkeit (velocCoeffStill ... 1), siehe battphys.odf
      const float v = std::fabs(s->speed);
      const float coeff = fp.veloc_coeff_still + (1.0f - fp.veloc_coeff_still) * v / (v + fp.veloc_half);
      const float lim = omega_max * coeff;
      want_yaw_rate = Clamp(2.5f * err, -lim, lim);
      const bool ahead = std::fabs(err) < fp.heading_ok_for_warp * kPi / 180.0f;
      float cruise = fp.impulse;
      if (ahead && dist > fp.too_close_for_warp && fp.warp > fp.impulse) {
        cruise = fp.warp;
        s->warp = true;
      }
      // Bremsen: Ausrollweg ~ speed * tau; rechtzeitig langsamer werden, ohne vor dem Ziel liegen zu bleiben
      const float brake = std::max(0.0f, dist - fp.too_close_to_turn) / std::max(0.2f, 1.2f * tau_fb);
      want_speed = std::min(cruise, brake + 2.0f);
      if (std::fabs(err) > 1.2f) want_speed = std::min(want_speed, fp.impulse * 0.5f);  // erst eindrehen
      want_pitch = Clamp(std::atan2(dy, std::max(1.0f, dh)), -fp.max_pitch * kPi / 180.0f, fp.max_pitch * kPi / 180.0f);
    }
  }
  s->speed = Approach(s->speed, want_speed, tau_fb, dt);
  if (!s->has_target && std::fabs(s->speed) < 0.05f) s->speed = 0;
  // Kurs
  s->yaw_rate = Approach(s->yaw_rate, want_yaw_rate, 0.25f, dt);
  s->yaw = Wrap(s->yaw + s->yaw_rate * dt);
  // Neigung
  const float max_pitch_step = fp.pitch_rate * dt;
  s->pitch += Clamp(want_pitch - s->pitch, -max_pitch_step, max_pitch_step);
  // Rollen in die Kurve
  const float want_roll = fp.max_roll * kPi / 180.0f * Clamp(s->yaw_rate / std::max(1e-3f, omega_max), -1.0f, 1.0f);
  const float max_roll_step = std::max(fp.roll_rate, 0.2f) * dt;
  s->roll += Clamp(want_roll - s->roll, -max_roll_step, max_roll_step);
  // Bewegung: Geschwindigkeit rutscht dem Bug nach (Seitenrutschen), Position folgt
  float f[3];
  ShipForward(*s, f);
  for (int k = 0; k < 3; ++k) {
    s->vel[k] = Approach(s->vel[k], f[k] * s->speed, tau_side, dt);
    s->pos[k] += s->vel[k] * dt;
  }
}

void ShipMatrices(const ShipState& s, float scale, Mat4* model, Mat4* rot) {
  // Der Bug des Modells zeigt in -z (GL): Ry(yaw+pi) dreht ihn auf (sin yaw, 0, cos yaw).
  Mat4 r = Mat4::RotationY(s.yaw + kPi) * Mat4::RotationX(s.pitch) * Mat4::RotationZ(s.roll);
  if (s.has_basis) {
    // Spalten: Modell-x = Bug x Oben (rechts), Modell-y = Oben, Modell-z = -Bug
    const float* f = s.b_front; const float* u = s.b_up;
    const float rx = f[1] * u[2] - f[2] * u[1], ry = f[2] * u[0] - f[0] * u[2], rz = f[0] * u[1] - f[1] * u[0];
    r = Mat4();
    r.m[0] = rx; r.m[1] = ry; r.m[2] = rz;
    r.m[4] = u[0]; r.m[5] = u[1]; r.m[6] = u[2];
    r.m[8] = -f[0]; r.m[9] = -f[1]; r.m[10] = -f[2];
  }
  *rot = r;
  Mat4 m = r;
  for (int i : {0, 1, 2, 4, 5, 6, 8, 9, 10}) m.m[i] *= scale;
  m.m[12] = s.pos[0];
  m.m[13] = s.pos[1];
  m.m[14] = s.pos[2];
  *model = m;
}

}  // namespace fleet
