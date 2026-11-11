#!/usr/bin/env python3
"""Expandierte Feldfolge einer Klasse (Slot 38 der Vtable = Load/Save) mit eingefuegten Basisklassen-Aufrufen.
  exe_chain.py asm Armada.exe <Klasse> [mission=0|1]"""
import sys, struct
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_vtables as vt, exe_walk as ew

SLOT = 38

def expand(w, funcs, start, mission, depth=0, stack=()):
    out = []
    for kind, a, b, addr in w.walk(start, mission):
        if kind == 'call':
            t = int(a, 16)
            if t in funcs and t not in stack and depth < 8:
                out.append(('{', funcs[t], None, addr))
                out += expand(w, funcs, t, mission, depth + 1, stack + (start,))
                out.append(('}', funcs[t], None, addr))
            continue
        out.append((kind, a, b, addr))
    return out

TYPE = {'float': 5, 'int': 4, 'bool': 1, 'vec3': 9, 'str': 2, 'ptr': 8}

def types(seq):
    t = []
    for kind, a, b, addr in seq:
        if kind == 'euler':
            t += [5, 5, 9, 9, 9]
        elif kind in TYPE:
            t.append(TYPE[kind])
    return t


if __name__ == '__main__':
    w = ew.Walker(sys.argv[1], sys.argv[2])
    d, secs = vt.load(sys.argv[2])
    cl = vt.classes(d, secs)
    funcs = {}
    for name, v in cl.items():
        try:
            f = vt.slot(d, secs, v, SLOT)
        except Exception:
            continue
        if f in w.idx:
            funcs.setdefault(f, name)
    start = vt.slot(d, secs, cl[sys.argv[3]], SLOT)
    mission = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    ind = 0
    for kind, a, b, addr in expand(w, funcs, start, mission):
        if kind == '}': ind -= 1
        print('  ' * ind + ' '.join(str(x) for x in (kind, a, b) if x is not None))
        if kind == '{': ind += 1
