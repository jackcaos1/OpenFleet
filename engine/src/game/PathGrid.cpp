#include <unordered_map>
#include <functional>
#include "game/PathGrid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fleet {

namespace {
struct P { double x, z; };
inline double Cr(P a, P b) { return a.x * b.z - a.z * b.x; }
inline P Sub(P p, P c) { return {p.x - c.x, p.z - c.z}; }
inline double Dist(P a, P b) { const double dx = a.x - b.x, dz = a.z - b.z; return std::sqrt(dx * dx + dz * dz); }
constexpr int kLevels = 8;
}  // namespace

void PathGrid::Init(float minx, float minz, float maxx, float maxz, float res) {
  init_ = true;
  res_ = res > 1 ? res : 50.0f;
  minx_ = minx; minz_ = minz;
  W_ = std::fabs(maxx - minx) + 1000.0f;   // Kartenbreite + 2 * 500 Rand (0x4093b0)
  H_ = std::fabs(maxz - minz) + 1000.0f;
  lv_.assign(kLevels, Level());
  for (int k = 0; k < kLevels; ++k) {
    Level& L = lv_[static_cast<size_t>(k)];
    L.size = static_cast<double>(res_) * static_cast<double>(1 << (kLevels - 1 - k));
    L.cols = static_cast<int>(std::ceil(static_cast<double>(W_) / L.size));
    L.rows = static_cast<int>(std::ceil(static_cast<double>(H_) / L.size));
    L.cost.assign(static_cast<size_t>(L.cols) * static_cast<size_t>(L.rows), default_cost_);
    L.sub.assign(L.cost.size(), 0);
  }
  dirty_ = true;
  // Konstruktor 0x408e30: vier Randstreifen der Breite 500 + Aufloesung sind gesperrt
  const double B = 500.0 + static_cast<double>(res_), w = W_, h = H_;
  for (int j = 0; j < lv_[0].rows; ++j)
    for (int i = 0; i < lv_[0].cols; ++i) {
      StampRect(0, 0, w, B, kImpassable, 1, 0, i, j);
      StampRect(0, h - B, w, h, kImpassable, 1, 0, i, j);
      StampRect(0, 0, B, h, kImpassable, 1, 0, i, j);
      StampRect(w - B, 0, w, h, kImpassable, 1, 0, i, j);
    }
}

bool PathGrid::Want(int32_t old, int32_t nw, int flag) const {   // Ueberschreibregel 0x40c831..0x40c87a
  if (old < nw && old != kImpassable) return true;
  if (nw == kImpassable) return true;
  if (old == default_cost_ || !flag) return true;
  return false;
}

void PathGrid::StampRect(double x0, double z0, double x1, double z1, int32_t cost, int flag, int k, int i, int j) {
  Level& L = lv_[static_cast<size_t>(k)];
  if (i < 0 || j < 0 || i >= L.cols || j >= L.rows) return;
  const double sz = L.size, cx0 = i * sz, cz0 = j * sz, cx1 = cx0 + sz, cz1 = cz0 + sz;
  const size_t a = static_cast<size_t>(j) * static_cast<size_t>(L.cols) + static_cast<size_t>(i);
  const bool full = !(x0 > cx0 || x1 < cx1 || z0 > cz0 || z1 < cz1);
  if (full && !L.sub[a]) { if (Want(L.cost[a], cost, flag)) L.cost[a] = cost; return; }
  const double ix0 = std::max(x0, cx0), iz0 = std::max(z0, cz0), ix1 = std::min(x1, cx1), iz1 = std::min(z1, cz1);
  if (!(ix0 <= ix1 && iz0 <= iz1)) return;
  if (k == kLevels - 1) { if (Want(L.cost[a], cost, flag)) L.cost[a] = cost; return; }
  if (!L.sub[a]) {
    L.sub[a] = 1;
    Level& C = lv_[static_cast<size_t>(k) + 1];
    for (int dj = 0; dj < 2; ++dj)
      for (int di = 0; di < 2; ++di) {
        const int ci = 2 * i + di, cj = 2 * j + dj;
        if (ci < C.cols && cj < C.rows) C.cost[static_cast<size_t>(cj) * static_cast<size_t>(C.cols) + static_cast<size_t>(ci)] = L.cost[a];
      }
  }
  for (int dj = 0; dj < 2; ++dj)
    for (int di = 0; di < 2; ++di) StampRect(x0, z0, x1, z1, cost, flag, k + 1, 2 * i + di, 2 * j + dj);
}

