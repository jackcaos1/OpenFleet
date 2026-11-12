#include "game/Game.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <array>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <cstdlib>

#include "core/Log.h"
#include "core/Math.h"
#include "data/UnitDef.h"
#include "formats/Bzn.h"
#include "formats/SaveGame.h"
#include "formats/Spr.h"
#include "audio/Mixer.h"
#include "formats/Wav.h"
#include "game/MapScene.h"
#include "game/Ui.h"
#include "game/Hud.h"
#include "game/HoverShip.h"
#include "game/PathGrid.h"
#include "game/ShipSim.h"
#include "render/HudRenderer.h"
#include "formats/ZfsSource.h"
#include "formats/Sod.h"
#include "model/ModelBuilder.h"
#include "render/Gl.h"
#include "core/Version.h"
#include "render/ModelRenderer.h"
#include "render/SpriteRenderer.h"
#include "game/Combat.h"
#include "game/Shockwave.h"
#include "game/Shell.h"
#include "game/FireDemo.h"
#include "render/EffectRenderer.h"
#include "formats/Odf.h"
#include "render/MarkerRenderer.h"
#include "render/TextRenderer.h"
#include "render/TriangleRenderer.h"

namespace fleet {

namespace {

const float kPi = 3.14159265f;
const float kTapMaxSeconds = 0.35f;

struct NamedColor {
  const char* name;
  float rgb[3];
};
const NamedColor kTeamColors[] = {{"weiss (Textur unveraendert)", {1.0f, 1.0f, 1.0f}}, {"orange", {1.0f, 0.54f, 0.22f}},
                                  {"rot", {1.0f, 0.1f, 0.1f}},   {"blau", {0.25f, 0.45f, 1.0f}},
                                  {"gruen", {0.2f, 0.9f, 0.3f}}, {"gelb", {1.0f, 0.95f, 0.2f}}};
const char* const kAlphaModes[] = {"Alpha ignorieren",
                                   "Alpha-Test (Alpha < 0.5 durchsichtig)",
                                   "Alpha mischen (transparent)",
                                   "Alpha = Emission (leuchtet wo Alpha hoch)",
                                   "Alpha = Emission invertiert (leuchtet wo Alpha niedrig)",
                                   "Alpha = Hoehenkarte (Bump, Normalen aus Alpha)"};
const int kAlphaModeCount = static_cast<int>(sizeof(kAlphaModes) / sizeof(kAlphaModes[0]));
const float kTapMaxMovePixels = 18.0f;
// Teamfarben in der Kartenansicht (0 = neutral); Zuordnung zu den Voelkern ist nicht belegt.
const float kMapTeamColors[9][3] = {{0.8f, 0.8f, 0.8f}, {0.3f, 1.0f, 0.4f}, {1.0f, 0.3f, 0.3f}, {0.35f, 0.55f, 1.0f}, {1.0f, 0.95f, 0.3f},
                                    {1.0f, 0.6f, 0.2f}, {1.0f, 0.4f, 1.0f}, {0.3f, 1.0f, 1.0f}, {0.6f, 0.6f, 0.6f}};

struct Touch {
  bool ui = false;  // Beruehrung begann auf der Bedienleiste: nicht als Ziehen/Drehen auswerten
  float x = 0, y = 0;
  float start_x = 0, start_y = 0;
  double start_time = 0;
  bool moved = false;
  float pivot[3] = {0, 0, 0};  // Drehpunkt der Geste in der Karte: Bodenpunkt unter dem Finger bei Beginn
};

}  // namespace

namespace {
// Drehung um eine Einheitsachse (Rodrigues), Spaltenanordnung wie Mat4.
Mat4 AxisAngle(const float a[3], float angle) {
  const float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
  Mat4 r;
  r.m[0] = t * a[0] * a[0] + c;         r.m[4] = t * a[0] * a[1] - s * a[2];  r.m[8] = t * a[0] * a[2] + s * a[1];
  r.m[1] = t * a[0] * a[1] + s * a[2];  r.m[5] = t * a[1] * a[1] + c;         r.m[9] = t * a[1] * a[2] - s * a[0];
  r.m[2] = t * a[0] * a[2] - s * a[1];  r.m[6] = t * a[1] * a[2] + s * a[0];  r.m[10] = t * a[2] * a[2] + c;
  return r;
}
// Original: 0,01 rad je Bild; mit 30 Bildern/s (Annahme, wie bei den Nebel-Lichtern) = 0,3 rad/s
constexpr float kFireballSpin = 0.3f;
}  // namespace

struct Game::Impl {
  GameHost* host = nullptr;
  std::shared_ptr<FileSource> source;
  TriangleRenderer triangle;  // Platzhalter solange kein Modell geladen ist
  ModelRenderer models;
  SpriteRenderer sprites;  // Positionslichter u. a.
  TextRenderer text;
  MarkerRenderer markers;  // Hardpoints
  EffectRenderer effects;  // Waffeneffekte
  FireDemo fire;
  bool show_hardpoints = false;
  std::string hardpoint_legend;
  std::string overlay;       // eingeblendeter Text
  double overlay_until = 0;  // Uhrzeit (clock), bis zu der er sichtbar bleibt
  SprLibrary spr;
  std::shared_ptr<ZfsArchive> odf_zfs;
  std::unique_ptr<ZfsSource> odf_source;
  std::unique_ptr<OdfLibrary> odf_lib;  // ODFs aus assets.zfs
  bool gl_ready = false;

  std::vector<std::string> model_files;  // Dateinamen in SOD/
  int current = -1;
  bool source_dirty = false;
  std::string pending_select;

  Model model;
  Sod sod;
  std::string model_name;
  std::vector<ModelVariant> variants;
  int variant = 0;
  int model_number = 0;
  int width = 1, height = 1;
  float angle = 0.0f;
  int frames = 0;
  double clock = 0.0;

  // Kamera
  float yaw = 0.5f, pitch = 0.35f, zoom = 1.0f;
  float radius = 1.0f;
  float view_pan[2] = {0, 0};  // Modellansicht: Verschiebung (Bildebene, Welteinheiten)
  float center[3] = {0, 0, 0};
  float idle = 0.0f;  // Sekunden seit der letzten Eingabe
  int alpha_mode = 0;
  int team_color = 4;  // gruen, wie im Vergleichsbild; die Spielerfarbe ist frei waehlbar

  std::map<int, Touch> touches;
  float pinch_start_dist = 0, pinch_start_zoom = 1;

  // --- Ton ---
  Mixer mixer;
  bool muted = false;
  std::map<std::string, std::shared_ptr<const PcmClip>> clips;  // Name (klein) -> Clip; nullptr = nicht ladbar
  Mixer::VoiceId music_voice = Mixer::kNone;
  struct Ambient {
    std::shared_ptr<const PcmClip> clip;
    std::vector<float> pos;  // x,y,z je Quelle (GL-Welt)
    Mixer::VoiceId voice = Mixer::kNone;
  };
  std::vector<Ambient> ambients;

  // Sucht einen Klang in den bekannten Ordnern und decodiert ihn einmal.
  std::shared_ptr<const PcmClip> LoadSound(const std::string& name) {
    if (!source || name.empty()) return nullptr;
    const std::string key = ToLowerAscii(name);
    auto it = clips.find(key);
    if (it != clips.end()) return it->second;
    std::shared_ptr<const PcmClip> result;
    static const char* const kDirs[] = {"sounds/effects/", "sounds/music/", "sounds/voiceover/ingame/", "wav/", ""};
    for (const char* d : kDirs) {
      std::vector<uint8_t> bytes;
      if (!source->ReadFile(std::string(d) + name, &bytes)) continue;
      auto clip = std::make_shared<PcmClip>();
      if (ParseWav(bytes.data(), bytes.size(), clip.get(), nullptr)) {
        result = clip;
        break;
      }
    }
    clips[key] = result;
    return result;
  }

  void PlayEffect(const std::string& name, float gain, float pan = 0.0f) {
    if (muted) return;
    auto c = LoadSound(name);
    if (c) mixer.Play(c, gain, pan, false);
  }

  void StopMapAudio() {
    if (music_voice != Mixer::kNone) mixer.Stop(music_voice);
    music_voice = Mixer::kNone;
    for (Ambient& a : ambients) mixer.Stop(a.voice);
    ambients.clear();
  }

  // Hintergrundmusik der Karte (je Volk eine Reihe Stuecke, Annahme: die laengste "<volk>300" ist die Dauerschleife).
  void StartMapMusic() {
    if (music_voice != Mixer::kNone) mixer.Stop(music_voice);
    music_voice = Mixer::kNone;
    std::string r = ToLowerAscii(map.race), prefix = "Mfed";
    if (r.find("borg") == 0) prefix = "Mbor";
    else if (r.find("klingon") == 0) prefix = "Mkli";
    else if (r.find("romulan") == 0) prefix = "MRom";
    auto c = LoadSound(prefix + "300.wav");
    if (!c) c = LoadSound(prefix + "115.wav");
    if (c) music_voice = mixer.Play(c, muted ? 0.0f : 0.45f, 0.0f, true);
  }

  // Dauergeraeusche der Kartenobjekte (ambientSound aus der ODF: Nebel, Schwarze Loecher, Wurmloecher).
  void StartMapAmbients() {
    for (Ambient& a : ambients) mixer.Stop(a.voice);
    ambients.clear();
    if (!odf_lib) return;
    std::map<std::string, size_t> by_sound;
    for (const BznObject& o : map.objects) {
      const OdfObject* od = odf_lib->Get(ToLowerAscii(o.odf));
      std::string snd;
      if (!od || !od->GetString("ambientSound", &snd) || snd.empty()) continue;
      const std::string key = ToLowerAscii(snd);
      auto it = by_sound.find(key);
      if (it == by_sound.end()) {
        Ambient a;
        a.clip = LoadSound(snd);
        if (!a.clip) {
          by_sound[key] = static_cast<size_t>(-1);
          continue;
        }
        by_sound[key] = ambients.size();
        ambients.push_back(std::move(a));
        it = by_sound.find(key);
      }
      if (it->second == static_cast<size_t>(-1)) continue;
      Ambient& a = ambients[it->second];
      a.pos.push_back(o.pos[0]);
      a.pos.push_back(o.pos[1]);
      a.pos.push_back(-o.pos[2]);
    }
    for (Ambient& a : ambients) a.voice = mixer.Play(a.clip, 0.0f, 0.0f, true);
  }

  // Lautstaerke/Schwenk der Dauergeraeusche nach Abstand zum Blickpunkt (target), je Klang die naechste Quelle.
  void UpdateAmbients(const Mat4& rot) {
    const float range = 1500.0f;
    for (Ambient& a : ambients) {
      float best = 1e30f, bx = 0, by = 0, bz = 0;
      for (size_t i = 0; i + 2 < a.pos.size(); i += 3) {
        const float dx = a.pos[i] - map_target[0], dy = a.pos[i + 1] - map_target[1], dz = a.pos[i + 2] - map_target[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best) {
          best = d2;
          bx = dx;
          by = dy;
          bz = dz;
        }
      }
      const float d = std::sqrt(best);
      float g = std::max(0.0f, 1.0f - d / range);
      g = g * g * 0.7f;
      const float right = (rot.m[0] * bx + rot.m[4] * by + rot.m[8] * bz) / (d + 50.0f);
      mixer.Set(a.voice, muted ? 0.0f : g, std::max(-0.8f, std::min(0.8f, right)));
    }
  }

  // --- Kartenansicht (BZN): Objekte der Karte als SOD-Modelle an ihrer Position ---
  bool map_mode = false;
  std::vector<std::string> map_files;
  int map_current = -1;
  bool map_loaded = false;
  BznMap map;
  MapScene scene;
  std::map<std::string, std::string> sod_lower;  // kleiner Name -> Dateiname in SOD/
  std::vector<std::unique_ptr<ModelRenderer>> map_models;  // je scene.models (nullptr = nicht ladbar)
  std::vector<std::unique_ptr<SpriteRenderer>> map_sprites;  // je scene.models: Sprite-Knoten (Nebel, Lichter), sonst nullptr
  std::unique_ptr<ModelRenderer> map_bg;
  std::unique_ptr<ModelRenderer> map_stars;  // Sternen-Himmel (MbgStars), liegt immer hinter dem Hintergrund der Karte
  float map_stars_center[3] = {0, 0, 0}, map_stars_radius = 1;
  float map_bg_center[3] = {0, 0, 0}, map_bg_radius = 1;
  MarkerRenderer map_markers;  // Objekte ohne Modell
  static constexpr float kTapPlaneY = 0.0f;  // Ebene fuer Tippen/Ziele (bis sie veraenderbar ist, immer 0)
  float map_target[3] = {0, 0, 0}, map_dist = 3000, map_yaw = 0.6f, map_pitch = 0.8f;

  // --- Gesetzte Objekte (Schiffe/Gebaeude, frei auf der Karte) ---
  // Waffenplatz im Gefecht (aus Waffen- und Geschoss-ODF)
  struct WeaponRt {
    std::string name, sound;
    float range = 300, delay = 2, timer = 0, width = 2, length = 0, speed = 0, life = 2, variance = 0;
    bool beam = false, ignore_shield = false;
    PerTarget damage, hit;
    HitInfo hi;  // Schwelle und Besatzungsfaktoren aus der Ordnance-ODF (damage bleibt leer)
    std::vector<ModelHardpoint> points;  // Modellraum
    const SprSprite* sprite = nullptr;
    const SprAnimation* anim = nullptr;
    bool has_light = false;
    float lcol[3] = {0, 0, 0}, lstart = 0, lrange = 0;  // Punktlicht des Geschosses (ODF lightColor/lightFalloff*)
  };
  // Glow-Licht einer Nebel-/Sturmwolke (Armada.exe 0x451e00 Auswahl, 0x452500 Farbe, 0x4522f0 Phase)
  struct NebLight {
    float pos[3] = {0, 0, 0}, base[3] = {0, 0, 0}, swing[3] = {0, 0, 0};
    float start = 140, range = 10;
    float freq = 0.319f, min_step = 0.0025f, step_range = 0.619f, rate = 0.0005f;
    double phase = 0;
  };
  std::vector<NebLight> neb_lights;
  double neb_ticks = 0;
  struct Projectile {
    Shot vis;
    bool has_light = false;
    float lcol[3] = {0, 0, 0}, lstart = 0, lrange = 0;
    unsigned owner = 0, target = 0;
    HitInfo hi;
    bool will_hit = true;
  };
  struct Spawned {
    unsigned id = 0;
    Health hp;
    unsigned target_id = 0;  // Angriffsziel (0 = keins)
    float retarget = 0;
    std::vector<WeaponRt> weapons;
    std::string odf_l;
    std::string odf, sod, label;
    int team = 1;
    float scale = 1;
    ShipState st;
    FlightParams fp;
    HoverOdf hodf;     // Original-Flugphysik (HoverSim), aus der Physik-ODF
    bool hodf_ok = false;
    HoverShip hv;
    bool movable = false;
    int officers = 0;
    bool derelict = false;
    bool hero = false;   // ODF isHero
    bool dying = false;  // zerstoert: Sterbephase laeuft (Combat.h, Dying), danach wird das Objekt entfernt
    Dying die;
    unsigned beam_id = 0;  // Transporter: Ziel, zu dem Besatzung gebeamt wird
    float beam_timer = 0;
    unsigned attacker_id = 0;
    float attacked_t = 0;  // Sekunden, in denen das Schiff (oder ein Nachbar) als angegriffen gilt
    int auto_move = 0;     // 0 keine, 1 Verfolgung, 2 Flucht, 3 Rueckkehr zum Ankerpunkt
    float anchor[3] = {0, 0, 0};
    float scan = 500, scan_damaged = 125;
    std::string expl_sound;
    std::string expl_model = "xplode";  // dbName der fireball-ODF
    bool has_shock = false;
    ShockwaveDef shock;  // Schockwelle der fireball-ODF (grosse Schiffe)
    float expl_len = 2.5f;
    bool has_energy = false;
    std::string cls;  // Klassenbezeichnung fuer das Schiffsfeld
    bool sel = false;  // gehoert zur Auswahl (selected = Hauptobjekt fuer Info/Befehlsleiste)
    int alert = 2;  // 0 gruen, 1 gelb, 2 rot (Standard), 3 Abwarten
    std::vector<std::pair<std::string, std::string>> specials;  // Waffen-ODF, Anzeigename
  };
  struct SpawnModel {
    std::unique_ptr<ModelRenderer> r;
    std::unique_ptr<SpriteRenderer> sp;
    float radius = 50;
    float bounds[6] = {0, 0, 0, 0, 0, 0};   // Bounding-Box (GL: min xyz, max xyz), ungeskaliert
    std::vector<ModelHardpoint> hardpoints;
    std::vector<float> tris;  // Dreiecke des Modells (je 9 Werte, Modellraum GL), Ort der Sterbeeffekte
  };
  struct SpawnEntry {
    std::string odf, label, race;
  };
  struct MenuRow {
    std::string label, key;
  };
  enum class MenuKind { None, Spawn, Maps, Options, Advanced, Debug };
  enum class CmdPage { Main, Orders };
  CmdPage cmd_page = CmdPage::Main;
  bool move_armed = false;
  bool attack_armed = false;
  bool transport_armed = false;
  mutable float mm_centre[2] = {0, 0};  // Zuordnung Minikarte <-> Welt (zuletzt in BuildHudState berechnet)
  mutable float mm_extent = 0;
  std::vector<Projectile> projectiles;
  std::vector<Shot> fx;  // Strahlen und Explosionen (Geschosse stehen in projectiles)
  // Endexplosion eines Schiffs: das Sprite-Modell der fireball-ODF (dbName, z. B. Xplode.sod), Animation ab Alter 0
  struct Fireball {
    std::string model;
    float pos[3] = {0, 0, 0};
    float scale = 1, age = 0, life = 2.5f;
    float axis[3] = {0, 1, 0};   // zufaellige Drehachse (Original 0x51b290: 0,01 rad je Bild um eine Zufallsachse)
    float radius = 0;            // Radius des zerstoerten Schiffs (Objektmassstab der Schockwelle)
    bool shock_pending = false;  // Schockwelle noch nicht erzeugt
    ShockwaveDef shock;
  };
  std::vector<Fireball> fireballs;
  struct ShockInst {
    ShockwaveDef def;
    Shockwave sw;
  };
  std::vector<ShockInst> shockwaves;
  Rng rng;
  unsigned next_id = 1;
  // Hauptmenue vor dem Spiel
  bool in_game = false;
  ShellScreen shell_screen = ShellScreen::Main;
  std::string shell_pressed;
  ShellLayout shell;
  float nebulae_coef = 1.8f, blackhole_coef = 1.5f;   // NEBULAE_/BLACKHOLE_PATHING_SAFETY_COEFFICIENT (RTS_CFG.h)
  int avoid_user = -1;     // Erweitert: Hindernisvermeidung (OBSTACLE_AVOIDANCE) von Hand; -1 = Wert aus RTS_CFG.h
  bool tap_moves = false;  // Erweitert: einfaches Antippen des Bodens schickt Schiffe (Standard aus, nur nach Bewegen-Taste)
  std::vector<Spawned> spawned;
  std::vector<char> scene_hidden;  // je scene.instances: ausgewaehlt und als Spawned uebernommen, nicht mehr als Kartenobjekt zeichnen
  ModelRenderer::SceneLighting map_light;   // Karten-Ambient + dlight-Objekte (Armada.exe 0x5a4410)
  std::vector<float> map_orig_radius;   // je scene.models: Kugelradius wie im Original (0x5967f0/0x5520f0), fuer Planeten-Stempel
  std::vector<float> map_radius;     // je scene.models: Modellradius (ungeskaliert)
  std::vector<std::array<float, 6>> map_bounds;   // je scene.models: Bounding-Box (GL: min xyz, max xyz), ungeskaliert
  // Unbewegliche Kartenobjekte, denen Schiffe ausweichen (ODF avoidMe, Standard 1; Nebel/Hintergrund 0).
  struct MapObstacle {
    float pos[3];       // GL-System
    float radius;
    int inst;           // Szeneninstanz (wird sie zum Spielobjekt, entfaellt dieses Hindernis); -1 = Asteroidenfeld
  };
  std::vector<MapObstacle> map_obstacles;
  PathGrid path_grid;                  // gesperrte Felder fuer die Wegsuche (Originalsystem x, z = -z_GL)
  int scene_hidden_count = 0;
  std::map<std::string, SpawnModel> spawn_models;
  int selected = -1, spawn_team = 1;
  int sym_mode = 1;      // Symbolansicht fuer Schiffe/Stationen in der Ferne: 0 aus, 1 Fraktionslogo, 2 Drahtgitter
  float sym_px = 48.0f;  // unterhalb dieses Bildschirmdurchmessers (Pixel) wird das Symbol gezeigt
  MarkerRenderer sel_markers;
  MarkerRenderer grid_markers;  // Orientierungsgitter in der Ebene des ausgewaehlten Objekts
  std::vector<SpawnEntry> spawn_list;
  std::vector<std::string> spawn_races;
  bool spawn_built = false;
  int spawn_filter = 0;
  MenuKind menu = MenuKind::None;
  std::vector<MenuRow> menu_rows;
  int menu_scroll = 0;
  int menu_hot = -1;  // Zeile unter dem Finger
  float menu_drag = 0;

  void ClearMap() {
    spawned.clear();
    projectiles.clear();
    fx.clear();
    fireballs.clear();
    shockwaves.clear();
    spawn_models.clear();
    scene_hidden.clear();
    map_radius.clear();
    map_orig_radius.clear();
    map_light = ModelRenderer::SceneLighting();
    map_bounds.clear();
    map_obstacles.clear();
    path_grid = PathGrid();
    scene_hidden_count = 0;
    selected = -1;
    sel_markers.Clear();
    map_models.clear();
    map_sprites.clear();
    map_bg.reset();
    map_stars.reset();
    map_markers.Clear();
    map_loaded = false;
  }

  // Modell-Kugel/Bbox des Originals (0x5967f0): ALLE Mesh-Knoten (alle Detailstufen, Hilfs-/Schadens-/Borg-/oq-Netze), Eckpunkte mit
  // Knotenmatrix; Bbox = Min/Max, Kugel = Bbox-Mitte + groesster Abstand (0x5520f0). GL-Koordinaten. Sprite-Knoten (0x599000) nicht beruecksichtigt.
  struct OrigBounds {
    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0}, radius = 0;
  };
  static OrigBounds ComputeOrigBounds(const Sod& sd, const std::string& name) {
    Model all;
    BuildOptions o;
    o.hide_helper_nodes = false;
    BuildModel(sd, name, &all, o);
    OrigBounds r;
    for (int k = 0; k < 3; ++k) { r.mn[k] = all.bounds_min[k]; r.mx[k] = all.bounds_max[k]; }
    float c[3], m2 = 0;
    for (int k = 0; k < 3; ++k) c[k] = 0.5f * (r.mn[k] + r.mx[k]);
    for (const auto& bt : all.batches)
      for (size_t v = 0; v + 2 < bt.vertices.size(); v += 8) {
        float d2 = 0;
        for (int k = 0; k < 3; ++k) { const float d = bt.vertices[v + static_cast<size_t>(k)] - c[k]; d2 += d * d; }
        m2 = std::max(m2, d2);
      }
    r.radius = std::sqrt(m2);
    return r;
  }

