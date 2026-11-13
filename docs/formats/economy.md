# Resources, production and costs

Verified in the GOG `Armada.exe` (addresses given). Everything marked **open** is not verified.
The disassembly was read, **not** emulated, so unlike the health model there is no oracle run for these routines.

## Team record (0xbc bytes, see session-start.md)
`+0x10` dilithium (float), `+0x14` max dilithium, `+0x18` officers (int), `+0x1c` max officers, `+0x20` crew (float), `+0x24` max crew.
`AddDilithium` (0x449340): `value = value + amount`, replaced by the maximum when it would exceed it; there is no lower clamp.
Limits come from `RTS_CFG.h`: `cfgMaxDilithium` 999999, `cfgMaxOfficers` 100, `cfgMaxCrew` 99999.

## Class cost fields (ODF)
`dilithiumCost` (+0x68), `officerCost` (+0x6c), `crewCost` (+0x70), `buildTime` (+0x74, float) of the unit class (loader 0x46a470).
`crewCost` is also the crew the new craft starts with (comment in `craft.odf`).

## Difficulty factor (accessors 0x46bbf0, 0x46bc80, 0x46bd10, 0x46bdd0)
The slot state of the owning team (`+0xc4` of the player record: 1 human, 3 computer easy, 4 medium, 5 hard) selects a factor on
**dilithium, officers, crew and build time**: hard 0.4 (`HARD_AI_COST_AND_SPEED_HANDICAP`), medium 0.8 (`MEDIUM_...`), easy and humans 1.0.
The loader 0x445522 reads both from `RTS_CFG.h` into the globals 0x5fb84c (medium) / 0x5fb850 (hard) (the file defaults 0.5 / 0.75 are overwritten).
The factor is skipped when the game type setting (+0x78 of the settings object) is 2, which is the lobby default (not an entry of the list).
Costs are `ftol(base * factor)` (truncation), the time is `base * factor`.

## Producer queue (Producer / Shipyard / ConstructionRig, object fields)
`+0x7b4` list head, `+0x7b8` count, `+0x798` class being built, `+0x7a0` remaining time, `+0x7a4` total time (the same float at start).
- **Enqueue** (0x45b3a0): at most **5** entries; a sixth is ignored. New entries are appended.
- **Start** (0x45ba40), only when nothing is being built, takes the first entry (it stays in the list while it is built):
  fails with message 0xb when `dilithium + 0.5 < cost`, 0xd when `officers < officerCost` (only if `officerCost != 0`), 0xc when `crew < crewCost`.
  On success it subtracts dilithium (`AddDilithium(-cost)`), officers and crew (each clamped to its maximum) and sets the timer to the build time.
  The callers (0x45da60, 0x457b80) drop the first entry when the start fails.
- **Progress** (0x45ccb0): remaining time -= dt; finished when it is below zero (virtual call +0x160 = completion).
- **Cancel** (0x45bc20), only while building: removes the first entry and gives back dilithium, officers and crew in full (same factors).
- **Completion** (0x45cdf0): removes the first entry, **officers come back** (clamped to max), dilithium and crew stay spent, the new object is created
  (0x46b590) with the race of the producer and takes the crew as its own. For an upgrade class (RTTI `UpgradeClass`, the officer quarters) the shipyard
  instead adds `+0x1ec` (`officerGain`) to officers **and** max officers (0x45f570); starbases add their own `officerGain` (20) the same way when built (0x45f530).
- **Construction ship** (0x457b80): the cost is taken when the ship **starts** building at the site; a placeholder object (`BuildingEffect`, worker bees)
  stands there while the timer runs, at the end it is deleted and the real object is created by the same completion routine (0x457e40).

## Crew income
`Starbase::Update` (0x45f830, vtable of `Starbase` 0x5ccabc slot 33): every frame `crew += raceCrewAccumulationRate * factor * dt`, clamped to max crew.
`factor` is 1.0, or `planetCrewAccumulationModifier * planetClass[+0x1f4]` when a planet is found near the base (field +0x7e8, lookup 0x45f680).
Values: `crewAccumulationRate` 3.5 for all four races (race ODF), planet modifier 1.25 (1.5 Borg). Per starbase, so two starbases double the income. **open:** what the
planet class field +0x1f4 is and the search radius; the client uses no planet bonus.

## Mining (partly verified)
`MiningStation::Receive` (0x45a910): while the team has room (`dilithium < max`), the freighter carries cargo (+0x798 of the freighter, +0x794 the capacity),
both belong to the same team and the station is not disabled: the transferred amount is `min(cargo, rate * dt)` with the class field +0x578
(`dilithiumTransferRate`, 15 for the mining stations), the cargo is reduced and `AddDilithium` is called. **open:** freighter loading at the moon, speeds, moon amounts.

