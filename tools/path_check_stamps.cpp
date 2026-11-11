// g++ -O1 -std=c++17 -w -I../engine/src path_check_stamps.cpp ../engine/src/game/PathGrid.cpp -o /tmp/pcs ; /tmp/pcs datei.txt
#include <cmath>
#include <cstdio>
#include <vector>
#include "game/PathGrid.h"
using namespace fleet;
int main(int, char** argv) {
  FILE* f = fopen(argv[1], "r"); int n, ok = 0, tot = 0;
  while (fscanf(f, "%d", &n) == 1) {
    PathGrid g; g.Init(-1500, -1500, 1500, 1500, 50);
    for (int i = 0; i < n; ++i) { float x, z, x1, z1, r; int c; fscanf(f, "%f %f %f %f %f %d", &x, &z, &x1, &z1, &r, &c); g.StampBox(x, z, x1, z1, r, c); }
    float sx, sz, gx, gz; int m; fscanf(f, "%f %f %f %f %d", &sx, &sz, &gx, &gz, &m);
    std::vector<PathPoint> want(static_cast<size_t>(m));
    for (int i = 0; i < m; ++i) fscanf(f, "%f %f", &want[static_cast<size_t>(i)].x, &want[static_cast<size_t>(i)].z);
    std::vector<PathPoint> p; const bool found = g.FindPath({sx, sz}, {gx, gz}, &p);
    ++tot; bool good = (m == 0) ? !found : (found && p.size() + 1 == want.size());
    for (size_t i = 0; good && i < p.size(); ++i) good = std::fabs(p[i].x - want[i + 1].x) < 0.6f && std::fabs(p[i].z - want[i + 1].z) < 0.6f;
    ok += good;
    if (!good) printf("DIFF found=%d got %zu want %d pts, start %.0f %.0f goal %.0f %.0f\n", found, p.size(), m, sx, sz, gx, gz);
  }
  printf("%d / %d\n", ok, tot);
}
