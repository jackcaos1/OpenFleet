# Original-Pathfinder (Armada.exe) - Arbeitsnotizen
Stand: Wrapper 0x40a310 gelesen (thiscall ecx=Planer, ret 0x28 = 10 Argumente).
- Planer+0x10 = Kostengitter (Quadtree, @0x6322b8). Zelle 28 Byte: +0 Kosten, +4 Region/Marker, +0x18 Flags (bit0 = unterteilt). Gitter: +0 Ebenen[], +4 Ebenenzahl, +8 Impassable-Wert, +0xc Standardkosten, +0x10/+0x14 Max X/Z, +0x20 Zellgroessen je Ebene (float[]), +0x24 Rand.
- 0x40c5e0(x,z): Zelle am Punkt (klemmt auf 0..max-rand, steigt Ebenen ab solange bit0).
- 0x40c700: Stempel (Rechteck, max-Kosten, Flag 0x28 = vorhandenes Impassable nicht ueberschreiben), rekursiv.
- 0x40a310 Wrapper: Start/Ziel in Gitterkoordinaten (minus Kartenversatz @0x6343f0/f8, +0x5ca220 Rundung, Klemmen). Gleiche Zelle (Kosten gleich) ODER 0x5fb86c==0 (USE_PATH_PLANNING) ODER Flag -> direkter Pfad (2 Punkte Start,Ziel). Sonst 0x408500(Regionen) == 1 -> kein Pfad (return 0). Sonst Suche 0x40ac80 + Verfeinerung 0x40b8c0 (nur wenn 0x5fb87c an). Pfadpunkte: x, y=(CEILING+FLOOR)*0.5 (0x5fbc0c+0x633a74)*0x5ca224, z; letzter Punkt = Ziel.
- Offen: 0x40ac80 (Suche, nutzt 0x406cb0/407330/4070c0/406e20 = Listen/Queue), 0x40b8c0, 0x408500, 0x42ded0.

## Architektur (aus 0x40a310 / 0x40ac80 gelesen)
Der Original-Planer ist KEIN einfaches Gitter-A*, sondern dreistufig:
1. Gitter -> Regionen/Portal-Graph (0x40c2f0 bei Dirty-Flag Gitter+0x28; Hilfen 0x40c570 Reset, 0x40d280, 0x40cd30, 0x40d420 Nachbarzelle).
2. 0x408500 = Graphsuche (Rueckgabe 1 = kein Weg, 2 = gefunden). Wird zweimal benutzt: auf Regionen (im Wrapper) und auf dem Knotengraph (am Ende von 0x40ac80).
3. 0x40ac80 baut aus dem Grobpfad (Liste >= 3 Punkte) einen Knotengraph: Knoten = 0x30-Byte-Objekte (0x407330/0x4070c0, vtable +0xc = X, +0x10 = Z), Kanten per 0x407b10(knoten, gewicht), Gewicht = Strecke * Zellkosten (fild [Zelle+0]). Nachbarn ueber 0x40aa20. Offene Liste 0x406e20.
4. 0x40b8c0 = Wegverfeinerung/Glaettung (nur wenn 0x5fb87c an).
5. 0x42ded0 = Wegfolgen im Schiff.
Aufwand grob: ~2500 Instruktionen + Hilfsklassen (Listen 0x406cb0/0x406c50/0x401000, Pfadobjekt 0x433d10/0x434750/0x434a50, 0x4fe2a0, 0x44ab30). Bisher NICHT portiert; PathGrid (A*) bleibt Annaeherung.

