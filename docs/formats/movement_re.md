# Originale Bewegungslogik (Armada.exe) – Zwischenstand RE

Status: in Arbeit, ShipSim.cpp noch NICHT ersetzt.

## HoverCraft-Simulationsfunktion @0x459440 (vtable-Slot 33), Argument dt (float)
Objektfelder (this=esi): +0x140 Front-Achse(3f), +0x14c Geschwindigkeit(3f), +0x158 Winkelgeschw./Drehmoment(3f),
+0x164 Beschleunigung(3f), +0x68/+0x74/+0x80 Matrix-Zeilen, +0x494 Zeiger auf Servo-/Steuerblock
(+0x0 yaw, +0x4 pitch/Schub-Eingabe, +0x8 roll, +0xc Schub), +0x518 Flag "angetrieben", +0x4dc Flag "gestoppt",
+0x67c Flag "tot/inaktiv" (Funktion bricht ab), +0x784 Substep-dt, +0x788 Substep-Anzahl.
Ablauf (bisher gelesen bis 0x459d40):
1. Substeps: dt wird in n Teile zerlegt (n = ceil(dt / maxStep), Teilschritt-dt in +0x784).
2. Je Substep: Beschleunigung aus Steuereingaben + ODF-Koeffizienten (+0x6a8.. = battphys: Koeffizienten
   Kopplung Geschwindigkeit/Quer-Schlupf, +0x6b4/+0x6c8 Vor-/Rück-Limits, +0x6f0/+0x6fc Max-Schub, +0x718/+0x710/+0x708 Seitenkräfte).
   Quer-Schlupfanteil: v_quer = v - front*(v·front); Skalar = 1 - 1/(|v_quer|*k1+k2).
3. Nicht-Hover-Zweig (0x459a99): Höhenregelung mit Clamp ±0x5cbfd0 (Servo Höhe/Pitch-Lift).
4. Danach: v += a*dt (0x459d04 ff.), weiter mit Winkelanteil/Integration (noch zu lesen).
Weitere Kandidaten: 0x465100, 0x4664e0, 0x465e80, 0x462120 (Slots 34/31/37/36).

## Korrektur / Präzisierung (Simulate @0x459440, nach Volllesung)
Objektfelder (this): Matrix-Zeilen +0x68 (right), +0x74 (up), +0x80 (front), Position +0x8c.
+0x140 = Geschwindigkeitsvektor V (Welt), +0x138 = |V|, +0x14c = Winkelgeschwindigkeit W (x Pitch, y Yaw, z Roll),
+0x158 = lineare Beschleunigung A, +0x164 = Winkelbeschleunigung AA, +0x768/+0x76c/+0x770 = Euler pitch/yaw/roll.
+0x494 = Zeiger auf Steuerblock (5 Floats): c0 Lenken(Roll), c1 Pitch, c2 Hoch/Runter, c3 Schub.
Klasse (ODF, battphys) wird in 0x455440 abgeleitet; s = impulseSpeed:
 velocWhenCoeffHalf*=s, pitchRot*=s, rollRot*=s, frontBackInertia/=s, maxFwd=s, maxRev*=s, upDownInertia/=s,
 maxVertical*=s, leftRightInertia/=s; dann (Klassenfelder, Offsets hex):
 45c=1/(1-velocCoeffStill); 458=(2-45c)/velocWhenCoeffHalf... (siehe Disassembly 0x455882..0x4559cd)
Instanz-Kopie in 0x458750 (class->inst): 4b4<-200,4b8<-204, 6a8<-458, 6ac<-45c, 774=1-1/45c, 6b4<-464, 6b8<-468,
 6c0<-470, 6c8<-478, 6cc<-47c, 6d4<-484, 6dc<-48c, 6f0<-4a0, 6fc<-4ac, 700<-4b0, 708<-4b8, 710<-4c0, 718<-4c8, 724..764<-4d4..514.
