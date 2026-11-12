// Fuehrt Entermannschafts-Szenarien mit engine/src/game/Boarding.cpp aus (Vergleich mit Armada.exe: tools/boarding_check.py).
// Bau: g++ -O1 -std=c++17 -Iengine/src tools/boarding_dump.cpp engine/src/game/Boarding.cpp engine/src/game/Combat.cpp -o /tmp/boarding_dump
#include <cstdio>
#include <cstring>

#include "game/Boarding.h"
using namespace fleet;

static Health Make(float crew, float crew_max, bool derelict) {
  HealthSpec sp;
  sp.crew_max = crew_max;
  sp.shield_max = 100;
  sp.shield_rate = 2;
  sp.shield_delay = 3;
  sp.hull_pct = 70;
  for (int i = 0; i < kSysCount; ++i) { sp.sys[i].points = 10; sp.sys[i].repair_time = 30; }
  Health h;
  InitHealth(&h, sp, RaceRules());
  h.crew = crew;
  h.derelict = derelict;
  if (derelict) for (SystemBlock& b : h.sys) b.active = false;
  return h;
}

int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "r");
  char tok[16];
  while (fscanf(f, "%15s", tok) == 1) {
    if (!strcmp(tok, "STEP")) {
      float sc, sm, red, tc, tm, tsh, tshm, dist; int en;
      fscanf(f, "%f %f %f %f %f %f %f %f %d", &sc, &sm, &red, &tc, &tm, &tsh, &tshm, &dist, &en);
      Health src = Make(sc, sm, false), tgt = Make(tc, tm, false);
      src.race.crew_red = red;
      tgt.shield_max = tshm;
      tgt.shield = tsh;
      const BeamStep r = TransporterStep(&src, &tgt, dist, en != 0);
      printf("T %d %d %.6g %d %.6g %.6g %d\n", r.stop ? 1 : 0, r.created ? 1 : 0, r.party_crew, r.enemy ? 1 : 0, src.crew, tgt.delay_left, src.derelict ? 1 : 0);
    } else if (!strcmp(tok, "ARRIVE")) {
      float crew, mx, ts, pc, ps; int der, tt, pt, en;
      fscanf(f, "%f %f %d %d %f %f %f %d %d", &crew, &mx, &der, &tt, &ts, &pc, &ps, &pt, &en);
      Health t = Make(crew, mx, der != 0);
      const BoardResult r = ArriveBoardingParty(&t, tt, ts, pc, ps, pt, en != 0);
      printf("A %.6g %d %d %d %.6g\n", t.crew, t.derelict ? 1 : 0, r.owner_changed ? 1 : 0, r.race_changed ? 1 : 0, r.party_left);
    }
  }
}
