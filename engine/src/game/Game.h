#pragma once

#include <memory>
#include <string>

#include <cstdint>

#include "io/FileSource.h"

namespace fleet {

struct PointerEvent {
  enum class Action { Down, Move, Up };
  Action action = Action::Down;
  float x = 0.0f;  // Pixel, Ursprung oben links
  float y = 0.0f;
  int id = 0;      // Finger-/Mausknopf-Nummer
};

// Rueckkanal in die Plattform (Toast/Titelzeile, Ordnerauswahl). Alle Aufrufe kommen aus dem Spiel-Thread.
class GameHost {
 public:
  virtual ~GameHost() = default;
  virtual void ShowMessage(const std::string& text) = 0;
  // Nutzer moechte den Spielordner (neu) waehlen.
  virtual void RequestDataFolder() = 0;
  // Nutzer waehlt "Beenden" im Hauptmenue.
  virtual void RequestQuit() {}
};

// Spielkern: Die Plattform (Desktop/Android) besitzt Fenster und GL-Kontext und ruft diese Methoden auf.
// M1: Modellbetrachter fuer die SOD-Dateien der Originalinstallation.
class Game {
 public:
  Game();
  ~Game();
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  void SetHost(GameHost* host);
  // Spielverzeichnis (Wurzel der Installation). nullptr = keins. Kann jederzeit gewechselt werden.
  void SetDataSource(std::shared_ptr<FileSource> source);
  // Zeigt das Modell mit diesem Dateinamen (ohne Endung, Schreibweise egal); false wenn es keins gibt.
  bool SelectModel(const std::string& name);
  void NextModel(int delta);
  // Schaltet durch die Ansichten des aktuellen Modells (Detailstufen, Offiziersquartiere, Borg, Schaden, alles).
  void NextVariant(int delta);
  // Experimentelle Umschalter, solange die Bedeutung nicht geklaert ist: Alpha-Behandlung und Teamfarbe.
  void CycleAlphaMode();
  void CycleTeamColor();
  // Zeigt die Spielwerte (ODF) des aktuellen Modells: Tippen in die Bildmitte.
  void ShowInfo();
  // Blendet die Hardpoints (Waffen-/Zielpunkte mit Richtung) ein oder aus: Ecke oben rechts.
  void ToggleHardpoints();
  // Alle normalen Waffen des Modells feuern lassen (Sichtprobe, kein Kampf): unten Mitte.
  void ToggleFire();
  // Schaltet zwischen Modellbetrachter und Kartenansicht (BZN aus bzn/): oben Mitte tippen.
  void ToggleMap();
  // Blendet einen Text einige Sekunden lang ueber dem Bild ein (und meldet ihn zusaetzlich an den Host).
  void ShowOverlay(const std::string& text);

  // GL-Kontext ist aktuell. Gibt false zurueck, wenn die Grafik nicht initialisiert werden konnte.
  bool OnSurfaceCreated();
  void OnSurfaceDestroyed();
  void OnResize(int width, int height);
  // Aktualisiert und zeichnet einen Frame. dt in Sekunden.
  void OnFrame(double dt);
  void OnPointer(const PointerEvent& event);
  void OnScroll(float amount);  // Mausrad: >0 naeher heran

  // Ton: Die Plattform ruft MixAudio() aus ihrem Audio-Thread auf (16-Bit-Stereo, verschachtelt, audio_rate() Hz).
  void MixAudio(int16_t* out, int frames);
  int audio_rate() const;
  // Ton ein-/ausschalten (Musik und Effekte).
  void ToggleMute();
  // Gitter in der Ebene y = 0 ein-/ausschalten (Original: Taste G).
  void ToggleGrid();
  // Entwicklungshilfe (Desktop, ohne Eingabe): Skirmish auf der Karte starten (Platz 1 als Computer, wenn ai_vs_ai) und den Zustand als Text ausgeben.
  bool DebugStartSkirmish(const std::string& map, bool ai_vs_ai);
  std::string DebugReport() const;
  // Menue-Weg des Spielers nachstellen: Objekt des eigenen Teams mit dieser ODF waehlen, Bauen-Seite oeffnen, Eintrag antippen; Rueckgabe: kurzer Befund
  std::string DebugHumanBuild(const std::string& odf, int item);


  // Spielstand im Originalformat (nicht im Mehrspieler). SaveState: false, wenn gerade nichts zu speichern ist (Menue).
  // LoadStateLater: wird geladen, sobald Grafik und Spieldaten bereit sind.
  bool SaveState(std::vector<uint8_t>* out);
  void LoadStateLater(std::vector<uint8_t> bytes);

  int frames_rendered() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace fleet
