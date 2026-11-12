#include "game/Shell.h"
#include "core/Version.h"
#include "game/MenuLayout.h"

#include <algorithm>

namespace fleet {
namespace {

struct Frame {
  float s = 1, ox = 0, oy = 0;  // Bezug 640x480 -> Pixel
};

HudDraw Quad(const std::string& tex, float x, float y, float w, float h, float a = 1.0f, float rgb = 1.0f) {
  HudDraw d;
  d.tex = tex;
  d.x = x; d.y = y; d.w = w; d.h = h;
  d.rgba[0] = d.rgba[1] = d.rgba[2] = rgb;
  d.rgba[3] = a;
  return d;
}

struct Def {
  const char* id;
  float x, y, w, h;   // Lage und Groesse im 640x480-Bezug (Original: Steuerelemente in Armada.exe 0x546150..0x546731)
  const char* bmp;    // Grafikname ohne Zustandsziffer, "" = Platte mit Text
  const char* label;  // Text auf der Platte (nur bei bmp == "")
  bool enabled;
};

// Ein Feld des Originalbildschirms (Bezugskoordinaten) mit Text; id leer = nur Anzeige.
void Slot(ShellLayout* L, const Frame& f, const HudData* font, float x, float y, float w, float h, const std::string& text, const float rgb[3], bool center,
          const std::string& id, bool enabled, bool down, bool highlight) {
  if (highlight || down) {
    HudDraw d = Quad("", f.ox + x * f.s, f.oy + y * f.s, w * f.s, h * f.s, 0.0f);
    d.rgba[0] = 1.0f; d.rgba[1] = down ? 0.95f : 0.85f; d.rgba[2] = 0.25f; d.rgba[3] = down ? 0.55f : 0.4f;
    L->draws.push_back(d);
  }
  if (!text.empty() && font && font->has_font()) {
    const float th = std::min(h - 3.0f, 11.0f) * f.s;
    HudTextBlock probe = LayoutHudText(*font, text, 0, 0, th, 1e6f, rgb, nullptr);
    float tx = f.ox + (x + 3.0f) * f.s;
    const float maxw = (w - 6.0f) * f.s;
    float scale_w = probe.w > maxw && probe.w > 0 ? maxw / probe.w : 1.0f;  // zu lange Texte: kleiner
    const float th2 = th * scale_w;
    if (scale_w < 1.0f) probe = LayoutHudText(*font, text, 0, 0, th2, 1e6f, rgb, nullptr);
    if (center) tx = f.ox + x * f.s + 0.5f * (w * f.s - probe.w);
    HudTextBlock tb = LayoutHudText(*font, text, tx, f.oy + (y + 0.5f * h) * f.s - 0.5f * th2, th2, 1e6f, rgb, nullptr);
    for (HudDraw& t : tb.draws) {
      if (!enabled) t.rgba[3] *= 0.5f;
      L->draws.push_back(t);
    }
  }
  if (!id.empty()) {
    ShellButton b;
    b.id = id;
    b.enabled = enabled;
    b.rect = {f.ox + x * f.s, f.oy + (y - 2.5f) * f.s, w * f.s, (h + 5.0f) * f.s};
    L->buttons.push_back(b);
  }
}

// Spiel-Einrichtung (Armada.exe 0x536250): Rechtecke aus dem Aufbaucode, Texte aus label.map (docs/formats/shell-setup.md).
void Plate(ShellLayout* L, const Frame& f, const std::string& base, int state, float x, float y, float w, float h) {
  L->draws.push_back(Quad("bitmaps/multiset/" + base + std::to_string(state) + ".bmp", f.ox + x * f.s, f.oy + y * f.s, w * f.s, h * f.s));
}

// Quad mit Ausschnitt eines Hintergrundbilds (Quellrechteck in 640x480-Bezug)
HudDraw SubQuad(const std::string& tex, float sx0, float sy0, float sx1, float sy1, float x, float y, float w, float h) {
  HudDraw d = Quad(tex, x, y, w, h);
  d.u0 = sx0 / 640.0f; d.v0 = sy0 / 480.0f; d.u1 = sx1 / 640.0f; d.v1 = sy1 / 480.0f;
  return d;
}

// Querformat: Bildschirm wie im Original. Hochformat (Abweichung vom Original): die linke Haelfte (Spieler, Meldungen) und die rechte Haelfte (Karte, Optionen)
// stehen uebereinander, darunter die Tasten (Zurueck / Hauptmenue nebeneinander, Start darunter). Zeichnung und Trefferflaechen folgen derselben Abbildung.
void BuildSkirmish(ShellLayout* L, const Frame& base, bool portrait, float W, float H, const std::string& pressed, const HudData* font, const SkirmishView* sk) {
  static const float kInk[3] = {0.05f, 0.04f, 0.03f}, kWhite[3] = {1, 1, 1}, kGreen[3] = {0.55f, 1.0f, 0.65f};
  constexpr float kLeftW = 345.0f, kSplit = 347.0f, kHalfH = 415.0f, kCanvasW = 345.0f, kCanvasH = 905.0f;  // Schnitt zwischen den Rahmen bei x 345..347; Chat-Zeile unten (y > 415) entfaellt
  Frame fl = base, fr = base, fb = base;  // links, rechts, Tasten
  if (portrait) {
    const float sc = std::min(W / kCanvasW, H / kCanvasH);
    const float ox0 = 0.5f * (W - kCanvasW * sc), oy0 = 0.5f * (H - kCanvasH * sc);
    fl.s = fr.s = fb.s = sc;
    fl.ox = fb.ox = ox0; fl.oy = fb.oy = oy0;
    fr.ox = ox0 + (0.5f * (kCanvasW - (640.0f - kSplit)) - kSplit) * sc;
    fr.oy = oy0 + kHalfH * sc;
    L->draws.push_back(SubQuad("bitmaps/multiset/mpsBG.bmp", 0, 0, kLeftW, kHalfH, fl.ox, fl.oy, kLeftW * sc, kHalfH * sc));
    L->draws.push_back(SubQuad("bitmaps/multiset/mpsBG.bmp", kSplit, 0, 640, kHalfH, fr.ox + kSplit * sc, fr.oy, (640 - kSplit) * sc, kHalfH * sc));
  }
  auto F = [&](float x) -> const Frame& { return portrait && x >= 360.0f ? fr : fl; };
  SkirmishView empty;
  if (!sk) sk = &empty;
  Slot(L, F(64), font, 64, 6, 178, 20, sk->title, kWhite, false, "", true, false, false);
  Slot(L, F(5), font, 5, 32, 140, 14, sk->col_name, kWhite, false, "", true, false, false);
  Slot(L, F(167), font, 167, 32, 75, 14, sk->col_side, kWhite, false, "", true, false, false);
  Slot(L, F(273), font, 273, 32, 35, 14, sk->col_team, kWhite, false, "", true, false, false);
  Slot(L, F(385), font, 385, 7, 100, 14, sk->col_game, kWhite, false, "", true, false, false);
  Slot(L, F(5), font, 5, 234, 80, 14, sk->label_messages, kWhite, false, "", true, false, false);
  for (int i = 0; i < 8; ++i) {
    const float y = 57.0f + 19.0f * static_cast<float>(i);  // Zeilenabstand aus dem Hintergrundbild (Zeilenhoehe 19 laut Code)
    const SkirmishSlotView& v = sk->slot[i];
    const bool on = v.state == 1;
    const std::string n = std::to_string(i);
    if (on) {  // Auswahlfelder der Zeile (Combo-Boxen 145 / 111 / 58 x 19), Grafiken mpsplapd / mpsracpd / mpsteapd
      Plate(L, fl, "mpsplapd", pressed == "slot_name:" + n ? 2 : 1, 5, y - 1, 145, 19);
      Plate(L, fl, "mpsracpd", pressed == "slot_race:" + n ? 2 : 1, 149, y - 1, 111, 19);
      Plate(L, fl, "mpsteapd", pressed == "slot_team:" + n ? 2 : 1, 261, y - 1, 58, 19);
    }
    Slot(L, fl, font, 7, y, 142, 14, v.name, i == 0 ? kGreen : kInk, false, on && i > 0 ? "slot_name:" + n : "", on, pressed == "slot_name:" + n, false);
    Slot(L, fl, font, 151, y, 109, 14, v.race, kInk, false, on ? "slot_race:" + n : "", on, pressed == "slot_race:" + n, false);
    Slot(L, fl, font, 262, y, 55, 14, v.team, kInk, false, on ? "slot_team:" + n : "", on, pressed == "slot_team:" + n, false);
  }
  for (const SkirmishOptionView& o : sk->options) {
    const std::string id = "opt:" + std::to_string(o.id);
    const std::string kind = o.toggle ? "tg" : "pd";  // Auswahlliste = pd (mit Pfeil), Schalter = tg
    Plate(L, F(o.x), std::string(o.w > 120 ? "mps128" : "mps111") + kind, pressed == id ? 2 : 1, o.x, o.y, o.w, o.h);
    Slot(L, F(o.x), font, o.x + 3, o.y, o.w - 6, o.h - 4, o.text, kInk, true, id, o.enabled, false, false);
  }
  if (!sk->map_name.empty()) Slot(L, fr, font, 385, 104, 130, 14, sk->map_name, kWhite, false, "", true, false, false);
  if (!sk->preview.empty()) L->draws.push_back(Quad(sk->preview, fr.ox + 385 * fr.s, fr.oy + 138 * fr.s, 128 * fr.s, 128 * fr.s));
  Plate(L, fr, "mps128tg", pressed == "select_map" ? 2 : 1, 385, 272, 128, 20);
  Slot(L, fr, font, 385, 272, 128, 20, sk->btn_select_map, kInk, true, "select_map", true, false, false);
  for (size_t i = 0; i < sk->info.size() && i < 12; ++i) Slot(L, fl, font, 12, 258.0f + 13.0f * static_cast<float>(i), 308, 12, sk->info[i], kWhite, false, "", true, false, false);
  // Tasten: Original unten in einer Reihe (6 / 124 / 524, y 446), im Hochformat Zurueck und Hauptmenue nebeneinander, Start darunter (gestaucht)
  const float py1 = portrait ? 836.0f : 446.0f, py2 = portrait ? 870.0f : 446.0f;
  const float xp = portrait ? 30.0f : 6.0f, xm = portrait ? kCanvasW - 30.0f - 112.0f : 524.0f, xl = portrait ? 15.0f : 124.0f, wl = portrait ? kCanvasW - 30.0f : 395.0f;
  Plate(L, fb, "navback", pressed == "back" ? 2 : 1, xp, py1, 112, 30);
  Slot(L, fb, font, xp, py1 + 8, 112, 14, sk->btn_prev, kInk, true, "back", true, false, false);
  Plate(L, fb, "mpsstart", pressed == "start" ? 2 : 1, xl, py2, wl, 30);
  Slot(L, fb, font, xl, py2 + 8, wl, 14, sk->btn_launch, kInk, true, "start", true, false, false);
  Plate(L, fb, "navmain", pressed == "mainmenu" ? 2 : 1, xm, py1, 112, 30);
  Slot(L, fb, font, xm, py1 + 8, 112, 14, sk->btn_main, kInk, true, "mainmenu", true, false, false);
}

// Kartenauswahl (Armada.exe 0x530b70, Hintergrund bitmaps/misc/DBmsBG.bmp 393 x 290, Elemente relativ dazu). Platzierung im Bild und Zeilenabstand der Liste sind geschaetzt.
void BuildSkirmishMap(ShellLayout* L, const Frame& f, float px, float py, const std::string& pressed, const HudData* font, const SkirmishView* sk) {
  static const float kInk[3] = {0.05f, 0.04f, 0.03f}, kGold[3] = {1.0f, 0.88f, 0.35f}, kWhite[3] = {1, 1, 1};
  SkirmishView empty;
  if (!sk) sk = &empty;
  L->draws.push_back(Quad("bitmaps/misc/DBmsBG.bmp", f.ox + px * f.s, f.oy + py * f.s, 393 * f.s, 290 * f.s));
  Slot(L, f, font, px + 242, py + 2, 128, 20, sk->filter_text, kInk, true, "mapsel_filter", true, pressed == "mapsel_filter", false);
  Plate(L, f, "navback", pressed == "mapsel_ok" ? 2 : 1, px + 19, py + 258, 112, 30);
  Slot(L, f, font, px + 19, py + 258 + 8, 112, 14, sk->btn_select, kInk, true, "mapsel_ok", sk->selected >= 0, false, false);
  Plate(L, f, "navmain", pressed == "mapsel_cancel" ? 2 : 1, px + 260, py + 258, 112, 30);
  Slot(L, f, font, px + 260, py + 258 + 8, 112, 14, sk->btn_cancel, kInk, true, "mapsel_cancel", true, false, false);
  L->draws.push_back(Quad(std::string("bitmaps/multiset/scrollup") + (pressed == "mapsel_up" ? "2" : "1") + ".bmp", f.ox + (px + 376) * f.s, f.oy + (py + 27) * f.s, 14 * f.s, 21 * f.s));
  { ShellButton b; b.id = "mapsel_up"; b.enabled = sk->can_up; b.rect = {f.ox + (px + 366) * f.s, f.oy + (py + 22) * f.s, 30 * f.s, 30 * f.s}; L->buttons.push_back(b); }
  L->draws.push_back(Quad(std::string("bitmaps/multiset/scrolldn") + (pressed == "mapsel_down" ? "2" : "1") + ".bmp", f.ox + (px + 376) * f.s, f.oy + (py + 235) * f.s, 14 * f.s, 20 * f.s));
  { ShellButton b; b.id = "mapsel_down"; b.enabled = sk->can_down; b.rect = {f.ox + (px + 366) * f.s, f.oy + (py + 230) * f.s, 30 * f.s, 30 * f.s}; L->buttons.push_back(b); }
  for (size_t k = 0; k < sk->maps.size() && k < 13; ++k) {
    const std::string id = "mapsel:" + std::to_string(k);
    Slot(L, f, font, px + 15, py + 48 + 14.0f * static_cast<float>(k), 345, 12, sk->maps[k], static_cast<int>(k) == sk->selected ? kGold : kWhite, false, id, true, pressed == id,
         static_cast<int>(k) == sk->selected);
  }
}

// "Multiplayer Connection" (bitmaps/wonlogin, Platten bitmaps/multiset). Positionen aus einem Bildschirmfoto des Originals (Bezug 640x480), nicht aus einer Datei.
void BuildMulti(ShellLayout* L, float s, float ox, float oy, const std::string& pressed, const HudData* font) {
  Frame f;
  f.s = s; f.ox = ox; f.oy = oy;
  static const float kInk[3] = {0.05f, 0.04f, 0.03f}, kWhite[3] = {1, 1, 1};
  auto plate = [&](const std::string& tex, float x, float y, float w, float h) { L->draws.push_back(Quad(tex, f.ox + x * f.s, f.oy + y * f.s, w * f.s, h * f.s)); };
  Slot(L, f, font, 80, 6, 400, 16, "MULTIPLAYER CONNECTION", kWhite, false, "", true, false, false);
  Slot(L, f, font, 168, 76, 200, 14, "Enter Name:", kWhite, false, "", true, false, false);
  plate("bitmaps/wonlogin/wlstext.bmp", 160, 94, 324, 31);
  Slot(L, f, font, 166, 99, 300, 20, "Player", kWhite, false, "", true, false, false);
  struct B { const char* id; float y; const char* label; bool on; };
  static const B kBtn[] = {{"internet", 162, "Internet", false}, {"lan_ipx", 229, "Local Area Network (IPX)", false},
                           {"lan_tcp", 295, "Local Area Network (TCP/IP)", false}, {"skirmish", 361, "Instant Action", true}};
  for (const B& b : kBtn) {
    const bool down = pressed == b.id;
    plate(std::string("bitmaps/wonlogin/wls324tg") + (down ? "2" : "1") + ".bmp", 160, b.y, 324, 32);
    Slot(L, f, font, 160, b.y + 8, 324, 16, b.label, kInk, true, b.id, b.on, false, false);
  }
  plate(std::string("bitmaps/multiset/navback") + (pressed == "back" ? "2" : "1") + ".bmp", 26, 444, 112, 30);
  Slot(L, f, font, 26, 452, 112, 14, "Previous Menu", kInk, true, "back", true, false, false);
  plate("bitmaps/multiset/mpsstart0.bmp", 136, 444, 370, 30);
  plate("bitmaps/multiset/nav3but0.bmp", 513, 444, 102, 30);
}

// Lage eines Menue-Elements im 640x480-Bezug des Rahmens (kann bei Bildschirmkanten-Ankern ausserhalb 0..640 liegen)
void PlaceMenuElement(const MenuElement& e, const Frame& f, float W, float H, float* x, float* y) {
  const float gx = f.ox / f.s, gy = f.oy / f.s;   // Rand links/oben neben dem Rahmen, in Bezugseinheiten (gleich rechts/unten)
  *x = e.ax == MenuAnchor::Frame ? e.x : e.ax == MenuAnchor::Left ? e.x - gx : e.x + gx;
  *y = e.ay == MenuAnchorV::Frame ? e.y : e.ay == MenuAnchorV::Top ? e.y - gy : e.y + gy;
  (void)W; (void)H;
}

}  // namespace

ShellLayout BuildShell(ShellScreen screen, int width, int height, const std::string& pressed, const HudData* font, const SkirmishView* sk) {
  ShellLayout L;
  const float W = static_cast<float>(width), H = static_cast<float>(height);
  Frame f;
  f.s = std::min(W / 640.0f, H / 480.0f);
  f.ox = 0.5f * (W - 640.0f * f.s);
  f.oy = 0.5f * (H - 480.0f * f.s);
  const bool main = screen == ShellScreen::Main;
  const bool skirm = screen == ShellScreen::Skirmish || screen == ShellScreen::SkirmishMap;
  const bool multi = screen == ShellScreen::Multi;
  const std::string bg = skirm ? "bitmaps/multiset/mpsBG.bmp" : multi ? "bitmaps/wonlogin/wlsBG.bmp" : main ? "bitmaps/main/mainbk.BMP" : "bitmaps/single/singlebk.bmp";
  {  // Ersatz fuer schwarze Balken: derselbe Hintergrund, ueber den ganzen Bildschirm gezogen und abgedunkelt
    const float c = std::max(W / 640.0f, H / 480.0f);
    L.draws.push_back(Quad(bg, 0.5f * (W - 640.0f * c), 0.5f * (H - 480.0f * c), 640.0f * c, 480.0f * c, 1.0f, 0.3f));
  }
  const bool portrait = H > W;
  if (!(skirm && portrait)) L.draws.push_back(Quad(bg, f.ox, f.oy, 640.0f * f.s, 480.0f * f.s));  // Hochformat: Setup zeichnet seinen Hintergrund selbst (zwei Ausschnitte)

  if (multi) {
    BuildMulti(&L, f.s, f.ox, f.oy, pressed, font);
    return L;
  }
  if (skirm) {
    if (screen == ShellScreen::SkirmishMap) {
      BuildSkirmish(&L, f, portrait, W, H, "", font, sk);
      L.buttons.clear();  // die Einrichtung dahinter ist gesperrt
      L.draws.push_back(Quad("", 0, 0, W, H, 0.0f));
      L.draws.back().rgba[0] = L.draws.back().rgba[1] = L.draws.back().rgba[2] = 0.0f;
      L.draws.back().rgba[3] = 0.45f;
      Frame fp = f;
      float px = (640.0f - 393.0f) * 0.5f, py = (480.0f - 290.0f) * 0.5f;  // mittig (Platzierung geschaetzt)
      if (portrait) {  // Hochformat: Fenster auf die Bildschirmbreite skaliert
        fp.s = std::min(W / 410.0f, H / 310.0f);
        fp.ox = 0.5f * (W - 393.0f * fp.s);
        fp.oy = 0.5f * (H - 290.0f * fp.s);
        px = py = 0.0f;
      }
      BuildSkirmishMap(&L, fp, px, py, pressed, font, sk);
    } else {
      BuildSkirmish(&L, f, portrait, W, H, pressed, font, sk);
    }
    return L;
  }
  std::vector<Def> defs;
  const MenuLayout* main_ml = nullptr;
  if (main) {
    // Lage aus den Menue-Daten (MenuLayout.cpp): Originalanordnung, oder eine Variante fuer das aktuelle Seitenverhaeltnis
    static const struct { const char* id; const char* bmp; bool enabled; } kMain[] = {
        {"single", "ms/MSsingl", true}, {"multi", "ms/MSmulti", true},    {"replay", "ms/MSintro", false},
        {"credits", "ms/MScredit", false}, {"options", "ms/MSoption", false}, {"quit", "ms/MSquit", true}};
    const MenuLayout* ml = SelectMenuLayout("main", W / H);
    for (const auto& m : kMain)
      if (const MenuElement* e = ml ? FindMenuElement(*ml, m.id) : nullptr) {
        float ex = 0, ey = 0;
        PlaceMenuElement(*e, f, W, H, &ex, &ey);
        defs.push_back({m.id, ex, ey, e->w, e->h, m.bmp, "", m.enabled});
      }
    main_ml = ml;
  } else {
    // Einzelspieler wie im Original: Titel, Previous Menu (unten links), Load Game (unten rechts); die vier Portraets starten die Kampagnen (noch nicht)
    L.draws.push_back(Quad("bitmaps/sp/SPtitle.bmp", f.ox + 256 * f.s, f.oy + 13 * f.s, 128 * f.s, 16 * f.s));
    const bool pd = pressed == "back", ld = pressed == "load";
    L.draws.push_back(Quad(std::string("bitmaps/sp/SPprev") + (pd ? "2" : "1") + ".bmp", f.ox + 32 * f.s, f.oy + 445 * f.s, 152 * f.s, 16 * f.s));
    L.draws.push_back(Quad(std::string("bitmaps/sp/SPload") + (ld ? "2" : "1") + ".bmp", f.ox + 500 * f.s, f.oy + 443 * f.s, 104 * f.s, 20 * f.s, 0.45f));
    ShellButton bb;
    bb.id = "back";
    bb.rect = {f.ox + 20 * f.s, f.oy + 435 * f.s, 190 * f.s, 36 * f.s};
    L.buttons.push_back(bb);
    return L;
  }
  for (const Def& d : defs) {
    const bool down = pressed == d.id;
    const int state = down ? 2 : 1;  // 1 grau/normal, 2 gelb
    const float alpha = d.enabled ? 1.0f : 0.45f;
    float iw = 0, ih = 0;
    std::string tex;
    if (d.bmp[0]) {
      tex = std::string("bitmaps/") + d.bmp + std::to_string(state) + ".bmp";
    } else {
      tex = std::string("bitmaps/sp/SPgame") + std::to_string(state) + ".bmp";
    }
    iw = d.w; ih = d.h;
    const float x = d.x, y = d.y;
    L.draws.push_back(Quad(tex, f.ox + x * f.s, f.oy + y * f.s, iw * f.s, ih * f.s, alpha));
    if (!d.bmp[0] && font && font->has_font()) {
      const float th = 11.0f * f.s;
      static const float kInk[3] = {0.05f, 0.04f, 0.03f};
      HudTextBlock probe = LayoutHudText(*font, d.label, 0, 0, th, 1e6f, kInk, nullptr);
      HudTextBlock tb = LayoutHudText(*font, d.label, f.ox + (x + 0.5f * iw) * f.s - 0.5f * probe.w, f.oy + (y + 0.5f * ih) * f.s - 0.5f * th, th, 1e6f, kInk, nullptr);
      for (HudDraw& t : tb.draws) {
        t.rgba[3] *= alpha;
        L.draws.push_back(t);
      }
    }
    ShellButton b;
    b.id = d.id;
    b.enabled = d.enabled;
    b.rect = {f.ox + (d.x - 4) * f.s, f.oy + (d.y - 2) * f.s, (d.w + 8) * f.s, (d.h + 4) * f.s};  // etwas grosszuegiger fuer den Finger
    L.buttons.push_back(b);
  }
  {  // Versionsanzeige wie im Original (Textfeld 0x5255a0 bei x=600, y=468, 40x10, Farbe rot; Original zeigt "v1.2" aus der Exe, hier Build d<Nummer>)
    static const float kRed[3] = {1.0f, 0.1f, 0.1f};
    float vx = 600, vy = 466, vw = 40, vh = 14;
    if (const MenuElement* e = main_ml ? FindMenuElement(*main_ml, "version") : nullptr) {
      PlaceMenuElement(*e, f, W, H, &vx, &vy);
      vw = e->w; vh = e->h;
    }
    Slot(&L, f, font, vx, vy, vw, vh, std::string(FLEET_BUILD_KIND) + std::to_string(FLEET_BUILD), kRed, false, "", true, false, false);
  }
  return L;
}

std::string ShellHit(const ShellLayout& layout, float x, float y) {
  for (const ShellButton& b : layout.buttons)
    if (b.enabled && b.rect.Contains(x, y)) return b.id;
  return std::string();
}

}  // namespace fleet
