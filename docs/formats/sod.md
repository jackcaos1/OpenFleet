# SOD – 3D-Modelle (Storm3D)

Status: **vollständig gelesen für v1.2, 1.4–1.8 (272 von 273 Dateien, exakt bis zum letzten Byte).** Offen: `Wviracod.SOD` (v1.0).
Werkzeuge: `tools/sod_format.py` (strenger Parser), `tools/sod_tool.py` (`info`, `obj`, `preview`, `check`).
Alle Zahlen little-endian. Strings: `u16 Länge` + ASCII, ohne Terminator.

Herkunft der Erkenntnisse: eigene Analyse der Originaldateien; Gegenprüfung des Aufbaus an den Community-Projekten `stau` (MIT) und
Blender-Addon von SomaZ (GPL-2.0), die lokal in `reference/` liegen (nicht im Repo). Aus ihnen wurde **kein Code übernommen**.

## Gesamtaufbau

```
char[10] magic      "StarTrekDB" (v1.0–1.5) oder "Storm3D_SW" (v1.6–1.8)
f32      version    1.0, 1.2, 1.4, 1.5, 1.6, 1.7, 1.8
[v ≤ 1.6]  u16 n, n × { string id1, string id2, u8[7] }          "Legacy"-Liste (Bedeutung unbekannt)
u16 nMaterial, nMaterial × Material
u16 nNode,     nNode × Node
[v ≥ 1.4]  u16 nChannel, nChannel × Kanal                        Animationen
[v ≥ 1.6]  u16 nRef,     nRef × Texturreferenz
```
Danach muss die Datei zu Ende sein (v1.2 hat keinen Animationsteil).

## Material

`string name; f32[3] ambient; f32[3] diffuse; f32[3] specular; f32 glanz; u8 lichtmodell`
Beispiele: `base_fed`, `team_fed`, `glow_blue`, `warp_fed`. Mit `Mesh.groups[i].material` über den Namen verknüpft.

## Knoten

```
u16 typ                   0 = Null/Hardpoint, 1 = Mesh, 3 = Sprite, 11 = LOD-Steuerung, 12 = Emitter
string name
string parent             "" = Wurzel; Elternbezug über den NAMEN (7678 von 7678 Knoten gültig)
f32[3] right, f32[3] up, f32[3] front, f32[3] position          lokale Transformation
typ 12: string emitterId
typ 1:  Mesh
```
Verteilung: 3453 Null/Hardpoints, 1795 Meshes, 2267 Sprites, 163 Emitter (alle 272 Dateien). LOD-Typ 11 kommt nicht vor.
Weltmatrix = lokal, dann Eltern (Zeilenvektor: `v' = v·[right;up;front] + position`).

## Mesh (v ≥ 1.4)

```
[v ≥ 1.7] string material      meist "default"
string texture                 Name einer Datei in Textures/ (ohne Endung), leer = Farbfläche
u16 nVertex, u16 nUV, u16 nGroup
f32[3] × nVertex               Eckpunkte
f32[2] × nUV                   Texturkoordinaten
nGroup × { u16 nFace; string lichtMaterial; nFace × 3 × (u16 vertexIdx, u16 uvIdx) }     Dreiecke
u8 cull                        0 = beidseitig, 1 = Rückseiten verwerfen
u16 n, n × u16                 Zusatzliste (in allen Dateien leer)
```
Keine Normalen gespeichert. Vertex- und UV-Index sind getrennt (wie bei OBJ).

## Mesh v1.2 (11 Dateien)

`string texture; string material; u16 nVertex, u16 nPolygon, u16 nUV; Eckpunkte; UVs; nPolygon × { u16 n; n × (vertexIdx, uvIdx) }`.
Polygone werden in der Engine zu Dreiecksfächern. Kein Cull-Byte, kein Animationsteil.

## Animation (v ≥ 1.4)

Kanal: `string node; u16 nKey; f32 dauer; u16 typ` (5 = Skalierung: `f32[nKey]`, sonst `nKey × f32[12]` Matrix+Position).
Texturreferenz (v ≥ 1.6): `u8 typ; string node; string anim; [v ≥ 1.8] f32 offset`.
Wird gelesen und beim Prüfen konsumiert, **Bedeutung und Abspiellogik noch nicht ausgewertet**.

