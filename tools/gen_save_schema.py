#!/usr/bin/env python3
"""Erzeugt aus der Armada.exe die Objekt-Feldfolgen (Karten-Modus und Spielstand-Modus) je ODF.
  gen_save_schema.py asm Armada.exe out.json"""
import sys, glob, json, collections
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import bzn_tool as b, exe_emu as ee, exe_walk as ew, exe_vtables as vt
from exe_classes import label_of

MAP = {'wingman': 'Wingman', 'sensor': 'SensorArray', 'mining': 'MiningStation', 'research': 'ResearchStation', 'planet_obj': 'Planet',
       'background_obj': 'BackgroundObject', 'nebula_obj': 'Nebula', 'blackhole_obj': 'BlackHole', 'wormhole_obj': 'WormHole'}
SYS = [('bool', '%sActive', 1), ('int', '%sHitPoints', 4), ('float', '%sAdjustedHitPercent', 4), ('float', '%sModifiedRepairTime', 4),
       ('float', '%sRepairTimeLeft', 4), ('float', '%sDisableTimeLeft', 4), ('bool', '%PermanentDisable', 1)]
SYSNAMES = ['shieldGenerator', 'engines', 'weapons', 'lifeSupport', 'sensors']

def fields(prod):
    out = []
    for kind, name, _, sz in prod:
        out.append([kind, name, sz])
    return out

def full_fields(prod):
    out = []
    for kind, name, _, sz in prod:
        if name == 'hullHitPercent':
            for sn in SYSNAMES:
                for k, n, s in SYS:
                    out.append([k, (n % sn) if '%s' in n else n.replace('%', sn), s])
        out.append([kind, name, sz])
    return out

if __name__ == '__main__':
    asm, exe, outp = sys.argv[1:4]
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
    res = {}
    for odf, c in sorted(sigs.items()):
        lab = label_of(odf, 'work/assets') or '?'
        cls = lower.get(lab.lower()) or MAP.get(lab.lower())
        if not cls: continue
        fn = vt.slot(d, secs, cl[cls], 38)
        if fn not in w.idx: continue
        entry = {'class': cls, 'fn': hex(fn), 'variants': []}
        for sig, n in c.most_common():
            if n < 1: continue
            extra = 0
            if cls == 'ResearchStation':
                lead = 0
                while lead < len(sig) and sig[lead] == 8: lead += 1
                extra = max(0, lead - 1)  # Schleife ueber die Forschungsplaetze (Zeiger, je ODF 4 oder 5); die Emulation zaehlt nur einen
            r = em.run(fn, 1, expected=list(sig[extra:]))
            if not r:
                continue
            dec = {}
            for a, vis, tk in r[1]: dec.setdefault(a, []).append(tk)
            full = em.run(fn, 0, decisions=dec)
            pre = [['ptr', 'undefptr%d' % i, 4] for i in range(extra)]
            entry['variants'].append({'count': n, 'types': list(sig), 'mission': pre + fields(r[0]), 'full': (pre + full_fields(full[0])) if full else None})
        if entry['variants']: res[odf] = entry
    json.dump(res, open(outp, 'w'), indent=0)
    print(len(res), 'ODFs,', sum(len(e['variants']) for e in res.values()), 'Varianten')
