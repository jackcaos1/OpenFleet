#!/usr/bin/env python3
"""Python-Referenz zu tests/data_dump.cpp. Aufruf: python3 -I tools/data_dump_ref.py <spielordner> > work/ref.txt"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sod_format as F  # noqa: E402
import bzn_tool as B  # noqa: E402
import sod_tool as T  # noqa: E402
import zfs_tool as Z  # noqa: E402


def odf_parse(text):
    """Unabhaengige Referenz: gibt (includes, {schluessel_klein: [werte]}) zurueck. Werte: ('s',text) | ('n',zahl) | ('i',text)."""
    import re
    includes, ent, cur = [], {}, None
    num = re.compile(r'^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?[fF]?$')

    def toks(s):
        out, i = [], 0
        while i < len(s):
            c = s[i]
            if c.isspace() or c == ';':
                i += 1
            elif s.startswith('//', i):
                break
            elif c == '"':
                j = s.find('"', i + 1)
                if j < 0:
                    out.append(('s', s[i + 1:]))
                    break
                out.append(('s', s[i + 1:j]))
                i = j + 1
            else:
                m = re.match(r'[^\s;"]+', s[i:])
                t = m.group(0)
                k = t.find('//')
                if k >= 0:
                    t = t[:k]
                    if not t:
                        break
                i += len(t)
                out.append(('n', float(t.rstrip('fF'))) if num.match(t) else ('i', t))
        return out

    for raw in text.replace('\r', '').split('\n'):
        line = raw.strip()
        if not line or line.startswith('//'):
            continue
        if line.startswith('#'):
            if line[:8].lower() == '#include':
                t = toks(line[8:])
                if t and t[0][0] == 's':
                    includes.append(t[0][1])
            cur = None
            continue
        if line.startswith('['):
            cur = None
            continue
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)[ \t]*=(.*)$', line)
        if m:
            cur = m.group(1).lower()
            ent[cur] = toks(m.group(2))
            continue
        if cur is not None and (line[0] == '"' or line[0].isdigit() or line[0] in '-.'):
            ent[cur] += toks(line)
    return includes, ent


def dump_odf(root):
    p = os.path.join(root, "assets.zfs")
    if not os.path.exists(p):
        return
    data, key, entries = Z.read_archive(p)
    files = {n.lower(): bytes(Z.decode(data[o:o + sz], key)) for n, o, sz, _ in entries}
    parsed = {}

    def resolve(n):
        if n in parsed:
            return parsed[n]
        inc, own = odf_parse(files[n].decode("latin-1"))
        merged = {}
        for i in inc:
            i = i.lower()
            if not i.endswith(".odf"):
                i += ".odf"
            merged.update(resolve(i)[0])
        merged.update(own)
        parsed[n] = (merged, len(inc))
        return parsed[n]

    for n in sorted(files):
        if n.endswith(".odf"):
            ent, ninc = resolve(n)
            vals = sum(len(v) for v in ent.values())
            strs = sum(1 for v in ent.values() for t in v if t[0] == 's')
            s = sum(t[1] for v in ent.values() for t in v if t[0] == 'n')
            print("ODF %s entries=%d vals=%d strs=%d includes=%d numsum=%.4f" % (n, len(ent), vals, strs, ninc, s))
        elif n.endswith(".tt"):
            lines = pre = 0
            for l in files[n].decode("latin-1").splitlines():
                t = l.split()
                if len(t) >= 2 and t[1].isdigit() and len(t) == 2 + int(t[1]) and not t[0].startswith("//"):
                    lines += 1
                    pre += len(t) - 2
            print("TT %s lines=%d prereqs=%d" % (n, lines, pre))
    return lambda n: resolve(n + ".odf")[0] if (n + ".odf") in files else None


def resolve_node(defs, name):
    import re
    n = name.lower()
    while True:
        if n in defs:
            return True
        m = re.match(r"^(.*)_(\d+)$", n)
        if not m:
            return False
        n = m.group(1)


def dump_weapons(root, resolve):
    p = os.path.join(root, "assets.zfs")
    data, key, entries = Z.read_archive(p)
    names = sorted(n.lower()[:-4] for n, _, _, _ in entries if n.lower().endswith(".odf"))

    def first(ent, k, default=0.0):
        v = ent.get(k)
        return v[0][1] if v and v[0][0] == "n" else default

    def overrides(ent, k):
        v = ent.get(k, [])
        n = 0
        i = 1
        while i + 1 < len(v) and v[i][0] == "s" and v[i + 1][0] == "n":
            n += 1
            i += 2
        return n

    ords = set()
    for n in names:
        ent = resolve(n)
        o = ent.get("ordname")
        if not o or o[0][0] != "s":
            continue
        ords.add(o[0][1].lower())
        print("WPN %s.odf ord=%s range=%.2f delay=%.2f hit=%.2f hitov=%d" % (n, o[0][1].lower(), first(ent, "range"), first(ent, "shotdelay"), first(ent, "hitchance"), overrides(ent, "hitchance")))
    for n in sorted(ords):
        if n not in names:
            print("ORD %s FEHLT" % n)
            continue
        ent = resolve(n)
        cls = ent.get("classlabel", [("s", "")])[0][1]
        print("ORD %s class=%s dmg=%.2f var=%.2f thr=%.2f dmgov=%d life=%.2f speed=%.2f radius=%.2f ign=%d light=%d" % (
            n, cls, first(ent, "damagebase"), first(ent, "damagevariance"), first(ent, "damagethreshold"), overrides(ent, "damagebase"),
            first(ent, "lifespan"), first(ent, "shotspeed"), first(ent, "radius"), 1 if first(ent, "ignoreshield") else 0,
            1 if len(ent.get("lightcolor", [])) >= 3 else 0))


def dump_spr(root):
    """Unabhaengige Referenz zum .spr-Parser. Gibt die Knotennamen zurueck."""
    import re
    sprites, anims, nodes = set(), {}, set()
    sprite_info = {}
    auto_ctx = [False]
    d = os.path.join(root, "Sprites")
    for f in sorted(os.listdir(d), key=str.lower):
        if not f.lower().endswith(".spr"):
            continue
        cur = None
        header = False
        keys = False
        for raw in open(os.path.join(d, f), encoding="latin-1"):
            line = re.sub(r"//.*", "", raw).strip()
            line = line.replace("(", " ").replace(")", " ").replace(",", " ")
            t = line.split()
            if not t or t[0].startswith("#"):
                continue
            if t[0].startswith("@"):
                keys = False
                if t[0] == "@animation" and len(t) >= 2:
                    cur = t[1].lower()
                    anims[cur] = {"field": "", "len": 0.0, "linear": False, "keys": [], "count": 0, "auto": ""}
                    header = True
                    auto_ctx[0] = False
                elif t[0].startswith("@auto=") and cur and auto_ctx[0]:
                    anims[cur]["auto"] = t[0][6:].lower()
                elif t[0] == "@keyframes":
                    keys = True
                    auto_ctx[0] = False
                elif t[0] == "@sprite_node" and len(t) >= 4:
                    nodes.add(t[1].lower())
                continue
            if cur and header:
                try:
                    anims[cur].update(field=t[0].lower(), len=float(t[2]), linear=t[3].lower() == "linear", count=int(t[1]))
                    header = False
                    auto_ctx[0] = True
                except (ValueError, IndexError):
                    pass
                continue
            if cur and keys:
                try:
                    vals = [float(x) for x in t[:4]]
                    if len(t) >= 2:
                        anims[cur]["keys"].append((vals[0], vals[1:] + [0.0] * (3 - len(vals[1:]))))
                except ValueError:
                    pass
                continue
            if len(t) == 6 or (len(t) >= 7 and t[6].startswith("@anim=")):
                try:
                    [float(x) for x in t[2:6]]
                    sprites.add(t[0].lower())
                    sprite_info[t[0].lower()] = (t[1], t[2:6], t[6][6:].lower() if len(t) >= 7 else "")
                except ValueError:
                    pass
    print("SPR sprites=%d animations=%d nodes=%d" % (len(sprites), len(anims), len(nodes)))

    def cell(a, tt):
        if not a["auto"] or a["count"] < 1 or a["len"] <= 0:
            return 0, 0
        tt = tt - a["len"] * math.floor(tt / a["len"])
        f = min(int(tt / a["len"] * a["count"]), a["count"] - 1)
        if a["auto"] == "row":
            return f, 0
        if a["auto"] == "column":
            return 0, f
        n = 1
        while n * n < a["count"]:
            n += 1
        return f % n, f // n

    for aid in ("photanim", "tex4x4", "tex4x4.12", "tex1x4", "tex4x1", "tex2x2", "tex3x3"):
        a = anims.get(aid)
        if not a:
            print("SPRANIM %s fehlt" % aid)
            continue
        print("SPRANIM %s field=%s count=%d auto=%s len=%.3f %s" % (aid, a["field"], a["count"], a["auto"], a["len"], " ".join("%d,%d" % cell(a, tt) for tt in (0.0, 0.37, 1.9, 4.23))))
    for sn in ("wphoton", "kphoton", "fphaser", "pphaser"):
        i = sprite_info.get(sn)
        print("SPRITE %s %s" % (sn, ("%s %d %d %d %d anim=%s" % (i[0], int(float(i[1][0])), int(float(i[1][1])), int(float(i[1][2])), int(float(i[1][3])), i[2])) if i else "fehlt"))
    for aid in ("strobe", "strobe1", "strobe2", "flicker"):
        a = anims.get(aid)
        if not a:
            print("ANIM %s fehlt" % aid)
            continue
        acc = 0.0
        for i in range(40):
            tt = i * 0.137
            if a["len"] > 0:
                tt = tt - a["len"] * math.floor(tt / a["len"])
            ks = a["keys"]
            j = 0
            while j + 1 < len(ks) and ks[j + 1][0] <= tt:
                j += 1
            if not a["linear"] or j + 1 >= len(ks) or tt < ks[j][0]:
                v = ks[j][1]
            else:
                span = ks[j + 1][0] - ks[j][0]
                f = (tt - ks[j][0]) / span if span > 0 else 0
                v = [ks[j][1][k] + (ks[j + 1][1][k] - ks[j][1][k]) * f for k in range(3)]
            acc += v[0] + 2 * v[1] + 3 * v[2]
        print("ANIM %s field=%s keys=%d len=%.3f linear=%d acc=%.3f" % (aid, a["field"], len(a["keys"]), a["len"], 1 if a["linear"] else 0, acc))
    return nodes


def dump_bzn(root):
    d = os.path.join(root, "bzn")
    if not os.path.isdir(d):
        return
    us = lambda s: s.replace(" ", "_") if s else "-"
    for f in sorted(os.listdir(d)):
        if not f.lower().endswith(".bzn"):
            continue
        try:
            r = B.parse(open(os.path.join(d, f), "rb").read())
        except B.BznError as e:
            print("BZN %s FEHLER %s" % (f.lower(), e))
            continue
        o = r["objects"]
        teams = [sum(1 for x in o if x["team"] == t) for t in range(9)]
        pts = sum(len(p["points"]) for p in r["paths"])
        ptsum = sum(sum(c * (1 + i % 3) for i, pt in enumerate(p["points"]) for c in [0]) for p in r["paths"])
        ptsum = 0.0
        for p in r["paths"]:
            for pt in p["points"]:
                ptsum += pt[0] + 2 * pt[1] + 3 * pt[2]
            ptsum += p["kind"] + p["speed"] + len(p["name"])
        b = r["bounds"]
        print("BZN %s v=%d desc=%s file=%s name=%s bg=%s tt=%s race=%s mission=%s title=%s objs=%d paths=%d points=%d bounds=sum=%.3f,%.3f,%.3f ids=%d text=%d "
              "teams=%s pos=sum=%.3f,%.3f,%.3f rot=numsum=%.4f path=acc=%.3f" % (
                  f.lower(), r["version"], us(r["desc"]), us(r["file"]), us(r["name"]), us(r["background"]), us(r["tt"]), us(r["race"]), us(r["mission"]),
                  us(r["title"]), len(o), len(r["paths"]), pts, b[0] + b[1], b[2] + b[3], b[4] + b[5], sum(x["id"] for x in o),
                  sum(len(x["label"]) + len(x["odf"]) for x in o), ",".join(str(t) for t in teams),
                  sum(x["pos"][0] for x in o), sum(x["pos"][1] for x in o), sum(x["pos"][2] for x in o), sum(sum(x["rot"]) for x in o), ptsum))


def main():
    root = sys.argv[1]
    for z in ("assets.zfs", "objtxt.zfs"):
        p = os.path.join(root, z)
        if not os.path.exists(p):
            continue
        data, key, entries = Z.read_archive(p)
        s = sum(sum(Z.decode(data[o:o + sz], key)) for _, o, sz, _ in entries)
        print("ZFS %s n=%d key=%08x bytesum=%d" % (z, len(entries), key, s))
    odf_resolve = dump_odf(root)
    dump_weapons(root, odf_resolve)
    spr_defs = dump_spr(root)
    idx = T.texture_index(os.path.join(root, "Textures"))
    sod_dir = os.path.join(root, "SOD")
    for f in sorted(os.listdir(sod_dir)):
        if not f.lower().endswith(".sod"):
            continue
        try:
            s = F.parse_file(os.path.join(sod_dir, f))
            base = f.lower()[:-4]
            ent = odf_resolve(base)
            if ent is not None:
                import re as _re
                wslots = sorted(int(m.group(1)) for k, v in ent.items() for m in [_re.match(r"^weapon(\d+)$", k)] if m and v and v[0][0] == "s")
                hps = missing = 0
                names = {nd.name.lower() for nd in s.nodes}
                for k in wslots:
                    for t in ent.get("weaponhardpoints%d" % k, []):
                        if t[0] == "s":
                            hps += 1
                            if t[1].lower() not in names:
                                missing += 1
                cls = ent.get("classlabel", [("s", "")])[0][1]
                hp = ent.get("maxhealth", [("n", 0.0)])[0][1]
                crew = int(ent.get("crewcost", [("n", 0.0)])[0][1])
                print("UNIT %s class=%s weapons=%d hardpoints=%d hp_missing=%d health=%.2f crew=%d" % (base, cls, len(wslots), hps, missing, hp, crew))
            sn = [n.name for n in s.nodes if n.type == 3]
            res = sum(1 for n in sn if resolve_node(spr_defs, n))
            nhp = sum(1 for n in s.nodes if n.type == 0 and re.match(r"^hp\d+$", n.name.lower()))
            print("LIGHTS %s sprite_nodes=%d resolved=%d modelnodes=%d hardpoints=%d" % (f.lower(), len(sn), res, len(sn), nhp))
        except F.SodError:
            print("SOD %s FEHLER" % f.lower())
            continue
        wm = F.world_matrices(s)
        meshes = verts = tris = ok = miss = empty = 0
        sx = sy = sz = 0.0
        for n in s.nodes:
            if not n.mesh:
                continue
            meshes += 1
            verts += len(n.mesh.verts)
            tris += sum(len(g[1]) for g in n.mesh.groups)
            for v in n.mesh.verts:
                x, y, z = T.xform(v, wm[id(n)])
                sx += x
                sy += y
                sz += z
            if not n.mesh.texture:
                empty += 1
            else:
                e = T.find_texture(idx, n.mesh.texture)
                if e and (e["rgb"] or e["index8"]):
                    ok += 1
                else:
                    miss += 1
        print("SOD %s v=%.1f nodes=%d meshes=%d verts=%d tris=%d modeltris=%d tex_ok=%d tex_missing=%d tex_empty=%d sum=%.2f,%.2f,%.2f"
              % (f.lower(), s.version, len(s.nodes), meshes, verts, tris, tris, ok, miss, empty, sx, sy, sz))
    dump_bzn(root)


if __name__ == "__main__":
    main()
