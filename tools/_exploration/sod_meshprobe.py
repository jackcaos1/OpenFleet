#!/usr/bin/env python3
"""Exploration: sucht in SOD-Dateien nach Mesh-Bloecken im Layout von v1.4
[u16 nv][u16 nuv][u16 ng] verts(f32x3) uvs(f32x2) { [u16 nf][str mat] faces(nf x 3 x (u16 v,u16 uv)) } x ng
und meldet je Version, wie viele Dateien/Bloecke passen."""
import collections, math, os, struct, sys

def ok_f(x): return math.isfinite(x) and abs(x) < 1e5

def try_mesh(b, p):
    if p + 6 > len(b): return None
    nv, nuv, ng = struct.unpack_from("<HHH", b, p)
    if not (3 <= nv <= 20000 and 1 <= nuv <= 40000 and 1 <= ng <= 16): return None
    q = p + 6
    end = q + nv * 12 + nuv * 8
    if end > len(b): return None
    for v in struct.unpack_from("<%df" % (nv * 3), b, q):
        if not ok_f(v): return None
    q += nv * 12
    for v in struct.unpack_from("<%df" % (nuv * 2), b, q):
        if not ok_f(v): return None
    q += nuv * 8
    nfaces = 0
    for _ in range(ng):
        if q + 4 > len(b): return None
        nf, ln = struct.unpack_from("<HH", b, q)
        if not (1 <= nf <= 40000 and 1 <= ln <= 64): return None
        s = b[q + 4:q + 4 + ln]
        if len(s) < ln or any(c < 32 or c > 126 for c in s): return None
        q += 4 + ln
        if q + nf * 12 > len(b): return None
        for i in range(nf * 6):
            idx = struct.unpack_from("<H", b, q + 2 * i)[0]
            if i % 2 == 0 and idx >= nv: return None
            if i % 2 == 1 and idx >= nuv: return None
        q += nf * 12
        nfaces += nf
    return (nv, nuv, ng, nfaces, q)

stats = collections.defaultdict(lambda: [0, 0, 0])  # files, files_with_mesh, meshes
for root, _, fs in os.walk(sys.argv[1]):
    for f in sorted(fs):
        b = open(os.path.join(root, f), "rb").read()
        ver = round(struct.unpack_from("<f", b, 10)[0], 2)
        found = []
        p = 0x0e
        while p < len(b) - 6:
            r = try_mesh(b, p)
            if r: found.append((p, r)); p = r[4]
            else: p += 1
        s = stats[(b[:4].decode("latin1"), ver)]
        s[0] += 1; s[1] += bool(found); s[2] += len(found)
        if len(sys.argv) > 2 and found: print(f, ver, [(hex(a), r[:4]) for a, r in found][:3])
for k, v in sorted(stats.items(), key=lambda kv: kv[0][1]):
    print(k, "Dateien:", v[0], "mit Mesh:", v[1], "Meshes:", v[2])
