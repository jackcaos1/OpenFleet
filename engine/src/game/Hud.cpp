#include "game/Hud.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "core/FontData.h"

namespace fleet {

namespace {

const char* const kCfgFiles[kHudRaceCount] = {"gui_fed.cfg", "gui_kli.cfg", "gui_rom.cfg", "gui_bor.cfg"};
const char* const kSprFiles[kHudRaceCount] = {"gui_federation.spr", "gui_klingon.spr", "gui_romulan.spr", "gui_borg.spr"};
const char* const kRaceNames[kHudRaceCount] = {"Foed", "Klin", "Rom", "Borg"};

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool ReadText(FileSource& src, const std::string& path, std::string* out) {
  std::vector<uint8_t> b;
  if (!src.ReadFile(path, &b)) return false;
  out->assign(b.begin(), b.end());
  return true;
}

// Panel-Beschreibung aus der Konfiguration (Praefix wie "minimap", "info", "control", ...).
struct PanelDef {
  bool ok = false;
  UiRect area;                 // Lage in der Bezugsaufloesung
  std::string bg;              // Sprite-Basisname, Teile "<bg>.<i>"
  std::vector<UiRect> pieces;  // Teil i relativ zum Panel
};

PanelDef ReadPanel(const GuiCfg& c, const std::string& p) {
  PanelDef d;
  float r[4];
  if (!c.Rect(p + "PanelArea", r)) return d;
  d.area = {r[0], r[1], r[2], r[3]};
  d.bg = c.Str(p + "BackgroundPanel");
  const int n = c.Int(p + "BackgroundPanelSize", 0);
  for (int i = 0; i < n; ++i) {
    float q[4];
    if (c.Rect(p + "BackgroundPanel_" + std::to_string(i), q)) d.pieces.push_back({q[0], q[1], q[2], q[3]});
    else d.pieces.push_back({0, 0, 0, 0});
  }
  d.ok = true;
  return d;
}

float Snap(float v) { return std::floor(v + 0.5f); }

// UTF-8 -> Unicode-Zeichen (ungueltige Folgen werden zu '?').
std::vector<unsigned> Utf8(const std::string& s) {
  std::vector<unsigned> out;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    unsigned cp = '?';
    size_t n = 1;
    if (c < 0x80) cp = c;
    else if ((c & 0xE0) == 0xC0 && i + 1 < s.size() && (s[i + 1] & 0xC0) == 0x80) { cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); n = 2; }
    else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) { n = 3; }
    else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) { n = 4; }
    out.push_back(cp);
    i += n;
  }
  return out;
}

void PopUtf8(std::string* s) {
  if (s->empty()) return;
  do s->pop_back(); while (!s->empty() && (static_cast<unsigned char>(s->back()) & 0xC0) == 0x80);
}

// Baut das Layout fuer eine Anordnung auf; ox, oy = Ursprung des Panels in Pixeln, s = Skalierung.
struct Builder {
  const HudData& data;
  HudLayout& L;

  void Solid(float x, float y, float w, float h, float r, float g, float b, float a) {
    HudDraw d;
    d.x = Snap(x); d.y = Snap(y);
    d.w = Snap(x + w) - d.x; d.h = Snap(y + h) - d.y;
    d.rgba[0] = r; d.rgba[1] = g; d.rgba[2] = b; d.rgba[3] = a;
    L.draws.push_back(d);
  }

  bool Sprite(const std::string& name, float x, float y, float w, float h, float tint = 1.0f) {
    HudDraw d;
    float uv[4];
    bool add = false;
    if (!data.Sprite(name, &d.tex, uv, &add)) return false;
    d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[2]; d.v1 = uv[3];
    d.additive = add;
    d.x = Snap(x); d.y = Snap(y);
    d.w = Snap(x + w) - d.x; d.h = Snap(y + h) - d.y;
    d.rgba[0] = d.rgba[1] = d.rgba[2] = tint;
    L.draws.push_back(d);
    return true;
  }

  // Sprite mit Farbe; frac < 1 schneidet von rechts ab (Balkenanzeige).
  bool SpriteBar(const std::string& name, float x, float y, float w, float h, float frac, float r, float g, float bl) {
    HudDraw d;
    float uv[4];
    bool add = false;
    if (frac <= 0 || !data.Sprite(name, &d.tex, uv, &add)) return false;
    frac = std::min(1.0f, frac);
    d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[0] + (uv[2] - uv[0]) * frac; d.v1 = uv[3];
    d.additive = add;
    d.x = Snap(x); d.y = Snap(y);
    d.w = Snap(x + w * frac) - d.x; d.h = Snap(y + h) - d.y;
    d.rgba[0] = r; d.rgba[1] = g; d.rgba[2] = bl;
    L.draws.push_back(d);
    return true;
  }

  bool Texture(const std::string& tex, float x, float y, float w, float h) {
    HudDraw d;
    d.tex = tex;
    d.x = Snap(x); d.y = Snap(y);
    d.w = Snap(x + w) - d.x; d.h = Snap(y + h) - d.y;
    L.draws.push_back(d);
    return true;
  }

  // Zeilenhoehe h_px in Pixeln. Mit Originalschrift Glyph-Rechtecke, sonst der eingebaute Zeichensatz (HudText).
  float TextW(const std::string& t, float h_px) const {
    if (!data.has_font()) {
      const int sc = std::max(1, static_cast<int>(h_px / static_cast<float>(kFontCellH) + 0.5f));
      return static_cast<float>(t.size()) * static_cast<float>(kFontCellW * sc);
    }
    float w = 0, uv[4], adv = 0;
    for (unsigned ch : Utf8(t)) {
      data.Glyph(ch, uv, &adv);
      w += adv;
    }
    return w * h_px / 10.0f;
  }

