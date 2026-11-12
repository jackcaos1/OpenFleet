# Starting a skirmish session (original behaviour, partly verified)

Status per statement is given in brackets.

- [confirmed from the data] The `.mdf` file next to a map (`StartLocations = N`, `Start1 = x y` ...) holds points on a 128 x 128 raster, not world
  positions. Checked on all multiplayer maps: the points only roughly follow the camera points (for 4nirvana `Start1 = 15 14` against the camera
  point (845, 5620) on a 6400 map; the y axis is flipped). The map definition reader is 0x4c0a50 (writer 0x4c0c30); both only handle the lobby side.
- [confirmed from the data] The start objects are part of the map file: objects with team N (base, construction ship, sometimes fleets) belong to
  player N. The map header also carries the paths `Camera_1 .. Camera_N`; the first point of `Camera_N` lies next to the start base of player N
  (distance below 200 on 35 of 41 multiplayer maps, larger on 2borders, 3race, 4mama, finale6 where team numbers or camera points differ).
- [suspected] The start camera of the local player is the path `Camera_<player number>` (0x4aeaf0 builds the name "Camera_" + player number and looks the
  path up; whether this is the start jump or a hotkey was not traced).
- [unknown] Maps without start objects (4first, 4nirvana) and whether the lobby race of a slot replaces the race prefix of the map's start objects
  (the maps contain fixed race prefixes, e.g. 4hub: team 1 federation, team 4 borg). The client keeps the map's objects and, for slots without any start object,
  places a base and a construction ship at the camera point (estimate).
- [client] `SkStart`: keeps the adopted team objects of the map, sets their team to the slot's alliance, removes objects of empty slots, centres the camera on `Camera_1`.
  The earlier invented start package at the mdf points was wrong (it put units outside the map, z is mirrored in the client).
