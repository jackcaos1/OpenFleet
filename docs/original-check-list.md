# Items to check in the original game (running installation)

Things that could not be settled from the executable or the data files. Add a result next to each when checked.

## Game setup / instant action
- [ ] The eight small plates below the player rows (code: buttons at x 16, 45, 74 ... step 29, y 209, 27 x 16; in the background bitmap they are further right, x about 47).
  Probably only a design element; check whether they react to clicks and what they do.
- [ ] Visibility field (`show_all`, `show_available`, `show_allies`, `show_none`): default selection and which entries appear (the list is rebuilt depending on setting +0x70).
- [ ] Which start units appear with the default setting ("Minimal Units") on maps without start objects (4first, 4nirvana): number, type, position relative to the camera point.
- [ ] Whether the race chosen for a slot replaces the race of the map's start objects (maps with fixed race prefixes, e.g. 4hub).
- [ ] What happens to start objects of slots nobody plays (a 4 player map with 2 players).
- [ ] Are the ready lamps lit for the local player / computer players in an instant action game?
- [ ] Default of the "Minimap Hidden / Shown" toggle and of "Random / Fixed Placement" (derived from the code: hidden, random).
- [ ] Map selection popup: position (derived: 120, 91), row layout, scroll step of the arrows.

## In the game
- [ ] Grid colour and brightness (client: dark olive 0.149 / 0.128 / 0, taken from an unclear structure at 0x690ac0).
- [ ] Whether the grid is drawn behind or in front of ships (depth test).
- [ ] Ring of the shockwave on destruction: size for stations and large ships (client radius = half the bounding box diagonal, minimum 40).
- [ ] Destroyed ships: tumbling or not (client: none, the ship keeps its previous motion).
- [x] Selection box: the rectangle is a debug display (switch 0x663c9c), not in the normal game (from the code).
- [ ] Do names of selected ships appear above them (global 0x60c4b0 = 1 by default)? Where, and for which ships (own, all)?
- [ ] Position and size of the shield / energy bars and crew dots over a selected ship; group number display.
- [ ] Target marker after a move order: a rotating square shrinking from size 50 to 2 (about 17 frames, duration depends on the frame rate); check shape and apparent duration.
- [ ] Info panel wireframe: which part of the wireframe belongs to which system (client assumes part i = system i: shield, engines, life support, sensors, weapons) and how damage looks (white / yellow / red).

## Economy (night branch night/economy-ai)
- Build menu slots: the client puts the build items in the first slots of the command panel (Cancel 11, Back 12). Check the real layout and whether the queue is shown in the rail.
- Placement of ships built at a yard (client: first free spot of the start spiral around the yard) and the exit animation.
- Construction ship: does the original start paying at the site (0x457b80) and show a scaffold; how long the ship stays; whether placement is checked.
- Freighter loop speed and cargo handling (client model, see docs/formats/economy.md).
- Resource display: dilithium / crew / officers values and when they change (crew income of starbases 3.5 per second each).

## Build 95 (user test feedback)
- Portrait layout: the rail (special weapons, build queue) now sits directly above the info panel, the control panel above it (user request). Landscape unchanged.
- Build placement: client rule = footprint (bounding box half diagonal + `footprintBuffer`, 80 for stations, from station.odf: "to block building") against stations and other sites, plus keep-out zones around moons, planets, nebulae (effectRadius), wormholes, black holes and asteroid fields. Check in the original: what blocks placement, preview colour, and whether a second tap confirms (own touch design).
- Construction animation: client invention (growing model, six orbiting worker bees from `workerBeeName`, beams). Original classes: `BuildingEffect`, `WorkerBee`, `mconstph.odf` placeholder, `weldingRadius`, `timeUntilClosure`. Compare look and duration; yards show nothing yet.
- Dilithium moon glow: nodes `oreglow` / `dmooninf` (lights.spr, animated colour) were hidden inside the sphere by the depth test; client lifts them 60 units towards the camera. Check how the original shows the glow.
- Moon info on tap (name, supply) is client design; the original may select moons like other objects.

## Results of the user's test in the original (13.11.2026)
- Eight small plates under the player rows = **team colour chooser** (not implemented in the client yet).
- Visibility field: default **show all**, the list only has show all and show none (client changed, build 96).
- Instant action, Borg, minimal units on 4first: only one Assembler (= `bconst`, matches `minimalUnits1`). Verified.
- "Map units": the map decides the race by position (start objects keep their race). Matches the client.
- Start objects of unoccupied slots are **not created** (client changed, build 96).
- Default "Minimap Hidden": confirmed. Popup placement fine; the popup height could follow the screen size (open).
- Grid: **much brighter** in the original (orange-yellow, screenshot) and depth tested: in front of objects behind it, behind objects in front of it (client changed, build 96).
- Ship names appear only in the info panel and tooltips (client has no names in the field: matches).
- Ready lamps: still open.
- Extra observation: single stars between the play plane and the background give depth when scrolling (added, build 96).
- Info panel (gui_*.cfg): `infoSingleConstructionBar` at the energy bar position (38, 86, 48 x 5, colour 1 / 0.6 / 0.2, sprite `large_construction_bar`) and `infoSingleDilithiumBar` (37, 86, 49 x 5, colour 0 / 0.5 / 1, `large_dilithium_bar`): used for construction progress and dilithium moons. Moon wireframe sprites `mdmoonw1..5`, `mmooninfw1..5` exist in gui_global.spr. Build 96 uses them. What else the original moon panel shows is open.

## Build 97
- Placement display now follows the original (Armada.exe 0x516170 / 0x5162a0): sprites `footprint`, `valid_build` (green), `invalid_build` (red), `planet_build` (blue: starbase within 300 of a planet's edge, Global 0x6000d0) from gui_map.spr / texture MBuild; occupied cells of the 50 grid (STATION_PLACEMENT_GRID_SIZE) get the footprint tile, the object's footprint rectangle the status sprite. Not read: how the original computes the footprint rectangle (client: bounding box + footprintBuffer) and which cells it marks as occupied.
- Start units and race on maps with start objects: superseded by the correction in docs/formats/session-start.md (map objects of player teams are deleted unless "Map Units"; default setting is Standard Units). Please verify in the original: 2duel with Federation + Minimal gives a Federation construction ship only, and the default combo shows "Standard Units".

## Build 99
- Mining station delivers a free freighter (verified in the code, see economy.md). Please check in the original: freighter appears at the station right when it is finished, its crew and the station's crew values, whether the AI then builds fewer freighters.