void PathGrid::StampField(float x, float z) {
  // 0x44e7e9..0x44e842: weicher Ring (Byte 0x5fdfc8 = 1): Radius 150/2 + 40, Kosten 3 * Standard; danach der Kern
  Stamp(x, z, 75.0f + 40.0f, default_cost_ * 3);
  Stamp(x, z, 75.0f, kImpassable);
}

void PathGrid::Stamp(float x, float z, float r, int32_t cost) { StampBox(x, z, x, z, r, cost); }

void PathGrid::StampBox(float x0, float z0, float x1, float z1, float r, int32_t cost) {
  if (!init_) return;
  // 0x409900: [x0-r, x1+r] in Gitterkoordinaten, auf [0,W]/[0,H] geklemmt
  auto cl = [](double v, double hi) { return v < 0 ? 0.0 : (v >= hi ? hi : v); };
  const double ax = cl(static_cast<float>(static_cast<double>(x0) - r - minx_ + 500.0), W_);
  const double az = cl(static_cast<float>(static_cast<double>(z0) - r - minz_ + 500.0), H_);
  const double bx = cl(static_cast<float>(static_cast<double>(x1) + r - minx_ + 500.0), W_);
  const double bz = cl(static_cast<float>(static_cast<double>(z1) + r - minz_ + 500.0), H_);
  for (int j = 0; j < lv_[0].rows; ++j)
    for (int i = 0; i < lv_[0].cols; ++i) StampRect(ax, az, bx, bz, cost, 1, 0, i, j);
  dirty_ = true;
}

PathGrid::Res PathGrid::Collect(int k, int i, int j, std::vector<Leaf>* out) const {
  const Level& L = lv_[static_cast<size_t>(k)];
  const size_t a = static_cast<size_t>(j) * static_cast<size_t>(L.cols) + static_cast<size_t>(i);
  if (L.sub[a] && k < kLevels - 1) {
    const Level& C = lv_[static_cast<size_t>(k) + 1];
    Res kids[4]; int ci[4], cj[4]; int n = 0;
    for (int dj = 0; dj < 2; ++dj)
      for (int di = 0; di < 2; ++di) {
        const int x = 2 * i + di, y = 2 * j + dj;
        if (x < C.cols && y < C.rows) { kids[n] = Collect(k + 1, x, y, out); ci[n] = x; cj[n] = y; ++n; }
      }
    bool merge = n == 4;   // 0x40d280: vier gleiche Blaetter -> ein Blatt
    for (int m = 0; merge && m < 4; ++m) merge = kids[m].leaf && kids[m].cost == kids[0].cost;
    if (merge) return kids[0];
    for (int m = 0; m < n; ++m)
      if (kids[m].leaf) {
        const double s = C.size;
        out->push_back({k + 1, ci[m], cj[m], kids[m].cost, ci[m] * s, (ci[m] + 1) * s, cj[m] * s, (cj[m] + 1) * s});
      }
    return {false, 0};
  }
  return {true, L.cost[a]};
}

