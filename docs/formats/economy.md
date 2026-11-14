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
- Client build 95/96 showed an invented version (growing model, six orbiting bees, beams). A faithful port needs its own session.
- **Verified parameters (class constructor 0x454b70):** `numberOfWorkerBees` (+0x550), `workerBeeName` (+0x554, default `fbee`), `workerBeeHardpoints` (+0x558, `bot1`..`bot6` exist in the rig models),
  `weldingBeamSprite` (+0x568, default `phaser`, the ODFs say `weldbeam`), `weldingBeamWidth` (+0x56c), `timeUntilTakeoff` (+0x570, 2.0 s), `timeUntilClosure` (+0x574, 2.0 s: delay before the bee
  disappears once the bay doors begin to close), `timeWelding` (+0x578, 0.5 s per point); `weldingRadius` (spark size, 0x46a470).
- **Bee state machine (0x518250, partly read):** per bee a record with the state in +0x3c (0..7), a timer in +0x40, the owner (the rig) handle in +0x7c, a flight curve (object +0x6c, parameter +0x70, evaluated by 0x58c9d0 / 0x58c930)
  and a weld target (+0x5c, chosen by 0x50d2f0 from the placeholder). The states switch on distance thresholds (squared compare with 0x5d2af8) and on the timer; state 4 waits `timeWelding`,
  state 7 waits `timeUntilClosure` and then the bee is removed (0x5195c0). Traced in build 110: state 1 flies to the point `home hardpoint + front x (rig radius + 0x5d2af4)` (hardpoint matrix via 0x58c9d0 with the handle +0x70); on arrival (squared distance < 0x5d2af8) the weld target is chosen (0x50d2f0 with the placeholder +0x74, stored +0x5c, offset +0x78),
a spline through a control point (+0x50, built by 0x450270 / 0x450f40 / 0x4ef9a0) is set up and state 3 starts; state 3 ends on reaching +0x50 (state 4, timer = +0x4c); state 4 waits, then chooses a new target again (loop 3 -> 4); the later states 5..7 (return flight, closing) and the movement code after 0x5187d5 were not read.
- **BuildingEffect (0x50c950 update / 0x50caa0 draw, create 0x50cce0):** an effect record per construction with a duration (+0x1c) and time left (+0x20); the update counts the time down and
  derives a count (time fraction x +0xd0) that the draw routine hands to the placeholder mesh (0x58ce40), i.e. the structure appears progressively. Sub-effects (sparks) are updated in a list (+0x108).
  The exact reveal order is **not read**.
- **Client since build 107:** bees use the real parameters above (count, model, home hardpoints, timings) with **assumed** flight paths (random weld points on the growing structure, 90 m/s).

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

## Docking in full (third pass, 13.11.2026, ported in Game.cpp StepFreighters)
Geometry (MiningQueueManager, created in 0x45a420): the dock hardpoint HP with axis D (its front). The queue base B = HP - D x dockingBuffer (class +0x574, 70 for the mining stations); the queue is a straight line behind B along -D.
`length` (+0x3c) is the sum of 2 x radius of all queued craft; the tail is `B - D x length`.
- **Slot 0 (0x556d30), waiting point:** station position + tail - 40 x D (constant 40 at 0x5d3398).
- **Mining routine:** when the freighter is within **tooCloseToTurn** (horizontal, class +0x208) of the waiting point it enqueues (slot 4, 0x556b40: length += 2 x radius, tail moved, craft appended) and gets order 8, which sets its phase (+0x67c) to 1.
- **Phase 1, curve (slot 9 = 0x556ff0 builds it, 0x558240 / 0x556650 follow it):** cubic Hermite curve from the freighter's position S (tangent = its front x k) to the new tail E (tangent = D x k) with `k = 0.4 x |E - S|` (double 0x5d21f8).
  Each frame the parameter t grows by `speed x dt / |P'(t)|` (speed = dockingSpeedFactor x maxForwardSpeed; a second limit with the physics value `yawRotationRate`, class +0x1f8, is applied as well, formula not decoded) and the position is the curve point; at t = 1 the craft's orientation is set along the curve and the phase becomes 2.
- **Phase 2, creeping:** along its front axis (0x465b50, same speed). The head of the queue moves until `dot(HP - pos, front) <= 0`; every other craft stops at `B - D x (40 + sum of 2 x radius of the craft ahead)`, and only moves once the craft ahead has reached phase 2 or higher.
  When the head arrives, slot 10 (0x557b20 -> `MiningStation` 0x45aa50) is called and the phase becomes 3.
