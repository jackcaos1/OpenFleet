// Per-team record of which parts of the map a team's ships have already seen.
// Used by the scout order (Armada.exe 0x42b870 asks the team's map object for an unexplored point around the
// start position, 0x41fd80, marks visited cells, 0x4220b0, and clears the map, 0x421900, when the search radius
// is exhausted) and by search-and-destroy (0x42bb20 reveals the area around the ship every frame, 0x421af0).
// Verified (build 110): the map is the AI "exploration map" with cells of 400 (exploration_map_resolution in AI/strategic_AI_config.txt); every craft marks the cell under
// itself each frame (0x422000 from the process update 0x427ff0), the scout marks its goal cell when it is stuck.
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace fleet {

class Shroud {
 public:
  void Init(float minx, float minz, float maxx, float maxz, float cell) {
    minx_ = minx; minz_ = minz; cell_ = cell > 1.0f ? cell : 1.0f;
    nx_ = static_cast<int>(std::ceil((maxx - minx) / cell_));
    nz_ = static_cast<int>(std::ceil((maxz - minz) / cell_));
    if (nx_ < 1) nx_ = 1;
    if (nz_ < 1) nz_ = 1;
    seen_.assign(static_cast<size_t>(nx_) * static_cast<size_t>(nz_), 0);
  }
  bool Empty() const { return seen_.empty(); }
  // Marks the cell that contains (x, z) (0x4220b0 / 0x422000 / 0x421af0: one cell per call, no radius).
  void MarkCell(float x, float z) {
    if (Empty()) return;
    const int ix = static_cast<int>(std::floor((x - minx_) / cell_)), iz = static_cast<int>(std::floor((z - minz_) / cell_));
    if (ix < 0 || iz < 0 || ix >= nx_ || iz >= nz_) return;
    seen_[Idx(ix, iz)] = 1;
  }
  bool Seen(float x, float z) const {
    if (Empty()) return true;
    const int ix = static_cast<int>(std::floor((x - minx_) / cell_)), iz = static_cast<int>(std::floor((z - minz_) / cell_));
    if (ix < 0 || iz < 0 || ix >= nx_ || iz >= nz_) return true;
    return seen_[Idx(ix, iz)] != 0;
  }
  void Clear() { seen_.assign(seen_.size(), 0); }
  // 0x41fd80: among the unexplored cells whose centre lies within r of the anchor (ax, az) and that pass `ok`, the one closest to the craft (bx, bz).
  // The scan order is rows then columns and only a strictly smaller distance replaces the best, as in the original.
  template <class Ok>
  bool FindNearestUnseen(float ax, float az, float r, float bx, float bz, Ok ok, float* ox, float* oz) const {
    if (Empty()) return false;
    float best = 3.0e30f;
    bool found = false;
    for (int iz = 0; iz < nz_; ++iz)
      for (int ix = 0; ix < nx_; ++ix) {
        if (seen_[Idx(ix, iz)]) continue;
        const float px = minx_ + (static_cast<float>(ix) + 0.5f) * cell_, pz = minz_ + (static_cast<float>(iz) + 0.5f) * cell_;
        const float da = (px - ax) * (px - ax) + (pz - az) * (pz - az);
        const float db = (px - bx) * (px - bx) + (pz - bz) * (pz - bz);
        if (db < best && da < r * r && ok(px, pz)) { best = db; *ox = px; *oz = pz; found = true; }
      }
    return found;
  }

 private:
  static int Clampi(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }
  size_t Idx(int ix, int iz) const { return static_cast<size_t>(iz) * static_cast<size_t>(nx_) + static_cast<size_t>(ix); }
  float minx_ = 0, minz_ = 0, cell_ = 100;
  int nx_ = 0, nz_ = 0;
  std::vector<uint8_t> seen_;
};

}  // namespace fleet