## Oracle (Unicorn) - tools/path_run.py  (Stand: funktioniert)
- `python3 tools/path_run.py scenario` laeuft den ORIGINAL-Planer: ctor 0x408e30, init 0x4093b0, Stempel 0x409900, Planung 0x40a310 (Args: start xyz, ziel xyz, 0, 0, &out, 0 ; ecx = Planer, Rueckgabe Pfadobjekt: +0 Anzahl, +4 Punkte (12 Byte)). ~10 s pro Szenario.
- cfg-Globals: 5fb868 OFF_MAP, 5fb870 IMPASSABLE(-1), 5fb874 DEFAULT_TERRAIN(100), 5fb878 CANNOT_PATH_INTO(200), 5fb86c USE_PATH_PLANNING, 5fb87c PERFORM_RUBBER_BANDING (Verfeinerung 0x40ac80/0x40b8c0 nur wenn an!), 5fbc48 Gitteraufloesung(50).
- Asteroidenfeld (0x44e848): Stempel 0x409900(planer, x,z,x,z, r,r, &planer[1]) mit r = 0x5fb8a8(=ASTEROID_BELT_PATHING_DISTANCE 150)*0.5 = 75, Kosten = planer+4 = IMPASSABLE. (Zusaetzlich weicher Ring r+40 mit Kosten 100*3, wenn Byte 0x5fdfc8 gesetzt, VOR dem Kern.)
- Ergebnis Feld bei (0,0), Weg (-600,0)->(600,0): ohne Rubber-Banding durch Quadtree-Blattmitten (-300,100),(-150,150),(-50,150),(50,150),(150,150),(300,100); MIT: (-200,100),(200,100). y der Zwischenpunkte = (CEILING+FLOOR)/2 = 38.5. => Quadtree mit ADAPTIVEN Zellgroessen ist wesentlich; nachbauen noetig.
- Szenarien (final): Feld (0,0): (-600,0)->(600,0): [(-200,100),(200,100)]; (-600,-300)->(600,300): [(-400,0),(-200,100),(400,200)]; (-600,200)->(600,200): direkt; (-100,0)->(100,0): [(0,100),(100,100),(100,100)].

## Stufe A (Quadtree-Stempel) - GELOEST, gegen Oracle verifiziert (tools/path_proto.py, /tmp-Skripte cmp.py/c3.py)
- Gitter: Raster-Koordinaten = Welt - kartenmin + 500 (0x5ca220 = 500 Rand). Ausdehnung W/H = |max-min| + 1000. 8 Ebenen, Zellgroesse = Aufloesung * 2^(7-k) (400..50), Zellen je Ebene ceil(W/groesse).
- Zelle (28 Byte): +0 Kosten, +4 Region*, +8 z1, +0xc z0, +0x10 x0, +0x14 x1, +0x18 Flags (bit0 = unterteilt). Ctor 0x408e30 (reset 0x40cfb0 / Grenzen 0x40d0a0) stempelt VIER Randstreifen der Breite 500+res (=550) mit IMPASSABLE: [0,W]x[0,550], [0,W]x[H-550,H], [0,550]x[0,H], [W-550,W]x[0,H], flag=1.
- 0x409900(x0,z0,x1,z1,r,_,&kosten): Rechteck [x0-r, x1+r] etc. nach Rasterkoordinaten, auf [0,W]/[0,H] geklemmt, dann 0x40c700(rect,&kosten,Ebene0,0,0,flag=1).
- 0x40c700 (rekursiv): Zelle ganz im Rechteck (x0<=cx0, x1>=cx1, z0<=cz0, z1>=cz1) und nicht unterteilt -> Kosten setzen (Regel unten) und Ende. Sonst: Schnitt Rechteck/Zelle leer (BERUEHREN zaehlt als Schnitt, also ix0<=ix1) -> Ende; letzte Ebene -> setzen; sonst unterteilen (Kinder erben Elternkosten, Flag setzen) und 4 Kinder rekursiv.
- Ueberschreibregel: setzen wenn (alt < neu (signed) und alt != IMPASSABLE) oder neu == IMPASSABLE oder alt == DEFAULT oder flag == 0.
- Kein Zusammenfassen von Kindern beobachtet (Dump-Vergleich 0 Diffs bei ~25 Zufallsszenarien).
- Erkenntnis: Pfad-Basisweg = Mitten der Quadtree-Blattzellen (Region je Blatt).

