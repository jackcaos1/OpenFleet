#!/usr/bin/env python3
"""Zieht aus der Disassembly (objdump -d -M intel Armada.exe) die Feld-E/A-Aufrufe (Datei, Zeiger, Groesse, Name) je Funktion.
  exe_fields.py asm.txt Armada.exe <startadresse-hex>   listet die Felder der Funktion in Aufrufreihenfolge
"""
import re, struct, sys

def load(exe):
    d = open(exe, 'rb').read()
    secs = []
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    off = pe + 24 + opt
    for i in range(n):
        name, vs, va, rs, ro = struct.unpack_from('8sIIII', d, off + 40 * i)
        secs.append((va + base, vs, ro, rs))
    return d, secs

def cstr_at(d, secs, va):
    for v, vs, ro, rs in secs:
        if v <= va < v + vs:
            o = ro + va - v
            e = d.find(b'\0', o)
            s = d[o:e]
            if 0 < len(s) < 80 and all(32 <= c < 127 for c in s):
                return s.decode()
    return None

def parse_asm(path):
    ins = []  # (addr, text)
    rx = re.compile(r'^\s*([0-9a-f]+):\t[0-9a-f ]+\t(.*)$')
    for line in open(path, errors='replace'):
        m = rx.match(line.rstrip('\n'))
        if m:
            ins.append((int(m.group(1), 16), m.group(2).strip()))
    return ins

def fields(ins, d, secs, start, end=None):
    i = next(k for k, (a, _) in enumerate(ins) if a >= start)
    out = []
    pushes = []
    depth_end = None
    while i < len(ins):
        a, t = ins[i]
        if end and a >= end:
            break
        if t.startswith('push'):
            pushes.append((a, t[5:].strip()))
        elif t.startswith('call'):
            tgt = t[5:].strip()
            names = []
            for _, p in pushes[-6:]:
                if p.startswith('0x'):
                    s = cstr_at(d, secs, int(p, 16))
                    if s:
                        names.append(s)
            out.append((a, tgt, names[-1] if names else None, [p for _, p in pushes[-5:]]))
            pushes = []
        elif t.startswith('ret') and not end:
            break
        i += 1
    return out

if __name__ == '__main__':
    asm, exe, start = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    end = int(sys.argv[4], 16) if len(sys.argv) > 4 else None
    d, secs = load(exe)
    ins = parse_asm(asm)
    for a, tgt, name, ps in fields(ins, d, secs, start, end):
        if name:
            print('%x %-18s %-28s %s' % (a, tgt, name, ' '.join(ps)))
