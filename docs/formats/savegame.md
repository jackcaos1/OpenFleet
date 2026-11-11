# Spielstaende (Original) – Stand der Untersuchung

Quellen: `Armada.exe` (unkomprimiert, mit `objdump -d -M intel` lesbar), Handbuch, `shell_cfg.h`, Hilfedatei `help/Help/Saved_Games.htm`.

## Ablauf im Original
- Strg+X oder Menue-Taste: Optionsmenue (Return 267, Save 300, Load 333, Restart 366, Options 399, Abort 432; x = 460).
- Save: Slot-Liste, freier Slot = neu, belegter Slot = ersetzen; Beschreibung frei, Vorgabe Missionstitel + Zeitstempel (`%b %d %I:%M:%S %p`).
- Load: Hauptmenue Single Player > Load oder Strg+X > Load. Instant Action (Skirmish) ist im Original nicht speicherbar (wir machen es trotzdem).
- Datei: `save\s<N>.sav` (Dateidialog `*.sav`), Quelle `Shell\LoadSaveGame.cpp`; daneben `.\savegame.bzn`.

## Dateiformat
Wie BZN (siehe bzn.md): Textkopf `version`, `saveGameDesc`, `binarySave`, danach Binaerstrom Typ(1)+3 Muellbytes+Groesse(u32)+Daten. Weitere Kopfschluessel in der exe: `msn_filename`, `missionSave`, `campaignNumber`, `missionNumber`, `runType`, `singlePlayer`, `TerrainName`, `AmbientRed/Green/Blue`, `BackgroundArt`, `start_time`, `techTreeFilename`, `raceName`.
Die 49 Karten sind `missionSave` und enthalten nur Startwerte (Schiff: 30 Elemente: Schild/maxHealth, Energie ...). Ein echter Spielstand enthaelt zusaetzlich Laufzeitfelder, z. B. Craft: curShieldDelay, curCrew, m_nameIndex, craftSystem, manheimState/Timer, SpecialForces, CannotDie, last_avoidance_force, impulseSpeed, warpSpeedMultiplier, desired_facing, attack_chain_ID, desired_attack_mode, m_shield_disabled_*, m_self_destruct_radius, m_blackHoleDeath/Center, transportDelay/Destination, isTransporting, m_slow_for_*, m_formation_move, queueStage, craftSpeedState, decommissionID, recycleTimer, m_dying, m_leftoverExplosion, je System `%sActive/HitPoints/AdjustedHitPercent/ModifiedRepairTime/RepairTimeLeft/DisableTimeLeft/PermanentDisable`.

## Methode (exakt statt geraten)
`tools/exe_fields.py asm.txt Armada.exe <adresse>` listet je Funktion die Feld-E/A-Aufrufe in Reihenfolge (Zeiger, Groesse, Feldname). Die Routinen dienen zum Laden und Speichern. E/A-Primitive (aus Beispiel Craft @0x462680): 0x4b46c0 = float (Typ 5), 0x4b44e0 = int32 (Typ 4), 0x4b4240 = bool (Typ 1), 0x4b4820 = Vektor (Typ 9), 0x4b40f0/0x4b41b0 = Text.
Craft-Laufzeitfelder: Funktion @0x462680.

## Stand der Entschluesselung (Fortsetzung)
- Werkzeuge: `tools/exe_fields.py` (Feldaufrufe einer Funktion), `tools/exe_vtables.py` (RTTI-Klassen/Vtables; Slot-Zuordnung noch unzuverlaessig, Vtables ueberlappen). Disassembly: `objdump -d -M intel Armada.exe > arm.asm` (685 000 Zeilen, 0,6 s).
- E/A-Primitive: 0x4b46c0 float, 0x4b44e0 int32, 0x4b4240 bool, 0x4b4820 Vektor (12 B), 0x4b40f0 / 0x4b41b0 Text (Groesse als Argument). Jeder Aufruf: (Datei, Zeiger, Groesse, Feldname). Dieselbe Routine laedt und speichert.
- Globale Schalter: `0x68b84c` = binarySave, `0x68b84d` = missionSave. Mit missionSave = 1 werden Laufzeitfelder uebersprungen (`jne` hinter `mov al,ds:0x68b84d`).
- Kopf/Speichern: Funktion 0x54d510 (version, saveGameDesc, binarySave, missionSave, campaignNumber, missionNumber, runType, singlePlayer, TerrainName(100), AmbientRed/Green/Blue, BackgroundArt, start_time, techTreeFilename, raceName); Aufrufer 0x54c9c0 (`.\savegame.bzn`) <- 0x472b60.
- Objekt-Anfang: GameObject::Load_Save @0x46f980: size, PrjID (ODF-Name, Typ 7), EntityID (Handle), pos, team, label, obj_addr (Typ 8), transform (Typ 11) – entspricht Elementen 0-6 jedes Objekts.
- Craft::Load_Save @0x462680 (aufgerufen von Wingman 0x4581a0, Starbase 0x45c0c0, 0x45d3d0 u. a.): zuerst curShieldDelay (float), curCrew (float), m_nameIndex (int) – in der Karte steht das als `5 0.0, 5 200.0 (=crewCost), 4 32` direkt nach der Matrix; danach (nur Spielstand) craftSystem x5, hullHitPercent, criticalHitPercent und die Laufzeitfelder.
- Offen: restliche Klassenkette je Objektart (Wingman/Starbase/Konstruktionsschiff/Turm/Umgebung), genaue Elementreihenfolge im Vollspeicher, Fusszeile/Teams/Missionszustand, Laden (gleiche Routinen).

