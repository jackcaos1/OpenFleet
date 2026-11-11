#!/usr/bin/env python3
"""Geht eine Load/Save-Routine der Armada.exe ab und liefert die Feldfolge (Typ, Name), mit Schaltern:
  exe_walk.py asm Armada.exe <adresse-hex> [mission=0|1]
Annahmen: Bedingungen nach E/A-Aufrufen (test al,al / je) gelten als erfuellt; der Schalter 0x68b84d (missionSave) wird ausgewertet;
rueckwaerts springende Schleifen werden einmal durchlaufen und markiert; Aufrufe anderer Routinen mit E/A werden eingefuegt."""
import re, sys, pickle
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_fields as ef

PRIM = {'0x4b46c0': ('float', 5), '0x4b44e0': ('int', 4), '0x4b4240': ('bool', 1), '0x4b4820': ('vec3', 9), '0x4b40f0': ('str', 2), '0x4b41b0': ('str', 2),
        '0x4b4620': ('int', 4), '0x4b4580': ('int', 4), '0x4b7420': ('ptr', 8), '0x4b4c10': ('euler', 0),
        '0x4b49f0': ('matrix', 11), '0x4b4b70': ('prjid', 7)}

class Walker:
    def __init__(self, asm, exe):
        self.d, self.secs = ef.load(exe)
        self.ins = ef.parse_asm(asm)
        self.idx = {a: i for i, (a, _) in enumerate(self.ins)}
        self.cat = None

    def has_io(self, addr, seen=None):
        return True

    def walk(self, start, mission, depth=0, trail=None, visited=None):
        out = []
        i = self.idx[start]
        pushes = []
        steps = 0
        loops = set()
        while steps < 20000:
            steps += 1
            a, t = self.ins[i]
            if t.startswith('push'):
                pushes.append(t[5:].strip())
            elif t.startswith('call'):
                tg = t[5:].strip()
                names = [ef.cstr_at(self.d, self.secs, int(p, 16)) for p in pushes[-6:] if p.startswith('0x')]
                names = [n for n in names if n]
                nm = names[-1] if names else None
                if tg in PRIM and nm:
                    sz = None
                    for p in pushes[-6:]:
                        if p in ('0x1', '0x4', '0xc', '0x10', '0x20', '0x28', '0x40', '0x64', '0x80', '0x8', '0x2', '0x14', '0x18', '0x30', '0x34'):
                            sz = p
                    out.append((PRIM[tg][0], nm, sz, '%x' % a))
                elif tg.startswith('0x') and depth < 6 and tg not in PRIM:
                    tgt = int(tg, 16)
                    if tgt in self.idx and tgt != start and nm is None:
                        out.append(('call', tg, None, '%x' % a))
                elif nm:
                    out.append(('virt', nm, tg, '%x' % a))
                pushes = []
            elif t.startswith('ret'):
                break
            elif t.startswith('j'):
                m = re.match(r'(j\w+)\s+0x([0-9a-f]+)', t)
                if m:
                    op, target = m.group(1), int(m.group(2), 16)
                    prev = self.ins[i - 1][1]
                    gate = any('ds:0x68b84d' in self.ins[k][1] for k in range(max(0, i - 4), i))
                    if gate and op in ('jne', 'je'):
                        taken = (op == 'jne') == bool(mission)
                        if taken and target in self.idx:
                            i = self.idx[target]
                            continue
                    elif op == 'jmp' and target in self.idx and target > a:
                        i = self.idx[target]
                        continue
                    elif target < a and op != 'jmp':
                        out.append(('loop-end', hex(target), None, '%x' % a))
            i += 1
        return out

if __name__ == '__main__':
    w = Walker(sys.argv[1], sys.argv[2])
    for r in w.walk(int(sys.argv[3], 16), int(sys.argv[4]) if len(sys.argv) > 4 else 0):
        print(*r)
