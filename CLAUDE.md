# CLAUDE.md – Armada-Android (Arbeitstitel: "OpenArmada")

Nativer Android-Client für das RTS *Star Trek: Armada* (Activision / Mad Doc Software, 2000).
Kein Emulator, kein Wine: Die Engine wird neu geschrieben, die Originaldateien des Spielers werden zur Laufzeit als Assets eingelesen (Ansatz wie OpenRA / OpenMW / OpenRCT2).

Sprache: Kommunikation mit Jack auf **Deutsch**. Alles, was auf GitHub sichtbar wird (Commits, PRs, Issues, README, neue Kommentare), auf **Englisch**, siehe Abschnitt 9. Code-Identifier auf Englisch.

---

## 1. Rechtliche und ethische Leitplanken (nicht verhandelbar)

- **Keine Originaldateien im Repo.** Weder Texturen, Modelle, Sounds, Karten, Videos, Binaries (EXE/DLL) noch Auszüge davon. `.gitignore` enthält `assets/`, `original/`, `*.exe`, `*.dll`, `*.pak` o. ä. Der Nutzer wählt beim ersten Start seine eigene Installation aus (Storage Access Framework).
- **Keine Dekompilat-Kopien.** Ghidra-/IDA-Output wird nie eingecheckt. Erkenntnisse werden in eigenen Worten in `docs/` dokumentiert (Dateiformate, Datenstrukturen, Regeln), der Code entsteht aus diesen Spezifikationen (Clean-Room-Prinzip, so weit praktikabel).
- Reverse Engineering dient der Interoperabilität (in Deutschland in engen Grenzen nach § 69e UrhG zulässig). Kein Umgehen von Kopierschutz, keine Cracks, kein "NoCD".
- **Keine Marken im Namen/Icon:** Nicht "Star Trek", "Armada", "Starfleet" o. ä. im Paketnamen, App-Namen oder Store-Auftritt. Eigene Icons und Grafiken.
- Eigene Lizenz für den Code: GPLv3 (oder MIT, vor dem ersten öffentlichen Release entscheiden).
- Kein Fremdcode aus verwandten Engines (z. B. Battlezone/Storm3D-Ableger) ohne bewusste Lizenzprüfung. Verwandte Formate werden nur dokumentiert und neu implementiert.

## 2. Ziel und Scope

**MVP (Meilenstein 1–4):**
1. Spielverzeichnis einlesen, alle Archive/Dateien erkennen.
2. Modelle, Texturen und UI-Grafiken laden und anzeigen.
3. Eine Skirmish-Karte rendern, Schiffe darstellen, Kamera steuern.
4. Einheiten auswählen, bewegen, angreifen. Einfache KI-Gegner.

**Später:** Kampagne, Ressourcen-/Baulogik komplett, Sound/Musik, Videos (Cutscenes), Multiplayer (LAN-/Original-Protokoll nur wenn realistisch).

**Nicht-Ziele:** iOS, Windows-Port (nur als Nebenprodukt für Debugging auf dem Desktop), Mods-Tooling, Original-Binärkompatibilität bei Savegames (optional später).

> Armada II (2001) ist ausdrücklich **nicht** Teil des Scopes, solange Armada I nicht läuft. Architektur aber so wählen, dass II später denkbar bleibt.

## 3. Tech-Stack (Vorschlag, vor Start bestätigen)

- **Core-Engine:** C++17, plattformunabhängig, läuft auf Desktop-Linux (Entwicklung/Debugging) und Android.
- **Android-Hülle:** Kotlin, Gradle, NDK; `GameActivity` oder SDL2-Activity.
- **Rendering:** OpenGL ES 3.0 (Fallback-Gedanke: Vulkan später, nicht jetzt).
- **Fenster/Input/Audio:** SDL2 (läuft auf Android und Desktop), Audio via SDL_mixer oder miniaudio.
- **Build:** CMake, Gradle für Android. Desktop-Build ist **First Class**: Alles muss zuerst am PC testbar sein.
- **Tools (Python 3, mit `python -I` ausführen, wenn sie fremde Dateien lesen):** Format-Analyse, Konverter, Hexdump-Helfer.
- **RE-Werkzeuge:** Ghidra, ImHex (Pattern-Dateien `.hexpat` einchecken), `binwalk`, `strings`, optional Wireshark für Netzwerkprotokoll.

## 4. Repo-Struktur

```
/engine/        C++-Core (formats/, render/, sim/, ai/, ui/, audio/, net/)
/android/       Gradle-Projekt, Kotlin-Hülle, JNI-Brücke
/desktop/       Desktop-Launcher für schnelles Debugging
/tools/         Python-/C++-Werkzeuge (Extraktor, Konverter, Viewer)
/docs/          Format-Spezifikationen und RE-Notizen (eigene Worte!)
  formats/      eine Datei pro Dateiformat
  sim/          Spielregeln, Werte, Formeln
  protocol/     Netzwerk (falls relevant)
/tests/         Unit-Tests, Format-Parser-Tests (mit synthetischen Testdaten)
/originalFiles/ LOKAL, in .gitignore: entpackte GOG-Installation (4596 Dateien, 676 MB)
/work/          LOKAL, in .gitignore: entpackte/konvertierte Zwischenstände (z. B. ZFS-Inhalt)
```

## 5. Arbeitsweise

**Reverse-Engineering-Workflow pro Format/System:**
1. Dateien inventarisieren (`tools/inventory.py`): Endungen, Größen, Magic Bytes, Entropie.
2. Hypothese zum Format aufstellen, mit ImHex-Pattern prüfen.
3. Spezifikation in `docs/formats/<name>.md` schreiben (Struktur, Offsets, Endianness, Unklares als **UNBEKANNT** markieren).
4. Parser in `engine/formats/` implementieren, Test schreiben (synthetische Mini-Dateien, nie Originalmaterial im Repo).
5. Validieren: Parser über **alle** Dateien der Installation laufen lassen (`tools/validate_all.py`), Abweichungen dokumentieren.
6. Viewer/Konverter bauen, um das Ergebnis sichtbar zu machen (Bilder statt Annahmen).

**Reihenfolge der Formate (Annahme, zuerst verifizieren):**
ZFS-Container ✔ → ODF/TT-Datendateien (Klartext, Parser fehlt) → Texturen (TGA) → Modelle (SOD, Storm3D) → Karten (BZN) → KI (`.dsl` sind native DLLs!) → UI-Layouts → Sprachdateien (LDL) → Sound/Videos.

**Wichtig:** Alle Annahmen über die Engine (z. B. Verwandtschaft zu Storm3D/Battlezone) sind **Hypothesen**, bis sie an echten Dateien bestätigt sind. Nichts als Fakt in `docs/` schreiben, was nicht geprüft wurde. Status je Abschnitt: `bestätigt` / `vermutet` / `unbekannt`.

**Simulation:**
- Fester Tick (z. B. 20–30 Hz), Render entkoppelt, Interpolation.
- Deterministisch bauen (Fixed-Point oder kontrollierte Floats, seedbarer RNG), damit Multiplayer/Replays später möglich bleiben.
- Spielwerte nie hartkodieren, sondern aus den Originaldaten des Spielers laden.

**Touch-Steuerung (früh mitdenken):**
- Tippen = auswählen, Doppeltippen = alle gleichen Einheiten, Ziehen = Auswahlrahmen, Lang-Tippen = Befehlsmenü.
- Zwei-Finger-Geste für Kamera (Pan/Zoom/Rotation), Befehlsleiste großzügig dimensioniert (mind. 48 dp Touch-Ziele).
- Alles auch per Maus/Tastatur auf dem Desktop bedienbar.

## 6. Code-Konventionen

- C++17, `clang-format` (Google-Style, 100 Spalten), `clang-tidy` im CI.
- Keine Exceptions in der Engine, Fehler per `Result<T>`/Statuscodes. Parser müssen mit kaputten/manipulierten Dateien sicher umgehen (Bounds-Checks, keine ungeprüften Offsets; Fuzz-Tests für jeden Parser).
- Keine Plattform-`#ifdef`s im Engine-Core. Plattformcode nur in `android/` und `desktop/` hinter Interfaces (`IFileSystem`, `IWindow`, `IAudio`).
- Kommentare erklären das **Warum** und dokumentieren Format-Erkenntnisse mit Verweis auf `docs/formats/…`.
- Kleine, thematische Commits. Commit-Message auf Deutsch, Präfix: `format:`, `render:`, `sim:`, `ui:`, `android:`, `tools:`, `docs:`.

