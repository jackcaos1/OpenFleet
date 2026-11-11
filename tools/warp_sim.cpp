// Flug eines Schiffs mit echter Physik-ODF: tools/warp_sim <odf> <entfernung> [yaw_grad]
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>
#include "game/HoverShip.h"
using namespace fleet;
int main(int c, char** v) {
  std::ifstream f(v[1]); std::stringstream ss; ss << f.rdbuf();
  OdfObject o; ParseOdfText(ss.str(), v[1], &o);
  HoverOdf h; HoverOdfFromObject(o, &h);
  printf("impulse %.1f warp %.1f tooCloseForWarp %.1f heading %.1f\n", h.impulseSpeed, h.warpSpeed, h.tooCloseForWarp, h.headingCloseEnoughForWarp);
  ShipState s; s.yaw = c > 3 ? std::atof(v[3]) * 3.14159265f / 180 : 0;
  HoverShip hs; hs.Init(h, s);
  s.has_target = true; s.target[0] = 0; s.target[1] = 0; s.target[2] = std::atof(v[2]);
  std::vector<HoverObstacle> obs;
  for (int k = 4; k + 2 < c; k += 3) { float p[3] = {(float)std::atof(v[k]), 0, (float)std::atof(v[k + 1])}, z[3] = {0, 0, 0}; obs.push_back(MakeHoverObstacle(p, z, (float)std::atof(v[k + 2]), true)); }
  for (int i = 0; i < 4000 && s.has_target; ++i) {
    HoverShipStep(&hs, &s, true, 0.05f, &obs);
    if (i % 100 == 0) printf("t %5.1f  pos %7.1f %6.1f %7.1f  tempo %5.1f %s yaw %6.1f\n", i * 0.05, s.pos[0], s.pos[1], s.pos[2], s.speed, s.warp ? "WARP" : "", s.yaw * 57.2958);
  }
}
