#!/usr/bin/env python3
"""Zufallsszenarien mit Stempeln (Nebel/Schwarzes Loch/Planet/Wurmloch) gegen den Original-Planer. Aufruf: SEED COUNT OUT.txt
Textformat je Szenario: n  (x z r cost)*n  sx sz gx gz  m  (x z)*m"""
import sys, random, os
sys.path.insert(0, os.path.dirname(__file__))
from path_run import *
seed, count, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
rnd = random.Random(seed)
lines = []
def kind():
    k = rnd.choice(['neb', 'bh', 'pl', 'wh', 'box'])
    x, z = rnd.uniform(-1000, 1000), rnd.uniform(-1000, 1000)
    if k == 'neb': return (x, z, x, z, 180.0, rnd.choice([2000, 400, 2, 800]))
    if k == 'bh': return (x, z, x, z, 105.0, -1)
    if k == 'pl': return (x, z, x, z, rnd.uniform(30, 160), 198)
    if k == 'box': return (x + rnd.uniform(-120, 0), z + rnd.uniform(-90, 0), x + rnd.uniform(0, 120), z + rnd.uniform(0, 90), 0.0, 198)
    return (x, z, x, z, 0.0, 500)
for _ in range(count):
    st = [kind() for _ in range(rnd.randint(1, 5))]
    s = (rnd.uniform(-1200, 1200), 30.0, rnd.uniform(-1200, 1200)); g = (rnd.uniform(-1200, 1200), 30.0, rnd.uniform(-1200, 1200))
    m = PathMachine(os.path.join(os.path.dirname(__file__), '..', 'originalFiles', 'Armada.exe'))
    m.w32(0x6322b8, m.make_planner()); p = m.r32(0x6322b8); m.call(0x4093b0, ecx=p)
    for (x, z, x1, z1, r, c) in st:
        cp = m.alloc(4); m.w32(cp, c & 0xffffffff); rr = fbits(r)
        m.call(0x409900, ecx=p, args=[fbits(x), fbits(z), fbits(x1), fbits(z1), rr, rr, cp])
    o = m.alloc(16)
    path = m.call(0x40a310, ecx=p, args=[fbits(s[0]), fbits(s[1]), fbits(s[2]), fbits(g[0]), fbits(g[1]), fbits(g[2]), 0, 0, o, 0])
    pts = []
    if path:
        n = m.r32(path); arr = m.r32(path + 4)
        pts = [(m.rf(arr + 12 * i), m.rf(arr + 12 * i + 8)) for i in range(n)]
    lines.append(' '.join([str(len(st))] + ['%r %r %r %r %r %d' % t for t in st] + ['%r %r %r %r' % (s[0], s[2], g[0], g[2]), str(len(pts))] + ['%r %r' % q for q in pts]))
open(out, 'w').write('\n'.join(lines) + '\n')
