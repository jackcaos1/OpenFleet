#!/usr/bin/env python3
"""SOD-Modelle (Storm3D) lesen, pruefen und exportieren. Basis: tools/sod_format.py.

  python3 -I tools/sod_tool.py info    <datei.sod> [textures-ordner]
  python3 -I tools/sod_tool.py obj     <datei.sod> <ziel.obj>
  python3 -I tools/sod_tool.py preview <datei.sod> <ziel.png>
  python3 -I tools/sod_tool.py check   <sod-ordner> <textures-ordner>     (alle Dateien pruefen)

Der Export dient nur der Entwicklung/Fehlersuche. Die Engine liest SOD direkt.
Ergebnisse gehoeren nach work/ (nicht ins Repo).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # eigener Werkzeugordner, auch im -I-Modus
import sod_format as F  # noqa: E402


def texture_index(tex_root):
    """Kleingeschriebener Name -> {'rgb': pfad, 'index8': pfad, 'mips': [pfade .1-.4 in RGB]}."""
    idx = {}
    for sub in ("RGB", "Index8"):
        d = os.path.join(tex_root, sub)
        if not os.path.isdir(d):
            continue
        for f in os.listdir(d):
            base, ext = os.path.splitext(f)
            e = idx.setdefault(base.lower(), {"rgb": None, "index8": None, "mips": {}})
            if ext.lower() == ".tga":
                e["rgb" if sub == "RGB" else "index8"] = os.path.join(d, f)
            elif sub == "RGB" and ext[1:].isdigit():
                e["mips"][int(ext[1:])] = os.path.join(d, f)
            # Namen mit Punkt (z. B. "mbg01.5") auch unter vollem Namen ablegen
            idx.setdefault(f.lower(), e)
    return idx


def find_texture(idx, name):
    return idx.get(name.lower())


def mesh_nodes(sod):
    return [n for n in sod.nodes if n.mesh]


def world(sod):
    return F.world_matrices(sod)


def xform(v, m):
    r, u, f, p = m
    return (v[0] * r[0] + v[1] * u[0] + v[2] * f[0] + p[0],
            v[0] * r[1] + v[1] * u[1] + v[2] * f[1] + p[1],
            v[0] * r[2] + v[1] * u[2] + v[2] * f[2] + p[2])


def write_obj(sod, out, idx=None):
    wm = world(sod)
    base = os.path.splitext(out)[0]
    mats = {}
    with open(out, "w") as f:
        f.write("# aus SOD exportiert (tools/sod_tool.py)\nmtllib %s.mtl\n" % os.path.basename(base))
        vo = uo = 0
        for n in mesh_nodes(sod):
            m = n.mesh
            f.write("o %s\n" % n.name)
            for v in m.verts:
                f.write("v %.5f %.5f %.5f\n" % xform(v, wm[id(n)]))
            for u, v in m.uvs:
                f.write("vt %.5f %.5f\n" % (u, 1 - v))
            key = m.texture
            mats[key] = (find_texture(idx, m.texture) if idx else None)
            f.write("usemtl %s\n" % key)
            for _, faces in m.groups:
                for fc in faces:
                    f.write("f " + " ".join("%d/%d" % (fc[2 * k] + 1 + vo, fc[2 * k + 1] + 1 + uo) for k in range(3)) + "\n")
            vo += len(m.verts)
            uo += len(m.uvs)
    with open(base + ".mtl", "w") as f:
        for name, t in mats.items():
            f.write("newmtl %s\nKd 1 1 1\n" % name)
            if t and (t["rgb"] or t["index8"]):
                f.write("map_Kd %s\n" % os.path.abspath(t["rgb"] or t["index8"]))


def tree_lines(sod, idx=None):
    kids = {}
    for n in sod.nodes:
        kids.setdefault(n.parent, []).append(n)
    names = {n.name for n in sod.nodes}
    out = []

    def walk(n, d):
        tn = {0: "null", 1: "mesh", 3: "sprite", 11: "lod", 12: "emitter"}.get(n.type, str(n.type))
        extra = ""
        if n.mesh:
            tri = sum(len(g[1]) for g in n.mesh.groups)
            tx = ""
            if idx is not None:
                tx = " OK" if find_texture(idx, n.mesh.texture) else " FEHLT"
            extra = "  v=%d tri=%d tex=%s%s" % (len(n.mesh.verts), tri, n.mesh.texture, tx)
        elif n.emitter:
            extra = "  emitter=%s" % n.emitter
        out.append("%s%s [%s] pos=(%.2f, %.2f, %.2f)%s" % ("  " * d, n.name, tn, *n.pos, extra))
        for c in kids.get(n.name, []):
            if c is not n:
                walk(c, d + 1)

    for n in sod.nodes:
        if n.parent == "" or n.parent not in names:
            walk(n, 0)
    return out


def preview(sod, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    wm = world(sod)
    tris = []
    for n in mesh_nodes(sod):
        wv = [xform(v, wm[id(n)]) for v in n.mesh.verts]
        for _, faces in n.mesh.groups:
            for fc in faces:
                tris.append([wv[fc[0]], wv[fc[2]], wv[fc[4]]])
    if not tris:
        raise SystemExit("keine Dreiecke")
    pts = [p for t in tris for p in t]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    fig, axs = plt.subplots(1, 3, figsize=(15, 5.2))
    for ax, (title, a, c, d) in zip(axs, [("Oben (x,z)", 0, 2, 1), ("Seite (z,y)", 2, 1, 0), ("Vorne (x,y)", 0, 1, 2)]):
        polys, cols = [], []
        for t in sorted(tris, key=lambda t: sum(p[d] for p in t) / 3):
            u = [t[1][i] - t[0][i] for i in range(3)]
            w = [t[2][i] - t[0][i] for i in range(3)]
            nn = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
            ln = math.sqrt(sum(x * x for x in nn)) or 1
            sh = 0.35 + 0.65 * abs(nn[d]) / ln
            polys.append([(p[a], p[c]) for p in t])
            cols.append((0.35 * sh + 0.1, 0.55 * sh + 0.1, 0.9 * sh))
        ax.add_collection(PolyCollection(polys, facecolors=cols, edgecolors="none"))
        ax.set_xlim(lo[a], hi[a]); ax.set_ylim(lo[c], hi[c]); ax.set_aspect("equal"); ax.set_title(title); ax.axis("off")
    fig.savefig(out, dpi=100, bbox_inches="tight")


def check(sod_dir, tex_root):
    idx = texture_index(tex_root)
    ok = bad = 0
    nmesh = ntex = 0
    missing = {}
    for root, _, fs in os.walk(sod_dir):
        for f in sorted(fs):
            try:
                s = F.parse_file(os.path.join(root, f))
                ok += 1
            except F.SodError as e:
                bad += 1
                print("  nicht lesbar:", f, "-", e)
                continue
            for n in mesh_nodes(s):
                nmesh += 1
                if find_texture(idx, n.mesh.texture):
                    ntex += 1
                else:
                    missing.setdefault(n.mesh.texture, set()).add(f)
    print("SOD-Dateien fehlerfrei: %d, nicht lesbar: %d" % (ok, bad))
    print("Meshes: %d, davon Textur gefunden: %d" % (nmesh, ntex))
    for t, fs in sorted(missing.items()):
        print("  Textur fehlt: %-14s in %s" % (t, ", ".join(sorted(fs))[:70]))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "check":
        return check(sys.argv[2], sys.argv[3])
    sod = F.parse_file(sys.argv[2])
    if cmd == "info":
        idx = texture_index(sys.argv[3]) if len(sys.argv) > 3 else None
        print("%s v%.1f: %d Materialien, %d Knoten, %d Animationskanaele" % (sod.magic, sod.version, len(sod.materials), len(sod.nodes), len(sod.channels)))
        print("Materialien:", ", ".join(m.name for m in sod.materials))
        print("\n".join(tree_lines(sod, idx)))
    elif cmd == "obj":
        write_obj(sod, sys.argv[3], texture_index(sys.argv[4]) if len(sys.argv) > 4 else None)
    elif cmd == "preview":
        preview(sod, sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
