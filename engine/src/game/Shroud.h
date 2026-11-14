// Per-team record of which parts of the map a team's ships have already seen.
// Used by the scout order (Armada.exe 0x42b870 asks the team's map object for an unexplored point around the
// start position, 0x41fd80, marks visited cells, 0x4220b0, and clears the map, 0x421900, when the search radius
// is exhausted) and by search-and-destroy (0x42bb20 reveals the area around the ship every frame, 0x421af0).
// ASSUMPTION: the original cell size and the exact reveal shape are not read yet; a square grid is used here.
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
  // Marks every cell whose centre lies within r of (x, z).
  void Reveal(float x, float z, float r) {
    if (Empty()) return;
    const int x0 = Clampi(static_cast<int>(std::floor((x - r - minx_) / cell_)), nx_);
    const int x1 = Clampi(static_cast<int>(std::floor((x + r - minx_) / cell_)), nx_);
    const int z0 = Clampi(static_cast<int>(std::floor((z - r - minz_) / cell_)), nz_);
    const int z1 = Clampi(static_cast<int>(std::floor((z + r - minz_) / cell_)), nz_);
    for (int iz = z0; iz <= z1; ++iz)
      for (int ix = x0; ix <= x1; ++ix) {
        const float cx = minx_ + (static_cast<float>(ix) + 0.5f) * cell_ - x;
        const float cz = minz_ + (static_cast<float>(iz) + 0.5f) * cell_ - z;
        if (cx * cx + cz * cz <= r * r) seen_[Idx(ix, iz)] = 1;
      }
  }
  bool Seen(float x, float z) const {
    if (Empty()) return true;
    const int ix = static_cast<int>(std::floor((x - minx_) / cell_)), iz = static_cast<int>(std::floor((z - minz_) / cell_));
    if (ix < 0 || iz < 0 || ix >= nx_ || iz >= nz_) return true;
    return seen_[Idx(ix, iz)] != 0;
  }
  void Clear() { seen_.assign(seen_.size(), 0); }
  // First unexplored cell centre within r of (cx, cz), scanning from a random start cell (rnd in [0,1)) so that the
  // choice does not always favour one corner. Returns false when everything within r is explored.
  bool FindUnseen(float cx, float cz, float r, float rnd, float* ox, float* oz) const {
    if (Empty()) return false;
    const int total = nx_ * nz_;
    const int start = static_cast<int>(rnd * static_cast<float>(total)) % total;
    for (int k = 0; k < total; ++k) {
      const int c = (start + k) % total;
      if (seen_[static_cast<size_t>(c)]) continue;
      const int ix = c % nx_, iz = c / nx_;
      const float px = minx_ + (static_cast<float>(ix) + 0.5f) * cell_, pz = minz_ + (static_cast<float>(iz) + 0.5f) * cell_;
      if ((px - cx) * (px - cx) + (pz - cz) * (pz - cz) <= r * r) { *ox = px; *oz = pz; return true; }
    }
    return false;
  }

 private:
  static int Clampi(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }
  size_t Idx(int ix, int iz) const { return static_cast<size_t>(iz) * static_cast<size_t>(nx_) + static_cast<size_t>(ix); }
  float minx_ = 0, minz_ = 0, cell_ = 100;
  int nx_ = 0, nz_ = 0;
  std::vector<uint8_t> seen_;
};

}  // namespace fleet
