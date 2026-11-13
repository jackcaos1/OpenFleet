#!/usr/bin/env python3
"""Aufrufgraph und Disassembly-Ansicht der Armada.exe (Hilfe fuer die Analyse, nicht Teil des Spiels).
  objdump -d -M intel Armada.exe > arm.asm
  exe_cg.py arm.asm func <addr>        Funktion, die <addr> enthaelt
  exe_cg.py arm.asm show <addr> [n]    n Befehle ab <addr>
  exe_cg.py arm.asm callers <addr>...  Funktionen mit direktem Aufruf
  exe_cg.py arm.asm field <hexoff>     Funktionen, die [reg+off] benutzen
  exe_cg.py arm.asm str <exe> <text>   Stellen, die den String pushen
"""
import bisect, re, struct, sys

def load(path):
    ins = []
    for l in open(path):
        m = re.match(r'^\s+([0-9a-f]{6,8}):\t([0-9a-f ]+?)\t(.*)', l)
        if m: ins.append((int(m.group(1), 16), m.group(3).rstrip()))
    return ins

def starts(ins):
    s = set()
    for a, t in ins:
        m = re.match(r'call\s+0x([0-9a-f]+)$', t)
        if m: s.add(int(m.group(1), 16))
    for i in range(1, len(ins)):
        if ins[i][1].startswith('push   ebp') and ins[i-1][1].startswith(('nop', 'int3', 'ret', 'xchg')): s.add(ins[i][0])
    return sorted(s)

def main():
    ins = load(sys.argv[1]); addrs = [a for a, _ in ins]; st = starts(ins)
    func = lambda a: st[max(0, bisect.bisect_right(st, a) - 1)]
    def show(x, n):
        i = bisect.bisect_left(addrs, x)
        for a, t in ins[i:i+n]: print('  %x  %s' % (a, t[:90]))
    cmd = sys.argv[2]
    if cmd == 'show': show(int(sys.argv[3], 16), int(sys.argv[4]) if len(sys.argv) > 4 else 80)
    elif cmd == 'func':
        f = func(int(sys.argv[3], 16)); i = st.index(f); e = st[i+1] if i + 1 < len(st) else f + 400
        show(f, bisect.bisect_left(addrs, e) - bisect.bisect_left(addrs, f))
    elif cmd == 'callers':
        for x in sys.argv[3:]:
            r = sorted({func(a) for a, t in ins if t.startswith('call') and t.endswith('0x' + x.lower())})
            print(x, '<-', [hex(c) for c in r])
    elif cmd == 'field':
        off = int(sys.argv[3], 16); r = {}
        for a, t in ins:
            if re.search(r'\+0x%x\]' % off, t): r.setdefault(func(a), []).append(hex(a))
        for f, v in sorted(r.items()): print(hex(f), v[:3])
    elif cmd == 'str':
        d = open(sys.argv[3], 'rb').read(); k = b'\0' + sys.argv[4].encode() + b'\0'
        pe = struct.unpack_from('<I', d, 0x3c)[0]; n = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
        base = struct.unpack_from('<I', d, pe + 24 + 28)[0]; off = pe + 24 + opt
        secs = [struct.unpack_from('8sIIII', d, off + 40 * i) for i in range(n)]
        pos = 0
        while True:
            i = d.find(k, pos)
            if i < 0: break
            pos = i + 1
            for nm, vs, va, rs, ro in secs:
                if ro <= i + 1 < ro + vs:
                    pat = '0x%x' % (va + base + i + 1 - ro)
                    for a, t in ins:
                        if t.startswith('push') and t.endswith(pat): print(pat, hex(a), hex(func(a)))
main()