void PathGrid::Build() const {
  if (!dirty_) return;
  dirty_ = false;
  leaves_.clear();
  for (int j = 0; j < lv_[0].rows; ++j)
    for (int i = 0; i < lv_[0].cols; ++i) {
      const Res r = Collect(0, i, j, &leaves_);
      if (r.leaf) { const double s = lv_[0].size; leaves_.push_back({0, i, j, r.cost, i * s, (i + 1) * s, j * s, (j + 1) * s}); }
    }
  const size_t n = leaves_.size();
  adj_.assign(n, {});
  blocked_.assign(n, 0);
  for (size_t a = 0; a < n; ++a) blocked_[a] = leaves_[a].cost == kImpassable;
  // Nachbarlisten wie im Original (0x40d450): rekursiv, Seitenlisten der Kinder, dann vier Zwei-Zeiger-Durchlaeufe.
  // Die Reihenfolge der Verknuepfungen bestimmt die Reihenfolge der Nachbarn und damit die A*-Gleichstaende.
  std::unordered_map<long long, int> idx;
  auto key = [](int k, int i, int j) { return (static_cast<long long>(k) << 40) | (static_cast<long long>(i) << 20) | j; };
  for (size_t a = 0; a < n; ++a) idx[key(leaves_[a].k, leaves_[a].i, leaves_[a].j)] = static_cast<int>(a);
  const double eps = 0.5;
  auto link = [&](int pa, int qb) {
    const Leaf &A = leaves_[static_cast<size_t>(pa)], &B = leaves_[static_cast<size_t>(qb)];
    const double dx = (A.x0 + A.x1) * 0.5 - (B.x0 + B.x1) * 0.5, dz = (A.z0 + A.z1) * 0.5 - (B.z0 + B.z1) * 0.5;
    const double ha = (A.x1 - A.x0) * 0.5, hb = (B.x1 - B.x0) * 0.5;
    const double t = static_cast<double>(static_cast<float>(ha / (ha + hb)));
    const float c = static_cast<float>(std::sqrt(dx * dx + dz * dz) * ((1.0 - t) * static_cast<double>(B.cost) + static_cast<double>(A.cost) * t));
    adj_[static_cast<size_t>(pa)].push_back({qb, c});
    adj_[static_cast<size_t>(qb)].push_back({pa, c});
  };
  // axis 0: Ueberlappung in x (Original +0x10/+0x14), axis 1: in z (+8/+0xc); adv_hi: Vorschub nach Oberkante bzw. Unterkante
  auto lo = [&](int c, int ax) { const Leaf& l = leaves_[static_cast<size_t>(c)]; return ax == 0 ? l.x0 : l.z0; };
  auto hi = [&](int c, int ax) { const Leaf& l = leaves_[static_cast<size_t>(c)]; return ax == 0 ? l.x1 : l.z1; };
  auto sweep = [&](const std::vector<int>* P, const std::vector<int>* Q, int ax, bool adv_hi) {
    size_t i = 0, j = 0;
    if (P->empty()) return;
    for (;;) {
      if (!(j < Q->size())) break;
      do {
        const int b = (*P)[i], a = (*Q)[j];
        if ((lo(a, ax) + eps >= lo(b, ax) && lo(a, ax) - eps < hi(b, ax)) || (hi(a, ax) + eps > lo(b, ax) && hi(a, ax) - eps <= hi(b, ax)))
          if (!(hi(a, ax) <= lo(b, ax) || hi(b, ax) <= lo(a, ax))) link(b, a);
        const bool adv = adv_hi ? (hi(a, ax) - eps < hi(b, ax)) : (lo(a, ax) - eps < lo(b, ax));
        if (!adv) break;
        ++j;
      } while (j < Q->size());
      std::swap(P, Q);
      const size_t ni = j, nj = i + 1;
      i = ni; j = nj;
      if (!(i < P->size())) break;
    }
  };
  struct Sides { std::vector<int> s[4]; };   // 0: unten (kleines j), 1: oben, 2: rechts (grosses i), 3: links
  std::function<Sides(int, int, int)> rec = [&](int k, int i, int j) -> Sides {
    Sides out;
    const auto it = idx.find(key(k, i, j));
    if (it != idx.end()) { for (auto& v : out.s) v.push_back(it->second); return out; }
    if (k >= kLevels - 1) return out;
    const Level& C = lv_[static_cast<size_t>(k) + 1];
    Sides ch[4];
    const int ci[4] = {0, 0, 1, 1}, cj[4] = {0, 1, 0, 1};
    for (int m = 0; m < 4; ++m) {
      const int x = 2 * i + ci[m], y = 2 * j + cj[m];
      if (x < C.cols && y < C.rows) ch[m] = rec(k + 1, x, y);
    }
    auto cat = [](std::vector<int>& d, const std::vector<int>& a, const std::vector<int>& b) { d = a; d.insert(d.end(), b.begin(), b.end()); };
    cat(out.s[0], ch[0].s[0], ch[2].s[0]);
    cat(out.s[1], ch[1].s[1], ch[3].s[1]);
    cat(out.s[3], ch[0].s[3], ch[1].s[3]);
    cat(out.s[2], ch[2].s[2], ch[3].s[2]);
    sweep(&ch[0].s[1], &ch[1].s[0], 0, true);
    sweep(&ch[0].s[2], &ch[2].s[3], 1, true);
    sweep(&ch[3].s[0], &ch[2].s[1], 0, true);
    sweep(&ch[3].s[3], &ch[1].s[2], 1, true);
    return out;
  };
  for (int j = 0; j < lv_[0].rows; ++j)
    for (int i = 0; i < lv_[0].cols; ++i) rec(0, i, j);
#ifdef PATHDBG
  { size_t have = 0, want = 0, miss = 0;
    for (size_t a = 0; a < n; ++a) have += adj_[a].size();
    for (size_t a = 0; a < n; ++a) for (size_t b = a + 1; b < n; ++b) {
      const Leaf &A = leaves_[a], &B = leaves_[b];
      const double ox = std::min(A.x1, B.x1) - std::max(A.x0, B.x0), oz = std::min(A.z1, B.z1) - std::max(A.z0, B.z0);
      if ((ox == 0 && oz > 0) || (oz == 0 && ox > 0)) { if (A.cost == kImpassable || B.cost == kImpassable) continue; want += 2; bool f = false; for (auto& e : adj_[a]) f |= e.to == static_cast<int>(b); if (!f) ++miss; }
    }
    std::fprintf(stderr, "ADJ have=%zu want=%zu missing=%zu\n", have, want, miss); }
#endif
}