## Client model of the freighter loop (ASSUMED, not read from the executable)
Data: freighter `maxScrap` 150 = cargo capacity, mining beam `gminebm` (shot delay 1.5 s, range 120) with `minebm` `damageBase` 15, moon `mdmoon*` `maxHealth` 20000 (classLabel `scrap`),
mining station `dilithiumTransferRate` 15, `dockingBuffer` 70. The client: freighter -> nearest own mining station -> nearest moon with supply to that station ->
15 per 1.5 s into the cargo (the moon supply shrinks, unless "infinite moons") -> back, unload with the verified station rule above. The state machine of `Freighter`
(vtable 0x5cbea4), the docking hardpoints and the beam-to-cargo conversion are open. Officers: `Craft::SetOwner` (0x46e030) subtracts `officerCost` for every craft that
gets an owner, the destruction (0x46e820) gives it back; so officers are a permanent unit cap, the build start/completion pair only reserves them meanwhile.

## Decommission (0x465c50)
Refunds `decommissionValue` (class +0xa0) dilithium and the craft's current crew (+0x4d8, clamped) to the team.

## Open
- Whether the `officerCost` return on completion is the complete officer rule: the executable shows no second deduction, but the object creation (0x46b590) was not read completely.
- Placement of the new craft (output queue, 0x46eac0 / spline queue), the shipyard animation, the worker bee scaffold.
- The AI accounting (0x41cfa0 / 0x41d090 subtract the same three costs from a planning budget).

## Construction animation (partly read, NOT implemented like the original)
- `ConstructionRig` start (0x457b80): creates the worker bees through the bee manager (global 0x67f6d0, class `WorkerBee`, create function 0x518ea0), the number from class field +0x550
  (`numberOfWorkerBees`, 6 in const.odf). Bee i starts at the hardpoint named by entry i of the class's name list (+0x55c, helper 0x457f70; falls back to the root node).
  Each bee gets a 0xb8-byte record built by 0x519010 and registered with 0x5194a0 (manager 0x67f6c0). The per-frame bee behaviour is 0x518250 (about 860 lines, not read).
- The placeholder object (`mconstph.odf`, class `constPlaceholder`, `is_starbase`, footprint) is created at the build hardpoint transform; `BuildingEffect` manager (global 0x67f4a8, update 0x50c950 / 0x50caa0) draws the growing structure; both not read.
- Client build 95/96 shows an invented version (growing model, six orbiting bees, beams). A faithful port needs its own session.

## Free freighter with a mining station (verified, Armada.exe 0x457e40 / 0x45a830)
- At the end of a construction (0x457e40, after the completion routine 0x45cdf0 created the building) the code tests the class flag `0x200000` of the new object (set in the `MiningStation` constructor at 0x45a3fd)
  and calls `MiningStation::CreateFreighter` (0x45a830). No resources are paid for it.
