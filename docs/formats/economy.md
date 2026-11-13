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

## Decommission (0x465c50)
Refunds `decommissionValue` (class +0xa0) dilithium and the craft's current crew (+0x4d8, clamped) to the team.

## Open
- Whether the `officerCost` return on completion is the complete officer rule: the executable shows no second deduction, but the object creation (0x46b590) was not read completely.
- Placement of the new craft (output queue, 0x46eac0 / spline queue), the shipyard animation, the worker bee scaffold.
- The AI accounting (0x41cfa0 / 0x41d090 subtract the same three costs from a planning budget).