int PathGrid::Find(double x, double z) const {
  for (int pass = 0; pass < 2; ++pass) {
    if (pass) { x = std::max(0.0, std::min(x, static_cast<double>(W_) - 1e-3)); z = std::max(0.0, std::min(z, static_cast<double>(H_) - 1e-3)); }
    for (size_t i = 0; i < leaves_.size(); ++i) {
      const Leaf& l = leaves_[i];
      if (l.x0 <= x && x < l.x1 && l.z0 <= z && z < l.z1) return static_cast<int>(i);
    }
  }
  return -1;
}

bool PathGrid::Blocked(float x, float z) const {
  if (!init_) return false;
  Build();
  const double gx = std::max(0.0, std::min(static_cast<double>(static_cast<float>(x - minx_ + 500.0f)), static_cast<double>(W_)));
  const double gz = std::max(0.0, std::min(static_cast<double>(static_cast<float>(z - minz_ + 500.0f)), static_cast<double>(H_)));
  const int i = Find(gx, gz);
  return i >= 0 && leaves_[static_cast<size_t>(i)].cost == kImpassable;
}

// A* der Armada (0x408500/0x408660): MSVC-Heap (push_heap/_Adjust_heap) ueber das AKTUELLE f der Knoten, float32.
bool PathGrid::AStar(const std::vector<double>& xs, const std::vector<double>& zs, const std::vector<std::vector<Edge>>& adj,
                     const std::vector<uint8_t>* blocked, int S, int G, std::vector<int>* path) {
  const size_t n = xs.size();
  std::vector<float> f(n, 0.0f), g(n, 0.0f);
  std::vector<int> par(n, -1);
  std::vector<uint8_t> closed(n, 0), seen(n, 0);
  std::vector<int> heap;
  auto h = [&](int k) { const double dx = xs[static_cast<size_t>(k)] - xs[static_cast<size_t>(G)], dz = zs[static_cast<size_t>(k)] - zs[static_cast<size_t>(G)]; return static_cast<float>(std::sqrt(dx * dx + dz * dz) * 100.0); };
  auto fv = [&](int k) { return f[static_cast<size_t>(k)]; };
  auto push = [&](int k) {
    heap.push_back(k);
    size_t idx = heap.size() - 1;
    while (idx > 0) {
      const size_t p = (idx - 1) / 2;
      if (fv(heap[p]) > fv(k)) { heap[idx] = heap[p]; idx = p; } else break;
    }
    heap[idx] = k;
  };
  auto pop = [&]() {
    const int top = heap[0];
    const int last = heap.back(); heap.pop_back();
    const size_t ln = heap.size();
    if (ln == 0) return top;
    size_t hole = 0, child = 2;
    while (child < ln) {
      if (fv(heap[child]) > fv(heap[child - 1])) --child;
      heap[hole] = heap[child]; hole = child; child = 2 * child + 2;
    }
    if (child == ln) { heap[hole] = heap[ln - 1]; hole = ln - 1; }
    size_t idx = hole;
    while (idx > 0) {
      const size_t p = (idx - 1) / 2;
      if (fv(heap[p]) > fv(last)) { heap[idx] = heap[p]; idx = p; } else break;
    }
    heap[idx] = last;
    return top;
  };
  auto relax = [&](int k, int p, float e) {
    const size_t K = static_cast<size_t>(k), Pp = static_cast<size_t>(p);
    if (!seen[K]) { seen[K] = 1; closed[K] = 0; f[K] = 0; g[K] = 0; par[K] = -1; }
    if (closed[K]) {
      const float hh = f[K] - g[K];
      const float gn = g[Pp] + e;
      const float fc = gn + hh;
      if (f[K] > fc) { g[K] = gn; f[K] = fc; par[K] = p; push(k); closed[K] = 0; }
      return;
    }
    if (f[K] == 0) { g[K] = g[Pp] + e; f[K] = g[K] + h(k); par[K] = p; push(k); }
  };
  auto expand = [&](int c) {
    for (const Edge& e : adj[static_cast<size_t>(c)]) {
      if (blocked && (*blocked)[static_cast<size_t>(e.to)]) continue;
      relax(e.to, c, e.cost);
    }
  };
  relax(S, S, 0.0f);
  int cur = S;
  closed[static_cast<size_t>(S)] = 1;
  expand(cur);
  while (cur != G) {
    if (heap.empty()) return false;
    cur = pop();
    closed[static_cast<size_t>(cur)] = 1;
    expand(cur);
  }
  path->assign(1, G);
  while (path->back() != S) {
    const int p = par[static_cast<size_t>(path->back())];
    if (p < 0 || path->size() > n + 1) return false;
    path->push_back(p);
  }
  std::reverse(path->begin(), path->end());
  return true;
}