## Stufe B (Regionen + A*) - GELOEST fuer Basisweg (10/10 gegen Oracle) - tools/path_proto_astar.py, path_proto_gates.py
- Vor der Planung (0x40a310, wenn Gitter dirty) wird der Quadtree ZUSAMMENGEFASST (0x40d280): unterteilte Zelle, deren 4 Kinder alle Blaetter mit GLEICHEN Kosten sind, wird Blatt (rekursiv von unten). Regionen = Blaetter nach dem Zusammenfassen. (Erst damit stimmt die Startzelle, z.B. Blatt [-100,0]x[0,100] bei Feld (0,0).)
- Region: vtable 0x5ca188: +0x14 x, +0x18 z (Zellmitte, Rasterkoord.), +0x1c f, +0x20 g, +0x24 Elternknoten, +0x10 bit0 = geschlossen, +8 Such-ID, +0x28 Nachbarliste, +0x2c Kantenkosten (float-Vektor).
- Nachbarn = Blaetter mit gemeinsamer Kante positiver Laenge; Kantenkosten = Mittendistanz * 100 (Standardkosten; ob Zellkosten eingehen: bei Standardzellen nicht unterscheidbar, noch pruefen).
- A* (0x408500/0x408660/0x408940): Heuristik h = Mittendistanz zum Ziel * 100 (float 0x20 des Gitters = 100.0). Relax: Knoten ggf. zuruecksetzen (id != Such-ID); GESCHLOSSEN + besserer Weg (f_alt > h_alt+g_neu) -> wieder oeffnen, Daten setzen, in Heap; sonst wenn f==0 (neu) -> g=g(p)+e, f=g+h, Eltern=p, in Heap; offene Knoten werden NICHT aktualisiert. Pop: oberstes Element, geschlossen setzen, expandieren (Nachbarn in Listenreihenfolge, Region mit Kosten IMPASSABLE wird auch als Nachbar... im Test uebersprungen). Heap = MSVC push_heap/_Adjust_heap mit Vergleich ueber AKTUELLES f der Knoten (Kinderwahl: rechts, ausser f(rechts) > f(links); Aufstieg solange f(Eltern) > f(neu)). float32-Arithmetik (numpy).
- Startzelle liegt bei Raster-Koordinaten exakt auf Kante -> obere Zelle (untere Grenze inklusive). Start/Ziel duerfen in gesperrten Zellen liegen.
- Rubber-Banding (PERFORM_RUBBER_BANDING, 0x40ac80 + 0x40b8c0): Weg = kuerzester Pfad durch die TORE (gemeinsame Kanten aufeinanderfolgender Zellen); Naeherung exakter Sichtbarkeits-Dijkstra trifft 8/10 Oracle-Szenarien (tools/path_proto_vis.py). Abweichungen: doppelter Punkt (100,100) bei (-100,0)->(100,0) und nicht-optimale Knicke bei (-300,-300)->(300,300): Original ist ein Knotengraph (2 Knoten je Tor, Kanten zwischen aufeinanderfolgenden Toren, Gewicht Strecke*Zellkosten) + Glaettung 0x40b8c0 (nutzt 0x40aa20 Tor, 0x40b4f0, 0x40b680, 0x40b490) - NOCH ZU LESEN.

