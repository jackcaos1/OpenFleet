#!/usr/bin/env python3
"""Kleiner Testdatensatz aus der eigenen Spielinstallation fuer erste Android-Tests.

Kopiert nur wenige SOD-Modelle und die dafuer noetigen Texturen (unveraendert, gleiche Ordnerstruktur wie im Spiel).
Der Client liest diesen Ordner genau wie die volle Installation. Das Ergebnis gehoert nicht ins Repo (work/ ist ignoriert).

  python3 -I tools/make_testset.py <spielordner> <zielordner> [--models Fcruise1,Kbattle,...] [--all-models] [--maps 2duel,4nirvana,...] [--maps 2duel,4nirvana,...]

Standard: eine Auswahl von Schiffen/Basen der vier Voelker plus Planet/Sterne (v1.2). Mit --all-models alle 265 Modelle
(ca. 5 MB SOD + deren Texturen).
"""
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bzn_tool as B  # noqa: E402
import sod_format as F  # noqa: E402
import zfs_tool as Z  # noqa: E402

DEFAULT_MAPS = ["2duel", "4nirvana", "3race", "4first"]
DEFAULT = ["Fcruise1", "Fbattle", "Fscout", "Fentd", "Kcruise1", "Kbattle", "Rcruise1", "Bcruise1", "Bbase", "Fbase", "Mstars",
           "Wprobe"]


def odf_base_names(src):
    """ODF-Name (klein) -> SOD-Name (klein) laut baseName (mit #include), nur Eintraege, die abweichen oder keine eigene Geometrie haben."""
    import re
    az = find_ci(src, "assets.zfs")
    if not az:
        return {}
    data, key, entries = Z.read_archive(os.path.join(src, az))
    text = {e[0].lower(): Z.decode(data[e[1]:e[1] + e[2]], key).decode("latin1") for e in entries}

    def get(n, k, depth=0):
        t = text.get(n + ".odf")
        if t is None or depth > 6:
            return None
        m = re.search(r'^\s*%s\s*=\s*"?([^"\r\n/]*)"?' % k, t, re.M | re.I)
        if m:
            return m.group(1).strip()
        inc = re.search(r'#include\s+"([^"]+)"', t)
        return get(inc.group(1).lower().replace(".odf", ""), k, depth + 1) if inc else None

    return {n[:-4]: (get(n[:-4], "baseName") or "").lower() for n in text if n.endswith(".odf")}, get


