#pragma once
// Beamen und Entern nach Armada.exe: Transporter-Schritt (Craft 0x467050), Entermannschaft (Klasse BoardingParty, Ankunft 0x55f750),
// Besatzung abziehen/hinzufuegen (0x4674d0, 0x467570). Alle Regeln: docs/formats/ship-health.md, Abschnitt "Beamen und Entern".
#include "game/Combat.h"

namespace fleet {

constexpr float kBeamRange = 300.0f;    // 0x600798: Mittelpunktabstand
constexpr float kBeamInterval = 1.0f;   // 0x60079c: Sekunden zwischen zwei Schritten
constexpr int kBeamCrew = 5;            // 0x6007a4: Leute je Schritt
constexpr float kBoardingDelayEnemy = 1.0f;  // Laufzeit der Entermannschaft bei Feinden (Zaehler des Effekts), bei Freunden 0

// Besatzung abziehen (0x4674d0): die Besatzung sinkt um amount (Folgen wie SetCrew); Rueckgabe = was die Mannschaft wirklich mitnimmt (hoechstens
// die vorherige Besatzung). Derelict und Schiffe ohne Besatzung verlieren nichts.
float RemoveCrew(Health* h, float amount);
// Besatzung hinzufuegen (0x467570): Rueckgabe = der Teil, der ueber die Maximalbesatzung hinausgeht (bleibt unberuecksichtigt).
float AddCrew(Health* h, float amount);

struct BeamStep {
  bool stop = false;      // Transport beenden
  bool created = false;   // eine Entermannschaft wurde gebildet
  float party_crew = 0;   // ihre Staerke
  bool enemy = false;     // Feind: Ankunft nach kBoardingDelayEnemy, sonst sofort
};
// Ein Transporter-Schritt: Abstand ueber kBeamRange oder Ziel-Bedingung nicht erfuellt -> Ende ohne Mannschaft; sonst nimmt die Quelle
// kBeamCrew Leute ab. Feindliche Ziele muessen einen Schild von hoechstens 1 % haben (und bleiben ohne Aufladung), freundliche Ziele
// brauchen Platz. Danach endet der Transport, wenn die naechste Runde die Quelle in den roten Besatzungsbereich brachte.
BeamStep TransporterStep(Health* source, Health* target, float distance, bool enemy);

struct BoardResult {
  bool owner_changed = false;   // Ziel gehoert jetzt dem Eigner der Mannschaft (Rasse und Team der Mannschaft)
  bool race_changed = false;    // Ziel desselben Teams: nur die Rasse wird die der Mannschaft
  float party_left = 0;         // Rest der Mannschaft
};
// Ankunft (0x55f750). Staerken = boardingStrength der Rassen von Mannschaft und Ziel (1,0, wenn keine Rasse).
BoardResult ArriveBoardingParty(Health* target, int target_team, float target_strength, float party_crew, float party_strength, int party_team, bool enemy);

}  // namespace fleet