namespace {
struct Gate { int code; P n1, n2; };
// 0x40aa20: Tor zwischen Zelle A und B (liegt an der Kante von B)
template <class LeafT>
Gate MakeGate(const LeafT& A, const LeafT& B, bool same) {
  if (same) return {4, {0, 0}, {0, 0}};
  const double dx = (B.x0 + B.x1) * 0.5 - (A.x0 + A.x1) * 0.5, dz = (B.z0 + B.z1) * 0.5 - (A.z0 + A.z1) * 0.5;
  if (std::fabs(dz) < std::fabs(dx)) {
    if (dx > 0) { const double x = B.x0; return {2, {x, std::min(A.z1, B.z1)}, {x, std::max(A.z0, B.z0)}}; }
    const double x = B.x1; return {3, {x, std::max(A.z0, B.z0)}, {x, std::min(A.z1, B.z1)}};
  }
  if (dz < 0) { const double z = B.z1; return {1, {std::min(A.x1, B.x1), z}, {std::max(A.x0, B.x0), z}}; }
  const double z = B.z0; return {0, {std::max(A.x0, B.x0), z}, {std::min(A.x1, B.x1), z}};
}
// 0x40b680: Trichter (L,R) um den Scheitel C mit dem neuen Tor (N1,N2) fortschreiben; false = Trichter zerbricht
bool UpdateFunnel(P C, P* L, P* R, P N1, P N2) {
  if (Cr(Sub(*L, C), Sub(N1, C)) < 0) {
    if (Cr(Sub(*R, C), Sub(N1, C)) > 0) *L = N1; else return false;
  }
  if (Cr(Sub(*R, C), Sub(N2, C)) > 0) {
    if (Cr(Sub(*L, C), Sub(N2, C)) < 0) { *R = N2; return true; }
    return false;
  }
  return true;
}
bool PreCheck(int code, P C, P N1) {
  switch (code) {
    case 0: return C.z < N1.z;
    case 1: return C.z > N1.z;
    case 2: return C.x < N1.x;
    case 3: return C.x > N1.x;
  }
  return true;
}
}  // namespace

