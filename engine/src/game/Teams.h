#pragma once
// Beziehungen zwischen Teams nach Armada.exe (Teamdatensatz, Eintraege bei +0x2c: Zugriffe 0x449430..0x449530, Aufbau fuer das Spiel 0x443b20):
// 0 = Feind, 1 = neutral, 2 = verbuendet. Team 0 (herrenlos) ist zu allen neutral; sonst gilt gleiche Gruppe = verbuendet, andere = Feind.
// Im Original steht die Gruppe (Spalte "Team" der Spieleinstellung) getrennt von der Spielernummer; der Client nutzt die Gruppe als Teamnummer.
namespace fleet {

enum Relation { kEnemy = 0, kNeutral = 1, kAlly = 2 };

inline int TeamRelation(int a, int b) {
  if (a <= 0 || b <= 0) return kNeutral;
  return a == b ? kAlly : kEnemy;
}
inline bool IsEnemyTeam(int a, int b) { return TeamRelation(a, b) == kEnemy; }

}  // namespace fleet