## Materialfarben und Lichtmodell (Beobachtung)

Das Material einer Dreiecksgruppe (`lichtMaterial`) bestimmt die Farbe zusammen mit der Textur: Farbe = Textur x Diffusfarbe.
Lichtmodell 0 = leuchtend/unbeleuchtet (`glow_*`, `warp_*`, `impulse_*`, `bussard_*`), 1 und 2 = beleuchtet. Materialien `team_*` tragen eine rote
Platzhalterfarbe (0.99/0/0), vermutlich zur Laufzeit durch die Teamfarbe ersetzt. Klingonen: `base_kling` hat ein tuerkises Diffus (0.55/1/0.92).

## Spielerfarbe, Orange, Borg-Ueberzug (am Vergleich mit Spielbildern, Fedbase)

- **Orange an den Pylonen** ist fest im Modell: Die Gruppen `glow_orange1..8` (je 4 Dreiecke auf den Knoten `fbasew_1..8`, Diffusfarbe 1,0/0,54/0,22, Lichtmodell 0).
  Keine Animationskanaele/Texturreferenzen in dieser Datei, keine Abhaengigkeit von der Spielerfarbe.
- **Spielerfarbe** ist eine Einfaerbung der Textur: Gruppe `team_fed_1` (76 Dreiecke, y 21.7..45, direkt ueber dem Aequatorring). Die rote Diffusfarbe der Datei ist
  Platzhalter. Alpha der Basistextur gehoert nicht dazu (Basismodell ist undurchsichtig).
- **Borg-Ueberzug** (`borg*`-Knoten mit Textur `Bcruise1`): Alpha der Textur schneidet das Gitter aus (Alpha 0 = durchsichtig), darunter ist die Spielerfarbe sichtbar.
  `team_Bglow` ist Borg-Gruen und wird nicht mit der Spielerfarbe eingefaerbt.

## Texturzuordnung

Der Texturname des Meshes ist der Dateiname in `Textures/RGB` (oder `Index8`), ohne Endung, Groß-/Kleinschreibung egal. Details: `texturen.md`.
Ergebnis: 1640 von 1795 Meshes finden ihre Datei, 134 haben bewusst keinen Namen (Glüh-/Blitzflächen), 21 verweisen auf fehlende Dateien (u. a. `Mbg01.1`–`.6`, `Mplanet`, `Wclass1probe`, `noIcon`).

## Knotennamen mit besonderer Rolle (Beobachtung, Engine-Verhalten noch nicht belegt)

Mesh-Knoten haengen unter Null-Knoten mit festen Namen. Zeichnet man alle Meshes gleichzeitig, liegen identische Flaechen uebereinander (Flimmern, falsche Tiefe):
- `lod0`, `lod1`, `lod2`, ... : Detailstufen desselben Objekts. Hoehere Zahl = mehr Dreiecke (z. B. Fcruise1: lod2 512, lod1 350, lod0 136).
- `damage` (Unterbaeume `borg`, `borgsmall/med/big` und Leuchtflaechen): Schadens- bzw. Borg-Uebernahmezustaende, im Normalzustand nicht sichtbar.
- `oq`: Ausbauten der Offiziersquartiere (`oq1..oqN`, je Ausbaustufe; im Original-Code belegt, siehe docs/model_audit.md). Die fruehere Vermutung "Okklusionshuellen" war falsch.
- weitere Muster: `geometry`, `root`, `ore`/`oreglow` (Erz auf Bergbauschiffen), `animation`, `planet_system`.
Der Client zeigt derzeit nur die hoechste Detailstufe ohne `oq`/`borg*`/`damage*` (`BuildOptions::hide_helper_nodes`).

## Offen

- `Wviracod.SOD` (v1.0, einzige Datei).
- Legacy-Liste (v ≤ 1.6): Bedeutung der beiden Namen und der 7 Bytes.
- Zusatzliste am Meshende, Lichtmodell-Werte, Sprite- und LOD-Knoten.
- Animationsdaten: Interpolation, Zuordnung `period`, Texturanimationen.
- Matrix-Konvention nur bei Identität und Elternketten plausibel geprüft, Drehung noch nicht an einem Modell mit rotierten Teilen verifiziert.