  std::unique_ptr<ModelRenderer> BuildMapRenderer(const std::string& lname, Model* built, OrigBounds* ob = nullptr) {
    auto it = sod_lower.find(lname);
    if (it == sod_lower.end() || !source) return nullptr;
    std::vector<uint8_t> bytes;
    Sod sd;
    std::string err;
    if (!source->ReadFile("SOD/" + it->second, &bytes) || !ParseSod(bytes.data(), bytes.size(), &sd, &err)) return nullptr;
    BuildModel(sd, lname, built, BuildOptions());
    if (ob) *ob = ComputeOrigBounds(sd, lname);
    std::unique_ptr<ModelRenderer> r(new ModelRenderer());
    if (!r->Init()) return nullptr;
    r->SetAlphaMode(alpha_mode);
    r->SetModel(*built, *source);
    return r;
  }

  // Welches Modell ein Kartenobjekt zeigt. Das SOD steht in der ODF (baseName, sonst gleicher Name); Asteroidenfelder haben
  // keins (dont_load_SOD) und bestehen aus Einzelfelsen. Annahme: Felsen = Master1..8 (sscr_1.sod gibt es nicht).
  bool ResolveMapObject(const BznObject& o, MapObjectInfo* info) {
    const std::string l = ToLowerAscii(o.odf);
    if (l.empty()) return false;
    std::string sod = l, cls;
    const OdfObject* od = odf_lib ? odf_lib->Get(l) : nullptr;
    bool dont_load = false;
    if (od) {
      std::string b;
      if (od->GetString("baseName", &b) && !b.empty()) sod = ToLowerAscii(b);
      od->GetString("classLabel", &cls);
      od->GetBool("dont_load_SOD", &dont_load);
      float f = 0;
      if (od->GetFloat("scaleSOD", &f) && f > 0) info->scale = f;
      if (od->GetFloat("ScaleSize", &f) && f > 0) info->scale = f;
      if (od->GetFloat("distanceBelowGrid", &f)) info->below_grid = f;
      if (cls == "asteroidfield") {
        od->GetInt("minAsteroids", &info->field_min);
        od->GetInt("maxAsteroids", &info->field_max);
        if (od->GetFloat("fieldRadius", &f)) info->field_radius = f;
        if (sod_lower.count(sod)) info->field_models.push_back(sod);
        if (info->field_models.empty())
          for (int i = 1; i <= 8; ++i)
            if (sod_lower.count("master" + std::to_string(i))) info->field_models.push_back("master" + std::to_string(i));
        return info->field_radius > 0 && !info->field_models.empty();
      }
    }
    if (dont_load || !sod_lower.count(sod)) return false;
    info->sod = sod;
    return true;
  }