// 0x40ac80 (Torgraph + A*) und 0x40b8c0 (Trichterglaettung). p = Regionspfad (Blattindizes) inkl. Start- und Zielregion.
bool PathGrid::Rubber(const std::vector<int>& p, double sx, double sz, double gx, double gz, std::vector<PathPoint>* result) const {
  const size_t n = p.size();
  if (n < 3) return false;
  std::vector<const Leaf*> cells;
  cells.push_back(&leaves_[static_cast<size_t>(Find(sx, sz))]);
  for (size_t i = 1; i < n; ++i) cells.push_back(&leaves_[static_cast<size_t>(p[i])]);
  std::vector<double> xs{sx}, zs{sz};
  std::vector<std::vector<Edge>> adj(1);
  const Leaf* prevc = cells[0];
  int P1 = 0, P2 = -1;
  auto ec = [&](int u, int v, int32_t cost) {
    const double dx = xs[static_cast<size_t>(u)] - xs[static_cast<size_t>(v)], dz = zs[static_cast<size_t>(u)] - zs[static_cast<size_t>(v)];
    return static_cast<float>(std::sqrt(dx * dx + dz * dz) * static_cast<double>(cost));   // fsqrt, fimul [Zelle], fstp float
  };
  for (size_t i = 1; i < n; ++i) {
    const Leaf* thisc = cells[i];
    const Gate gt = MakeGate(*prevc, *thisc, prevc == thisc);
    const int a = static_cast<int>(xs.size()); xs.push_back(gt.n1.x); zs.push_back(gt.n1.z); adj.emplace_back();
    const int b = static_cast<int>(xs.size()); xs.push_back(gt.n2.x); zs.push_back(gt.n2.z); adj.emplace_back();
    const int32_t cost = prevc->cost;   // die Kosten der durchquerten Zelle (vorherige Zelle)
    adj[static_cast<size_t>(P1)].push_back({a, ec(P1, a, cost)});
    adj[static_cast<size_t>(P1)].push_back({b, ec(P1, b, cost)});
    if (P2 >= 0) { adj[static_cast<size_t>(P2)].push_back({a, ec(P2, a, cost)}); adj[static_cast<size_t>(P2)].push_back({b, ec(P2, b, cost)}); }
    P1 = a; P2 = b; prevc = thisc;
  }
  const int e = static_cast<int>(xs.size()); xs.push_back(gx); zs.push_back(gz); adj.emplace_back();
  adj[static_cast<size_t>(P1)].push_back({e, ec(P1, e, prevc->cost)});
  adj[static_cast<size_t>(P2)].push_back({e, ec(P2, e, prevc->cost)});
  std::vector<int> path;
  if (!AStar(xs, zs, adj, nullptr, 0, e, &path)) return false;
  cells.push_back(&leaves_[static_cast<size_t>(Find(gx, gz))]);
  // Glaettung
  std::vector<P> L1;
  for (int k : path) L1.push_back({xs[static_cast<size_t>(k)], zs[static_cast<size_t>(k)]});
  const size_t count = L1.size();
#ifdef PATHDBG
  for (size_t i = 0; i < count; ++i) fprintf(stderr, "L1[%zu]=(%.0f,%.0f) cell k%d (%.0f..%.0f,%.0f..%.0f) c%d\n", i, L1[i].x, L1[i].z, cells[i]->k, cells[i]->x0, cells[i]->x1, cells[i]->z0, cells[i]->z1, cells[i]->cost);
#endif
  std::vector<P> out{L1[0]};
  if (count > 1) {
    P cur = L1[0], N1{0, 0}, N2{0, 0}, lastacc{0, 0}, nxt{0, 0}, Lb{0, 0}, Rb{0, 0};
    const Leaf* prev = cells[0];
    size_t anchor = 0, esi = 1, acc = 0;
    bool ok = true;
    while (esi < count) {
      while (ok && esi < count) {
        nxt = L1[esi];
        const Leaf* cell = cells[esi];
        const Gate g = MakeGate(*prev, *cell, prev == cell);
        if (g.code != 4) {   // 0x40b4f0: Torpunkte nach Drehsinn um den Scheitel ordnen
          const double t1 = (g.n1.x - cur.x) * (g.n2.z - cur.z), t2 = (g.n2.x - cur.x) * (g.n1.z - cur.z);
          if (t1 < t2) { N1 = g.n1; N2 = g.n2; } else { N1 = g.n2; N2 = g.n1; }
        }
        if (esi == anchor + 1) {
          Lb = N1; Rb = N2;
        } else {
          if (g.code != 4) ok = PreCheck(g.code, cur, N1) && UpdateFunnel(cur, &Lb, &Rb, N1, N2);
          if (ok) {
            if (Cr(Sub(Lb, cur), Sub(nxt, cur)) > 0) ok = false;
            else if (Cr(Sub(Rb, cur), Sub(nxt, cur)) < 0) ok = false;
          }
        }
#ifdef PATHDBG
        fprintf(stderr, "esi=%zu code=%d cur=(%.0f,%.0f) N1=(%.0f,%.0f) N2=(%.0f,%.0f) L=(%.0f,%.0f) R=(%.0f,%.0f) nxt=(%.0f,%.0f) ok=%d first=%d\n", esi, g.code, cur.x, cur.z, N1.x, N1.z, N2.x, N2.z, Lb.x, Lb.z, Rb.x, Rb.z, nxt.x, nxt.z, (int)ok, (int)(esi == anchor + 1));
#endif
        if (ok) { lastacc = nxt; acc = esi; ++esi; prev = cell; }
      }
      if (esi == count && ok) { out.push_back(nxt); break; }
      out.push_back(lastacc); cur = lastacc; anchor = acc; esi = acc + 1; ok = true;
    }
  }
  result->clear();
  for (const P& q : out) result->push_back({static_cast<float>(q.x), static_cast<float>(q.z)});
  return true;
}

