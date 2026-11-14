# Craft orders (CraftProcess)

Status per section: **verified** = read from the disassembly of the GOG `Armada.exe`, **assumed** = our choice, **unknown** = not read yet.

## Order types (verified)
The names of the 40 order types are stored in the exe as a pointer table (file offset 0x1f953c). The index is the order type:
0 NONE, 1 SELECT, 2 STOP, 3 GO, 4 NOMOVE, 5 ATTACK, 6 SPECIAL_ATTACK, 7 AI_ATTACK, 8 QUEUE, 9 FOLLOW, 10 FORMATION, 11 TRANSPORT, 12 SPECIAL_TRANSPORT,
13 NO_TRANSPORT, 14 RECREW, 15 SET_RALLY, 16 PICKUP, 17 DROPOFF, 18 NO_DROPOFF, 19 GET_REPAIR, 20 DEFEND, 21 CANCEL_BUILD, 22 RESCUE, 23 RECYCLE, 24 MINE,
25 HUNT, 26 GUARD, 27 SCOUT, 28 BUILD, 29 PATROL, 30 SEND, 31 GET_IN, 32 LAY_MINES, 33 RED_ALERT, 34 YELLOW_ALERT, 35 GREEN_ALERT, 36 STAND DOWN,
37 CANCEL BUILD QUEUE ITEM, 38 WORMHOLE, 39 GO RALLY POINT.

`CraftProcess::NewOrder` (0x426ad0) switches on the type (jump table 0x427f3c, index = type - 1). The per-frame dispatcher (0x427ff0) maps the type with the byte table
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
- Verified (NewOrder case 26, 0x4273e3): no class restriction on the target, the order only needs the craft not to be in a queue transition (0x4268d0).
- Verified but not implemented: when the protected object enters a wormhole the guard follows (order type 38, WORMHOLE).


## Scout (verified, handler 0x42b870; rewritten in build 110)
- Start (NewOrder): search radius `+0x8c` = 500 (0x5ca7a0), start position `+0x90` = current position.
- **The map** is the AI "exploration map" of the team: cells of **400** (`exploration_map_resolution` in `AI/strategic_AI_config.txt`), origin at the map's minimum x and maximum z. Every craft marks the cell under itself each frame
  (0x422000, called from the process update 0x427ff0); nothing is revealed by sensor radius.
- Per frame: FindTarget; with a target the craft flies to an attack position, otherwise it explores. Arrived = squared distance to the goal below 100 m (0x5ca79c, 3D). Stuck = the position snapshot (`+0x140`) is renewed when the craft moved
  more than 1 m (squared distance > 1.0, 0x5ca7d8) together with the time stamp (`+0x13c`, the game time truncated to whole seconds); stuck means more than 6 whole seconds (0x5ca7cc) without such a move.
- When arrived or stuck: a stuck craft first marks its goal cell (0x4220b0); then 0x41fd80 picks the unexplored, walkable (0x409f70) cell whose centre is within the radius of the start position and **closest to the craft** (rows then columns,
  strictly smaller distance wins, no randomness). If there is none the radius grows by 500 while x extent x 1.414 (0x5ca7a4) is larger than it; otherwise a non-AI team (0x472100) clears the map (0x421900) and the radius becomes 100.
- Client: `Shroud` (cell 400), `MarkCell`, `FindNearestUnseen`, `Spawned::ord_stamp`.

## Search and destroy = HUNT (verified, handler 0x42bb20)
- FindTarget; with a target: fly towards it while the distance is above weapon range - 50 (0x5ca798), otherwise hold position and fire (0x42c400).
- Without a target: reveal the map around the craft (0x421af0), then pick a random point in the whole map (margin 100, 100 attempts until a walkable path grid cell, 0x409db0);
  when it is reached (closer than `tooCloseToTurn`) or the craft is stuck (6 s) pick the next one.
- **Verified mapping:** GUI command 0xe issues order 25 (HUNT) and 0xd issues 27 (SCOUT) (craft command switch 0x463410, table 0x463698); the AI script function `SearchAndDestroy` also issues HUNT (0x436b38).

## Patrol (read, build 109)
NewOrder jump table index is `type - 1` (the table has 39 entries; type 3 GO = 0x426f5c, which sets order 3). PATROL (29) shares the GO handler 0x428170 and the waypoint follower 0x42ded0.
The order carries a waypoint list (count, points, mode); mode 2 marks a patrol. The follower advances to the next point when the craft is within the class radius `+0x214` of the current one;
at the last point of a mode 2 list the index is reset to 0, i.e. **the craft cycles through its points forever**; other lists end with STOP on arrival (0x428411). FindTarget only chooses the fire target (`+0x74`), the craft keeps flying; the extra engage routine (0x429b00, enemies within class range `+0x3f4`, near the waypoint within 750 m) runs for computer teams only (0x472100). The waypoint radius is `pathLeadDistance` (class `+0x214`, 150). The client uses the two points start and tapped point (a cycle of two), which is the same behaviour for one tapped point.
Open: whether the original GUI lets the player add more points (shift-click), not checked.

