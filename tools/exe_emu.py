#!/usr/bin/env python3
"""Pfadsuche durch Load/Save-Routinen der Armada.exe mit Abgleich gegen die Typfolge eines echten Karten-Objekts.
Nutzung (als Modul): find_path(w, funcs_io, start, expected_types, mission=1) -> Entscheidungen
                    run(w, funcs_io, start, decisions, mission=0)             -> Feldliste
Bedingte Spruenge sind Entscheidungspunkte (Adresse, Besuch Nr. -> genommen?); der Schalter missionSave (0x68b84d) wird festgelegt."""
import re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_walk as ew
import exe_fields as ef

TYPES = {'float': 5, 'int': 4, 'bool': 1, 'vec3': 9, 'str': 2, 'ptr': 8, 'matrix': 11, 'prjid': 7}
COND = ('je', 'jne', 'jl', 'jle', 'jg', 'jge', 'ja', 'jae', 'jb', 'jbe', 'jz', 'jnz', 'js', 'jns')

class Emu:
    def __init__(self, w, io_funcs):
        self.w = w
        self.io = io_funcs  # Menge von Funktionsadressen, die (transitiv) Felder schreiben

    def is_gate(self, i):
        w = self.w
        m = None
        ti = i - 1
        for k in range(i - 1, max(0, i - 4), -1):
            m = re.match(r'test\s+(\w+),(\w+)$', w.ins[k][1])
            if m:
                ti = k
                break
        reg = m.group(1) if m and m.group(1) == m.group(2) else None
        i = ti + 1
        if reg is None:
            return any('ds:0x68b84d' in w.ins[k][1] for k in range(max(0, i - 3), i))
        for k in range(i - 2, max(0, i - 40), -1):
            tt = w.ins[k][1]
            if tt.startswith('call') or tt.startswith('j'):
                if tt.startswith('call'):
                    return False
                continue
            mm = re.match(r'mov\s+(\w+),(.*)$', tt)
            if mm and mm.group(1) == reg:
                return 'ds:0x68b84d' in mm.group(2)
            if re.match(r'(and|or|xor|setne|sete)\s+%s,' % reg, tt):
                pass
        return False

    def step_fields(self, kind):
        return {'euler': [5, 5, 9, 9, 9]}.get(kind) or ([TYPES[kind]] if kind in TYPES else [])

    def run(self, start, mission, expected=None, decisions=None, limit=4000, longest=False):
        """DFS. decisions: dict (addr)->list of bools zum Wiederabspielen. Liefert (fields, decisions) oder None."""
        w = self.w
        best = None
        found = None
        # Zustand: (i, stack, pushes, produced, trail, visits, loopcount)
        init = (w.idx[start], (), (), (), (), (), ())
        last_fmt = None
        stack = [init]
        iters = 0
        while stack:
            iters += 1
            if iters > (30000 if longest else 400000) or len(stack) > 20000:
                return found
            i, cs, pushes, prod, trail, visits, loops = stack.pop()
            n = 0
            while True:
                n += 1
                if n > 6000:
                    break
                a, t = w.ins[i]
                if t.startswith('push'):
                    pushes = pushes + (t[5:].strip(),)
                elif t.startswith('call'):
                    tg = t[5:].strip()
                    names = [ef.cstr_at(w.d, w.secs, int(p, 16)) for p in pushes[-6:] if p.startswith('0x')]
                    names = [x for x in names if x]
                    nm = names[-1] if names else None
                    if not tg.startswith('0x') and nm:
                        last_fmt = nm
                    if tg in ew.PRIM and not nm and last_fmt:
                        nm = last_fmt
                    if tg in ew.PRIM and (nm or tg == '0x4b4c10'):
                        kind = ew.PRIM[tg][0]
                        ts = self.step_fields(kind)
                        sz = None
                        for pi in range(len(pushes) - 1, -1, -1):
                            if pushes[pi].startswith('0x') and ef.cstr_at(w.d, w.secs, int(pushes[pi], 16)) == nm and pi + 1 < len(pushes) and pushes[pi + 1].startswith('0x'):
                                sz = int(pushes[pi + 1], 16)
                                break
                        prod = prod + ((kind, nm, a, sz),)
                        if expected is not None:
                            flat = [x for kk, _, _, _ in prod for x in self.step_fields(kk)]
                            if flat != expected[:len(flat)]:
                                break
                    elif tg.startswith('0x') and int(tg, 16) in self.io and int(tg, 16) != start:
                        cs = cs + (w.idx[a] + 1,)
                        i = w.idx[int(tg, 16)]
                        pushes = ()
                        continue
                    pushes = ()
                elif t.startswith('ret'):
                    if cs:
                        i = cs[-1]
                        cs = cs[:-1]
                        pushes = ()
                        continue
                    flat = [x for kk, _, _, _ in prod for x in self.step_fields(kk)]
                    if expected is None or flat == expected:
                        if not longest:
                            return prod, trail
                        if found is None or len(prod) > len(found[0]):
                            found = (prod, trail)
                    break
                else:
                    m = re.match(r'(j\w+)\s+0x([0-9a-f]+)', t)
                    if m:
                        op, target = m.group(1), int(m.group(2), 16)
                        gate = self.is_gate(i)
                        if op == 'jmp':
                            if target in w.idx:
                                i = w.idx[target]
                                continue
                        elif gate:
                            taken = (op == 'jne') == bool(mission)
                            if taken and target in w.idx:
                                i = w.idx[target]
                                continue
                        elif op in COND and target in w.idx:
                            back = target < a
                            if back:
                                # Schleife: Zaehler aus einem 'mov DWORD PTR [ebp-x],imm' davor
                                cnt = 1
                                for k in range(i - 1, max(0, i - 40), -1):
                                    mm = re.match(r'mov\s+DWORD PTR \[ebp-0x[0-9a-f]+\],(0x[0-9a-f]+)$', w.ins[k][1])
                                    if mm and w.ins[k][0] >= target - 0x40:
                                        cnt = int(mm.group(1), 16)
                                        break
                                done = dict(loops).get(a, 0)
                                if done + 1 < cnt:
                                    loops = tuple((x, y) for x, y in loops if x != a) + ((a, done + 1),)
                                    i = w.idx[target]
                                    continue
                                loops = tuple((x, y) for x, y in loops if x != a)
                            else:
                                key = a
                                vis = dict(visits).get(key, 0)
                                visits2 = tuple((x, y) for x, y in visits if x != key) + ((key, vis + 1),)
                                if decisions is not None:
                                    seq = decisions.get(a, [])
                                    take = seq[vis] if vis < len(seq) else False
                                    if take:
                                        trail2 = trail
                                        i = w.idx[target]
                                        visits = visits2
                                        continue
                                    visits = visits2
                                else:
                                    # beide Moeglichkeiten; zuerst Durchfallen, taken auf den Stack
                                    stack.append((w.idx[target], cs, pushes, prod, trail + ((a, vis, True),), visits2, loops))
                                    trail = trail + ((a, vis, False),)
                                    visits = visits2
                i += 1
        return found

def io_function_set(w):
    """Funktionen, die direkt oder indirekt PRIM-Aufrufe enthalten (ausser den Primitiven selbst)."""
    import pickle
    starts = [i for i in range(len(w.ins) - 1) if w.ins[i][1] == 'push   ebp' and w.ins[i + 1][1] == 'mov    ebp,esp']
    bounds = {w.ins[s][0]: (s, starts[k + 1] if k + 1 < len(starts) else len(w.ins)) for k, s in enumerate(starts)}
    calls = {}
    for a, (s, e) in bounds.items():
        calls[a] = [int(w.ins[i][1][5:].strip(), 16) for i in range(s, e) if w.ins[i][1].startswith('call') and w.ins[i][1][5:].strip().startswith('0x')]
    prims = {int(p, 16) for p in ew.PRIM}
    io = set(a for a, c in calls.items() if any(x in prims for x in c) and a not in prims)
    ch = True
    while ch:
        ch = False
        for a, c in calls.items():
            if a not in io and a not in prims and any(x in io for x in c):
                io.add(a)
                ch = True
    return io - prims
