# Inventar der Spielinstallation

Quelle: GOG-Version (Produkt-ID 1582325353), entpackt nach `originalFiles/` (lokal, nicht im Repo).
Stand der Dateien: 13.01.2022. Gesamt: **4596 Dateien, 676 MB**.
Status-Spalte: `bestätigt` = an den echten Dateien geprüft, `vermutet` = nur aus Namen/Kopfbytes geschlossen, `unbekannt`.

## Verzeichnisse (nach Größe)

| Ordner | Dateien | Inhalt | Status |
|---|---|---|---|
| sounds/ | 1163 | WAV-Soundeffekte/Sprache | vermutet (Standard-WAV) |
| Textures/ | 2072 | `Index8/`, `RGB/`: TGA plus Mip-Stufen `.1`–`.4` | bestätigt, siehe texturen.md |
| animations/ | 69 | Bink-Videos (`.bik`) | vermutet |
| __redist/, __support/ | 154+ | GOG-Beigaben (DirectX-Cabs, Runtimes) | irrelevant |
| wav/ | 5 | WAV | vermutet |
| bitmaps/, bmp/ | 209+13 | BMP (Oberfläche) | vermutet |
| bzn/ | 49 (+28 `.mdf`, 28 `.bmp`) | Karten/Missionen | Kopf/Objekte/Pfade bestätigt (bzn.md) |
| missions/ | 52 | je Mission `.dsl` + `.drl` (26+26) | `.dsl` = PE-DLL (siehe other.md) |
| SOD/ | 273 | 3D-Modelle (Storm3D) | Mesh-Layout bestätigt (v1.4–1.8), siehe sod.md |
| AI/ | 95 | `.aip` (85), `.dsl`/`.drl`, Config | teilweise |
| help/ | 140 | HTML-Hilfe | irrelevant |
| misc/, Sprites/, giddi/, 3D_Paths/, cursors/, ico/, addon/ | klein | Konfig, Sprites, Joystick-Profile, Pfade | unbekannt |

## Dateien im Wurzelverzeichnis (Auswahl)

- `Armada.exe` (PE32, Direct3D 7), `WonInterface.dll`, `gwstartrek.dll`, `binkw32.dll`, `ddraw.dll` (GOG-Wrapper)
- **Container:** `assets.zfs`, `objtxt.zfs` → `zfs.md`
- Textkonfig: `*_CFG.h`, `Input.map`, `label.map`, `Gamekey.map`, `net.ini`, `woncfg.cfg`, `events.odf`
- Sprachdateien: `*.ldl` (Kopfbytes verschleiert, unbekannt)
- `startrek.zix`, `texture.dat`, `shell.set`, `mshell.set` → unbekannt

## Häufigste Endungen

tga 1369 · wav 1168 · sod 273 · bmp 251 · `.2` 187 · `.1` 184 · `.3` 179 · `.4` 144 · bik 69 · bzn 49 · aip 86 · dsl 30 · drl 29 · mdf 28 · spr 24.
