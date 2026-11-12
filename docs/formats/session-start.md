# Starting a skirmish session: what is verified

Verified in the executable (GOG Armada.exe, addresses given) or in the data files; everything else is listed as open.

## Verified
- **Start camera.** The game view setup (0x4aaae0, the same routine that loads the grid effect sprite) builds the name `"Camera_" + local player number`
  (player number from 0x46d060 -> 0x4c97b0 -> 0x4bf1a0), passes it with point index 0 to `GameCamera` (0x4791e0 -> 0x479250), which looks the path up
  (0x4348d0) and sets the camera to x and z of that point at the height `(ceiling + floor) / 2` (SHIP_COMBAT_CEILING 67, FLOOR 10 = 38.5).
  The map files carry the paths `Camera_1 .. Camera_N`; the first point of `Camera_N` lies next to the start base of player N (checked on all multiplayer maps:
  distance below 200 on 35 of 41, larger on 2borders, 3race, 4mama and finale6).
  (The routine at 0x4aeaf0 that also builds `Camera_<n>` writes camera paths in a developer mode and is not the start.)
- **Start objects are in the map.** On the maps with start objects (2duel, 4hub, ...) base and construction ship per player are normal map objects with
  team N = player N. The mission class used by these maps (`EmptyMission`, vtable 0x5caaa4) contains no code that creates objects; the mission script
  functions that can create objects (`BuildObject`, `ReplaceObject`, `SetRace`) have no caller inside the executable, only the script DLLs use them.
- **The `.mdf` start points are not world positions.** They are points on a 128 x 128 raster (the preview bitmap); the reader is 0x4c0a50 and is used by
  the lobby map list. Compared with the camera paths they only agree roughly (and the y axis is flipped). The client used them as world coordinates
  (x 50 + 25, z not mirrored): that was the error that put the start units and the camera outside the map.
- **Object race.** `SetRace` (0x439a20) writes the race of one *object* (field +0x1a4, looked up by name by 0x4440b0); the default comes from the object class
  (ODF). The map's object names carry the race prefix (`kbase`, `fbase`).

## Open (not verified)
- What happens to the start objects of slots nobody plays (a 4 player map with 2 players). The client keeps them as they are in the map.
- Whether the race chosen for a slot in the lobby replaces the map's start objects. The map header has one "race" entry (2duel: klingon, 4first: borg), the
  start objects carry fixed prefixes (4hub: team 1 federation, team 4 borg). The client keeps the map's objects; the HUD race follows slot 1.
- How maps without any start object (4first, 4nirvana, 285 and 266 environment objects only) get their start units. The routine is not found yet; the
  client starts them empty and says so in a message.
- The resource start values (`cfgStartingDilithium`, `cfgStartingCrew`): read into a configuration object (0x448c60), the use at session start is not traced.
- The local player number of the human in a skirmish (assumed to be slot 1 = `Camera_1`).
