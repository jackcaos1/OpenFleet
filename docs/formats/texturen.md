# Texturen (`Textures/`)

Status: **Format bestätigt, Laufzeitleser fehlt noch.**

- `Textures/RGB/`: unkomprimiertes TGA (Typ 2), 24 oder 32 Bit (402 + 247 Dateien).
- `Textures/Index8/`: palettiertes TGA (Typ 1), 8 Bit, 24-Bit-Palette (720 Dateien). Gleiche Texturen wie in RGB, nur kleiner.
- **Mip-Stufen:** Dateien mit Endung `.1` bis `.4` sind keine eigenen Bilder, sondern dieselbe Textur verkleinert, wieder als TGA:
  Beispiel `Bcruise1.TGA` 256×256, `.1` 128×128, `.2` 64×64, `.3` 32×32, `.4` 16×16.
  (Größen am Beispiel nachgerechnet: 18 Byte Kopf + Pixeldaten + 26 Byte Fußzeile.)
- Sonderfälle: `.jbf`, `.pal`, `.6`, `.56`, Dateien ohne Endung (5 in Index8).
- Zuordnung vom Modell: `Mesh.texture` = Dateiname ohne Endung, Groß-/Kleinschreibung egal.
- Alpha: 32-Bit-Dateien enthalten einen Alphakanal, 8-Bit-Varianten nicht. **Kein Cutout:** In `Fbase.TGA` ist Alpha genau auf den
  weißen Flächen 0 (961 Texel, alle RGB 255/255/255), und diese Flächen sind im Spiel sichtbar. Wahrscheinliche Deutung: Maske
  (z. B. für Teamfarben), noch nicht belegt. Der Client ignoriert Alpha standardmaessig (umschaltbar: Test/Mischen).
  Weitere Beobachtung (Fbase): Alpha 0 betrifft nur 20 Dreiecke der Gruppe `base_fed` (Aequatorring mit 16 Feldern und 4 Felder der unteren
  Scheibe). Die Gruppen `glow_*` und `team_*` liegen auf voellig deckenden Texeln. Das Orange der Pylonen im Spiel stammt aus den Materialien
  `glow_orange1..8` (Diffusfarbe 1,0/0,54/0,22), nicht aus der Textur.

## Offen

- Bedeutung der Sonderendungen, Teamfarben (`team_*`-Materialien: wird Textur oder Farbe getönt?).
- Ob die Engine die Index8- oder RGB-Fassung bevorzugt (Armada.exe/Konfiguration).
