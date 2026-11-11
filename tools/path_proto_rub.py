# Exakter Prototyp des Rubber-Banding (0x40ac80 + 0x40b8c0) gegen oracle_rub.json
import sys, os, json, math
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pathproto'))
import numpy as np
import astar2
from astar2 import *            # leaves_merged-Patch liegt in gates.py
import gates                    # patcht leaves -> merged
from gates import leaves_merged
F = np.float32

def gate(A, B):
    """0x40aa20: Tor zwischen Zelle A und B (Leaf-Tupel k,i,j,cost,x0,x1,z0,z1). -> code,(n1,n2)"""
    if A is B: return 4, None
    dx = (B[4] + B[5]) * 0.5 - (A[4] + A[5]) * 0.5
    dz = (B[6] + B[7]) * 0.5 - (A[6] + A[7]) * 0.5
    if abs(dz) < abs(dx):
        if dx > 0:
            x = B[4]; return 2, ((x, min(A[7], B[7])), (x, max(A[6], B[6])))
        x = B[5]; return 3, ((x, max(A[6], B[6])), (x, min(A[7], B[7])))
    if dz < 0:
        z = B[7]; return 1, ((min(A[5], B[5]), z), (max(A[4], B[4]), z))
    z = B[6]; return 0, ((max(A[4], B[4]), z), (min(A[5], B[5]), z))

def cr(a, b):  # ax*bz-az*bx
    return a[0] * b[1] - a[1] * b[0]
def sub(p, c): return (p[0] - c[0], p[1] - c[1])

def sort4f0(C, P, Q):   # 0x40b4f0
    T1 = (P[0] - C[0]) * (Q[1] - C[1]); T2 = (Q[0] - C[0]) * (P[1] - C[1])
    return (P, Q) if T1 < T2 else (Q, P)

def upd(C, st, N1, N2):  # 0x40b680, st=[L,R]
    L, R = st
    if cr(sub(L, C), sub(N1, C)) < 0:
        if cr(sub(R, C), sub(N1, C)) > 0: st[0] = L = N1
        else: return False
    if cr(sub(R, C), sub(N2, C)) > 0:
        if cr(sub(L, C), sub(N2, C)) < 0: st[1] = N2; return True
        return False
    return True

def precheck(code, C, N1):
    if code == 0: return C[1] < N1[1]
    if code == 1: return C[1] > N1[1]
    if code == 2: return C[0] < N1[0]
    if code == 3: return C[0] > N1[0]
    return True

def smooth(L1, cells):   # 0x40b8c0
    count = len(L1)
    out = [L1[0]]
    if count <= 1: return out
    cur = L1[0]; prev = cells[0]; anchor = 0; esi = 1; ok = True
    N1 = N2 = (0.0, 0.0); st = [None, None]; lastacc = None; nxt = None
    while esi < count:
        # innere Schleife
        while ok and esi < count:
            nxt = L1[esi]; cell = cells[esi]
            code, g = gate(prev, cell)
            if code != 4:
                N1, N2 = sort4f0(cur, g[0], g[1])
            if esi == anchor + 1:
                st[0], st[1] = N1, N2
            else:
                if code != 4:
                    if not precheck(code, cur, N1): ok = False
                    else: ok = upd(cur, st, N1, N2)
                if ok:
                    L, R = st
                    if cr(sub(L, cur), sub(nxt, cur)) > 0: ok = False
                    elif cr(sub(R, cur), sub(nxt, cur)) < 0: ok = False
            if ok:
                lastacc = nxt; acc = esi; esi += 1; prev = cell
        if esi == count and ok:
            out.append(nxt); break
        # Fehlschlag bei esi
        out.append(lastacc); cur = lastacc; anchor = acc; esi = acc + 1; ok = True
    return out

