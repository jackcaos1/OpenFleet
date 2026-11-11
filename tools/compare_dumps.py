#!/usr/bin/env python3
"""Vergleicht zwei Dump-Dateien (C++ vs. Python). Zahlen ohne Punkt muessen exakt gleich sein, 'sum=' mit Toleranz."""
import re
import sys


def load(p):
    return [l.rstrip("\n") for l in open(p, encoding="utf-8") if l.strip()]


a, b = load(sys.argv[1]), load(sys.argv[2])
bad = 0
if len(a) != len(b):
    print("Zeilenzahl verschieden: %d vs %d" % (len(a), len(b)))
    bad += 1
for x, y in zip(a, b):
    xs, ys = x.split(), y.split()
    ok = len(xs) == len(ys)
    if ok:
        for p, q in zip(xs, ys):
            if p.startswith("sum="):
                pa = [float(t) for t in p[4:].split(",")]
                qa = [float(t) for t in q[4:].split(",")]
                if any(abs(m - n) > 1e-3 * max(1.0, abs(n)) + 0.05 for m, n in zip(pa, qa)):
                    ok = False
            elif p.startswith("acc="):
                if abs(float(p[4:]) - float(q[4:])) > 0.01:
                    ok = False
            elif p.startswith("numsum="):
                if abs(float(p[7:]) - float(q[7:])) > 1e-6 * max(1.0, abs(float(q[7:]))) + 1e-3:
                    ok = False
            elif p != q:
                ok = False
    if not ok:
        bad += 1
        if bad <= 10:
            print("ABWEICHUNG\n  C++:    %s\n  Python: %s" % (x, y))
print("%d Zeilen verglichen, %d Abweichungen" % (min(len(a), len(b)), bad))
sys.exit(1 if bad else 0)
