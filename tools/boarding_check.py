#!/usr/bin/env python3
"""Vergleicht engine/src/game/Boarding.cpp (tools/boarding_dump) mit der Ankunft der Entermannschaft in Armada.exe (BoardingParty 0x55f750, Unicorn).
Aufruf: python3 tools/boarding_check.py [Anzahl] [Startwert]
Vorher: g++ -O1 -std=c++17 -Iengine/src tools/boarding_dump.cpp engine/src/game/Boarding.cpp engine/src/game/Combat.cpp -o /tmp/boarding_dump"""
import random, subprocess, sys, struct
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import health_oracle as H
from unicorn.x86_const import *

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
    cases = []
    for _ in range(n):
        mx = rng.choice([0, 0.5, 50, 100, 400])
        derelict = int(rng.random() < 0.3 and mx > 0)
        crew = 0 if derelict else round(rng.choice([0.5, 5, 30, 50, 100, 399]) * (0.5 + rng.random()), 3)
        crew = min(crew, mx) if mx else crew
        cases.append((crew, mx, derelict, rng.choice([1, 2, 3]), rng.choice([0.75, 1.0, 1.5, 1.75]), round(rng.choice([1, 5, 10, 40]) * (0.5 + rng.random()), 3),
                      rng.choice([0.75, 1.0, 1.5, 1.75]), rng.choice([1, 2]), rng.randint(0, 1)))
    with open('/tmp/boarding_scen.txt', 'w') as f:
        for c in cases:
            f.write('ARRIVE %g %g %d %d %g %g %g %d %d\n' % c)
    cpp = [l.split()[1:] for l in subprocess.run(['/tmp/boarding_dump', '/tmp/boarding_scen.txt'], capture_output=True, text=True).stdout.split('\n') if l.startswith('A ')]
    s = H.Ship()
    m = s.m
    O = s.O
    party = m.heap + 0xa000; race_t = m.heap + 0xa400; race_p = m.heap + 0xa500; rec = m.heap + 0xa600
    calls = []
    def setowner(mm):
        race, team = mm.arg(0), mm.arg(1)
        calls.append((race, team))
        m.w32(O + 0x1a4, race); m.w32(O + 0x194, team)
        mm.ret_stdcall(2)
    m.hook(0x46dff0, setowner)
    m.uc.mem_write(0x68dea5, b'\x01'); m.uc.mem_write(0x68dea4, b'\x00')   # Ton-Pruefung ueberspringen
    spec = dict(shield_max=100, shield_rate=2, crew_max=0, hull_pct=70, sys={k: dict(points=10, hit_pct=0, crew_loss=0, repair_time=30) for k in H.FILE_ORDER})
    race = dict(repair_strength=1, crew_yellow=0.4, crew_red=0.2, repair_yellow=1.5, repair_red=2, weapon_yellow=1, weapon_red=2)
    bad = 0
    for i, (crew, mx, der, tt, ts, pc, ps, pt, en) in enumerate(cases):
        spec['crew_max'] = mx
        s.setup(spec, race)
        m.wf(O + 0x4d8, crew); m.w32(O + 0x4dc, der); m.w32(O + 0x194, tt)
        m.w32(O + 0xbc, 8); m.w32(O + 0xc0, 0); m.w32(O + 0x1bf, 0)
        if der:
            for k in range(5): m.uc.mem_write(O + 0x4e0 + 0x38 * k, b'\x00')
        m.wf(race_t + 0x1f8, ts); m.wf(race_p + 0x1f8, ps); m.w32(O + 0x1a4, race_t)
        m.w32(party + 8, O); m.wf(party + 0xd8, pc); m.w32(party + 0xdc, race_p); m.w32(party + 0xe0, pt)
        # Team-Tabelle: Eintrag je Team zeigt auf einen Datensatz; relation[+0x2c + 4 * Zielteam] == 0 heisst Feind
        for t in (1, 2, 3):
            m.w32(0x633e40 + 4 * t, rec + 0x100 * t)
        for t in (1, 2, 3):
            m.w32(rec + 0x100 * pt + 0x2c + 4 * t, 0 if (t != pt and en) or False else 1) if False else None
        for t in (1, 2, 3):
            m.w32(rec + 0x100 * pt + 0x2c + 4 * t, 1)
        m.w32(rec + 0x100 * pt + 0x2c + 4 * tt, 0 if en else 1)   # Beziehung Mannschaftsteam -> Zielteam
        calls.clear()
        s.call(0x55f750, ecx=party)
        st = s.state()
        owner = 1 if calls and calls[-1][1] == pt and calls[-1][0] == race_p else 0
        race_changed = 1 if (not calls and m.r32(O + 0x1a4) == race_p) else 0
        got = [st['crew'], st['derelict'], owner, race_changed, m.rf(party + 0xd8)]
        want = [float(x) for x in cpp[i]]
        ok = all(abs(a - b) <= 2e-3 * (1 + abs(b)) for a, b in zip(got, want))
        if not ok:
            bad += 1
            if bad <= 8: print('Fall %d %s: exe %s, C++ %s' % (i, cases[i], [round(x, 4) for x in got], want))
    print('%d Faelle, %d Abweichungen' % (n, bad))

