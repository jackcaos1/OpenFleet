# Audit Modell-Renderer gegen Armada.exe (Stand debug-54)

Methode: Disassembly (/tmp/arm.asm, objdump) der ST3D_*-Klassen (RTTI: ST3D_Node vtable 0x5d4308, ST3D_Mesh 0x5d4258, ST3D_Mesh_MRM 0x5d42b4,
ST3D_LODControlNode 0x5d420c, ST3D_SpriteNode 0x5d437c) und der Aufrufer. Noch KEIN Bildvergleich mit dem laufenden Original.

## Belegt (Code gelesen)
- Modell-Lader 0x59c130 sucht benannte Knoten ueber 0x57ff30: `hardpoints` (wird versteckt: Flags |= 3), `damage` (+0x2c), `lights` (+0x30, sonst `light`).
- Sichtbarkeit eines Knotens: Bits in [Knoten+0x58] (Bit 0 und 1 = versteckt). LOD-Knoten (0x58ef60) zeigen genau EIN Kind, alle anderen bekommen die Bits gesetzt.
- LOD-Wahl ist NICHT "immer hoechste": 0x57c810 projiziert die Position auf den Bildschirm und bildet eine Groessenzahl; Schwellen 20 / 60 / [0x5ca86c] / [0x5ca898] ergeben Index 0..4, gedeckelt durch die Detail-Einstellung ([0x6d4aa0+0x11c]+0x2c); Flag [0x6d4aa0+0x100] (0x589d10) schaltet auf festes Kind.
- `oq` = Offiziersquartier-Ausbauten (NICHT Okklusionshuellen, die Annahme in sod.md war falsch): ODF `maximumUpgrades`/`officerGain`; Lader 0x457070 sucht Knoten `oq` und darunter `oq%d` (1..maximumUpgrades); je ein Eintrag pro Ausbaustufe.
- Craft (0x460xxx): sucht die Knoten `crew`, `engines`, `life`, `sensors`, `shields`, `target`, `borg` ([Craft+0x438..0x44c]). 0x467800: Bit 1 dieser Knoten = Zustandsbyte des jeweiligen Subsystems (Schadensanzeige je Subsystem); `damage`-Kinder werden nach Gesundheitsanteil (Rumpf/[odf+0x204]) einzeln ein-/ausgeblendet; `borg`-Knoten ist nur sichtbar, wenn [Objekt+0x1a4] == Klasse aus 0x4440b0("borg") (Borg-Eigentuemer), sonst versteckt.
- Bbox/Kugel (0x5967f0, 0x5520f0) siehe pathfinder_notes.md (alle Mesh-Knoten, alle LODs).
- ST3D_Mesh Slot 6 (0x5910a0) skaliert die Eckpunkte (x,y,z-Faktoren) gegen eine gesicherte Kopie ([+0x98]).

## Engine weicht ab / unbelegt
| Thema | Engine | Original |
|---|---|---|
| LOD | immer hoechste Stufe | Bildschirmgroesse, 5 Stufen, Detail-Deckel |
| oq | immer versteckt | Ausbaustufe n zeigt oq1..n |
| damage | immer versteckt | Kinder je Gesundheitsanteil |
| Subsystem-Knoten (crew, engines, life, sensors, shields, target) | nicht gesondert behandelt (im Builder kein Eintrag; sie werden wie normale Knoten gezeichnet, sofern sie Netze haben) | je Subsystem-Zustand |
| borg-Ueberzug | nur ueber Debug-Variante | sichtbar bei Borg-Eigentuemer |
| Sprite-Knoten (0x599000), Lichter-Knoten | Heuristik "Licht in oq-Huelle ausblenden" | nicht gelesen |
| Animationen (Kanaele, Textur-Anim.) | nicht ausgewertet | nicht gelesen |
| Matrix-Konvention/Drehung, Windung, Cull, Lichtmodell, Teamfarbe, Alpha | aus Datenvergleich/Screenshots | nicht im Code geprueft |