- **Phase 3, unloading (slot 11 = 0x557a40):** each frame `MiningStation::Receive` (0x45a910, transfer `dilithiumTransferRate` x dt as described above). Receive returns true when it can do nothing more (cargo empty, team full, wrong owner).
  Then the craft is removed from the queue (length -= 2 x radius) and appended to the station's **Output queue** (+0x7c8, see "Leaving"), i.e. it leaves through the same dock hardpoint, finally order 24 (mine) again.
- Craft handles that no longer resolve are dropped from the queue (0x558240 start).

**Moon choice (0x435010):** the candidates come from a spatial query; for each: cast to the moon class, the owner's bit in the moon's visibility mask (+0xb8) must be set, health > 0. For **AI teams only** (team record byte +0xa8, which 0x472100 reads) the shroud cell at the moon must be above 1.0 and a moon with
**3 or more other freighters within radius 125** (squared 15625, 0x5cab24) is skipped; human teams just take the nearest. The nearest remaining moon by squared 3D distance (0x4258b0) wins. Client: nearest moon, AI crowd rule included, no shroud (the client has none).

**Station destroyed:** not found in the queue code. The docked freighter's order 8 handler (0x42af80) falls back to other orders when the partner station is gone; the client lets such freighters resume normal flight and pick another station.

## Docking: remaining details (fourth pass, 13.11.2026)
- **Class field names** of the craft class (loader 0x45fea0): +0x1ec `borgPhysics` flag, +0x1f0 `impulseSpeed`, +0x1f4 `warpSpeed`, +0x1f8 `yawRotationRate`, +0x1fc `dockingSpeedFactor`, +0x200 `braccelControlStiffness`, +0x208 `tooCloseToTurn`.
  The craft object's +0x4a8 is initialised from class +0x1f0 (0x461899), i.e. the impulse speed (changeable through 0x462d80 / 0x458960). So the movement of 0x465b50 and the curve speed are **dockingSpeedFactor x impulseSpeed**, as ported.
  (My earlier remark that +0x4a8 was `maxForwardSpeed` was wrong: that name belongs to the class field +0x4a4 of the hover derivation.)
- **Curve speed limit decoded (0x558240 phase 1):** the helper functions are `0x558990` = P'(t) = 3 c3 t^2 + 2 c2 t + c1, `0x558a30` = P''(t) = 6 c3 t + 2 c2, `0x558930` = cross product, `0x450210` = norm, `0x4502b0` = divide by a scalar, `0x425300` = multiply, `0x450f40` = normalise.
  The curvature is `kappa = |P' x P''| / |P'|^3`; the parameter step is `min(dt x yawRotationRate / (kappa x |P'|), dockingSpeedFactor x impulseSpeed x dt / |P'|)`: the craft never turns faster than its yaw rate. Ported.
  The curve coefficients are set by 0x5569a0 in power form: c0 = P0, c1 = T0, c2 = 3 (P1 - P0) - 2 T0 - T1, c3 = 2 (P0 - P1) + T0 + T1 (a cubic Hermite curve). 0x556650 advances t (clamped to 1) and sets the craft's orientation (virtual +0x14) from the normalised tangent (a different branch for craft with the borg flag +0x1ec).
- **Axis:** the manager's vector at +0x24 is the hardpoint's **front** row of the world matrix (the 12-float matrix of 0x58c9d0: right, up, front, position); the queue base is `hardpoint - front x dockingBuffer`. The sign convention of the client (exit along +front, queue behind) therefore matches the data; the hardpoint matrix includes the station's rotation.
- **Station state while a freighter is docked:** `0x45aa50` (called when a freighter reaches the head, and with 0 at the end) stores the docked freighter's handle at station +0x7d0 (`dockedShip`), starts the station's animation state 0x10 (0x58c750) and, when the owner is the local player, triggers a cinematic request (0x47aef0, the shot director).
  `0x45a7d0` switches the station's `oreglow` sprite node (class +0x570) on while `dockedShip` is set.
  Not in the client yet: the dock animation, the ore glow, the cinematic shot.
- **Order 8 handler 0x42af80** only acts for AI teams (0x472100 reads the team flag +0xa8): for an AI freighter in the docked state with the right class flags it issues order 7 (slot 0x50). Human freighters are driven by the queue manager alone.
- **Station destroyed:** the station destructor (0x45ac30 -> 0x45a6d0) tears down its managers; nothing in the queue code re-targets the freighters, and `0x46e820` (craft destruction) only deletes attached children of a specific class. What exactly happens to docked or queued freighters is therefore
  not determinable from the static code alone; the client lets them resume normal flight.

