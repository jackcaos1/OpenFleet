# Ship health, derelict state and destruction

Notes from reading the original executable's disassembly (not emulated). Addresses refer to the GOG build of the
original executable. Status per item: **confirmed** (read in the code), **assumed**, **unknown**.

## Systems (confirmed)

Five systems, each a 0x38-byte block in the craft object, in this memory order: shield generator, engines, weapons,
life support, sensors. Per system: active byte, hit points, crew-loss factor, repair values, a script "disabled" flag.
Damage rolls also know "hull" (costs crew) and "critical" (ship explodes). A hit-point value of 0 in the unit
definition means the ship does not have that system.

When a system reaches 0 points (0x467b70): it is switched off, and unless the ship is already derelict it loses
`crew * factor% + 1` crew. Life support off drains crew every frame (percentage of current crew plus a constant).

## Derelict (confirmed, 0x466ca0 / 0x466d80)

Crew below 1.0 on a ship with a maximum crew above 0 makes it derelict:

- crew is set to 0, all five systems are switched off (active flag 0) and the shield is set to 0 (virtual call `+0x54` is
  "set health", the shield; an earlier note called it the target speed, which was wrong),
- with all systems off, the flight simulation brakes the ship linearly and no longer damps its rotation,
- the ship stays in the world and stays selectable; no deselection happens here,
- while derelict, the per-frame health step does nothing (no repair, no shield recharge, no life support loss),
- if crew is restored (>= 1.0), the derelict flag is cleared and every switched-off system that has all its points is
  switched on again ("System n Renabled").

If a self-destruct countdown is running (handle in `+0x690`, started by 0x465be0), the ship explodes instead.
(Countdown meaning: assumed; the start function and its callers were read, the timer callback was not traced.)

Unknown: whether the owner changes when a ship becomes derelict (boarding/beaming is handled elsewhere).

## Destruction (confirmed unless noted)

`Explode` (0x468990): sets the exploding flag, logs "Ship Destroyed", removes the object from the selection, books
the loss in the team statistics. Does nothing if an invulnerability-like flag (`+0x678`) is set (assumed meaning).

Per-frame update (0x465cc0) while exploding:

1. Once: release weapon/attached objects, set the death timer: 5 s if the ship's speed (`+0x138`, length of the velocity
   vector, `+0x13c` is its inverse) is above 1.0, otherwise 5 s for a hero ship (unit key `isHero`) and 3 s for
   any other. A flag set by a separate removal routine (`+0x624`, meaning unknown) makes the timer 0.
2. While the timer runs: the normal update continues (the ship keeps drifting and rotating); every 0.05 s one of the
   seven effects `systemexpl1..7` (sprites in the weapon sprite file) is created at a random vertex of a random
   triangle of the ship's model (uniform over triangles, transformed to world space; the ship position if the model has
   no geometry). Effect scale: max(0.14 x `[+0x60]`, 4.0), lifetime parameter 1.0 s. `[+0x60]` is the object radius
   (confirmed by the ring definition's comment). The scale is the half edge length of the camera-facing quad in world
   units (the draw routine 0x5975e0 spans centre -+ size). The position is stored relative to the model of the destroyed
   object and is transformed with its current matrix every frame, so the effects move and turn with the tumbling ship.
   The sprite's 4x4 frame sequence (0.5 s) therefore plays twice within the effect's lifetime of 1 s.
3. At the end: the object named by the unit key `fireball` (a separate definition, e.g. a large fireball with its own
   model and sound) is created at the ship position with scale `[+0x60]`; if a self-destruct countdown was running the
   object named by `selfDestruct` is used instead. Then the ship's velocity vector, multiplied by 1.0 (2.5 for the
   countdown case), is handed to an unknown routine, and the object is marked for removal.

## The shockwave ring (read from the code at 0x51c8d0-0x51dd90; geometry details unknown)

Class `shockwavexplode` (also `omegaexplode`, same loader) is a separate effect object created some time after the
fireball (the `shockwaveDelay` of 0.6 s in the fireball definition; the creation call itself was not traced).
Unit-definition keys and defaults (constructor 0x51d830): `rays` (default 10), `color` (default 0.5/0.5/0.5), `texture`,
`radiusFactor` (4.0), `fadeFactor` (0.8), `rate` (10.0), plus damage keys `damageBase`, `damageVariance`,
`damageThreshold`, `shieldCrewModifier`, `hullCrewModifier` (the ring definition used by ordinary ships has none of them).

Behaviour per frame (update at 0x51d2a0, confirmed):
- a timer runs; the current radius is `2 x rate x timer` (units per second: twice the rate);
- the maximum radius is the object's own scale (field `+0x60`) times `radiusFactor`; when the radius exceeds it the
  object is removed;
- colour: the class colour; from `fadeFactor x maximum radius` on, the colour is multiplied by a factor falling
  linearly from 1 to 0 at the maximum radius (0x51d570);