## Naechste Schritte (Vorschlag)
1. ST3D_Mesh Render-Slots (0x5909f0/0x590da0/0x591050) lesen: Windung, Cull, Material, Teamfarben-Ersatz, Alpha.
2. Knotenmatrix-Konvention (0x596b00/0x4500d0) gegen SodWorldMatrices pruefen.
3. LOD-Schwellen und Detail-Deckel portieren; oq/damage/Subsystem/borg-Sichtbarkeit nachbauen (braucht Gesundheit/Ausbau/Eigentuemer-Zustand).
4. Sprite-Knoten und Animationen lesen.

## Teil 2: Mesh-Laden, Normalen, Beleuchtung (debug-55)
Belegt (Code gelesen):
- Mesh-Lader (ST3D_Mesh Slot 8, 0x5909f0; gleich fuer ST3D_Mesh_MRM): Textur- und Materialname, Eckpunkte, UVs, Gruppen (je 0x1c Byte, Polygon 0x28 Byte), danach u8 (Cull-Byte, [+0xa4]) und u16-Liste ([+0xa8]/[+0xac]) = die "Zusatzliste am Meshende" (Verwendung unbekannt).
- **UVs werden beim Laden auf 0..1 geklemmt** (0x590c31..0x590c9c). Engine: jetzt ebenfalls (Sod.cpp), vorher GL_REPEAT ohne Klemmung.
- **Normalen** (Slot 2, 0x58faf0 / 0x5a2f40 / 0x5a2fc0): je Positions-Vertex des Meshes, Summe der Polygon-Normalen ueber ALLE Gruppen, danach normalisiert (0x55dd50). Engine: je (Position,UV)-Ecke und Gruppe getrennt -> an UV-Naehten und Gruppengrenzen andere Glaettung. NICHT angepasst.
- Windung: Original nutzt die Polygonreihenfolge direkt (Direct3D); die Engine bestimmt sie per Volumenvorzeichen (Heuristik) und kann damit bei offenen/gemischten Modellen abweichen. Nicht geprueft, welcher D3D-Cull-Modus gesetzt wird.
- Beleuchtung im Original: eigene Klassen ST3D_Directional_Light (vtable 0x5d4148), ST3D_Point_Light (0x5d40f8), ST3D_Spot_Light (0x5d4198), ST3D_Light (0x5d40a8), ST3D_LightingMaterial, ST3D_VertexLighting_Group (0x5d4588 = Vertex-Beleuchtung auf der CPU), Material-Palette `SOD/Material.SOD` ("Couldn't load lighting material palette (material.sod)"). SOD-Materialien tragen ambient/diffuse/specular/Glanz/Lichtmodell.
- Karten-Ambient: BZN/Savegame lesen `AmbientRed/Green/Blue` (Floats, Globals 0x6d6810/14/18; Default-Setzer 0x496605). Engine liest/nutzt sie nicht.
- Richtungslicht: Karten enthalten ein Objekt `dlight0` (ODF dlight.odf: classLabel background_obj, hidden, Modell dlight.SOD = Markerform). 0x4395e0 laeuft ueber alle Objekte AUSSER diesem und ruft je Objekt Slot 0x1c auf (vermutlich Licht anwenden). Lichtrichtung/-farbe aus dem dlight-Objekt: nicht gelesen.
- Fog-of-War-Licht: `light.odf` (ghostAmbient/ghostDiffuse 0.1, ghostSpecular 0) fuer Objekte im Nebel des Krieges (0x5149cf). Engine: nichts.
- Weitere Lichtquellen/Emission in ODF-Schluesseln: lightColor(Red/Green/Blue), lightFalloffStart/Range, ord_light, neblight, glow_falloff_start/range, red/green/blue_glow(+_range), one_in_n_chance_of_light_source (Leser 0x44c22b..0x44c392, Karten-/Nebelobjekte), flareSprite (0x4e171c), pflare, oreglow/glow_ore. Nicht gelesen, nicht portiert.

Engine-Beleuchtung (ModelRenderer): erfunden. Feste Richtung (-0.4,0.6,0.7), Schattierung 0.35+0.65*|n.l|, Lichtmodell 0 = unbeleuchtet; ambient/specular/Glanz der SOD-Materialien ungenutzt; "Alpha-Modi" (Alpha = Emission, Bump aus Alpha) sind Debug-Experimente ohne Beleg.

