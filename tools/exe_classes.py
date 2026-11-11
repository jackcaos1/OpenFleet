#!/usr/bin/env python3
"""Pfadsuche fuer alle Objektklassen der Karten; schreibt docs/formats/savegame-classes.txt
  exe_classes.py asm Armada.exe"""
import sys, glob, re, os, collections
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import bzn_tool as b, exe_emu as ee, exe_walk as ew, exe_vtables as vt

def label_of(odf, root):
    seen = set()
    def rd(n):
        n = n.lower()
        if n in seen: return None
        seen.add(n)
        p = os.path.join(root, n if n.endswith('.odf') else n + '.odf')
        if not os.path.exists(p): return None
        txt = open(p, errors='replace').read()
        m = re.search(r'^\s*classLabel\s*=\s*"([^"]+)"', txt, re.M | re.I)
        if m: return m.group(1)
        for inc in re.findall(r'#include\s+"([^"]+)"', txt):
            r = rd(inc)
            if r: return r
        return None
    return rd(odf)

if __name__ == '__main__':
    asm, exe = sys.argv[1], sys.argv[2]
    w = ew.Walker(asm, exe)
    io = ee.io_function_set(w)
    em = ee.Emu(w, io)
    d, secs = vt.load(exe)
    cl = vt.classes(d, secs)
    lower = {k.lower(): k for k in cl}
    sigs = collections.defaultdict(collections.Counter)
    for f in sorted(glob.glob('originalFiles/bzn/*.bzn')):
        data = open(f, 'rb').read()
        m = data.find(b"true\r\n"); T = b.tokenize(data[m + 6:])
        st = [k for k in range(len(T)) if b.is_obj_start(T, k)]
        for a, e in zip(st, st[1:]):
            sigs[b.cstr(T[a][1])][tuple(t for t, _ in T[a + 7:e])] += 1
    out = []
    for odf, c in sorted(sigs.items()):
        sig, n = c.most_common(1)[0]
        lab = label_of(odf, 'work/assets') or '?'
        cls = lower.get(lab.lower())
        cls = cls or {'wingman': 'Wingman', 'sensor': 'SensorArray', 'mining': 'MiningStation', 'research': 'ResearchStation', 'planet_obj': 'Planet', 'background_obj': 'BackgroundObject', 'nebula_obj': 'Nebula', 'blackhole_obj': 'BlackHole', 'wormhole_obj': 'WormHole'}.get(lab.lower())
        res = 'KEINE KLASSE'
        if cls:
            fn = vt.slot(d, secs, cl[cls], 38)
            r = em.run(fn, 1, expected=list(sig)) if fn in w.idx else None
            res = 'ok %s' % hex(fn) if r else 'KEIN PFAD %s' % hex(fn)
        out.append('%-12s %-14s %-14s n=%-5d len=%-3d %s' % (odf, lab, cls, n, len(sig), res))
    print('\n'.join(out))