## 7. Tests und Qualität

- Unit-Tests mit Catch2 oder GoogleTest, laufen per `ctest` auf dem Desktop.
- Golden-Image-Tests für Rendering (nur aus selbst erzeugten Testdaten).
- Optionaler lokaler Integrationstest, der `originalFiles/` nutzt, wird übersprungen, wenn das Verzeichnis fehlt (CI läuft ohne Originaldateien).
- Android: Test auf mindestens einem echten Gerät und einem Emulator. Ziel: arm64-v8a, minSdk 26, Performance-Budget 30 fps auf Mittelklasse-Hardware.

## 8. Meilensteine

| Nr. | Ergebnis |
|-----|----------|
| M0 | Repo-Gerüst, CMake + Gradle bauen "Hello Triangle" auf Desktop und Android |
| M1 | Dateiinventar und Container-Parser, Spielverzeichnis-Auswahl in der App |
| M2 | Textur- und Modell-Viewer (Desktop + Android) |
| M3 | Karte rendern, Kamera, Schiffe platzieren |
| M4 | Auswahl, Bewegung, Kampf, Ressourcen, einfache KI (spielbare Skirmish) |
| M5 | UI/HUD komplett, Sound, Musik |
| M6 | Kampagne, Videos, Politur |
| M7 | Multiplayer (nur nach Machbarkeitsprüfung) |

## 9. Verhalten von Claude in diesem Projekt

- Vor jeder größeren Aufgabe kurz den Plan nennen, dann direkt loslegen. Rückfragen nur bei echten Weggabelungen (z. B. Lizenz, Engine-Architektur).
- Immer zuerst an echten Dateien prüfen statt zu raten. Wenn `originalFiles/` fehlt, das klar sagen und stattdessen Tools/Parser vorbereiten, die der Nutzer lokal laufen lässt.
- Ergebnisse von RE-Läufen (Hexdumps, Strings, Statistiken) in `docs/` zusammenfassen, nicht ungefiltert ins Repo kippen.
- Nie Originalmaterial in Commits, Issues oder Beispieldaten übernehmen.
- Ehrlich über Unsicherheit und Fortschritt berichten: lieber "Format X zu 60 % verstanden, Felder A/B unbekannt" als beschönigen.
- Bei neuen Abhängigkeiten Lizenz prüfen (GPL-kompatibel, keine Closed-Source-SDKs im Core).
- **Sprache:** Alles, was auf GitHub sichtbar wird, ist auf **Englisch**: Commit-Messages, Pull Requests, Issues, README, Release-Notes und Code-Kommentare in neuem Code. Die Unterhaltung mit Jack bleibt auf Deutsch. `CLAUDE.md` und interne Notizen in `docs/` duerfen vorerst deutsch bleiben; neue oeffentliche Dokumentation (README, Beschreibungen) auf Englisch.

## 10. Offene Entscheidungen (zu Projektbeginn klären)

- [ ] Endgültiger Projektname und Paket-ID (ohne Markenbezug)
- [ ] Code-Lizenz (GPLv3 vs. MIT)
- [ ] SDL2-Activity vs. `GameActivity` als Android-Basis
- [x] Edition: GOG (Produkt-ID 1582325353), Dateistand 13.01.2022, entpackt in `originalFiles/`
- [x] Lokale Installation vorhanden: `originalFiles/` im Projektordner

## 10a. Grundsatz: keine Konvertierung zur Laufzeit

Die App liest die **Originaldateien des Spielers direkt** (ZFS, SOD, TGA, ODF …). Es werden keine konvertierten Assets ausgeliefert oder im Repo abgelegt.
Die Python-Werkzeuge in `tools/` sind Referenzimplementierung und Prüfhilfe (Spezifikation → Parser → Validierung über alle Dateien); daraus entstehen die C++-Loader.
OBJ-Export und Vorschaubilder dienen nur der Fehlersuche und landen in `work/` (nicht im Repo). `reference/` enthält fremde Community-Projekte nur zum Gegenprüfen, nie zum Kopieren.

## 11. Stand der Erkenntnisse (zuletzt aktualisiert: 08.11.2026)

Details in `docs/formats/`. Kurzfassung:

- **ZFS-Archive: gelöst.** `tools/zfs_tool.py list|extract` liest `assets.zfs` (516 Dateien) und `objtxt.zfs` (81). Inhalt ist mit einem 4-Byte-XOR-Schlüssel aus dem Header verschleiert.
- **ODF/TT:** Spielwerte liegen als Klartext vor (nach dem Entpacken). Parser und Grammatik fehlen noch.
- **Engine-Herkunft:** nicht Quake III. Magic `Storm3D_SW`, `.bzn`, `.odf` deuten auf die Battlezone-/Storm3D-Linie.
- **KI:** `.dsl` sind kompilierte x86-DLLs. Verhalten muss später über Ghidra rekonstruiert werden.
- **Exe:** PE32 mit Direct3D 7, Debugpfad und RTTI-Namen → gut für Ghidra.
- **SOD-Modelle: gelöst bis auf eine Datei.** `tools/sod_format.py` liest 272 von 273 Dateien (v1.2–1.8) exakt bis zum letzten Byte: Materialien, Knotenbaum (Elternbezug über Namen, 7678 von 7678 gültig), Meshes (1795, 125 679 Dreiecke), Animationsdaten. `tools/sod_tool.py` hat `info`, `obj`, `preview`, `check`. Offen: `Wviracod.SOD` (v1.0), Bedeutung der Legacy-Liste, Animationslogik. Details: `docs/formats/sod.md`.
- **Texturen:** TGA (RGB 24/32 Bit, Index8 palettiert), `.1`–`.4` sind Mip-Stufen. Mesh-Textur = Dateiname ohne Endung, 1640 von 1795 Meshes finden ihre Datei. Details: `docs/formats/texturen.md`.
- **Nächste Schritte:** (1) ODF-Parser mit Tests, (2) C++-Loader für ZFS, SOD und TGA (Engine liest die Originaldateien direkt, siehe Grundsatz unten), (3) BZN-Binärteil, (4) LDL-Verschleierung, (5) erster Renderer auf Desktop und Android (M2).


## 12 Stand M0 (Repo-Gerüst)

- Kern: C++17, plattformunabhängig (`engine/`), Log-Sink-Abstraktion, eigene Mathe, minimales Test-Framework (`tests/`).
- Desktop: SDL2 + OpenGL ES 3.0 (Mesa libGLESv2). Android: NativeActivity + EGL ohne Java (`android:hasCode="false"`), minSdk 26, ABIs arm64-v8a und x86_64.
- Platzhalter-Name `fleetclient`/`OpenFleet`, Paket `dev.openfleet.client`, keine Markennamen.
- Geprüft: Kern + Tests gebaut. Nur Syntaxcheck: Renderer, Game, Desktop-main. Ungeprüft: GL-Lauf, Android-Build.
- Abhängigkeiten Desktop: `libsdl2-dev libgles-dev cmake ninja-build`. Siehe `README.md`.
- Nächster Schritt: M1, C++-Loader für ZFS/SOD/TGA, gegen die Python-Werkzeuge testen.

## 13 Stand M1 (Modellbetrachter, Ordnerauswahl)

- Reale Lage der Daten: Modelle (`SOD/`) und Texturen (`Textures/RGB`, `Textures/Index8`) liegen als lose Dateien vor; `assets.zfs`/`objtxt.zfs` enthalten nur ODF/TT/Texte.
  ZFS-Loader ist fertig und geprueft, wird fuer M1 aber noch nicht gebraucht (kommt mit dem ODF-Parser).