## Teil 3: Vertex-Beleuchtung (gelesen, noch nicht portiert)

Quelle: 0x5a4410 (Treiber), 0x5a4f80 (Packen), Lichtklassen ST3D_*Light.
- Pro Modell-Vertex Startwert (RGB) = Szenen-Ambient ([0x6d4aa0]+0x2c..0x34) + Material-Ambient ([mat+0x58/5c/60]), Alpha-Feld 0xffffffff (0x5a4495..0x5a44ce).
- Szenen-Ambient = Karten-AmbientRed/Green/Blue (Globals 0x6d6810/14/18, kopiert bei 0x5b94f9..0x5b9519). Default-Setzer 0x496605 (Wert noch nicht gelesen, Quelle 0x4955a0).
- Danach je Licht (Liste [scene+0x70]): Richtung ins Modell-Koordinatensystem transformiert, Slot 13 = Lambert d = L.n > 0: diffuse += d * Lichtfarbe * Material-Diffuse ([mat+0x68/6c/70]); Slot 15 = zweiseitig |d|.
- Ergebnis je Kanal auf 0..1 begrenzt, *255 gerundet (0x5a4f80, Magic-Rounding 0x5d4578), Alpha = Material-Opacity [mat+0x64]*255.
- Spekular Slot 14 (0x58dd90) nicht gelesen. Lichtrichtung/-farbe des Objekts dlight0 noch nicht belegt.
- Engine bisher: erfundene Festrichtung (-0.4,0.6,0.7), shade = 0.35+0.65*|n.l|. Das weicht ab (einseitig statt |d|, Ambient/Material-Diffuse fehlen).

### Teil 3b: dlight0 / Ambient (debug-56 portiert)
- dlight-Objekt: ODF-Name "dlight" erzeugt in 0x46b0b6 ein ST3D_DirectionalLight (ctor 0x58db20: Typ 1, Farbe Vorgabe weiss 1,1,1 aus 0x57f98a, Falloff 0).
- BZN: die drei Floats NACH der Matrix des Objekts = lightColorRed/Green/Blue (belegt ueber SaveSchema + 106 Lichter in den Karten, viele Karten haben mehrere Lichter mit eigener Farbe). Richtung kommt aus der Objektmatrix: Lichtvektor [+0x74] = 3. Zeile (vorne) des Knotens, ins Modell transformiert (0x5a4638), ohne Vorzeichenwechsel.
- Karten-Ambient: BZN-Kopf Token 10..12 (Floats 0..0.4; 37 Karten = 0). Zuordnung zu AmbientRed/Green/Blue plausibel, nicht ueber Code belegt.
- UNBELEGT: Vorzeichen. Das Original rechnet d = vorne . n > 0 ohne Negation. Die Standardkarte hat vorne = (0.577,-0.577,0.577) (zeigt nach unten). Engine nimmt Richtung ZUM Licht = -vorne (Licht von oben). Ob das Original das durch umgekehrte Normalen ausgleicht, ist nicht gelesen (Prepare-Normalen/Windung).
- Engine (debug-56): Szenenlicht wie oben (max 4 Lichter), Material-Ambient/-Diffus, Begrenzung 0..1, je Fragment statt je Vertex; Himmelskasten ohne Szenenlicht. Modellbetrachter behaelt die alte Festbeleuchtung.
- Sonnen (msun1/2/3, Sprite-Knoten, planets.spr 200x200, Zusatzwert 4.0 im SOD): Groesse/Sichtbarkeit spaeter lesen (0x599000).

