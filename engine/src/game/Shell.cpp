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

void BuildSkirmish(ShellLayout* L, float s, float ox, float oy, const std::string& pressed, const HudData* font, const SkirmishView* sk) {
  Frame f;
  f.s = s; f.ox = ox; f.oy = oy;
  static const float kInk[3] = {0.05f, 0.04f, 0.03f}, kWhite[3] = {1, 1, 1}, kGold[3] = {1.0f, 0.88f, 0.35f}, kGreen[3] = {0.55f, 1.0f, 0.65f};
  SkirmishView empty;
  if (!sk) sk = &empty;
  Slot(L, f, font, 8, 8, 330, 16, "GAME SETUP", kWhite, false, "", true, false, false);
  for (int i = 0; i < 8; ++i) {
    const float y = 57.0f + 19.0f * static_cast<float>(i);
    const SkirmishSlotView& v = sk->slot[i];
    const bool on = v.state == 1;
    const std::string n = std::to_string(i);
    Slot(L, f, font, 7, y, 142, 14, v.name, i == 0 ? kGreen : kInk, false, on && i > 0 ? "slot_name:" + n : "", on, pressed == "slot_name:" + n, false);
    Slot(L, f, font, 151, y, 109, 14, v.race, kInk, false, on ? "slot_race:" + n : "", on, pressed == "slot_race:" + n, false);
    Slot(L, f, font, 262, y, 55, 14, v.team, kInk, false, on ? "slot_team:" + n : "", on, pressed == "slot_team:" + n, false);
  }
  // rechte Spalte: Kartenliste (14 Felder: Pfeil hoch, 12 Karten, Pfeil runter)
  Slot(L, f, font, 526, 63, 107, 14, "^", kInk, true, "map_up", sk->can_up, pressed == "map_up", false);
  for (int k = 0; k < 12; ++k) {
    const float y = 63.0f + 24.0f * static_cast<float>(k + 1);
    const bool have = k < static_cast<int>(sk->maps.size());
    const std::string id = "map:" + std::to_string(k);
    Slot(L, f, font, 526, y, 107, 14, have ? sk->maps[static_cast<size_t>(k)] : std::string(), k == sk->selected ? kGold : kInk, false, have ? id : "", have, pressed == id, k == sk->selected);
  }
  Slot(L, f, font, 526, 63 + 24 * 13, 107, 14, "v", kInk, true, "map_down", sk->can_down, pressed == "map_down", false);
  if (!sk->title.empty()) Slot(L, f, font, 384, 36, 248, 16, sk->title, kGold, false, "", true, false, false);
  if (!sk->preview.empty()) L->draws.push_back(Quad(sk->preview, f.ox + 385 * f.s, f.oy + 138 * f.s, 128 * f.s, 128 * f.s));
  Slot(L, f, font, 387, 275, 124, 14, sk->res_label, kInk, false, "res", true, pressed == "res", false);
  // Infofeld links unten (Kommentar-/Chatfeld des Originals)
  for (size_t i = 0; i < sk->info.size() && i < 12; ++i) Slot(L, f, font, 12, 258.0f + 13.0f * static_cast<float>(i), 308, 12, sk->info[i], kWhite, false, "", true, false, false);
  Slot(L, f, font, 9, 450, 106, 21, "Previous Menu", kInk, true, "back", true, pressed == "back", false);
  Slot(L, f, font, 126, 449, 390, 23, "LAUNCH", kInk, true, "start", true, pressed == "start", false);
  Slot(L, f, font, 527, 450, 106, 21, "Main Menu", kInk, true, "mainmenu", true, pressed == "mainmenu", false);
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
  const bool skirm = screen == ShellScreen::Skirmish;
  const bool multi = screen == ShellScreen::Multi;
  const std::string bg = skirm ? "bitmaps/multiset/mpsBG.bmp" : multi ? "bitmaps/wonlogin/wlsBG.bmp" : main ? "bitmaps/main/mainbk.BMP" : "bitmaps/single/singlebk.bmp";
  {  // Ersatz fuer schwarze Balken: derselbe Hintergrund, ueber den ganzen Bildschirm gezogen und abgedunkelt
    const float c = std::max(W / 640.0f, H / 480.0f);
    L.draws.push_back(Quad(bg, 0.5f * (W - 640.0f * c), 0.5f * (H - 480.0f * c), 640.0f * c, 480.0f * c, 1.0f, 0.3f));
  }
  L.draws.push_back(Quad(bg, f.ox, f.oy, 640.0f * f.s, 480.0f * f.s));

  if (multi) {
    BuildMulti(&L, f.s, f.ox, f.oy, pressed, font);
    return L;
  }
  if (skirm) {
    BuildSkirmish(&L, f.s, f.ox, f.oy, pressed, font, sk);
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
