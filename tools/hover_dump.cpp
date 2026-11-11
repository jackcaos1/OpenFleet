// Gibt Instanzwerte und Zustandsfolge der C++-Portierung aus (Abgleich mit tools/hover_check.py).
#include <cstdio>
#include <cmath>
#include "game/HoverSim.h"
using namespace fleet;
int main() {
  HoverOdf o; o.impulseSpeed = 20; o.warpSpeed = 80; o.velocCoeffStill = 0.5f; o.velocWhenCoeffHalf = 0.7f;
  o.pitchRotationSpeed = 0.0667f; o.maxPitch = 45; o.rollRotationSpeed = 0.5f; o.maxRoll = 40; o.turnRadius = 30;
  o.frontBackInertia = 50; o.maxReverseSpeed = 4; o.upDownInertia = 7.5f; o.leftRightInertia = 7.5f; o.maxVerticalSpeed = 0.667f;
  o.rotationAcceleration = 0; o.rotationStiffness = 1;
  HoverClass c = HoverClass::From(o);
  HoverCraftState s; s.Init(c);
  s.pos[0] = 10; s.pos[1] = 5; s.pos[2] = -20; s.yaw = 0.3f; s.pitch = 0.05f; s.BuildMatrix();
  s.vel[0] = 2; s.vel[2] = 3; s.speed = std::sqrt(13.f);
  const char* names[] = {"k6a8","k6ac","k6b4","k6b8","k6c0","k6c8","k6cc","k6d4","k6dc","k6f0","k6fc","k700","k708","k710","k718"};
  const float* vals[] = {&s.k6a8,&s.k6ac,&s.k6b4,&s.k6b8,&s.k6c0,&s.k6c8,&s.k6cc,&s.k6d4,&s.k6dc,&s.k6f0,&s.k6fc,&s.k700,&s.k708,&s.k710,&s.k718};
  for (int i = 0; i < 15; ++i) printf("K %s %.9g\n", names[i], *vals[i]);
  { const char* qn[] = {"k724","k728","k72c","k730","k734","k738","k73c","k740","k744","k748","k74c","k750","k754","k758","k75c","k760","k764","k4b4","k4b8","warp_ratio"};
    const float* qv[] = {&s.k724,&s.k728,&s.k72c,&s.k730,&s.k734,&s.k738,&s.k73c,&s.k740,&s.k744,&s.k748,&s.k74c,&s.k750,&s.k754,&s.k758,&s.k75c,&s.k760,&s.k764,&s.k4b4,&s.k4b8,&s.warp_ratio};
    for (int i = 0; i < 20; ++i) printf("Q %s %.9g\n", qn[i], *qv[i]); }
  printf("S %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", s.pos[0], s.pos[1], s.pos[2], s.pitch, s.yaw, s.roll, s.vel[0], s.vel[2]);
  for (int i = 0; i < 80; ++i) {
    HoverControls ctl; ctl.c0 = std::sin(i * 0.21f); ctl.c1 = 0.5f * std::cos(i * 0.13f); ctl.c2 = (i % 17 < 4) ? 0.4f : 0; ctl.c3 = (i % 23 < 15) ? 1.0f : -0.3f;
    float dt = (i % 10 == 9) ? 0.4f : 0.05f;
    printf("C %.9g %.9g %.9g %.9g %.9g\n", ctl.c0, ctl.c1, ctl.c2, ctl.c3, dt);
    HoverSimulate(c, &s, ctl, dt);
    printf("R %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", s.pos[0], s.pos[1], s.pos[2], s.vel[0], s.vel[1], s.vel[2], s.pitch, s.yaw, s.roll, s.angvel[0], s.angvel[1], s.angvel[2]);
  }

  // Servo-Faelle
  unsigned seed = 12345;
  auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f * 2.0f - 1.0f; };
  for (int i = 0; i < 30; ++i) {
    HoverCraftState t; t.Init(c);
    t.pos[0] = rnd() * 50; t.pos[1] = rnd() * 20; t.pos[2] = rnd() * 50;
    t.pitch = rnd() * 0.5f; t.yaw = rnd() * 3.0f; t.roll = rnd() * 0.6f; t.BuildMatrix();
    for (int k = 0; k < 3; ++k) { t.vel[k] = rnd() * 15; t.angvel[k] = rnd() * 0.3f; }
    t.speed = std::sqrt(t.vel[0]*t.vel[0] + t.vel[1]*t.vel[1] + t.vel[2]*t.vel[2]);
    float arg = rnd() * 2.5f; float tp[3] = {rnd()*100, rnd()*30, rnd()*100}, tv[3] = {rnd()*5, 0, rnd()*5};
    HoverControls a, b, d, e, f, g; bool w = (i & 1);
    HoverServoYaw(t, &a, arg); HoverServoPitch(t, &b, arg * 0.4f); HoverServoPitchLift(t, &d, arg * 0.4f);
    HoverServoHeightPitch(t, &e, arg * 5); HoverGotoPD(c, t, &f, tp, tv, w);
    printf("V %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n", t.pos[0], t.pos[1], t.pos[2], t.pitch, t.yaw, t.roll,
           t.vel[0], t.vel[1], t.vel[2], t.angvel[0], t.angvel[1], t.angvel[2], arg, tp[0], tp[1], tp[2], tv[0], tv[2], (int)w);
    printf("W %.9g %.9g %.9g %.9g %.9g %.9g\n", a.c0, b.c1, d.c1, d.c2, e.c1, f.c3);
  }
}
