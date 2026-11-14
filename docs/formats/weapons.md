# Weapon firing (Armada.exe, read 14.11.2026)

Status: **verified** = read from the GOG `Armada.exe`, **assumed** = the client's choice.

## Weapon object (Weapon::Process 0x509bd0, CanFire 0x509ff0 / Cannon 0x506a90)
- One weapon slot (`weaponN` with `weaponHardpointsN`) is **one** weapon object with a list of hardpoints (+0x14). Per shot only **one** hardpoint fires: the one for which `CanHit` is true with the smallest squared distance
  to the target (0x50a520 using vtable slots 0x20 = 0x50a290 CanHit and 0x44 = 0x50a730 metric). The client used to fire every hardpoint of the slot at once (changed in build 112).
- `CanHit(hardpoint, target)`: squared distance from the hardpoint's **world position** to the target's position (`+0x8c`, its centre) <= range^2 (class `+0xf8`). The target's radius is not subtracted.
  A firing arc exists only for classes with `restrictFireArc` (+0xef, default 0): the target must lie inside +-atan(0.58) (about 30 degrees, constant 0x5d2808) of the ship's heading in the x/z plane (0x50a170).
  In the shipped ODFs only the base `pulse.odf` sets it (1) and all its descendants set 0, so in practice weapons fire in every direction.
- Aim: direction = normalize(target centre - hardpoint position) (0x50a7c0); the ordnance is created at the hardpoint with that direction (Cannon 0x506930 with a target, 0x506a20 at a position).
- Timing: `CanFire(dt)` clamps the timer (+0x28) at 0, subtracts `dt x f` (f = field +0xd8 of the craft's modifier object 0x471ae0, 1.0 normally) and is true when the timer is <= 0. The weapon only fires when its "pressed" flag (+0x2c) was set
  by the craft's order logic in that frame. After a shot the timer gets `shotDelay (+0xf4) x m`, m = 1, or `weaponYellow` / `weaponRed` of the race when `crewAffectsWeaponDelay` (+0xf0, default 1) and the crew fraction is at most `crewYellowStatus` / `crewRedStatus`.
  Energy: with a cost (+0x34) the shot needs the craft's special energy and takes it (slot 0x58).
- Class keys read by the loader 0x5053a0: ordName, fireSound, offSound, fireEventID, wpnName, buttonSlot, buttonPriority, buttonBorder, tooltip, verboseTooltip, special, restrictFireArc, targetLocation, AItargetLocation,
  crewAffectsWeaponDelay, targetEnemyCraft, targetEnemyBuildings, targetAlliedCraft, targetAlliedBuildings, shotDelay, range, hitChance, needTarget, cloakEnable, beneficial, toggle, switchToAttack, specialEnergyCost, regularWeaponDelay,
  warpToHitModifier, stopToHitModifier, slowToHitModifier, toHitWarpModifier, toHitStopModifier (and toHitSlowModifier).

## Client
`Game.cpp` StepCombat (weapon loop): range check per hardpoint, one shot from the nearest hardpoint per delay. Not ported: the hit-chance modifiers (0x4ffd70), the energy cost, the "pressed" flag logic of the order handlers (target choice is the client's alert/order logic), the beam/projectile creation details.
