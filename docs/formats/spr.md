# Sprite-Beschreibungen (`Sprites/*.spr`)

Status: **Parser (`formats/Spr`) deckt sich mit unabhängiger Python-Referenz** (Zählwerte, Animationspegel). Dargestellt sind bisher nur die Sprite-Knoten von Modellen (Positionslichter).

Textdateien (24 Stück). Für Schiffslichter relevant: `lights.spr` (Sprites + Knotendefinitionen) und `animation.spr` (Keyframe-Animationen). Alle Dateien werden zu einer Bibliothek zusammengeführt.

## Syntax (aus den Dateien abgeleitet)

- `@reference=N` (auch `@referenceWidth=`/`@referenceHeight=`): Texturgröße in Pixeln, auf die sich die folgenden U/V/W/H beziehen. `@tmaterial=additive|default|interface|…`: Mischmodus der folgenden Sprites.
- Sprite-Tabelle: `Name Textur U V W H` (Pixel im Bezugsformat, V von oben). Zeilen mit `@auto=…` (Raster) sind noch nicht ausgewertet.
- Knotendefinition: `@sprite_node name sprite animation (breite,hoehe) (r,g,b) [billboard] [sprite_table]`. Tippfehler wie `(.7,.7))` kommen vor; `//billboard` ist auskommentiert (= kein Billboard).
- Animation: `@animation id`, Zeile `feld anzahl dauer step|linear`, `@keyframes`, dann `zeit wert` bzw. `zeit (r,g,b)`. Felder: `colour`, `draw` (an/aus), `offset`, `width`. Animationen wiederholen sich nach `dauer`. Die Animation `const` ist nicht definiert und heißt: immer an.

## Positionslichter

SOD-Knoten vom Typ 3 (Sprite) heißen `<knotendef>_<n>`, z. B. `redstrobe_2`; bei Verschachtelung auch `whitestrobe_1_2`. Auflösung: Namen exakt, sonst Endungen `_<Zahl>` abschneiden. Von 2267 Sprite-Knoten werden 2208 aufgelöst; die übrigen 59 sind Borg-Assimilationsflächen (`assimbig…`, `assimsmall…` an Türmen/Sensoren), die Sprites direkt benennen und eine eigene Behandlung brauchen.

Blinkmuster (Animation `draw`, `step`):
- `strobe`: 2,0 s Zyklus, an 0–0,1 s, aus, an 0,2–0,3 s, dann aus (Doppelblitz).
- `strobe1`: 4,0 s, an in der ersten Hälfte. `strobe2`: 2,0 s, an von 0,4 bis 0,5 s (Kurzblitz).
- `flicker`: Farbe 3 s linear (leichtes Flackern).

Flächige Lichtflecken: `whiteconstbig` (Sprite `Spot`, 7×18, ohne `billboard`) ist kein Punktlicht. Die Knoten sitzen auf der Untertassensektion (z. B. Fbattle, Fentd) mit der Fläche in der right/up-Ebene des Knotens (um 90° gedreht gezeichnet: Breite 7 entlang `up`, Höhe 18 entlang `right`, quer zur Schiffslänge; auf Sicht gewählt, Datenlage unklar) (`front` zeigt nach oben bzw. unten), also ein Lichtfleck, der die Hülle beleuchtet. Als Billboard gezeichnet ergab das einen störenden Lichtkegel.

Sichtbarkeit: Lichter hängen unter `lights`, nicht unter den Teilen. Lichter innerhalb (2 Einheiten Rand) der Hüllbox eines ausgeblendeten `oq`-Netzes werden mit ausgeblendet (Fbase: 21 statt 25 Lichter ohne Offiziersquartiere).

Annahmen: alle Lichter blinken synchron (Phase nicht in den Daten gefunden); Sprites mit `billboard` oder quadratischer Größe werden zur Kamera gedreht (auch ohne Flag, weil das bei Positionslichtern sichtbar besser aussieht), nur nicht-quadratische ohne Flag liegen flach in der Knotenebene; Größen (z. B. 0,5) in Modelleinheiten; Zeichnung additiv (`GL_ONE, GL_ONE`) mit Tiefentest, ohne Tiefenschreiben.

## Sprite-Tabelle mit Bildfolgen (Waffeneffekte, `weapon.spr`)

- Tabellenzeile: `Name Textur U V W H [@anim=<animation>]` (z. B. `wphoton wphoton 0 0 32 32 @anim=photanim`). Rassenvarianten teilen eine Textur und unterscheiden sich im Ausschnitt (`wphoton`/`bphoton`/`kphoton`: U = 0/32/64).
- Bildfolge (Flipbook, aus `tex_anim.spr`): `@animation id`, `offset <anzahl> <dauer> step`, `@auto=square|row|column`, `@keyframes`. Das Bild wechselt gleichmäßig (`dauer/anzahl` je Bild) und der Ausschnitt wandert um (W,H): `square` = Raster mit `ceil(sqrt(anzahl))` Spalten zeilenweise (16 → 4×4, 12 → 4 Spalten), `row` nebeneinander, `column` untereinander.
- Geprüft gegen Python-Referenz (Zellen zu mehreren Zeiten).

## Waffenfeuer im Betrachter (Sichtprobe)

`game/FireDemo`, `render/EffectRenderer`: alle normalen Waffen feuern im Takt `shotDelay` von ihren Hardpoints in Richtung `front`. Ohne Ziel/Schaden, ohne Sonderwaffen (`special = 1`, `specialEnergyCost > 0`) und ohne Waffen, deren Sprite fehlt.
- Strahl (`shotSpeed` ≥ 1e5, z. B. Phaser): Rechteck vom Hardpoint, Länge = Reichweite (im Betrachter auf 8 Modellradien begrenzt), Dauer = `lifeSpan` (höchstens `shotDelay`), Breite = `Radius`.
- Puls (`Length` > 0, z. B. Pulse-Phaser): fliegender Streifen der Länge `Length`, Breite `Radius`. Torpedo (`Length` = 0): zur Kamera gedrehter Ball mit Durchmesser `Radius`, Bildfolge des Sprites.
- Annahmen: Breite = `Radius`; Fluggeschwindigkeit im Betrachter mit 0,25 verlangsamt (`kDemoSpeedScale`), sonst fliegen Geschosse in Sekundenbruchteilen aus dem Bild; additive Mischung; Strahlen stehen still am Hardpoint, solange sie leben.