- damage: every ship (selectable object flagged 0x8) within the current radius, except destroyed ones, takes damage
  `(damageBase + random x damageVariance) x (1 - distance / radius) x dt` through the normal damage entry point, with
  the threshold and crew modifiers of the definition; all five values may have per-target exceptions like weapon
  damage. A ring without damage keys therefore only looks like a ring. Before the damage step the update checks a class
  field (`+0x60 > 0.1`) whose meaning is unknown (nothing in the loader writes it for this class); the client applies
  damage whenever `damageBase` is above 0. Definitions with damage keys: self-destruct, artillery and omega rings (none of them is created
  by the client yet).
- the visual is a mesh built once per class and scaled every frame by (time, 1, time) (horizontal only). For each of the
  `rays` directions (angle = ray x 2 pi / rays) there are six vertices: radius factor 1,2,1,2,1,2 (times `rate`) and
  height 0,0,2,-2,-2,2 (not scaled by time). Neighbouring rays are joined by two triangles per band, three bands
  (vertex pairs 1/0, 3/2, 5/4): a flat ring from radius `rate x t` to `2 x rate x t` and two crossed slanted bands of
  height +-2. Texture u alternates 0/1 per ray, v is 0 on the inner and 1 on the outer edge. The texture and colour come
  from the definition (`wselfdes`, orange for the standard one). Blending is additive (confirmed: the object gets the
  renderer material "additive", whose blend constants are source ONE / destination ONE; "translucent" uses source alpha /
  inverse source alpha); culling: the renderer sets the cull mode to "none" at start-up and no override was found for
  this object, so both sides are drawn.