  void Text(const std::string& t, float x, float y, float h_px, float r, float g, float bl) {
    if (!data.has_font()) {
      HudText ht;
      ht.text = t; ht.x = x; ht.y = y; ht.scale = std::max(1, static_cast<int>(h_px / static_cast<float>(kFontCellH) + 0.5f));
      ht.rgb[0] = r; ht.rgb[1] = g; ht.rgb[2] = bl;
      L.texts.push_back(ht);
      return;
    }
    const float k = h_px / 10.0f;
    float cx = x, uv[4], adv = 0;
    for (unsigned ch : Utf8(t)) {
      if (data.Glyph(ch, uv, &adv) && ch != ' ') {
        HudDraw d;
        d.tex = "font";
        d.x = Snap(cx); d.y = Snap(y);
        d.w = std::max(1.0f, Snap(cx + (uv[2] - uv[0]) * 128.0f * k) - d.x);
        d.h = std::max(1.0f, Snap(y + h_px) - d.y);
        d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[2]; d.v1 = uv[3];
        d.rgba[0] = r; d.rgba[1] = g; d.rgba[2] = bl;
        L.draws.push_back(d);
      }
      cx += adv * k;
    }
  }

  // Teilstuecke des Hintergrunds.
  void Background(const PanelDef& d, float ox, float oy, float s) {
    for (size_t i = 0; i < d.pieces.size(); ++i) {
      const UiRect& p = d.pieces[i];
      if (p.w <= 0 || p.h <= 0) continue;
      Sprite(d.bg + "." + std::to_string(i), ox + p.x * s, oy + p.y * s, p.w * s, p.h * s);
    }
    L.panels.push_back({ox, oy, d.area.w * s, d.area.h * s});
  }

  // Schaltflaeche mit Symbol und Kurzbeschriftung in Rechteck r (Pixel).
  void Button(UiAction action, const std::string& icon, const std::string& label, UiRect r, bool active, bool enabled, float progress = -1.0f) {
    Solid(r.x, r.y, r.w, r.h, 0.03f, 0.05f, 0.09f, 1.0f);
    if (icon.size() > 2 && icon[0] == 'b' && icon[1] == '_') Sprite(icon, r.x, r.y, r.w, r.h);
    else if (!icon.empty()) Texture(icon, r.x, r.y, r.w, r.h);
    if (active) Solid(r.x, r.y, r.w, r.h, 0.1f, 0.85f, 0.35f, 0.38f);
    if (!enabled) Solid(r.x, r.y, r.w, r.h, 0.0f, 0.0f, 0.0f, 0.62f);
    if (progress >= 0.0f) {
      const float bh = std::max(3.0f, r.h * 0.12f);
      Solid(r.x, r.y, r.w, bh, 0.0f, 0.0f, 0.0f, 0.8f);
      Solid(r.x, r.y, r.w * std::min(1.0f, progress), bh, 0.1f, 0.6f, 1.0f, 1.0f);
    }
    if (!label.empty()) {
      float th = std::max(8.0f, r.h * 0.3f);
      float tw = TextW(label, th);
      if (tw > r.w * 0.94f) {
        th *= r.w * 0.94f / tw;
        tw = TextW(label, th);
      }
      Solid(r.x, r.y + r.h - th - 3.0f, r.w, th + 3.0f, 0.0f, 0.0f, 0.0f, 0.7f);
      Text(label, r.x + 0.5f * (r.w - tw), r.y + r.h - th - 1.0f, th, enabled ? 1.0f : 0.5f, enabled ? 0.95f : 0.5f, enabled ? 0.6f : 0.5f);
    }
    HudButtonSlot b;
    b.action = action; b.rect = r; b.active = active; b.enabled = enabled && action != UiAction::None;
    L.buttons.push_back(b);
  }
};

// Zustandsfarbe: gruen / gelb / rot.
void StateRgb(float v, float* r, float* g, float* bl) {
  if (v > 0.66f) { *r = 0.2f; *g = 1.0f; *bl = 0.2f; }
  else if (v > 0.33f) { *r = 1.0f; *g = 0.9f; *bl = 0.2f; }
  else { *r = 1.0f; *g = 0.2f; *bl = 0.2f; }
}

