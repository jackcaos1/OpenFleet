#pragma once
// Sichtprobe fuer Waffenfeuer im Modellbetrachter: alle normalen Waffen einer Einheit feuern im Takt ihrer ODF von ihren Hardpoints
// in deren Richtung. Kein Ziel, kein Schaden, keine Sonderwaffen (special = 1 / specialEnergyCost > 0). Siehe docs/formats/odf-tt.md.
#include <string>
#include <vector>

#include "data/UnitDef.h"
#include "formats/Spr.h"
#include "model/ModelBuilder.h"

namespace fleet {

struct Shot {
  enum class Kind { Beam, Bolt, Ball };
  Kind kind = Kind::Ball;
  float pos[3] = {0, 0, 0};  // Beam: Startpunkt (bleibt am Hardpoint); Bolt/Ball: aktuelle Spitze/Mitte
  float dir[3] = {0, 0, 1};
  float age = 0, life = 1;
  float speed = 0;   // Einheiten pro Sekunde (Demo-Massstab, siehe kDemoSpeedScale)
  float length = 0;  // Beam/Bolt
  float width = 1;   // Breite bzw. Durchmesser
  const SprSprite* sprite = nullptr;
  const SprAnimation* anim = nullptr;  // Bildfolge des Sprites, falls vorhanden
};

// Demo-Massstab: Originalgeschwindigkeiten (Puls 300, Torpedo 140 Einheiten/s) tragen Geschosse in Sekundenbruchteilen aus dem Bild des
// Betrachters; sie werden hier mit diesem Faktor verlangsamt. Groessen bleiben im Originalmassstab.
constexpr float kDemoSpeedScale = 0.25f;

class FireDemo {
 public:
  // Richtet die feuernden Waffen ein. model_radius begrenzt Strahlenlaenge und Flugweite im Betrachter.
  void Setup(const Model& model, const UnitDef& unit, OdfLibrary& lib, const SprLibrary& spr, float model_radius);
  void Clear();
  void SetFiring(bool on) { firing_ = on; }
  bool firing() const { return firing_; }
  void Update(float dt);
  const std::vector<Shot>& shots() const { return shots_; }
  // Namen der Feuergeraeusche (fireSound), die seit dem letzten Aufruf ausgeloest wurden (ein Eintrag je Salve eines Waffenplatzes).
  std::vector<std::string> TakeSounds() {
    std::vector<std::string> r;
    r.swap(sounds_);
    return r;
  }
  // Zahl der Waffenplaetze/Hardpoints, die feuern, und uebersprungene (Sonderwaffen o. ae.)
  int active_slots() const { return static_cast<int>(slots_.size()); }
  int skipped_slots() const { return skipped_; }
  std::string Describe() const;

 private:
  struct Slot {
    std::string name;
    std::string sound;
    Shot proto;  // Vorlage ohne Position
    std::vector<ModelHardpoint> points;
    float delay = 1.0f;
    float timer = 0;
  };
  std::vector<Slot> slots_;
  std::vector<Shot> shots_;
  std::vector<std::string> sounds_;
  bool firing_ = false;
  float max_dist_ = 100;
  int skipped_ = 0;
};

}  // namespace fleet
