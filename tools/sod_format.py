#!/usr/bin/env python3
"""Strenger SOD-Parser (Storm3D, Versionen 1.4-1.8). Eigene Implementierung nach docs/formats/sod.md.

Jede Datei muss exakt bis zum letzten Byte gelesen werden, sonst SodError.
Aufbau: Kopf | [Legacy-Liste nur v<=1.6] | Materialien | Knoten | Animationskanaele | [Texturreferenzen ab v1.6]
v1.2 (11 Dateien): Meshes als Polygone, ohne Animationsteil. v1.0 (1 Datei: Wviracod.SOD) ist noch nicht gelesen.
"""
import math
import struct
from dataclasses import dataclass, field

NODE_NULL, NODE_MESH, NODE_SPRITE, NODE_LOD, NODE_EMITTER = 0, 1, 3, 11, 12


class SodError(Exception):
    pass


class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def take(self, n):
        if n < 0 or self.p + n > len(self.d):
            raise SodError("Datei zu kurz bei Offset %#x (brauche %d Byte)" % (self.p, n))
        v = self.d[self.p:self.p + n]
        self.p += n
        return v

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack("<H", self.take(2))[0]

    def f32(self):
        return struct.unpack("<f", self.take(4))[0]

    def floats(self, n):
        return struct.unpack("<%df" % n, self.take(4 * n))

    def string(self):
        n = self.u16()
        raw = self.take(n)
        if any(c < 32 or c > 126 for c in raw):
            raise SodError("unlesbarer String bei Offset %#x" % (self.p - n))
        return raw.decode("ascii")


@dataclass
class Material:
    name: str
    ambient: tuple
    diffuse: tuple
    specular: tuple
    power: float
    lighting: int


@dataclass
class Mesh:
    material: str          # ab v1.7 (sonst "default")
    texture: str
    verts: list
    uvs: list
    groups: list           # [(lighting_material, [(v0,t0,v1,t1,v2,t2), ...])]
    cull: int
    extra: tuple = ()


@dataclass
class Node:
    type: int
    name: str
    parent: str
    right: tuple
    up: tuple
    front: tuple
    pos: tuple
    emitter: str = ""
    mesh: Mesh = None


@dataclass
class Channel:
    name: str
    length: float
    kind: int
    keys: list


@dataclass
class TexRef:
    kind: int
    node: str
    anim: str
    offset: float


@dataclass
class Sod:
    magic: str
    version: float
    legacy: list = field(default_factory=list)
    materials: list = field(default_factory=list)
    nodes: list = field(default_factory=list)
    channels: list = field(default_factory=list)
    texrefs: list = field(default_factory=list)


def _mesh(r, ver):
    material = r.string() if ver >= 1.7 else "default"
    texture = r.string()
    nv, nuv, ng = r.u16(), r.u16(), r.u16()
    fl = r.floats(nv * 3)
    verts = [fl[i:i + 3] for i in range(0, len(fl), 3)]
    fl = r.floats(nuv * 2)
    uvs = [fl[i:i + 2] for i in range(0, len(fl), 2)]
    if not all(math.isfinite(x) for v in verts for x in v):
        raise SodError("nicht endliche Eckpunkte")
    groups = []
    for _ in range(ng):
        nf = r.u16()
        lm = r.string()
        raw = struct.unpack("<%dH" % (nf * 6), r.take(nf * 12))
        for i in range(0, len(raw), 2):
            if raw[i] >= nv or raw[i + 1] >= nuv:
                raise SodError("Index ausserhalb")
        groups.append((lm, [raw[i:i + 6] for i in range(0, len(raw), 6)]))
    cull = r.u8()
    n = r.u16()
    extra = struct.unpack("<%dH" % n, r.take(2 * n)) if n else ()
    return Mesh(material, texture, verts, uvs, groups, cull, extra)


