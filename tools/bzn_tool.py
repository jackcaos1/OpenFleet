#!/usr/bin/env python3
"""Unabhaengige Python-Referenz fuer das BZN-Format (Karten/Missionen), siehe docs/formats/bzn.md.

  bzn_tool.py <datei.bzn>        gibt Kopf, Objekte und Pfade lesbar aus
"""
import struct
import sys


class BznError(Exception):
    pass


def tokenize(b):
    """Binaerteil -> Liste (typ, bytes). Jedes Element: Typ(1) + 3 unbenutzte Bytes + Groesse u32 + Daten."""
    out, i = [], 0
    while i < len(b):
        if i + 8 > len(b):
            raise BznError("abgeschnittener Elementkopf bei %d" % i)
        t = b[i]
        sz = struct.unpack_from("<I", b, i + 4)[0]
        if i + 8 + sz > len(b):
            raise BznError("Element ueberragt Dateiende bei %d" % i)
        out.append((t, b[i + 8:i + 8 + sz]))
        i += 8 + sz
    return out


def cstr(d):
    return d.split(b"\0")[0].decode("latin1")


def is_obj_start(T, k):
    """Objektanfang: ODF-Name (7), Handle (4, Bit 31 gesetzt), Position (9x12), Team (4), Label (2x40), Zeiger (8), Matrix (11)."""
    if k + 6 >= len(T):
        return False
    return (T[k][0] == 7 and T[k + 1][0] == 4 and T[k + 1][1][3] >= 0x80 and T[k + 2][0] == 9 and len(T[k + 2][1]) == 12
            and T[k + 3][0] == 4 and T[k + 4][0] == 2 and len(T[k + 4][1]) == 40 and T[k + 5][0] == 8 and T[k + 6][0] == 11)


def is_footer(T, k):
    return (k + 4 < len(T) and T[k][0] == 2 and len(T[k][1]) == 40 and T[k + 1][0] == 8
            and T[k + 2][0] == 2 and len(T[k + 2][1]) == 64 and T[k + 3][0] == 2 and len(T[k + 3][1]) == 64
            and T[k + 4][0] == 0 and len(T[k + 4][1]) == 64)


def parse(data):
    m = data.find(b"true\r\n")
    if m < 0:
        raise BznError("Kopf ohne 'true'")
    head = data[:m + 6].decode("latin1").split("\r\n")
    r = {"version": int(head[1])}
    desc = head[2].split("=", 1)[1].strip()
    r["desc"] = bytes.fromhex(desc).split(b"\0")[0].decode("latin1")
    T = tokenize(data[m + 6:])
    if len(T) < 17 or T[16][0] != 4:
        raise BznError("Kopf zu kurz")
    f = lambda k: struct.unpack("<f", T[k][1])[0]
    r["file"] = cstr(T[0][1])
    r["name"] = cstr(T[2][1])
    r["bounds"] = [f(k) for k in (3, 4, 5, 6, 7, 8)]
    r["background"] = cstr(T[13][1])
    r["tt"] = cstr(T[14][1])
    r["race"] = cstr(T[15][1])
    count = struct.unpack("<i", T[16][1])[0]
    starts = [k for k in range(17, len(T)) if is_obj_start(T, k)]
    if len(starts) != count:
        raise BznError("Objektzahl %d laut Kopf, %d gefunden" % (count, len(starts)))
    objs = []
    for k in starts:
        mat = struct.unpack("<12f", T[k + 6][1])
        objs.append({
            "odf": cstr(T[k][1]),
            "id": struct.unpack("<I", T[k + 1][1])[0],
            "pos": list(struct.unpack("<3f", T[k + 2][1])),
            "team": struct.unpack("<i", T[k + 3][1])[0],
            "label": cstr(T[k + 4][1]),
            "rot": list(mat[:9]),
        })
    r["objects"] = objs
    fk = next((k for k in range(starts[-1] + 7 if starts else 17, len(T)) if is_footer(T, k)), None)
    if fk is None:
        raise BznError("Fussteil nicht gefunden")
    r["mission"] = cstr(T[fk][1])
    r["title"] = cstr(T[fk + 4][1])
    k = fk + 5
    npaths = struct.unpack("<i", T[k + 1][1])[0]
    k += 2
    paths = []
    for _ in range(npaths):
        if T[k][0] != 0 or T[k + 1][0] != 4 or T[k + 2][0] != 2 or T[k + 3][0] != 4 or T[k + 4][0] != 9:
            raise BznError("Pfadelement unerwartet bei Token %d" % k)
        ln = struct.unpack("<i", T[k + 1][1])[0]
        n = struct.unpack("<i", T[k + 3][1])[0]
        if ln != len(T[k + 2][1]) or n * 12 != len(T[k + 4][1]):
            raise BznError("Pfadlaengen passen nicht bei Token %d" % k)
        pts = struct.unpack("<%df" % (3 * n), T[k + 4][1])
        paths.append({"name": cstr(T[k + 2][1]), "points": [pts[i:i + 3] for i in range(0, len(pts), 3)],
                      "kind": struct.unpack("<i", T[k + 5][1])[0], "speed": struct.unpack("<f", T[k + 6][1])[0]})
        k += 7
    if k != len(T):
        raise BznError("%d Elemente nach den Pfaden uebrig" % (len(T) - k))
    r["paths"] = paths
    return r


def main():
    r = parse(open(sys.argv[1], "rb").read())
    print("%s v%d '%s' name=%s bg=%s tt=%s race=%s bounds=%s" % (r["file"], r["version"], r["desc"], r["name"], r["background"], r["tt"], r["race"], r["bounds"]))
    print("mission=%s titel=%s objekte=%d pfade=%d" % (r["mission"], r["title"], len(r["objects"]), len(r["paths"])))
    for o in r["objects"][:20]:
        print(" ", o["odf"], o["label"], o["team"], ["%.1f" % x for x in o["pos"]])
    for p in r["paths"]:
        print(" pfad", p["name"], len(p["points"]), p["kind"], p["speed"])


if __name__ == "__main__":
    main()
