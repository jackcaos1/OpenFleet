# Craft orders (CraftProcess)

Status per section: **verified** = read from the disassembly of the GOG `Armada.exe`, **assumed** = our choice, **unknown** = not read yet.

## Order types (verified)
The names of the 40 order types are stored in the exe as a pointer table (file offset 0x1f953c). The index is the order type:
0 NONE, 1 SELECT, 2 STOP, 3 GO, 4 NOMOVE, 5 ATTACK, 6 SPECIAL_ATTACK, 7 AI_ATTACK, 8 QUEUE, 9 FOLLOW, 10 FORMATION, 11 TRANSPORT, 12 SPECIAL_TRANSPORT,
13 NO_TRANSPORT, 14 RECREW, 15 SET_RALLY, 16 PICKUP, 17 DROPOFF, 18 NO_DROPOFF, 19 GET_REPAIR, 20 DEFEND, 21 CANCEL_BUILD, 22 RESCUE, 23 RECYCLE, 24 MINE,
25 HUNT, 26 GUARD, 27 SCOUT, 28 BUILD, 29 PATROL, 30 SEND, 31 GET_IN, 32 LAY_MINES, 33 RED_ALERT, 34 YELLOW_ALERT, 35 GREEN_ALERT, 36 STAND DOWN,
37 CANCEL BUILD QUEUE ITEM, 38 WORMHOLE, 39 GO RALLY POINT.

`CraftProcess::NewOrder` (0x426ad0) switches on the type (jump table 0x427f3c, index = type - 2). The per-frame dispatcher (0x427ff0) maps the type with the byte table
0x42813c and the jump table 0x4280f4 to a virtual slot of the process (vtable 0x5ca7e0):

| type | slot (handler) |
|---|---|
| GO, PATROL, GO RALLY POINT | 0x3c (0x428170) |
| FOLLOW | 0x40 (0x4288f0) |
| ATTACK | 0x48 (0x428e90) |
| SPECIAL_ATTACK | 0x4c (0x429030) |
| AI_ATTACK | 0x50 (0x4294e0) |
| QUEUE | 0x54 (0x42af80) |
| STOP | 0x58 (0x42a170) |
| RECYCLE | 0x5c (0x42a450) |
| BUILD | 0x60 (0x42b040) |
| CANCEL_BUILD | 0x64 (0x42b420) |
| GET_REPAIR | 0x68 (0x42a8c0) |
| MINE | 0x6c (0x42b430) |
| GUARD | 0x70 (0x428690) |
| SCOUT | 0x74 (0x42b870) |
| HUNT | 0x78 (0x42bb20) |
| WORMHOLE (and NONE) | inline in the dispatcher / issues STOP |

## Target choice, FindTarget (0x42ff80, verified)
Keeps the current target while it is inside weapon range - 25 (the constant is the double at 0x5ca6e0), otherwise takes the nearest enemy inside that range (0x4300a0).
Orders that call it every frame: GUARD, SCOUT, HUNT. **Consequence:** these orders only react to enemies that are already inside weapon range.

## Guard (verified, handler 0x428690)
- The protected object is stored in +0x7c. If it no longer exists the craft issues STOP.
- If the protected object was attacked within the last second (0x46e640 with 1.0) its attacker (+0x44c) becomes the target (if it can be attacked), otherwise FindTarget.
- Following: when the squared distance to the protected object is above 10000 (100 m) the craft flies to the point 100 m from the object on the line object -> craft.
- Verified but not implemented: when the protected object enters a wormhole the guard follows (order type 38, WORMHOLE).
- Unknown: which classes may guard whom (the original may restrict the target); the client allows any non-hostile object.

## Scout (verified, handler 0x42b870)
- Start (NewOrder): search radius +0x8c = 500 (0x5ca7a0), start position +0x90 = current position.
- Per frame: FindTarget; with a target the craft flies to an attack position, otherwise it explores. A destination counts as reached below 100 m (0x5ca79c squared).
- Stuck detection: position snapshot (+0x140) and its time (+0x13c); when the craft did not move more than 1 m for more than 6 s a new destination is chosen.
- New destination: the team's shroud map (object +0x178 of the team, 0x41fd80) returns an unexplored point within the search radius around the start position and
  the reached point is marked (0x4220b0). When there is none the radius grows by 500 until it exceeds the map diagonal (x extent x 1.414, 0x5ca7a4), then it
  resets to 100 and the explored map is cleared (0x421900, human teams only).