Substeps: maxStep=max(min(0.1/|W|, 4/|V|), 0.05); n=ceil(dt/maxStep) wenn dt>maxStep.
Leapfrog: W+=dt/2*AA; (borg: ...) euler+=dt*W; V+=dt/2*A; pos+=dt*V; matrix aus (pitch,yaw,roll); V+=dt/2*A; speed=|V|.
Pitch-Clamp +-1.5, Roll-Wrap +-pi im Substep, Yaw-Wrap +-pi nach der Schleife.
Konstanten: 0.001, 0.01, 0.05, 0.1, 4.0, 1.5, 120, pi.
Steuer-Setter: 0x465960 (Yaw: c0=clamp(atan2(Vx,Vz)-ziel,-1,1), 0 wenn |Vh|^2<1e-4), 0x4659f0 (Pitch).
AI-Prozesse (Goto/Move/...): 0x428000-0x42fb00 nutzen tooCloseToTurn(+0x208), tooSlowToTurn(+0x210), pathLeadDistance(+0x214), tooCloseForWarp(+0x20c), headingCloseEnoughForWarp(+0x218).

## Umsetzung
`engine/src/game/HoverSim.{h,cpp}`: getreue Portierung (Klassen-Ableitung, Instanz, Simulate-Substeps, Matrix<->Euler),
Test `TestHoverSim`. Matrix-Konvention: front=(cp*sy,-sp,cp*cy), right.y=cp*sr; pitch=-asin(front.y), yaw=atan2(fx,fz), roll=atan2(right.y,up.y).
Noch offen: Borg-Zweig (0x459a99), Craft::Simulate 0x465cc0 (Kollision/Position), Steuerlogik (Goto/Pfad @0x428000-0x42fb00,
Setter 0x465960/0x4659f0/...), Hindernisvermeidung, Warp-Umschaltung (SetSpeedCap), Einbindung in Game.cpp (ersetzt StepShip).

## Steuerschicht (gelesen, in HoverSim.cpp portiert)
Craft-vtable (Hover): 0xdc SetThrottle(accel,warp) @0x458990, 0xe0 GotoPD @0x4657b0, 0xe4 SetVertical @0x465870, 0xe8 ServoHeightPitch @0x458aa0
(shp_*), 0xf0 ServoYaw @0x458b80 (sy_*), 0xf4 ServoPitch @0x458ca0 (sp_*), 0xf8 ServoPitchLift @0x458d60 (spl_*), 0xb0 = turnRadius.
Zielfahrt (Process @0x42e4a0, Nicht-Borg): dist<=tooCloseToTurn -> Steuer 0 + PD-Schub; sonst Wendekreistest (Kreise +-R seitlich, Ziel
im Kreis -> geradeaus/PD); sonst dir=(Ziel-Pos)/max(dist,20) (Hoehengrenzen 0/80), Warp nur wenn |Kurs-Fehler|<headingCloseEnoughForWarp
und dist^2>=tooCloseForWarp^2 (Schubfaktor = warp/impulse), AimDirection: c3=min(|dir|,1)*Faktor, Pitch-/Yaw-Servo auf asin(dy/len)/atan2(dx,dz);
liegt das Ziel innerhalb 45 Grad voraus, ueberschreibt der PD-Regler den Schub. Hindernisvermeidung @0x42ef70 (Raumabfrage, Summe ueber Objekte
@0x424860..0x425200) noch NICHT portiert; ebenso Pfadfolgen (pathLeadDistance, Funktionen 0x42ded0/0x42e0xx) und Borg-Zweig.

