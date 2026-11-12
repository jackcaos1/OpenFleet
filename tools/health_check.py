#!/usr/bin/env python3
"""Vergleicht engine/src/game/Combat.cpp (tools/health_dump) mit Armada.exe (Unicorn): Zufallsszenarien aus Treffern, Zeitschritten
und Besatzungsaenderungen; nach jedem Schritt muessen Schild, Besatzung, Systempunkte, Trefferanteile und Reparaturzeiten uebereinstimmen.
Aufruf (Unicorn aus einem venv): python3 tools/health_check.py [Anzahl] [Startwert]
Vorher: g++ -O1 -std=c++17 -Iengine/src tools/health_dump.cpp engine/src/game/Combat.cpp -o /tmp/health_dump"""
import random, subprocess, sys, struct
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import health_oracle as H

RACES = [(1.5, 0.4, 0.2, 1.5, 2.0, 1.0, 2.0), (0.75, 0.4, 0.2, 2.0, 5.0, 1.0, 0.75), (1.0, 0.5, 0.25, 1.5, 2.0, 1.5, 2.0), (1.25, 0.5, 0.25, 1.0, 1.0, 1.5, 5.0)]
RACE_KEYS = ['repair_strength', 'crew_yellow', 'crew_red', 'repair_yellow', 'repair_red', 'weapon_yellow', 'weapon_red']

def make(rng):
    cfg = (rng.choice([1.0, 0.8, 0.5]), rng.choice([0, 0, 5]), 1)
    shield_max = rng.choice([0, 80, 150, 300]); rate = rng.choice([0, 1.5, 3.0]); delay = rng.choice([0, 3.0, 5.0])
    crew = rng.choice([0, 100, 400]); life_loss = rng.choice([2.0, 4.0]); crit = rng.choice([0.5, 2.5, 5.0, 15.5])
    sysp = []
    left = 100.0
    hull = rng.choice([0, 40, 70])
    for i in range(5):
        pts = rng.choice([0, 1, 5, 10, 20])
        pct = round(rng.choice([0, 2.0, 2.5, 7.5, 10.0]) if pts else 0, 1)
        sysp.append((pts, pct, rng.choice([0, 2, 3, 5]), rng.choice([1, 5, 10, 60])))
    race = rng.choice(RACES)
    ops = []
    for _ in range(rng.randint(10, 60)):
        k = rng.random()
        if k < 0.5:
            ops.append(('HIT', rng.randint(0, 6), round(rng.choice([0, 0.3, 1, 5, 20, 60, 150]) * rng.random() * 2, 3), rng.choice([0, 30, 60]),
                        rng.choice([0, 0.3, 1.0]), rng.choice([0, 0.05, 0.5]), int(rng.random() < 0.15)))
        elif k < 0.85:
            ops.append(('STEP', round(rng.choice([0.01, 0.1, 0.5, 1.0, 3.0, 12.0]) * (0.5 + rng.random()), 4)))
        elif k < 0.90:
            ops.append(('ROLL',))
        elif k < 0.95:
            ops.append(('CREW', round(rng.choice([0, 0.5, 1.0, 5.0, 50.0, 400.0, 900.0]) * (0.5 + rng.random()), 3)))
        else:
            ops.append(('SELFD', int(rng.random() < 0.5)))
    return dict(cfg=cfg, spec=(shield_max, rate, delay, crew, hull, life_loss, crit), sys=sysp, race=race, ops=ops)

def write_cpp(scs, path):
    with open(path, 'w') as f:
        for sc in scs:
            f.write('CFG %g %d %d\n' % sc['cfg'])
            f.write('SPEC %g %g %g %g %g %g %g\n' % sc['spec'])
            for i, s in enumerate(sc['sys']):
                f.write('SYS %d %d %g %g %g\n' % ((i,) + s))
            f.write('RACE %g %g %g %g %g %g %g\n' % sc['race'])
            f.write('GO\n')
            for op in sc['ops']:
                if op[0] == 'HIT': f.write('HIT %d %g %g %g %g %d\n' % op[1:])
                elif op[0] == 'STEP': f.write('STEP %g\n' % op[1])
                elif op[0] == 'ROLL': f.write('ROLL\n')
                elif op[0] == 'CREW': f.write('CREW %g\n' % op[1])
                else: f.write('SELFD %d\n' % op[1])
            f.write('END\n')

def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]