- **Assumed:** the shroud is not implemented as fog of war in the client yet. `game/Shroud.h` is a simple per-team grid (cell 100 m); ships with a scout or hunt order reveal
  their sensor radius around themselves. The original reveal shape and cell size are unknown. The attack position with a target is the same stand-off point as for attack.

## Search and destroy = HUNT (verified, handler 0x42bb20)
- FindTarget; with a target: fly towards it while the distance is above weapon range - 50 (0x5ca798), otherwise hold position and fire (0x42c400).
- Without a target: reveal the map around the craft (0x421af0), then pick a random point in the whole map (margin 100, 100 attempts until a walkable path grid cell, 0x409db0);
  when it is reached (closer than `tooCloseToTurn`) or the craft is stuck (6 s) pick the next one.
- Mapping of the button: the GUI command "search and destroy" is assumed to issue HUNT (the AI script function `SearchAndDestroy` issues HUNT, 0x436b38).

## Patrol (unknown)
The PATROL order type shares the GO handler (slot 0x3c); the way the patrol path is stored was **not read**. The client implements an **assumption**: the craft flies
between its start position and the tapped point, engaging enemies inside weapon range - 25 on the way.

## Recrew and rally point (verified, build 108)
Both belong to the yard class (shipyard and starbase, ODF keys read by 0x4561c0).
- **Recrew** (NewOrder handler 0x426c1d): toggles the yard flag `+0x7ac`. Per frame (0x45b220) while the flag is on: when the team's crew pool is at least 1, `amount = min(dt x recrewRate, pool)`,
  the station adds it to its own crew (0x467570 returns the part above its maximum), the pool loses only the accepted part and the flag is cleared as soon as something overflowed.
  `recrewRate` is an ODF key of the yard class, default 10 crew per second; no shipped ODF sets it. The button (location 7, 0x45c841) is enabled when the pool is above 0 and the station crew is below its maximum, shown pressed while the flag is on.
- **Set rally point** (handler 0x426ebe): copies the order's last point into the yard's `+0x7d8` (three floats). Default made when the yard is created (0x45e540): position + front axis x 3 x radius (constant 0x5cb170 = 3.0).
  (A second routine, 0x45e9f0, computes 2 x radius instead; it is called from a different virtual slot and was not traced, the client uses 3 x radius.)
- A craft leaving a yard always gets order 39 with the point `+0x7d8` (0x557400, phase 2); order 2 only when the yard no longer exists.
- Note: with the order name table as printed above, the jump entries for 15 and 16 do the recrew toggle and the rally copy, i.e. the table of names seems shifted by one against the NewOrder jump table here; the handlers are
  identified by what they do, not by the name.
- Client: `Spawned::recrew`, `StepProduction`; `Spawned::rally_set/rally`, `RallyOf`, `StepExits`, tap handling `rally_armed`. Headless tests: `--human-order recrew|rally`.

## Repair (partly read, NOT implemented)
- Ships are repaired by yard-class objects with the ODF key `repairFacility` (default 1; only `zmama` sets 0) through a `RepairQueueManager` (RTTI, vtable 0x5cca3c) that shares the spline docking code (0x558240) with the mining queue.
  Other keys of the same loader: `repairHardpoint`, `dockingBuffer` (default 90), `repairSide` (default 2, `byard2` uses 0), `buildAnimation`.
- The queue accepts a craft only while its order type is 0x13 (0x5577f0) and it belongs to the same owner. The station state 0x10 (animation) is started when a craft reaches the head (0x45eaa0, same as for freighters).
- Systems repair themselves everywhere in the original (health step 0x4683d0, see `ship-health.md`); what the docked repair adds (rate, hull) was **not found**. The handler 0x42a8c0 scans the craft's weapon list for a usable weapon and target
  (0x463b70 / 0x463c10) and is not a plain "fly to the dock" routine as the name suggests.

## Other order types
GET_REPAIR (0x42a8c0, repair queue of a facility), RECYCLE (0x42a450, decommission), SET_RALLY / GO RALLY POINT, RECREW are analysed in `docs/formats/economy.md` and the GUI document
as far as they were read.

## Client
`Game.cpp`: `Spawned::order` (+ `ord_*` fields), `StepOrder`, `StartOrder`. Move, stop, attack and stand down cancel the order. Headless test:
`fleetclient_desktop --data <dir> --autostart 2duel --sim-seconds 120 --human-order scout|hunt|patrol|guard`.