## Abgleich mit dem Original (Emulation)
`tools/hover_check.py` (Unicorn) fuehrt Armada.exe-Funktionen real aus und vergleicht mit HoverSim.cpp:
- HoverSimulate (0x459440): 80 Schritte inkl. Multi-Substep, max. rel. Abweichung 3.9e-7 (Fix: zweiter W-Halbschritt @0x459df1).
- Servos/Steuerung (0x458b80 Yaw, 0x458ca0 Pitch, 0x458d60 PitchLift, 0x458aa0 HeightPitch, 0x4657b0 GotoPD+0x458990 SetThrottle): 30 Zufallsfaelle, max. rel. Abweichung 3.6e-7.
- Emulations-Trick: CRT-Thunk 0x5bbb9a (asin) wird durch x87-Code ersetzt.
- Hindernisvermeidung (HoverAvoid; Helfer 0x424860 ClosestApproach, 0x424ba0 Rosette, 0x4250e0 Gefahr, 0x424b50 Vorrichtung, 0x425200 Wahl): `tools/avoid_check.py`, 60 Zufallsszenen, max. rel. Abweichung 3.6e-8. Die Schleife/Filter aus 0x42ef70 sind im Skript nur nachgebildet (gelesen, nicht emuliert).
Offen: Pfadfolgen, Borg-Zweig, 0x8000-Objekte (0x42ed20), Regionsabfrage 0x40e990 (wir pruefen alle Hindernisse), dynamic_cast-Typ @0x5f8480.

## Hindernisvermeidung (Original @0x42ef70, aufgerufen aus 0x42ebb0 = Richtungsberechnung der Zielfahrt)
- Lookahead R = speed*4 s; Objekte mit Abstand-Radien <= R; nur wenn kleinste Annaeherung 0 < t <= 4 s (mit Wunschgeschwindigkeit dir*speed).
- 10 Kandidaten: 0 Wunschrichtung, 1/2 +-15 Grad, 3/4 +-30, 5/6 +-60 (nur Kurs), 7 Vorausrichtung, 8/9 +-15 Grad Neigung. Start-Score 100*(dir.d).
- Gefahr je Objekt: s=(r1+r2)*1.1; wenn s^2 > miss^2: score += -100*c*s^2/miss^2 (c=2 bei ruhendem Objekt, sonst 1). Objekte mit class+0x235 sperren 8/9 (-1e5).
- Bisherige Richtung (+0x49c) bekommt +50*dot. Wahl = hoechster Score; ist er < 0, nur 75 % Schub. Ergebnis -> +0x49c. Hoehengrenzen 0..80 danach.
- Niedriger priorisierte Objekte (class+0x1ac) zaehlen nur, wenn speed <= 1. Bewegte Objekte mit Elternobjekt: Annaeherung gegen deren Kurs Richtung Elternobjekt.

## Pfadfolgen / Pfadfinder (nur gelesen, nicht portiert)
- Wegpunktliste (Befehl) -> 0x42ded0: Index [this+0xf4]; Wegpunkt gilt als erreicht, wenn Abstand^2 < pathLeadDistance^2 (Klasse +0x214, ODF pathLeadDistance=150, gespeichert quadriert). Letzter Wegpunkt: gleiche Pruefung (bei Modus 2).
- Ist kein Pfadobjekt ([this+0xf8]) da, ruft es den Pfadfinder 0x40a310 (Objekt @0x6322b8, 10 Argumente: Start, Ziel, Radius, Flags ...). Der Pfad wird danach mit 0x434a50 an einem Punkt "Laufweite+10 m" voraus abgetastet (Spline/Polygonzug), das Ergebnis ist das aktuelle Lenkziel ([this+0xc4]).
- Pfadfinder = mehrstufiges Quadtree-Raster (Zellen 28 Byte, Flag bit0 = gemischt/blockiert, rekursiv 2x2 in 0x40d280) mit Regionsnummern je Zelle (Zelle+4); Start/Ziel in derselben Region -> direkter Weg (0x40a8a4), sonst Regionsgraph (0x408500) + Verfeinerung (0x40ac80, 0x40b8c0). Roh ~100 Funktionen (0x406000-0x411000); Raster wird aus den Kartenobjekten (0x40e990 Raster 0x65a308) gebaut.
- Folge: ohne blockierende Gelaende-/Kartenobjekte fliegt das Schiff im Original geradeaus zum Wegpunkt; Umwege entstehen nur um solche Hindernisse.