def step_check(n, seed):
    """Transporter-Schritt (Craft 0x467050): Abbruchbedingungen, Mannschaftsbildung, Besatzung der Quelle, Schildverzoegerung des Ziels."""
    rng = random.Random(seed)
    cases = []
    for _ in range(n):
        sm = rng.choice([50, 100, 400])
        sc = round(rng.choice([2, 10, 24, 30, 60, 100, 399]) * (0.5 + rng.random()), 3)
        sc = min(sc, sm)
        tm = rng.choice([50, 100, 400])
        tc = min(tm, round(rng.choice([0, 5, 30, 50, 99, 100, 399]) * (0.5 + rng.random()), 3))
        shm = rng.choice([0, 100, 200])
        sh = round(shm * rng.choice([0, 0.005, 0.01, 0.0101, 0.3, 1.0]), 4)
        cases.append((sc, sm, rng.choice([0.2, 0.25]), tc, tm, sh, shm, round(rng.choice([50, 250, 299, 300, 301, 500]) * (0.9 + 0.2 * rng.random()), 3), rng.randint(0, 1)))
    with open('/tmp/boarding_step.txt', 'w') as f:
        for c in cases:
            f.write('STEP %g %g %g %g %g %g %g %g %d\n' % c)
    cpp = [l.split()[1:] for l in subprocess.run(['/tmp/boarding_dump', '/tmp/boarding_step.txt'], capture_output=True, text=True).stdout.split('\n') if l.startswith('T ')]
    s = H.Ship()
    m = s.m
    O = s.O
    T = m.heap + 0x10000; tcls = m.heap + 0x11000; rec = m.heap + 0x12000; stub = m.heap + 0x13000; REC = m.heap + 0x14000
    # Stub fuer den Konstruktor der Entermannschaft (0x55f620): Merker setzen, Besatzung der Quelle mit der echten Funktion 0x4674d0 abziehen
    code = (bytes.fromhex('8b44240c') + b'\xa3' + struct.pack('<I', REC) + bytes.fromhex('c705') + struct.pack('<I', REC + 4) + struct.pack('<I', 1)
            + b'\xb9' + struct.pack('<I', O) + bytes.fromhex('68000000a0'.replace('000000a0', '0000a040')))
    code += b'\xe8' + struct.pack('<i', 0x4674d0 - (stub + len(code) + 5)) + bytes.fromhex('d91d') + struct.pack('<I', REC + 8) + bytes.fromhex('c21c00')
    m.uc.mem_write(stub, code)
    m.uc.mem_write(0x55f620, b'\xe9' + struct.pack('<i', stub - (0x55f620 + 5)))
    m.hook(0x40f4d0, lambda mm: (mm.uc.reg_write(UC_X86_REG_EAX, T), H.ret_cdecl(mm)))
    m.hook(0x430430, lambda mm: (mm.uc.reg_write(UC_X86_REG_EAX, T), H.ret_cdecl(mm)))
    m.uc.mem_write(0x639fa1, b'\x01'); m.uc.mem_write(0x639fa0, b'\x00')
    spec = dict(shield_max=100, shield_rate=2, crew_max=0, hull_pct=70, sys={k: dict(points=10, hit_pct=0, crew_loss=0, repair_time=30) for k in H.FILE_ORDER})
    bad = 0
    for i, (sc, sm, red, tc, tm, sh, shm, dist, en) in enumerate(cases):
        spec['crew_max'] = sm
        s.setup(spec, dict(repair_strength=1, crew_yellow=0.4, crew_red=red, repair_yellow=1.5, repair_red=2, weapon_yellow=1, weapon_red=2))
        m.wf(O + 0x4d8, sc); m.w32(O + 0x4dc, 0); m.w32(O + 0x194, 1); m.w32(O + 0x19c, 1); m.w32(O + 0x1bf, 0); m.w32(O + 0xc0, 0)
        m.w32(O + 0x198, rec); m.w32(O + 0x484, 0); m.w32(O + 0x4d4, 1); m.wf(O + 0x4cc, 0.0); m.w32(O + 0x4d0, 77)
        for k in (0x8c, 0x90, 0x94): m.wf(O + k, 0.0)
        # Ziel
        m.w32(T + 0xe0, tcls); m.wf(tcls + 0x21c, 3.0)
        m.wf(T + 0x4d8, tc); m.wf(T + 0x4c8, tm); m.w32(T + 0x194, 2 if en else 1); m.w32(T + 0x19c, 2 if en else 1)
        m.wf(T + 0x468, (sh / shm) if shm else 0.0); m.wf(T + 0x4c4, 0.0); m.w32(T + 0xbc, 4); m.w32(T + 0x1bf, 0)
        m.wf(T + 0x8c, dist); m.wf(T + 0x90, 0.0); m.wf(T + 0x94, 0.0)
        for t in (1, 2): m.w32(rec + 0x2c + 4 * t, 1)
        m.w32(rec + 0x2c + 4 * 2, 0 if en else 1)
        m.w32(REC + 4, 0); m.wf(REC, 0.0); m.wf(REC + 8, 0.0)
        s.call(0x467050, args=[H.f2i(0.01)])
        created = m.r32(REC + 4)
        stop = 0 if m.r32(O + 0x4d4) & 0xff else 1
        got = [stop, created, m.rf(REC + 8) if created else 0.0, 1 if created and m.rf(REC) > 0.5 else 0, m.rf(O + 0x4d8), m.rf(T + 0x4c4), m.r32(O + 0x4dc) & 0xff]
        want = [float(x) for x in cpp[i]]
        if not all(abs(a - b) <= 2e-3 * (1 + abs(b)) for a, b in zip(got, want)):
            bad += 1
            if bad <= 8: print('Schritt %d %s: exe %s, C++ %s' % (i, cases[i], [round(x, 4) for x in got], want))
    print('%d Transporterschritte, %d Abweichungen' % (n, bad))

if __name__ == '__main__':
    if len(sys.argv) > 3 and sys.argv[3] == 'step':
        step_check(int(sys.argv[1]), int(sys.argv[2]))
    else:
        main()
