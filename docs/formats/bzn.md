# BZN – Karten und Missionen

Stand: Kopf, Objektliste und Pfade sind entschlüsselt und gegen eine unabhängige Python-Referenz (`tools/bzn_tool.py`) an **allen 49** Dateien aus `bzn/` verglichen (0 Abweichungen, `fleet_data_dump` ↔ `data_dump_ref.py`). Die klassenspezifischen Zusatzdaten der Objekte sind **nicht** entschlüsselt, sondern werden übersprungen.

## Aufbau der Datei

1. **Textkopf** (CRLF): `version [1] =`, Versionszahl (2050/2051), `saveGameDesc = <Hex>` (ASCII-Beschreibung, nullgefüllt), `binarySave [1] =`, `true`.
2. **Binärstrom** direkt danach, ohne Feldnamen. Jedes Element: Byte 0 = Typ, Bytes 1–3 unbenutzt (Müll, meist `cd`), Bytes 4–7 = u32 Größe, dann Daten. Alle 49 Dateien zerlegen sich damit exakt bis zum Dateiende.

| Typ | Größe | Bedeutung |
|---|---|---|
| 0 | 4 | Zeiger/Zahl (Müll); 64 = Kartentitel, 4 = Pfadtyp |
| 1 | 1 | Bool/Byte |
| 2 | n | nullgefüllter Text fester Länge |
| 4 | 4 | int32 |
| 5 | 4 | float |
| 7 | 8 | ODF-Name (Klassenname) |
| 8 | 4 | Zeiger (Laufzeitwert, unbrauchbar) |
| 9 | 12·n | n Vektoren (x,y,z) |
| 11 | 48 | 3×3-Drehung (Zeilen rechts, oben, vorne) + Position |

## Abschnitte

**Kopf** (Elemente 0–16): 0 Dateiname (16), 2 Kartenname (100), 3–8 sechs floats (vermutet: x min/max, y min/max, z min/max; üblich 0/6400, −28,5/105,5, 0/6400), 13 Hintergrund-SOD (`mbg*.sod`), 14 Tech-Tree (`.tt`), 15 Rasse des Spielers, 16 Objektanzahl.

**Objekt** (Anfang erkennbar an der festen Folge `7 ODF-Name`, `4 Handle (Bit 31 gesetzt)`, `9 Position`, `4 Team`, `2/40 Label`, `8 Zeiger`, `11 Matrix`): danach folgen klassenabhängig weitere Felder (Gesundheit, Name, Rasse, Untersysteme; Konstruktionsschiffe haben eingebettete Unterobjekte mit leerem ODF-Namen). Weil jeder Objektanfang so eindeutig ist, braucht der Loader das Schema der Zusatzdaten nicht: Die Anzahl gefundener Anfänge stimmt in allen 49 Dateien mit der Zahl im Kopf überein (17 595 Objekte).
- Team: 0 = neutral (Umgebung, Nebel, Monde, Wurmlöcher, Licht), 1–8 = Teamnummer.
- Die Positions-Y ist meist 38,5 (Ebene).
- Gut zu wissen: 15 715 der 17 595 Objekte sind Umgebung (`mafield`, `mnebula1–5`, `mdmoon`, `mwrmhole`, `mblkhole`), nur wenige Hundert sind Schiffe/Stationen.

**Fußteil** (beginnt an der ersten Folge `2/40 Name ("EmptyMission")`, `8`, `2/64`, `2/64`, `0/64 Titel`): davor liegt eine Tabelle für 9 Teams (je 7 Elemente: Sichtweiten o. ä., Bedeutung unbekannt). Nach dem Titel: `4` (0), `4` Pfadanzahl, dann je Pfad: `0/4` Zeiger, `4` Namenslänge, `2` Name (genau so lang, ohne Nullbyte), `4` Punktanzahl, `9` Punkte, `0/4` Typ, `5` Geschwindigkeit/Zahl (z. B. 0,09 oder 0,01). Namen wie `Camera_1`, `RotatingBelt`; die Kamerapfade stammen aus den Missionsskripten.

## Offen / Annahmen

- Bedeutung der sechs Grenzwerte im Kopf, der Team-Tabelle, des zweiten int im Objektrest (z. B. 5, 0, 6, 8 bei `zferscav*`) und des Pfadtyps ist nicht gesichert.
- Gesundheit und Namen der Objekte stehen vermutlich in den Zusatzfeldern (z. B. 1, 1500, 1500 nach dem Anzeigenamen), sind aber nicht ausgewertet.
- Das Terrain steckt nicht in der BZN (Karten sind flach, y = 38,5); `.mdf`/`.bmp` mit gleichem Namen noch ungeprüft.
- Missionslogik liegt in `missions/*.dsl` (native DLLs), nicht in der BZN.

## Code

`engine/src/formats/Bzn.h/.cpp` (`ParseBzn`), Referenz `tools/bzn_tool.py`, Zeilen `BZN …` in `fleet_data_dump` / `data_dump_ref.py`, Test `TestBzn` (synthetische Datei).