- C++-Module im Kern: `io/` (ByteReader, FileSource, DirSource, Gross-/Kleinschreibung egal), `formats/` (Tga, Zfs, Sod, TextureLookup), `model/ModelBuilder` (Weltmatrizen, Normalen, Windungserkennung ueber das Vorzeichen des Volumens, z negiert wegen links-/rechtshaendig).
- Render: `render/ModelRenderer` (ES 3.0, Texturen mit Mipmaps, Alpha-Test, additive Leuchtflaechen), `game/Game` als Modellbetrachter (Orbit-Kamera, Touch/Maus).
- Android: `MainActivity.java` (NativeActivity-Unterklasse) erledigt nur die Ordnerauswahl per Storage Access Framework (dauerhafte Berechtigung) und liefert Dateien als Deskriptoren; `SafSource.cpp` bindet das als `FileSource` an. Keine Kopie der Spieldaten.
- Pruefweg ohne Geraet: `fleet_data_dump` (C++) gegen `tools/data_dump_ref.py` (Python) ueber alle echten Dateien, 0 Abweichungen.
- Testdatensatz: `tools/make_testset.py` (12 Modelle, ca. 3 MB).
- Annahmen, noch am Bild zu pruefen: Koordinatenumrechnung (z negiert), Texturkoordinate v ohne Spiegelung, Windung/Cull, Teamfarben (aktuell nur Textur), Animationen/Sprites/Emitter werden nicht gezeichnet.
- Offen: `Wviracod.SOD` (v1.0), Hardpoint-Visualisierung, ODF-Parser, BZN-Karten, Sprachdateien.
- Erste Geraetetests: Tiefenfehler/Flimmern kamen von gleichzeitig gezeichneten Detailstufen (`lodN`), Borg-/Schadenszustaenden und `oq`-Huellen. Der Client blendet diese aus (`BuildOptions`, Details in docs/formats/sod.md). Windung wurde gegen die Daten geprueft (Mehrheit der Meshes: GL-CCW aussen, keine Umkehr noetig).

## 14 Spaeter geplant (Vermerke)

- **Shader ueberarbeiten:** Die aktuellen Shader (`render/ModelRenderer.cpp`) sind bewusst einfach (eine Richtungslichtquelle, Textur x Materialfarbe,
  Alpha-Experimente). Spaeter sollen sie besser aussehen als das Original: z. B. bessere Beleuchtung/Specular aus den Materialwerten, Emission fuer
  `glow_*`, Bloom, weichere Teamfarb-Einfaerbung, Borg-Ueberzug mit sauberer Transparenz, Normalen/Glaettung. Erst nach der Klaerung der Materialsemantik
  (Alpha der Basistexturen, 20 Felder am Aequatorring der Fedbase offen).
- Die experimentellen Umschalter (Alpha-Modus, Teamfarbe) fliegen raus oder werden Debug-Optionen, sobald die Bedeutung geklaert ist.

## 15 Plattformen und Schichten (Entscheidung)

Name bleibt neutral (OpenFleet, `dev.openfleet.client`). Ziel ist ein Spielkern, der von jeder Plattform nur ueber schmale Schnittstellen abhaengt.

**Schichten**
1. **Kern** (`engine/src`, C++17, Standardbibliothek + OpenGL ES 3 ueber `render/Gl.h`): Formate, Modell, Spiellogik, Renderer. Kein Plattformcode,
   per Test erzwungen (`tools/check_layering.py`, ctest `layering`).
2. **Schnittstellen zur Plattform** (heute): `FileSource` (Spieldaten lesen, Schreibweise egal), `GameHost` (Meldungen, Ordnerwahl), Eingabe als `PointerEvent`/Tasten,
   `LogSink`, GL-Kontext + Frame-Takt gehoeren der Plattform. Spaeter dazu: Audio, Netzwerk, Zeit.
3. **Plattformschichten:** `desktop/` (SDL2, Linux als gleichrangiger Client; Windows/macOS ueber SDL2 moeglich, ungetestet), `android/` (NativeActivity + EGL + SAF).
   Geplant/offen: `web/` (Emscripten/WebAssembly mit WebGL2).

**Linux** ist ein vollwertiger Client (`fleetclient_desktop`), nicht nur Entwicklungshilfe.

**Web-App (Vorbehalt, noch nicht gebaut):** WebGL2 entspricht ES 3.0, der Renderer sollte unveraendert laufen. Offene Punkte: Spieldaten ueber Ordnerwahl im Browser
(`<input webkitdirectory>` oder File System Access API) als eigene `FileSource`, die Daten bleiben beim Nutzer; kein blockierender Dateizugriff im Kern (Dateien
muessen synchron aus dem Speicher lesbar sein, also vorher geladen werden); keine Threads voraussetzen; Rechenzeit pro Frame begrenzen.

**Regeln fuer neuen Kerncode:** nichts aus `<filesystem>`/Sockets/Threads ausserhalb von `io/DirSource.cpp` annehmen (DirSource ist die einzige Ausnahme und kann bei Bedarf
in die Plattformschicht wandern); Zeit, Datei, Eingabe, Audio und Netz nur ueber Schnittstellen; keine globalen Zustaende, die ein Neustart des GL-Kontexts zerstoert.

## 16 Stand M2 (ODF/TT-Parser)

