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

## Other order types
GET_REPAIR (0x42a8c0, repair queue of a facility), RECYCLE (0x42a450, decommission), SET_RALLY / GO RALLY POINT, RECREW are analysed in `docs/formats/economy.md` and the GUI document
as far as they were read.

## Client
`Game.cpp`: `Spawned::order` (+ `ord_*` fields), `StepOrder`, `StartOrder`. Move, stop, attack and stand down cancel the order. Headless test:
`fleetclient_desktop --data <dir> --autostart 2duel --sim-seconds 120 --human-order scout|hunt|patrol|guard`.
