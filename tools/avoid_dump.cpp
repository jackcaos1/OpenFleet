// Zufallsszenen fuer HoverAvoid; Ausgabe wird von tools/avoid_check.py gegen das Original (Armada.exe) geprueft.
#include <cstdio>
#include <cmath>
#include <vector>
#include "game/HoverSim.h"
using namespace fleet;
int main() {
  unsigned seed = 777;
  auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f * 2.0f - 1.0f; };
  for (int sc = 0; sc < 60; ++sc) {
    HoverCraftState s; s.radius = 3.0f + 2.0f * std::fabs(rnd());
    s.pos[0] = rnd() * 50; s.pos[1] = rnd() * 10 + 10; s.pos[2] = rnd() * 50;
    s.speed = 12.0f + 10.0f * std::fabs(rnd());
    float fy = rnd() * 3.0f; s.front[0] = std::sin(fy); s.front[1] = 0; s.front[2] = std::cos(fy);
    float pr = rnd() * 3.0f; if (sc & 1) { s.prev_dir[0] = std::sin(pr); s.prev_dir[2] = std::cos(pr); }
    float d[3] = {rnd(), rnd() * 0.3f, rnd()}; float l = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]); float sc2 = (sc % 3 == 0) ? 0.6f : 1.0f;
    for (float& x : d) x = x / l * sc2;
    int n = 1 + (sc % 5);
    std::vector<HoverObstacle> obs(n);
    for (int i = 0; i < n; ++i) {
      HoverObstacle& o = obs[i];
      o.pos[0] = s.pos[0] + d[0] * 60 + rnd() * 40; o.pos[1] = s.pos[1] + rnd() * 10; o.pos[2] = s.pos[2] + d[2] * 60 + rnd() * 40;
      o.radius = 4 + 8 * std::fabs(rnd()); o.speed = (i % 3 == 0) ? 0.0f : 6.0f * std::fabs(rnd());
      o.mobile = (i % 3 != 0); o.no_dodge = (i == 4);
      if (o.mobile) { o.vel[0] = rnd() * 5; o.vel[2] = rnd() * 5; o.speed = std::sqrt(o.vel[0]*o.vel[0] + o.vel[2]*o.vel[2]); }
      if (i == 1) { o.has_parent = o.parent_active = true; o.parent_pos[0] = o.pos[0] + 80 * rnd(); o.parent_pos[1] = o.pos[1]; o.parent_pos[2] = o.pos[2] + 80 * rnd(); }
    }
    float prev[3] = {s.prev_dir[0], s.prev_dir[1], s.prev_dir[2]};
    HoverAvoidArgs a; a.obstacles = obs.data(); a.count = n;
    float out[3] = {d[0], d[1], d[2]};
    HoverAvoid(&s, a, out);
    printf("C %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n", s.pos[0], s.pos[1], s.pos[2], s.front[0], s.front[2], s.speed, s.radius,
           prev[0], prev[2], d[0], d[1], d[2], 0.0, 0.0, n);
    for (auto& o : obs)
      printf("O %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %d %d %.9g %.9g %.9g\n", o.pos[0], o.pos[1], o.pos[2], o.vel[0], o.vel[1], o.vel[2], o.speed, o.radius,
             (int)o.mobile, (int)o.no_dodge, (int)o.has_parent, o.parent_pos[0], o.parent_pos[1], o.parent_pos[2]);
    printf("R %.9g %.9g %.9g\n", out[0], out[1], out[2]);
  }
}