bool PathGrid::FindPath(const PathPoint& a, const PathPoint& b, std::vector<PathPoint>* out) const {
  out->clear();
  out->push_back(b);
  if (!init_) return true;
  Build();
  // Wrapper 0x40a310 / 0x40a950: Welt -> Gitter (x - min + 500); Ziel ausserhalb der Karte -> kein Weg
  const float gxf = (b.x - minx_) + 500.0f, gzf = (b.z - minz_) + 500.0f;
  if (gxf < 0 || gxf > W_ || gzf < 0 || gzf > H_) return false;
  const int gi = Find(gxf, gzf);
  if (gi < 0 || leaves_[static_cast<size_t>(gi)].cost == kImpassable) return false;   // Ziel in gesperrter Zelle
  auto cl = [](float v, float hi) { return v < 0 ? 0.0f : (v > hi ? hi : v); };
  const float sx = cl((a.x - minx_) + 500.0f, W_), sz = cl((a.z - minz_) + 500.0f, H_);
  const int si = Find(sx, sz);
  if (si < 0) return false;
  if (si == gi) return true;   // gleiche Region: direkter Weg
  std::vector<std::vector<Edge>>& adj = adj_;
  std::vector<double> xs(leaves_.size()), zs(leaves_.size());
  for (size_t i = 0; i < leaves_.size(); ++i) { xs[i] = (leaves_[i].x0 + leaves_[i].x1) * 0.5; zs[i] = (leaves_[i].z0 + leaves_[i].z1) * 0.5; }
  std::vector<int> p;
  if (!AStar(xs, zs, adj, &blocked_, si, gi, &p)) return false;
  std::vector<PathPoint> nodes;
  if (!Rubber(p, sx, sz, gxf, gzf, &nodes)) {   // PERFORM_RUBBER_BANDING=1; weniger als 3 Regionen -> Regionsmitten
    nodes.clear();
    for (int k : p) nodes.push_back({static_cast<float>(xs[static_cast<size_t>(k)]), static_cast<float>(zs[static_cast<size_t>(k)])});
  }
  out->clear();
  for (size_t i = 1; i + 1 < nodes.size(); ++i)
    out->push_back({(minx_ + nodes[i].x) - 500.0f, (minz_ + nodes[i].z) - 500.0f});
  out->push_back(b);   // erster/letzter Punkt sind Start und Ziel selbst
  return true;
}

}  // namespace fleet
