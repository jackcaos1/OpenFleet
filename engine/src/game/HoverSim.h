#pragma once
// Getreue Portierung der Flugphysik aus Armada.exe (HoverCraft-Simulate @0x459440, Klassen-Ableitung @0x455440,
// Instanz-Kopie @0x458750, Matrix-Aufbau @0x55d080, Matrix->Euler @0x458ea0). Siehe docs/formats/movement_re.md.
// Koordinaten wie im Original (y oben, z nach Norden); Umrechnung in die GL-Welt macht der Aufrufer.
#include <cstdint>

namespace fleet {

// Rohwerte aus battphys.odf (Namen wie in der Datei). Fehlende Werte behalten die Exe-Vorgaben (@0x455230).
struct HoverOdf {
  float impulseSpeed = 20, warpSpeed = 80;
  float velocCoeffStill = 0.1f, velocWhenCoeffHalf = 7.5f;
  float pitchRotationSpeed = 3.5f, maxPitch = 70.0f;
  float rollRotationSpeed = 5.5f, maxRoll = 30.0f;
  float turnRadius = 25.0f, maxRotationSpeed = 0.25f, rotationAcceleration = 0;
  float frontBackInertia = 0, maxReverseSpeed = 15.0f, upDownInertia = 0, maxVerticalSpeed = 10.0f;
  float leftRightInertia = 0, rotationStiffness = 1.0f;
  float shp_verticalVelocCoeff = -0.995f, shp_verticalPosCoeff = -1.649f, shp_pitchVelocCoeff = 1.9f,
        shp_pitchAngleCoeff = 6.7f;
  float sy_rollVelocCoeff = 1.22f, sy_rollAngleCoeff = 0.174f, sy_yawVelocCoeff = -0.0431f,
        sy_yawAngleCoeff = 0.7258f, sy_velocAngleCoeff = 0.1418f;
  float sp_pitchVelocCoeff = 0, sp_pitchAngleCoeff = -1.197f, sp_velocAngleCoeff = 0.06f;
  float spl_pitchVelocCoeff = -71.844f, spl_pitchAngleCoeff = -3.986f, spl_pvelocAngleCoeff = -0.049f,
        spl_verticalVelocCoeff = 1.492f, spl_lvelocAngleCoeff = -0.925f;
  float braccelControlStiffness = 4.0f;               // 0x200
  float tooCloseToTurn = 50, tooCloseForWarp = 10, tooSlowToTurn = 0, headingCloseEnoughForWarp = 30;  // Grad
};

// Abgeleitete Klassenwerte (Loader @0x455440).
struct HoverClass {
  float c458, c45c, c464, c468, c470, c478, c47c, c484, c48c, c498, c4a0, c4ac, c4b0, c4b8, c4c0, c4c8, c4d0;
  float maxFwd;            // 0x4a4
  float shp[4], sy[5], sp[3], spl[5];   // 0x4d4.. (shp_vv, shp_vp, shp_pv, shp_pa | sy_rv, sy_ra, sy_yv, sy_ya, sy_va | sp_pv, sp_pa, sp_va | spl_pv, spl_pa, spl_pva, spl_vv, spl_lva)
  float turn_radius, braccel_k, braccel_c, too_close_to_turn, too_close_for_warp, too_slow_to_turn, heading_ok_rad, warp_ratio;
  float impulse, warp;
  static HoverClass From(const HoverOdf& o);
};

struct HoverControls { float c0 = 0, c1 = 0, c2 = 0, c3 = 0; };  // c0 Lenken(Roll), c1 Pitch, c2 hoch/runter, c3 Schub

struct HoverCraftState {
  // Matrix wie im Objekt: Zeilen right(+0x68) up(+0x74) front(+0x80), Position(+0x8c)
  float right[3] = {1, 0, 0}, up[3] = {0, 1, 0}, front[3] = {0, 0, 1}, pos[3] = {0, 0, 0};
  float vel[3] = {0, 0, 0};     // +0x140
  float speed = 0;              // +0x138
  float angvel[3] = {0, 0, 0};  // +0x14c (Pitch, Yaw, Roll)
  float accel[3] = {0, 0, 0};   // +0x158
  float angacc[3] = {0, 0, 0};  // +0x164
  float pitch = 0, yaw = 0, roll = 0;  // +0x768/+0x76c/+0x770
  // Instanzwerte (Kopie der Klasse, @0x458750)
  float k6a8, k6ac, k6b4, k6b8, k6c0, k6c8, k6cc, k6d4, k6dc, k6f0, k6fc, k700, k708, k710, k718;
  float k724, k728, k72c, k730, k734, k738, k73c, k740, k744, k748, k74c, k750, k754, k758, k75c, k760, k764;
  float k4b4 = 4, k4b8 = 4, warp_ratio = 4;   // PD-Gains (+0x4b4/+0x4b8), Warp/Impuls-Verhaeltnis (+0x4ac)
  float coeff774 = 0;           // Wendigkeits-Faktor (+0x774)
  float cap_speed = 20;         // +0x4a8 (aktuelle Geschwindigkeitsgrenze)
  float scale_a = 1.0f, scale_b = 1.0e10f;  // +0x4bc / +0x4c0
  float speed_mult = 1.0f;      // Faktor aus dem Zusatzobjekt von 0x471ae0 (+0xe8), 1 = keins
  bool powered = true;          // +0x518
  bool hold = false;            // +0x4dc
  bool dead = false;            // +0x67c
  bool borg = false;            // Klasse +0x1ec
  float radius = 1.0f;          // +0x60 (Kollisionsradius)
  float prev_dir[3] = {0, 0, 0};  // +0x49c: letzte Wunschrichtung nach der Hindernisvermeidung
  int priority = 0;             // Klasse +0x1ac (hoehere Prioritaet weicht nicht aus)
  int group = -1;               // +0xc4
  void Init(const HoverClass& c);
  void SetSpeedCap(float s);    // @0x458960
  void SetFromMatrix();         // @0x458ea0: Euler aus der Matrix
  void BuildMatrix();           // @0x55d080: Matrix aus Euler (Position bleibt)
};

// HoverCraft::Simulate (dt Sekunden). Nicht abgebildet: Borg-Zweig (borgPhysics), Basisklasse Craft::Simulate @0x465cc0.
// Steuer-Methoden (Craft/HoverCraft-vtable, schreiben nur den Steuerblock). Alle clampen auf [-1,1].
float HoverSetThrottle(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, float accel, bool warp);  // vtbl 0xdc @0x458990
void HoverSetVertical(const HoverCraftState& s, HoverControls* ctl, float target_y);        // vtbl 0xe4 @0x465870
void HoverServoHeightPitch(const HoverCraftState& s, HoverControls* ctl, float target_y);   // vtbl 0xe8 @0x458aa0
void HoverServoYaw(const HoverCraftState& s, HoverControls* ctl, float target_yaw);         // vtbl 0xf0 @0x458b80
void HoverServoPitch(const HoverCraftState& s, HoverControls* ctl, float target_pitch);     // vtbl 0xf4 @0x458ca0
void HoverServoPitchLift(const HoverCraftState& s, HoverControls* ctl, float target_pitch); // vtbl 0xf8 @0x458d60
// PD-Schubregelung zum Zielpunkt (vtbl 0xe0 @0x4657b0): Beschleunigung = k*(front.dp) + c*(front.dv)
void HoverGotoPD(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, const float target[3], const float target_vel[3], bool warp);
// Richtung anfliegen (@0x42f740): |dir| (<=1) ist der Schubanteil; Kurs/Neigung per Servo. warp_scale = 1 oder Warp-Verhaeltnis.
void HoverAimDirection(const HoverClass& c, const HoverCraftState& s, HoverControls* ctl, const float dir[3], float warp_scale);
// Hindernis (Objektfelder, wie sie die Vermeidung @0x42ef70 liest)
struct HoverObstacle {
  float pos[3] = {0, 0, 0};     // +0x12c
  float vel[3] = {0, 0, 0};     // +0x140
  float speed = 0;              // +0x138
  float radius = 1;             // +0x60
  bool collidable = true;       // Flags +0xbc & 4
  bool active = true;           // +0xd8
  bool hidden = false;          // +0x1b1
  bool special = false;         // Flags +0xbc & 0x8000 (Sonderbehandlung @0x42ed20, nicht portiert -> uebersprungen)
  bool mobile = false;          // Flags +0xbc & 8 (Einheit)
  bool no_dodge = false;        // Klasse +0x235
  int priority = 0;             // Klasse +0x1ac
  int group = -1;               // +0xc4
  bool has_parent = false;      // +0xe4 gesetzt (Elternobjekt)
  bool parent_active = false;   // Elternobjekt +0xc0 != 0
  float parent_pos[3] = {0, 0, 0};  // Elternobjekt +0xb4..+0xbc
  int parent_group = -1;        // Elternobjekt +0x7c
};
// Hindernisvermeidung (@0x42ef70 + Helfer 0x424860/0x424ba0/0x4250e0/0x425200/0x424b50): veraendert dir (Wunschrichtung, |dir|<=1)
// und s->prev_dir. proc_group = Prozess +0x7c, invalid_group = Konstante @0x5cd0e0. target_obj (optional) wird ignoriert,
// target_pos (optional) begrenzt die Vorausschau auf den Zielabstand.
struct HoverAvoidArgs {
  const HoverObstacle* obstacles = nullptr; int count = 0;
  const HoverObstacle* target_obj = nullptr;
  const float* target_pos = nullptr;
  int proc_group = -1, invalid_group = -1;
};
void HoverAvoid(HoverCraftState* s, const HoverAvoidArgs& a, float dir[3]);
// Weltwerte aus RTS_CFG.h (Original: Armada.exe liest sie beim Start; Vorgaben der exe: Ausweichen an, Boden 0, Decke 80).
// GOG-RTS_CFG.h: OBSTACLE_AVOIDANCE = 0 (Ausweichroutine @0x42ef70 springt dann direkt zur Hoehenbegrenzung), SHIP_COMBAT_FLOOR = 10, SHIP_COMBAT_CEILING = 67.
struct HoverWorldCfg { bool obstacle_avoidance = true; float floor = 0.0f; float ceiling = 80.0f; };
HoverWorldCfg& HoverWorld();
// Zielfahrt-Entscheidung (@0x42e4a0, Nicht-Borg-Zweig, ohne Hindernisvermeidung): setzt ctl fuer einen Schritt.
void HoverGotoSteer(const HoverClass& c, HoverCraftState& s, HoverControls* ctl, const float target[3], bool allow_warp, const HoverAvoidArgs* avoid = nullptr);

void HoverSimulate(const HoverClass& c, HoverCraftState* s, const HoverControls& ctl, float dt);

}  // namespace fleet