### Teil 3c: Vorzeichen, Normalen, Lichtmodelle (debug-57)
- Lichtmodell je Material ([mat+0x4c]) waehlt eine Funktion aus der Tabelle [renderer+0x80/84/88] (0x590401): 0 = 0x5a4200 (feste Diffusfarbe, geklemmt, unbeleuchtet), 1 = 0x5a4410 (Ambient + Diffus), 2 = 0x5a4890 (zusaetzlich Glanzlicht, Spekular-Register). Modell 3 (24 Materialien) hat keinen Eintrag bei +0x8c gefunden: Engine behandelt es wie 1.
- Glanzlicht Slot 14 (0x58dd90): Halbvektor H = normalize(V + L) (V = Blickvektor 0x6d4b80..88), t = n.H, bei t>0: t/(p - p*t + t) (p = Material-Power [mat+0x88]) * Lichtfarbe * Material-Spekular, als Spekular-Register addiert. Engine: V = (0,0,1) im Kameraraum (naeherungsweise), nur bei Modell 2.
- VORZEICHEN GEKLAERT (per Emulator): Dreiecksnormale = (v1-v0)x(v2-v0), normiert (0x5a3120 -> 0x55e240). Die SOD-Dreiecke sind in GL-Raum gegen den Uhrzeigersinn (205 von 226 Modellen, Rest = Himmelskaesten), im Original-Raum wegen der z-Spiegelung also Normalen NACH INNEN. Damit ist L = vorne-Achse des dlight = Richtung zum Licht in "Innen"-Normalen, also fuer Aussennormalen Richtung zum Licht = -vorne. Genau so setzt es die Engine. Belegt.
- Vertexnormalen: normierte Summe der EINHEITS-Dreiecksnormalen je Positionsindex des Mesh ueber alle Gruppen (0x58faf0/0x5a2f40). Engine hatte flaechengewichtet und an UV-Nahten getrennt; jetzt wie das Original.
- Build-Nummer: engine/src/core/Version.h (FLEET_BUILD, bei jedem APK hochzaehlen) + android/app/build.gradle.kts versionCode/versionName; Anzeige d<Build> rot unten rechts im Hauptmenue.
- Hauptmenue (Original, Armada.exe 0x546150..0x546731, Steuerelement 0x5239a0 mit Argumenten x,y,w,h): Single (22,10,136x16), Multi (488,10,137x16), Options (22,452,100x16), Replay/Intro (439,451,140x16), Credits (513,433,84x15), Quit (544,415,67x16). Versionstext: Textfeld 0x5255a0 bei (600,468) 40x10, rot; Text im Original fest "v1.2" (String 0x610764 in der Exe). Engine zeigt "d<Build>" an derselben Stelle.
- Menue-Anordnungen als Daten: engine/src/game/MenuLayout.cpp (Tabelle je Bildschirm + Variante mit Seitenverhaeltnis-Bereich; Vorgabe "original" = Exe-Werte). Neue Variante = weiterer Eintrag mit min/max_aspect; Anker Frame/Left/Right und Frame/Top/Bottom erlauben Randbuendigkeit bei Breitbild/Hochformat. Bisher nur Hauptmenue ausgelagert; Einzelspieler/Multi/Skirmish stehen noch fest in Shell.cpp.

## Teil 3d: Punktlichter (debug-59)
ST3D_PointLight (vtable 0x5d40f8), Slot 13 = 0x58d7f0 (pro Vertex), gelesen:
- Reichweite = falloffStart [+0xa0] + falloffRange [+0xa4]; Vertex ausserhalb wird uebersprungen.
- Intensitaet = max(n·L,0) (L = normierte Richtung zum Licht); bis Abstand falloffStart voll, danach linear 1-(d-start)/range.
- Beitrag = Intensitaet × lightColor [+0x90..0x98] × Materialdiffus (nur diffus, kein Glanz), auf die Vertexfarbe addiert.
- Slot 15 (0x58e5b0) = Sichtbarkeitstest der Kugel (Weltposition, Radius start+range).
Umsetzung: Geschosse (nicht Strahlen) mit ODF lightColor/lightFalloffStart/Range, max. 4 Lichter, pro Fragment gerechnet (Original pro Vertex).
Offen: Nebel-/Tor-glow_*-Lichter, Skalierung bei Karteninstanzen (Welt-Matrix wird ungeprueft verwendet), Lichtmodell 3, Blickvektor fuer Glanz.

Backlog (Beobachtung debug-60): beschossenes Schiff war irgendwann nicht mehr auswaehlbar, trudelte nur noch durchs All und explodierte nie (Zerstoerung/Auswahl untersuchen).