## Recrew and rally point (verified, build 108)
Both belong to the yard class (shipyard and starbase, ODF keys read by 0x4561c0).
- **Recrew** (type 15, NewOrder handler 0x426c1d): toggles the yard flag `+0x7ac`. Per frame (0x45b220) while the flag is on: when the team's crew pool is at least 1, `amount = min(dt x recrewRate, pool)`,
  the station adds it to its own crew (0x467570 returns the part above its maximum), the pool loses only the accepted part and the flag is cleared as soon as something overflowed.
  `recrewRate` is an ODF key of the yard class, default 10 crew per second; no shipped ODF sets it. The button (location 7, 0x45c841) is enabled when the pool is above 0 and the station crew is below its maximum, shown pressed while the flag is on.
- **Set rally point** (type 16, handler 0x426ebe): copies the order's last point into the yard's `+0x7d8` (three floats). Default made when the yard is created (0x45e540): position + front axis x 3 x radius (constant 0x5cb170 = 3.0).
  The Shipyard virtual slot 47 (0x45e9f0, called from the owner assignment 0x46dfc0 right after creation) then replaces it with position + front x **2** x radius, so 2 is the effective default for shipyards (build 110); the Starbase slot 47 is a different routine (0x45f4b0, officer bookkeeping), so starbases keep 3.
- A craft leaving a yard always gets order 39 with the point `+0x7d8` (0x557400, phase 2); order 2 only when the yard no longer exists.
- The jump table index is `type - 1` (see Patrol), so the name table is right: 15 RECREW toggles `+0x7ac`, 16 SET_RALLY writes `+0x7d8`.
- Client: `Spawned::recrew`, `StepProduction`; `Spawned::rally_set/rally`, `RallyOf`, `StepExits`, tap handling `rally_armed`. Headless tests: `--human-order recrew|rally`.

## Repair (read, build 110)
Manual: "proceed to the nearest friendly repair bay and get repaired and recrewed". The GUI command 0xa issues order 19 (GET_REPAIR) at once (craft command switch 0x463410, table 0x463698); decommission (0xf) issues RECYCLE (23) and uses the same queue.
- **Yard choice (0x45ecd0):** objects of the same owner, not derelict, not disabled, whose class has `repairFacility` (yard class key, default 1; only `zmama` sets 0; the Shipyard **and** the Starbase class load it, loader 0x4571f0).
  Cost = (centre distance - craft radius - yard radius) / craft speed + 5 x ships in the yard's queue (0x5ca6c0 = 5.0); the cheapest wins; when the chosen yard is gone the choice is repeated every frame.
- **Queue:** RepairQueueManager (vtable 0x5cca3c, shares the update 0x558240 with the mining queue). Created in 0x45e540 from the object's **own** world matrix: docking point = the object, axis = its front, queue base `dockingBuffer`
  (default 90, 0x456ca8) behind it. The class loader does not store `repairHardpoint`; the models' `repair` node is not used by the code that was read. The craft is accepted only while its order is 0x13 (0x5577f0).
- **At the head (0x5577f0 calls the craft's 0x4685c0 with dt):** v = min(missing crew, team crew pool), computer teams treat a pool under 10 as 0. Finished when v < 0.1, every system is on with all points and the shield is full.
  Otherwise whole crew units move from the pool to the craft: t = v if v <= n else (n <= 1 ? 1 : n), n = int(20 x dt) (20 per second, 0x5ccde4); then the shield recharge and the five system repairs run once more with dt x 15 (0x5ca8a8).
  A finished craft goes into the yard's output queue (leaves through the `build` hardpoint, then order 39 to the rally point). Order 23 (recycle) at the head calls 0x465be0 instead (not ported).
- Client: `RepairBay`, `FindRepairYard`, `StepRepairs`, `StepDockedRepair` (Combat), `StepDocking` (shared with the freighters). Headless test: `--human-order repair`.
- **Not verified:** whether the original really docks at the object's centre (the queue code was read, the visual result was not seen); the order's weapon scan in 0x42a8c0 (the craft may fire at enemies while approaching) is not modelled.

## Other order types
GET_REPAIR (0x42a8c0, repair queue of a facility), RECYCLE (0x42a450, decommission), SET_RALLY / GO RALLY POINT, RECREW are analysed in `docs/formats/economy.md` and the GUI document
as far as they were read.

## Client
`Game.cpp`: `Spawned::order` (+ `ord_*` fields), `StepOrder`, `StartOrder`. Move, stop, attack and stand down cancel the order. Headless test:
`fleetclient_desktop --data <dir> --autostart 2duel --sim-seconds 120 --human-order scout|hunt|patrol|guard`.
