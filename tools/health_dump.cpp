// Fuehrt Szenarien (von health_check.py erzeugt) mit engine/src/game/Combat.cpp aus und druckt den Zustand nach jedem Schritt.
// Bau: g++ -O1 -std=c++17 -Iengine/src tools/health_dump.cpp engine/src/game/Combat.cpp -o /tmp/health_dump
#include <cstdio>
#include <cstring>
#include <string>

#include "game/Combat.h"
using namespace fleet;

// Reihenfolge der Datei/Exe-Bloecke: Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren -> Sys
static const int kFile[5] = {kShield, kEngines, kWeapons, kLife, kSensors};
// Wurfergebnis der Exe (0 Antrieb, 1 Leben, 2 Waffen, 3 Schild, 4 Sensoren, 5 Rumpf, 6 kritisch) -> Ziel
static int Target(int idx) {
  static const int k[5] = {kEngines, kLife, kWeapons, kShield, kSensors};
  return idx <= 4 ? k[idx] : idx;
}

static void Print(const Health& h) {
  printf("S %.6g %.6g %.6g %.6g %d %d %.6g", h.shield, h.delay_left, h.crew, h.carry, h.derelict ? 1 : 0, h.exploded ? 1 : 0, h.crit_pct);
  for (int i = 0; i < 5; ++i) {
    const SystemBlock& b = h.sys[kFile[i]];
    printf(" | %d %d %.6g %.6g %.6g %.6g", b.active ? 1 : 0, b.points, b.hit_pct, b.per_point, b.countdown, b.disable_timer);
  }
  printf("\n");
}

int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "r");
  char tok[32];
  HealthSpec spec;
  RaceRules race;
  Health h;
  while (fscanf(f, "%31s", tok) == 1) {
    if (!strcmp(tok, "CFG")) {
      float p; int b, u;
      fscanf(f, "%f %d %d", &p, &b, &u);
      CombatCfg() = CombatConfig{u != 0, b, p};
    } else if (!strcmp(tok, "SPEC")) {
      spec = HealthSpec();
      fscanf(f, "%f %f %f %f %f %f %f", &spec.shield_max, &spec.shield_rate, &spec.shield_delay, &spec.crew_max, &spec.hull_pct, &spec.life_support_loss, &spec.critical_pct);
    } else if (!strcmp(tok, "SYS")) {
      int i, pts; float hit, loss, rep;
      fscanf(f, "%d %d %f %f %f", &i, &pts, &hit, &loss, &rep);
      spec.sys[kFile[i]] = SystemSpec{pts, hit, loss, rep};
    } else if (!strcmp(tok, "RACE")) {
      fscanf(f, "%f %f %f %f %f %f %f", &race.repair_strength, &race.crew_yellow, &race.crew_red, &race.repair_yellow, &race.repair_red, &race.weapon_yellow, &race.weapon_red);
    } else if (!strcmp(tok, "GO")) {
      InitHealth(&h, spec, race);
      printf("BEGIN\n");
      Print(h);
    } else if (!strcmp(tok, "HIT")) {
      int idx, ign; HitInfo hi;
      fscanf(f, "%d %f %f %f %f %d", &idx, &hi.damage, &hi.threshold, &hi.shield_crew_mod, &hi.hull_crew_mod, &ign);
      hi.ignore_shield = ign != 0;
      ApplyHitTo(&h, Target(idx), hi);
      Print(h);
    } else if (!strcmp(tok, "STEP")) {
      float dt; fscanf(f, "%f", &dt);
      StepHealth(&h, dt);
      Print(h);
    } else if (!strcmp(tok, "CREW")) {
      float c; fscanf(f, "%f", &c);
      SetCrew(&h, c);
      Print(h);
    } else if (!strcmp(tok, "ROLL")) {
      // Trefferwurf fuer alle 100 Wuerfe (Exe-Zielnummern: 0 Antrieb, 1 Leben, 2 Waffen, 3 Schild, 4 Sensoren, 5 Rumpf, 6 kritisch)
      printf("R");
      for (int roll = 0; roll < 100; ++roll) {
        const int t = HitSystemForRoll(h, roll);
        int out = t;
        if (t == kEngines) out = 0; else if (t == kLife) out = 1; else if (t == kWeapons) out = 2; else if (t == kShield) out = 3; else if (t == kSensors) out = 4;
        printf(" %d", out);
      }
      printf("\n");
    } else if (!strcmp(tok, "SELFD")) {
      int v; fscanf(f, "%d", &v);
      h.self_destruct = v != 0;
      Print(h);
    } else if (!strcmp(tok, "END")) {
      printf("END\n");
    }
  }
}