## Werkzeugkette (Stand)
- `tools/exe_emu.py`: Pfadsuche durch Load/Save-Routinen. Bedingte Spruenge sind Entscheidungspunkte; mit missionSave=1 wird die Typfolge gegen ein echtes Karten-Objekt abgeglichen (DFS), die gefundenen Entscheidungen werden fuer missionSave=0 wiederverwendet. Klassen -> Funktion: Vtable-Slot 38 (`exe_vtables.py`): Craft/Wingman/HoverCraft/Scavenger/SensorArray 0x462680, GameObject 0x470180, Freighter 0x4581a0, Producer 0x45c0c0, Starbase 0x45f5f0, ConstructionRig 0x457af0, ConstructionObject 0x451210, MiningStation 0x45abb0, ResearchPod 0x45d3d0 ...
- Validierung: Wingman-Typfolge (23 Elemente nach der Matrix) wird von der Pfadsuche exakt getroffen.
- CraftSystem-Satz (Funktion 0x462b80): bool Active, int HitPoints, float AdjustedHitPercent, float ModifiedRepairTime, float RepairTimeLeft, float DisableTimeLeft, bool PermanentDisable.

## Wingman-Vollspeicher (missionSave=0), Feldfolge nach der GameObject-Matrix
    float  curShieldDelay
    float  curCrew
    int    m_nameIndex
      craftSystem x5 (Reihenfolge angenommen: shieldGenerator, engines, weapons, lifeSupport, sensors): bool Active, int HitPoints, float AdjustedHitPercent, float ModifiedRepairTime, float RepairTimeLeft, float DisableTimeLeft, bool PermanentDisable
    float  hullHitPercent
    float  criticalHitPercent
    bool   manheimState
    float  manheimTimer
    bool   SpecialForces
    bool   CannotDie
    vec3   last_avoidance_force
    float  impulseSpeed
    float  warpSpeedMultiplier
    float  desired_facing
    int    attack_chain_ID
    int    desired_attack_mode
    bool   m_shield_disabled_last_frame
    float  m_shield_disabled_last_value
    float  m_self_destruct_radius
    bool   m_blackHoleDeath
    vec3   m_blackHoleCenter
    float  transportDelay
    int    transportDestination
    bool   isTransporting
    bool   m_slow_for_formations
    bool   m_slow_for_combat
    bool   m_formation_move
    int    queueStage
    str    craftSpeedState
    int    decommissionID
    float  recycleTimer
    bool   m_dying
    float  m_leftoverExplosion
    vec3   pos
    euler  euler
    str    name
    bool   isSelected
    int    isVisible
    int    seen
    bool   Invincible
    float  healthRatio
    float  curHealth
    float  maxHealth
    float  specialEnergyRatio
    float  curSpecialEnergy
    float  maxSpecialEnergy
    str    ArgType
    str    what
    int    who
    ptr    where
    int    param
    str    ArgType
    str    what
    int    who
    ptr    where
    int    param
    ptr    undefptr
    int    perceivedTeam
    str    selectGroup
    bool   isHidden
    int    wasVisible
    bool   m_is_destroyed
    bool   m_force_onto_map
    int    invalidTargetFlags
    bool   reflectMissile
    bool   reflectBeam
    bool   avoidMe
    bool   disableRender
    int    WhoShotMe
    int    WhoIShot
    float  WhenIShot
    int    LastEnemy
    float  WhenLastHit
    int    FlagHandle
    int    Restrictions
    int    NumODFRestrictions
    str    odfRestrict
    int    NumHandleRestrictions
    int    handleRestrict
    str    raceString