## Dock presentation (client, build 104)
- While a freighter is at the head of the queue (unloading), the station's `oreglow` mesh group (six `wbeams` beam meshes around the station) is drawn; otherwise it is hidden (0x45a7d0 toggles the node's flags with `dockedShip`).
  The moon's sprite node of the same name is not affected. The client builds a second renderer for the `*mining` models for this.
- A cinematic request (0x47aef0) is posted for the local player when a freighter starts unloading; the raw arguments of the original call are 0, 0, station, freighter, 4, 3, `0x5cd560`, 3.0, 0 (last argument first); the director itself is not implemented.
- The station animation state 0x10 (SOD animation channels of the mining station models) is not played: the client does not animate station models yet.

## Construction: approach point and animation (read 14.11.2026, build 111)
**Where the rig stops (BUILD handler 0x42b040, verified).** Not in the middle of the site. Each frame the target point T lies on the line from the site centre (`+0x7e8`) towards the craft, at
D = craft radius + max(half diagonal of the structure class's bounding box in x/z, class `+0x1e0`) + 10 (0x5ca7bc), at the craft's own height. `+0x1e0` is **not** `footprintBuffer`: the class copies the
model's bounding sphere (centre `+0x1d4..`, radius `+0x1e0`) from the model at load (0x46b0df). Construction starts when the craft is already inside D, or within 15 (0x5ca7c0) of T, or within `tooCloseToTurn` of T while its
speed is at most 2.0; otherwise it flies to T. While building it holds its position (the handler goes to its own position). Client: `SiteStandoff`, `StepProduction`.

**Structure (BuildingEffect, verified).** The effect draws the structure's real model (class `+0x1e4`) at full size and only the first `n` triangles: `n = floor(progress x N)`, N = triangle count of the model, set through 0x50d7b0 / 0x58ce40.
The order is the model's node traversal (a node, then its children, mesh triangles in file order). No scaling, no placeholder model. **Edge sparks (correction, build 112):** every edge of a newly revealed triangle gets a spark (0x50d850; one per edge, kept in a map keyed by the vertex pair). `weldingSpriteName` defaults to `xspark` (string 0x60152c; sprite in weapon.spr, texture Xspark01, 4 x 4 frames, "Welding Sparks"), so it is not empty: the earlier remark that no sparks are drawn was wrong. The spark travels from one end of the edge to the other: `t += dt x weldingSpeed` (default 1.0), deleted above 1 (0x50c950), drawn at A + (B - A) t with size `weldingRadius` (station.odf: 1.0, "size of the build animation sparks"), animation speed `weldingAnimSpeed` (default 4.0). **Assumed:** the frame rate of the sprite animation and that the radius is the half size of the quad. Client: `Spark`, `StepBuildFx`. Client: `ModelRenderer::SetTriangleLimit`, `RevealedTriangles`. The client counts the triangles of the batches it draws (hidden LOD / borg nodes are not in the count; the original counts the whole model).

**Worker bee (0x518250, read).** Record `+0x3c` state, `+0x40` timer, `+0x78` anchor = site + bounding sphere centre, `+0x84` hover distance = max(15 (0x5d2b00), sphere radius), `+0x5c` weld point, `+0x50` hover point.
State 0 waits `timeUntilTakeoff`; state 1 flies to home hardpoint + front x (rig radius + 10 (0x5d2af4)); on arrival (squared distance < 1.0) a weld point is chosen (0x50d2f0: a random triangle among the revealed ones, or among the first min(N, 10)
while fewer than 10 are revealed, a random corner of it, in world space) and the hover point = anchor + normalize(weld point - anchor) x hover; state 3 flies there; state 4 waits `timeWelding`, the beam
(`weldingBeamSprite`, width `weldingBeamWidth`; 0x518dc0 -> 0x597370) runs from the bee to the weld point; then a new weld point is chosen. Speed limit 80 m/s (0x5d2af0). Orientation: while flying along its heading (curve tangent, look-at matrix 0x55d840),
in state 4 unchanged, in the bay states (0, 1, 6, 7) the hardpoint's orientation. So the bees face the structure and the beam ends on its surface. States 5 (to the exit point), 6 (to the bay), 7 (closing, `timeUntilClosure`, then the bee is deleted) mirror 1 and 0.
**Assumed:** what recalls the bees (the client does it when `timeUntilClosure` seconds of the build are left); the smoothing and the random wobble of the bee's flight (the original adds a leash and jitter, 0x5189a6..0x518c25) are not ported.
