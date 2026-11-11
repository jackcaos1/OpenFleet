#!/usr/bin/env python3
"""RTTI-Auswertung der Armada.exe: Klassen -> Vtable -> Slot-Funktionen.
  exe_vtables.py Armada.exe list                 alle Klassen mit Vtable-Adresse
  exe_vtables.py Armada.exe slot <Klasse> <n>    Funktionsadresse in Slot n (dezimal)
"""
import struct, sys, re

def load(exe):
    d = open(exe, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    off = pe + 24 + opt
    secs = []
    for i in range(n):
        name, vs, va, rs, ro = struct.unpack_from('8sIIII', d, off + 40 * i)
        secs.append((name.rstrip(b'\0').decode(), va + base, vs, ro, rs))
    return d, secs

def va2off(secs, va):
    for _, v, vs, ro, rs in secs:
        if v <= va < v + max(vs, rs):
            return ro + va - v
    return None

def classes(d, secs):
    """Klassenname -> Vtable-VA (ueber Complete Object Locator)."""
    typedesc = {}
    for m in re.finditer(rb'\.\?AV([A-Za-z0-9_]+)@@\x00', d):
        o = m.start() - 8  # TypeDescriptor: vftable ptr, spare, name
        for _, v, vs, ro, rs in secs:
            if ro <= o < ro + rs:
                typedesc[v + o - ro] = m.group(1).decode()
    cols = {}
    for _, v, vs, ro, rs in secs:
        for o in range(ro, ro + rs - 12, 4):
            sig, off, cd, td = struct.unpack_from('<IIII', d, o)
            if sig == 0 and off == 0 and td in typedesc:
                cols[v + o - ro] = typedesc[td]
    out = {}
    for _, v, vs, ro, rs in secs:
        for o in range(ro, ro + rs - 4, 4):
            p = struct.unpack_from('<I', d, o)[0]
            if p in cols and cols[p] not in out:
                out[cols[p]] = v + o - ro + 4
    return out

def slot(d, secs, vt, n):
    return struct.unpack_from('<I', d, va2off(secs, vt + 4 * n))[0]

if __name__ == '__main__':
    d, secs = load(sys.argv[1])
    cl = classes(d, secs)
    if sys.argv[2] == 'list':
        for k, v in sorted(cl.items()):
            print('%-32s %x' % (k, v))
    else:
        print('%x' % slot(d, secs, cl[sys.argv[3]], int(sys.argv[4])))