def graph_astar(coords, adj, S, G):
    """wie astar2.search, aber auf explizitem Knotengraphen (adj[k]=[(v,cost)])"""
    n = len(coords)
    f = [F(0)] * n; g = [F(0)] * n; par = [None] * n; closed = [0] * n; seen = [0] * n
    heap = []
    def h(k): return F(math.dist(coords[k], coords[G]) * 100.0)
    def push(k):
        heap.append(k); idx = len(heap) - 1
        while idx > 0:
            p = (idx - 1) // 2
            if f[heap[p]] > f[k]: heap[idx] = heap[p]; idx = p
            else: break
        heap[idx] = k
    def pop():
        top = heap[0]; last = heap.pop(); ln = len(heap)
        if ln == 0: return top
        hole = 0; child = 2
        while child < ln:
            if f[heap[child]] > f[heap[child - 1]]: child -= 1
            heap[hole] = heap[child]; hole = child; child = 2 * child + 2
        if child == ln: heap[hole] = heap[ln - 1]; hole = ln - 1
        idx = hole
        while idx > 0:
            p = (idx - 1) // 2
            if f[heap[p]] > f[last]: heap[idx] = heap[p]; idx = p
            else: break
        heap[idx] = last
        return top
    def relax(k, p, e):
        if not seen[k]: seen[k] = 1; closed[k] = 0; f[k] = F(0); g[k] = F(0); par[k] = None
        if closed[k]:
            fc = F(F(g[p] + e) + F(f[k] - g[k]))
            if f[k] > fc:
                g[k] = F(g[p] + e); f[k] = fc; par[k] = p; push(k); closed[k] = 0
            return
        if f[k] == 0:
            g[k] = F(g[p] + e); f[k] = F(g[k] + h(k)); par[k] = p; push(k)
    relax(S, S, F(0)); cur = S; closed[S] = 1
    def expand(c):
        for v, e in adj[c]: relax(v, c, e)
    expand(cur)
    while cur != G:
        if not heap: return None
        cur = pop(); closed[cur] = 1; expand(cur)
    p = [G]
    while p[-1] != S: p.append(par[p[-1]])
    p.reverse(); return p

def rubber(L, ctr, p, s, g):
    """p = Regionspfad (Leaf-Indizes), s,g Start/Ziel (Gitter). -> Knotenliste oder None"""
    n = len(p)
    if n < 3: return None
    cells = [L[astar2.find(L, *s)]] + [L[i] for i in p[1:]]
    coords = [s]; adj = [[]]
    prevc = cells[0]; P1 = 0; P2 = None
    for i in range(1, n):
        thisc = cells[i]
        code, gt = gate(prevc, thisc)
        a = len(coords); coords.append(gt[0]); adj.append([])
        b = len(coords); coords.append(gt[1]); adj.append([])
        cost = F(np.int32(np.uint32(prevc[3])))  # fild: vorzeichenbehaftet
        def ec(u, v): return F(math.dist(coords[u], coords[v]) * float(cost))
        adj[P1].append((a, ec(P1, a))); adj[P1].append((b, ec(P1, b)))
        if P2 is not None:
            adj[P2].append((a, ec(P2, a))); adj[P2].append((b, ec(P2, b)))
        P1, P2 = a, b; prevc = thisc
    e = len(coords); coords.append(g); adj.append([])
    cost = F(np.int32(np.uint32(prevc[3])))
    adj[P1].append((e, F(math.dist(coords[P1], g) * float(cost))))
    adj[P2].append((e, F(math.dist(coords[P2], g) * float(cost))))
    path = graph_astar(coords, adj, 0, e)
    if path is None: return None
    cells.append(L[astar2.find(L, *g)])
    return smooth([coords[k] for k in path], cells)

def run(oracle, verbose=True):
    ok = 0
    for sc in oracle:
        q = newq()
        for (x, z) in sc['fields']: proto_world(q, x, z, x, z, 75.0, IMP)
        L, ctr, adj = build(q)
        s = (sc['start'][0] + 2000., sc['start'][1] + 2000.); g = (sc['goal'][0] + 2000., sc['goal'][1] + 2000.)
        p = astar2.search(L, ctr, adj, s, g)
        r = rubber(L, ctr, p, s, g)
        if r is None: pts = [sc['start']] + [(ctr[i][0] - 2000., ctr[i][1] - 2000.) for i in p[1:-1]] + [sc['goal']]
        else: pts = [(a - 2000., b - 2000.) for a, b in r]
        want = [tuple(x) for x in sc['pts']]
        good = len(pts) == len(want) and all(abs(a[0] - b[0]) < 1 and abs(a[1] - b[1]) < 1 for a, b in zip(pts, want))
        ok += good
        if not good and verbose:
            print(' MISMATCH', sc['fields'], sc['start'], sc['goal'], '\n   got ', [(round(a), round(b)) for a, b in pts], '\n   want', [(round(a), round(b)) for a, b in want])
    print(ok, '/', len(oracle))
if __name__ == '__main__':
    run(json.load(open(sys.argv[1] if len(sys.argv) > 1 else '/tmp/oracle_rub.json')))