def _mesh_v12(r):
    """v1.2: [str texture][str material][u16 nv][u16 npoly][u16 nuv] verts uvs polys{[u16 n]{v,uv}*n}; Polygone werden zu Dreiecksfaechern."""
    texture, material = r.string(), r.string()
    nv, npoly, nuv = r.u16(), r.u16(), r.u16()
    fl = r.floats(nv * 3)
    verts = [fl[i:i + 3] for i in range(0, len(fl), 3)]
    fl = r.floats(nuv * 2)
    uvs = [fl[i:i + 2] for i in range(0, len(fl), 2)]
    tris = []
    for _ in range(npoly):
        n = r.u16()
        pr = [(r.u16(), r.u16()) for _ in range(n)]
        if n < 3 or any(v >= nv or t >= nuv for v, t in pr):
            raise SodError("ungueltiges Polygon")
        for i in range(1, n - 1):
            tris.append((pr[0][0], pr[0][1], pr[i][0], pr[i][1], pr[i + 1][0], pr[i + 1][1]))
    return Mesh(material, texture, verts, uvs, [(material, tris)], 1)


def parse(data):
    r = Reader(data)
    magic = r.take(10).decode("latin1")
    if magic not in ("Storm3D_SW", "StarTrekDB"):
        raise SodError("unbekannte Kennung %r" % magic)
    ver = round(r.f32(), 2)
    if ver not in (1.2, 1.4, 1.5, 1.6, 1.7, 1.8):
        raise SodError("Version %s nicht unterstuetzt" % ver)
    s = Sod(magic, ver)
    if ver <= 1.6:
        for _ in range(r.u16()):
            s.legacy.append((r.string(), r.string(), r.take(7)))
    for _ in range(r.u16()):
        name = r.string()
        amb, dif, spe = r.floats(3), r.floats(3), r.floats(3)
        s.materials.append(Material(name, amb, dif, spe, r.f32(), r.u8()))
    for _ in range(r.u16()):
        t = r.u16()
        name, parent = r.string(), r.string()
        f = r.floats(12)
        n = Node(t, name, parent, f[0:3], f[3:6], f[6:9], f[9:12])
        if t == NODE_EMITTER:
            n.emitter = r.string()
        elif t == NODE_MESH:
            n.mesh = _mesh_v12(r) if ver < 1.4 else _mesh(r, ver)
        elif t not in (NODE_NULL, NODE_SPRITE, NODE_LOD):
            raise SodError("unbekannter Knotentyp %d (%s)" % (t, name))
        s.nodes.append(n)
    for _ in range(r.u16() if ver >= 1.4 else 0):
        name = r.string()
        nk, length, kind = r.u16(), r.f32(), r.u16()
        keys = list(r.floats(nk)) if kind == 5 else [r.floats(12) for _ in range(nk)]
        s.channels.append(Channel(name, length, kind, keys))
    if ver >= 1.6:
        for _ in range(r.u16()):
            kind = r.u8()
            node, anim = r.string(), r.string()
            s.texrefs.append(TexRef(kind, node, anim, r.f32() if ver >= 1.8 else 0.0))
    if r.p != len(data):
        raise SodError("%d Byte uebrig nach dem Parsen (Offset %#x)" % (len(data) - r.p, r.p))
    return s


def parse_file(path):
    with open(path, "rb") as f:
        return parse(f.read())


def world_matrices(sod):
    """Weltmatrizen je Knotenname: (right, up, front, pos). Elternbezug ueber Namen, '' = Wurzel."""
    by_name = {}
    for n in sod.nodes:
        by_name.setdefault(n.name, n)
    cache = {}

    def mul(a, b):  # a (lokal) dann b (Eltern); Zeilenvektor-Konvention v' = v*R + p
        rot = [tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3)]
        pos = tuple(sum(a[3][k] * b[k][j] for k in range(3)) + b[3][j] for j in range(3))
        return rot[0], rot[1], rot[2], pos

    def get(n, depth=0):
        key = id(n)
        if key in cache:
            return cache[key]
        local = (n.right, n.up, n.front, n.pos)
        par = by_name.get(n.parent)
        if par is None or par is n or depth > 64:
            res = local
        else:
            res = mul(local, get(par, depth + 1))
        cache[key] = res
        return res

    return {id(n): get(n) for n in sod.nodes}