  // Hindernisse aus den Kartenobjekten ohne Team. Original: GameObjectClass avoidMe (+0x1a5, Standard 1; Nebel- und
  // Hintergrundklasse 0) wird zu Objekt +0xd8, dem Filter der Ausweichroutine 0x42ef70. Radius = Modellradius * Skalierung,
  // Asteroidenfeld = fieldRadius (ANNAHME, im Original nicht geprueft).
  void BuildMapObstacles() {
    map_obstacles.clear();
    if (!odf_lib) return;
    std::set<int> done;
    for (size_t ii = 0; ii < scene.instances.size(); ++ii) {
      const MapInstance& in = scene.instances[ii];
      if (in.team > 0 || in.object < 0 || in.object >= static_cast<int>(map.objects.size()) || done.count(in.object)) continue;
      if (ii < scene_hidden.size() && scene_hidden[ii]) continue;
      const BznObject& o = map.objects[static_cast<size_t>(in.object)];
      const OdfObject* od = odf_lib->Get(ToLowerAscii(o.odf));
      std::string cls;
      if (od) od->GetString("classLabel", &cls);
      int avoid = (cls == "nebula_obj" || cls == "background_obj") ? 0 : 1;
      if (od) od->GetInt("avoidMe", &avoid);
      if (!avoid) continue;
      MapObjectInfo info;
      if (!ResolveMapObject(o, &info)) continue;
      done.insert(in.object);
      MapObstacle ob;
      if (info.field_radius > 0) {
        Mat4 base = BznObjectMatrix(o);
        ob.pos[0] = base.m[12]; ob.pos[1] = base.m[13] - info.below_grid; ob.pos[2] = base.m[14];
        // Felsen liegen in der Kugel um den Mittelpunkt; ihr eigener Radius kommt dazu, das Feld ist als Ganzes undurchdringlich
        float rock = 0;
        for (const MapInstance& r : scene.instances)
          if (r.object == in.object) {
            const float* rm = r.transform.m;
            const float rs = std::sqrt(rm[0] * rm[0] + rm[1] * rm[1] + rm[2] * rm[2]);
            rock = std::max(rock, map_radius[static_cast<size_t>(r.model)] * rs);
          }
        ob.radius = info.field_radius + rock;
        ob.inst = -1;
      } else {
        const float* m = in.transform.m;
        for (int k = 0; k < 3; ++k) ob.pos[k] = m[12 + k];
        const float sc = std::max(1e-3f, std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
        ob.radius = map_radius[static_cast<size_t>(in.model)] * sc;
        ob.inst = static_cast<int>(ii);
      }
      map_obstacles.push_back(ob);
    }
    // Pfadraster: Zellgroesse HIGHEST_QUADTREE_GRID_RESOLUTION (50). Asteroidenfeld: Quadrat mit Halbkante ASTEROID_BELT_PATHING_DISTANCE/2 (150/2).
    float lo[2] = {-1500, -1500}, hi[2] = {1500, 1500};
    for (const BznObject& o : map.objects) {
      lo[0] = std::min(lo[0], o.pos[0] - 1000); hi[0] = std::max(hi[0], o.pos[0] + 1000);
      lo[1] = std::min(lo[1], -o.pos[2] - 1000); hi[1] = std::max(hi[1], -o.pos[2] + 1000);   // Originalsystem: z = -z_GL
    }
    path_grid.Init(lo[0], lo[1], hi[0], hi[1], 50.0f);
    for (const MapObstacle& mo : map_obstacles)
      if (mo.inst < 0) path_grid.StampField(mo.pos[0], -mo.pos[2]);
    // Fussabdruck von Stationen (0x46d5c0: CraftClass mit is_starbase): Bounding-Box des Modells (ungedreht, skaliert) + Position, Kosten 198, Radius 0
    for (const MapInstance& in : scene.instances) {
      if (in.object < 0 || in.object >= static_cast<int>(map.objects.size()) || in.model < 0 || in.model >= static_cast<int>(map_bounds.size())) continue;
      const OdfObject* od = odf_lib ? odf_lib->Get(ToLowerAscii(map.objects[static_cast<size_t>(in.object)].odf)) : nullptr;
      int sb = 0;
      if (!od || !od->GetInt("is_starbase", &sb) || !sb) continue;
      const float* m = in.transform.m;
      const float sc = std::max(1e-3f, std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
      const std::array<float, 6>& b = map_bounds[static_cast<size_t>(in.model)];
      const float x = m[12], z = -m[14];
      path_grid.StampBox(x + b[0] * sc, z - b[5] * sc, x + b[3] * sc, z - b[2] * sc, 0.0f, 198);
    }
    // Stempel der Instanz-Initialisierung im Original (Armada.exe 0x450810 Schwarzes Loch, 0x451e00 Nebel, 0x4531a0 Planet, 0x453f00 Wurmloch)
    neb_lights.clear();
    for (size_t oi = 0; oi < map.objects.size(); ++oi) {
      const BznObject& o = map.objects[oi];
      const OdfObject* od = odf_lib ? odf_lib->Get(ToLowerAscii(o.odf)) : nullptr;
      std::string cls;
      if (!od || !od->GetString("classLabel", &cls)) continue;
      const Mat4 base = BznObjectMatrix(o);
      {  // Glow-Licht: Zufall 1 zu one_in_n_chance_of_light_source (0x451e00), Farbe/Abfall aus der ODF (Konstruktor 0x44bfa0)
        float n = 0;
        if (od->GetFloat("one_in_n_chance_of_light_source", &n) && n >= 1.0f && static_cast<int>(rng.F01() * n) == 0) {
          NebLight nl;
          for (int k = 0; k < 3; ++k) nl.pos[k] = base.m[12 + k];
          static const char* const kBase[3] = {"red_glow", "green_glow", "blue_glow"};
          static const char* const kSwing[3] = {"red_glow_range", "green_glow_range", "blue_glow_range"};
          for (int k = 0; k < 3; ++k) {
            nl.base[k] = 0.5f;
            od->GetFloat(kBase[k], &nl.base[k]);
            od->GetFloat(kSwing[k], &nl.swing[k]);
          }
          od->GetFloat("glow_falloff_start", &nl.start);
          od->GetFloat("glow_falloff_range", &nl.range);
          od->GetFloat("speed_change_period_divisor", &nl.freq);
          od->GetFloat("min_speed_change", &nl.min_step);
          od->GetFloat("speed_change_range", &nl.step_range);
          nl.rate = rng.F01() * 0.015f + 0.0005f;
          neb_lights.push_back(nl);
        }
      }
      const float x = base.m[12], z = -base.m[14];
      if (cls == "blackhole_obj") {
        float er = 10.0f; od->GetFloat("EventRadius", &er);
        path_grid.Stamp(x, z, blackhole_coef * er, PathGrid::kImpassable);
      } else if (cls == "nebula_obj") {
        float er = 100.0f, pc = 200.0f; od->GetFloat("effectRadius", &er); od->GetFloat("pathing_cost", &pc);
        path_grid.Stamp(x, z, nebulae_coef * er, static_cast<int32_t>(pc));
      } else if (cls == "wormhole_obj") {
        path_grid.Stamp(x, z, 0.0f, 500);
      } else if (cls == "planet_obj") {
        for (const MapInstance& in : scene.instances)
          if (in.object == static_cast<int>(oi) && in.team <= 0) {
            const float* m = in.transform.m;
            const float sc = std::max(1e-3f, std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
            path_grid.Stamp(x, z, map_orig_radius[static_cast<size_t>(in.model)] * sc, 198);
            break;
          }
      }
    }
  }

  // RTS_CFG.h des Originals: Werte, die die Flugsteuerung betreffen (einfache Zeilen "float NAME = Zahl;").
  void LoadWorldCfg() {
    std::vector<uint8_t> b;
    if (!source || !source->ReadFile("RTS_CFG.h", &b)) return;
    std::string txt(b.begin(), b.end());
    auto val = [&](const char* key, float* out) {
      size_t p = 0;
      const std::string k = key;
      while ((p = txt.find(k, p)) != std::string::npos) {
        const bool lead = p == 0 || txt[p - 1] == ' ' || txt[p - 1] == '\t' || txt[p - 1] == '\n';
        size_t q = p + k.size();
        while (q < txt.size() && (txt[q] == ' ' || txt[q] == '\t')) ++q;
        if (lead && q < txt.size() && txt[q] == '=') {
          ++q;
          char* end = nullptr;
          const float v = std::strtof(txt.c_str() + q, &end);
          if (end != txt.c_str() + q) { *out = v; return true; }
        }
        p += k.size();
      }
      return false;
    };
    float f = 0;
    HoverWorldCfg& w = HoverWorld();
    if (val("OBSTACLE_AVOIDANCE", &f)) w.obstacle_avoidance = f != 0;
    if (avoid_user >= 0) w.obstacle_avoidance = avoid_user != 0;
    if (val("NEBULAE_PATHING_SAFETY_COEFFICIENT", &f)) nebulae_coef = f;
    if (val("BLACKHOLE_PATHING_SAFETY_COEFFICIENT", &f)) blackhole_coef = f;
    if (val("SHIP_COMBAT_FLOOR", &f)) w.floor = f;
    if (val("SHIP_COMBAT_CEILING", &f)) w.ceiling = f;
    CombatConfig& cc = CombatCfg();  // Kampfwerte (Exe-Vorgaben gelten, wenn ein Schluessel fehlt)
    if (val("USE_SYSTEM_HITPOINTS", &f)) cc.use_system_hitpoints = f != 0;
    if (val("SYSTEM_HITPOINTS_BONUS", &f)) cc.system_hitpoints_bonus = static_cast<int>(f);
    if (val("SHIELD_DAMAGE_PROTECTION", &f)) cc.shield_damage_protection = f;
  }

  void BuildStars() {
    Model m;
    map_stars = BuildMapRenderer("mbgstars", &m);
    if (map_stars) map_stars->SetLit(false);
    if (!map_stars) return;
    float r2 = 0;
    for (int k = 0; k < 3; ++k) {
      map_stars_center[k] = 0.5f * (m.bounds_min[k] + m.bounds_max[k]);
      const float d = m.bounds_max[k] - m.bounds_min[k];
      r2 += d * d;
    }
    map_stars_radius = std::max(0.001f, 0.5f * std::sqrt(r2));
  }

  void LoadMap(int index, const SaveGame* sg = nullptr) {
    const int n = static_cast<int>(map_files.size());
    if (n == 0 || !source) return;
    index = ((index % n) + n) % n;
    ClearMap();
    LoadWorldCfg();
    std::vector<uint8_t> bytes;
    std::string err;
    BznMap parsed;
    if (!source->ReadFile("bzn/" + map_files[static_cast<size_t>(index)], &bytes) || !ParseBzn(bytes.data(), bytes.size(), &parsed, &err)) {
      Message(map_files[static_cast<size_t>(index)] + ": nicht lesbar (" + err + ")");
      return;
    }
    map = std::move(parsed);
    // Szenenlicht: Karten-Ambient plus die dlight-Objekte (Richtung = Vorwaertsachse des Objekts; Richtung ZUM Licht = -vorne, GL: z gespiegelt)
    map_light = ModelRenderer::SceneLighting();
    map_light.enabled = true;
    for (int k = 0; k < 3; ++k) map_light.ambient[k] = map.ambient[k];
    for (const BznObject& o : map.objects) {
      if (map_light.count >= 4 || ToLowerAscii(o.odf) != "dlight") continue;
      float* d = map_light.dir[map_light.count];
      d[0] = -o.rot[6]; d[1] = -o.rot[7]; d[2] = o.rot[8];
      for (int k = 0; k < 3; ++k) map_light.color[map_light.count][k] = o.light_color[k];
      ++map_light.count;
    }
    map_current = index;
    std::map<unsigned, const SaveObject*> sg_state;
    if (sg) {
      // Spielstand: Objektliste der Karte durch die gespeicherten Objekte ersetzen (Rest der Karte bleibt)
      map.objects.clear();
      for (const SaveObject& so : sg->objects) {
        BznObject o;
        o.odf = so.odf;
        o.label = so.label;
        o.handle = so.id | 0x80000000u;
        for (int k = 0; k < 3; ++k) o.pos[k] = so.pos[k];
        o.team = so.team;
        for (int k = 0; k < 9; ++k) o.rot[k] = so.mat[k];
        map.objects.push_back(o);
        if (so.has_state) sg_state[so.id] = &so;
      }
    }
    for (const BznObject& o : map.objects)
      if ((o.handle & 0x7fffffffu) >= next_id) next_id = (o.handle & 0x7fffffffu) + 1;
    scene = BuildMapScene(map, [this](const BznObject& o, MapObjectInfo* info) { return ResolveMapObject(o, info); });
    map_models.resize(scene.models.size());
    map_sprites.resize(scene.models.size());
    map_radius.assign(scene.models.size(), 50.0f);
    map_orig_radius.assign(scene.models.size(), 50.0f);
    map_bounds.assign(scene.models.size(), std::array<float, 6>{{0, 0, 0, 0, 0, 0}});
    scene_hidden.assign(scene.instances.size(), 0);
    int loaded = 0;
    for (size_t i = 0; i < scene.models.size(); ++i) {
      Model built;
      OrigBounds ob;
      map_models[i] = BuildMapRenderer(scene.models[i], &built, &ob);
      if (!map_models[i]) continue;
      ++loaded;
      {
        float r2 = 0;
        for (int k = 0; k < 3; ++k) {
          const float d = built.bounds_max[k] - built.bounds_min[k];
          r2 += d * d;
        }
        map_radius[i] = std::max(1.0f, 0.5f * std::sqrt(r2));
        map_orig_radius[i] = ob.radius > 0 ? ob.radius : map_radius[i];
        for (int k = 0; k < 3; ++k) { map_bounds[i][static_cast<size_t>(k)] = ob.mn[k]; map_bounds[i][static_cast<size_t>(k) + 3] = ob.mx[k]; }
      }
      std::unique_ptr<SpriteRenderer> sp(new SpriteRenderer());
      if (sp->Init() && sp->SetNodes(built, spr, *source) > 0) map_sprites[i] = std::move(sp);
    }
    // Kartenobjekte mit Team (Schiffe, Stationen) sind sofort Spielobjekte.
    for (size_t ii = 0; ii < scene.instances.size(); ++ii)
      if (map_models[static_cast<size_t>(scene.instances[ii].model)] &&
          (scene.instances[ii].team > 0 ||
           (sg && scene.instances[ii].object >= 0 && sg_state.count(map.objects[static_cast<size_t>(scene.instances[ii].object)].handle & 0x7fffffffu))))
        AdoptInstance(ii);
    BuildMapObstacles();
    if (sg) {
      for (Spawned& sp : spawned) {
        auto it = sg_state.find(sp.id);
        if (it == sg_state.end()) continue;
        RestoreState(&sp, *it->second);
      }
    }
    BuildStars();
    std::string bg = ToLowerAscii(map.background);
    if (bg.size() > 4 && bg.compare(bg.size() - 4, 4, ".sod") == 0) bg.resize(bg.size() - 4);
    Model bgm;
    map_bg = BuildMapRenderer(bg, &bgm);
    if (map_bg) map_bg->SetLit(false);
    if (map_bg && map_stars) map_bg->SetForceAdditive(true);  // ergaenzt den Sternenhimmel dahinter
    if (map_bg) {
      float r2 = 0;
      for (int k = 0; k < 3; ++k) {
        map_bg_center[k] = 0.5f * (bgm.bounds_min[k] + bgm.bounds_max[k]);
        const float d = bgm.bounds_max[k] - bgm.bounds_min[k];
        r2 += d * d;
      }
      map_bg_radius = std::max(0.001f, 0.5f * std::sqrt(r2));
    }
    // Punkte fuer alles, was kein Modell hat (Asteroidenfelder, Lichter, nicht ladbare Modelle)
    std::vector<MarkerRenderer::Marker> pts;
    auto add_point = [&](const BznObject& o) {
      MarkerRenderer::Marker m;
      m.pos[0] = o.pos[0];
      m.pos[1] = o.pos[1];
      m.pos[2] = -o.pos[2];
      m.dir[0] = m.dir[1] = m.dir[2] = 0;
      const std::string l = ToLowerAscii(o.odf);
      const float* c = kMapTeamColors[o.team >= 0 && o.team < 9 ? o.team : 0];
      static const float kField[3] = {0.6f, 0.45f, 0.25f}, kLight[3] = {1.0f, 1.0f, 0.5f};
      if (l == "mafield") c = kField;
      else if (l == "dlight") c = kLight;
      for (int k = 0; k < 3; ++k) m.color[k] = c[k];
      pts.push_back(m);
    };
    for (int oi : scene.unmodeled) add_point(map.objects[static_cast<size_t>(oi)]);
    int last_failed = -1;
    for (const MapInstance& inst : scene.instances) {
      if (map_models[static_cast<size_t>(inst.model)] || inst.object == last_failed) continue;
      last_failed = inst.object;  // nicht ladbares Modell: Punkt (bei Feldern nur einer je Feld)
      add_point(map.objects[static_cast<size_t>(inst.object)]);
    }
    map_markers.Set(pts, 0.0f);
    for (int k = 0; k < 3; ++k) map_target[k] = scene.center[k];
    map_dist = std::max(1200.0f, scene.extent * 2.4f);
    zoom = 1.0f;
    map_loaded = true;
    StartMapMusic();
    StartMapAmbients();
    Message("Karte " + std::to_string(index + 1) + "/" + std::to_string(n) + ": " + map.title + " (" + map_files[static_cast<size_t>(index)] + ")  " +
            std::to_string(map.objects.size()) + " Objekte, " + std::to_string(scene.models.size()) + " Modelle (" + std::to_string(loaded) +
            " geladen), " + std::to_string(pts.size()) + " Punkte, Hintergrund " + map.background + (map_bg ? "" : " (fehlt)") +
            "\nZiehen: drehen, 2 Finger: zoomen/verschieben. Tasten unten: Karte wechseln, Info, Ton, zurueck zu den Modellen (Taste M).");
  }


  // ---- Spielstand (Originalformat, formats/SaveGame.h; Beschreibung in docs/formats/savegame.md) ----
  static int SysOfFile(int i) {
    static const int k[5] = {kShield, kEngines, kWeapons, kLife, kSensors};  // Reihenfolge der Datei: Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren
    return k[i];
  }

  void RestoreState(Spawned* sp, const SaveObject& so) {
    Health& h = sp->hp;
    h.shield = std::max(0.0f, std::min(so.shield, h.shield_max));
    h.delay_left = so.shield_delay_left;
    h.crew = std::max(0.0f, std::min(so.crew, h.crew_max));
    for (int i = 0; i < 5; ++i) {
      SystemBlock& b = h.sys[SysOfFile(i)];
      if (so.has_sysx) {  // vollstaendiger Zustand wie im Original (CraftSystem)
        const SaveObject::SysState& s = so.sysx[i];
        b.active = s.active;
        b.points = s.points;
        b.hit_pct = s.hit_pct;
        b.per_point = s.per_point;
        b.countdown = s.countdown;
        b.disable_timer = s.disable_timer;
        b.script_disabled = s.permanent;
      } else {  // aeltere Staende dieses Clients: nur Punkte; eingeschaltet, solange Punkte uebrig sind
        b.points = std::max(0, so.sys[i]);
        b.active = b.points > 0;
      }
    }
    h.derelict = h.crew_max > 0 && h.crew < 1.0f;
    if (h.derelict) {
      sp->derelict = true;
      sp->team = 0;
    }
  }

  bool CanSave() const { return in_game && map_loaded && map_current >= 0 && source && !map_files.empty(); }

  // Alles ausser Mehrspieler (es gibt nur Spiele auf einer Karte).
  bool SaveToBytes(std::vector<uint8_t>* out) {
    if (has_pending_state) {  // geladener Stand wartet noch auf die Spieldaten: unveraendert weiterreichen
      *out = pending_state;
      return true;
    }
    if (!CanSave()) return false;
    std::vector<uint8_t> bytes;
    SgFile base;
    std::string err;
    if (!source->ReadFile("bzn/" + map_files[static_cast<size_t>(map_current)], &bytes) || !ReadSgFile(bytes.data(), bytes.size(), &base, &err)) return false;
    std::map<unsigned, int> by_handle;
    for (size_t k = 0; k < base.objects.size(); ++k) by_handle[static_cast<unsigned>(SgGetInt(base.objects[k].e[1])) & 0x7fffffffu] = static_cast<int>(k);
    SaveGame g;
    {
      char buf[64] = "";
      const time_t t = time(nullptr);
      if (const tm* lt = localtime(&t)) strftime(buf, sizeof buf, "%b %d %I:%M:%S %p", lt);
      g.desc = (map.title.empty() ? map_files[static_cast<size_t>(map_current)] : map.title) + " " + buf;
    }
    static const char* const kRaceName[4] = {"federation", "klingon", "romulan", "borg"};
    g.race = kRaceName[hud_race & 3];
    g.single_player = true;
    for (int t = 0; t < kSaveTeams; ++t) {  // Anfangswerte der Karte (Dilithium/Besatzung/Offiziere), es gibt noch keine Wirtschaft
      SaveTeam& st = g.teams[t];
      const size_t b = static_cast<size_t>(t) * 7;
      if (base.tail.size() >= b + 7) {
        st.scrap = SgGetFloat(base.tail[b]);
        st.max_scrap = SgGetFloat(base.tail[b + 1]);
        st.max_pilot = SgGetInt(base.tail[b + 2]);
        st.crew = SgGetFloat(base.tail[b + 3]);
        st.max_crew = SgGetFloat(base.tail[b + 4]);
        st.color = SgGetInt(base.tail[b + 5]);
        st.map_color = SgGetInt(base.tail[b + 6]);
      }
    }
    std::set<int> adopted;
    for (size_t ii = 0; ii < scene.instances.size(); ++ii)
      if (ii < scene_hidden.size() && scene_hidden[ii] && scene.instances[ii].object >= 0) adopted.insert(scene.instances[ii].object);
    auto base_of = [&](unsigned id) {
      auto it = by_handle.find(id);
      return it == by_handle.end() ? -1 : it->second;
    };
    for (size_t i = 0; i < map.objects.size(); ++i) {
      if (adopted.count(static_cast<int>(i))) continue;
      const BznObject& o = map.objects[i];
      SaveObject so;
      so.odf = o.odf;
      so.id = o.handle & 0x7fffffffu;
      so.team = o.team;
      so.label = o.label;
      for (int k = 0; k < 3; ++k) so.pos[k] = o.pos[k];
      for (int k = 0; k < 9; ++k) so.mat[k] = o.rot[k];
      for (int k = 0; k < 3; ++k) so.mat[9 + k] = o.pos[k];
      so.base_index = base_of(so.id);
      g.teams[o.team >= 0 && o.team < kSaveTeams ? o.team : 0].exists = g.teams[o.team >= 0 && o.team < kSaveTeams ? o.team : 0].exists || o.team > 0;
      g.objects.push_back(so);
    }
    for (const Spawned& sp : spawned) {
      if (sp.odf.empty()) continue;
      SaveObject so;
      so.odf = sp.odf;
      so.cls = ClassLabelOf(sp.odf);  // nicht sp.cls: das ist der Anzeigetext (Schiff/Station)
      so.id = sp.id & 0x7fffffffu;
      so.team = sp.team;
      std::string lab = sp.odf + std::to_string(sp.id);
      so.label = lab.substr(0, 39);
      MapObjectInfo info;
      BznObject probe;
      probe.odf = sp.odf;
      ResolveMapObject(probe, &info);
      Mat4 model, rot;
      ShipMatrices(sp.st, 1.0f, &model, &rot);
      const float* m = rot.m;
      const float r[9] = {m[0], m[1], -m[2], m[4], m[5], -m[6], -m[8], -m[9], m[10]};
      for (int k = 0; k < 9; ++k) so.mat[k] = r[k];
      so.pos[0] = sp.st.pos[0];
      so.pos[1] = sp.st.pos[1] + info.below_grid;
      so.pos[2] = -sp.st.pos[2];
      for (int k = 0; k < 3; ++k) so.mat[9 + k] = so.pos[k];
      so.base_index = base_of(so.id);
      if (so.base_index >= 0) so.label = SgGetStr(base.objects[static_cast<size_t>(so.base_index)].e[4]);
      so.has_state = true;
      so.shield = sp.hp.shield;
      so.shield_max = sp.hp.shield_max;
      so.crew = sp.hp.crew;
      so.shield_delay_left = sp.hp.delay_left;
      so.has_sysx = true;
      for (int i = 0; i < 5; ++i) {
        const SystemBlock& b = sp.hp.sys[SysOfFile(i)];
        so.sys[i] = b.points;
        SaveObject::SysState& s = so.sysx[i];
        s.active = b.active;
        s.points = b.points;
        s.hit_pct = b.hit_pct;
        s.per_point = b.per_point;
        s.countdown = b.countdown;
        s.disable_timer = b.disable_timer;
        s.permanent = b.script_disabled;
      }
      if (sp.team > 0 && sp.team < kSaveTeams) g.teams[sp.team].exists = true;
      g.objects.push_back(so);
    }
    SgFile f;
    if (!BuildSave(base, g, &f, &err)) {
      FLEET_LOGW("Spielstand: %s", err.c_str());
      return false;
    }
    *out = WriteSgFile(f);
    return true;
  }

  std::string ClassLabelOf(const std::string& odf) const {
    UnitDef u;
    if (odf_lib && LoadUnitDef(*odf_lib, odf, &u)) return u.class_label;
    return std::string();
  }

  bool LoadFromBytes(const uint8_t* data, size_t size) {
    SgFile f;
    SaveGame g;
    std::string err;
    if (!ReadSgFile(data, size, &f, &err) || !ParseSave(f, &g, &err, [this](const std::string& o) { return ClassLabelOf(o); })) {
      Message("Spielstand nicht lesbar: " + err);
      return false;
    }
    EnsureMapFiles();
    int idx = -1;
    for (size_t i = 0; i < map_files.size(); ++i)
      if (ToLowerAscii(map_files[i]) == ToLowerAscii(g.map_file)) idx = static_cast<int>(i);
    if (idx < 0) {
      Message("Spielstand: Karte " + g.map_file + " fehlt.");
      return false;
    }
    map_mode = true;
    LoadMap(idx, &g);
    if (!map_loaded) return false;
    const std::string r = ToLowerAscii(g.race);
    hud_race = r == "klingon" ? 1 : r == "romulan" ? 2 : r == "borg" ? 3 : 0;
    LoadHud();
    in_game = true;
    selected = -1;
    // Blick auf die erste Basis/das erste Schiff von Team 1
    for (const Spawned& sp : spawned)
      if (sp.team == 1) {
        for (int k = 0; k < 3; ++k) map_target[k] = sp.st.pos[k];
        break;
      }
    Message("Spielstand geladen: " + g.desc);
    return true;
  }

  std::vector<uint8_t> pending_state;
  bool has_pending_state = false;

  void ToggleMap() {
    if (!gl_ready || !source) return;
    if (map_mode) {
      OpenMenu(MenuKind::Maps);
      return;
    }
    if (map_files.empty()) {
      for (const std::string& nm : source->ListDir("bzn")) {
        const std::string l = ToLowerAscii(nm);
        if (l.size() > 4 && l.compare(l.size() - 4, 4, ".bzn") == 0) map_files.push_back(nm);
      }
      std::sort(map_files.begin(), map_files.end(), [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
    }
    if (map_files.empty()) {
      Message("Keine Karten: im Spielordner fehlt bzn/*.bzn.");
      return;
    }
    map_mode = true;
    zoom = 1.0f;
    if (!map_loaded) LoadMap(map_current < 0 ? 0 : map_current);
    else {
      StartMapMusic();
      StartMapAmbients();
      Message("Kartenansicht: " + map.title);
    }
  }


  // ---------------------------------------------------------------------------------------------------------------------------
  void EnsureMapFiles() {
    if (!map_files.empty() || !source) return;
    for (const std::string& nm : source->ListDir("bzn")) {
      const std::string l = ToLowerAscii(nm);
      if (l.size() > 4 && l.compare(l.size() - 4, 4, ".bzn") == 0) map_files.push_back(nm);
    }
    std::sort(map_files.begin(), map_files.end(), [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
  }

  // Hintergrund fuer das Hauptmenue (Sterne, keine Karte). Eine leere Spielwiese gibt es im Original nicht.
  void ShowBackdrop() {
    ClearMap();
    StopMapAudio();
    map = BznMap();
    scene = MapScene();
    map_current = -1;
    BuildStars();
    std::vector<MarkerRenderer::Marker> pts;  // leer: Orientierung gibt das Gitter
    map_markers.Set(pts, 0.0f);
    for (int k = 0; k < 3; ++k) map_target[k] = 0;
    map_dist = 2500;
    zoom = 1.0f;
    map_loaded = true;
  }

  void BuildSpawnList() {
    if (spawn_built || !odf_zfs || !odf_lib) return;
    spawn_built = true;
    std::set<std::string> races;
    for (const ZfsEntry& e : odf_zfs->entries()) {
      std::string n = ToLowerAscii(e.name);
      if (n.size() < 5 || n.compare(n.size() - 4, 4, ".odf") != 0) continue;
      n.resize(n.size() - 4);
      UnitDef u;
      if (!LoadUnitDef(*odf_lib, n, &u) || u.unit_name.empty()) continue;
      BznObject o;
      o.odf = n;
      MapObjectInfo info;
      if (!ResolveMapObject(o, &info) || info.sod.empty()) continue;
      SpawnEntry se;
      se.odf = n;
      se.race = ToLowerAscii(u.race);
      se.label = u.unit_name + " (" + n + ")";
      if (se.label.size() > 40) se.label.resize(40);
      if (!se.race.empty()) races.insert(se.race);
      spawn_list.push_back(se);
    }
    std::sort(spawn_list.begin(), spawn_list.end(), [](const SpawnEntry& a, const SpawnEntry& b) { return a.race != b.race ? a.race < b.race : a.odf < b.odf; });
    spawn_races.assign(races.begin(), races.end());
  }

  void RebuildMenu() {
    menu_rows.clear();
    if (menu == MenuKind::Spawn) {
      const std::string f = spawn_filter == 0 ? std::string("Alle") : spawn_races[static_cast<size_t>(spawn_filter - 1)];
      menu_rows.push_back({"Rasse: " + f + "  (antippen = wechseln)", "#race"});
      for (const SpawnEntry& e : spawn_list)
        if (spawn_filter == 0 || e.race == spawn_races[static_cast<size_t>(spawn_filter - 1)]) menu_rows.push_back({e.label, e.odf});
    } else if (menu == MenuKind::Options) {
      menu_rows = {{"Ziele wiederholen", ""},
                   {"Spiel speichern", ""},
                   {"Spiel laden", ""},
                   {"Grafik-Einstellungen", ""},
                   {muted ? "Ton: aus" : "Ton: an", "@mute"},
                   {"Spiel-Einstellungen", ""},
                   {"Erweiterte Einstellungen", "@advanced"},
                   {"DEBUG", "@debug"},
                   {"Zum Hauptmen\xC3\xBC", "@abort"},
                   {"Zur\xC3\xBC" "ck zum Spiel", "@close"}};
    } else if (menu == MenuKind::Advanced) {
      // Optionen, die es im Original nicht gab. Zeilen ohne Aktion sind noch nicht umgesetzt (ausgegraut).
      menu_rows = {{"Bessere Shader (folgt)", ""},
                   {"Lagesensor steuert Ansicht (folgt)", ""},
                   {"Alternative GUI-Anordnung (folgt)", ""},
                   {tap_moves ? "Bewegen per Antippen: an" : "Bewegen per Antippen: aus", "@adv_move"},
                   {std::string("Symbolansicht: ") + (sym_mode == 0 ? "aus" : sym_mode == 1 ? "Fraktionslogo" : "Drahtgitter"), "@sym"},
                   {std::string("Symbol-Schwelle: ") + std::to_string(static_cast<int>(sym_px)) + " px", "@sympx"},
                   {HoverWorld().obstacle_avoidance ? "Hindernisvermeidung: an" : "Hindernisvermeidung: aus (Original)", "@adv_avoid"},
                   {"<  Zur\xC3\xBC" "ck", "@options"}};
    } else if (menu == MenuKind::Debug) {
      menu_rows = {{"Objekt setzen ...", "@spawn"},
                   {"Karte waehlen ...", "@maps"},
                   {"Info zur Auswahl / Karte", "@info"},
                   {std::string("Teamfarbe wechseln (jetzt ") + std::to_string(spawn_team) + ")", "@team"},
                   {muted ? "Ton einschalten" : "Ton ausschalten", "@mute"},
                   {"Kamera naeher", "@zin"},
                   {"Kamera weiter weg", "@zout"},
                   {"Ansicht zuruecksetzen", "@reset"},
                   {std::string("HUD-Rasse wechseln (") + HudRaceName(static_cast<HudRace>(hud_race)) + ")", "@hudrace"},
                   {"Datenquellen: Ordner / ZIP-Dateien ...", "@folder"},
                   {"Auswahl loeschen", "@delete"},
                   {"<  Zurueck", "@options"}};
    } else if (menu == MenuKind::Maps) {
      for (size_t i = 0; i < map_files.size(); ++i) menu_rows.push_back({map_files[i], "#map" + std::to_string(i)});
    }
  }

  void OpenMenu(MenuKind k) {
    if (!gl_ready || !source || !map_mode) return;
    if (k == MenuKind::Options || k == MenuKind::Debug || k == MenuKind::Advanced) {
      // feste Listen
    } else if (k == MenuKind::Spawn) {
      BuildSpawnList();
      if (spawn_list.empty()) {
        Message("Keine Objekte gefunden (assets.zfs oder SOD fehlt).");
        return;
      }
    } else {
      EnsureMapFiles();
    }
    menu = k;
    menu_scroll = 0;
    menu_drag = 0;
    RebuildMenu();
  }

  // Menue im Stil des Original-Optionsmenues: Titelleiste oben, mittig ein Rahmen mit grossen Tasten (gut per Finger zu treffen).
  struct MenuGeo {
    float tx = 0, ty = 0, tw = 0, th = 0;    // Titelleiste
    float px = 0, py = 0, pw = 0, ph = 0;    // Rahmen
    float x0 = 0, y0 = 0, bw = 0, btn_h = 1, gap = 0, row_h = 1, pad = 0, h_px = 1;  // Tasten
    int vis = 1;
  };
  MenuGeo MenuGeometry() const {
    MenuGeo g;
    const float W = static_cast<float>(width), H = static_cast<float>(height), mn = std::min(W, H);
    g.btn_h = std::max(56.0f, std::min(170.0f, 0.13f * mn));
    g.gap = 0.22f * g.btn_h;
    g.row_h = g.btn_h + g.gap;
    g.pad = 0.3f * g.btn_h;
    g.h_px = 0.42f * g.btn_h;
    const float m = 0.03f * mn;
    g.tx = m; g.ty = m; g.tw = W - 2 * m; g.th = 0.5f * g.btn_h;
    const float top = g.ty + g.th + 0.6f * m, avail = H - m - top;
    const int vis_max = std::max(1, static_cast<int>((avail - 2 * g.pad + g.gap) / g.row_h));
    g.vis = std::max(1, std::min(vis_max, static_cast<int>(menu_rows.size())));
    g.pw = std::min(W - 2 * m, 1.05f * mn);
    g.ph = 2 * g.pad + static_cast<float>(g.vis) * g.row_h - g.gap;
    g.px = 0.5f * (W - g.pw);
    g.py = top + 0.5f * (avail - g.ph);
    g.x0 = g.px + g.pad;
    g.bw = g.pw - 2 * g.pad;
    g.y0 = g.py + g.pad;
    return g;
  }

  const char* MenuTitle() const {
    switch (menu) {
      case MenuKind::Options: return "OPTIONEN";
      case MenuKind::Advanced: return "ERWEITERTE EINSTELLUNGEN";
      case MenuKind::Debug: return "DEBUG";
      case MenuKind::Spawn: return "OBJEKT SETZEN";
      case MenuKind::Maps: return "KARTE W\xC3\x84HLEN";
      default: return "";
    }
  }

  // Zeile unter dem Finger (nur auf einer Taste, nicht in der Luecke), sonst -1.
  int MenuRowAt(const MenuGeo& g, float x, float y) const {
    if (x < g.x0 || x > g.x0 + g.bw || y < g.y0) return -1;
    const float rel = y - g.y0;
    const int i = static_cast<int>(rel / g.row_h);
    if (i >= g.vis || rel - static_cast<float>(i) * g.row_h > g.btn_h) return -1;
    const size_t idx = static_cast<size_t>(menu_scroll + i);
    return idx < menu_rows.size() ? static_cast<int>(idx) : -1;
  }

  void MenuDrag(float dy) {
    const MenuGeo g = MenuGeometry();
    menu_drag += dy;
    const int rows = static_cast<int>(menu_drag / g.row_h);
    menu_drag -= static_cast<float>(rows) * g.row_h;
    const int maxs = std::max(0, static_cast<int>(menu_rows.size()) - g.vis);
    menu_scroll = std::max(0, std::min(maxs, menu_scroll - rows));
  }

  void MenuAction(const std::string& key) {
    if (key == "@debug") { menu = MenuKind::Debug; menu_scroll = 0; RebuildMenu(); return; }
    if (key == "@advanced") { menu = MenuKind::Advanced; menu_scroll = 0; RebuildMenu(); return; }
    if (key == "@adv_move") {
      tap_moves = !tap_moves;
      RebuildMenu();
      return;
    }
    if (key == "@adv_avoid") {
      avoid_user = HoverWorld().obstacle_avoidance ? 0 : 1;
      HoverWorld().obstacle_avoidance = avoid_user != 0;
      RebuildMenu();
      return;
    }
    if (key == "@options") { menu = MenuKind::Options; menu_scroll = 0; RebuildMenu(); return; }
    menu = MenuKind::None;
    if (key == "@close") return;
    if (key == "@abort") {
      in_game = false;
      shell_screen = ShellScreen::Main;
      shell_pressed.clear();
    }
    else if (key == "@spawn") OpenMenu(MenuKind::Spawn);
    else if (key == "@maps") OpenMenu(MenuKind::Maps);
    else if (key == "@info") ShowSelectedInfo();
    else if (key == "@team") CycleSpawnTeam();
    else if (key == "@mute") ToggleMute();
    else if (key == "@zin") zoom = std::min(30.0f, zoom * 1.25f);
    else if (key == "@zout") zoom = std::max(0.2f, zoom / 1.25f);
    else if (key == "@reset") { zoom = 1.0f; map_yaw = 0.6f; map_pitch = 0.8f; }
    else if (key == "@sym") { sym_mode = (sym_mode + 1) % 3; OpenMenu(MenuKind::Advanced); }
    else if (key == "@sympx") { sym_px = sym_px >= 160.0f ? 16.0f : sym_px * 1.5f; OpenMenu(MenuKind::Advanced); }
    else if (key == "@hudrace") { hud_race = (hud_race + 1) % kHudRaceCount; LoadHud(); Message(std::string("HUD: ") + HudRaceName(static_cast<HudRace>(hud_race))); }
    else if (key == "@folder") { if (host) host->RequestDataFolder(); }
    else if (key == "@delete") DeleteSelected();
  }

  void MenuTap(float x, float y) {
    const MenuGeo g = MenuGeometry();
    if (x < g.px || x > g.px + g.pw || y < g.py || y > g.py + g.ph) {
      menu = MenuKind::None;
      return;
    }
    const int row = MenuRowAt(g, x, y);
    if (row < 0) return;
    const size_t idx = static_cast<size_t>(row);
    const std::string key = menu_rows[idx].key;
    if (key.empty()) return;
    if (key[0] == '@') {
      MenuAction(key);
      return;
    }
    if (key == "#race") {
      spawn_filter = (spawn_filter + 1) % (static_cast<int>(spawn_races.size()) + 1);
      menu_scroll = 0;
      RebuildMenu();
      return;
    }
    const MenuKind kind = menu;
    menu = MenuKind::None;
    if (kind == MenuKind::Maps) {
      LoadMap(std::atoi(key.c_str() + 4));
    } else {
      SpawnObject(key);
    }
  }

  // Modell fuer ein gesetztes Objekt bereitstellen (einmal je SOD).
  bool EnsureSpawnModel(const std::string& sod) {
    SpawnModel& sm = spawn_models[sod];
    if (sm.r) return true;
    Model built;
    OrigBounds ob;
    sm.r = BuildMapRenderer(sod, &built, &ob);
    if (!sm.r) {
      spawn_models.erase(sod);
      return false;
    }
    float r2 = 0;
    for (int k = 0; k < 3; ++k) {
      const float d = built.bounds_max[k] - built.bounds_min[k];
      r2 += d * d;
    }
    sm.radius = std::max(1.0f, 0.5f * std::sqrt(r2));
    for (int k = 0; k < 3; ++k) { sm.bounds[k] = ob.mn[k]; sm.bounds[k + 3] = ob.mx[k]; }
    sm.hardpoints = built.hardpoints;
    for (const ModelBatch& b : built.batches)
      for (size_t i = 0; i + 2 < b.indices.size(); i += 3)
        for (int c = 0; c < 3; ++c) {
          const float* v = &b.vertices[static_cast<size_t>(b.indices[i + static_cast<size_t>(c)]) * 8];
          sm.tris.insert(sm.tris.end(), v, v + 3);
        }
    std::unique_ptr<SpriteRenderer> sp(new SpriteRenderer());
    if (sp->Init() && sp->SetNodes(built, spr, *source) > 0) sm.sp = std::move(sp);
    return true;
  }

  // Anzeigename, Spezialwaffen und Flugdaten aus der ODF.
  void InitSpawnedFromOdf(Spawned* s, const std::string& odf) {
    s->label = odf;
    UnitDef u;
    if (!odf_lib || !LoadUnitDef(*odf_lib, odf, &u)) return;
    if (!u.unit_name.empty()) s->label = u.unit_name;
    s->officers = u.officer_cost;
    s->has_energy = u.max_special_energy > 0;
    s->hero = u.is_hero;
    s->cls = u.class_label.empty() ? std::string("?") : u.class_label;
    s->odf_l = ToLowerAscii(odf);
    {
      HealthSpec q;
      const SubsystemDef* sd[kSysCount] = {&u.shield_generator, &u.engines, &u.life_support, &u.sensors, &u.weapons_sys};
      for (int k = 0; k < kSysCount; ++k) {
        q.sys[k].points = sd[k]->hit_points;
        q.sys[k].hit_pct = sd[k]->hit_percent;
        q.sys[k].crew_loss = sd[k]->crew_loss;
        q.sys[k].repair_time = sd[k]->repair_time;
      }
      q.hull_pct = u.hull.hit_percent;
      q.critical_pct = u.critical_percent;
      q.life_support_loss = u.life_support_loss;
      q.shield_max = u.max_health;
      q.shield_rate = u.shield_rate;
      q.shield_delay = u.shield_delay;
      q.crew_max = static_cast<float>(u.crew_cost);
      RaceDef rd;
      LoadRaceDef(*odf_lib, u.race, &rd);
      RaceRules rr;
      rr.repair_strength = rd.repair_strength; rr.crew_yellow = rd.crew_yellow; rr.crew_red = rd.crew_red;
      rr.repair_yellow = rd.repair_yellow; rr.repair_red = rd.repair_red; rr.weapon_yellow = rd.weapon_yellow; rr.weapon_red = rd.weapon_red;
      InitHealth(&s->hp, q, rr);
    }
    if (u.range_scan > 0) s->scan = u.range_scan;
    if (u.damaged_scan > 0) s->scan_damaged = u.damaged_scan;
    if (!u.fireball.empty()) {
      std::string e2;
      if (const OdfObject* fo = odf_lib->Get(u.fireball + ".odf", &e2)) {
        std::string sv;
        if (fo->GetString("explSound", &sv)) s->expl_sound = sv;
        fo->GetFloat("length", &s->expl_len);
        if (fo->GetString("dbName", &sv) && !sv.empty()) s->expl_model = ToLowerAscii(sv);
        s->has_shock = LoadShockwaveDef(*odf_lib, u.fireball, &s->shock);
      }
    }
    auto sm_it = spawn_models.find(s->sod);
    for (const WeaponSlot& ws : u.weapons) {
      WeaponDef wd;
      OrdnanceDef od;
      if (!LoadWeaponDef(*odf_lib, ws.odf, &wd) || wd.special || wd.ordnance.empty() || !LoadOrdnanceDef(*odf_lib, wd.ordnance, &od) || od.special_energy_cost > 0 || wd.shot_delay <= 0) continue;
      WeaponRt w;
      w.name = wd.name.empty() ? wd.odf : wd.name;
      w.sound = wd.fire_sound;
      w.range = wd.range > 0 ? wd.range : 300.0f;
      w.delay = wd.shot_delay;
      w.damage = od.damage;
      w.variance = od.damage_variance;
      w.hit = wd.hit_chance;
      w.ignore_shield = od.ignore_shield;
      w.hi.threshold = od.damage_threshold > 0 ? od.damage_threshold : 1e9f;
      w.hi.shield_crew_mod = od.shield_crew_modifier;
      w.hi.hull_crew_mod = od.hull_crew_modifier;
      w.hi.ignore_shield = od.ignore_shield;
      w.width = od.radius > 0 ? od.radius : 2.0f;
      w.beam = od.shot_speed >= 1e5f;
      w.length = od.length;
      w.speed = od.shot_speed;
      w.life = od.life_span > 0 ? od.life_span : 4.0f;
      w.has_light = od.has_light && !w.beam && od.light_falloff_range > 0;
      for (int k = 0; k < 3; ++k) w.lcol[k] = od.light_color[k];
      w.lstart = od.light_falloff_start;
      w.lrange = od.light_falloff_range;
      if (!od.sprite.empty()) {
        w.sprite = spr.FindSprite(od.sprite);
        if (w.sprite && !w.sprite->anim.empty()) w.anim = spr.FindAnimation(w.sprite->anim);
      }
      if (sm_it != spawn_models.end())
        for (const std::string& h : ws.hardpoints)
          for (const ModelHardpoint& mh : sm_it->second.hardpoints)
            if (mh.name == ToLowerAscii(h)) w.points.push_back(mh);
      if (w.points.empty()) continue;
      s->weapons.push_back(std::move(w));
    }
    if (s->cls == "wingman") s->cls = "Schiff";
    else if (s->cls == "starbase" || s->cls == "shipyard") s->cls = "Station";
    for (const WeaponSlot& w : u.weapons) {
      WeaponDef wd;
      if (LoadWeaponDef(*odf_lib, w.odf, &wd) && wd.special) {
        bool dup = false;
        for (const auto& sp : s->specials) dup = dup || sp.first == w.odf;
        if (!dup && s->specials.size() < 6) s->specials.push_back({w.odf, wd.name.empty() ? w.odf : wd.name});
      }
    }
    PhysicsDef pd;
    if (!u.physics_file.empty() && LoadPhysicsDef(*odf_lib, u.physics_file, &pd)) {
      s->fp = FlightFromPhysics(pd);
      s->hodf_ok = LoadHoverOdf(*odf_lib, u.physics_file, &s->hodf);
      // Nur Schiffe bewegen sich: Stationen, Plattformen und Kartenobjekte haben teils ebenfalls eine physics-Datei.
      static const char* const kFixed[] = {"starbase", "shipyard", "pod", "planet_obj", "background_obj", "nebula_obj", "wormhole_obj", "cannon", "areacannon",
                                           "sensor", "launcher", "areamissile", "detectcloak", "toggleweapon", "upgrade", "scrap", "fireballexplode", "shockwavexplode"};
      bool fixed = u.is_starbase;
      for (const char* k : kFixed) fixed = fixed || u.class_label == k;
      s->movable = s->fp.valid && !fixed;
    }
  }

  // Fussabdruck-Box (Original-z, skaliert) einer Station; false wenn keine Station / kein Modell.
  bool StationBox(const Spawned& s, float* b) {
    int sb = 0;
    const OdfObject* od = odf_lib ? odf_lib->Get(ToLowerAscii(s.odf)) : nullptr;
    auto it = spawn_models.find(s.sod);
    if (!od || !od->GetInt("is_starbase", &sb) || !sb || it == spawn_models.end()) return false;
    const float* mb = it->second.bounds;
    b[0] = s.st.pos[0] + mb[0] * s.scale; b[1] = -s.st.pos[2] - mb[5] * s.scale;
    b[2] = s.st.pos[0] + mb[3] * s.scale; b[3] = -s.st.pos[2] - mb[2] * s.scale;
    return true;
  }
  // Entfernen einer Station (Original 0x461e70 -> 0x409a80): "Unstamp" mit Standardkosten. Der Footprint (198) bleibt dadurch bestehen.
  void UnstampStation(const Spawned& s) {
    float b[4];
    if (!path_grid.Empty() && StationBox(s, b)) path_grid.UnstampBox(b[0], b[1], b[2], b[3]);
  }

  void SpawnObject(const std::string& odf) {
    float p[3] = {map_target[0], map_target[1], map_target[2]};
    SpawnAt(odf, spawn_team, p, true, true);
  }

  // Objekt an eine Stelle setzen. offset_row: nach Anzahl der Objekte seitlich versetzt (Handsetzen), sonst genau dort.
  bool SpawnAt(const std::string& odf, int team, const float pos[3], bool select, bool offset_row) {
    BznObject o;
    o.odf = odf;
    MapObjectInfo info;
    if (!ResolveMapObject(o, &info) || info.sod.empty()) {
      Message(odf + ": kein Modell.");
      return false;
    }
    if (!EnsureSpawnModel(info.sod)) {
      Message(odf + ": Modell nicht ladbar.");
      return false;
    }
    const SpawnModel& sm = spawn_models[info.sod];
    Spawned s;
    s.odf = odf;
    s.sod = info.sod;
    s.scale = info.scale > 0 ? info.scale : 1.0f;
    s.team = team;
    InitSpawnedFromOdf(&s, odf);
    const float off = offset_row ? static_cast<float>(spawned.size() % 5) * sm.radius * s.scale * 2.2f : 0.0f;
    s.st.pos[0] = pos[0] + off;
    s.st.pos[1] = pos[1];
    s.st.pos[2] = pos[2];
    if (select) for (Spawned& o2 : spawned) o2.sel = false;
    s.sel = select;
    s.id = next_id++;
    // Wie im Original (Basis-Init 0x46d5c0 laeuft bei jeder Objekterzeugung, auch beim Bauen): Stationen stempeln ihren Fussabdruck.
    {
      int sb = 0;
      const OdfObject* od = odf_lib ? odf_lib->Get(ToLowerAscii(odf)) : nullptr;
      if (od && od->GetInt("is_starbase", &sb) && sb && !path_grid.Empty())
        path_grid.StampBox(s.st.pos[0] + sm.bounds[0] * s.scale, -s.st.pos[2] - sm.bounds[5] * s.scale, s.st.pos[0] + sm.bounds[3] * s.scale,
                           -s.st.pos[2] - sm.bounds[2] * s.scale, 0.0f, 198);
    }
    spawned.push_back(s);
    if (select) selected = static_cast<int>(spawned.size()) - 1;
    if (offset_row) Message(s.label + (s.movable ? " gesetzt. Ziel auf der Karte antippen = hinfliegen." : " gesetzt (unbeweglich)."));
    return true;
  }

  // Kartenobjekt (BZN) als gesetztes Objekt uebernehmen: Lage, Groesse, Team aus der Instanz; wird dann nicht mehr als Kartenobjekt gezeichnet.
  // Rueckgabe: Index in spawned oder -1.
  int AdoptInstance(size_t ii) {
    if (ii >= scene.instances.size()) return -1;
    const MapInstance& in = scene.instances[ii];
    if (scene_hidden.size() != scene.instances.size()) scene_hidden.assign(scene.instances.size(), 0);
    if (scene_hidden[ii]) return -1;
    const std::string& sod = scene.models[static_cast<size_t>(in.model)];
    if (!EnsureSpawnModel(sod)) return -1;
    const std::string odf = in.object >= 0 && in.object < static_cast<int>(map.objects.size()) ? map.objects[static_cast<size_t>(in.object)].odf : std::string();
    Spawned s;
    s.odf = odf;
    s.sod = sod;
    s.team = in.team;
    const float* m = in.transform.m;
    s.scale = std::max(1e-3f, std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
    InitSpawnedFromOdf(&s, odf);
    for (int k = 0; k < 3; ++k) s.st.pos[k] = m[12 + k];
    const float zl = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
    if (zl > 1e-6f) {
      const float fwd[3] = {-m[8] / zl, -m[9] / zl, -m[10] / zl};
      ShipFromForward(fwd, &s.st);
    }
    scene_hidden[ii] = 1;
    ++scene_hidden_count;
    const unsigned hid = in.object >= 0 && in.object < static_cast<int>(map.objects.size()) ? (map.objects[static_cast<size_t>(in.object)].handle & 0x7fffffffu) : 0;
    s.id = hid && FindById(hid) < 0 ? hid : next_id++;  // Kennung = Handle des Kartenobjekts (fuer den Spielstand)
    spawned.push_back(s);
    return static_cast<int>(spawned.size()) - 1;
  }

  void CycleSpawnTeam() {
    spawn_team = spawn_team % 8 + 1;
    if (selected >= 0 && selected < static_cast<int>(spawned.size())) spawned[static_cast<size_t>(selected)].team = spawn_team;
    Message("Team " + std::to_string(spawn_team));
  }

  int SelCount() const {
    int n = 0;
    for (const Spawned& x : spawned) n += x.sel ? 1 : 0;
    return n;
  }

  void SelectOnly(int i) {
    for (Spawned& x : spawned) x.sel = false;
    selected = i;
    if (i >= 0 && i < static_cast<int>(spawned.size())) spawned[static_cast<size_t>(i)].sel = true;
    else selected = -1;
  }

  void ToggleSel(int i) {
    Spawned& x = spawned[static_cast<size_t>(i)];
    x.sel = !x.sel;
    if (x.sel) {
      selected = i;
    } else if (selected == i) {
      selected = -1;
      for (size_t k = 0; k < spawned.size(); ++k)
        if (spawned[k].sel) selected = static_cast<int>(k);
    }
  }

  void StopSelected() {
    attack_armed = move_armed = transport_armed = false;
    if (SelCount() > 0) {
      for (Spawned& s : spawned)
        if (s.sel) {
          StopShip(&s.st);
          s.target_id = 0;
          s.auto_move = 0;
          s.beam_id = 0;
        }
    } else {
      for (Spawned& s : spawned) StopShip(&s.st);
    }
  }

  void DeleteSelected() {
    const int n = SelCount();
    if (n == 0) {
      Message("Nichts ausgewaehlt.");
      return;
    }
    for (const Spawned& x : spawned) if (x.sel) UnstampStation(x);
    spawned.erase(std::remove_if(spawned.begin(), spawned.end(), [](const Spawned& x) { return x.sel; }), spawned.end());
    selected = -1;
    Message(std::to_string(n) + (n == 1 ? " Objekt entfernt." : " Objekte entfernt."));
  }

  void ShowSelectedInfo() {
    if (selected >= 0 && selected < static_cast<int>(spawned.size()) && odf_lib) {
      UnitDef u;
      if (LoadUnitDef(*odf_lib, spawned[static_cast<size_t>(selected)].odf, &u)) {
        Message(DescribeUnit(*odf_lib, u));
        return;
      }
    }
    ShowMapInfo();
  }

  int FindById(unsigned id) const {
    if (id == 0) return -1;
    for (size_t i = 0; i < spawned.size(); ++i)
      if (spawned[i].id == id) return static_cast<int>(i);
    return -1;
  }

  static void Xf(const Mat4& m, const float p[3], float o[3]) {
    for (int i = 0; i < 3; ++i) o[i] = m.m[i] * p[0] + m.m[4 + i] * p[1] + m.m[8 + i] * p[2] + m.m[12 + i];
  }

  float RadiusOf(const Spawned& s) const {
    auto it = spawn_models.find(s.sod);
    return (it != spawn_models.end() ? it->second.radius : 20.0f) * s.scale;
  }

  bool Hostile(const Spawned& a, const Spawned& b) const { return a.team > 0 && b.team > 0 && a.team != b.team && b.hp.alive() && !b.derelict; }

  static float Dist3(const float a[3], const float b[3]) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

  bool CanFly(const Spawned& s) const { return s.movable && !s.derelict && s.hp.alive() && s.hp.sys_ok(kEngines); }

  // Treffer anwenden; Ziel und Nachbarn desselben Teams gelten danach als angegriffen (Alarmstufen).
  void HitShip(Spawned& t, unsigned owner, const HitInfo& hi) {
    ApplyHit(&t.hp, &rng, hi);
    if (t.team <= 0) return;
    t.attacker_id = owner;
    t.attacked_t = 10.0f;
    for (Spawned& n : spawned)
      if (n.team == t.team && n.id != t.id && Dist3(n.st.pos, t.st.pos) < 400.0f) {
        n.attacker_id = owner;
        n.attacked_t = 10.0f;
      }
  }

  void MakeDerelict(Spawned& s) {
    s.derelict = true;
    s.team = 0;
    s.target_id = 0;
    s.beam_id = 0;
    s.auto_move = 0;
    StopShip(&s.st);  // Auswahl bleibt: im Original ist ein Schiff bis zur Zerstoerung waehlbar (docs/formats/ship-health.md)
    Message(s.label + " hat keine Besatzung mehr und treibt fuehrerlos.");
  }

  // Transporter: alle 1 s werden Leute zum Ziel gebeamt (Reichweite = rangeScan). Befreundet/herrenlos: Besatzung wird uebergeben,
  // das Schiff gehoert dann dem Absender. Feind: nur bei gesenktem Schild, Enterkommando; bei 0 Besatzung geht das Schiff an den Enterer.
  // Annahmen: 10 Mann je Sekunde; Entermannschaft verliert halb so viele Leute, wie sie dem Gegner nimmt.
  void DoBeam(Spawned& s) {
    constexpr float kBeamRate = 10.0f;
    const int ti = FindById(s.beam_id);
    if (ti < 0 || s.derelict || s.hp.crew <= 1.0f) { s.beam_id = 0; return; }
    Spawned& t = spawned[static_cast<size_t>(ti)];
    if (Dist3(s.st.pos, t.st.pos) - RadiusOf(t) > s.scan) {
      Message("Transport: Ziel ausser Reichweite.");
      s.beam_id = 0;
      return;
    }
    const float give = std::min(kBeamRate, s.hp.crew - 1.0f);
    if (t.derelict || t.team == s.team) {
      const float room = std::max(0.0f, t.hp.crew_max - t.hp.crew);
      const float n = std::min(give, room);
      if (n <= 0) { s.beam_id = 0; return; }
      SetCrew(&s.hp, s.hp.crew - n);
      SetCrew(&t.hp, t.hp.crew + n);
      if (t.derelict) {
        t.derelict = false;
        t.team = s.team;
        t.alert = 1;
        Message(t.label + " uebernommen.");
      }
    } else if (t.team > 0) {
      if (t.hp.shield_max > 0 && t.hp.shield > 0) {
        Message("Transport: Schild des Feindes ist noch oben.");
        s.beam_id = 0;
        return;
      }
      SetCrew(&s.hp, s.hp.crew - give * 0.5f);
      SetCrew(&t.hp, t.hp.crew - give);
      if (t.hp.derelict) {
        SetCrew(&t.hp, give * 0.5f);
        t.team = s.team;
        t.target_id = 0;
        t.derelict = false;
        t.alert = 1;
        StopShip(&t.st);
        Message(t.label + " geentert!");
        s.beam_id = 0;
      }
    } else {
      s.beam_id = 0;
    }
  }

  int NearestHostile(size_t i, float scan) const {
    const Spawned& s = spawned[i];
    int best = -1;
    float bd = scan;
    for (size_t j = 0; j < spawned.size(); ++j) {
      if (j == i || !Hostile(s, spawned[j])) continue;
      const float d = Dist3(s.st.pos, spawned[j].st.pos) - RadiusOf(spawned[j]);
      if (d < bd) {
        bd = d;
        best = static_cast<int>(j);
      }
    }
    return best;
  }

  // Gefecht (Originalregeln siehe Combat.h): Schild laedt auf, Besatzung/Systeme, Wracks, Alarmstufen
  // (Rot: feuert auf alles Sichtbare, verfolgt begrenzt; Gelb: nur wenn selbst/Nachbarn angegriffen; Gruen: flieht, feuert nicht; Abwarten: kein Ziel),
  // Waffen mit Besatzungsstatus-Verzoegerung, Strahlen treffen sofort, Torpedos fliegen. Annahmen: Verfolgungsradius 1,5 x Sensorreichweite,
  // Nachbarn = 400 m, Angriffsstatus 10 s; Schussfelder werden nicht beachtet.
  void StepCombat(float dt) {
    // Glow-Phase (0x4522f0): pro Originalbild phase += |sin(freq*phase)| * range * rate + min; hier mit 30 Bildern/s (Annahme)
    neb_ticks += dt * 30.0;
    while (neb_ticks >= 1.0) {
      neb_ticks -= 1.0;
      for (NebLight& l : neb_lights) l.phase += std::fabs(std::sin(l.freq * l.phase)) * l.step_range * l.rate + l.min_step;
    }
    const unsigned sel_id = (selected >= 0 && selected < static_cast<int>(spawned.size())) ? spawned[static_cast<size_t>(selected)].id : 0;
    for (Spawned& s : spawned) {
      StepHealth(&s.hp, dt);
      s.attacked_t = std::max(0.0f, s.attacked_t - dt);
      if (s.hp.derelict && !s.derelict) MakeDerelict(s);
      if (s.beam_id) {
        s.beam_timer -= dt;
        if (s.beam_timer <= 0) {
          s.beam_timer = 1.0f;
          DoBeam(s);
        }
      }
    }
    for (size_t i = 0; i < spawned.size(); ++i) {
      Spawned& s = spawned[i];
      if (s.derelict || !s.hp.alive()) continue;
      const bool armed = !s.weapons.empty() && s.hp.sys_ok(kWeapons);
      const float scan = s.hp.sys_ok(kSensors) ? s.scan : s.scan_damaged;
      int tgt = FindById(s.target_id);
      if (tgt >= 0 && !Hostile(s, spawned[static_cast<size_t>(tgt)])) {
        tgt = -1;
        s.target_id = 0;
      }
      if (tgt < 0) s.target_id = 0;
      int fire_at = armed ? tgt : -1;
      if (armed && fire_at < 0 && s.alert == 2) fire_at = NearestHostile(i, scan);
      if (armed && fire_at < 0 && s.alert == 1 && s.attacked_t > 0) {
        const int a = FindById(s.attacker_id);
        if (a >= 0 && Hostile(s, spawned[static_cast<size_t>(a)])) fire_at = a;
      }
      // Bewegung: Angriffsbefehl > Verfolgung (Rot) > Flucht (Gruen) > Rueckkehr
      if (CanFly(s)) {
        s.retarget -= dt;
        if (s.retarget <= 0) {
          s.retarget = 0.5f;
          int chase = -1;
          if (armed && tgt >= 0) {
            chase = tgt;
            s.auto_move = 0;
          } else if (armed && s.alert == 2 && fire_at >= 0 && (!s.st.has_target || s.auto_move == 1)) {
            if (s.auto_move != 1) {
              for (int k = 0; k < 3; ++k) s.anchor[k] = s.st.pos[k];
              s.auto_move = 1;
            }
            if (Dist3(s.st.pos, s.anchor) < scan * 1.5f) chase = fire_at;
          } else if (s.auto_move == 1) {
            s.auto_move = 3;
            SetShipTarget(&s.st, s.anchor);
          }
          if (chase >= 0) {
            float standoff = 1e30f;
            for (const WeaponRt& w : s.weapons) standoff = std::min(standoff, w.range);
            standoff *= 0.8f;
            const Spawned& t = spawned[static_cast<size_t>(chase)];
            float dv[3], dist = 0;
            for (int k = 0; k < 3; ++k) {
              dv[k] = s.st.pos[k] - t.st.pos[k];
              dist += dv[k] * dv[k];
            }
            dist = std::sqrt(dist);
            if (dist > standoff && dist > 1e-3f) {
              const float p[3] = {t.st.pos[0] + dv[0] / dist * standoff * 0.9f, t.st.pos[1] + dv[1] / dist * standoff * 0.9f, t.st.pos[2] + dv[2] / dist * standoff * 0.9f};
              SetShipTarget(&s.st, p);
            } else if (s.auto_move == 1) {
              StopShip(&s.st);
            }
          }
          if (s.alert == 0 && s.attacked_t > 0 && tgt < 0 && (!s.st.has_target || s.auto_move == 2)) {
            const int a = FindById(s.attacker_id);
            if (a >= 0) {
              const Spawned& t = spawned[static_cast<size_t>(a)];
              float dv[3], dist = 0;
              for (int k = 0; k < 3; ++k) {
                dv[k] = s.st.pos[k] - t.st.pos[k];
                dist += dv[k] * dv[k];
              }
              dist = std::max(1e-3f, std::sqrt(dist));
              const float p[3] = {s.st.pos[0] + dv[0] / dist * 600.0f, s.st.pos[1], s.st.pos[2] + dv[2] / dist * 600.0f};
              SetShipTarget(&s.st, p);
              s.auto_move = 2;
            }
          } else if (s.auto_move == 2 && s.attacked_t <= 0) {
            s.auto_move = 0;
            StopShip(&s.st);
          } else if (s.auto_move == 3 && !s.st.has_target) {
            s.auto_move = 0;
          }
        }
      }
      if (fire_at < 0) continue;
      Spawned& t = spawned[static_cast<size_t>(fire_at)];
      Mat4 m, r;
      ShipMatrices(s.st, s.scale, &m, &r);
      const float tr = RadiusOf(t);
      bool sound_done = false;
      for (WeaponRt& w : s.weapons) {
        w.timer -= dt;
        if (w.timer > 0) continue;
        float dist = 0;
        for (int k = 0; k < 3; ++k) dist += (t.st.pos[k] - s.st.pos[k]) * (t.st.pos[k] - s.st.pos[k]);
        dist = std::sqrt(dist);
        if (dist - tr > w.range) {
          w.timer = 0;  // bereit, sobald das Ziel in Reichweite ist
          continue;
        }
        w.timer = w.delay * s.hp.weapon_delay_mult();
        for (const ModelHardpoint& hp : w.points) {
          float o[3];
          Xf(m, hp.pos, o);
          float d[3], dl = 0;
          for (int k = 0; k < 3; ++k) {
            d[k] = t.st.pos[k] - o[k];
            dl += d[k] * d[k];
          }
          dl = std::sqrt(dl);
          if (dl < 1e-3f) continue;
          const bool hit = RollHit(&rng, w.hit.For(t.odf_l));
          HitInfo hi = w.hi;
          hi.damage = RollDamage(&rng, w.damage.For(t.odf_l), w.variance);
          Shot sh;
          sh.sprite = w.sprite;
          sh.anim = w.anim;
          sh.width = w.width;
          for (int k = 0; k < 3; ++k) sh.pos[k] = o[k];
          if (w.beam) {
            sh.kind = Shot::Kind::Beam;
            for (int k = 0; k < 3; ++k) sh.dir[k] = d[k] / dl;
            sh.length = dl;
            sh.life = std::max(0.3f, std::min(w.life, 1.0f));
            fx.push_back(sh);
            if (hit) HitShip(t, s.id, hi);
          } else {
            Projectile pr;
            sh.kind = w.length > 0 ? Shot::Kind::Bolt : Shot::Kind::Ball;
            sh.length = w.length;
            sh.speed = w.speed;
            sh.life = w.life;
            float aim[3] = {d[0], d[1], d[2]};
            if (!hit) {  // Fehlschuss: an der Seite vorbei
              aim[0] += (rng.F01() * 2 - 1) * tr * 3.0f;
              aim[2] += (rng.F01() * 2 - 1) * tr * 3.0f;
              const float al = std::sqrt(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
              for (int k = 0; k < 3; ++k) aim[k] = aim[k] / al * dl;
            }
            const float al = std::sqrt(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
            for (int k = 0; k < 3; ++k) sh.dir[k] = aim[k] / std::max(1e-3f, al);
            pr.vis = sh;
            pr.owner = s.id;
            pr.target = t.id;
            pr.hi = hi;
            pr.will_hit = hit;
            pr.has_light = w.has_light;
            for (int k = 0; k < 3; ++k) pr.lcol[k] = w.lcol[k];
            pr.lstart = w.lstart;
            pr.lrange = w.lrange;
            projectiles.push_back(pr);
          }
        }
        if (!sound_done && !w.sound.empty()) {
          PlayEffect(w.sound, 0.4f);
          sound_done = true;
        }
      }
    }
    // Geschosse fliegen; Treffer richten Schaden an
    for (Projectile& p : projectiles) {
      p.vis.age += dt;
      const int ti = FindById(p.target);
      if (ti >= 0 && p.will_hit) {  // suchen das Ziel
        Spawned& t = spawned[static_cast<size_t>(ti)];
        float d[3], dl = 0;
        for (int k = 0; k < 3; ++k) {
          d[k] = t.st.pos[k] - p.vis.pos[k];
          dl += d[k] * d[k];
        }
        dl = std::sqrt(dl);
        if (dl > 1e-3f)
          for (int k = 0; k < 3; ++k) p.vis.dir[k] = d[k] / dl;
        if (dl <= std::max(RadiusOf(t) * 0.7f, p.vis.speed * dt)) {
          HitShip(t, p.owner, p.hi);
          p.vis.age = p.vis.life;  // verbraucht
          continue;
        }
      }
      for (int k = 0; k < 3; ++k) p.vis.pos[k] += p.vis.dir[k] * p.vis.speed * dt;
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return p.vis.age >= p.vis.life; }), projectiles.end());
    for (Shot& sh : fx) {
      sh.age += dt;
      if (sh.owner == 0) continue;  // Sterbeeffekte folgen dem Schiff (Original: Position im Modellraum des Besitzers)
      const int oi = FindById(sh.owner);
      if (oi < 0) continue;
      Mat4 m, r;
      ShipMatrices(spawned[static_cast<size_t>(oi)].st, spawned[static_cast<size_t>(oi)].scale, &m, &r);
      Xf(m, sh.local, sh.pos);
    }
    for (Fireball& f : fireballs) {
      f.age += dt;
      if (f.shock_pending && f.age > f.shock.delay) {
        f.shock_pending = false;
        ShockInst w;
        w.def = f.shock;
        for (int k = 0; k < 3; ++k) w.sw.pos[k] = f.pos[k];
        w.sw.scale = f.radius;
        shockwaves.push_back(w);
      }
    }
    for (ShockInst& w : shockwaves) {
      w.sw.age += dt;
      for (Spawned& t : spawned) {  // Original 0x51d2a0: Schaden an Objekten im Radius, nicht an bereits zerstoerten
        if (!t.hp.alive()) continue;
        HitInfo hi;
        if (ShockwaveHit(w.def, w.sw, Dist3(t.st.pos, w.sw.pos), t.odf, rng.F01(), dt, &hi)) HitShip(t, 0, hi);
      }
    }
    shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(), [](const ShockInst& w) { return ShockwaveDone(w.def, w.sw); }), shockwaves.end());
    fireballs.erase(std::remove_if(fireballs.begin(), fireballs.end(), [](const Fireball& f) { return f.age >= f.life; }), fireballs.end());
    fx.erase(std::remove_if(fx.begin(), fx.end(), [](const Shot& sh) { return sh.age >= sh.life; }), fx.end());
    // Zerstoerte Objekte (Original Craft::Explode/Update, docs/formats/ship-health.md): Auswahl endet sofort, das Schiff fliegt weiter
    // (Antrieb gilt als aus, der Drehimpuls bleibt), alle 0,05 s ein kleiner Effekt (systemexpl1..6), nach ~3 s die grosse Explosion
    // (hier Sprite "bigorange" statt Xplode-Modell, Ton und Dauer aus der fireball-ODF) und Entfernen.
    // Annahmen: Feld +0x60 = Radius, Effektgroesse in Weltenheiten.
    bool sel_changed = false, any_gone = false;
    for (Spawned& s : spawned) {
      if (s.hp.alive()) continue;
      if (!s.dying) {
        s.dying = true;
        s.die = MakeDying(std::sqrt(s.st.vel[0] * s.st.vel[0] + s.st.vel[1] * s.st.vel[1] + s.st.vel[2] * s.st.vel[2]), s.hero);
        s.target_id = 0;
        s.beam_id = 0;
        s.auto_move = 0;
        if (s.sel || s.id == sel_id) sel_changed = true;
        s.sel = false;
        Message(s.label + " zerstoert.");
      }
      bool done = false;
      const int puffs = StepDying(&s.die, dt, &done);
      for (int k = 0; k < puffs; ++k) {
        // Original (0x466320/0x466180): zufaelliger Effekt systemexpl1..7 an einem zufaelligen Eckpunkt eines zufaelligen Dreiecks
        const SprSprite* sx = spr.FindSprite(std::string("systemexpl") + static_cast<char>('1' + static_cast<int>(rng.F01() * kDyingFxKinds) % kDyingFxKinds));
        if (!sx) break;
        Shot e;
        e.kind = Shot::Kind::Ball;
        e.sprite = sx;
        e.anim = sx->anim.empty() ? nullptr : spr.FindAnimation(sx->anim);
        e.width = DyingFxSize(RadiusOf(s));  // halbe Kantenlaenge in Welteinheiten (0x5975e0: Mitte +- Groesse)
        e.life = 1.0f;                       // Lebensdauer im Original: letzter Parameter von 0x515e30
        e.owner = s.id;
        for (int c = 0; c < 3; ++c) e.pos[c] = s.st.pos[c];
        auto sm_it = spawn_models.find(s.sod);
        if (sm_it != spawn_models.end() && !sm_it->second.tris.empty()) {
          const std::vector<float>& tr = sm_it->second.tris;
          const size_t ntri = tr.size() / 9;
          const size_t t = std::min(ntri - 1, static_cast<size_t>(rng.F01() * static_cast<float>(ntri)));
          const size_t v = std::min<size_t>(2, static_cast<size_t>(rng.F01() * 3.0f));
          for (int c = 0; c < 3; ++c) e.local[c] = tr[t * 9 + v * 3 + static_cast<size_t>(c)];
          Mat4 m, r;
          ShipMatrices(s.st, s.scale, &m, &r);
          Xf(m, e.local, e.pos);
        }
        fx.push_back(e);
      }
      if (!done) continue;
      any_gone = true;
      if (!s.expl_sound.empty()) PlayEffect(s.expl_sound, 0.6f);
      // Original: das fireball-Objekt (Modell dbName, hier Xplode.sod aus acht Sprite-Knoten); das Modell wird mit Faktor 1,0 geladen,
      // der Objektmassstab (Schiffsradius) bestimmt nur die Schockwelle (0x51b290/0x51b330)
      bool fireball_ok = false;
      if (EnsureSpawnModel(s.expl_model) && spawn_models[s.expl_model].sp) {
        Fireball f;
        f.model = s.expl_model;
        for (int k = 0; k < 3; ++k) f.pos[k] = s.st.pos[k];
        f.scale = 1.0f;
        f.life = std::max(1.5f, s.expl_len);
        // Original (0x51b330): nach shockwaveDelay, nur wenn der Objektmassstab (Schiffsradius) ueber shockwaveMinimum liegt
        f.radius = RadiusOf(s);
        {
          float v[3], l2;
          do {
            for (float& c : v) c = rng.F01() * 2.0f - 1.0f;
            l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
          } while (l2 > 1.0f || l2 < 1e-4f);
          const float inv = 1.0f / std::sqrt(l2);
          for (int c = 0; c < 3; ++c) f.axis[c] = v[c] * inv;
        }
        f.shock_pending = s.has_shock && RadiusOf(s) > s.shock.minimum;
        f.shock = s.shock;
        fireballs.push_back(f);
        fireball_ok = true;
      }
      if (const SprSprite* ex = fireball_ok ? nullptr : spr.FindSprite("bigorange")) {
        Shot e;
        e.kind = Shot::Kind::Ball;
        e.sprite = ex;
        e.anim = ex->anim.empty() ? nullptr : spr.FindAnimation(ex->anim);
        e.width = std::max(30.0f, RadiusOf(s) * 2.5f);
        e.life = std::max(1.0f, s.expl_len);
        for (int k = 0; k < 3; ++k) e.pos[k] = s.st.pos[k];
        fx.push_back(e);
      }
    }
    if (any_gone) {
      const auto gone = [](const Spawned& s) { return s.dying && s.die.left <= 0; };
      for (const Spawned& s : spawned) if (gone(s)) UnstampStation(s);
      spawned.erase(std::remove_if(spawned.begin(), spawned.end(), gone), spawned.end());
    }
    if (sel_changed) {  // Hauptobjekt war betroffen: naechstes ausgewaehltes Objekt uebernehmen
      selected = -1;
      for (size_t i = 0; i < spawned.size() && selected < 0; ++i)
        if (spawned[i].sel) selected = static_cast<int>(i);
    } else if (any_gone) {
      selected = FindById(sel_id);
    }
  }

  void StepSpawned(float dt) {
    int n = static_cast<int>(std::ceil(dt / 0.05f));
    n = std::max(1, std::min(n, 4));
    std::vector<HoverObstacle> obs;
    for (Spawned& s : spawned) {
      if (!s.movable) continue;
      const bool can_fly = CanFly(s);
      if (!can_fly) StopShip(&s.st);  // Antrieb ausgefallen oder Wrack: kein Kurs, nur Ausrollen
      if (s.hodf_ok) {
        // Original-Flugphysik und -steuerung (HoverSim); Hindernisse = die anderen Schiffe
        if (!s.hv.ok) s.hv.Init(s.hodf, s.st);
        s.hv.st.radius = RadiusOf(s);
        obs.clear();
        for (const Spawned& o : spawned) {
          if (&o == &s || !o.hp.alive()) continue;
          obs.push_back(MakeHoverObstacle(o.st.pos, o.st.vel, RadiusOf(o), o.movable));
        }
        for (int i = 0; i < n; ++i) HoverShipStep(&s.hv, &s.st, can_fly, dt / static_cast<float>(n), &obs, &path_grid);
      } else {
        for (int i = 0; i < n; ++i) StepShip(s.fp, &s.st, dt / static_cast<float>(n));
      }
    }
  }

  // Blickstrahl durch einen Bildschirmpunkt (Welt-GL); beruecksichtigt die Bildmitten-Verschiebung ueber dem HUD.
  // Verschiebung der Bildmitte (Bildraum, +1 = halbe Bildhoehe nach oben) in die Mitte des freien Bereichs zwischen den HUD-Feldern.
  float CentreShift() const {
    // Die 3D-Szene fuellt den ganzen Bildschirm hinter dem HUD; nur der Drehpunkt (Szenenmitte) sitzt etwas hoeher,
    // in Richtung der Mitte des freien Bereichs, hoechstens um kMaxCentreShift (Bildraum, 1 = halbe Bildhoehe).
    constexpr float kMaxCentreShift = 0.12f;
    if (!hud_ok || hud.view.h <= 0) return 0.0f;
    const float H = static_cast<float>(std::max(1, height));
    return std::max(0.0f, std::min(kMaxCentreShift, (H - 2.0f * hud.view.y - hud.view.h) / H));
  }

  void ScreenRay(float x, float y, float o[3], float d[3]) const {
    const float W = static_cast<float>(std::max(1, width)), H = static_cast<float>(std::max(1, height));
    const float nx = 2.0f * x / W - 1.0f, ny = 1.0f - 2.0f * y / H - CentreShift();
    const float t = std::tan(22.5f * kPi / 180.0f);
    const float c[3] = {nx * t * (W / H), ny * t, -1.0f};
    const Mat4 rot = Mat4::RotationX(map_pitch) * Mat4::RotationY(map_yaw);
    const float dist = map_dist / zoom;
    for (int i = 0; i < 3; ++i) {
      d[i] = rot.m[i * 4] * c[0] + rot.m[i * 4 + 1] * c[1] + rot.m[i * 4 + 2] * c[2];
      o[i] = map_target[i] + rot.m[i * 4 + 2] * dist;
    }
    const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (int i = 0; i < 3; ++i) d[i] /= l;
  }

  // Bodenpunkt (Ebene y = Zielhoehe) unter einem Bildschirmpunkt; false wenn der Strahl den Boden nicht trifft.
  bool GroundPoint(float x, float y, float out[3]) const {
    float o[3], d[3];
    ScreenRay(x, y, o, d);
    if (std::fabs(d[1]) < 1e-4f) return false;
    const float t = (kTapPlaneY - o[1]) / d[1];
    if (t <= 0 || t > 40.0f * map_dist / zoom) return false;
    for (int k = 0; k < 3; ++k) out[k] = o[k] + d[k] * t;
    return true;
  }

  // Ansicht verschieben: Der Bodenpunkt unter dem Finger folgt dem Finger.
  void PanMap(float x0, float y0, float x1, float y1) {
    float a[3], b[3];
    if (!GroundPoint(x0, y0, a) || !GroundPoint(x1, y1, b)) return;
    for (int k = 0; k < 3; ++k) map_target[k] += a[k] - b[k];
  }

  // Ansicht um den Bodenpunkt P drehen (Gier dyaw, Neigung dpitch); P bleibt an derselben Bildstelle.
  void OrbitAbout(const float P[3], float dyaw, float dpitch) {
    const Mat4 r_old = Mat4::RotationX(map_pitch) * Mat4::RotationY(map_yaw);
    map_yaw += dyaw;
    map_pitch = std::max(-1.5f, std::min(1.5f, map_pitch + dpitch));
    const Mat4 r_new = Mat4::RotationX(map_pitch) * Mat4::RotationY(map_yaw);
    float w[3], v[3], u[3];
    for (int i = 0; i < 3; ++i) w[i] = P[i] - map_target[i];
    for (int j = 0; j < 3; ++j) v[j] = r_old.m[0 * 4 + j] * w[0] + r_old.m[1 * 4 + j] * w[1] + r_old.m[2 * 4 + j] * w[2];  // R*w
    for (int i = 0; i < 3; ++i) u[i] = r_new.m[i * 4] * v[0] + r_new.m[i * 4 + 1] * v[1] + r_new.m[i * 4 + 2] * v[2];       // R'^T*v
    for (int i = 0; i < 3; ++i) map_target[i] = P[i] - u[i];
  }

  // Zwei-Finger-Geste in der Karte: Zoomen (Spreizen), Verdrehen (Gier), gemeinsames Ziehen (Gier/Neigung), um den Punkt unter den Fingern.
  float g_cx = 0, g_cy = 0, g_ang = 0, g_dist = 0;
  bool g_have = false;

  // Naechstes Objekt unter dem Finger (Kugel um die Modellmitte, mindestens ein Anteil der Kameradistanz).
  // Gesetzte Objekte und Kartenobjekte mit Team (Schiffe, Stationen); ein getroffenes Kartenobjekt wird dabei uebernommen.
  // Rueckgabe: Index in spawned, -1 = nichts.
  int PickAny(const float o[3], const float d[3]) {
    const float dist = map_dist / zoom;
    auto hit = [&](const float c[3], float rad, float* t) {
      const float r = std::max(rad, dist * 0.03f);
      const float oc[3] = {c[0] - o[0], c[1] - o[1], c[2] - o[2]};
      const float tca = oc[0] * d[0] + oc[1] * d[1] + oc[2] * d[2];
      if (tca < 0) return false;
      const float d2 = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - tca * tca;
      *t = tca;
      return d2 <= r * r;
    };
    int best = -1, best_inst = -1;
    float bt = 1e30f;
    for (size_t i = 0; i < spawned.size(); ++i) {
      const Spawned& s = spawned[i];
      if (!s.hp.alive()) continue;  // zerstoert: nicht mehr waehlbar (Original: Auswahl endet bei Explode)
      auto it = spawn_models.find(s.sod);
      float t;
      if (hit(s.st.pos, it != spawn_models.end() ? it->second.radius * s.scale : 10.0f, &t) && t < bt) {
        bt = t;
        best = static_cast<int>(i);
        best_inst = -1;
      }
    }
    for (size_t i = 0; i < scene.instances.size(); ++i) {
      const MapInstance& in = scene.instances[i];
      if (in.team <= 0 || (i < scene_hidden.size() && scene_hidden[i]) || !map_models[static_cast<size_t>(in.model)]) continue;
      const float* m = in.transform.m;
      const float sc = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
      const float c[3] = {m[12], m[13], m[14]};
      float t;
      if (hit(c, map_radius[static_cast<size_t>(in.model)] * sc, &t) && t < bt) {
        bt = t;
        best = -1;
        best_inst = static_cast<int>(i);
      }
    }
    if (best_inst >= 0) return AdoptInstance(static_cast<size_t>(best_inst));
    return best;
  }

  // Langer Druck: Objekt zur Auswahl hinzufuegen/entfernen; auf leerem Grund: Auswahl aufheben.
  void WorldLongPress(float x, float y) {
    float o[3], d[3];
    ScreenRay(x, y, o, d);
    const int best = PickAny(o, d);
    if (best < 0) {
      if (SelCount() > 0) Message("Auswahl aufgehoben.");
      SelectOnly(-1);
      return;
    }
    ToggleSel(best);
    cmd_page = CmdPage::Main;
    const int n = SelCount();
    Message(spawned[static_cast<size_t>(best)].label + (spawned[static_cast<size_t>(best)].sel ? " hinzugefuegt" : " abgewaehlt") + " (" + std::to_string(n) + " ausgewaehlt).");
  }

  void WorldTap(float x, float y) {
    float o[3], d[3];
    ScreenRay(x, y, o, d);
    const int best = move_armed ? -1 : PickAny(o, d);
    if (attack_armed) {
      attack_armed = false;
      if (best >= 0) {
        const Spawned& t = spawned[static_cast<size_t>(best)];
        int n = 0;
        for (Spawned& a : spawned)
          if (a.sel && !a.weapons.empty() && Hostile(a, t)) {
            a.target_id = t.id;
            ++n;
          }
        Message(n > 0 ? "Angriff auf " + t.label : "Kein gueltiges Ziel.");
      } else {
        Message("Kein Ziel getroffen.");
      }
      return;
    }
    if (transport_armed) {
      transport_armed = false;
      if (best >= 0) {
        Spawned& t = spawned[static_cast<size_t>(best)];
        int n = 0;
        for (Spawned& a : spawned)
          if (a.sel && a.id != t.id && !a.derelict && a.hp.crew > 1.0f) {
            a.beam_id = t.id;
            a.beam_timer = 0;
            ++n;
          }
        Message(n > 0 ? "Transport zu " + t.label : "Kein gueltiges Ziel.");
      } else {
        Message("Kein Ziel getroffen.");
      }
      return;
    }
    if (best >= 0) {
      if (selected != best || SelCount() != 1) cmd_page = CmdPage::Main;
      SelectOnly(best);
      Message(spawned[static_cast<size_t>(best)].label + " ausgewaehlt.");
      return;
    }
    // Ziel fuer alle ausgewaehlten beweglichen Schiffe; mehrere werden quer zur Blickrichtung nebeneinander gestaffelt.
    std::vector<size_t> mv;
    for (size_t i = 0; i < spawned.size(); ++i)
      if (spawned[i].sel && spawned[i].movable) mv.push_back(i);
    if (!mv.empty() && !move_armed && !tap_moves) return;  // Ohne vorher gedrueckte Bewegen-Taste kein Ziel (Vertippen beim Auswaehlen)
    if (!mv.empty()) {
      const Spawned& lead = spawned[selected >= 0 && spawned[static_cast<size_t>(selected)].movable ? static_cast<size_t>(selected) : mv[0]];
      if (std::fabs(d[1]) > 1e-4f) {
        const float t = (kTapPlaneY - o[1]) / d[1];   // Tippebene immer y = 0
        if (t > 0) {
          const Mat4 rot = Mat4::RotationX(map_pitch) * Mat4::RotationY(map_yaw);
          float rx = rot.m[0], rz = rot.m[8];
          const float rl = std::sqrt(rx * rx + rz * rz);
          if (rl > 1e-4f) { rx /= rl; rz /= rl; } else { rx = 1; rz = 0; }
          float spacing = 0;
          for (size_t i : mv) {
            auto it = spawn_models.find(spawned[i].sod);
            spacing = std::max(spacing, (it != spawn_models.end() ? it->second.radius : 20.0f) * spawned[i].scale * 2.5f);
          }
          for (size_t k = 0; k < mv.size(); ++k) {
            Spawned& s = spawned[mv[k]];
            const float off = (static_cast<float>(k) - 0.5f * static_cast<float>(mv.size() - 1)) * spacing;
            const float tg[3] = {o[0] + d[0] * t + rx * off, kTapPlaneY, o[2] + d[2] * t + rz * off};
            if (!CanFly(s)) continue;
            s.auto_move = 0;
            SetShipTarget(&s.st, tg);
          }
        }
      }
      move_armed = false;
      return;
    }
    SelectOnly(-1);
  }

  void ShowMapInfo() {
    if (!map_loaded) return;
    if (map_current < 0) return;
    long teams[9] = {0};
    for (const BznObject& o : map.objects)
      if (o.team >= 0 && o.team < 9) ++teams[o.team];
    std::string t = map.title + " (" + map_files[static_cast<size_t>(map_current)] + ")\nRasse " + map.race + ", Tech-Tree " + map.tech_tree + ", Hintergrund " +
                    map.background + "\nObjekte: " + std::to_string(map.objects.size()) + ", Pfade: " + std::to_string(map.paths.size()) + "\nTeams:";
    for (int i = 0; i < 9; ++i)
      if (teams[i]) t += " " + std::to_string(i) + "=" + std::to_string(teams[i]);
    Message(t);
  }

  void Message(const std::string& t) {
    FLEET_LOGI("%s", t.c_str());
    overlay = t;
    overlay_until = clock + std::min(12.0, 4.0 + 0.05 * static_cast<double>(t.size()));
    if (host) host->ShowMessage(t);
  }

  void ScanModels() {
    model_files.clear();
    variants.clear();
    current = -1;
    model.batches.clear();
    models.ClearModel();
    sprites.Clear();
    ClearMap();
    StopMapAudio();
    clips.clear();
    map_files.clear();
    map_current = -1;
    sod_lower.clear();
    spr = SprLibrary();
    odf_lib.reset();
    odf_source.reset();
    odf_zfs.reset();
    if (!source) return;
    {
      std::vector<uint8_t> zb;
      auto z = std::make_shared<ZfsArchive>();
      std::string zerr;
      if (source->ReadFile("assets.zfs", &zb) && z->Open(std::move(zb), &zerr)) {
        odf_zfs = z;
        odf_source.reset(new ZfsSource(z));
        odf_lib.reset(new OdfLibrary([this](const std::string& n, std::vector<uint8_t>* o) { return odf_source->ReadFile(n, o); }));
      }
    }
    for (const std::string& n : source->ListDir("Sprites")) {
      const std::string l = ToLowerAscii(n);
      std::vector<uint8_t> b;
      if (l.size() > 4 && l.compare(l.size() - 4, 4, ".spr") == 0 && source->ReadFile("Sprites/" + n, &b)) {
        spr.AddText(std::string(b.begin(), b.end()));
      }
    }
    LoadHud();
    for (const std::string& n : source->ListDir("SOD")) {
      const std::string l = ToLowerAscii(n);
      if (l.size() > 4 && l.compare(l.size() - 4, 4, ".sod") == 0) {
        model_files.push_back(n);
        sod_lower[l.substr(0, l.size() - 4)] = n;
      }
    }
    std::sort(model_files.begin(), model_files.end(),
              [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
    if (model_files.empty()) {
      Message("Im gewaehlten Ordner fehlt SOD/. Bitte den Spielordner waehlen (Ordner mit SOD und Textures).");
    }
  }

  // Laedt model_files[index]; bei Fehlern wird in Richtung dir weitergesprungen.
  void LoadModel(int index, int dir) {
    const int n = static_cast<int>(model_files.size());
    for (int tries = 0; tries < n; ++tries) {
      index = ((index % n) + n) % n;
      const std::string& file = model_files[static_cast<size_t>(index)];
      std::vector<uint8_t> bytes;
      std::string err;
      if (!source->ReadFile("SOD/" + file, &bytes)) {
        Message("Nicht lesbar: " + file);
      } else if (!ParseSod(bytes.data(), bytes.size(), &sod, &err)) {
        Message(file + ": uebersprungen (" + err + ")");
      } else {
        model_name = file.substr(0, file.size() - 4);
        model_number = index + 1;
        variants = ListModelVariants(sod);
        variant = 0;
        current = index;
        ShowVariant();
        return;
      }
      index += dir == 0 ? 1 : dir;
    }
    Message("Kein Modell konnte gelesen werden.");
  }

  // Baut die aktuelle Ansicht des geladenen Modells und laedt sie auf die GPU.
  void ShowVariant() {
    const ModelVariant& v = variants[static_cast<size_t>(variant)];
    BuildModel(sod, model_name, &model, v.options);
    const ModelLoadStats st = models.SetModel(model, *source);
    const int lights = sprites.SetNodes(model, spr, *source);
    for (int k = 0; k < 3; ++k) center[k] = 0.5f * (model.bounds_min[k] + model.bounds_max[k]);
    const float dx = model.bounds_max[0] - model.bounds_min[0], dy = model.bounds_max[1] - model.bounds_min[1],
                dz = model.bounds_max[2] - model.bounds_min[2];
    radius = std::max(0.001f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
    zoom = 1.0f;
    idle = 0.0f;
    BuildHardpointMarkers();  // braucht radius
    SetupFire();
    const int ver = static_cast<int>(sod.version * 10 + 0.5f);
    std::string head = model_name + " (" + std::to_string(model_number) + "/" + std::to_string(model_files.size()) + ", v" +
            std::to_string(ver / 10) + "." + std::to_string(ver % 10) + ")  Ansicht " + std::to_string(variant + 1) + "/" +
            std::to_string(variants.size()) + ": " + v.name + "  " + std::to_string(model.batches.size()) + " Teile, " +
            std::to_string(model.triangle_count) + " Dreiecke, " + std::to_string(lights) + " Lichter, Texturen " + std::to_string(st.textures_ok) + "/" +
            std::to_string(st.textures_ok + st.textures_missing);
    const bool first = !hint_done;
    if (first) head += "\nSteuerung: Ziehen dreht, 2 Finger zoomen. Tasten unten: Modell, Ansicht, Karte, Daten (I), Hardpt. (H), Feuer (F), Ton (S), Ordner, Farbe (T), Alpha (A).";
    hint_done = true;
    Message(head);
    if (first) overlay_until += 6.0;  // Steuerungshinweis etwas laenger stehen lassen
  }
  bool hint_done = false;

  // Farbige Hardpoint-Marker: je Waffenplatz der ODF eine Farbe, Ziel-Teilsysteme weiss, sonstige grau.
  void BuildHardpointMarkers() {
    markers.Clear();
    hardpoint_legend.clear();
    if (model.hardpoints.empty()) return;
    static const struct { const char* name; float rgb[3]; } kPalette[] = {{"rot", {1.0f, 0.2f, 0.2f}},   {"gruen", {0.2f, 1.0f, 0.3f}},
                                                                         {"blau", {0.3f, 0.5f, 1.0f}},  {"gelb", {1.0f, 0.9f, 0.2f}},
                                                                         {"magenta", {1.0f, 0.3f, 1.0f}}, {"orange", {1.0f, 0.6f, 0.1f}}};
    std::map<std::string, int> slot_of;  // Hardpoint -> Waffenplatz
    std::map<std::string, bool> target_of;
    UnitDef u;
    bool have_unit = odf_lib && LoadUnitDef(*odf_lib, ToLowerAscii(model_name), &u);
    if (have_unit) {
      for (size_t i = 0; i < u.weapons.size(); ++i) {
        for (const std::string& h : u.weapons[i].hardpoints) slot_of[ToLowerAscii(h)] = static_cast<int>(i);
      }
      for (const SubsystemDef* sd : {&u.engines, &u.life_support, &u.weapons_sys, &u.shield_generator, &u.sensors, &u.hull}) {
        for (const std::string& h : sd->target_hardpoints) target_of[ToLowerAscii(h)] = true;
      }
    }
    std::vector<MarkerRenderer::Marker> list;
    for (const ModelHardpoint& hp : model.hardpoints) {
      MarkerRenderer::Marker m;
      for (int k = 0; k < 3; ++k) {
        m.pos[k] = hp.pos[k];
        m.dir[k] = hp.dir[k];
      }
      const float* c = nullptr;
      static const float kWhite[3] = {1, 1, 1}, kGrey[3] = {0.5f, 0.5f, 0.5f};
      auto it = slot_of.find(hp.name);
      if (it != slot_of.end()) c = kPalette[it->second % 6].rgb;
      else if (target_of.count(hp.name)) c = kWhite;
      else c = kGrey;
      for (int k = 0; k < 3; ++k) m.color[k] = c[k];
      list.push_back(m);
    }
    markers.Set(list, radius * 0.08f);
    hardpoint_legend = "Hardpoints (" + std::to_string(list.size()) + "):";
    if (have_unit) {
      for (size_t i = 0; i < u.weapons.size(); ++i) {
        std::string wn = u.weapons[i].odf;
        WeaponDef wd;
        if (LoadWeaponDef(*odf_lib, wn, &wd) && !wd.name.empty()) wn = wd.name;
        hardpoint_legend += std::string(" ") + kPalette[i % 6].name + "=" + wn + ";";
      }
      hardpoint_legend += " weiss=Ziele der Teilsysteme; grau=ohne Zuordnung";
    } else {
      hardpoint_legend += " keine ODF, alle grau";
    }
  }

  void SetupFire() {
    const bool was = fire.firing();
    fire.Clear();
    UnitDef u;
    if (odf_lib && LoadUnitDef(*odf_lib, ToLowerAscii(model_name), &u)) fire.Setup(model, u, *odf_lib, spr, radius);
    fire.SetFiring(was);
  }

  void ToggleFire() {
    if (fire.active_slots() == 0) {
      Message(odf_lib ? "Dieses Modell hat keine feuernden Waffen." : "assets.zfs fehlt: keine Waffendaten.");
      return;
    }
    fire.SetFiring(!fire.firing());
    Message(fire.firing() ? fire.Describe() : "Feuer aus");
  }

  void ToggleHardpoints() {
    show_hardpoints = !show_hardpoints;
    if (show_hardpoints) Message(hardpoint_legend.empty() ? "Dieses Modell hat keine Hardpoints." : hardpoint_legend);
    else Message("Hardpoints aus");
  }

  void CycleAlpha() {
    alpha_mode = (alpha_mode + 1) % kAlphaModeCount;
    models.SetAlphaMode(alpha_mode);
    Message(std::string("Alpha: ") + kAlphaModes[alpha_mode]);
  }

  void CycleTeam() {
    team_color = (team_color + 1) % static_cast<int>(sizeof(kTeamColors) / sizeof(kTeamColors[0]));
    const float* c = kTeamColors[team_color].rgb;
    models.SetTeamColor(c[0], c[1], c[2]);
    Message(std::string("Teamfarbe: ") + kTeamColors[team_color].name);
  }

  void StepVariant(int delta) {
    if (!gl_ready || !source || model_files.empty() || variants.size() < 2) return;
    const int n = static_cast<int>(variants.size());
    variant = ((variant + delta) % n + n) % n;
    ShowVariant();
  }

  // --- Skirmish-Einstellung ---
  struct SkSlot { int kind = 0; int race = 0; int team = 1; };  // kind: 0 geschlossen, 1 Mensch, 2 Computer
  std::vector<std::string> sk_maps;    // Dateistamm; Eintrag 0 = leere Karte (nur zum Ausprobieren)
  std::vector<int> sk_players;         // Startplaetze je Karte 
  int sk_sel = 0, sk_first = 0, sk_res = 1;
  SkSlot sk_slot[8];
  std::vector<std::pair<int, int>> sk_starts;  // Startfelder der gewaehlten Karte
  static constexpr const char* kSkRaces[4] = {"F\xC3\xB6" "deration", "Klingonen", "Romulaner", "Borg"};
  static constexpr const char* kSkRes[3] = {"Wenig", "Normal", "Viel"};

  void SkLoadMapInfo() {
    sk_starts.clear();
    if (sk_sel < 0 || !source) return;
    std::vector<uint8_t> b;
    if (!source->ReadFile("bzn/" + sk_maps[static_cast<size_t>(sk_sel)] + ".mdf", &b)) return;
    std::istringstream in(std::string(b.begin(), b.end()));
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream ls(line);
      std::string k, eq;
      int x = 0, z = 0;
      ls >> k >> eq >> x >> z;
      if (k.size() > 5 && k.compare(0, 5, "Start") == 0 && ls) sk_starts.push_back({x, z});
    }
  }

  void SkEnter() {
    EnsureMapFiles();
    if (sk_maps.empty()) {
      std::vector<std::string> names;
      if (source)
        for (const std::string& nm : source->ListDir("bzn")) {
          const std::string l = ToLowerAscii(nm);
          if (l.size() > 4 && l.compare(l.size() - 4, 4, ".mdf") == 0) names.push_back(nm.substr(0, nm.size() - 4));
        }
      std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
      for (const std::string& n : names) {
        bool has_bzn = false;
        for (const std::string& f : map_files)
          if (ToLowerAscii(f) == ToLowerAscii(n) + ".bzn") has_bzn = true;
        if (!has_bzn) continue;
        sk_maps.push_back(n);
        sk_players.push_back(0);
      }
      sk_sel = sk_maps.empty() ? -1 : 0;
      for (int i = 0; i < 8; ++i) {
        sk_slot[i].kind = i == 0 ? 1 : (i == 1 ? 2 : 0);
        sk_slot[i].race = i % 4;
        sk_slot[i].team = i + 1;
      }
    }
    SkLoadMapInfo();
    shell_screen = ShellScreen::Skirmish;
  }

  int SkSlots() const { return sk_sel < 0 ? 8 : std::min<int>(8, static_cast<int>(sk_starts.size())); }

  SkirmishView SkView() const {
    SkirmishView v;
    const int n = SkSlots();
    for (int i = 0; i < 8; ++i) {
      SkirmishSlotView& sv = v.slot[i];
      if (i >= n) continue;
      const SkSlot& sl = sk_slot[i];
      sv.state = 1;
      sv.name = i == 0 ? "Spieler" : (sl.kind == 2 ? "Computer" : "Geschlossen");
      sv.race = sl.kind == 0 ? std::string("-") : std::string(kSkRaces[sl.race]);
      sv.team = "Team " + std::to_string(sl.team);
    }
    const int vis = 12;
    v.can_up = sk_first > 0;
    v.can_down = sk_first + vis < static_cast<int>(sk_maps.size());
    for (int k = 0; k < vis && sk_first + k < static_cast<int>(sk_maps.size()); ++k) v.maps.push_back(sk_maps[static_cast<size_t>(sk_first + k)]);
    v.selected = sk_sel - sk_first >= 0 && sk_sel - sk_first < vis ? sk_sel - sk_first : -1;
    v.title = sk_sel < 0 ? std::string("Keine Karten") : sk_maps[static_cast<size_t>(sk_sel)] + " - " + std::to_string(n) + " Spieler";
    if (sk_sel >= 0) v.preview = "bzn/" + sk_maps[static_cast<size_t>(sk_sel)] + ".bmp";
    v.res_label = std::string("Ressourcen: ") + kSkRes[sk_res];
    if (sk_sel < 0) {
      v.info = {"Im Spielordner fehlen Karten", "(bzn/*.bzn mit .mdf)."};
    } else {
      v.info = {"Karte: " + sk_maps[static_cast<size_t>(sk_sel)], "Startplaetze: " + std::to_string(n), "",
                "Jeder aktive Platz startet mit Basis", "und einem Bauschiff (wie im Original).", "",
                "Computer-Gegner stehen noch (keine KI).", "Ressourcen: noch ohne Wirkung."};
    }
    return v;
  }

  void SkStart() {
    if (!source) return;
    if (sk_sel < 0) return;
    int idx = -1;
    for (size_t i = 0; i < map_files.size(); ++i)
      if (ToLowerAscii(map_files[i]) == ToLowerAscii(sk_maps[static_cast<size_t>(sk_sel)]) + ".bzn") idx = static_cast<int>(i);
    if (idx < 0) return;
    LoadMap(idx);
    if (!map_loaded) return;
    // Karte: nur Neutrales bleibt Kartenobjekt; angelegte Spielerobjekte sofort setzen
    spawned.clear();
    selected = -1;
    static const char kRace[4] = {'f', 'k', 'r', 'b'};
    const int n = SkSlots();
    for (int i = 0; i < n; ++i) {
      const SkSlot& sl = sk_slot[i];
      if (sl.kind == 0) continue;
      const std::string p(1, kRace[sl.race]);
      const float cx = static_cast<float>(sk_starts[static_cast<size_t>(i)].first) * 50.0f + 25.0f;
      const float cz = static_cast<float>(sk_starts[static_cast<size_t>(i)].second) * 50.0f + 25.0f;
      const float base[3] = {cx, 0, cz};
      SpawnAt(p + "base", sl.team, base, false, false);
      // Originalstart (aus den Vorlagen in 4hub.bzn u. a.): je Spieler eine Basis und ein Bauschiff, Bauschiff etwa (+176, -151) neben der Basis
      const float cpos[3] = {cx + 176.0f, 0, cz - 151.0f};
      SpawnAt(p + "const", sl.team, cpos, false, false);
      if (i == 0) {
        hud_race = sl.race;
        LoadHud();
        map_target[0] = cx; map_target[1] = 0; map_target[2] = cz;
        map_dist = 2200.0f;
        zoom = 1.0f;
      }
    }
    in_game = true;
    Message("Skirmish: " + sk_maps[static_cast<size_t>(sk_sel)]);
  }

  void ShellAction(const std::string& id) {
    if (id == "single") shell_screen = ShellScreen::Single;
    else if (id == "multi") shell_screen = ShellScreen::Multi;
    else if (id == "mainmenu") shell_screen = ShellScreen::Main;
    else if (id == "back") shell_screen = shell_screen == ShellScreen::Skirmish ? ShellScreen::Multi : ShellScreen::Main;
    else if (id == "skirmish") SkEnter();
    else if (id == "start") SkStart();
    else if (id == "quit") {
      if (host) host->RequestQuit();
    } else if (id == "map_up") sk_first = std::max(0, sk_first - 6);
    else if (id == "map_down") sk_first = std::min(std::max(0, static_cast<int>(sk_maps.size()) - 12), sk_first + 6);
    else if (id == "res") sk_res = (sk_res + 1) % 3;
    else if (id.compare(0, 4, "map:") == 0) {
      sk_sel = sk_first + std::atoi(id.c_str() + 4);
      SkLoadMapInfo();
    } else if (id.compare(0, 10, "slot_name:") == 0) {
      const int i = std::atoi(id.c_str() + 10);
      if (i > 0 && i < 8) sk_slot[i].kind = sk_slot[i].kind == 2 ? 0 : 2;
    } else if (id.compare(0, 10, "slot_race:") == 0) {
      const int i = std::atoi(id.c_str() + 10);
      if (i >= 0 && i < 8) sk_slot[i].race = (sk_slot[i].race + 1) % 4;
    } else if (id.compare(0, 10, "slot_team:") == 0) {
      const int i = std::atoi(id.c_str() + 10);
      if (i >= 0 && i < 8) sk_slot[i].team = sk_slot[i].team % 8 + 1;
    }
  }

  void ApplyPending() {
    if (!gl_ready) return;
    if (source_dirty) {
      source_dirty = false;
      ScanModels();
      if (!model_files.empty()) {
        EnsureMapFiles();
        map_mode = true;
        zoom = 1.0f;
        ShowBackdrop();
        if (!pending_select.empty()) SpawnObject(ToLowerAscii(pending_select));
      }
      pending_select.clear();
    }
    if (has_pending_state && source && !map_files.empty()) {
      has_pending_state = false;
      std::vector<uint8_t> b;
      b.swap(pending_state);
      LoadFromBytes(b.data(), b.size());
    }
  }

  UiLayout ui;
  // Original-HUD (misc/gui_*.cfg); fehlen die Dateien, bleibt die einfache Leiste (ui).
  HudRenderer hud_gl;
  HudData hud_data;
  HudLayout hud;
  bool hud_ok = false;
  int hud_race = 0;

  void LoadHud() {
    hud_gl.ReleaseTextures();
    hud_ok = source && hud_data.Load(*source, static_cast<HudRace>(hud_race));
    if (!hud_ok) FLEET_LOGW("HUD: Konfiguration oder Sprites fehlen, einfache Leiste");
  }

  void FillCommands(HudState* st) const {
    st->commands = true;
    const bool sel = selected >= 0 && selected < static_cast<int>(spawned.size());
    st->control.assign(12, HudCmd());
    st->rail.assign(6, HudCmd());
    if (!sel) return;
    const Spawned& sp = spawned[static_cast<size_t>(selected)];
    auto cmd = [](UiAction a, const char* spr, const char* label, bool enabled, bool active = false) {
      HudCmd c;
      c.action = a; c.sprite = spr; c.label = label; c.enabled = enabled; c.active = active;
      return c;
    };
    // Plaetze nach game_cfg.h (1-basiert -> Index = Platz - 1)
    const bool live = !sp.derelict && sp.team > 0;
    const bool can_fly = live && CanFly(sp);
    const bool can_fire = live && !sp.weapons.empty() && sp.hp.sys_ok(kWeapons);
    if (cmd_page == CmdPage::Main) {
      st->control[0] = cmd(UiAction::Move, "b_move", "Bewegen", can_fly, move_armed);
      st->control[1] = cmd(UiAction::Attack, "b_attack", "Angriff", can_fire, attack_armed);
      st->control[2] = cmd(UiAction::Stop, "b_stop", "Stopp", live && sp.movable);
      st->control[3] = cmd(UiAction::Repair, "b_repair", "Reparatur", false);
      st->control[4] = cmd(UiAction::Orders, "b_order", "Befehle", live);
      st->control[5] = cmd(UiAction::StandDown, "b_standdown", "Abwarten", live, sp.alert == 3);
      st->control[7] = cmd(UiAction::Transport, "b_transport", "Beamen", live && sp.hp.crew_max > 0 && sp.movable, transport_armed);
    } else {
      st->control[0] = cmd(UiAction::AlertGreen, "b_galert", "Gruen", live, sp.alert == 0);
      st->control[1] = cmd(UiAction::AlertYellow, "b_yalert", "Gelb", live, sp.alert == 1);
      st->control[2] = cmd(UiAction::AlertRed, "b_ralert", "Rot", live, sp.alert == 2);
      st->control[3] = cmd(UiAction::Patrol, "b_patrol", "Patrouille", false);
      st->control[4] = cmd(UiAction::Scout, "b_scout", "Erkunden", false);
      st->control[5] = cmd(UiAction::SearchDestroy, "b_sandd", "Suchen", false);
      st->control[6] = cmd(UiAction::Guard, "b_guard", "Wache", false);
      st->control[9] = cmd(UiAction::Delete, "b_decom", "Ausmustern", true);
      st->control[10] = cmd(UiAction::SelfDestruct, "b_gselfdes", "Selbstzerst.", live);
      st->control[11] = cmd(UiAction::Back, "b_back", "Zurueck", true);
    }
    for (size_t i = 0; i < sp.specials.size() && i < 6; ++i) {
      std::string lab = sp.specials[i].second;
      if (lab.size() > 8) lab.resize(8);
      st->rail[i] = cmd(static_cast<UiAction>(static_cast<int>(UiAction::Special0) + static_cast<int>(i)), ("b_" + sp.specials[i].first).c_str(), lab.c_str(), true);
    }
  }

  HudState BuildHudState() const {
    HudState st;
    if (map_mode) FillCommands(&st);
    st.ui = CurrentUiState();
    st.race_label = HudRaceName(static_cast<HudRace>(hud_race));
    if (map_mode && map_loaded) {
      char buf[96];
      for (const Spawned& sp : spawned) {
        if (!sp.sel || st.ships.size() >= 8) continue;
        HudShip h;
        h.name = sp.label;
        h.cls = sp.cls;
        h.wire = ToLowerAscii(sp.odf);
        if (sp.st.speed > 1.0f) { std::snprintf(buf, sizeof buf, "%.0f m/s%s", sp.st.speed, sp.st.warp ? " WARP" : ""); h.status = buf; }
        h.crew = static_cast<int>(sp.hp.crew + 0.5f);
        h.crew_max = static_cast<int>(sp.hp.crew_max + 0.5f);
        h.crew_state = sp.hp.crew_status() == 0 ? 1.0f : (sp.hp.crew_status() == 1 ? 0.5f : 0.0f);
        for (int k = 0; k < kSysCount; ++k) h.system[k] = sp.hp.sys_frac(k);
        h.officers = sp.officers;
        h.energy = sp.has_energy ? 1.0f : -1.0f;
        h.shield = sp.hp.shield_frac();
        h.hull = sp.hp.crew_frac();
        st.ships.push_back(h);
      }
      if (selected >= 0 && selected < static_cast<int>(spawned.size())) {
        const Spawned& sp = spawned[static_cast<size_t>(selected)];
        const int nsel = SelCount();
        st.info_lines.push_back(nsel > 1 ? sp.label + " + " + std::to_string(nsel - 1) : sp.label);
        std::snprintf(buf, sizeof buf, "Tempo %.0f m/s%s", sp.st.speed, sp.st.warp ? " (Warp)" : "");
        st.info_lines.push_back(buf);
        float hd = sp.st.yaw * 180.0f / kPi;
        while (hd < 0) hd += 360.0f;
        std::snprintf(buf, sizeof buf, "Kurs %.0f  Team %d", hd, sp.team);
        st.info_lines.push_back(buf);
        st.info_lines.push_back(sp.movable ? (sp.st.has_target ? "Unterwegs" : "Wartet") : "Unbeweglich");
      } else {
        st.info_lines.push_back(map_current < 0 ? std::string("Leere Karte") : (map.name.empty() ? std::string("Karte") : map.name));
        st.info_lines.push_back(std::to_string(scene.instances.size() - static_cast<size_t>(scene_hidden_count) + spawned.size()) + " Objekte");
        st.info_lines.push_back("Team " + std::to_string(spawn_team));
      }
      std::vector<std::array<float, 3>> pts;  // x, z, Team
      for (size_t ii = 0; ii < scene.instances.size(); ++ii) {
        const MapInstance& in = scene.instances[ii];
        if (in.team > 0 && !(ii < scene_hidden.size() && scene_hidden[ii])) pts.push_back({in.transform.m[12], in.transform.m[14], static_cast<float>(in.team)});
      }
      for (const Spawned& sp : spawned) pts.push_back({sp.st.pos[0], sp.st.pos[2], static_cast<float>(sp.team)});
      float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
      for (const auto& p : pts) {
        lo[0] = std::min(lo[0], p[0]); hi[0] = std::max(hi[0], p[0]);
        lo[1] = std::min(lo[1], p[1]); hi[1] = std::max(hi[1], p[1]);
      }
      if (map_current >= 0 && map.bounds[1] > map.bounds[0] && map.bounds[5] > map.bounds[4]) {
        // ganze Karte (Kopfwerte x min/max, z min/max; die Karte ist nicht immer quadratisch, z. B. 2duel 12800 x 3200); GL-z = -z
        lo[0] = map.bounds[0];
        hi[0] = map.bounds[1];
        lo[1] = -map.bounds[5];
        hi[1] = -map.bounds[4];
      }
      const float ex = std::max(1.0f, std::max(hi[0] - lo[0], hi[1] - lo[1]));
      mm_centre[0] = 0.5f * (lo[0] + hi[0]);
      mm_centre[1] = 0.5f * (lo[1] + hi[1]);
      mm_extent = ex;
      for (const auto& p : pts) {
        if (st.dots.size() >= 400) break;
        HudDot d;
        d.nx = 0.5f + (p[0] - 0.5f * (lo[0] + hi[0])) / ex * 0.9f;
        d.ny = 0.5f + (p[1] - 0.5f * (lo[1] + hi[1])) / ex * 0.9f;
        const int tm = static_cast<int>(p[2]);
        const float* c = kMapTeamColors[tm < 9 ? tm : 0];
        d.rgb[0] = c[0]; d.rgb[1] = c[1]; d.rgb[2] = c[2];
        st.dots.push_back(d);
      }
      {  // Kamera-Sichtfeld: die vier Bildecken auf die Gitterebene projiziert
        const float W = static_cast<float>(width), H = static_cast<float>(height);
        const float cs[4][2] = {{0, 0}, {W, 0}, {W, H}, {0, H}};
        const float far_t = 4.0f * map_dist / zoom;
        for (int k = 0; k < 4; ++k) {
          float o[3], d[3];
          ScreenRay(cs[k][0], cs[k][1], o, d);
          float t = d[1] < -1e-4f ? -o[1] / d[1] : far_t;
          t = std::max(0.0f, std::min(t, far_t));
          const float gx = o[0] + d[0] * t, gz = o[2] + d[2] * t;
          st.view_quad[k * 2] = 0.5f + (gx - 0.5f * (lo[0] + hi[0])) / ex * 0.9f;
          st.view_quad[k * 2 + 1] = 0.5f + (gz - 0.5f * (lo[1] + hi[1])) / ex * 0.9f;
        }
        st.has_view_quad = true;
      }
    } else if (current >= 0) {
      st.info_lines.push_back(model_name);
      st.info_lines.push_back("Modell " + std::to_string(current + 1) + "/" + std::to_string(model_files.size()));
      st.info_lines.push_back("Variante " + std::to_string(variant + 1) + "/" + std::to_string(std::max<size_t>(1, variants.size())));
      st.info_lines.push_back(std::string("Alpha: ") + kAlphaModes[alpha_mode]);
    } else {
      st.info_lines.push_back("Kein Modell");
    }
    return st;
  }

  UiState CurrentUiState() const {
    UiState st;
    st.map_mode = map_mode;
    st.hardpoints = show_hardpoints;
    st.firing = fire.firing();
    st.muted = muted;
    return st;
  }

  void ToggleMute() {
    muted = !muted;
    if (music_voice != Mixer::kNone) mixer.Set(music_voice, muted ? 0.0f : 0.45f, 0.0f);
    Message(muted ? "Ton aus" : "Ton an");
  }

  void Dispatch(UiAction a) {
    switch (a) {
      case UiAction::PrevModel: Step(-1); break;
      case UiAction::NextModel: Step(1); break;
      case UiAction::Variant: StepVariant(1); break;
      case UiAction::Info: if (map_mode) ShowSelectedInfo(); else ShowInfo(); break;
      case UiAction::Hardpoints: ToggleHardpoints(); break;
      case UiAction::Fire: ToggleFire(); break;
      case UiAction::Map: if (map_mode) OpenMenu(MenuKind::Maps); else ToggleMap(); break;
      case UiAction::Mute: ToggleMute(); break;
      case UiAction::Folder:
        if (host) host->RequestDataFolder();
        break;
      case UiAction::Team: if (map_mode) CycleSpawnTeam(); else CycleTeam(); break;
      case UiAction::Alpha: CycleAlpha(); break;
      case UiAction::PrevMap: LoadMap(map_current - 1); break;
      case UiAction::NextMap: LoadMap(map_current + 1); break;
      case UiAction::MapInfo: ShowSelectedInfo(); break;
      case UiAction::ModelView: OpenMenu(MenuKind::Maps); break;
      case UiAction::Menu: OpenMenu(MenuKind::Options); break;
      case UiAction::Move: attack_armed = transport_armed = false; move_armed = !move_armed; if (move_armed) Message("Ziel auf der Karte antippen."); break;
      case UiAction::Attack:
        attack_armed = !attack_armed;
        move_armed = transport_armed = false;
        if (attack_armed) Message("Ziel antippen.");
        break;
      case UiAction::Repair: case UiAction::Guard: case UiAction::Patrol: case UiAction::Scout: case UiAction::SearchDestroy:
        Message("Noch nicht umgesetzt."); break;
      case UiAction::Orders: cmd_page = CmdPage::Orders; break;
      case UiAction::Transport:
        transport_armed = !transport_armed;
        attack_armed = move_armed = false;
        if (transport_armed) Message("Ziel zum Beamen antippen (Reichweite 500).");
        break;
      case UiAction::StandDown:
        for (Spawned& x : spawned)
          if (x.sel) {
            x.alert = 3;
            x.target_id = 0;
            x.auto_move = 0;
          }
        Message("Abwarten: schiesst nicht von selbst.");
        break;
      case UiAction::SelfDestruct:
        for (Spawned& x : spawned)
          if (x.sel && !x.derelict) x.hp.exploded = true;
        break;
      case UiAction::Back: cmd_page = CmdPage::Main; break;
      case UiAction::AlertGreen: case UiAction::AlertYellow: case UiAction::AlertRed:
        if (SelCount() > 0) {
          const int lv = a == UiAction::AlertGreen ? 0 : (a == UiAction::AlertYellow ? 1 : 2);
          for (Spawned& x : spawned)
            if (x.sel) x.alert = lv;
          Message(lv == 0 ? "Alarmstufe Gruen (flieht bei Beschuss)" : (lv == 1 ? "Alarmstufe Gelb" : "Alarmstufe Rot"));
        }
        break;
      case UiAction::Special0: case UiAction::Special1: case UiAction::Special2: case UiAction::Special3: case UiAction::Special4: case UiAction::Special5:
        if (selected >= 0 && selected < static_cast<int>(spawned.size())) {
          const size_t i = static_cast<size_t>(static_cast<int>(a) - static_cast<int>(UiAction::Special0));
          const Spawned& sp = spawned[static_cast<size_t>(selected)];
          if (i < sp.specials.size()) Message(sp.specials[i].second + ": Spezialwaffen sind noch nicht umgesetzt.");
        }
        break;
      case UiAction::Spawn: OpenMenu(MenuKind::Spawn); break;
      case UiAction::MapMenu: OpenMenu(MenuKind::Maps); break;
      case UiAction::Focus0: case UiAction::Focus1: case UiAction::Focus2: case UiAction::Focus3:
      case UiAction::Focus4: case UiAction::Focus5: case UiAction::Focus6: case UiAction::Focus7: {
        // Kamera zum i-ten ausgewaehlten Schiff (Reihenfolge wie im Schiffsfeld)
        int want = static_cast<int>(a) - static_cast<int>(UiAction::Focus0), i = 0;
        for (const Spawned& sp : spawned) {
          if (!sp.sel) continue;
          if (i++ == want) { map_target[0] = sp.st.pos[0]; map_target[2] = sp.st.pos[2]; break; }   // nur seitlich, Hoehe bleibt
        }
        break;
      }
      case UiAction::Stop: StopSelected(); break;
      case UiAction::Delete: DeleteSelected(); break;
      case UiAction::ZoomIn: zoom = std::min(30.0f, zoom * 1.25f); break;
      case UiAction::ZoomOut: zoom = std::max(0.2f, zoom / 1.25f); break;
      case UiAction::ResetView:
        zoom = 1.0f;
        if (map_mode) { map_yaw = 0.6f; map_pitch = 0.8f; } else { yaw = 0.5f; pitch = 0.35f; view_pan[0] = view_pan[1] = 0; }
        break;
      case UiAction::Race:
        hud_race = (hud_race + 1) % kHudRaceCount;
        LoadHud();
        Message(std::string("HUD: ") + HudRaceName(static_cast<HudRace>(hud_race)));
        break;
      case UiAction::None: break;
    }
  }

  void Tap(float x, float y) {
    if (menu != MenuKind::None) {
      MenuTap(x, y);
      return;
    }
    const UiAction a = hud_ok ? HudHit(hud, x, y) : UiHit(ui, x, y);
    if (a != UiAction::None) {
      Dispatch(a);
      return;
    }
    if (hud_ok && map_mode && map_loaded && mm_extent > 0 && hud.minimap.w > 0 && hud.minimap.Contains(x, y)) {  // Minikarte: Kamera dorthin setzen
      const float nx = (x - hud.minimap.x) / hud.minimap.w, ny = (y - hud.minimap.y) / hud.minimap.h;
      map_target[0] = mm_centre[0] + (nx - 0.5f) / 0.9f * mm_extent;
      map_target[2] = mm_centre[1] + (ny - 0.5f) / 0.9f * mm_extent;
      return;
    }
    const bool covered = hud_ok ? HudCovers(hud, x, y) : y >= ui.bar_y;
    if (!covered && map_mode && map_loaded) WorldTap(x, y);
  }

  // Spielwerte aus der ODF zum aktuellen Modell (Namensgleichheit ODF = SOD, in 169 Faellen belegt).
  void ShowInfo() {
    if (current < 0) return;
    if (!odf_lib) {
      Message("assets.zfs fehlt im Spielordner: keine Spielwerte.");
      return;
    }
    UnitDef u;
    std::string err;
    if (!LoadUnitDef(*odf_lib, ToLowerAscii(model_name), &u, &err)) {
      Message(model_name + ": keine ODF mit diesem Namen (Hintergrund-/Effektmodell).");
      return;
    }
    Message(DescribeUnit(*odf_lib, u));
  }

  void Step(int delta) {
    if (model_files.empty()) return;
    LoadModel(current + delta, delta);
  }
};

Game::Game() : impl_(new Impl) {}
Game::~Game() = default;


void Game::SetHost(GameHost* host) { impl_->host = host; }

void Game::SetDataSource(std::shared_ptr<FileSource> source) {
  impl_->source = std::move(source);
  impl_->source_dirty = true;
}

bool Game::SelectModel(const std::string& name) {
  impl_->pending_select = name;
  if (impl_->gl_ready && impl_->source) {
    impl_->source_dirty = true;
    impl_->ApplyPending();
  }
  return true;
}

void Game::NextModel(int delta) { impl_->Step(delta); }

void Game::NextVariant(int delta) { impl_->StepVariant(delta); }

void Game::CycleAlphaMode() { impl_->CycleAlpha(); }

void Game::ShowInfo() { impl_->ShowInfo(); }
void Game::ToggleFire() { impl_->ToggleFire(); }
void Game::ToggleHardpoints() { impl_->ToggleHardpoints(); }
void Game::ToggleMap() { impl_->ToggleMap(); }
void Game::MixAudio(int16_t* out, int frames) { impl_->mixer.Mix(out, frames); }
int Game::audio_rate() const { return impl_->mixer.output_rate(); }
void Game::ToggleMute() { impl_->ToggleMute(); }

bool Game::SaveState(std::vector<uint8_t>* out) { return impl_->SaveToBytes(out); }
void Game::LoadStateLater(std::vector<uint8_t> bytes) {
  impl_->pending_state = std::move(bytes);
  impl_->has_pending_state = true;
  if (impl_->gl_ready && impl_->source) impl_->ApplyPending();
}

void Game::CycleTeamColor() { impl_->CycleTeam(); }

bool Game::OnSurfaceCreated() {
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  FLEET_LOGI("GL: %s | %s", version ? version : "?", renderer ? renderer : "?");
  impl_->gl_ready = impl_->triangle.Init() && impl_->models.Init() && impl_->sprites.Init() && impl_->markers.Init() && impl_->map_markers.Init() && impl_->sel_markers.Init() && impl_->grid_markers.Init() && impl_->effects.Init() && impl_->text.Init() && impl_->hud_gl.Init();
  impl_->models.SetAlphaMode(impl_->alpha_mode);
  impl_->models.SetTeamColor(kTeamColors[impl_->team_color].rgb[0], kTeamColors[impl_->team_color].rgb[1],
                             kTeamColors[impl_->team_color].rgb[2]);
  if (impl_->gl_ready && impl_->source) impl_->source_dirty = true;  // Kontextverlust: neu hochladen
  return impl_->gl_ready;
}

void Game::OnSurfaceDestroyed() {
  impl_->ClearMap();
  impl_->map_markers.Shutdown();
  impl_->sel_markers.Shutdown();
  impl_->grid_markers.Shutdown();
  impl_->models.Shutdown();
  impl_->sprites.Shutdown();
  impl_->markers.Shutdown();
  impl_->effects.Shutdown();
  impl_->text.Shutdown();
  impl_->hud_gl.Shutdown();
  impl_->triangle.Shutdown();
  impl_->gl_ready = false;
  impl_->model_files.clear();
  impl_->variants.clear();
}

void Game::OnResize(int width, int height) {
  impl_->width = width > 0 ? width : 1;
  impl_->height = height > 0 ? height : 1;
}

void Game::OnFrame(double dt) {
  Impl& s = *impl_;
  s.clock += dt;
  s.ApplyPending();
  s.ui = BuildUiLayout(s.width, s.height, s.CurrentUiState());
  int bar = std::min(static_cast<int>(s.ui.bar_height), s.height - 1);
  if (s.hud_ok) {
    s.hud = BuildHudLayout(s.hud_data, s.width, s.height, s.BuildHudState());
    bar = std::max(0, std::min(s.height - 1, s.height - static_cast<int>(s.hud.view.h)));
  }
  glViewport(0, 0, s.width, s.height);
  glClearColor(0.02f, 0.03f, 0.08f, 1.0f);
  glClearDepthf(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!s.in_game) {  // Hauptmenue
    const SkirmishView skv = s.shell_screen == ShellScreen::Skirmish ? s.SkView() : SkirmishView();
    s.shell = BuildShell(s.shell_screen, s.width, s.height, s.shell_pressed, s.hud_ok ? &s.hud_data : nullptr, &skv);
    if (s.source) s.hud_gl.Draw(s.shell.draws, *s.source, s.width, s.height);
    return;
  }
  // Das 3D-Bild fuellt den ganzen Bildschirm (auch hinter dem HUD); der Bildmittelpunkt wird in den freien Bereich ueber dem HUD
  // geschoben (Verschiebung im Bildraum: um bar/height nach oben).
  const float aspect = static_cast<float>(s.width) / static_cast<float>(s.height);
  const Mat4 centre_shift = Mat4::Translation(0.0f, s.hud_ok ? s.CentreShift() : static_cast<float>(bar) / static_cast<float>(s.height), 0.0f);

  if (s.map_mode && s.map_loaded) {
    const float dist = s.map_dist / s.zoom;
    const Mat4 rot = Mat4::RotationX(s.map_pitch) * Mat4::RotationY(s.map_yaw);
    {  // Geschosslichter (die vier naechsten an der Kamera sind nicht noetig: die ersten vier genuegen)
      ModelRenderer::SceneLighting ml = s.map_light;
      ml.pcount = 0;
      for (int k = 0; k < 3; ++k) ml.origin[k] = s.map_target[k];
      for (const Impl::Projectile& p : s.projectiles) {
        if (!p.has_light || ml.pcount >= 32) continue;
        for (int k = 0; k < 3; ++k) {
          ml.ppos[ml.pcount][k] = p.vis.pos[k];
          ml.pcol[ml.pcount][k] = p.lcol[k];
        }
        ml.prng[ml.pcount][0] = p.lstart;
        ml.prng[ml.pcount][1] = p.lrange;
        ++ml.pcount;
      }
      {  // Wolkenlichter: die dem Blickpunkt naechsten fuellen die restlichen Plaetze (Farbe = Basis + Schwankung*sin, je Kanal >= 0; 0x452500)
        std::vector<std::pair<float, const Impl::NebLight*>> cand;
        for (const Impl::NebLight& l : s.neb_lights) {
          float d2 = 0;
          for (int k = 0; k < 3; ++k) d2 += (l.pos[k] - s.map_target[k]) * (l.pos[k] - s.map_target[k]);
          cand.push_back({d2, &l});
        }
        std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& c : cand) {
          if (ml.pcount >= 32) break;
          const Impl::NebLight& l = *c.second;
          const float sv = std::sin(l.freq * static_cast<float>(l.phase));
          for (int k = 0; k < 3; ++k) {
            ml.ppos[ml.pcount][k] = l.pos[k];
            ml.pcol[ml.pcount][k] = std::max(0.0f, l.base[k] + sv * l.swing[k]);
          }
          ml.prng[ml.pcount][0] = l.start;
          ml.prng[ml.pcount][1] = l.range;
          ++ml.pcount;
        }
      }
      ModelRenderer::SetSceneLighting(ml, rot);
    }
    if (s.map_stars) {  // Sternenhimmel ganz hinten, der Kartenhintergrund ergaenzt ihn
      const float k = 60000.0f / s.map_stars_radius;
      Mat4 sc;
      sc.m[0] = sc.m[5] = sc.m[10] = k;
      const Mat4 sproj = centre_shift * Mat4::Perspective(45.0f * kPi / 180.0f, aspect, 100.0f, 200000.0f);
      s.map_stars->Draw(sproj * rot * sc * Mat4::Translation(-s.map_stars_center[0], -s.map_stars_center[1], -s.map_stars_center[2]), rot);
      glClear(GL_DEPTH_BUFFER_BIT);
    }
    if (s.map_bg) {  // Hintergrund: immer um die Kamera, ohne Verschiebung, danach Tiefe loeschen
      const float k = 60000.0f / s.map_bg_radius;
      Mat4 sc;
      sc.m[0] = sc.m[5] = sc.m[10] = k;
      const Mat4 bgproj = centre_shift * Mat4::Perspective(45.0f * kPi / 180.0f, aspect, 100.0f, 200000.0f);
      s.map_bg->Draw(bgproj * rot * sc * Mat4::Translation(-s.map_bg_center[0], -s.map_bg_center[1], -s.map_bg_center[2]), rot);
      glClear(GL_DEPTH_BUFFER_BIT);
    }
    const Mat4 view = Mat4::Translation(0, 0, -dist) * rot * Mat4::Translation(-s.map_target[0], -s.map_target[1], -s.map_target[2]);
    const Mat4 proj = centre_shift * Mat4::Perspective(45.0f * kPi / 180.0f, aspect, std::max(2.0f, dist * 0.01f), dist + 14000.0f);
    {  // Orientierungsgitter (waagerechte Ebene) in Hoehe des Hauptobjekts, vor den Modellen gezeichnet
      const float cell = 75.0f;  // etwa Groesse der Foederationsbasis (Fbase: ca. 75 Einheiten)
      const float gy = 0.0f;     // Gitterebene = Kartenebene (y = 0)
      const float ext = std::ceil(std::max(6000.0f, s.map_loaded && s.map_current >= 0 ? 3.0f * s.scene.extent : 0.0f) / cell) * cell;
      const int half = static_cast<int>(ext / cell);
      const float cx = s.map_current >= 0 ? std::round(s.scene.center[0] / cell) * cell : 0.0f;
      const float cz = s.map_current >= 0 ? std::round(s.scene.center[2] / cell) * cell : 0.0f;
      std::vector<MarkerRenderer::Marker> gm;
      for (int i = -half; i <= half; ++i) {
        MarkerRenderer::Marker m;
        const float v = i % 5 == 0 ? 0.55f : 0.28f;  // weiss, jede fuenfte Linie heller
        m.color[0] = m.color[1] = m.color[2] = v;
        m.pos[1] = gy;
        m.pos[0] = cx - ext; m.pos[2] = cz + cell * static_cast<float>(i);
        m.dir[0] = 1; m.dir[1] = 0; m.dir[2] = 0;
        gm.push_back(m);
        m.pos[0] = cx + cell * static_cast<float>(i); m.pos[2] = cz - ext;
        m.dir[0] = 0; m.dir[2] = 1;
        gm.push_back(m);
      }
      s.grid_markers.Set(gm, 2.0f * ext);
      s.grid_markers.Draw(proj * view, 1.0f);
    }
    s.StepSpawned(static_cast<float>(std::min(dt, 0.1)));
    s.StepCombat(static_cast<float>(std::min(dt, 0.1)));
    struct SymItem { float p[3]; int team; std::string wire; bool sel; };
    std::vector<SymItem> syms;
    std::vector<char> sym_inst(s.scene.instances.size(), 0);
    const Mat4 sym_pv = proj * view;
    const float sym_fy = static_cast<float>(s.height) * 0.5f / std::tan(22.5f * kPi / 180.0f);
    auto sym_small = [&](const float p[3], float rad) {  // Bildschirmdurchmesser unter der Schwelle?
      if (s.sym_mode == 0) return false;
      const float* m = sym_pv.m;
      const float cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
      return cw > 1e-3f && 2.0f * rad * sym_fy / cw < s.sym_px;
    };
    for (size_t ii = 0; ii < s.scene.instances.size(); ++ii) {
      const MapInstance& inst = s.scene.instances[ii];
      ModelRenderer* r = s.map_models[static_cast<size_t>(inst.model)].get();
      if (!r || (ii < s.scene_hidden.size() && s.scene_hidden[ii])) continue;
      if (inst.team > 0 && inst.object >= 0 && inst.object < static_cast<int>(s.map.objects.size())) {
        const float* im = inst.transform.m;
        const float isc = std::sqrt(im[0] * im[0] + im[1] * im[1] + im[2] * im[2]);
        const float ip[3] = {im[12], im[13], im[14]};
        if (sym_small(ip, s.map_radius[static_cast<size_t>(inst.model)] * isc)) {
          sym_inst[ii] = 1;
          syms.push_back({{ip[0], ip[1], ip[2]}, inst.team, ToLowerAscii(s.map.objects[static_cast<size_t>(inst.object)].odf), false});
          continue;
        }
      }
      const float* c = kMapTeamColors[inst.team >= 0 && inst.team < 9 ? inst.team : 0];
      r->SetTeamColor(c[0], c[1], c[2]);
      r->Draw(proj * view * inst.transform, rot * inst.rotation, &inst.transform);
    }
    for (size_t ii = 0; ii < s.scene.instances.size(); ++ii) {  // Sprites (Nebel, Positionslichter) nach den Modellen
      const MapInstance& inst = s.scene.instances[ii];
      const SpriteRenderer* sp = s.map_sprites[static_cast<size_t>(inst.model)].get();
      if (sp && !sym_inst[ii] && !(ii < s.scene_hidden.size() && s.scene_hidden[ii])) sp->Draw(proj, view * inst.transform, s.clock);
    }
    for (size_t si = 0; si < s.spawned.size(); ++si) {
      const Impl::Spawned& sp = s.spawned[si];
      auto it = s.spawn_models.find(sp.sod);
      if (it == s.spawn_models.end() || !it->second.r) continue;
      if (sym_small(sp.st.pos, it->second.radius * sp.scale)) {
        syms.push_back({{sp.st.pos[0], sp.st.pos[1], sp.st.pos[2]}, sp.team, ToLowerAscii(sp.odf), sp.sel});
        continue;
      }
      Mat4 m, r;
      ShipMatrices(sp.st, sp.scale, &m, &r);
      const float* c = kMapTeamColors[sp.team >= 0 && sp.team < 9 ? sp.team : 0];
      it->second.r->SetTeamColor(c[0], c[1], c[2]);
      it->second.r->Draw(proj * view * m, rot * r, &m);
      if (it->second.sp) it->second.sp->Draw(proj, view * m, s.clock);
    }
    for (const Impl::Fireball& f : s.fireballs) {
      auto it = s.spawn_models.find(f.model);
      if (it != s.spawn_models.end() && it->second.sp) it->second.sp->Draw(proj, view * Mat4::Translation(f.pos[0], f.pos[1], f.pos[2]) * AxisAngle(f.axis, kFireballSpin * f.age), f.age, f.scale);
    }
    if (s.source) {
      std::vector<float> mesh;
      for (const Impl::ShockInst& w : s.shockwaves) {
        BuildShockwaveMesh(w.def, w.sw, &mesh);
        s.effects.DrawMesh(proj, view * Mat4::Translation(w.sw.pos[0], w.sw.pos[1], w.sw.pos[2]), mesh, w.def.texture, w.def.colour, ShockwaveFade(w.def, w.sw), *s.source);
      }
    }
    if (s.source && (!s.fx.empty() || !s.projectiles.empty())) {
      std::vector<Shot> shots = s.fx;
      for (const Impl::Projectile& p : s.projectiles) shots.push_back(p.vis);
      s.effects.Draw(proj, view, shots, *s.source);
    }
    if (s.hud_ok && s.source) {  // Auswahl: Ring auf dem Boden und Eckklammern; Ziel: gruener Ring (Original-Sprites aus gui_global.spr)
      const Mat4 mvp = proj * view;
      const float fy = static_cast<float>(s.height) * 0.5f / std::tan(22.5f * kPi / 180.0f);
      const float W = static_cast<float>(s.width), H = static_cast<float>(s.height);
      const float squash = std::max(0.2f, std::sin(std::max(0.05f, s.map_pitch)));
      std::vector<HudDraw> dr;
      auto project = [&](const float p[3], float* px, float* py, float* pw) {
        const float* m = mvp.m;
        const float cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
        if (cw <= 1e-3f) return false;
        const float cx = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], cy = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
        *px = (cx / cw * 0.5f + 0.5f) * W;
        *py = (1.0f - (cy / cw * 0.5f + 0.5f)) * H;
        *pw = cw;
        return true;
      };
      auto sprite = [&](const char* name, float cx, float cy, float w, float h, float tint) {
        HudDraw d;
        float uv[4];
        bool add = false;
        if (!s.hud_data.Sprite(name, &d.tex, uv, &add)) return;
        d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[2]; d.v1 = uv[3];
        d.additive = add;
        d.x = cx - 0.5f * w; d.y = cy - 0.5f * h; d.w = w; d.h = h;
        d.rgba[0] = d.rgba[1] = d.rgba[2] = tint;
        dr.push_back(d);
      };
      for (const SymItem& sy : syms) {  // Symbolansicht: kamerazugewandtes Sprite in Teamfarbe
        float px, py, pw;
        if (!project(sy.p, &px, &py, &pw)) continue;
        const float sz = 0.0225f * H;
        const float* c = kMapTeamColors[sy.team >= 0 && sy.team < 9 ? sy.team : 0];
        auto put = [&](const std::string& name, float x, float y, float w, float h, float a) {
          HudDraw d;
          float uv[4];
          bool add = false;
          if (!s.hud_data.Sprite(name.c_str(), &d.tex, uv, &add)) return;
          d.u0 = uv[0]; d.v0 = uv[1]; d.u1 = uv[2]; d.v1 = uv[3];
          d.additive = add;
          d.x = x - 0.5f * w; d.y = y - 0.5f * h; d.w = w; d.h = h;
          d.rgba[0] = c[0]; d.rgba[1] = c[1]; d.rgba[2] = c[2]; d.rgba[3] = a;
          dr.push_back(d);
        };
        if (s.sym_mode == 2) for (int i = 1; i <= 5; ++i) put(sy.wire + "w" + std::to_string(i), px, py, sz * 1.4f, sz * 1.4f, 1.0f);
        else {  // Fraktionslogo nach dem ersten ODF-Buchstaben (f/k/r/b), sonst HUD-Rasse
          static const char* kIcon[4] = {"federation_icon", "klingon_icon", "romulan_icon", "borg_icon"};
          const char ch = sy.wire.empty() ? 'f' : sy.wire[0];
          const int ri = ch == 'k' ? 1 : ch == 'r' ? 2 : ch == 'b' ? 3 : ch == 'f' ? 0 : (s.hud_race & 3);
          put(kIcon[ri], px, py, sz * 1.46f, sz, 1.0f);
        }
        if (sy.sel) {  // Auswahlecken (GCselect, 3x3-Rahmen)
          const float h = 0.5f * sz * 1.5f, q = 22.0f * std::max(1.0f, H / 700.0f);
          put("selection.00", px - h, py - h, q, q, 1.0f);
          put("selection.20", px + h, py - h, q, q, 1.0f);
          put("selection.02", px - h, py + h, q, q, 1.0f);
          put("selection.22", px + h, py + h, q, q, 1.0f);
        }
      }
      for (const Impl::Spawned& sp : s.spawned) {  // Schild-/Huellenbalken ueber beschaedigten oder ausgewaehlten Objekten
        if (sp.derelict || (!sp.sel && sp.hp.shield >= sp.hp.shield_max && sp.hp.crew_frac() >= 1.0f && !sp.hp.damaged())) continue;
        auto it = s.spawn_models.find(sp.sod);
        const float rw = std::max(5.0f, (it != s.spawn_models.end() ? it->second.radius : 20.0f) * sp.scale);
        const float top[3] = {sp.st.pos[0], sp.st.pos[1] + rw, sp.st.pos[2]};
        float px, py, pw;
        if (!project(top, &px, &py, &pw)) continue;
        const float bw = std::max(30.0f, rw * fy / pw * 1.6f), bh = std::max(3.0f, bw * 0.07f);
        auto bar = [&](float y, float frac, float r, float g, float b) {
          HudDraw bg;
          bg.x = px - 0.5f * bw; bg.y = y; bg.w = bw; bg.h = bh;
          bg.rgba[0] = bg.rgba[1] = bg.rgba[2] = 0.0f; bg.rgba[3] = 0.6f;
          dr.push_back(bg);
          HudDraw f = bg;
          f.w = bw * std::max(0.0f, std::min(1.0f, frac));
          f.rgba[0] = r; f.rgba[1] = g; f.rgba[2] = b; f.rgba[3] = 0.9f;
          dr.push_back(f);
        };
        bar(py - bh * 2.5f, sp.hp.shield_frac(), 0.3f, 0.6f, 1.0f);
        bar(py - bh * 1.2f + bh, sp.hp.crew_frac(), 0.3f, 0.9f, 0.3f);
      }
      for (size_t i = 0; i < s.spawned.size(); ++i) {
        const Impl::Spawned& sp = s.spawned[i];
        if (!sp.sel) continue;
        const float tint = static_cast<int>(i) == s.selected ? 1.0f : 0.6f;
        auto it = s.spawn_models.find(sp.sod);
        const float rw = std::max(5.0f, (it != s.spawn_models.end() ? it->second.radius : 20.0f) * sp.scale);
        const float foot[3] = {sp.st.pos[0], 0.0f, sp.st.pos[2]};  // Ring liegt auf der Gitterebene unter dem Objekt
        float px, py, pw;
        if (project(foot, &px, &py, &pw)) {
          const float r = std::max(18.0f, rw * fy / pw);  // Pixel
          sprite("selectionCircle", px, py, 2.4f * r, 2.4f * r * squash, tint);
          if (sp.st.has_target) {
            const float tg[3] = {sp.st.target[0], 0.0f, sp.st.target[2]};
            float tx, ty, tw;
            if (project(tg, &tx, &ty, &tw)) {
              const float rt = std::max(12.0f, rw * fy / tw);
              sprite("gridFXSprite", tx, ty, 2.0f * rt, 2.0f * rt * squash, tint);
            }
          }
        }
      }
      if (!dr.empty()) s.hud_gl.Draw(dr, *s.source, s.width, s.height);
    }
    s.UpdateAmbients(rot);
    s.map_markers.Draw(proj * view, 3.0f * static_cast<float>(std::max(1, s.height / 500)));
  } else if (s.models.has_model()) {
    s.idle += static_cast<float>(dt);
    if (s.touches.empty() && s.idle > 2.0f) s.yaw += static_cast<float>(dt) * 0.3f;  // langsame Drehung im Leerlauf
    const float dist = s.radius * 2.6f / s.zoom;
    const Mat4 rot = Mat4::RotationX(s.pitch) * Mat4::RotationY(s.yaw);
    const Mat4 model = rot * Mat4::Translation(-s.center[0], -s.center[1], -s.center[2]);
    const Mat4 view = Mat4::Translation(s.view_pan[0], s.view_pan[1], -dist);
    const Mat4 proj = centre_shift * Mat4::Perspective(45.0f * kPi / 180.0f, aspect, std::max(0.01f, dist - s.radius * 1.5f), dist + s.radius * 2.0f);
    s.models.Draw(proj * view * model, rot);
    s.sprites.Draw(proj, view * model, s.clock);
    s.fire.Update(static_cast<float>(std::min(dt, 0.1)));
    for (const std::string& snd : s.fire.TakeSounds()) s.PlayEffect(snd, 0.5f);
    if (s.source) s.effects.Draw(proj, view * model, s.fire.shots(), *s.source);
    if (s.show_hardpoints) s.markers.Draw(proj * view * model, 6.0f * static_cast<float>(std::max(1, s.height / 500)));
  } else {
    s.angle += static_cast<float>(dt) * 0.8f;
    const Mat4 mvp = centre_shift * Mat4::Ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f) * Mat4::RotationZ(s.angle);
    static const float kWhite[3] = {1, 1, 1};
    s.triangle.Draw(mvp, kWhite);
  }
  glViewport(0, 0, s.width, s.height);
  if (s.menu != Impl::MenuKind::None) {
    // HUD ausgeblendet, solange ein Menue offen ist: die 3D-Szene bleibt sichtbar
  } else if (s.hud_ok && s.source) {  // Original-HUD
    s.hud_gl.Draw(s.hud.draws, *s.source, s.width, s.height);
    static const float kNoBg[4] = {0, 0, 0, 0};
    for (const HudText& t : s.hud.texts) s.text.DrawStyled(t.text, t.x, t.y, t.scale, s.width, s.height, 0, false, kNoBg, t.rgb);
  } else {  // einfache Bedienleiste
    const float bar_bg[4] = {0.04f, 0.06f, 0.11f, 0.96f};
    s.text.FillRect(0, s.ui.bar_y, static_cast<float>(s.width), s.ui.bar_height, bar_bg, s.width, s.height);
    static const float kWhite[3] = {1, 1, 1}, kNone[4] = {0, 0, 0, 0};
    for (const UiButton& b : s.ui.buttons) {
      const float col_idle[4] = {0.13f, 0.19f, 0.31f, 1.0f}, col_on[4] = {0.12f, 0.45f, 0.28f, 1.0f};
      s.text.FillRect(b.rect.x, b.rect.y, b.rect.w, b.rect.h, b.active ? col_on : col_idle, s.width, s.height);
      float tw = 0, th = 0;
      TextRenderer::Measure(b.label, s.ui.text_scale, 0, &tw, &th);
      s.text.DrawStyled(b.label, b.rect.x + 0.5f * (b.rect.w - tw), b.rect.y + 0.5f * (b.rect.h - th), s.ui.text_scale, s.width, s.height, 0, false, kNone, kWhite);
    }
  }
  if (s.menu != Impl::MenuKind::None) {
    const Impl::MenuGeo g = s.MenuGeometry();
    auto solid = [](std::vector<HudDraw>* v, float x, float y, float w, float h, float r, float gr, float b, float a) {
      HudDraw d;
      d.x = x; d.y = y; d.w = w; d.h = h;
      d.rgba[0] = r; d.rgba[1] = gr; d.rgba[2] = b; d.rgba[3] = a;
      v->push_back(d);
    };
    if (s.hud_ok && s.source && s.hud_data.has_font()) {
      std::vector<HudDraw> dr;
      static const float kWhite[3] = {1, 1, 1}, kInk[3] = {0.06f, 0.04f, 0.03f}, kOff[3] = {0.38f, 0.30f, 0.24f};
      // Titelleiste (grau-gruen wie im Original) mit weisser Schrift
      solid(&dr, g.tx, g.ty, g.tw, g.th, 0.42f, 0.52f, 0.50f, 0.92f);
      {
        const float hp = 0.62f * g.th;
        const HudTextBlock tb = LayoutHudText(s.hud_data, s.MenuTitle(), g.tx + 0.5f * hp, g.ty + 0.5f * (g.th - hp), hp, 1e6f, kWhite, nullptr);
        dr.insert(dr.end(), tb.draws.begin(), tb.draws.end());
      }
      // Rahmen: aussen grau-gruen, innen schwarz
      solid(&dr, g.px, g.py, g.pw, g.ph, 0.42f, 0.52f, 0.50f, 0.95f);
      const float ins = 0.1f * g.pad;
      solid(&dr, g.px + ins, g.py + ins, g.pw - 2 * ins, g.ph - 2 * ins, 0.02f, 0.02f, 0.03f, 0.93f);
      for (int i = 0; i < g.vis; ++i) {
        const size_t idx = static_cast<size_t>(s.menu_scroll + i);
        if (idx >= s.menu_rows.size()) break;
        const bool off = s.menu_rows[idx].key.empty();
        const bool hot = static_cast<int>(idx) == s.menu_hot;
        const float by = g.y0 + g.row_h * static_cast<float>(i);
        const float r = off ? 0.55f : (hot ? 1.0f : 0.91f), gr = off ? 0.43f : (hot ? 0.88f : 0.69f), b = off ? 0.34f : (hot ? 0.55f : 0.53f);
        solid(&dr, g.x0, by, g.bw, g.btn_h, r, gr, b, 1.0f);
        solid(&dr, g.x0, by, g.bw, 0.07f * g.btn_h, 1.0f, 1.0f, 1.0f, 0.35f);                       // Lichtkante oben
        solid(&dr, g.x0, by + 0.93f * g.btn_h, g.bw, 0.07f * g.btn_h, 0.0f, 0.0f, 0.0f, 0.35f);      // Schatten unten
        float hp = g.h_px;
        HudTextBlock tb = LayoutHudText(s.hud_data, s.menu_rows[idx].label, 0, 0, hp, 1e6f, kInk, nullptr);
        if (tb.w > 0.94f * g.bw) {
          hp *= 0.94f * g.bw / tb.w;
          tb = LayoutHudText(s.hud_data, s.menu_rows[idx].label, 0, 0, hp, 1e6f, kInk, nullptr);
        }
        tb = LayoutHudText(s.hud_data, s.menu_rows[idx].label, g.x0 + 0.5f * (g.bw - tb.w), by + 0.5f * (g.btn_h - hp), hp, 1e6f, off ? kOff : kInk, nullptr);
        dr.insert(dr.end(), tb.draws.begin(), tb.draws.end());
      }
      if (s.menu_rows.size() > static_cast<size_t>(g.vis)) {  // Rollbalken am Rahmenrand
        const float frac = static_cast<float>(g.vis) / static_cast<float>(s.menu_rows.size());
        const float bw = std::max(4.0f, 0.12f * g.pad), th = (g.ph - 2 * ins) * frac;
        const float ty = g.py + ins + (g.ph - 2 * ins) * static_cast<float>(s.menu_scroll) / static_cast<float>(s.menu_rows.size());
        solid(&dr, g.px + g.pw - ins - bw, ty, bw, th, 0.9f, 0.9f, 0.9f, 0.7f);
      }
      s.hud_gl.Draw(dr, *s.source, s.width, s.height);
    } else {
      for (int i = 0; i < g.vis; ++i) {
        const size_t idx = static_cast<size_t>(s.menu_scroll + i);
        if (idx >= s.menu_rows.size()) break;
        s.text.Draw(s.menu_rows[idx].label, g.x0, g.y0 + g.row_h * static_cast<float>(i), std::max(1, UiMessageScale(s.width, s.height) / 2), s.width, s.height, g.bw);
      }
    }
  }
  if (!s.overlay.empty() && s.clock < s.overlay_until && s.menu == Impl::MenuKind::None) {
    const int scale = UiMessageScale(s.width, s.height);
    const float margin = 8.0f * static_cast<float>(scale);
    bool done = false;
    if (s.hud_ok && s.source && s.hud_data.has_font()) {  // Originalschrift
      static const float kWhite[3] = {1, 1, 1}, kPanel[4] = {0.0f, 0.0f, 0.0f, 0.62f};
      const float h_px = 0.8f * static_cast<float>(scale * 22);
      const HudTextBlock tb = LayoutHudText(s.hud_data, s.overlay, margin * 1.5f, margin * 1.5f + s.hud.view.y, h_px, static_cast<float>(s.width) - 3.0f * margin, kWhite, kPanel);
      if (!tb.draws.empty()) {
        s.hud_gl.Draw(tb.draws, *s.source, s.width, s.height);
        done = true;
      }
    }
    if (!done) s.text.Draw(s.overlay, margin, margin, scale, s.width, s.height, static_cast<float>(s.width) - 2.0f * margin);
  }
  ++s.frames;
}

void Game::ShowOverlay(const std::string& text) { impl_->Message(text); }

void Game::OnPointer(const PointerEvent& e) {
  Impl& s = *impl_;
  s.idle = 0.0f;
  if (!s.in_game) {
    const std::string hit = ShellHit(s.shell, e.x, e.y);
    if (e.action == PointerEvent::Action::Down) s.shell_pressed = hit;
    else if (e.action == PointerEvent::Action::Move) {
      if (hit != s.shell_pressed) s.shell_pressed.clear();
    } else {
      const std::string id = s.shell_pressed;
      s.shell_pressed.clear();
      if (!id.empty() && id == hit) s.ShellAction(id);
    }
    return;
  }
  auto it = s.touches.find(e.id);
  if (e.action == PointerEvent::Action::Down) {
    Touch t;
    t.x = t.start_x = e.x;
    t.y = t.start_y = e.y;
    t.start_time = s.clock;
    t.ui = s.hud_ok ? HudCovers(s.hud, e.x, e.y) : UiHit(s.ui, e.x, e.y) != UiAction::None;
    s.touches[e.id] = t;
    if (s.menu != Impl::MenuKind::None) s.menu_hot = s.MenuRowAt(s.MenuGeometry(), e.x, e.y);
    if (s.touches.size() == 2) {
      auto a = s.touches.begin(), b = std::next(a);
      s.pinch_start_dist = std::hypot(a->second.x - b->second.x, a->second.y - b->second.y);
      s.pinch_start_zoom = s.zoom;
      s.g_cx = 0.5f * (a->second.x + b->second.x);
      s.g_cy = 0.5f * (a->second.y + b->second.y);
      s.g_ang = std::atan2(b->second.y - a->second.y, b->second.x - a->second.x);
      s.g_dist = s.pinch_start_dist;
      s.g_have = true;
    }
    return;
  }
  if (it == s.touches.end()) return;
  Touch& t = it->second;
  if (e.action == PointerEvent::Action::Move) {
    const float dx = e.x - t.x, dy = e.y - t.y;
    t.x = e.x;
    t.y = e.y;
    if (std::hypot(e.x - t.start_x, e.y - t.start_y) > kTapMaxMovePixels) t.moved = true;
    if (s.menu != Impl::MenuKind::None) {
      if (t.moved) s.menu_hot = -1;
      if (s.touches.size() == 1) s.MenuDrag(dy);
      return;
    }
    const float k = 3.0f / static_cast<float>(std::max(1, std::min(s.width, s.height)));
    if (s.map_mode && s.map_loaded) {
      if (s.touches.size() == 1 && !t.ui) {
        s.PanMap(e.x - dx, e.y - dy, e.x, e.y);  // ein Finger: Ansicht verschieben
      } else if (s.touches.size() == 2 && s.g_have) {
        auto a = s.touches.begin(), b = std::next(a);
        const float cx = 0.5f * (a->second.x + b->second.x), cy = 0.5f * (a->second.y + b->second.y);
        const float dist = std::hypot(a->second.x - b->second.x, a->second.y - b->second.y);
        const float ang = std::atan2(b->second.y - a->second.y, b->second.x - a->second.x);
        if (dist > 20.0f && s.g_dist > 20.0f) {
          float dang = ang - s.g_ang;
          while (dang > kPi) dang -= 2.0f * kPi;
          while (dang < -kPi) dang += 2.0f * kPi;
          float pv[3] = {s.map_target[0], s.map_target[1], s.map_target[2]};
          s.GroundPoint(cx, cy, pv);
          s.zoom = std::max(0.2f, std::min(30.0f, s.zoom * dist / s.g_dist));  // Spreizen/Zusammenziehen: Zoom
          // Verdrehen: Ansicht dreht mit (Uhrzeigersinn der Finger = Szene im Uhrzeigersinn); gemeinsames Ziehen: Gier/Neigung
          s.OrbitAbout(pv, -dang + (cx - s.g_cx) * k, (cy - s.g_cy) * k);
          s.g_dist = dist;
          s.g_ang = ang;
        }
        s.g_cx = cx;
        s.g_cy = cy;
      }
      return;
    }
    // Modellansicht (keine Karte): gleiche Bedienung wie in der Karte
    const float wpp = 2.0f * (s.radius * 2.6f / s.zoom) * std::tan(22.5f * kPi / 180.0f) / static_cast<float>(std::max(1, s.height));
    if (s.touches.size() == 1 && !t.ui) {
      s.view_pan[0] += dx * wpp;
      s.view_pan[1] -= dy * wpp;
    } else if (s.touches.size() == 2 && s.g_have) {
      auto a = s.touches.begin(), b = std::next(a);
      const float cx = 0.5f * (a->second.x + b->second.x), cy = 0.5f * (a->second.y + b->second.y);
      const float dist = std::hypot(a->second.x - b->second.x, a->second.y - b->second.y);
      const float ang = std::atan2(b->second.y - a->second.y, b->second.x - a->second.x);
      if (dist > 20.0f && s.g_dist > 20.0f) {
        float dang = ang - s.g_ang;
        while (dang > kPi) dang -= 2.0f * kPi;
        while (dang < -kPi) dang += 2.0f * kPi;
        s.zoom = std::max(0.2f, std::min(30.0f, s.zoom * dist / s.g_dist));
        s.yaw += -dang + (cx - s.g_cx) * k;
        s.pitch = std::max(-1.5f, std::min(1.5f, s.pitch + (cy - s.g_cy) * k));
        s.g_dist = dist;
        s.g_ang = ang;
      }
      s.g_cx = cx;
      s.g_cy = cy;
    }
    return;
  }
  // Up
  const bool tap = !t.moved && (s.clock - t.start_time) < kTapMaxSeconds && s.touches.size() == 1;
  const float x = t.x, y = t.y;
  const bool t_moved = t.moved, t_ui = t.ui, t_alone = s.touches.size() == 1;
  const double t_dur = s.clock - t.start_time;
  s.touches.erase(it);
  if (s.touches.size() < 2) s.g_have = false;
  s.menu_hot = -1;
  if (tap) s.Tap(x, y);
  else if (!t_moved && !t_ui && t_alone && t_dur >= kTapMaxSeconds && t_dur < 3.0 && s.map_mode && s.map_loaded && s.menu == Impl::MenuKind::None) s.WorldLongPress(x, y);
}

void Game::OnScroll(float amount) {
  impl_->zoom = std::max(0.2f, std::min(30.0f, impl_->zoom * std::pow(1.1f, amount)));
  impl_->idle = 0.0f;
}

int Game::frames_rendered() const { return impl_->frames; }

}  // namespace fleet
