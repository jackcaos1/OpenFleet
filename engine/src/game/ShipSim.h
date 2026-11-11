#pragma once
// Flugmodell der Schiffe (GL-frei, testbar). Werte stammen aus den Physik-ODFs (battphys.odf u. a.), siehe docs/formats/physik.md.
// Belegt ist nur, WELCHE Werte es gibt; wie das Spiel sie verrechnet, ist nicht belegt. Die Verrechnung hier ist eine Annahme
// (Konstanten unten, jeweils kommentiert) und soll sich nur plausibel anfuehlen.
//
// Koordinaten: GL-Welt (y oben). Kurs yaw: Blickrichtung = (sin yaw, 0, cos yaw) (ohne Neigung); pitch > 0 = Bug nach oben;
// roll > 0 = Linkskurve geneigt (linke Flaeche unten).
#include "core/Math.h"
#include "data/UnitDef.h"

namespace fleet {

struct FlightParams {
  bool valid = false;
  float impulse = 20, warp = 80;     // m/s
  float turn_radius = 30;            // m bei Impulsgeschwindigkeit
  float max_roll = 40;               // Grad
  float max_pitch = 45;              // Grad
  float pitch_rate = 0.0667f;        // Anteil/s (Annahme: rad/s)
  float roll_rate = 0.5f;            // Anteil/s (Annahme: rad/s)
  float front_back_inertia = 50;     // groesser = traeger beim Beschleunigen
  float side_inertia = 7.5f;         // Seitenrutschen (leftRightInertia)
  float max_reverse = 4;             // m/s (Annahme: absolut)
  float veloc_coeff_still = 0.5f;    // Anteil der Wendigkeit im Stand
  float veloc_half = 0.7f;           // Geschwindigkeit, bei der der Anteil auf halbem Weg zu 1 ist (Annahme)
  float heading_ok_for_warp = 30;    // Grad: Warp nur, wenn das Ziel ungefaehr voraus liegt
  float too_close_for_warp = 10;     // m
  float too_close_to_turn = 50;      // m: naeher am Ziel wird nicht mehr gewendet, das Schiff bleibt stehen
};

FlightParams FlightFromPhysics(const PhysicsDef& p);

struct ShipState {
  float pos[3] = {0, 0, 0};
  float yaw = 0, pitch = 0, roll = 0;
  float speed = 0;            // Schub entlang des Bugs (m/s)
  float vel[3] = {0, 0, 0};   // tatsaechliche Bewegung (rutscht dem Bug nach)
  float yaw_rate = 0;         // rad/s, fuer die Neigung
  bool has_target = false;
  float target[3] = {0, 0, 0};
  bool warp = false;          // gerade mit Warp unterwegs
  // Ausrichtung aus der Original-Flugphysik (HoverShip): Bug und Oben in der GL-Welt; wenn gesetzt, ersetzt sie yaw/pitch/roll beim Zeichnen
  bool has_basis = false;
  float b_front[3] = {0, 0, 1}, b_up[3] = {0, 1, 0};
};

// Zeitkonstanten (Annahmen): Sekunden je Traegheitseinheit.
constexpr float kInertiaSeconds = 0.1f;

// Ein Schritt (dt in Sekunden, bis ca. 0.1 empfohlen). Mit Ziel: dorthin fliegen und stehen bleiben; ohne Ziel: ausrollen.
void StepShip(const FlightParams& fp, ShipState* s, float dt);
void StopShip(ShipState* s);                  // Ziel loeschen, ausrollen
void SetShipTarget(ShipState* s, const float p[3]);

// Blickrichtung (Einheitsvektor) des Schiffs.
void ShipForward(const ShipState& s, float out[3]);
// Modellmatrix (Modell-GL -> Welt-GL; Bug des Modells zeigt in -z) mit Massstab; rot = nur Drehung (Beleuchtung).
void ShipMatrices(const ShipState& s, float scale, Mat4* model, Mat4* rot);
// Anfangskurs aus dem Bug-Vektor in der GL-Welt (z.B. aus der Kartenmatrix); Neigung und Rollen bleiben 0.
void ShipFromForward(const float fwd[3], ShipState* s);

}  // namespace fleet