## Stand debug-47: Port fertig (Rubber-Banding)
- 0x40ac80: Regionspfad p (>=3 Regionen) -> Zellenliste [Startzelle, Zelle(p[1..]), Zielzelle]; je Paar (vorherige, aktuelle Zelle) Tor via 0x40aa20 (Knoten n1,n2 an der Kante der aktuellen Zelle; Richtung nach |dz|<|dx| und Vorzeichen; Reihenfolge n1/n2 siehe PathGrid.cpp MakeGate). Kanten: vorige Knoten (P1, P2) -> n1,n2 mit Kosten sqrt(dx^2+dz^2) * Kosten der VORHERIGEN Zelle (fimul, vorzeichenbehaftet), zuletzt -> Zielknoten mit Kosten der letzten Regionszelle. A* (gleiche Routine, h = Abstand*100) Start->Ziel.
- 0x40b8c0 (Trichter/"funnel"): Scheitel = erster Knoten; je Tor (ab dem zweiten) Torpunkte N1/N2 nach Drehsinn geordnet (0x40b4f0), Vorpruefung (Scheitel auf der richtigen Seite der Torlinie), Trichter (L,R) fortschreiben (0x40b680), dann muss der naechste Pfadknoten im Trichter liegen (Kreuzprodukte), sonst wird der zuletzt akzeptierte Knoten Wegpunkt und neuer Scheitel. Gleiche Zelle (Code 4) ueberspringt die Torpruefung.
- Wrapper: Ergebnis = Knoten in Weltkoordinaten ((min + x) - 500); erster und letzter Punkt werden durch Start und Ziel selbst ersetzt; kein Weg bei Ziel ausserhalb der Karte oder in gesperrter Zelle.
- Pruefung: tools/path_check.cpp gegen oracle_rub.json/oracle_rand1.json/oracle_rand2.json (34 Szenarien, alle gleich).

## Stand debug-48
- Weicher Ring um Felder (Byte 0x5fdfc8 = 1 in der exe): Stamp r=115 Kosten 300, danach Kern r=75 IMPASSABLE (PathGrid::StampField, in Game.cpp benutzt).
- Regionen-Kantenkosten 0x40e470/0x40e320: dist(Mitten) * (t*cost(A)+(1-t)*cost(B)), t = float(ha/(ha+hb)).
- ERLEDIGT: Nachbarlisten wie im Original rekursiv gebaut (0x40d450): Kinder c0=(2a,2b), c1=(2a,2b+1), c2=(2a+1,2b), c3=(2a+1,2b+1); Seitenlisten werden verkettet; danach 4 Zwei-Zeiger-Durchlaeufe (c0.oben-c1.unten, c0.rechts-c2.links, c3.unten-c2.oben, c3.links-c1.rechts), Vorschub immer nach Oberkante. Kanten in Ereignisreihenfolge angehaengt. Orakel: rub 10/10, rand1 12/12, rand2 12/12, soft1 12/12, soft2 14/14.
- OFFEN: Stamps fuer Nebel/Schwarzes Loch/Planeten, echte Kartengrenzen (0x6343f0..), Wegverfolgung 0x42ded0, Verhalten bei "kein Pfad".

## Stand debug-50: Stempel der Instanz-Initialisierung (Armada.exe, Slot 16 der Instanzklassen)
- Schwarzes Loch (0x450810): Stamp(x,z, BLACKHOLE_PATHING_SAFETY_COEFFICIENT(1.5) * ODF EventRadius (Default 10), BLACKHOLE_PATHING_COST = -1 gesperrt).
- Nebel (0x451e00): Stamp(x,z, NEBULAE_PATHING_SAFETY_COEFFICIENT(1.8) * effectRadius, Kosten = ODF pathing_cost).
- Planet (0x4531a0): Stamp(x,z, Kugelradius [this+0x60], Kosten 198 fest).
- Wurmloch (0x453f00): Stamp(x,z, 0, Kosten 500 fest). Geklaert: [odf+0x1e0] ist im Basis-ODF ein Zaehler (Startwert 0, 0x4442e7), als float gelesen = 0.0; +80 (0x5fb8a0) geht nur in den zweiten Radius (+0x1c). 0x5fb89c=500 wird nie per cfg ueberschrieben.
- Pruefung: tools/path_gen_stamps.py + path_check_stamps.cpp (Nebel/Schwarzes Loch/Planet/Wurmloch gegen den Original-Planer): 34/34 identisch (Seeds 5 und 9).
- UNSICHER bleibt nur die Quelle des Planetenradius [this+0x60] (Kugel um Objektursprung, Mittelpunkt [+0x54..0x5c]); Engine nimmt Modellradius*Skalierung (halbe Bounding-Box-Diagonale), Schreibstelle im Original nicht gefunden.
- Klassen ueber Ctor-Aufrufer zugeordnet (0x44b98a Schwarzes Loch, 0x44c4fa Nebel, 0x44ceea Planet, 0x44d96a Wurmloch).
- Neu gefunden: 0x46d5c0 (Basis-Init aller Objekte) stempelt per dynamic_cast auf CraftClass (+0x235 Flag) die Bounding-Box (Kosten 198, Rand = ODF footprintBuffer [+0x23c]) -> Fussabdruck von Stationen/Gebaeuden. Planeten sind keine CraftClass, bekommen ihn nicht.
- Offen: Quader-Stempel (0x4510a0 / 0x46d5c0, Kosten 198, Radius [+0x23c]) fuer Stationen/Schiffe; echte Kartengrenzen; Wegverfolgung; "kein Pfad".

