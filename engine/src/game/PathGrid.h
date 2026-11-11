#pragma once
// Pfadfinder der Armada.exe (GL-frei). Koordinaten: Originalsystem (x, z = -z_GL), nur die Ebene.
// EXAKTER PORT des Planers @0x6322b8 (Wrapper 0x40a310), geprueft gegen die Originalcode-Emulation (tools/path_run.py,
// tools/oracle_*.json, tools/path_proto_rub.py):
//  - Mehrebenen-Quadtree (8 Ebenen, Aufloesung HIGHEST_QUADTREE_GRID_RESOLUTION), Gitter = Welt - Kartenmin + 500, Rand 550 gesperrt
//  - Stempel 0x409900/0x40c700 (Asteroidenfeld: Quadrat Halbkante 75, IMPASSABLE)
//  - vor der Suche Zusammenfassen gleicher Kinder (0x40d280), Regionen = Blaetter, Nachbarn = gemeinsame Kante
//  - A* ueber die Regionsmitten (0x408500, MSVC-Heap, float32), danach Rubber-Banding (0x40ac80 Torgraph + A*, 0x40b8c0 Trichterglaettung)
// Kantenkosten der Regionssuche: 0x40e470 (mit Zellkosten). Offen: Stempel von Nebel/Schwarzem Loch/Planeten.
#include <cstdint>
#include <vector>

namespace fleet {

struct PathPoint { float x, z; };

class PathGrid {
 public:
  static constexpr int32_t kImpassable = -1;   // IMPASSABLE_PATHING_COST (RTS_CFG.h)
  // Kartenbereich [min,max] in Weltkoordinaten, res = HIGHEST_QUADTREE_GRID_RESOLUTION.
  void Init(float minx, float minz, float maxx, float maxz, float res);
  // 0x409900 mit x0=x1, z0=z1: Rechteck [x-r, x+r] x [z-r, z+r] mit Kosten stempeln
  void Stamp(float x, float z, float r, int32_t cost);
  void StampBox(float x0, float z0, float x1, float z1, float r, int32_t cost);   // 0x409900 mit Rechteck (Fussabdruck)
  void UnstampBox(float x0, float z0, float x1, float z1) { StampBox(x0, z0, x1, z1, 0.0f, default_cost_); }   // 0x409a80: wie Stempel mit Standardkosten (hebt nur Zellen < 100 an, 198 bleibt!)
  void StampSquare(float cx, float cz, float half) { Stamp(cx, cz, half, kImpassable); }   // nur Kern
  void StampField(float x, float z);   // Asteroidenfeld wie im Original: weicher Ring + unpassierbarer Kern
  bool Blocked(float x, float z) const;   // Zelle unter dem Punkt gesperrt?
  // Weg von a nach b (ohne a, mit b). false = kein Weg (Ziel ausserhalb/gesperrt/unerreichbar), dann bleibt out = {b}.
  bool FindPath(const PathPoint& a, const PathPoint& b, std::vector<PathPoint>* out) const;
  bool Empty() const { return !init_; }   // noch nicht initialisiert

 private:
  struct Level { int cols = 0, rows = 0; double size = 0; std::vector<int32_t> cost; std::vector<uint8_t> sub; };
  struct Leaf { int k, i, j; int32_t cost; double x0, x1, z0, z1; };
  struct Edge { int to; float cost; };
  struct Res { bool leaf; int32_t cost; };
  void StampRect(double x0, double z0, double x1, double z1, int32_t cost, int flag, int k, int i, int j);
  bool Want(int32_t old, int32_t nw, int flag) const;
  Res Collect(int k, int i, int j, std::vector<Leaf>* out) const;
  void Build() const;
  int Find(double x, double z) const;
  static bool AStar(const std::vector<double>& xs, const std::vector<double>& zs, const std::vector<std::vector<Edge>>& adj,
                    const std::vector<uint8_t>* blocked, int S, int G, std::vector<int>* path);
  bool Rubber(const std::vector<int>& p, double sx, double sz, double gx, double gz, std::vector<PathPoint>* nodes) const;

  bool init_ = false;
  float minx_ = 0, minz_ = 0, W_ = 0, H_ = 0, res_ = 50;
  int32_t default_cost_ = 100;
  std::vector<Level> lv_;
  mutable bool dirty_ = true;
  mutable std::vector<Leaf> leaves_;
  mutable std::vector<std::vector<Edge>> adj_;
  mutable std::vector<uint8_t> blocked_;
};

}  // namespace fleet