def find_ci(folder, name):
    """Datei im Ordner ohne Beachtung der Schreibweise."""
    want = name.lower()
    try:
        for f in os.listdir(folder):
            if f.lower() == want:
                return f
    except FileNotFoundError:
        pass
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = [a for a in sys.argv[1:] if a.startswith("--")]
    if len(args) < 2:
        sys.exit(__doc__)
    src, dst = args[0], args[1]
    sod_dir = os.path.join(src, "SOD")
    if not os.path.isdir(sod_dir):
        sys.exit("Kein SOD-Ordner in %s" % src)
    all_models = "--all-models" in flags
    names = DEFAULT
    if "--models" in sys.argv:
        names = sys.argv[sys.argv.index("--models") + 1].split(",")
        args = [a for a in args if a != sys.argv[sys.argv.index("--models") + 1]]
        src, dst = args[0], args[1]
    if all_models:
        names = [os.path.splitext(f)[0] for f in os.listdir(sod_dir) if f.lower().endswith(".sod")]

    # Karten (bzn/*.bzn) samt allen SOD-Modellen, die ihre Objekte und ihr Hintergrund brauchen
    map_names = DEFAULT_MAPS
    if "--maps" in sys.argv:
        map_names = [m for m in sys.argv[sys.argv.index("--maps") + 1].split(",") if m]
    bzn_src = os.path.join(src, "bzn")
    sod_have = {f[:-4].lower() for f in os.listdir(sod_dir) if f.lower().endswith(".sod")}
    extra = set(n.lower() for n in names)
    bases = odf_base_names(src)
    base_of = bases[0] if bases else {}
    maps_copied = 0
    need_rocks = False
    for m in map_names:
        f = find_ci(bzn_src, m + ".bzn")
        if not f:
            print("Karte nicht vorhanden:", m)
            continue
        try:
            bz = B.parse(open(os.path.join(bzn_src, f), "rb").read())
        except B.BznError as e:
            print("Karte nicht lesbar:", f, e)
            continue
        os.makedirs(os.path.join(dst, "bzn"), exist_ok=True)
        shutil.copyfile(os.path.join(bzn_src, f), os.path.join(dst, "bzn", f))
        maps_copied += 1
        for o in bz["objects"]:
            n = o["odf"].lower()
            sod_name = base_of.get(n) or n  # SOD laut baseName der ODF, sonst gleicher Name
            if sod_name in sod_have:
                extra.add(sod_name)
            if n in sod_have:
                extra.add(n)
            if base_of.get(n) == "sscr_1" and n == "mafield":
                need_rocks = True  # Asteroidenfeld: Einzelfelsen Master1..8
        bg = os.path.splitext(bz["background"])[0].lower()
        if bg in sod_have:
            extra.add(bg)
    if need_rocks:
        extra.update(r for r in ("master%d" % i for i in range(1, 9)) if r in sod_have)
    if not all_models:
        names = sorted(extra)

    out_sod = os.path.join(dst, "SOD")
    os.makedirs(out_sod, exist_ok=True)
    copied, textures, missing, total = [], set(), set(), 0
    for n in names:
        f = find_ci(sod_dir, n + ".sod")
        if not f:
            print("uebersprungen (nicht vorhanden):", n)
            continue
        path = os.path.join(sod_dir, f)
        try:
            sod = F.parse_file(path)
        except F.SodError:
            print("uebersprungen (nicht lesbar):", f)
            continue
        shutil.copyfile(path, os.path.join(out_sod, f))
        total += os.path.getsize(path)
        copied.append(f)
        for node in sod.nodes:
            if node.mesh and node.mesh.texture:
                textures.add(node.mesh.texture)

    # assets.zfs (ODF/TT-Spielwerte, klein) fuer die Anzeige der Spielwerte im Betrachter
    az = find_ci(src, "assets.zfs")
    if az:
        shutil.copyfile(os.path.join(src, az), os.path.join(dst, az))
        total += os.path.getsize(os.path.join(src, az))

    # Sprites/*.spr (klein) und die Texturen aus lights.spr (Positionslichter)
    spr_src = os.path.join(src, "Sprites")
    if os.path.isdir(spr_src):
        os.makedirs(os.path.join(dst, "Sprites"), exist_ok=True)
        for f in os.listdir(spr_src):
            if f.lower().endswith(".spr"):
                shutil.copyfile(os.path.join(spr_src, f), os.path.join(dst, "Sprites", f))
                total += os.path.getsize(os.path.join(spr_src, f))
        for sf in sorted(os.listdir(spr_src)):  # Tabellen fuer Lichter, Waffen und Nebel (die anderen waeren zu gross)
            if sf.lower() in ("lights.spr", "weapon.spr", "nebula.spr"):
                for line in open(os.path.join(spr_src, sf), encoding="latin-1"):
                    t = line.split("#")[0].split()
                    if len(t) >= 6 and not t[0].startswith("@") and t[2].isdigit() and t[3].isdigit():
                        textures.add(t[1])

    # Ton: Feuer-/Treffer-/Explosions-/Dauergeraeusche aus allen ODFs (sounds/effects) und je Volk eine Musik (115er Stueck;
    # die Langfassung "<volk>300" waere je 16 MB, der Client faellt auf 115 zurueck)
    snd_src = os.path.join(src, "sounds")
    if az and os.path.isdir(snd_src):
        import re as _re
        zdata, zkey, zentries = Z.read_archive(os.path.join(src, az))
        wanted = set()
        for e in zentries:
            if e[0].lower().endswith(".odf"):
                txt = Z.decode(zdata[e[1]:e[1] + e[2]], zkey).decode("latin1")
                for m in _re.finditer(r'^\s*(?:fireSound|hitSound|explSound|ambientSound)\s*=\s*"?([^"\r\n/]+?)"?\s*(?://.*)?$', txt, _re.M | _re.I):
                    wanted.add(m.group(1).strip().lower())
        n_snd = 0
        eff_dir = os.path.join(snd_src, "effects")
        for w in sorted(wanted):
            f = find_ci(eff_dir, w)
            if f:
                os.makedirs(os.path.join(dst, "sounds", "effects"), exist_ok=True)
                shutil.copyfile(os.path.join(eff_dir, f), os.path.join(dst, "sounds", "effects", f))
                total += os.path.getsize(os.path.join(eff_dir, f))
                n_snd += 1
        mus_dir = os.path.join(snd_src, "music")
        for pre in ("Mfed", "Mbor", "Mkli", "MRom"):
            f = find_ci(mus_dir, pre + "115.wav")
            if f:
                os.makedirs(os.path.join(dst, "sounds", "music"), exist_ok=True)
                shutil.copyfile(os.path.join(mus_dir, f), os.path.join(dst, "sounds", "music", f))
                total += os.path.getsize(os.path.join(mus_dir, f))
                n_snd += 1
        print("%d Klangdateien" % n_snd)

    # Original-HUD: misc/gui_*.cfg, Texturen der gui_*.spr (ohne die grossen Bruecken-Hintergruende) und die Tastensymbole
    misc_src = os.path.join(src, "misc")
    if os.path.isdir(misc_src):
        for f in os.listdir(misc_src):
            if f.lower().startswith("gui_") and f.lower().endswith(".cfg"):
                os.makedirs(os.path.join(dst, "misc"), exist_ok=True)
                shutil.copyfile(os.path.join(misc_src, f), os.path.join(dst, "misc", f))
                total += os.path.getsize(os.path.join(misc_src, f))
    if os.path.isdir(spr_src):
        for sf in sorted(os.listdir(spr_src)):
            if sf.lower().startswith("gui_") and sf.lower().endswith(".spr"):
                for line in open(os.path.join(spr_src, sf), encoding="latin-1"):
                    t = line.split("#")[0].split()
                    if len(t) >= 6 and not t[0].startswith("@") and "bridge" not in t[0].lower() and t[2].isdigit() and t[3].isdigit():
                        textures.add(t[1])
    textures.update(["GBcompovr00", "GBflag00", "GBrally00", "GBrepair00", "GBengovr00", "GBharve00", "GBchnrxn00", "GBfrupgrd00",
                     "GBbaabore00", "font", "GBcancel00", "GBbuild00", "GBresrch00", "GBbsensor00", "GBbspec00", "GBenrcvr00"])

    for t in sorted(textures, key=str.lower):
        found = False
        for sub in ("RGB", "Index8"):  # gleiche Reihenfolge wie der Client: zuerst RGB
            d = os.path.join(src, "Textures", sub)
            for cand in (t + ".tga", t):
                f = find_ci(d, cand)
                if f:
                    os.makedirs(os.path.join(dst, "Textures", sub), exist_ok=True)
                    shutil.copyfile(os.path.join(d, f), os.path.join(dst, "Textures", sub, f))
                    total += os.path.getsize(os.path.join(d, f))
                    found = True
                    break
            if found:
                break
        if not found:
            missing.add(t)
    print("%d Karten" % maps_copied)
    print("%d Modelle, %d Texturen (%d fehlen im Original), %.1f MB nach %s" % (len(copied), len(textures) - len(missing), len(missing),
                                                                                total / 1e6, dst))
    if missing:
        print("fehlende Texturen (auch im Original nicht vorhanden):", ", ".join(sorted(missing)))


if __name__ == "__main__":
    main()