## Stand debug-51
- Footprints: Game.cpp stampt je Instanz mit ODF is_starbase die Modell-Bbox (StampBox, r=0, Kosten 198, Bbox*Skalierung, nicht rotiert). Annahme: Original-Bbox = Render-Bbox (unverifiziert).
- Planetenradius: Modell-Kugel aus 0x5520f0 (aufgerufen von 0x5967f0): Mittelpunkt = Bbox-Mitte, Radius = max. Vertex-Abstand dazu. Engine nutzt alle Batch-Vertices (Original: Knoten-Vertices, max. 500 Punkte Puffer - ungeprueft).

## Stand debug-52 (Abgleich Objekt-Erzeugung)
- Belegt (Disassembly): 0x46b590 = Klassen-"Build": legt Objekt an ([vtbl+0x14]) und ruft IMMER dessen Init ([vtbl+0x40]) -> CraftClass-Init 0x469ef0 -> Basis-Init 0x46d5c0 (Footprint). Aufrufer: BZN/Savegame-Lader (0x46f550, Aufruf 0x46f6a2), BuildObject (0x435831), Bau/Produktion in 0x45xxxx (45a88c, 45cf13, 45dc51), 0x457bec. Karte laden und spaeter gebaute Stationen nehmen also denselben Weg -> beide stempeln den Footprint.
- Engine: SpawnAt stempelt jetzt Stationen (is_starbase) ebenfalls (Modell-Bbox, Kosten 198). Bau-Logik selbst gibt es in der Engine noch nicht.
- Beim Bau gibt es zusaetzlich einen Baustellen-Stempel (0x4510a0: Box des Zielobjekts, Radius [odf+0x23c]) und dessen Gegenstueck 0x451150 (ruft 0x409a80).
- Beim Entfernen einer Station (CraftClass-Destruktor 0x461e70, is_starbase) ruft das Original 0x409a80 mit derselben Box: ein "Unstamp"-Gegenstueck. Emulator-Test: nach Stempel + 0x409a80 (Argumente wie im Destruktor) bleibt der Umweg bestehen, 0x409a80 allein blockiert nichts. Semantik NICHT geklaert, nicht portiert (Engine entfernt Footprints beim Zerstoeren bisher nicht).
- Bbox/Vertex-Sammlung (0x5967f0) gegen Engine-Bbox weiterhin nicht gegengeprueft.

## Stand debug-53: Unstamp 0x409a80 geklaert
- 0x409a80 ruft dieselbe Stempel-Rekursion (0x40c700, Flag 1) wie der Stempel, aber mit Kosten-Zeiger = Planer-Objekt (+0 = Standardkosten 100). Die Ueberschreibregel hebt damit nur Zellen mit Kosten < 100 (z. B. Nebel 2) auf 100 an; 198 (Footprint) und 300 bleiben. Emulator-Probe: Box mit 2 + Unstamp = Weg wie ohne Stempel; mit 198/300 + Unstamp = Weg wie ohne Unstamp.
- Folge: Beim Zerstoeren einer Station bleibt ihr Footprint im Original stehen (Quirk). Engine: PathGrid::UnstampBox + Game::UnstampStation (Zerstoeren, Loeschen) bilden das exakt nach. Rechteck wie beim Stempel (Rand 0).