- 0x45a830 creates the craft class named by the mining station's ODF `freighterName` (looked up with 0x46b380, created with 0x46b590) at the world transform of the station's dock hardpoint
  (hardpoint handle +0x7cc, set in 0x45a420 from the class's docking hardpoint name, fallback: model root), owner = the station's owner (+0x194), registers it with the world and with the
  station's queue manager (+0x7c8, OutputQueueManager), sets craft state 8 with the station as partner (0x46eb70) and subtracts the freighter's crew (+0x4d8, crewCost 100) from the station's crew (0x467570).
- Client: `OnBuildEvent` (Game.cpp) does the same: free freighter at the SOD node `dock`, station crew reduced by the freighter's crew. Open: the exact meaning of craft state 8, the exit path through the output queue,
  and the default dock hardpoint name when the ODF has none (all four mining models have a node `dock`).

## Freighter behaviour (read 13.11.2026; partly ported)
**Order types** (CraftProcess, `NewOrder` 0x426ad0 / per-frame dispatcher 0x427ff0 with the byte table 0x42813c and jump table 0x4280f4): the order record's type selects a handler slot of the process.
**Order 24 = MINE** (slot 0x6c, 0x42b430). **Order 8 = "docked with a station"** (handler slot 0x54, 0x42af80; `NewOrder` case 8 sets the craft's inactive flag +0x67c and clears its velocity target). So the user's guess
(state 8 = mining) was close but not exact: 8 is the docked / in-station state, 24 is the mining routine. `MiningStation::CreateFreighter` creates the freighter and issues order 8 with the station as partner.

Freighter object: +0x794 capacity (class +0x518 = `maxScrap`), +0x798 cargo, +0x79c target moon handle, +0x7a0 target station handle, +0x1c4 the mining beam weapon (`MiningBeam`, class `minebm`).

**Mining routine (0x42b430), one call per AI tick:**
1. Target moon: if the current one is missing or its health (+0x46c) is not above 0.01, take the nearest one around the freighter (0x435010, search radius 100000) and store it in +0x79c.
2. Target station: if the stored one is missing or destroyed, the nearest own `MiningStation` by 3D distance (0x45a620 walks the global list 0x6399a4, filter: owner team, not destroyed) is stored in +0x7a0.
3. If the free cargo space is below 0.01 (full): release the moon (if the freighter is its user) and deliver. With no moon: deliver when cargo > 5, otherwise stop and go idle (order 2).
4. Otherwise approach the moon until the distance between the two surfaces is within the beam range minus 15 (0x42dc30 / 0x42e4a0 move helpers). If another freighter is the moon's current user (+0x494, set by 0x453580), this one holds 30 units further out and does not fire (the moon serves one freighter at a time).
5. In range: aim the beam at the moon (0x4582f0 -> weapon 0x507180 / 0x5071c0). When the moon's health drops below 0.01, the target is cleared, the moon released and a random wait timer set.
6. Delivery: fly towards the station; when the distance is below class field +0x208 the freighter calls the station's queue manager (vtable +0x10 of the object at +0x7c4, `MiningInputQueue`) and issues **order 8 with the station** (dock). `MiningStation::Receive` (0x45a910) then unloads at `dilithiumTransferRate` per second (see above).

**Beam hit (`MiningBeam`, 0x4e0900, once per shot):** amount = min(beam damage (class +0x7c; `minebm` damageBase 15), free cargo space, moon health); cargo += amount; the moon's health is set to health - amount (the moon's health IS its dilithium
supply, ODF `maxHealth` 20000); the moon's user := the freighter (0x453580); when the moon's health falls below 0.01 a message (type 0x12) goes to the owner. Shot delay 1.5 s (`gminebm`).

**Client (Game.cpp StepFreighters)** now follows these rules for the decisions: nearest station and nearest moon measured from the freighter, one miner per moon with the 30 unit hold, full / idle thresholds (0.01 and 5), per-hit amount.
Still invented: the approach / hold positions (surface distance 100), the docking distance, and the whole undocking: how a docked freighter leaves (order 8 handler 0x42af80, `MiningInputQueue`, the station's `OutputQueueManager` and the spline path out of the dock) is **not read yet**; the client just continues from the dock point.
Also not read: 0x435010's filter (which objects count as moons), the random wait after a moon is exhausted, what happens when the station is destroyed while docked.

## Docking and leaving (read 13.11.2026, second pass)
**Order 8 / queue phases.** Every craft in a queue manager has a phase in `+0x67c`: 0 = just added (the manager logs and removes it if it is still 0 in the Output queue), 1 = being moved, 2 = finished.
`NewOrder` case 8 sets the phase to 1.

**Leaving (OutputQueueManager::Update, 0x557400, ported):**
- The new craft (yard ship, freighter from a mining station) is created at the world matrix of the producer's hardpoint (yard: ODF `buildHardpoint`, default `hp01`; mining station: dock hardpoint; both SOD nodes exist as `build` / `dock`).
- Phase 1: every frame the craft is moved along its own front axis by `dockingSpeedFactor` (physics ODF, a multiple of the impulse speed; 2.5 for `cnstphys`) x impulse speed x dt (0x465b50), until
  `dot(P2 - pos, front) <= 0` with `P2 = hardpoint + front * 1.1 * (station radius + craft radius)` (constant 0x5caa50 = 1.1).
- Phase 2: orders are issued. Freighter (class flag 0x80) leaving a mining station: order 24 (mine) with its stored moon (+0x79c), else with the station, else without target. Craft leaving a shipyard: order 39 (move to the rally point +0x7d8 when set), otherwise order 2 (idle).
- Client: `BeginExit` / `StepExits` implement phases 1 and 2 for new yard ships and for the free freighter. Rally points do not exist in the client yet, so yard ships stop at the exit point.

**Docking (MiningQueueManager::Update, 0x558240, 544 lines, NOT ported):**
- The freighter's mining routine flies to the point returned by the queue manager's slot 0 (0x556d30: a slot point built from the manager's vectors at +0x24 / +0x30, the constant 40.0 at 0x5d3398 and the station position) and enters the queue (slot 4, 0x556b40) when closer than the class value +0x208 = **tooCloseToTurn** of its physics ODF (50 for `cnstphys`).
  The client now uses that value for the dock test.
- Inside the queue the craft follows a **spline** (a 0x34-byte path object at craft +0x684, built by queue manager slot 9 = 0x556ff0): each frame the parameter advances by `min(dt * class[+0x1f8] / length, dockingSpeedFactor * maxForwardSpeed * dt / length)`, the position is evaluated from the cubic coefficients (0x558990, 0x558a30, 0x558930, 0x4502b0, 0x450210) and set with 0x556650; at the end of the spline the phase counts up (orientation set through the craft's virtual +0x14).
  Waiting crafts keep a spacing of 40 units (+ index) behind the one in front (phase 2 branch).
- Not read: the spline construction (0x556ff0, 0x556ed0, 0x556fa0), the unloading trigger (MiningStation::Receive 0x45a910 is not called directly from the queue update), and the way out after unloading (presumably the same Output queue as above, order 24 afterwards).
- Not read either: the filter of 0x435010 (which objects count as moons), what happens to docked freighters when their station is destroyed.
- `process +0x114` set when a moon is exhausted is a random angle (0x5018c0 x pi x 0.5), not a waiting time (corrects the earlier note).
