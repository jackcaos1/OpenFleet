// Gibt das HUD-Layout fuer eine Bildschirmgroesse als Textliste aus (Pruefung gegen die echten Spieldaten, Rendern in tools/hud_render.py).
#include <cstdio>
#include <cstdlib>
#include <string>
#include "game/Hud.h"
#include "game/Shell.h"
using namespace fleet;
int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "hud_dump <spielordner> <breite> <hoehe> <rasse 0-3> [map]\n"); return 2; }
  auto src = OpenDirectory(argv[1]);
  HudData d;
  if (!d.Load(*src, static_cast<HudRace>(std::atoi(argv[4])))) { std::fprintf(stderr, "HUD-Daten fehlen\n"); return 1; }
  if (argc > 5 && std::string(argv[5]) == "shell") {  // Hauptmenue: hud_dump <ordner> <b> <h> <rasse> shell <main|single> [gedrueckt]
    const std::string sc = argc > 6 ? argv[6] : "main";
    SkirmishView sv;
    for (int i = 0; i < 4; ++i) { sv.slot[i].state = 1; sv.slot[i].name = i == 0 ? "Spieler" : "Computer"; sv.slot[i].race = "Klingonen"; sv.slot[i].team = "Team " + std::to_string(i + 1); }
    sv.maps = {"(leere Karte)", "2blue", "2borders", "2deadzone", "2divide", "2duel", "2hidden", "2maze", "3race", "3triad", "4across", "4brown"};
    sv.selected = 1; sv.can_down = true; sv.title = "2blue - 2 Spieler"; sv.preview = "bzn/2blue.bmp"; sv.res_label = "Ressourcen: Normal";
    sv.info = {"Karte: 2blue", "Startplaetze: 2", "", "Jeder aktive Platz startet mit Basis"};
    const ShellLayout SL = BuildShell(sc == "skirmish" ? ShellScreen::Skirmish : sc == "single" ? ShellScreen::Single : ShellScreen::Main, std::atoi(argv[2]), std::atoi(argv[3]), argc > 7 ? argv[7] : "", &d, &sv);
    std::printf("L 0 1 0 0 0 0\n");
    for (const HudDraw& q : SL.draws)
      std::printf("D %s %.0f %.0f %.0f %.0f %.5f %.5f %.5f %.5f %.3f %.3f %.3f %.3f %d\n", q.tex.empty() ? "-" : q.tex.c_str(), q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1, q.rgba[0], q.rgba[1], q.rgba[2], q.rgba[3], q.additive);
    return 0;
  }
  HudState st;
  st.ui.map_mode = argc > 5;
  st.ui.firing = true;
  st.race_label = HudRaceName(d.race());
  st.info_lines = {"fcruiser", "Modell 3/46", "Variante 1/2", "Gr\xC3\xB6\xC3\x9F" "e \xC3\xA4\xC3\xB6\xC3\xBC \xC3\x84\xC3\x96\xC3\x9C"};
  if (argc > 5 && std::string(argv[5]) == "ship") {
    HudShip h; h.name = "Sovereign"; h.wire = "bbattle"; h.cls = "Schiff"; h.crew = 850; h.crew_max = 850; h.officers = 12; h.energy = 0.6f; h.shield = 0.8f; h.system[1] = 0.5f; h.system[4] = 0.2f;
    st.ships.push_back(h);
    if (argc > 6) { h.name = "Defiant"; st.ships.push_back(h); st.ships.push_back(h); }
  }
  for (int i = 0; i < 30; ++i) { HudDot x; x.nx = 0.1f + 0.027f * i; x.ny = 0.5f + 0.3f * ((i * 7) % 5 - 2) / 5.0f; x.rgb[0] = i % 2; x.rgb[1] = 1; x.rgb[2] = 0.3f; st.dots.push_back(x); }
  const HudLayout L = BuildHudLayout(d, std::atoi(argv[2]), std::atoi(argv[3]), st);
  std::printf("L %d %.3f %.1f %.1f %.1f %.1f\n", L.portrait, L.scale, L.view.x, L.view.y, L.view.w, L.view.h);
  for (const HudDraw& q : L.draws)
    std::printf("D %s %.0f %.0f %.0f %.0f %.5f %.5f %.5f %.5f %.3f %.3f %.3f %.3f %d\n", q.tex.empty() ? "-" : q.tex.c_str(), q.x, q.y, q.w, q.h, q.u0, q.v0, q.u1, q.v1, q.rgba[0], q.rgba[1], q.rgba[2], q.rgba[3], q.additive);
  for (const HudText& t : L.texts) std::printf("T %.0f %.0f %d %.2f %.2f %.2f %s\n", t.x, t.y, t.scale, t.rgb[0], t.rgb[1], t.rgb[2], t.text.c_str());
  for (const HudButtonSlot& b : L.buttons) std::printf("B %d %.0f %.0f %.0f %.0f %d\n", static_cast<int>(b.action), b.rect.x, b.rect.y, b.rect.w, b.rect.h, b.enabled);
  return 0;
}