class Orig:
    def __init__(self):
        self.s = H.Ship()
        s = self.s; m = s.m
        s.m.hook(0x464910, lambda mm: mm.ret_stdcall(0))   # vtable+0x110 (unbekannt, ohne Folgen auf die Gesundheit?) wird unten geprueft
        self.pk = m.heap + 0x8100
    def run(self, sc):
        s = self.s; m = s.m
        prot, bonus, use = sc['cfg']
        m.wf(0x5fbc08, prot); m.w32(0x5fbc00, use)
        names = H.FILE_ORDER
        spec = dict(shield_max=sc['spec'][0], shield_rate=sc['spec'][1], crew_max=sc['spec'][3], hull_pct=sc['spec'][4], crit_pct=sc['spec'][6], sys={})
        for i, n in enumerate(names):
            pts, pct, loss, rep = sc['sys'][i]
            if use and pts > 0: pts += bonus
            spec['sys'][n] = dict(points=pts, hit_pct=pct, crew_loss=loss, repair_time=rep)
        race = dict(zip(RACE_KEYS, sc['race']))
        # frischer Objektzustand
        for off in (0x4c4, 0x4cc, 0x4d4, 0x4dc, 0x600, 0x1bf, 0x678):
            m.w32(s.O + off, 0)
        m.w32(s.O + 0x690, 0xffffffff)
        s.setup(spec, race, shield_delay=sc['spec'][2], life_loss=sc['spec'][5])
        out = [s.state()]
        for op in sc['ops']:
            if op[0] == 'HIT':
                _, idx, dmg, thr, scm, hcm, ign = op
                pk = self.pk
                for o in range(0, 0x20, 4): m.w32(pk + o, 0)
                m.wf(pk, dmg); m.w32(pk + 4, idx); m.wf(pk + 8, thr); m.wf(pk + 0xc, scm); m.wf(pk + 0x10, hcm); m.w32(pk + 0x14, 1 if ign else 0)
                s.call(0x465100, args=[pk, 0])
            elif op[0] == 'ROLL':
                res = []
                out_p = m.heap + 0x8200
                for roll in range(100):
                    s.roll = roll
                    s.call(0x464dd0, args=[out_p])
                    res.append(m.r32(out_p + 4))
                out.append({'roll': res})
                continue
            elif op[0] == 'STEP':
                s.call(0x465e80, args=[H.f2i(op[1])])
            elif op[0] == 'CREW':
                m.wf(s.O + 0x4d8, op[1])
                s.call(0x466ca0)
            else:
                m.w32(s.O + 0x690, 5 if op[1] else 0xffffffff)
            out.append(s.state())
        return out

def parse_cpp(text):
    scs, cur = [], None
    for ln in text.split('\n'):
        if ln == 'BEGIN': cur = []
        elif ln == 'END': scs.append(cur)
        elif ln.startswith('R '):
            cur.append({'roll': [int(x) for x in ln[2:].split()]})
        elif ln.startswith('S '):
            parts = ln[2:].split('|')
            h = parts[0].split()
            st = dict(shield=float(h[0]), delay=float(h[1]), crew=float(h[2]), carry=float(h[3]), derelict=int(h[4]), exploded=int(h[5]), crit=float(h[6]), sys={})
            for n, p in zip(H.FILE_ORDER, parts[1:]):
                q = p.split()
                st['sys'][n] = dict(active=int(q[0]), points=int(q[1]), hit=float(q[2]), per=float(q[3]), count=float(q[4]), timer=float(q[5]))
            cur.append(st)
    return scs

def close(a, b):
    return abs(a - b) <= 2e-3 * (1 + max(abs(a), abs(b)))

def diff(a, b):
    out = []
    if 'roll' in a or 'roll' in b:
        return [] if a.get('roll') == b.get('roll') else ['Trefferwurf: %s != %s' % (a.get('roll'), b.get('roll'))]
    for k in ('shield', 'delay', 'crew', 'carry', 'crit'):
        if not close(a[k], b[k]): out.append('%s: exe %g, C++ %g' % (k, a[k], b[k]))
    for k in ('derelict', 'exploded'):
        if a[k] != b[k]: out.append('%s: exe %d, C++ %d' % (k, a[k], b[k]))
    for n in H.FILE_ORDER:
        x, y = a['sys'][n], b['sys'][n]
        for k in ('active', 'points'):
            if x[k] != y[k]: out.append('%s.%s: exe %d, C++ %d' % (n, k, x[k], y[k]))
        for k in ('hit', 'per', 'count', 'timer'):
            if not close(x[k], y[k]): out.append('%s.%s: exe %g, C++ %g' % (n, k, x[k], y[k]))
    return out

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    rng = random.Random(seed)
    scs = [make(rng) for _ in range(n)]
    write_cpp(scs, '/tmp/health_scen.txt')
    cpp = parse_cpp(subprocess.run(['/tmp/health_dump', '/tmp/health_scen.txt'], capture_output=True, text=True).stdout)
    orig = Orig()
    bad = steps = 0
    for i, sc in enumerate(scs):
        exe = orig.run(sc)
        for j, (a, b) in enumerate(zip(exe, cpp[i])):
            steps += 1
            d = diff(a, b)
            if d:
                bad += 1
                if bad <= 8:
                    print('Szenario %d Schritt %d (%s): %s' % (i, j, sc['ops'][j - 1] if j else 'Start', '; '.join(d[:6])))
                break
    print('%d Szenarien, %d Schritte geprueft, %d Szenarien mit Abweichung' % (n, steps, bad))

if __name__ == "__main__":
    main()
