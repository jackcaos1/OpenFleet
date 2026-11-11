#!/usr/bin/env python3
"""Explorationshilfe: zeigt u16-laengenpraefixierte ASCII-Strings in SOD-Dateien und die Bytes dazwischen."""
import re, struct, sys

def scan(b):
    out, i = [], 0x0e
    while i < len(b) - 3:
        n = struct.unpack_from("<H", b, i)[0]
        if 2 <= n <= 64 and i + 2 + n <= len(b):
            s = b[i + 2:i + 2 + n]
            if all(32 <= c < 127 for c in s):
                out.append((i, n, s.decode())); i += 2 + n; continue
        i += 1
    return out

for p in sys.argv[1:]:
    b = open(p, "rb").read()
    print(f"=== {p}  {len(b)} B  magic={b[:10].decode('latin1')} ver={struct.unpack_from('<f', b, 10)[0]:.2f}")
    prev = 0x0e
    for off, n, s in scan(b):
        gap = b[prev:off]
        print(f"  gap[{len(gap):3d}] {gap[:28].hex(' ')}{' ..' if len(gap) > 28 else ''}")
        print(f"{off:#06x} str({n}) {s!r}")
        prev = off + 2 + n
    print(f"  tail[{len(b) - prev}] {b[prev:prev + 28].hex(' ')}")
