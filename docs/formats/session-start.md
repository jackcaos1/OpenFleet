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

## Starting units and settings (verified in 0x4408b0 (start), 0x442fd0 (starting units), 0x448d50 (team records))
- **Start function 0x4408b0** loads the rule set for the chosen tech level (key `techfile<N>` in `techlvl.odf`: 0 `tech1.tt`, 1 `nosuper.tt`, 2 `nosci.tt`,
  3 `nospweap.tt`, 4 `scionly.tt`, 5 `freetech.tt`, 6 `supersid.tt`; the seven lobby entries), and for the default-units mode 3 ("super side") it sets the race of each human player
  from the race of the map objects owned by that team (federation 0, borg 1, romulan 2, klingon 3), then creates the starting units (0x442fd0).
- **Starting units 0x442fd0:** only teams that have a player and own no object in the map get units. The unit list comes from the player's race ODF by the
  default-units setting (+0x80): 0 = map units (nothing is created), 1 = `minimalUnits1..9`, 2 = `standardUnits1..9`, 3 = `superUnits1..9` (comments in
  `fed.odf`: minimal = construction ship, standard = 2 construction ships + starbase, super side = one construction ship of every race). The default of the lobby
  is 1 (minimal).
- **Position:** the path `Camera_<team number>` of the map (first point); if the map has no such path, the centre of the map bounds (x and z) at the height (ceiling + floor) / 2.
  Each unit is tried on a spiral around that point: radius counter r = 1, 1 + 1/12, ... up to 40; the angle is 2 pi r, the offset is 120 r (x: sin, z: cos);
  the first position the object class accepts (virtual call, placement test) is used; if r passes 40 the centre is used. The test itself was not decoded.
- **Team records** (0x448d50): current dilithium = (lobby dilithium x player handicap) / 100 (+0x10), maximum dilithium = `cfgMaxDilithium` (+0x14),
  current crew = (lobby crew x handicap) / 100 (+0x20), maximum crew = `cfgMaxCrew` (+0x24), officers (+0x18 / +0x1c) = lobby officers. In mode 3 the `RTS_CFG.h`
  starting values (3500 dilithium, 4000 crew) replace the lobby values. Lobby defaults: dilithium 3000, crew 3500 (the host falls back to the first list entry
  when the configured value is not in the list), officers 100, tech level 1 (standard), default units 1 (minimal), handicap 100.

## Open (not verified)
- What happens to the start objects of slots nobody plays (a 4 player map with 2 players). The client keeps them as they are in the map.
- Whether the race chosen for a slot in the lobby replaces the map's start objects. The map header has one "race" entry (2duel: klingon, 4first: borg), the
  start objects carry fixed prefixes (4hub: team 1 federation, team 4 borg). The client keeps the map's objects; the HUD race follows slot 1.
- The placement test of the spiral (virtual call 0x443229, object class method); the client uses its own free-spot test.
- The local player number of the human in a skirmish (assumed to be slot 1 = `Camera_1`).


## Correction: default units setting (re-read 13.11.2026, Armada.exe 0x442f00, 0x442fd0, 0x4408b0, 0x53b5e8, 0x539bb4)
- The settings value `+0x80` is **not** the combo index. The lobby combo (entries `default_units_map`, `_minimal`, `_standard`, `_super`) maps to the value: map -> 3, minimal -> 0, standard -> 1, super -> 2
  (selection handler 0x53b5e8 .. 0x53b64b; display 0x539bb4 maps back). The constructor default 1 therefore means **Standard Units** (not minimal).
- The race object holds three unit lists: +0x214 `minimalUnits1..9`, +0x2a4 `standardUnits1..9`, +0x334 `superUnits1..9` (loader 0x4442b0). Value 0 uses the first, 1 the second, 2 the third, 3 (map units) the second for teams without objects.
- **Before** the start units are created, 0x442f00 deletes every craft whose owner team is not kept. Kept: team 0, teams above the map's player count, and (only for value 3, map units) the teams that have a player.
  So with minimal / standard / super the map's own start objects of teams 1..N are **deleted** (also those of occupied slots) and replaced by the race list of the lobby race (0x442fd0, spiral around `Camera_<team>`);
  with map units the occupied teams keep their map objects (their race decides, 0x440ac6) and unoccupied teams lose theirs. The earlier statement "teams that own objects get no units" only holds for map units.
