#pragma once
// Anforderungen an die Kinosicht (Klasse ShotDirector, Armada.exe 0x47aef0, Objekt 0x662bf0). Die Kinosicht selbst fehlt noch; hier werden nur die Werte
// gesammelt, die das Original uebergibt (Bedeutung der Zahlen UNBEKANNT, benannt nach der Stellung im Aufruf). Siehe docs/formats/ship-health.md.
#include <vector>

namespace fleet {

struct CinematicRequest {
  float value0 = 14.0f;    // 1. Argument (immer 14,0)
  float duration = 0;      // 2. Argument: bei zerstoerten Schiffen der Sterbezaehler, bei Feuerball und Schockwelle 1,0
  float value2 = 0;        // 3. Argument: 1,5 (Sterben), 2,0 (Feuerball), 6,0 (Schockwelle)
  int int0 = 0, int1 = 0;  // 4. und 5. Argument: 0/3 bzw. 3/3
  unsigned object = 0;     // Objekt, um das es geht (Kennung)
};

// Fester Ringpuffer der letzten Anforderungen (es liest noch niemand mit).
inline void PostCinematic(std::vector<CinematicRequest>* q, const CinematicRequest& r) {
  q->push_back(r);
  if (q->size() > 32) q->erase(q->begin());
}

}  // namespace fleet