## Teil 3e: Glow-Lichter der Wolken (Armada.exe, gelesen)
- Konstruktor 0x44bfa0 (Nebel-/Sturm-/Tor-Klasse) liest: +0x22c one_in_n_chance_of_light_source, +0x230..0x238 red/green/blue_glow, +0x23c..0x244 *_glow_range, +0x248 glow_falloff_start (Std 140), +0x24c glow_falloff_range (Std 10), +0x224 Schwankungs-Frequenz (Std 2.5); legt ein ST3D_PointLight an ([+4]: falloff start/range in +0xa0/+0xa4).
- Wolkenteil-Init 0x451e00: Lichtflag [wolke+0x18] = (Lichter global an, 0x628c78>0) && Zufall(one_in_n)==0; Startfarbe = glow-Basis, Alpha -1.
- Pro Bild 0x452500 (nur wenn Flag): s = f(freq * [wolke+0x498]) (0x500260 auf Objekt 0x67dba8, vermutlich Sinus-/Rauschkurve); Farbe_k = max(0, base_k + s * range_k) je Kanal.
- Offen: Lage des Lichts je Wolkenteil (Wolkenpositionen/-anzahl), Funktion 0x500260, wo [wolke+0x498] hochgezaehlt wird.
- Umsetzung debug-61: Objekte mit one_in_n_chance_of_light_source: Wurf beim Kartenladen, Licht an der Objektposition, Farbe = Basis + Schwankung*sin(freq*phase), phase pro Originalbild += |sin(freq*phase)|*range*rate + min (0x4522f0; freq/min/range = ODF speed_change_*; 30 Bilder/s ANGENOMMEN). Bis zu 8 Lichter gesamt, die dem Blickpunkt naechsten. Toere/Wurmloch (nur feste Farbe) noch ohne Licht.
- Zu Teil 3e Position: die "Wolke" ist das Kartenobjekt selbst (this+0x8c = Objektposition), nicht ein Teilchen.
- Pruefung debug-62: (1) Wolken-Update 0x4522f0/0x452500 laeuft in Objekt-Update (vtable 0x5cb50c Slot 0x84) einmal pro Hauptschleifen-Durchlauf (0x43f1a0 -> 0x471410), also pro BILD, nicht pro Sim-Tick; Bildrate unbekannt (30/s bleibt Annahme). (2) Lichtliste des Originals = verkettete Liste ([Renderer+0x70], 0x5a4410), keine Obergrenze -> Engine jetzt 32 Lichter (je Fragment, Lagen relativ zum Blickpunkt, highp).
- OFFEN/WICHTIG: Lichtflag der Wolke (0x451e00) wird nur gesetzt wenn globale 0x628c78 > 0; das ist die Config ST3D_SHOW_LIGHT_SOURCE_INFO, in ART_CFG.h = 0. Ob die Glow-Lichter im Original ohne diese Debug-Option ueberhaupt leuchten/flackern ist damit UNGEKLAERT (Weg Wolkenfarbe [obj+0x1c] -> Lichtknoten nicht gelesen). Wurmloch/Tore nutzen +0x230 nicht (0x453f00 verweist nicht darauf).

## Teil 3f: Nebel-Rendering (debug-63)
- Nebel = 8 Sprite-Knoten je Modell (Mnebula1..5.sod), kein weiteres Erzeugen von Wolkenteilchen (0x409900 ist eine Nebel-des-Krieges-Flaeche, kein Partikelaufruf). Blend additiv Eins/Eins (Material 0x5891e7, @tmaterial=additive).
- Farb-Animationskanal (Klasse Ctor 0x577540, vtable 0x5d3b28, Anwenden 0x577760): schreibt die interpolierte Keyframe-Farbe (auf 0..1 begrenzt) in die Zielfarbe des Knotens, ERSETZT also die @sprite_node-Farbe. Bisher multipliziert -> Nebel ca. 5-10x zu dunkel. Alle Nebel-Knoten tragen dieselben Platzhalterfarben (.2,.1,.1)/(.4,.2,.1)/(.1,.2,.4)/(.1,.5,.2); die Typfarbe kommt allein aus der Animation (yellow/red/purple/green/blue).
- Kontext-Faktor [0x6d4aa0+4] (Start 1.0) multipliziert die Knotenfarbe beim Zeichnen; Wert zur Laufzeit nicht weiter verfolgt.
