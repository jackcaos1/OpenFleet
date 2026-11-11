// Vergleicht engine/src/game/PathGrid.cpp mit Orakel-JSON (via path_check.py -> Textformat). g++ -O1 -I../engine/src path_check.cpp ../engine/src/game/PathGrid.cpp
#include <cmath>
#include <cstdio>
#include <vector>
#include "game/PathGrid.h"
using namespace fleet;
int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "r"); int nf, ok = 0, tot = 0;
  while (fscanf(f, "%d", &nf) == 1) {
    PathGrid g; g.Init(-1500, -1500, 1500, 1500, 50);
    for (int i = 0; i < nf; ++i) { float x, z; fscanf(f, "%f %f", &x, &z); if (argc > 2) g.StampField(x, z); else g.StampSquare(x, z, 75); }
    float sx, sz, gx, gz; int n; fscanf(f, "%f %f %f %f %d", &sx, &sz, &gx, &gz, &n);
    std::vector<PathPoint> want(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) fscanf(f, "%f %f", &want[static_cast<size_t>(i)].x, &want[static_cast<size_t>(i)].z);
    std::vector<PathPoint> p; const bool found = g.FindPath({sx, sz}, {gx, gz}, &p);
    ++tot;
    bool good = found && p.size() + 1 == want.size();
    for (size_t i = 0; good && i < p.size(); ++i) good = std::fabs(p[i].x - want[i + 1].x) < 0.6f && std::fabs(p[i].z - want[i + 1].z) < 0.6f;
    ok += good;
    if (!good) { printf("DIFF start %.1f %.1f goal %.1f %.1f found=%d\n  got:", sx, sz, gx, gz, found); for (auto& q : p) printf(" (%.0f,%.0f)", q.x, q.z); printf("\n  want:"); for (size_t i = 1; i < want.size(); ++i) printf(" (%.0f,%.0f)", want[i].x, want[i].z); printf("\n"); }
  }
  printf("%d / %d\n", ok, tot);
}
