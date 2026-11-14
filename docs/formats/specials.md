# Special weapons (state machine classes)

Status: **verified** = read from the GOG `Armada.exe`, **assumed** = our choice.

All weapons derive from `Weapon` (vtable 0x5d27a4). Slot 4 of the weapon vtable is the per-frame process; the weapon keeps a "button pressed" byte at +0x2c and an
"active" byte at +0x2d. The craft keeps its special energy at +0x47c (maximum +0x478, fraction +0x474); craft vtable slot 0x58 (0x46f020) adds to it and clamps.
ODF keys on the craft: `maxSpecialEnergy`, `specialEnergyRate` (recharge per second, "we think" says the ODF comment).

Weapons with `special = 1` are placed in the speed rail by `buttonSlot` (1..5); `buttonSlot = 0` means no rail button (self destruct, detect cloak, mining beam).

## Implemented in the client (`game/Specials.*`)
| class label | original | behaviour |
|---|---|---|
| regeneration | 0x508ad0 | press toggles; switching on needs energy >= `specialEnergyCost`; while active energy drops by cost x dt per second and the shield rises by `regenModifier` x dt (craft slot 0x4c); it switches itself off when the energy is gone or the shield is full (max - current < 0.01). **Not implemented:** the call to 0x467e90 each frame (the ODF text says it also raises the repair rate of the systems). |
| cloakingdevice | 0x507770 | press toggles; switching on needs energy >= cost and takes the cost **once**; the state machine runs through fade states (`effectTimer` = 3 s). Client: fully cloaked ships can no longer be targeted by enemies (`Hostile`). **Assumed:** the fade progress (0..1) and that only a fully cloaked ship is hidden. **Not implemented:** `unsafeCloakDistance`, the detection weapons (`detectcloak`), transparent drawing, the crew message (craft slot 0x462d40 is called with 9 / 10). |
| selfdestruct | 0x508fc0 | first press starts a countdown of `timer` (5 s) and sets the craft's countdown flag (a crew-less ship then explodes instead of becoming derelict, 0x466dc3), a second press cancels; at zero the craft explodes (craft slot 0xd0 with the class radius 300 stored in +0x61c). **Not implemented:** damage to other objects inside `radius`. |

Energy start value is **assumed** to be the maximum.

## Not implemented yet (classes found in the ODFs)
toggleweapon (tractor beam, holding beam, borg assimilation beam, shield inverter...), launcher (mines, plasma), wormholegen, reflectweapon, phasecloakdevice, transwarpconduit,
cannon (engineering team, nanites, overrides, psychic blast, ripple...), PointDefenseLaser, minelayer, areacannon, areamissile, detectcloak. The rail button of these is shown disabled.