// Schiffsfeld wie im Original: eine Auswahl gross (Rassenleiste, Klasse, Name, Besatzung, Offiziere, Schild-/Energiebalken, Systeme),
// mehrere als bis zu acht kleine Felder. Das Drahtgittermodell fehlt noch.
void DrawShips(Builder& b, const GuiCfg& c, const std::vector<HudShip>& ships, float ix, float iy, float s, std::vector<HudButtonSlot>* btns) {
  auto focus_btn = [&](int i, float x, float y, float w, float h) {
    HudButtonSlot bs;
    bs.action = static_cast<UiAction>(static_cast<int>(UiAction::Focus0) + i);
    bs.rect = {x, y, w, h};
    btns->push_back(bs);
  };
  auto R = [&](const std::string& key, float* a) { return c.Rect(key, a); };
  float a[4];
  if (ships.size() == 1) {
    const HudShip& sh = ships[0];
    const int n = c.Int("infoSingleBackgroundSize", 0);
    for (int i = 0; i < n; ++i)
      if (R("infoSingleBackground_" + std::to_string(i), a)) b.Sprite("single_background." + std::to_string(i), ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    if (R("infoSingleRaceIconArea", a)) b.Sprite("race_icon_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    float th = std::max(6.0f, 8.0f * s);
    auto text = [&](const char* key, const std::string& t0, float r, float g, float bl, float dx = 0) {
      float q[4];
      if (!R(key, q)) return;
      q[0] += dx; q[2] -= dx;
      std::string t = t0;
      while (t.size() > 1 && b.TextW(t, th) > q[2] * s) PopUtf8(&t);
      b.Text(t, ix + q[0] * s, iy + q[1] * s, th, r, g, bl);
    };
    text("infoSingleClassTextInformationArea", sh.status.empty() ? sh.cls : sh.cls + "  " + sh.status, 1.0f, 1.0f, 1.0f);
    if (R("infoSingleNameTextBackgroundArea", a)) b.Sprite("name_label_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    text("infoSingleNameTextInformationArea", sh.name, 1.0f, 1.0f, 1.0f);
    if (R("infoSingleCrewTextBackgroundArea", a)) b.Sprite("crew_label_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    text("infoSingleCrewTextLabelArea", "Besatzung", 1, 1, 1, 8);
    text("infoSingleCrewTextInformationArea", std::to_string(sh.crew) + "/" + std::to_string(sh.crew_max), 1, 1, 1);
    if (R("infoSingleOfficerTextBackgroundArea", a)) b.Sprite("officer_label_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    text("infoSingleOfficerTextLabelArea", "Offiziere", 1, 1, 1, 2);
    text("infoSingleOfficerTextInformationArea", std::to_string(sh.officers), 1, 1, 1);
    float r, g, bl;
    if (R("infoSingleCrewDotArea", a)) { StateRgb(sh.crew_state, &r, &g, &bl); b.SpriteBar("crew_dot", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, 1, r, g, bl); }
    if (R("infoSingleShieldBarArea", a)) { StateRgb(sh.shield, &r, &g, &bl); b.SpriteBar("large_shield_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, sh.shield, r, g, bl); }
    if (R("infoSingleEnergyBarArea", a)) {
      if (sh.energy >= 0) b.SpriteBar("large_energy_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, sh.energy, 1, 1, 0);
      else b.SpriteBar("large_energy_bar", ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, 1, 0.3f, 0.3f, 0.3f);
    }
    if (R("infoSingleWireframeIconArea", a)) focus_btn(0, ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s);
    if (R("infoSingleWireframeIconArea", a))
      for (int i = 1; i <= 5; ++i) {
        float wr = 1, wg = 1, wb = 1;  // Handbuch: weiss = intakt, gelb = beschaedigt, rot = zerstoert (Teil i <-> System i, Zuordnung angenommen)
        { const float sv = sh.system[i - 1]; if (sv <= 0.0f) { wg = 0.2f; wb = 0.2f; } else if (sv < 1.0f) wb = 0.2f; }
        b.SpriteBar(sh.wire + "w" + std::to_string(i), ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, 1.0f, wr, wg, wb);
      }
    for (int i = 0; i < 5; ++i)
      if (R("infoSystem_" + std::to_string(i), a)) {
        StateRgb(sh.system[i], &r, &g, &bl);
        b.SpriteBar("info_system." + std::to_string(i), ix + a[0] * s, iy + a[1] * s, a[2] * s, a[3] * s, 1, sh.system[i] > 0.66f ? 1.0f : r, sh.system[i] > 0.66f ? 1.0f : g, sh.system[i] > 0.66f ? 1.0f : bl);
      }
    return;
  }
  const float th = std::max(5.0f, 6.0f * s);
  for (size_t i = 0; i < ships.size() && i < 8; ++i) {
    float q[4];
    if (!R("infoMultiShipIcon_" + std::to_string(i), q)) continue;
    const HudShip& sh = ships[i];
    const float x = ix + q[0] * s, y = iy + q[1] * s, w = q[2] * s, h = q[3] * s;
    focus_btn(static_cast<int>(i), x, y, w, h);
    b.Solid(x, y, w, h, 0.04f, 0.06f, 0.1f, 1.0f);
    float r, g, bl;
    StateRgb(sh.shield, &r, &g, &bl);
    if (R("infoMultiWireframeIconArea", a))
      for (int k = 1; k <= 5; ++k) {
        float wr = 1, wg = 1, wb = 1;  // Handbuch: weiss = intakt, gelb = beschaedigt, rot = zerstoert (Teil i <-> System i, Zuordnung angenommen)
        { const float sv = sh.system[k - 1]; if (sv <= 0.0f) { wg = 0.2f; wb = 0.2f; } else if (sv < 1.0f) wb = 0.2f; }
        b.SpriteBar(sh.wire + "w" + std::to_string(k), x + a[0] * s, y + a[1] * s, a[2] * s, a[3] * s, 1.0f, wr, wg, wb);
      }
    if (R("infoMultiShieldBarArea", a)) b.SpriteBar("small_shield_bar", x + a[0] * s, y + a[1] * s, a[2] * s, a[3] * s, sh.shield, r, g, bl);
    if (R("infoMultiEnergyBarArea", a)) {
      if (sh.energy >= 0) b.SpriteBar("small_energy_bar", x + a[0] * s, y + a[1] * s, a[2] * s, a[3] * s, sh.energy, 1, 1, 0);
      else b.SpriteBar("small_energy_bar", x + a[0] * s, y + a[1] * s, a[2] * s, a[3] * s, 1, 0.3f, 0.3f, 0.3f);
    }
    if (R("infoMultiCrewDotArea", a)) { StateRgb(sh.crew_state, &r, &g, &bl); b.SpriteBar("small_crew_dot", x + a[0] * s, y + a[1] * s, a[2] * s, a[3] * s, 1, r, g, bl); }
    if (sh.wire.empty()) {
      std::string t = sh.name;
      while (t.size() > 1 && b.TextW(t, th) > w - 2.0f * s) PopUtf8(&t);
      b.Text(t, x + 1.0f * s, y + 8.0f * s, th, 0.9f, 0.9f, 0.9f);
    }
  }
}


struct Slot {
  UiAction action;
  const char* icon;
  const char* label;
};

// Belegung der Testfunktionen. Index entspricht den Plaetzen im Steuerfeld (0..11) bzw. der Geschwindigkeitsleiste (0..5).
const Slot kControl[12] = {
    {UiAction::Info, "GBcompovr00", "Info"},  {UiAction::MapMenu, "GBflag00", "Karte"},    {UiAction::Team, "GBrally00", "Team"},
    {UiAction::Alpha, "GBrepair00", "Alpha"}, {UiAction::ZoomIn, "GBengovr00", "Nah"}, {UiAction::ZoomOut, "GBharve00", "Fern"},
    {UiAction::ResetView, "GBchnrxn00", "Mitte"}, {UiAction::Folder, "GBfrupgrd00", "Ordner"}, {UiAction::Race, "GBbaabore00", ""},
    {UiAction::None, "", ""},                 {UiAction::None, "", ""},                {UiAction::None, "", ""}};

}  // namespace

const char* HudRaceName(HudRace race) { return kRaceNames[static_cast<int>(race)]; }
const char* HudCfgFile(HudRace race) { return kCfgFiles[static_cast<int>(race)]; }
const char* HudSprFile(HudRace race) { return kSprFiles[static_cast<int>(race)]; }

bool HudData::Load(FileSource& src, HudRace race) {
  loaded_ = false;
  race_ = race;
  cfg_ = GuiCfg();
  spr_ = SprLibrary();
  textures_.clear();
  std::string text;
  if (!ReadText(src, std::string("misc/") + HudCfgFile(race), &text)) return false;
  cfg_.AddText(text, [&src](const std::string& name, std::string* out) { return ReadText(src, "misc/" + name, out); });
  const char* const sprs[3] = {"gui_global.spr", "gui_map.spr", HudSprFile(race)};
  for (const char* f : sprs) {
    std::string st;
    if (!ReadText(src, std::string("Sprites/") + f, &st)) continue;
    spr_.AddText(st);
    std::istringstream in(st);  // genannte Texturen sammeln
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream ls(line);
      std::string name, file, u, v, w, h;
      if (!(ls >> name >> file >> u >> v >> w >> h) || name[0] == '#' || name[0] == '@') continue;
      const std::string lf = Lower(file);
      if (std::find(textures_.begin(), textures_.end(), lf) == textures_.end()) textures_.push_back(lf);
    }
  }
  font_uv_.clear();
  {
    std::string ft;
    if (ReadText(src, "Sprites/font.spr", &ft)) {
      // Eigene Auswertung: die Schluessel sind hier Zahlenpaare (font_uv: x y) bzw. einzelne Breiten (font_w), keine Zeit/Wert-Paare.
      std::vector<std::pair<float, float>> pos;
      std::istringstream in(ft);
      std::string line, cur;
      bool keys = false;
      while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        std::string w0;
        if (!(ls >> w0)) continue;
        if (w0 == "@animation") {
          ls >> cur;
          keys = false;
        } else if (w0 == "@keyframes") {
          keys = true;
        } else if (w0[0] == '@') {
          keys = false;
        } else if (keys) {
          char* e1 = nullptr;
          const float v0 = std::strtof(w0.c_str(), &e1);
          if (e1 == w0.c_str()) continue;
          if (cur == "font_uv") {
            float v1 = 0;
            if (ls >> v1) pos.push_back({v0, v1});
          }
        }
      }
      // Die Breitentabelle (font_w) hat eine Eintrag weniger als font_uv und ist ab etwa 'M' gegenueber den Positionen verschoben.
      // Breite je Glyph daher aus den Positionen: bis zum naechsten Glyph derselben Zeile, minus 1 Pixel Abstand.
      if (pos.size() >= 96) {
        for (size_t i = 0; i < pos.size(); ++i) {
          float next = 128.0f;
          for (size_t j = 0; j < pos.size(); ++j)
            if (pos[j].second == pos[i].second && pos[j].first > pos[i].first && pos[j].first < next) next = pos[j].first;
          float w = next - pos[i].first - 1.0f;
          if (next >= 128.0f) w = std::min(5.0f, 128.0f - pos[i].first);  // letztes Glyph der Zeile
          font_uv_.push_back({pos[i].first, pos[i].second, std::max(1.0f, w)});
        }
      }
    }
  }
  float r[4];
  loaded_ = cfg_.Rect("controlPanelArea", r) && spr_.sprite_count() > 0;
  return loaded_;
}

bool HudData::Sprite(const std::string& name, std::string* tex, float uv[4], bool* additive) const {
  const SprSprite* s = spr_.FindSprite(name);
  if (!s || s->file.empty()) return false;
  const float rw = s->ref_w > 0 ? s->ref_w : 256.0f;
  const float rh = s->ref_h > 0 ? s->ref_h : rw;
  *tex = s->file;
  uv[0] = s->u / rw;
  uv[1] = s->v / rh;
  uv[2] = (s->u + s->w) / rw;
  uv[3] = (s->v + s->h) / rh;
  *additive = s->material == "additive" || s->material == "add_nomipf";
  return true;
}

bool HudData::Glyph(unsigned ch, float uv[4], float* advance) const {
  if (font_uv_.empty()) return false;
  size_t i = ch >= 32 && ch < 256 ? static_cast<size_t>(ch) - 32 : static_cast<size_t>('?') - 32;
  if (i >= font_uv_.size()) i = static_cast<size_t>('?') - 32;
  const GlyphDef& g = font_uv_[i];
  const float e = 0.2f;  // kleiner Rand nach innen: bei linearer Filterung keine Nachbarpixel
  uv[0] = (g.x + e) / 128.0f;
  uv[1] = (g.y + e) / 128.0f;
  uv[2] = (g.x + g.w - e) / 128.0f;
  uv[3] = (g.y + 10.0f - e) / 128.0f;
  *advance = g.w + 1.0f;
  return true;
}

std::vector<std::string> HudData::TextureNames() const {
  std::vector<std::string> v = textures_;
  if (has_font()) v.push_back("font");
  return v;
}

HudLayout BuildHudLayout(const HudData& data, int width, int height, const HudState& state) {
  HudLayout L;
  const float W = static_cast<float>(std::max(width, 1)), H = static_cast<float>(std::max(height, 1));
  L.portrait = H > W;
  Builder b{data, L};
  const GuiCfg& c = data.cfg();
  const PanelDef mm = ReadPanel(c, "minimap"), info = ReadPanel(c, "info"), ctl = ReadPanel(c, "control"), cin = ReadPanel(c, "cinematic"),
                 spd = ReadPanel(c, "speed"), off = ReadPanel(c, "officer"), crew = ReadPanel(c, "crew"), dil = ReadPanel(c, "dilithium");
  L.view = {0, 0, W, H};
  if (!mm.ok || !info.ok || !ctl.ok || !cin.ok || !spd.ok) return L;

  // --- Anordnung (Ursprung der Felder in Pixeln) ---
  float s = 1, mm_x, mm_y, info_x, info_y, ctl_x, ctl_y, cin_x, cin_y, spd_x, spd_y, bottom_top, view_y = 0;
  const float mm_w = mm.area.w, mm_h = mm.area.h, info_w = info.area.w, info_h = info.area.h, ctl_w = ctl.area.w, ctl_h = ctl.area.h,
              cin_w = cin.area.w, cin_h = cin.area.h, spd_w = spd.area.w, spd_h = spd.area.h;
  if (!L.portrait) {
    const float rail_top = spd_h + info_h;                    // Leiste liegt ueber dem Infofeld
    const float hud_h = std::max({rail_top, mm_h, ctl_h, cin_h});
    const float total_w = mm_w + std::max(spd_w, info_w) + cin_w + ctl_w;
    s = std::min(W / total_w, 0.36f * H / hud_h);
    s = std::max(s, 0.5f);
    if (s >= 1.0f) s = std::floor(s * 4.0f) / 4.0f;  // Viertelschritte: scharfere Kanten
    const float bottom = H;
    mm_x = 0; mm_y = bottom - mm_h * s;
    ctl_x = W - ctl_w * s; ctl_y = bottom - ctl_h * s;
    const float mid_w = (std::max(spd_w, info_w) + cin_w) * s;
    const float free_x = mm_w * s, free_w = ctl_x - free_x;
    info_x = free_x + std::max(0.0f, (free_w - mid_w) * 0.5f);
    info_y = bottom - info_h * s;
    spd_x = info_x; spd_y = info_y - spd_h * s;
    cin_x = info_x + std::max(spd_w, info_w) * s; cin_y = bottom - cin_h * s;
    bottom_top = bottom - hud_h * s;
  } else {
    // Hochformat (Anordnung "D"): Kino oben links, daneben die Ressourcen, darunter die Leiste; unten links die Minikarte,
    // unten rechts das Infofeld, darueber am rechten Rand das Steuerfeld. Das 3D-Bild liegt dazwischen (und dahinter).
    const float res_w = 3.0f * std::max({dil.ok ? dil.area.w : 0.0f, crew.ok ? crew.area.w : 0.0f, off.ok ? off.area.w : 0.0f});
    const float res_h = std::max({dil.ok ? dil.area.h : 0.0f, crew.ok ? crew.area.h : 0.0f, off.ok ? off.area.h : 0.0f});
    // Die Leiste (Spezialwaffen, Bauwarteschlange) sitzt direkt ueber dem Infofeld, das Steuerfeld darueber.
    const float top_w = cin_w + res_w, bot_w = mm_w + std::max(info_w, spd_w);
    const float top_h = std::max(cin_h, res_h), bot_h = std::max(mm_h, info_h + spd_h + ctl_h);
    s = std::min(W / std::max(top_w, bot_w), 0.46f * H / (top_h + bot_h));
    s = std::max(s, 0.5f);
    if (s >= 1.0f) s = std::floor(s * 4.0f) / 4.0f;
    const float bottom = H;
    cin_x = 0; cin_y = 0;
    mm_x = 0; mm_y = bottom - mm_h * s;
    info_x = W - info_w * s; info_y = bottom - info_h * s;
    spd_x = W - spd_w * s; spd_y = info_y - spd_h * s;
    ctl_x = W - ctl_w * s; ctl_y = spd_y - ctl_h * s;
    view_y = std::max(cin_h, res_h) * s;
    bottom_top = std::min(mm_y, ctl_y);
  }
  L.scale = s;
  L.view = {0, view_y, W, std::max(1.0f, bottom_top - view_y)};

  // --- Ressourcenanzeigen oben rechts (Dilithium, Mannschaft, Offiziere) ---
  {
    const float rw = std::max({dil.ok ? dil.area.w : 0.0f, crew.ok ? crew.area.w : 0.0f, off.ok ? off.area.w : 0.0f});
    const float st = std::min(s, W / std::max(1.0f, 3.0f * rw));
    const PanelDef* const rp[3] = {&dil, &crew, &off};
    const char* const names[3] = {"dilithium", "crew", "officer"};
    const char* const vals[3] = {"9999", "9999", "99"};
    float x = L.portrait ? cin_x + cin_w * s : W - rw * st * 3.0f;
    for (int i = 0; i < 3; ++i, x += rw * st) {
      if (!rp[i]->ok) continue;
      b.Background(*rp[i], x, 0, st);
      const std::string n = names[i];
      float ia[4], ta[4];
      if (c.Rect(n + "IconArea", ia)) b.Sprite(c.Str(n + "Icon"), x + ia[0] * st, ia[1] * st, ia[2] * st, ia[3] * st);
      if (c.Rect(n + "TextArea", ta)) {
        const float th = ta[3] * st * 0.85f;
        const std::string v = state.res[i] >= 0 ? std::to_string(state.res[i]) : std::string(vals[i]);
        b.Text(v.c_str(), x + ta[0] * st, ta[1] * st + 0.5f * (ta[3] * st - th), th, 1.0f, 0.8f, 0.2f);
      }
    }
  }

  // --- Minikarte ---
  {
    float da[4] = {9, 9, 128, 128};
    c.Rect("minimapDisplayArea", da);
    b.Solid(mm_x + da[0] * s, mm_y + da[1] * s, da[2] * s, da[3] * s, 0.01f, 0.02f, 0.06f, 1.0f);
    L.minimap = {mm_x + da[0] * s, mm_y + da[1] * s, da[2] * s, da[3] * s};
    b.Background(mm, mm_x, mm_y, s);
    const float dot = std::max(2.0f, std::floor(1.5f * s));
    for (const HudDot& d : state.dots)
      b.Solid(mm_x + (da[0] + d.nx * da[2]) * s - 0.5f * dot, mm_y + (da[1] + d.ny * da[3]) * s - 0.5f * dot, dot, dot, d.rgb[0], d.rgb[1], d.rgb[2], 1.0f);
    if (state.has_view_quad) {  // Kamerasicht: weisser Umriss, an den Rand der Minikarte gekappt
      const float lw = std::max(1.0f, std::floor(s));
      for (int e = 0; e < 4; ++e) {
        float x0 = state.view_quad[e * 2], y0 = state.view_quad[e * 2 + 1];
        const float x1 = state.view_quad[((e + 1) % 4) * 2], y1 = state.view_quad[((e + 1) % 4) * 2 + 1];
        const float dx = x1 - x0, dy = y1 - y0;
        float t0 = 0, t1 = 1;  // Liang-Barsky gegen [0,1]^2
        const float p[4] = {-dx, dx, -dy, dy}, q[4] = {x0, 1 - x0, y0, 1 - y0};
        bool ok = true;
        for (int k = 0; k < 4 && ok; ++k) {
          if (std::fabs(p[k]) < 1e-9f) { if (q[k] < 0) ok = false; continue; }
          const float r = q[k] / p[k];
          if (p[k] < 0) t0 = std::max(t0, r); else t1 = std::min(t1, r);
          if (t0 > t1) ok = false;
        }
        if (!ok) continue;
        const float ax = x0 + t0 * dx, ay = y0 + t0 * dy, bx = x0 + t1 * dx, by = y0 + t1 * dy;
        const float len = std::sqrt((bx - ax) * da[2] * (bx - ax) * da[2] + (by - ay) * da[3] * (by - ay) * da[3]) * s;
        const int steps = std::max(1, static_cast<int>(len / lw));
        for (int k = 0; k <= steps; ++k) {
          const float f = static_cast<float>(k) / static_cast<float>(steps);
          b.Solid(mm_x + (da[0] + (ax + (bx - ax) * f) * da[2]) * s - 0.5f * lw, mm_y + (da[1] + (ay + (by - ay) * f) * da[3]) * s - 0.5f * lw, lw, lw, 1.0f, 1.0f, 1.0f, 0.9f);
        }
      }
    }
  }

  // --- Infofeld ---
  {
    float ba[4] = {10, 10, 229, 95};
    c.Rect("infoBlackArea", ba);
    b.Solid(info_x + ba[0] * s, info_y + ba[1] * s, ba[2] * s, ba[3] * s, 0.0f, 0.0f, 0.0f, 1.0f);
    b.Background(info, info_x, info_y, s);
    if (!state.ships.empty()) {
      DrawShips(b, c, state.ships, info_x, info_y, s, &L.buttons);
    } else {
    const float aw = (ba[2] - 8) * s, ah = (ba[3] - 6) * s;
    float th = std::max(8.0f, std::min(aw / 34.0f * (data.has_font() ? 10.0f / 6.0f : static_cast<float>(kFontCellH) / kFontCellW), ah / 4.4f));
    if (!data.has_font()) th = static_cast<float>(kFontCellH) * std::max(1.0f, std::floor(th / static_cast<float>(kFontCellH)));
    const float pitch = th * 1.3f;
    const int rows = std::max(1, static_cast<int>(ah / pitch));
    for (int i = 0; i < rows && i < static_cast<int>(state.info_lines.size()); ++i) {
      std::string t = state.info_lines[static_cast<size_t>(i)];
      while (t.size() > 1 && b.TextW(t, th) > aw) PopUtf8(&t);
      b.Text(t, info_x + (ba[0] + 4) * s, info_y + (ba[1] + 3) * s + static_cast<float>(i) * pitch, th, i == 0 ? 1.0f : 0.75f,
             i == 0 ? 0.55f : 0.85f, i == 0 ? 0.2f : 1.0f);
    }
    }
  }

  // --- Geschwindigkeitsleiste: sechs Taertasten ---
  {
    b.Background(spd, spd_x, spd_y, s);
    float ba[4] = {15, 4, 32, 32};
    c.Rect("speedSingleButtonArea", ba);
    const int gap = c.Int("speedSingleButtonGap", 3);
    const bool map = state.ui.map_mode;
    const Slot sl[6] = {{UiAction::PrevModel, "GBcancel00", "<"},
                        {UiAction::NextModel, "GBbuild00", ">"},
                        {map ? UiAction::MapInfo : UiAction::Variant, "GBresrch00", map ? "Info" : "Var"},
                        {UiAction::Hardpoints, "GBbsensor00", "HP"},
                        {UiAction::Fire, "GBbspec00", "Feuer"},
                        {UiAction::Mute, "GBenrcvr00", "Ton"}};
    const Slot ms[6] = {{UiAction::Spawn, "GBbuild00", "Neu"},   {UiAction::MapMenu, "GBflag00", "Karte"},   {UiAction::MapInfo, "GBresrch00", "Info"},
                        {UiAction::Stop, "GBbsensor00", "Stopp"}, {UiAction::Delete, "GBcancel00", "Weg"}, {UiAction::Mute, "GBenrcvr00", "Ton"}};
    if (state.commands) {
      for (int i = 0; i < 6; ++i) {
        const float bx = spd_x + (ba[0] + static_cast<float>(i) * (ba[2] + static_cast<float>(gap))) * s;
        UiRect rr = {bx, spd_y + ba[1] * s, ba[2] * s, ba[3] * s};
        if (static_cast<size_t>(i) < state.rail.size() && state.rail[static_cast<size_t>(i)].action != UiAction::None) {
          const HudCmd& cm = state.rail[static_cast<size_t>(i)];
          b.Button(cm.action, cm.sprite, cm.label, rr, cm.active, cm.enabled, cm.progress);
        }
      }
    }
    for (int i = 0; i < 6 && !state.commands; ++i) {
      const Slot& cur = map ? ms[i] : sl[i];
      UiAction a = cur.action;
      bool active = false, enabled = true;
      if (a == UiAction::Hardpoints) active = state.ui.hardpoints;
      if (a == UiAction::Fire) active = state.ui.firing;
      if (a == UiAction::Mute) active = state.ui.muted;
      const float bx = spd_x + (ba[0] + static_cast<float>(i) * (ba[2] + static_cast<float>(gap))) * s;
      b.Button(a, cur.icon, cur.label, {bx, spd_y + ba[1] * s, ba[2] * s, ba[3] * s}, active, enabled);
    }
  }

  // --- Kinosicht (Bild + Menue/Komm-Tasten) ---
  {
    float da[4] = {9, 35, 113, 80};
    c.Rect("cinematicDisplayArea", da);
    b.Sprite(c.Str("cinematicDefaultBackground"), cin_x + da[0] * s, cin_y + da[1] * s, da[2] * s, da[3] * s);
    b.Background(cin, cin_x, cin_y, s);
    float mb[4], cb[4];
    if (c.Rect("cinematicMenuButtonArea", mb)) {
      b.Sprite(c.Str("cinematicMenuButton"), cin_x + mb[0] * s, cin_y + mb[1] * s, mb[2] * s, mb[3] * s);
      const float th = mb[3] * s * 0.7f, tw = b.TextW("Men\u00fc", th);
      b.Text("Men\u00fc", cin_x + mb[0] * s + 0.5f * (mb[2] * s - tw), cin_y + mb[1] * s + 0.5f * (mb[3] * s - th), th, 1, 1, 1);
      HudButtonSlot bs;
      bs.action = state.commands ? UiAction::Menu : UiAction::Folder; bs.rect = {cin_x + mb[0] * s, cin_y + mb[1] * s, mb[2] * s, mb[3] * s};
      L.buttons.push_back(bs);
    }
    if (c.Rect("cinematicCommButtonArea", cb)) {
      b.Sprite(c.Str("cinematicCommButton"), cin_x + cb[0] * s, cin_y + cb[1] * s, cb[2] * s, cb[3] * s);
      if (state.ui.muted) b.Sprite(c.Str("cinematicCommBorder"), cin_x + cb[0] * s, cin_y + cb[1] * s, cb[2] * s, cb[3] * s);
      const char* clabel = state.commands ? "Komm" : "Ton";
      const float th = cb[3] * s * 0.7f, tw = b.TextW(clabel, th);
      b.Text(clabel, cin_x + cb[0] * s + 0.5f * (cb[2] * s - tw), cin_y + cb[1] * s + 0.5f * (cb[3] * s - th), th, 1, 1, 1);
      HudButtonSlot bs;
      bs.action = state.commands ? UiAction::None : UiAction::Mute; bs.active = state.ui.muted && !state.commands; bs.rect = {cin_x + cb[0] * s, cin_y + cb[1] * s, cb[2] * s, cb[3] * s};
      L.buttons.push_back(bs);
    }
  }

  // --- Steuerfeld: 12 Tasten ---
  {
    float ba[4] = {8, 8, 100, 133};
    c.Rect("controlBlackArea", ba);
    b.Solid(ctl_x + ba[0] * s, ctl_y + ba[1] * s, ba[2] * s, ba[3] * s, 0.0f, 0.0f, 0.0f, 1.0f);
    b.Background(ctl, ctl_x, ctl_y, s);
    for (int i = 0; i < 12 && state.commands; ++i) {
      float r[4];
      if (!c.Rect("controlButton" + std::to_string(i + 1), r)) continue;
      if (static_cast<size_t>(i) >= state.control.size() || state.control[static_cast<size_t>(i)].action == UiAction::None) continue;
      const HudCmd& cm = state.control[static_cast<size_t>(i)];
      b.Button(cm.action, cm.sprite, cm.label, {ctl_x + r[0] * s, ctl_y + r[1] * s, r[2] * s, r[3] * s}, cm.active, cm.enabled);
    }
    for (int i = 0; i < 12 && !state.commands; ++i) {
      float r[4];
      if (!c.Rect("controlButton" + std::to_string(i + 1), r)) continue;
      const Slot& sl = kControl[i];
      const bool enabled = sl.action != UiAction::None;
      std::string label = sl.label;
      if (sl.action == UiAction::Race) label = state.race_label;
      b.Button(sl.action, sl.icon, label, {ctl_x + r[0] * s, ctl_y + r[1] * s, r[2] * s, r[3] * s}, false, enabled);
    }
  }
  return L;
}

HudTextBlock LayoutHudText(const HudData& data, const std::string& utf8, float x, float y, float h_px, float max_w, const float rgb[3],
                           const float panel_rgba[4]) {
  HudTextBlock out;
  if (!data.has_font() || h_px < 1) return out;
  const float k = h_px / 10.0f, pitch = h_px * 1.3f;
  auto width = [&](const std::vector<unsigned>& cps) {
    float w = 0, uv[4], adv = 0;
    for (unsigned c : cps) { data.Glyph(c, uv, &adv); w += adv; }
    return w * k;
  };
  // Zeilen umbrechen: erst an Zeilenenden, dann greedy an Leerzeichen, zu lange Woerter zeichenweise
  std::vector<std::vector<unsigned>> lines;
  std::vector<unsigned> cur, word;
  auto flush_word = [&]() {
    if (word.empty()) return;
    if (!cur.empty() && width(cur) + width({' '}) + width(word) > max_w) { lines.push_back(cur); cur.clear(); }
    if (!cur.empty()) cur.push_back(' ');
    for (unsigned c : word) {
      cur.push_back(c);
      if (width(cur) > max_w && cur.size() > 1) { cur.pop_back(); lines.push_back(cur); cur.assign(1, c); }
    }
    word.clear();
  };
  for (unsigned c : Utf8(utf8)) {
    if (c == '\n') { flush_word(); lines.push_back(cur); cur.clear(); }
    else if (c == ' ') flush_word();
    else word.push_back(c);
  }
  flush_word();
  if (!cur.empty()) lines.push_back(cur);
  float maxw = 0;
  for (const auto& l : lines) maxw = std::max(maxw, width(l));
  out.w = maxw;
  out.h = lines.empty() ? 0.0f : static_cast<float>(lines.size() - 1) * pitch + h_px;
  if (panel_rgba && !lines.empty()) {
    HudDraw p;
    const float pad = 0.35f * h_px;
    p.x = Snap(x - pad); p.y = Snap(y - pad); p.w = Snap(maxw + 2 * pad); p.h = Snap(out.h + 2 * pad);
    for (int i = 0; i < 4; ++i) p.rgba[i] = panel_rgba[i];
    out.draws.push_back(p);
  }
  float ly = y;
  for (const auto& l : lines) {
    float cx = x, uv[4], adv = 0;
    for (unsigned c : l) {
      if (data.Glyph(c, uv, &adv) && c != ' ') {
        HudDraw d;
        d.tex = "font";
        d.x = Snap(cx); d.y = Snap(ly);
        d.w = std::max(1.0f, Snap(cx + (uv[2] - uv[0]) * 128.0f * k) - d.x);
        d.h = std::max(1.0f, Snap(ly + h_px) - d.y);
        d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[2]; d.v1 = uv[3];
        for (int i = 0; i < 3; ++i) d.rgba[i] = rgb[i];
        out.draws.push_back(d);
      }
      cx += adv * k;
    }
    ly += pitch;
  }
  return out;
}

UiAction HudHit(const HudLayout& layout, float x, float y) {
  for (const HudButtonSlot& b : layout.buttons)
    if (b.enabled && b.action != UiAction::None && b.rect.Contains(x, y)) return b.action;
  return UiAction::None;
}

bool HudCovers(const HudLayout& layout, float x, float y) {
  for (const UiRect& r : layout.panels)
    if (r.Contains(x, y)) return true;
  return false;
}

}  // namespace fleet
