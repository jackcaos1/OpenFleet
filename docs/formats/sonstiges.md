# Sammelbefunde (noch keine eigene Spezifikation)

Alles hier ist Stand erster Sichtung. Status je Punkt in Klammern.

## Engine-Herkunft
- Hypothese "Quake III" aus dem ersten Entwurf ist **widerlegt**. Die Dateien zeigen Storm3D-Magic (`.sod`), `.bzn`-Karten, `.odf`-Objekte und `BZONE.FNT`, also Verwandtschaft zur Battlezone-/Storm3D-Linie von Activision (vermutet, nur aus Namen/Magic geschlossen).
- `Armada.exe`: PE32, Direct3D 7 (`IDirect3D7`), enthält Debugpfad `D:\Startrek\Debug\Armada.pdb` und C++-RTTI-Namen (z. B. `Armada_Camera`) → gute Ausgangslage für Ghidra (bestätigt).

## SOD – 3D-Modelle (273 Dateien)
- Beginnt mit ASCII `Storm3D_SW`, danach float `1.6` (`cd cc cc 3f`), ein u16-Zähler (`0x15`) und Einträge mit u16-Längenpräfix + Namen (`crewfbas5`, `sensorfmin` …, wohl Hardpoints/Teile). (vermutet)
- Als Nächstes: Struktur durchgehen, Vertexdaten finden, Viewer bauen.

## BZN – Karten/Missionen (49 + `.mdf` 28)
- Kopf, Objektliste und Pfade entschlüsselt und verifiziert: siehe `bzn.md`. (bestätigt)

## KI
- `AI/*.dsl`, `missions/*.dsl`: beginnen mit `MZ`/`PE`, also **kompilierte x86-DLLs**, kein Skript. Die Spiellogik der KI steckt in nativem Code, muss später aus dem Disassembly nachgebaut werden. (bestätigt für Magic)
- `.drl` (Regeln) und `.aip` (86 Stück, wohl KI-Pläne) noch nicht geprüft.

## Texturen
- `Textures/Index8|RGB/*.tga`: unkomprimierte TGA, Typ 2, Beispiele 64×64 und 128×128 mit 32 bpp (aus Header). (bestätigt für Stichprobe)
- Dateien `.1`–`.4` (z. B. `Romega.1`, `kbattle.4`): Zweck unbekannt (Varianten? Schadensstufen? Mip-Ebenen?).
- `Armada.pal` (768 Byte = 256×RGB-Palette), `texture.dat` (8 Byte). (vermutet)

## Sprachdateien (`*.ldl`)
- `string.ldl`, `language.ldl`, `default.ldl`: alle beginnen mit denselben 12 Byte `9a 41 db e2 20 e3 b9 63 ff ef d2 …` → vermutlich eigene Verschleierung/Verschlüsselung, **nicht** das ZFS-XOR-Verfahren. (unbekannt)

## Konfiguration / Textformate
- `*_CFG.h`, `Input.map`, `label.map`, `Gamekey.map` (Kommentare mit `#`), `net.ini`, `woncfg.cfg`: Klartext, direkt lesbar. (bestätigt)
- `shell.set`, `mshell.set`: Binär, enthält Spielername. (vermutet: Einstellungen/Profil)
- `startrek.zix`: Textkopf `002009` und Verzeichnislisten wie `d:\bzone\bzone…`. Wirkt wie ein Dateiindex. (unbekannt)
- `giddi/*.std`: Joystick-Profile. `3D_Paths/*.pth`: Kamerapfade (vermutet).

## Multiplayer
- Original nutzt WON-Server (`armada.*.won.net`, dauerhaft abgeschaltet) und DirectPlay/IPX (GOG liefert `ipxwrapper`). Eigenes Netzwerkprotokoll müsste aus der Exe rekonstruiert werden. (bestätigt als Konfig, Protokoll unbekannt)
- `kver.pub`: RSA-Public-Key (816 Byte), vermutlich für WON-Authentifizierung. Für den Client irrelevant.
