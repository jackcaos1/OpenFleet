# ZFS-Archive (`*.zfs`)

Status: **bestätigt** an `assets.zfs` (516 Dateien) und `objtxt.zfs` (81 Dateien). Alle Einträge liegen innerhalb der Datei,
Indizes laufen lückenlos 0..n-1, alle 597 Dateien entschlüsseln zu Text. Werkzeug: `tools/zfs_tool.py`.

Alle Zahlen little-endian.

## Kopf (32 Byte)

| Offset | Typ | Wert/Bedeutung |
|---|---|---|
| 0x00 | char[4] | `ZFSF` |
| 0x04 | u32 | Version, immer 1 |
| 0x08 | u32 | Namenslänge = 16 |
| 0x0C | u32 | Einträge pro Verzeichnisblock = 100 |
| 0x10 | u32 | Gesamtzahl der Dateien |
| 0x14 | u32 | XOR-Schlüssel (0 = Klartext), `assets.zfs`: `0x9466279b` |
| 0x18 | u32 | immer 28, Bedeutung **unbekannt** |
| 0x1C | u32 | Offset des nächsten Verzeichnisblocks, 0 = keiner |

## Verzeichnisblock

Direkt nach dem Kopf (Offset 0x20) stehen bis zu 100 Einträge à 36 Byte, danach folgen die Dateidaten
(erster Datenoffset = 0x20 + 100·36 = 0xE30). Weitere Blöcke liegen an dem Offset, auf den der vorherige Block zeigt:
`[u32 nächster Block][Einträge …]`, danach wieder Daten. Der letzte Block hat Zeiger 0. 516 Dateien = 5 Blöcke à 100 + 1 Block à 16.

Eintrag (36 Byte):

| Offset | Typ | Bedeutung |
|---|---|---|
| 0 | char[16] | Name, mit Nullbytes aufgefüllt. **Kein** Terminator bei 16 Zeichen (Beispiel: `testobjectives.t` ist abgeschnitten) |
| 16 | u32 | absoluter Offset der Daten |
| 20 | u32 | laufender Index |
| 24 | u32 | Größe in Byte |
| 28 | u32 | Unix-Zeitstempel (Anfang 2000) |
| 32 | u32 | immer 0 |

## Verschleierung

Inhalt = Rohdaten XOR Schlüssel, wobei die 4 Schlüsselbytes in Dateireihenfolge (`9b 27 66 94`) zyklisch pro Datei ab Dateianfang verwendet werden.
**Nur volle 4-Byte-Wörter** werden verändert, die letzten `Größe mod 4` Bytes sind Klartext.
Gefunden per Häufigkeitsanalyse (Schlüssellänge 4, 100 % druckbare Zeichen); der Schlüssel steht im Header.

## Inhalt

- `assets.zfs`: 489 × `.odf`, 27 × `.tt` → `odf-tt.md`
- `objtxt.zfs`: 81 Missionsziel-Texte (`.txt`, CP1252, Klartext)

## Offen

- Bedeutung von Header 0x18 (=28).
- Ob die Dateidaten lückenlos aneinanderliegen (nicht geprüft).
- Ob `Armada.exe` weitere Archive öffnet (`startrek.zix`?).
