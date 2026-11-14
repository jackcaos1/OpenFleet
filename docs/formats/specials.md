# Special weapons (state machine classes)

Status: **verified** = read from the GOG `Armada.exe`, **assumed** = our choice.

All weapons derive from `Weapon` (vtable 0x5d27a4). Slot 4 of the weapon vtable is the per-frame process; the weapon keeps a "button pressed" byte at +0x2c and an
"active" byte at +0x2d. The craft keeps its special energy at +0x47c (maximum +0x478, fraction +0x474); craft vtable slot 0x58 (0x46f020) adds to it and clamps.
ODF keys on the craft: `maxSpecialEnergy`, `specialEnergyRate` (recharge per second, "we think" says the ODF comment).

Weapons with `special = 1` are placed in the speed rail by `buttonSlot` (1..5); `buttonSlot = 0` means no rail button (self destruct, detect cloak, mining beam).

## Implemented in the client (`game/Specials.*`)
| class label | original | behaviour |
|---|---|---|
| regeneration | 0x508ad0 | press toggles; switching on needs energy >= `specialEnergyCost`; while active energy drops by cost x dt per second and the shield rises by `regenModifier` x dt (craft slot 0x4c); it switches itself off when the energy is gone or the shield is full (max - current < 0.01). **Verified (build 109):** every frame while active the process calls 0x467e90, which gives every system that is not script-disabled all its points back at once (`RepairAllSystems`); the health step then switches them on. |
| cloakingdevice | 0x507770 | press toggles; switching on needs energy >= cost and takes the cost **once**; the state machine (0x507770): states 0 and 2 handle the press, 1 = fading in (slot 0x64 = 0x5078b0, counts `effectTimer` down, then slot 0x68 = 0x507950 sets state 2), 3 = fading out (slot 0x6c = 0x5079a0, back to state 0). The visual is a shared "Cloak_Fog" effect object (global 0x6d4aa0) attached to the craft (`+0x38`) whose value ripples between two limits by `stepSize` per frame; the press also resets the craft's "attacked" stamp (`+0x45c`). The rule that makes a cloaked craft untargetable was **not found**. Client: fully cloaked ships can no longer be targeted by enemies (`Hostile`). **Assumed:** the fade progress (0..1) and that only a fully cloaked ship is hidden. Build 109: ships are drawn see-through while (partly) cloaked, enemies of the human player see nothing at full cloak (**assumed look**; the class reads `transparency` (gcloak: 0.0) and `stepSize` (0.0004), their use was not traced). **Not implemented:** `unsafeCloakDistance` (class field +0x158, its consumer was not found), the detection weapons (`detectcloak`, keys `detectClass`, `detectTimer`), the crew message (craft slot 0x462d40 is called with 9 / 10). |
| selfdestruct | 0x508fc0 | first press starts a countdown of `timer` (5 s) and sets the craft's countdown flag (a crew-less ship then explodes instead of becoming derelict, 0x466dc3), a second press cancels; at zero the craft explodes (craft slot 0xd0 with the class radius 300 stored in +0x61c). Build 109: the explosion is the class' `selfDestruct` fireball (default `xselfbal`, string at 0x200280) scaled to `radius` (0x4663d0); its shockwave `xselfd` (damageBase 380, threshold 605) damages everything in reach through the existing shockwave code. Client: the explosion still follows the normal dying sequence (delay), the original calls the explosion directly. |

Energy start value: **verified** (craft init 0x46d790: current = maximum = class `maxSpecialEnergy`).

## Not implemented yet (classes found in the ODFs)
toggleweapon (tractor beam, holding beam, borg assimilation beam, shield inverter...), launcher (mines, plasma), wormholegen, reflectweapon, phasecloakdevice, transwarpconduit,
cannon (engineering team, nanites, overrides, psychic blast, ripple...), PointDefenseLaser, minelayer, areacannon, areamissile, detectcloak. The rail button of these is shown disabled.

## Targeted specials (`cannon`, 14 weapons) - why they are not done
Each cannon fires a special ordnance whose class (RTTI `ShieldRemodulation`, `ComputerOverride`, `Nanites`, `PsychonicBlast`, `TargetOverride`, `TemporalStasis`...) has its own per-frame and hit code in a secondary vtable.
The firing path itself (Cannon vtable 0x5d214c, slots 0x506930 / 0x506a20) is the generic weapon path. Every effect is a separate reverse engineering job (several hundred instructions each); none has been read yet.
