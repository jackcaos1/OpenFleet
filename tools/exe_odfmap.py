#!/usr/bin/env python3
"""Offset -> ODF-Feldname aus einer Lade-Funktion der Exe (Muster: lea reg,[esi+OFF] ... push STRING ... call 0x5547e0/Lesefunktion).
  exe_odfmap.py asm Armada.exe start_addr [end_addr]"""
import sys, re
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import exe_fields as ef, exe_walk as ew

def run(asm, exe, start, end):
    w = ew.Walker(asm, exe)
    d, secs = w.d, w.secs
    lines = open(asm).read().split('\n')
    out = []
    pend = []   # (off) in Reihenfolge
    on = False
    for ln in lines:
        m = re.match(r'\s*([0-9a-f]+):\t[0-9a-f ]+\t(.*)', ln)
        if not m: continue
        a = int(m.group(1), 16); t = m.group(2).strip()
        if a < start: continue
        if end and a > end: break
        mo = re.search(r'\[(?:esi|ebx|edi)\+0x([0-9a-f]+)\]', t)
        if t.startswith('lea') and mo: pend.append(int(mo.group(1), 16))
        elif t.startswith('mov') and mo and 'DWORD PTR [' in t and t.split(',')[0].endswith(']') is False and False: pass
        ms = re.match(r'push\s+0x([0-9a-f]+)$', t)
        if ms:
            s = ef.cstr_at(d, secs, int(ms.group(1), 16))
            if s and re.match(r'^[A-Za-z_][A-Za-z_0-9]*$', s) and pend:
                out.append((pend[-1], s, a))
    return out

if __name__ == '__main__':
    asm, exe = sys.argv[1:3]
    start = int(sys.argv[3], 16); end = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0
    for off, s, a in run(asm, exe, start, end):
        print('0x%x %s  (%x)' % (off, s, a))