## Objektklassen (Stand)
`tools/exe_classes.py` sucht fuer jede ODF der Karten (Klassenbezeichnung -> C++-Klasse -> Vtable-Slot 38) einen Pfad durch die Speicherroutine, dessen Typfolge exakt der des echten Karten-Objekts entspricht. Ergebnis in `savegame-classes.txt`: 123 von 140 ODFs (alle Schiffe, Stationen, Konstruktionsschiffe, Frachter, Werften, Tuerme, Sensoren, Planeten, Nebel u. a.) gefunden; offen: Forschungsstation (0x45dfb0) und Wurmloch-Objekt.
Zusatz-Primitive: 0x4b49f0 Matrix (Typ 11), 0x4b4b70 PrjID/ODF-Name (Typ 7), 0x4b4620/0x4b4580 int, 0x4b7420 Zeiger (Typ 8), 0x4b4c10 euler (float, float, 3 x vec3), 0x4b4de0/0x4b40c0 Abschnitt beginnt/endet (`[Teams]`, `[AI_Team]`, `[AiMission]` ...). Der Schalter missionSave wird auch ueber Register erkannt (`mov al,ds:0x68b84d` ... `test al,al`).
Noch nicht entschluesselt: Kopf-/Fuss-Routine 0x54d510 (ruft 0x4ed8a0 Objektliste?, 0x449190 [Teams], 0x41a470 [AI_Team], 0x432bc0 [Global AI], 0x4020d0 [AiMission], EmptyMission 0x434dd0 ...).

## Stand C++ (SaveFile / SaveSchema / SaveGame)
- `engine/src/formats/SaveFile.*`: Container lesen/schreiben, byte-identisch fuer alle 49 Original-Karten.
- `engine/src/formats/SaveSchema.*` (+ `SaveSchema.inc`, erzeugt mit `tools/gen_save_schema.py` + `tools/gen_save_schema_cpp.py`): Feldfolgen je ODF (140 ODFs), Zugriff per Feldname. Gegen alle 17.595 Kartenobjekte geprueft (Typfolge stimmt). Forschungsstationen: fuehrende Zeigerliste (4 oder 5 Plaetze je ODF). `mconstph` (PlaceholderFeature): PrjID + GameObject-Felder von Hand. Unbekannte ODFs: Feldfolge einer ODF mit gleichem classLabel.
- `engine/src/formats/SaveGame.*`: `BuildSave(Grundkarte, Spielzustand)` / `ParseSave`. Alle 49 Karten laufen Karte -> Spielstand -> Datei -> Lesen -> Werte ohne Abweichung (Testprogramm + `TestSaveGame`).
- Aufbau des Spielstands (Spielstandmodus): Kopf = msn_filename(16), missionSave=false, campaign/mission/runType, singlePlayer, 9 x 17 Elemente Spielerstatistik (0x52a570), Terrain, Grenzen (6 Float + Bool + 6 Float nur im Spielstand), Licht, Hintergrund, start_time, Technikbaum, Rasse, Objektzahl. Objekte in Spielstandfeldern (CraftSystem x5 vor hullHitPercent, Befehlsstrukturen, Restriktionslisten mit Zaehler 0 = keine Elemente). Fuss = 9 x 7 Teamelemente, teamRelation/allyShipAttacks/allyStationAttacks/timeOfLastAllyAttack (je 36 Byte), giveViewRef (324), EmptyMission-Block, Listen/Pfade aus der Karte, dann ListSize x2, EffectListSize, TextA/ShroudData, SpecialFlagNextHandle, NumScripts, NumGhosts, difficultySetting + 3 Felder, groupNum, groupList, objectivesFile.
- NICHT gegen einen echten Spielstand geprueft (es gibt keinen): Reihenfolge/Typen aus der Exe-Emulation (ein Pfad mit Zaehlern 0). Unsicher: AI-Abschnitte ([Global AI], [AI_Team], [AiMission]; der virtuelle Aufruf in 0x54d510), ShroudData-Groesse, die 6 Zusatz-Floats, die letzten Felder von 0x4388e0, Reihenfolge der Systeme (Schild, Antrieb, Waffen, Lebenserhaltung, Sensoren), Bedeutung von curHealth/maxHealth (Schild).
- Nicht gespeichert (noch): Bewegungs-/Angriffsbefehle, Alarmstufe, Geschosse/Effekte, laufende Transporte (die Befehlsstrukturen des Originals sind mit Nullen gefuellt), Wirtschaft (es gibt noch keine).
