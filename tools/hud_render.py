#!/usr/bin/env python3
"""Rendert die Ausgabe von fleet_hud_dump als PNG (Sichtpruefung des Original-HUDs ohne GPU).
Aufruf: hud_render.py <spielordner> <dump.txt> <ausgabe.png> <breite> <hoehe>
Text wird nur als Kasten angedeutet (die App nutzt den eingebauten Monospace-Font)."""
import sys, os
from PIL import Image, ImageDraw
game, dump, out, W, H = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
cache = {}
def tex(name):
    k = name.lower()
    if k not in cache:
        img = None
        if "/" in name:  # Menue-Bitmap, Pfad ab Spielordner, Schreibweise egal
            cur = game
            for part in name.split("/"):
                m = [f for f in os.listdir(cur) if f.lower() == part.lower()]
                cur = os.path.join(cur, m[0]) if m else None
                if cur is None: break
            if cur: img = Image.open(cur).convert("RGBA")
            cache[k] = img
            return img
        for d in ("Textures/RGB", "Textures/Index8"):
            dd = os.path.join(game, d)
            if not os.path.isdir(dd): continue
            for f in os.listdir(dd):
                if f.lower() in (k + ".tga", k):
                    img = Image.open(os.path.join(dd, f)).convert("RGBA"); break
            if img: break
        cache[k] = img
    return cache[k]
canvas = Image.new("RGBA", (W, H), (5, 8, 20, 255))
for line in open(dump):
    p = line.split()
    if not p: continue
    if p[0] == "L":
        vh = float(p[6]); ImageDraw.Draw(canvas).rectangle([0, 0, W, vh], fill=(10, 18, 40, 255))
    elif p[0] == "D":
        t = p[1]; x, y, w, h = [int(float(v)) for v in p[2:6]]
        u0, v0, u1, v1 = [float(v) for v in p[6:10]]; r, g, b, a = [float(v) for v in p[10:14]]
        if w <= 0 or h <= 0: continue
        if t == "-":
            layer = Image.new("RGBA", (w, h), (int(r*255), int(g*255), int(b*255), int(a*255)))
        else:
            im = tex(t)
            if im is None:
                layer = Image.new("RGBA", (w, h), (255, 0, 255, 120))
            else:
                iw, ih = im.size
                crop = im.crop((round(u0*iw), round(v0*ih), max(round(u0*iw)+1, round(u1*iw)), max(round(v0*ih)+1, round(v1*ih))))
                layer = crop.resize((w, h), Image.BILINEAR)
                if p[14] == "1":  # additiv
                    base = canvas.crop((x, y, x+w, y+h)).convert("RGB")
                    add = Image.merge("RGB", [Image.eval(c, lambda v: v) for c in layer.convert("RGB").split()])
                    from PIL import ImageChops
                    canvas.paste(ImageChops.add(base, add).convert("RGBA"), (x, y)); continue
                if r < 1 or g < 1 or b < 1:
                    ch = layer.split()
                    layer = Image.merge('RGBA', [ch[0].point(lambda v: int(v*r)), ch[1].point(lambda v: int(v*g)), ch[2].point(lambda v: int(v*b)), ch[3]])
                if a < 1:
                    layer.putalpha(layer.getchannel("A").point(lambda v: int(v*a)))
        canvas.alpha_composite(layer, (x, y)) if 0 <= x and 0 <= y and x+w <= W and y+h <= H else canvas.paste(layer, (x, y), layer)
    elif p[0] == "T":
        x, y, sc = int(float(p[1])), int(float(p[2])), int(p[3]); text = " ".join(p[7:])
        ImageDraw.Draw(canvas).text((x, y), text, fill=(int(float(p[4])*255), int(float(p[5])*255), int(float(p[6])*255)))
canvas.convert("RGB").save(out)
