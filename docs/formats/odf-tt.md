# ODF- und TT-Dateien (Spielobjekt-Daten)

Status: **Grammatik ausgewertet, C++-Parser (`formats/Odf`, `formats/Tt`) deckt sich mit unabhängiger Python-Referenz bei allen 489 ODF + 27 TT.** Zeichensatz: CP1252 (Byte 0x92 = Apostroph in Namen wie J'hong).

## ODF (Object Definition File), 489 Stück in `assets.zfs`, dazu `events.odf` im Klartext im Wurzelordner

Textformat, Zeilen im Stil `schluessel = wert`:

```
name = "alien"
displayName = "Alien"
repairStrength = 1.25f
//Kommentar
#include "gcloak.odf"
```

- Zahlen mit `f`-Suffix für Gleitkomma, Zeichenketten in Anführungszeichen, `//`-Kommentare, `#include "datei.odf"`.
- Namenslisten können mehrzeilig sein (Schiffsnamen `kbattle.odf` usw.).
- `events.odf` beginnt mit `#EVENT data file`.
- Wichtigste Quelle für **Spielwerte** (Schiffe, Gebäude, Waffen). Spielwerte nie hartkodieren, sondern hieraus laden.

## TT (Tech Tree), 27 Stück

Eine Zeile je Objekt: `<odf-datei> <anzahl-voraussetzungen> <voraussetzung>…`

```
fbase.odf    0
fconst.odf   1 fbase.odf
ffreight.odf 2 fbase.odf fmining.odf
```

Vermutet: je Volk eine Datei (`fed1.tt`, `borg4.tt` …), die Ziffer vermutlich ein Szenario/Level-Satz. Zuordnung prüfen.

## Grammatik (aus Zählung aller Dateien)

- Optional `[Abschnitt]` am Anfang; `#include "x.odf"` nur am Dateianfang (Vererbung, Basis zuerst, eigene Zuweisungen überschreiben).
- `schluessel = wert wert …`; Werte: Zeichenkette, Ganzzahl, Gleitkomma (`2.5f`, `1e6`), Bezeichner (`true`/`false`, Namen).
- Optionales `;` am Ende, `//` beginnt Kommentar; Fortsetzungszeilen beginnen mit `"`, Ziffer, `-` oder `.`.
- Vereinzelt Zeilen ohne `=` (Müll): überspringen und als Warnung merken. CRLF. Schlüssel ohne Beachtung der Schreibweise.
- Mehrfach vorkommende Schlüssel: der letzte gilt (Annahme, noch nicht an Spielverhalten geprüft).
- TT: 4333 Zeilen, alle passen zu `<odf> <n> <n Namen>`.
- Zur Laufzeit kommen ODF/TT aus `assets.zfs` (`ZfsSource`), lose Dateien können per `OverlaySource` vorgehen.

## Schlüssel (Schiffe/Gebäude, ausgewertet in `data/UnitDef`)

- **Verknüpfung ODF ↔ Modell:** Der ODF-Name ist der SOD-Name (`fcruise1.odf` ↔ `Fcruise1.SOD`, belegt für 169 Einheiten). Es gibt keinen Schlüssel, der das Modell nennt.
- Vererbung: `fcruise1` → `cruise1` → `craft` (Basiswerte, Teilsysteme, Ereignisse); Gebäude → `station`. `classLabel`: `wingman` = Schiff (59), `starbase`, `shipyard`, `research`, `mining`, … ; Waffen `cannon`, `launcher`, `phaser`, `pod` …
- Werte: `unitName`, `tooltip`, `verboseTooltip`, `race`, `buildTime`, `officerCost`, `crewCost`, `dilithiumCost`, `maxHealth` (Schild), `shieldRate`, `shieldDelay`, `maxSpecialEnergy`, `specialEnergyRate`, `rangeScan`, `attackPower`, `physicsFile`, `possibleCraftNames`.
- Teilsysteme `engines`/`lifeSupport`/`weapons`/`shieldGenerator`/`sensors`/`hull` + `HitPoints`, `HitPercent`, `RepairTime`, `CrewLoss`, `TargetHardpoints` (Namen von Hardpoint-Knoten im SOD).
- Waffen: `weaponN = "<waffen-odf>"` mit `weaponHardpointsN = "hp01" …` (N ab 1, bei Stationen bis 8+). Waffen-ODF: `wpnName`, `range`, `shotDelay`, `hitChance`, `ordName` (Geschoss-ODF), `fireSound`, `special`.
- Physik (`battphys.odf`): `impulseSpeed`, `warpSpeed`, `yawRotationRate`, `turnRadius`, `maxRoll` und viele Regelkoeffizienten (`shp_*`, `sy_*`, `sp_*`, `spl_*`, noch nicht ausgewertet).
- Geprüft: bei allen 169 Einheiten mit gleichnamigem Modell stimmt die Zahl der Waffen/Hardpoints/Werte mit der Python-Referenz überein; alle Waffen-Hardpoints existieren im SOD, außer bei `zcarddes` (2) und `zferengi` (1) (Eigenheit der Originaldaten).
- Einheiten (Sekunden, Entfernungen) sind aus den Zahlen abgeleitet, nicht belegt.

## Geschosse (`ordName` einer Waffen-ODF, 118 Waffen → 83 Geschoss-ODFs, `ripple` fehlt im Original)

- Muster **Wert mit Ausnahmen je Ziel**: `damageBase = 24 "zmama.odf" 2` (Standard 24, gegen `zmama` 2); gleiches Muster bei `hitChance` der Waffe (z. B. `0.7 "bbattle.odf" 1.0 …`). → `PerTarget::For(ziel)`.
- Schlüssel: `classLabel` (phaser, pphaser, photontorpedo, plasmacannon, chainpulsar, tractor_bm, …), `damageBase`, `damageVariance`, `damageThreshold`, `shieldCrewModifier`, `hullCrewModifier`, `specialEnergyCost`, `lifeSpan`, `shotSpeed` (1e6 = praktisch sofort, Strahl), `shotAccel`, `omegaTurn`, `seekTime`, `radius`/`Radius`, `Length`, `ignoreShield`, `Sprite` (Name aus `Sprites/*.spr`), `shotGeometry` (SOD, z. B. `wtractor.sod`), `hitSound`, `lightColor`/`lightFalloffStart`/`lightFalloffRange`, Rauch (`smoke*`).
- Spezialklassen haben weitere Schlüssel (Ketten: `chainRadius`, `maxHits`, `chainDamageModifier`; Minen, Boarding u. a.), noch nicht erfasst.
- Geprüft: 118 Waffen + 83 Geschosse gegen Python-Referenz ohne Abweichung.

## Offen

- Spezialfähigkeiten, Bau-Menüs (`buildItemN`), Ereignisse (`events.odf`), Physik-Koeffizienten.
- Bedeutung der Dateinamens-Präfixe (`f`=Föderation, `k`=Klingonen, `r`=Romulaner, `b`=Borg vermutet; `g`, `h`, `m` unklar).
- TT-Dateinamen (`fed1.tt` …) und ihre Zuordnung zu Szenarien.
