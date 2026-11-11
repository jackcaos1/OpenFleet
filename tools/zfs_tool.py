#!/usr/bin/env python3
"""ZFS-Archive lesen (Format: docs/formats/zfs.md).

  python3 -I tools/zfs_tool.py list    <archiv.zfs>
  python3 -I tools/zfs_tool.py extract <archiv.zfs> <zielordner>

Der Zielordner muss ausserhalb von Git liegen oder in .gitignore stehen (work/).
Der Inhalt ist mit einem 4-Byte-XOR-Schluessel aus dem Header (0x14) verschleiert;
Schluessel 0 bedeutet: Klartext.
"""
import os
import struct
import sys

ENTRY = struct.Struct("<16sIIIII")  # name[16], offset, index, size, timestamp, pad
HEADER = struct.Struct("<4sIIIIIII")  # magic, version, namelen, per_block, total, key, f18, next


def read_archive(path):
    data = open(path, "rb").read()
    magic, ver, namelen, per_block, total, key, f18, nxt = HEADER.unpack_from(data, 0)
    if magic != b"ZFSF" or ver != 1 or namelen != 16:
        raise ValueError(f"{path}: kein bekanntes ZFS (magic={magic!r}, ver={ver}, namelen={namelen})")
    pos, seen, entries = HEADER.size, 0, []
    while seen < total:
        n = min(per_block, total - seen)
        for i in range(n):
            name, off, idx, size, ts, pad = ENTRY.unpack_from(data, pos + i * ENTRY.size)
            if off + size > len(data):
                raise ValueError(f"{path}: Eintrag {seen + i} liegt ausserhalb der Datei")
            entries.append((name.split(b"\0")[0].decode("latin1"), off, size, ts))
        seen += n
        if seen < total:
            if not nxt:
                raise ValueError(f"{path}: Blockkette endet nach {seen} von {total} Eintraegen")
            pos = nxt + 4
            nxt = struct.unpack_from("<I", data, pos - 4)[0]
    return data, key, entries


def decode(blob, key):
    """XOR mit 4-Byte-Schluessel; nur volle 4-Byte-Woerter, der Rest (size % 4) bleibt Klartext."""
    if key == 0:
        return blob
    k = struct.pack("<I", key)
    out = bytearray(blob)
    for i in range(len(out) - len(out) % 4):
        out[i] ^= k[i & 3]
    return bytes(out)


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    data, key, entries = read_archive(path)
    if cmd == "list":
        print(f"{len(entries)} Dateien, Schluessel {key:#010x}")
        for name, off, size, ts in entries:
            print(f"{size:8d}  {off:#09x}  {name}")
    elif cmd == "extract":
        out = sys.argv[3]
        os.makedirs(out, exist_ok=True)
        for name, off, size, ts in entries:
            safe = os.path.basename(name)  # Pfadtricks verhindern
            with open(os.path.join(out, safe), "wb") as f:
                f.write(decode(data[off:off + size], key))
        print(f"{len(entries)} Dateien nach {out}")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
