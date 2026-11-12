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

- **Team records and start resources** (0x448d50 creates the nine team records of 0xbc bytes, 0x448b50 is the single-record variant). Per record:
  dilithium (+0x14, float), crew (+0x18 current, +0x1c), a third value (+0x24, float), the team number (+0x28) and a relation table (+0x2c, nine entries,
  the own entry set to 2 = ally). The configuration loader (0x4453f0) reads `cfgMax...` and `cfgStarting...` from RTS_CFG.h into the globals 0x633e2c..0x633e3c
  through 0x448c60. For a team that has a player, the values come from the lobby game settings (settings object fields +0x58, +0x5c, +0x60; the
  game mode in +0x80 decides: mode 3 uses the RTS_CFG.h globals 3500 / 4000 instead) and are scaled per player: `value * playerHandicap / 100`
  (handicap from settings +0x94 + 0x4c * slot, 100 if handicaps are off, byte +0x4e). The lobby option lists for these settings (arrays at
  0x68aa4c, 0x68a9f4, 0x68a9dc, five entries each) are read from the plain text file `label.map` (section `multiplayer_setup`; the reader 0x4740f0 opens
  `label.map` and looks for `<section> {` and the key). The option values are: starting dilithium `sd_value_1..5` = 3000, 4000, 5000, 7500, 10000;
  starting crew `sc_value_1..5` = 3500, 5000, 7500, 10000, 99999; officers `mo_value_1..5` = 100, 150, 200, 250, 999. Which entry is the default and
  which game mode a skirmish uses is not yet read. The `*.ldl` files (obfuscated) are not needed for the lobby; the game itself calls the string
  library with the file name `lang.txt` (0x43ff37), which does not exist in the installation, so the `.ldl` role is unclear.

## Open (not verified)
- What happens to the start objects of slots nobody plays (a 4 player map with 2 players). The client keeps them as they are in the map.
- Whether the race chosen for a slot in the lobby replaces the map's start objects. The map header has one "race" entry (2duel: klingon, 4first: borg), the
  start objects carry fixed prefixes (4hub: team 1 federation, team 4 borg). The client keeps the map's objects; the HUD race follows slot 1.
- How maps without any start object (4first, 4nirvana, 285 and 266 environment objects only) get their start units. The routine is not found yet; the
  client starts them empty and says so in a message.
- Which of the five lobby values is the default and which game mode value a skirmish uses.
- The local player number of the human in a skirmish (assumed to be slot 1 = `Camera_1`).