- `engine/src/formats/Odf.*` (Parser, `OdfLibrary` mit #include-Vererbung und Schleifenerkennung, `Cp1252ToUtf8`), `Tt.*`, `ZfsSource.h` (FileSource über ZFS), `io/OverlaySource.h`.
- Geprüft: 126 Unit-Checks; `fleet_data_dump` vs. `tools/data_dump_ref.py` (unabhängige Python-Auflösung) bei 489 ODF + 27 TT ohne Abweichung (Eintragszahl, Wertezahl, Stringzahl, Zahlensumme).
- Annahme: bei doppelten Schlüsseln gilt der letzte. Schlüsselbedeutung noch offen. Format: `docs/formats/odf-tt.md`.

## 17 Positionslichter (Sprite-Knoten)

- Blinklichter sind SOD-Sprite-Knoten (`redstrobe_N` usw.), Definition in `Sprites/lights.spr`, Animation in `Sprites/animation.spr`; nicht in den ODFs. Format: `docs/formats/spr.md`.
- Code: `formats/Spr.*` (SprLibrary), `Model::sprite_nodes` (ModelBuilder), `render/SpriteRenderer.*` (additive Billboards), Anbindung in `game/Game.cpp`.
- Geprüft: 138 Unit-Checks; Dump-Abgleich C++/Python (Zählwerte, Animationspegel, 2208 von 2267 Knoten aufgelöst) ohne Abweichung. Das Rendering selbst ist hier nur auf Kompilierbarkeit geprüft, nicht visuell.
- Offen: Borg-`assim*`-Knoten, `@auto`-Sprites, Blinkphase, Größenmaßstab prüfen.
- Nachtrag: nicht-quadratische Sprite-Knoten ohne `billboard` (whiteconstbig/Spot 7x18) liegen flach in der right/up-Ebene des Knotens (Lichtfleck auf der Untertasse); Lichter innerhalb der Huellbox ausgeblendeter oq-Netze entfallen mit (Fbase 21 statt 25).

## 18 Spielwerte aus ODF (UnitDef)

- `engine/src/data/UnitDef.*`: LoadUnitDef/LoadWeaponDef/LoadPhysicsDef, DescribeUnit. Anzeige im Betrachter: Tippen in die Bildmitte (Desktop: Taste I). ODF = SOD-Name.
- Geprüft: 148 Unit-Checks; Dump-Abgleich (UNIT-Zeilen) bei 169 Einheiten ohne Abweichung; Hardpoints der Waffen existieren im SOD (Ausnahmen zcarddes, zferengi). Details: `docs/formats/odf-tt.md`.
- Naechste Kandidaten: Geschoss-ODFs, Hardpoints im Viewer markieren, BZN-Karten, LDL-Sprachdateien.

## 19 Textebene (Overlay)

- Meldungen und Spielwerte werden als Text ueber dem Bild eingeblendet (Linux und Android gleich, kein Toast/Titelzeile mehr): `render/TextRenderer.*`, Layout/Umbruch in `core/TextLayout.*` (getestet), Schrift als eingebetteter Atlas `core/FontData.h` (DejaVu Sans Mono, frei einbettbar, Lizenz siehe `docs/NOTICE-font.md`), erzeugt mit `tools/gen_font.py`.
- Sichtbar 4 bis 12 s je nach Textlaenge; Steuerungshinweis beim ersten Modell. Skalierung ganzzahlig nach Bildhoehe (`hoehe/500`).
- Nur auf Kompilierbarkeit geprueft, nicht visuell (kein GL in der Entwicklungsumgebung). Spaeter evtl. Originalschrift (`BZONE.FNT`) nutzen.

- Nachtrag Geschosse: `OrdnanceDef`/`LoadOrdnanceDef`, `PerTarget` (Wert mit Ausnahmen je Zielart) in `data/UnitDef`; DescribeUnit zeigt Schaden. Abgleich 118 Waffen/83 Geschosse ohne Abweichung.

## 20 Hardpoints im Betrachter

- `Model::hardpoints` (Null-Knoten `hpNN`, Position + Knoten-Achse `front` als Richtung, z negiert); `render/MarkerRenderer.*` zeichnet Punkte und Richtungslinien ohne Tiefentest. Umschalten: Ecke oben rechts (Desktop: H). Farben: je Waffenplatz der ODF eine Farbe, weiss = Ziele der Teilsysteme, grau = sonstige; Legende als Text.
- Abgleich Anzahl Hardpoints C++/Python ohne Abweichung. Ob `front` wirklich die Feuerrichtung ist, ist noch nicht visuell bestätigt (Daten: hp01 am Bug zeigt nach vorn, Seiten-Hardpoints nach aussen).
- Naechster Schritt: Feuertaste mit Strahlen/Pulsen/Torpedos (Sprites aus weapon.spr, @anim/@auto-Parser fehlt noch).

## 21 Waffenfeuer (Sichtprobe)

- Unten Mitte tippen (Desktop: F): alle normalen Waffen feuern von den Hardpoints. Code: `game/FireDemo.*` (GL-frei, 176 Unit-Checks), `render/EffectRenderer.*`; `.spr`-Parser kennt jetzt `@anim=` und Bildfolgen (`FrameCell`). Details und Annahmen: `docs/formats/spr.md`.
- Nur auf Kompilierbarkeit/Logik geprüft, nicht visuell. Testset enthält jetzt die Texturen aus `weapon.spr` (6,6 MB).
- Offen: Sonderwaffen, Schiffsbewegung, Ton (`fireSound`), Treffer/Ziele, Waffenmodelle (`shotGeometry`).

## 22 BZN-Karten

- `formats/Bzn.*` liest Kopf (Hintergrund-SOD, Tech-Tree, Rasse), Objekte (ODF, Handle, Position, Team, Label, Drehung) und Pfade aller 49 Karten; Abgleich C++/Python `tools/bzn_tool.py` ohne Abweichung, 189 Unit-Checks. Format und Annahmen: `docs/formats/bzn.md`.
- Objektanfänge werden über eine feste Elementfolge erkannt, die klassenspezifischen Zusatzdaten werden übersprungen (nicht entschlüsselt).
- Kartenansicht im Viewer: oben Mitte tippen (Desktop: M). `game/MapScene.*` (GL-frei, getestet) berechnet die Platzierung: Matrix Modell(GL) -> Welt(GL) aus Drehung (Zeilen rechts/oben/vorne) und Position, z gespiegelt. `Game.cpp`: ein `ModelRenderer` je SOD-Name, Hintergrund `mbg*.sod` um die Kamera (auf Radius 60000 skaliert, danach Tiefe loeschen), Objekte ohne SOD (`mafield`, `dlight`) als Punkte, Teamfarbe je Team. Bedienung: ziehen drehen, 2 Finger zoomen/verschieben, Rand links/rechts andere Karte, Mitte Info.
- Nur auf Kompilierbarkeit und die Platzierungslogik geprueft, nicht visuell. Ob die Drehung (Zeilen = rechts/oben/vorne) und die Skalierung des Hintergrunds stimmen, ist offen. Testset hat jetzt 4 Karten (2duel, 4nirvana, 3race, 4first), 35 Modelle, 13,7 MB; `--maps` waehlt andere.
- Naechster Schritt: Schiffsbewegung (physics-ODF), Lichter/Sprites in der Karte, Asteroidenfelder.
- Korrekturen nach der ersten Sichtprobe: (1) Himmelswuerfel (`mbg*.sod`, Material BACKGROUND) wird nie rueckseitig ausgeblendet (die Windung wird pro Modell aus dem Volumen geschaetzt, bei offenen Wuerfelseiten falsch). (2) Nebel (`mnebula*.sod`) haben keine Netze, nur Sprite-Knoten (`nebula.spr`): in der Karte pro Modell ein `SpriteRenderer`, so erscheinen auch Positionslichter der Schiffe. (3) Das SOD eines Objekts steht in der ODF (`baseName`, z. B. zferscav -> zferengi, mwrmhole -> Mwormh, wingmen -> Schiffsklasse); ODF-Name = SOD-Name gilt nur fuer die meisten Einheiten. (4) Asteroidenfelder (`mafield`, `dont_load_SOD`, `minAsteroids`/`maxAsteroids`, `fieldRadius`, `scaleSOD`) werden aus Einzelfelsen zusammengesetzt (deterministisch aus dem Handle). `sscr_1.sod` gibt es nicht, Annahme: Felsen sind `Master1..8.sod`. Schwarzes Loch skaliert mit `ScaleSize`, Wurmloch/Loch sitzen `distanceBelowGrid` unter dem Raster.
- Testset: 4 Karten, 46 Modelle, 15,3 MB (Texturen aus lights/weapon/nebula.spr).

## 23 Ton, Bedienleiste, APK-Bau

- Ton: `formats/Wav.*`, `audio/Mixer.*`, Backends SDL/AAudio (siehe `docs/formats/wav-ton.md`). Musik der Karte (Volk der Karte), Dauergeraeusche der Objekte (`ambientSound`), Feuergeraeusche im Feuer-Demo (`FireDemo::TakeSounds`). Taste S / Knopf "Ton" schaltet stumm. Nicht gehoert, nur Logik und Mischer getestet (223 Checks).
- Bedienleiste statt unsichtbarer Tippzonen: `game/Ui.*` (GL-frei, getestet in 4 Bildformaten), Hochformat 4 Spalten, Querformat 6. Das 3D-Bild liegt ueber der Leiste (`glViewport`), Meldungen wachsen mit der Breite (mind. 40 Zeichen je Zeile). `TextRenderer::FillRect/DrawStyled` fuer Flaechen und Schrift ohne Panel.
- APK in der Geraete-VM gebaut: JDK 17 (Temurin) und Android-SDK/NDK 26.1 unter `$HOME/tc/` (Download: Google, GitHub). Prozesse im Hintergrund werden nach jedem Befehl beendet; Gradle in Schritten (`timeout 170 ./gradlew --no-daemon assembleDebug`) aufrufen. Projekt wird nach `$HOME/tc/proj` kopiert (rsync engine android), damit `local.properties`/Build-Ordner im Repo unberuehrt bleiben. Debug-Schluessel stammt aus der VM, nicht von Jacks Rechner: bei Signaturkonflikt erst deinstallieren.
- Testset: `make_testset.py` kopiert jetzt auch Klaenge (alle in ODFs genannten Effekte) und je Volk Musik "115" (47 MB gesamt).


## 24. Original-HUD (Stand: Android-APK openfleet-debug-3)
- Layout aus `misc/gui_<rasse>.cfg` (#include gui_glob.cfg; Bezug 640x480), Teilstuecke aus `Sprites/gui_*.spr` (Sprite `<bg>.<i>` = Textur+Ausschnitt), Texturen `Fguia..f04` (Foederation; K/R/B analog), Tastensymbole `GB*.tga`.
- Code: `formats/GuiCfg`, `game/Hud` (GL-frei: HudData, BuildHudLayout, HudHit), `render/HudRenderer` (Texturquads). Alte Leiste (`game/Ui`) bleibt als Rueckfall, wenn die HUD-Dateien fehlen.
- Anordnung: Querformat Minikarte links / Steuerfeld rechts / Info+Leiste+Kinosicht mittig, Hochformat gestapelt; Skalierung ganzzahlig in Viertelschritten, 3D-Bild ueber dem HUD.
- Tastenbelegung = Testfunktionen (Leiste: Zurueck/Weiter/Variante/HP/Feuer/Ton; Steuerfeld: Info/Karte/Team/Alpha/Nah/Fern/Mitte/Ordner/Rasse). Rasse-Taste schaltet das HUD der vier Voelker.
- Nicht uebernommen: Originalschrift (font.spr), Tooltips, Wireframe-Icon im Infofeld, Kinosicht zeigt nur das Standardbild.
- Sichtpruefung ohne GPU: `fleet_hud_dump` + `tools/hud_render.py`. 708 Pruefungen, 0 Fehler. Auf dem Geraet noch ungetestet.
- Originalschrift im HUD (Sprites/font.spr + Textures/font.tga, 128er Atlas, Zellhoehe 10): Glyph i = ASCII i+32, Position aus `font_uv`; die Breitentabelle `font_w` hat einen Eintrag weniger und ist verschoben, deshalb Breite = Abstand zum naechsten Glyph der Zeile - 1, Vorschub +1. Einblendungen (Meldungen) nutzen noch den eingebauten Font. 713 Pruefungen.
- Umlaute: font.spr deckt Latin-1 ab (Glyph = Code-32; 128..165 zusaetzlich CP437-Varianten). UTF-8 wird zu Unicode dekodiert, >255 -> '?'. Einblendungen (Game::Message) nutzen bei vorhandener Schrift `LayoutHudText` (Umbruch, Panel) statt des eingebauten Fonts; der bleibt nur als Rueckfall ohne HUD-Dateien. 716 Pruefungen.
- 3D-Bild fuellt den ganzen Bildschirm (auch hinter dem HUD); Bildmitte per Projektionsverschiebung bar/height in den freien Bereich.

## 25. Menue, Befehle, Spawn (Stand: openfleet-debug-9)
- Modellviewer entfernt: dauerhaft Kartenansicht, Start auf virtueller "Leere Karte" (Punktraster). Objekte setzen (`Spawned`, eigene Modell-Cache `spawn_models`), Flugmodell `game/ShipSim.*` (Werte aus Physik-ODFs, Verrechnung ist Annahme).
- Menue sitzt im Original im Kinofeld (Knopf "Menu", Strg+X; Handbuch S. "Menu/Comm Buttons"): Optionsmenue (Ziele, Speichern/Laden, Einstellungen = noch nicht), Untermenue DEBUG (Objekt setzen, Karte waehlen, Info, Team, Alpha, Ton, Zoom, HUD-Rasse, Ordner, Loeschen). Oben bei den Ressourcen gibt es in Armada 1 laut gui_*.cfg kein Menue.
- Steuerfeld = Befehle des ausgewaehlten Objekts mit Original-Sprites (`b_move`, `b_attack`, `b_stop`, `b_order`, Untermenue: `b_galert/yalert/ralert`, `b_standdown`, `b_repair`, `b_guard`, `b_patrol`, `b_scout`, `b_sandd`, `b_decom`, `b_back`). Plaetze (Zeile fuer Zeile) sind eine Annahme; Handbuch nennt nur Bewegen, Angriff, Stopp, Bauen, Befehle (Mitte). Leiste = Spezialwaffen (`b_<waffen-odf>`, WeaponDef.special), noch ohne Wirkung. Angriff/Reparatur/Wache/Patrouille/Erkunden/Suchen noch ohne Funktion.

## 26. Datenquellen: Ordner und ZIP-Dateien (Stand: openfleet-debug-10)
- `io/ZipSource` (GL-/plattformfrei): liest ZIP ueber eine Leseroutine (Android: pread auf Dateideskriptor), Verzeichnis beim Oeffnen, Dateien beim Zugriff entpackt; `io/Inflate` ist ein eigener Deflate-Entpacker (keine zlib). Gespeichert + Deflate, ZIP64 gelesen, Verschluesselung nicht. Gemeinsamer Oberordner (z. B. "Armada/") wird weggelassen, ausser er heisst wie ein Spielordner (SOD, Textures, ...).
- Rangfolge: Android legt die ZIPs in der gewaehlten Reihenfolge per `OverlaySource` uebereinander (oben gewinnt), der Ordner (falls gewaehlt) liegt darunter. Dialog `showSourceDialog` in MainActivity (Mehrfachauswahl, hoch/runter/entfernen, Ordner entfernen), Auswahl und Reihenfolge werden gespeichert (SharedPreferences `zip_uris`). Aufruf: DEBUG-Menue "Datenquellen" bzw. beim ersten Start.
- Geprueft: 535 Dateien (65 MB) des Testsets bitgenau gegen CRC; Tests mit Mini-ZIPs inkl. Rangfolge. Nicht geprueft: Verhalten der Android-Dokumentenauswahl auf Jacks Geraet (pread auf SAF-Deskriptoren, Rueckfall: ganze Datei in den Speicher).

## 27. Menue im Stil des Original-Optionsmenues (Stand: openfleet-debug-11)
- Titelleiste oben (grau-gruen, weisse Schrift), mittig Rahmen (grau-gruen/schwarz) mit grossen sandfarbenen Tasten, gedrueckte Taste gelb, ausgegraute = noch nicht umgesetzt. Tastenhoehe 0,13 x kleinere Bildschirmseite (ca. 8 mm), Schrift schrumpft je Taste auf die Breite; mehr Zeilen als Platz = Ziehen zum Scrollen. HUD und Meldungen sind ausgeblendet, solange ein Menue offen ist (3D-Szene bleibt).
- Farben/Form sind nach dem Screenshot des Originals geschaetzt, nicht aus den Original-Grafiken gelesen (die Menue-Bitmaps wurden nicht gesucht). Optionsmenue-Reihenfolge wie im Original (Ziele, Speichern, Laden, Grafik, Ton, Spiel, Abbrechen, Zurueck) plus DEBUG.

## 28. Hochkant-Layout D und Auswahl (Stand: openfleet-debug-12)
- Hochkant (Variante D, per Claude-Design gewaehlt): Kino oben links, Ressourcen rechts daneben, Leiste direkt unter den Ressourcen, Minikarte unten links, Info unten rechts, Steuerfeld am rechten Rand direkt ueber Info. `HudLayout.view` = freier 3D-Bereich zwischen den Feldern; die 3D-Mitte wird dorthin verschoben (`CentreShift`, auch fuer `ScreenRay`). Nur im Vorschau-Render geprueft, nicht auf dem Geraet.
- Auswahl (nur gesetzte Objekte = `Spawned`, Schiffe und Stationen): Tippen = nur dieses Objekt; Gedrueckthalten (>0,35 s, ohne Bewegung, beim Loslassen) = Objekt hinzufuegen/entfernen; Gedrueckthalten auf leerem Grund = alles abwaehlen. Tippen auf Boden = Ziel fuer alle ausgewaehlten beweglichen Schiffe (gestaffelt quer zur Blickrichtung). Stopp/Entfernen/Alarmstufe wirken auf alle ausgewaehlten. `selected` = Hauptobjekt (Info, Befehlsleiste).
- Offen: BZN-Kartenobjekte nicht auswaehlbar, Original-Auswahlrahmen (`selection.xx`) nicht gezeichnet (Marker-Punkte), kein Aufziehrahmen.
- Update debug-13: 3D-Szene fuellt wieder den ganzen Bildschirm hinter dem HUD (CentreShift per kCentreInView abschaltbar, Standard aus).
- debug-14: 3D vollflaechig, Drehpunkt per CentreShift hoechstens 0,12 (Bildraum) nach oben Richtung Mitte des freien Bereichs (kMaxCentreShift).
- debug-15: Kartenobjekte (BZN, Team>0) auswaehlbar: Treffer wird als Spawned uebernommen (AdoptInstance), Instanz ueber scene_hidden ausgeblendet; Modellradius je Szenenmodell (map_radius). Nur kompiliert, nicht auf dem Geraet geprueft; Kurs aus -Spalte z der Transform per ShipFromForward (Annahme).
- debug-16: BZN-Objekte mit Team werden beim Kartenladen sofort als Spawned uebernommen. Nur Schiffe bewegen sich (Sperrliste von Klassenbezeichnungen in InitSpawnedFromOdf, Annahme). Bewegen nur nach Druck auf die Bewegen-Taste (move_armed). Schiffsfeld wie im Original (HudShip, DrawShips in Hud.cpp, Layout aus infoSingle*/infoMulti* der gui_*.cfg): Rassenleiste, Klasse, Name, Besatzung/Offiziere (Annahme: crew_cost/officer_cost), Schild-/Energiebalken, 5 Systemsymbole; mehrere Auswahl = bis zu 8 kleine Felder. Fehlt: Drahtgittermodell, echte Schadenswerte (alles voll), Rassensymbol, Klassenname aus Sprachdatei.
- debug-17: Orientierungsgitter (500 m Zellen, 41x41 Linien, jede 5. heller) in der waagerechten Ebene des Hauptobjekts, vor den Modellen ohne Tiefentest. Info-Drahtgitter sind im Original vorgerenderte 40x40-Sprites (gui_global.spr, Name <odf>w1..w5, Textur BW*.tga, Teile des Schiffs): noch nicht eingebaut.
- debug-18: Optionen > Erweitert (Optionen, die es im Original nicht gab): Bewegen per Antippen (tap_moves, Standard aus, nur Sitzung); Bessere Shader, Lagesensor-Ansicht, alternative GUI-Anordnung als ausgegraute Platzhalter. Einstellungen werden noch nicht gespeichert.
- debug-19: (1) Android: beim Wechsel in den Hintergrund bleiben EGL-Kontext und Game erhalten, nur die Flaeche wird neu erstellt (android_main.cpp: OnWindowDestroyed/FullShutdown). (2) Fehler behoben: sel_markers und grid_markers wurden nie initialisiert (Init fehlte) -> Gitter und Auswahlmarker waren unsichtbar. (3) Drahtgitter im Schiffsfeld: Sprites <odf>w1..w5 (gui_global.spr) uebereinander, noch ohne Schadensfaerbung (Zuordnung Teil->System unbekannt).
- debug-20: Gitterzelle 75 (Fbase-Groesse, 81x81 Linien); Ansicht dreht um den Bodenpunkt unter dem Finger (Touch.pivot, Zielpunkt wird nachgefuehrt); Auswahl = Original-Sprites selectionCircle (Boden-Ellipse, Stauchung sin(pitch)) + selection.00/20/02/22 (Eckklammern, Annahme Eckzuordnung) in Bildschirmraum, Ziel = gridFXSprite; kein Test auf dem Geraet.
- debug-21: Sternenhimmel MbgStars (Sphaere ysp1gn1b + Kasten Mcelbods) immer als hinterste Skybox vor dem Kartenhintergrund, auch auf der leeren Karte (Punktgitter dort entfernt). Gitterebene fest bei y=0. Auswahlring und Zielring liegen auf der Gitterebene (Fusspunkt), Eckklammern entfernt (laut Screenshot im Original nicht vorhanden). Ob der Kartenhintergrund die Sterne ergaenzt (Blending) oder verdeckt, ist ungeprueft.
- debug-22: Touch in der Karte: 1 Finger verschiebt (Bodenpunkt folgt dem Finger, PanMap), 2 Finger: Spreizen = Zoom (bis 30x), Verdrehen = Gier, gemeinsames Ziehen = Gier/Neigung, jeweils um den Bodenpunkt unter den Fingern (OrbitAbout). Gitter weiss, 75er Zellen, ueber max(6000, 3*Extent). Sterne: Materialname BACKGROUND_1 war nicht von der Rueckseitenausblendung ausgenommen (Kugel von innen unsichtbar) -> Praefix 'background'; Kartenhintergrund jetzt additiv ueber den Sternen (ModelRenderer::SetForceAdditive). Drehrichtung beim Verdrehen ist eine Annahme.
- debug-23: Neue Touch-Bedienung (1 Finger verschieben, 2 Finger zoomen/verdrehen/ziehen) gilt auch in der Modellansicht ohne Karte (view_pan).
- debug-24 (Kampf): `game/Combat.*` (Schild/Huelle, Schaden, RNG), Angriffsbefehl (Angriff-Taste, dann Feind antippen), Waffen feuern aus Hardpoints (Strahlen sofort, Torpedos als lenkende Geschosse), Schild-/Huellenbalken, Infofeld mit echten Werten, Drahtgitter faerbt sich nach Huelle (<66% gelb/rot), Zerstoerung mit Explosion. Annahmen: Huelle = 2x maxHealth (ODF nennt keine), alle fremden Teams feindlich, Alarm Rot schiesst automatisch auf Feinde in Reichweite, Schussfelder ignoriert, shot_delay in Sekunden, Explosion = Sprite "bigorange". Nicht auf Geraet getestet.
- debug-25: Minikarte zeigt das Kamera-Sichtfeld (weisser Umriss der Bildecken auf der Gitterebene, an den Kartenrand gekappt; Minikarten-Ausschnitt richtet sich nur nach den Objekten). Nicht auf Geraet getestet.
- debug-26: Tippen auf die Minikarte setzt die Kamera (Zielpunkt) an diese Stelle; Zuordnung = Objekt-Ausdehnung wie bei den Punkten. Nicht auf Geraet getestet.
- debug-28 (Hauptmenue): `game/Shell.*` + `formats/Bmp.*`. Start im Hauptmenue (bitmaps/main/mainbk.BMP, Tasten bitmaps/ms/MS*{1,2}.bmp an den Positionen aus originalFiles/shell_cfg.h: x=460, y=267..432, Bezug 640x480, Tasten mittig in 180x25). 4:3-Flaeche wird ganz gezeigt, Rest = derselbe Hintergrund vergroessert und auf 30 % abgedunkelt. Zustand 1 = normal, 2 (gelb) = gedrueckt; nicht umgesetzte Tasten mit 45 % Deckkraft. Einzelspieler-Menue (bitmaps/single/singlebk.bmp): Kampagne/Skirmish/Training/Laden/Optionen/Zurueck; Platten bitmaps/sp/SPgame*.bmp mit eigener Schrift (die Originalplatten haben keinen Text; "Skirmish", "Kampagne", "Training" sind NICHT originale Beschriftungen). Skirmish startet vorerst direkt die leere Karte (Einstellungsbildschirm nach Vorbild bitmaps/multiset/mpsBG.bmp folgt). "Zum Hauptmenue" im Spiel-Optionsmenue (vorher "Mission abbrechen"). Beenden ueber GameHost::RequestQuit. Spielstand speichert `ingame`. Alpha-Modus aus dem Menue und Desktop-Tasten fuer Modellbetrachter entfernt (Code im Kern noch vorhanden, spaeter ganz entfernen). hud_dump: `fleet_hud_dump <ordner> <b> <h> 0 shell <main|single> [gedrueckt]` + tools/hud_render.py.
- debug-29 (Skirmish-Einstellung): Bildschirm auf bitmaps/multiset/mpsBG.bmp, Felder pixelgenau aus dem Bild vermessen (Spielerzeilen y=57+19i: Name x7 w142 / Rasse x151 w109 / Team x262 w55; Kartenliste rechts x526 w107, y=63+24k, 14 Felder; vorschau 385,138 128x128; Zeilen darunter x387 w124; unten Zurueck 9,450 / Start 126,449 / Optionen 527,450). Karten = bzn/*.mdf mit gleichnamiger .bzn und 128x128-.bmp-Vorschau; mdf "StartK = x z" in Feldern (1 Feld = 50 Einheiten, Mitte +25). Start: Karte laden, pro aktivem Platz Basis (`<f|k|r|b>base`), 2x const, scout, 2x cruise1 im Kreis (Radius 220) — Startpaket ist eine ANNAHME. Teamwert des Platzes = Spiel-Team (andere Teams feindlich). Spieler 1 (Mensch) bestimmt die HUD-Rasse. Computer-Plaetze haben noch keine KI; Ressourcen-Umschalter ohne Wirkung. "(leere Karte)" bleibt als Sandkasten. Skirmish-Beschriftung/Texte sind eigene.
- debug-30: Startpaket korrigiert nach Originaldaten: In 4hub/4brown/4crux/4more/4strange/8nirvana/2duel u. a. liegen je Spieler Vorlagen `<f|k|r|b>base` + `<f|k|r|b>const` (Team = Spieler); Bauschiff etwa (+176,-151) neben der Basis. Startpaket = Basis + 1 Bauschiff (keine Kreuzer). RTS_CFG.h: `cfgStartingDilithium = 3500`, `cfgStartingCrew = 4000`, Max Dilithium 999999, Max Offiziere 100, Max Crew 99999 (Ressourcen noch nicht umgesetzt). game_cfg.h enthaelt die ECHTEN Plaetze der Befehlstasten (Hauptseite: Move 1, Attack 2, Stop 3, Repair 4, Orders 5, StandDown 6, Harvest/Recrew 7, TransportSpecial 8, Upgrade 9, Build 10, Rally 11; Befehle: Green 1, Yellow 2, Red 3, Patrol 4, Scout 5, Search&Destroy 6, Guard 7, Decommission 10, SelfDestruct 11, Back 12) — Steuerfeld bei Gelegenheit daran angleichen.
- debug-31: Originalgetreues Schadensmodell (Combat.h/.cpp neu): Schild = maxHealth; Schaden > damageThreshold bei Schild kostet Besatzung ((Schaden-Schwelle)×shieldCrewModifier %); Durchschlag wuerfelt nach hitPercent System (je 10 Punkte) / Huelle (Besatzung −Schaden×hullCrewModifier %) / kritisch (explodiert); Systemausfall kostet Besatzung % (craft.odf), ohne Lebenserhaltung −2 %/s; Reparatur 1 Punkt je repairTime×Rassenfaktor (Besatzungsstatus gelb/rot, Rassen-ODF via LoadRaceDef); Waffenverzoegerung ×weaponYellow/Red; Antrieb defekt = kein Flug, Waffen defekt = kein Feuer; Besatzung 0 = Wrack (Team 0, trudelt, wird nicht beschossen), Beamen (Slot 8, Reichweite rangeScan, 1 s-Takt) uebergibt Besatzung/Schiff, Feind nur bei gesenktem Schild (Entern). Alarmstufen: Rot feuert auf alles Sichtbare (Sensor 500) und verfolgt bis 1,5×Sensor vom Ankerpunkt; Gelb nur bei Angriff auf sich/Nachbarn (400 m, 10 s); Gruen flieht, feuert nicht; Abwarten (alert 3) kein Auto-Ziel. Befehlsplaetze nach game_cfg.h. Wireframe-Teile weiss/gelb/rot je System. Explosion: Ton+Dauer aus fireball-ODF, Darstellung weiter Sprite "bigorange".
  ANNAHMEN (offen): 1 Systempunkt je durchgeschlagenem Treffer; Beamrate 10 Mann/s, Entern: Angreifer verliert halb so viele wie der Gegner; Wireframe-Teil i = System i (Schild, Antrieb, Leben, Sensoren, Waffen); Xplode.sod wird nicht gezeigt.
- debug-32: Objekte ohne Besatzung (crew_max 0) haben keine Ersatz-Huellenpunkte: sie arbeiten weiter, bis Systeme ausfallen oder ein kritischer Treffer sie zerstoert.
- Spielstand-Untersuchung: docs/formats/savegame.md, tools/exe_fields.py (Feldlisten der Save/Load-Routinen aus der Disassembly).
- debug-33: alter Autosave (state.txt, SaveState/LoadState) entfernt; Ersatz folgt als state.bzn im Originalformat (siehe docs/formats/savegame.md).
- debug-37: Menues wie im Original: Skirmish heisst Instant Action und liegt hinter Multiplayer (Multiplayer Connection: Internet/LAN IPX/LAN TCP-IP noch aus), Game Setup mit Previous Menu/LAUNCH/Main Menu; Einzelspieler nur mit Portraets (Kampagne noch aus), Previous Menu, Load Game (noch aus). Minikarte nutzt die Kartengrenzen aus dem Kopf (2duel ist 12800 x 3200, nicht quadratisch). Positionen der neuen Bildschirme aus Original-Screenshots geschaetzt.
- debug-36: Fix Spielstand: Klassenbezeichnung fuer ODFs ohne Kartenschema kam aus dem Anzeigetext (Schiff/Station) -> Speichern scheiterte still, nach Neustart nur Hauptmenue.
- debug-35: leere Spielwiese entfernt (Skirmish-Liste, Kartenmenue, Start). 
- debug-34: state.bzn im Originalformat (formats/SaveFile, SaveSchema, SaveGame; Game::SaveState/LoadStateLater). App verlaesst den Vordergrund (APP_CMD_PAUSE): Stand wird nach internalDataPath/state.bzn geschrieben; RESUME im selben Prozess loescht die Datei; Kaltstart laedt sie und loescht sie. Es gibt keine leere Spielwiese mehr (im Original nicht vorhanden): Skirmish-Liste und Kartenmenue enthalten nur echte Karten. Kopf/Fuss nicht gegen einen echten Original-Spielstand geprueft (siehe savegame.md). Linux-Client noch ohne Anbindung.
- Spielstand: Objektklassen aus der exe entschluesselt (123/140 ODFs, docs/formats/savegame-classes.txt); Teams/KI/Missionsfuss und Schreiber/Leser stehen aus.

- debug-38 (in Arbeit): HoverSim.cpp = originale Flugphysik (Simulate 0x459440) portiert + Test; Physik+Servos+Hindernisvermeidung gegen das Original per Unicorn-Emulation geprueft (tools/hover_check.py, tools/avoid_check.py); in Game eingebunden (HoverShip.cpp, APK debug-38); Pfadfinder (0x40a310) nur analysiert. Siehe docs/formats/movement_re.md.
- debug-39: Tipp auf das Drahtgittermodell im Schiffsfeld (einzeln oder eines der kleinen Felder) setzt die Kamera auf das jeweilige ausgewaehlte Schiff (UiAction::Focus0..7).
- debug-40: Kamerasprung (Drahtgitter) nur seitlich; Tippebene fix y=0 (Ziele liegen auf y=0, Schiffe fliegen auf diese Ebene); Warp laeuft automatisch wie im Original (nur mit warpSpeed > impulseSpeed, Ziel weiter als tooCloseForWarp, Kurs innerhalb headingCloseEnoughForWarp).
- debug-41: Tempo/Warp-Anzeige im Schiffsfeld (neben der Klasse). tools/warp_sim.cpp = Flug mit echter Physik-ODF (Entfernung, Start-Kurs, Hindernisse); Warp in der Simulation ok (1400 m: Warp nach ~3 s bis ~80 m/s).
- debug-42: Kartenobjekte als Hindernisse der Ausweichroutine. Original: Objekt +0xd8 = ODF avoidMe (Klasse +0x1a5; Standard 1, Nebel/Hintergrund 0); Flag 0x8000 wird nirgends gesetzt (Sonderpruefung 0x42ed20 = Strahl-Kugel-Test, tritt nie auf). Planeten, Dilithiummond, Asteroidenfelder werden ausgewichen; Wurmloch/Schwarzes Loch/Lichter nicht. ANNAHME: Radius Planet = Modellradius*Skalierung, Asteroidenfeld = fieldRadius. Nicht per Emulation geprueft (nur Filterlogik aus dem Disassembly).
- debug-43: Asteroidenfeld als Ganzes undurchdringlich (Kugel fieldRadius + Felsenradius, Schiffe werden an die Oberflaeche geschoben, auch ueber/unter dem Feld); Nutzerbeobachtung am Original.
- debug-44: GOG-RTS_CFG.h gelesen: OBSTACLE_AVOIDANCE = 0 -> Ausweichroutine 0x42ef70 springt im Original direkt zur Hoehenbegrenzung (Boden SHIP_COMBAT_FLOOR=10, Decke SHIP_COMBAT_CEILING=67, Globals 0x633a74/0x5fbc0c). Schiffe weichen also weder Schiffen noch Planeten aus (HoverWorld() liest RTS_CFG.h). Undurchdringliche Asteroidenfelder kommen vom Pfadfinder: AsteroidField @0x44e7b0 stempelt per 0x409900 Kosten ins Pfadraster (Radius ASTEROID_BELT_PATHING_DISTANCE*0.5 = 75, Kosten > CANNOT_PATH_INTO_COST 200). Pfadfinder noch nicht portiert; Zwischenloesung: harte Kugel um Felder (Game::KeepOutOfFields).

## debug-45: PathGrid (NAEHERUNG des Original-Pathfinders)
- Original stempelt Asteroidenfelder (Quadrat Halbkante 75, Zelle 50, Funktion 0x409900/0x40c700) in ein Wegkosten-Gitter; Schiffe laufen per Wegsuche (0x40a310) darum herum. OBSTACLE_AVOIDANCE=0 in der GOG-cfg.
- engine/src/game/PathGrid.*: NUR Stempel + Zellgroesse aus dem Original; Suche ist A* + Sichtlinien-Glaettung (NICHT identisch). Wegpunkt erreicht bei < 150 (pathLeadDistance).
- Offen: exakter Port 0x40a310/0x40ac80/0x40b8c0/0x42ded0, Stempel fuer Nebel/Schwarze Loecher/Planeten (0x450845, 0x451f10).

## debug-46/47: Hindernisvermeidung-Schalter, EXAKTER Pathfinder-Port
- debug-46: Erweitert-Menue: Schalter "Hindernisvermeidung" (OBSTACLE_AVOIDANCE, Original aus).
- debug-47: PathGrid.* ist jetzt ein EXAKTER Port von 0x40a310 (Wrapper), Quadtree/Stempel (0x408e30/0x409900/0x40c700), Zusammenfassen (0x40d280), A* ueber Regionsmitten (0x408500, MSVC-Heap, float32), Rubber-Banding (0x40ac80 Torgraph+A*, 0x40b8c0 Trichterglaettung, 0x40aa20 Tore, 0x40b680/0x40b4f0/0x40b490). Geprueft gegen die Emulation des Originals: 10 + 24 Zufallsszenarien (tools/oracle_rub.json, oracle_rand1/2.json; tools/path_check.cpp + path_check.py; Erzeugung tools/path_gen_rand.py) -> alle identisch inkl. Eigenheiten (Doppelpunkt, nicht-optimale Knicke).
- Wrapper-Regeln: Ziel ausserhalb [min-500, max+500] oder in gesperrter Zelle -> kein Weg (FindPath=false, Schiff fliegt dann geradeaus); gleiche Region -> direkt; Kartenbereich Game.cpp: z = -z_GL (Fix: vorher z ungespiegelt in den Grenzen).
- Offen: weicher Ring um Felder (Byte 0x5fdfc8), Stempel Nebel/Schwarzes Loch/Planet, Kantenkosten Regionssuche mit Zellkosten (nur Standardzellen geprueft), Kartengrenzen des Originals (0x6343f0..: hier aus Objekten +-1000), Pfadverfolgung 0x42ded0 nur als "< 150" nachgebaut, Verhalten bei "kein Weg".

### debug-48 bis 50
Weicher Feldring (StampField) aktiv; Kantenkosten 0x40e470; Nachbarlisten-Reihenfolge exakt (rekursives Linking). Offen: Nebel/Loch/Planeten-Stamps, Kartengrenzen, Wegverfolgung, "kein Pfad". Siehe docs/pathfinder_notes.md.
debug-50: Pfadraster stempelt zusaetzlich Schwarzes Loch (gesperrt), Nebel (pathing_cost), Planet (198), Wurmloch (500, unsicher). Siehe docs/pathfinder_notes.md.

### debug-51 bis 65 (Uebergabe an Claude Code, 11.11.2026)
**Arbeitsweise (gilt weiter):** Vor jedem Nachbau den Originalcode ansehen ("schau dir wirklich jedesmal den code an und bau ihn dann nach"), bei Antworten klar trennen zwischen verifiziert und angenommen. Jack antwortet auf Deutsch, kurz. Bei jeder APK die Build-Nummer erhoehen: `engine/src/core/Version.h` (FLEET_BUILD) und `android/app/build.gradle.kts` (versionCode, versionName "0.1.N-debug"). APK als `openfleet-debug-N.apk` ins Repo-Wurzelverzeichnis legen und Jack schicken.

**APK bauen (VM):** `rsync -a --exclude local.properties --exclude build --exclude .gradle --exclude originalFiles --exclude reference --exclude work <repo>/ $HOME/tc/proj/`, dann in `$HOME/tc/proj/android`: `JAVA17_HOME=$HOME/tc/jdk17 ANDROID_HOME=$HOME/tc/asdk ./build.sh` (dauert 1-2 Min., im Hintergrund starten und das Log pruefen). Ergebnis: `app/build/outputs/apk/debug/app-debug.apk`. Achtung: Der GL-Render-Code ist nicht Teil des Host-Builds (ninja meldet "no work to do"), Compilefehler dort zeigt nur der Gradle/NDK-Build. rsync `--delete` loescht `local.properties`, deshalb ausschliessen.

**Shader-Fallen:** Uniforms, die in Vertex- und Fragmentstufe vorkommen, brauchen gleiche Praezision (`highp` ausdruecklich setzen, sonst Linkfehler und schwarzer Bildschirm). Weltpositionen als `highp`; Punktlichter werden pro Fragment berechnet (Varying-Limit).

**Erledigt:** Punktlichter fuer Geschosse (bis 32 pro Fragment, Original: 0x58d7f0 / 0x58e5b0), Nebel-/Ionensturm-Glow-Lichter (ODF-Schluessel *_glow, one_in_n_chance_of_light_source), Nebelfarbanimation ersetzt die Knotenfarbe (SpriteRenderer, Kanal "colour"), Symbolansicht (siehe unten). Details in `docs/model_audit.md` Teil 3d bis 3f.

**Symbolansicht (debug-64/65):** Schiffe und Stationen mit Team > 0 werden unter `sym_px` (Standard 48 Pixel Bildschirmdurchmesser) durch ein kamerazugewandtes HUD-Sprite ersetzt, eingefaerbt mit `kMapTeamColors`. Modi (Erweiterte Einstellungen): aus / Fraktionslogo (`federation_icon` usw. aus gui_federation.spr, Rasse nach erstem ODF-Buchstaben f/k/r/b) / Drahtgitter (`<odf>w1..w5` aus gui_global.spr). Auswahlecken `selection.00/20/02/22` nur fuer ausgewaehlte Schiffe. Code: Game.cpp (`sym_small`, `syms`, Block "Symbolansicht"). **Noch nicht bestaetigt:** ob Drahtgitter-Modus richtig aussieht (debug-64 zeigte nur den fehlerhaften Logo-Modus). Offen: weiche Ueberblendung, Stationen ohne Auswahlzustand, vereinfachtes Waffenfeuer in der Ferne, Kartenobjekte wie auf der Minimap, Farbenblind-/Freund-Feind-Modus (soll auf alle Teamfarben wirken), Zweck der Auswahlecken im Original klaeren.

**Backlog / offen:**
- Wrack-Fehler: Ein beschossenes Schiff wurde unauswaehlbar, trudelte (`s.st.yaw += 0.7f*dt` im Derelict-Zweig) und explodierte nie.
- Potentialfeld / Collision.cfg (Bauschiff fliegt durch die Sternbasis), Sonnen, LOD-Wahl (0x57c810).
- Licht: Beleuchtungsmodell 3, Specular-Blickvektor, ob die Glow-Lichter im Original ohne Config-Flag ST3D_SHOW_LIGHT_SOURCE_INFO (Global 0x628c78) aktiv sind, Phasenrate der Animation (30/s angenommen).
- Optionsmenue: Hauptmenue-Optionsknopf ist noch deaktiviert; Einstellungen stehen unter "Erweiterte Einstellungen" im Spielmenue (MenuKind::Advanced), DEBUG-Menue bleibt separat.
- Aeltere Liste: weitere Menuebildschirme, Savegame, HUDs, Missionen, Gegner-KI.