- when it is created (fireball update 0x51b330, confirmed): the fireball object remembers its own scale (the destroyed
  object's radius; the definition comment says "multiplier for the ship radius"). Once the fireball's age exceeds
  `shockwaveDelay`, and only if the scale is greater than `shockwaveMinimum`, the ring is created at the fireball's
  position with that scale, so its maximum radius is `radius x radiusFactor`. For smaller objects (and when no ring is
  defined) a request is sent to the cinematic camera director (class `ShotDirector`, the "cinema view" of the HUD) instead;
  it is not a camera shake. Not implemented.

## Engine failure and tumbling (confirmed)

With the engine system off, the flight simulation sets all angular accelerations to 0, so existing angular velocity
is never damped; linear velocity is braked with 2 m/s^2. A ship that was turning when its engines failed keeps rotating.

## The fireball model (confirmed from the data files, not from code)

The unit key `fireball` names a small definition (class `fireballexplode`) with: `dbName = "xplode"` (the model),
`explSound`, `length` (2.5 s for large and medium, 1.5 s for small and turret fireballs) and a `shockwave` reference
(`xshock`, class `shockwavexplode`: ring texture, orange, 16 rays) with a delay of 0.6 s and a minimum size of 40.

The model `Xplode` has no meshes, only eight sprite nodes `critical1..8` (positions within about 30 units of the
origin, all additive billboards). Their definitions are in the explosion sprite file: node 1 is the large orange ball
(size 40, 4x4 frame sequence over 1.1 s), nodes 2-5 spark balls (sizes 32, 32, 21, 18; frames over 1.3 s), nodes 6-8 a
second spark ball (sizes 15, 12, 12; frames over 1.5 s). After its frame sequence a node is switched off (visibility
key). The node animation channels of the model itself contain no keys. The model is loaded with a factor of 1.0 and the
object scale (`[+0x60]`, the destroyed object's radius) is not applied to it, it only decides about and sizes the
shockwave (0x51b290, 0x51b330, confirmed); all fireball definitions use the same model and differ in `length` and sound.
Every frame the model is turned by a fixed step of 0.01 rad about a random axis chosen at creation
(0x51b290 creates the step, 0x51b4a0 accumulates it); the client assumes 30 frames per second (0.3 rad/s).

## Engine implementation (OpenFleet)

`game/Combat.*` (`Dying`, `MakeDying`, `StepDying`, `DyingFxSize`), `game/Shockwave.*`, `Game::StepCombat`,
`EffectRenderer::DrawMesh`. Taken from the original: seven small effects at random model vertices that follow the ship,
death timer 3 s / 5 s, effect size max(0.14 x radius, 4) as half edge, the unscaled fireball model and the ring.
Assumptions in the client: the fireball rotation assumes 30 frames per second; if the fireball model cannot be loaded
the `bigorange` sprite is used. Not implemented: the ring's end-of-life colour reset and the cinematic shot request.
Not yet implemented elsewhere: crew-loss formulas as in the original, hit points per system from the unit definition,
the "disabled" system state, switching all systems off when a ship becomes derelict.

## Open: cinematic view (shot director)

Destroyed ships (`0x466090`, once at the start of the dying phase) and fireballs (`0x51b330`, for small objects or when no ring
is created) send a request to the shot director (class `ShotDirector`, global object `0x662bf0`, queue method `0x47aef0`; 27 call
sites in total). Parameters seen: 14.0, a duration (the dying timer or 1.0), a constant (1.5, 2.0 or 6.0), 3, 3 or 0, and up to four
object handles. The client has no cinematic view yet (the HUD field shows only its default image), so these requests are
not issued. To do: read the parameters and the shot selection, the camera classes (`CinematicCamera`, `ParabolicCamera`,
`CameraTrack`) and render a second camera into the HUD field. Also listed in `docs/status-analysis.md`.

## Health model (confirmed by emulation)

All of this is implemented in `game/Combat.*` and checked against the original: `tools/health_check.py` runs random
scenarios (hits with forced target, time steps, crew changes, self-destruct toggles, hit rolls) through the real functions
of the executable (Unicorn, single-precision x87 as Direct3D 7 sets it up) and through the C++ model, and compares shield, crew,
points, hit percentages, repair timers and the hit roll after every step. Six thousand scenarios (218 000 steps) agree.
Tools: `tools/health_oracle.py`, `tools/health_dump.cpp`.

- **Hit roll** (0x464dd0): an integer 0..99 from the game's generator (`s = (2273 s + 31) mod 0x19981595`, roll = `s mod 100`).
  The float hit percentages are subtracted in the order engines, life support, weapons, shield, sensors, hull; what is left is
  a critical hit. Result numbers: 0 engines, 1 life support, 2 weapons, 3 shield, 4 sensors, 5 hull, 6 critical. With the usual
  100 percent total including 0.5 percent critical the integer roll never reaches the critical remainder.
- **Damage packet** (0x465100): damage `d` (times the difficulty factor for the player's ships in single player: 0.5 easy, 2.0 hard,
  taken from `RTS_CFG.h`; not modelled in the client, factor 1). The shield takes `SHIELD_DAMAGE_PROTECTION x d` (1.0 in the GOG
  configuration), the rest `d - S` goes to the systems. A shield that goes below 0 passes the overflow to the systems. With
  `ignoreShield` the shield is skipped and the systems take all of `d`. If the shield is above 0, the threshold above 0 and
  `S - threshold` above 0, the crew drops by `crew x shieldCrewModifier x excess / 100 + min(excess, 1)`. After the shield stage a shield at or
  below 0 is set to 0 and the recharge delay starts.
- **Points** are whole numbers: `floor(systems damage + carry)`, the fraction carries over to the next hit (not for hull hits).
  Hull hits cost `crew x hullCrewModifier x damage / 100 + min(damage, 1)`; a critical hit with at least one point explodes the ship.
- **System at 0 points** (0x467b70): switched off, per-point repair time set, its hit percentage handed out equally to the other living systems
  (a system is living if it is on, or temporarily disabled with points left), its own hit percentage set to 0, crew loses `crew x crewLoss / 100 + 1`.
  When no system lives, the share goes to a collector (`criticalHitPercent` field `+0x5fc`); the first system repaired afterwards gets it back.
  The consequence is the answer to "what if all other systems are 0": the hit roll only distinguishes hull and critical afterwards
  (with 70 percent hull, 30 percent of the hits are critical).
- **Systems missing** in the unit definition: points 0 and hit percentage 0 (switched off from the start, never re-enabled).
  Template defaults for missing keys: 1 point, repair time 1. `SYSTEM_HITPOINTS_BONUS` (0 in the GOG configuration) is added to every system that has points.
- **Repair** (0x4683d0): all five systems repair at the same time, one point every `repairTime / maxPoints x crewFactor x repairStrength`
  seconds (the race value multiplies; checked with the real instruction: `DE F1` divides st0 by st1, so the time is proportional to the strength).
  The crew factor is the race's `repairRed` at a crew fraction of at most `crewRedStatus`, else `repairYellow` at most `crewYellowStatus`, else 1.
  The countdown is 0 while a system is at full health, so the first point lost to a hit that does not destroy the system is
  repaired on the very next frame. A system switched off by damage is switched on again only when all points are back; then its hit percentage is restored.
  A `TimedHyper` effect divides the time (not modelled: the divisor is 1).
- **Shield recharge** (0x466b00): only with the shield system on and the ship not derelict; the delay timer (`shieldDelay`, set when a hit leaves
  the shield at 0) runs down first and the rest of the frame recharges at `shieldRate` per second.
- **Life support off:** every frame `crew x lifeSupportLoss x dt / 100 + min(dt, 1)` is lost, until the ship becomes derelict.
- **Temporary disable** (special weapons) is a per-system timer that only runs while the system is off; the packet flags that set it are not modelled.

Not covered by the checks: the original random number sequence (the seed is set at run time), the impact effect that every hit with
systems damage spawns, the guard flags `+0x1b1`, `+0xc8` and `+0x678` (script flags), the disable flags of the damage packet, the
`TimedHyper` effect.