## Stand debug-54: Original-Bbox/Kugel (0x5967f0)
- Belegt: 0x5967f0 sammelt die Eckpunkte ALLER Mesh-Knoten (Slot 7 = 0x590760 fuer ST3D_Mesh und ST3D_Mesh_MRM; ST3D_Node und LODControl-Knoten tragen nichts bei) als einfachen push_back (kein Dedupe, keine 500er-Grenze), jeweils mit der Knotenmatrix (0x596b00). Keine Filterung nach LOD/oq/Borg/Schaden. Danach: Kugel 0x5520f0 (Bbox-Mitte, max. Abstand) und Bbox min/max.
- Engine: Game::ComputeOrigBounds baut das Modell mit hide_helper_nodes=false (alle Knoten) und liefert Bbox + Kugelradius. Footprints (map_bounds, SpawnModel.bounds) und Planeten-Stempel (map_orig_radius) nutzen das. map_radius (Auswahl/Trefferprobe/Felsen) bleibt unveraendert.
- Nicht beruecksichtigt: Sprite-Knoten (Slot 7 = 0x599000 liefert evtl. Quad-Ecken). Nicht per Emulator gegen ein echtes SOD verglichen (nur Code gelesen).

## Stand debug-56: Schiff fliegt durch Station (Ursache)
- Footprint 198 ist nur ein Kostenfaktor (Kante = Laenge * Zellkosten), kein Hindernis: Ein Pfad quer durch die Stationsbox kostet ~2x, wird also gewaehlt, wenn der Umweg laenger ist. Das ist wie im Original.
- Die Engine hat KEINE Kollisionsvermeidung. RTS_CFG OBSTACLE_AVOIDANCE=0 schaltet nur 0x42ef70 ab. Das Original hat zusaetzlich das System aus Collision.cfg (Config-Loader 0x4365c6, Globals 0x5f99d8..0x5f9a10, 0x63338c): Range-Search-Grid 256, Lateral-Repulsion (AN, Faktor 120, Gefahrenbreite 1.0), Full-Potential-Field (AN), Direct/Martin/RealCollisions (AUS), REPULSIVE_COEFFICIENT 1200, Cutoff 300, Influence 110, EXTRA_GOAL_ATTRACTION 5, SWITCH_TO_NEXT_POINT 20.
- Lateral-Funktion beginnt 0x435600 (Gefahrenbreite 0x5f99f4, Multiplikator 0x5f99f0 bei 0x43572c); Potentialfeld (FindPotentialField) und Aufrufer noch nicht gelesen. Nicht portiert.

## Nachtrag: Wegfolgen 0x42ded0 (gelesen, nicht emuliert)
- Wegpunkt weiter (Index +0xf4), wenn der QUADRIERTE DREIDIMENSIONALE Abstand zum Wegpunkt kleiner ist als `pathLeadDistance^2` (Klassenfeld +0x214, beim Laden quadriert,
  Wert 150 in allen sechs Physik-ODFs); die Zwischenpunkte liegen auf (Decke + Boden) / 2. Der Client rechnet jetzt ebenso (vorher 2D).
- Kein Weg (0x42e0c3): Rueckgabe von 0x40a310 ist 0 -> Wegobjekt (+0xf8) bleibt leer; ist der Zustand des Lenkobjekts (+0x20) nicht 0x19, wird 0x46eac0(2, 0, 0) aufgerufen
  und die Funktion endet, sonst springt sie nach 0x42e3e6. 0x46eac0(Modus, a, b) setzt einen neuen Befehl (Modus in +0x110, Argumente in +0x124/+0x121), gibt das alte Zielobjekt
  (+0x118) frei und legt ein neues an der EIGENEN Position an (+0x12c): Das Schiff bekommt also das Ziel "hier bleiben" und fliegt nicht los. Der Client verwirft in diesem Fall
  den Zielbefehl (ShipState::has_target = false). UNBEKANNT bleibt der Pfad fuer den Zustand 0x19 (springt nach 0x42e3e6).

