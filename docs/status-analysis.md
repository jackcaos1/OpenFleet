# Project status analysis (as of build 65, 2026-11-11)

Compiled from CLAUDE.md, `docs/` and the verification tools in `tools/`. The tests and oracle checks were not re-run for this document; the statements reflect the documented state.

Verification levels:
- **A** = checked against the original code (exe), by emulation or disassembly
- **B** = checked against the real game files (independent Python reference); shows the format is read correctly, not that the meaning is right
- **C** = assumption or own approximation
- **D** = not implemented

## 1. Checked against the original code (A)

- [x] Flight physics `HoverSim` (port of `Simulate 0x459440`); physics, servos and obstacle avoidance compared by Unicorn emulation (`hover_check.py`, `avoid_check.py`)
- [x] Pathfinder `PathGrid`: quadtree, stamps, region A\*, rubber-banding, funnel smoothing. 34 random scenarios identical (`path_check.*`, `oracle_*.json`)
- [x] Path stamps for nebula, black hole, planet, wormhole: 34 of 34 identical (`path_check_stamps.cpp`)
- [x] Vertex lighting, scene light (dlight), vertex normals, point lights: disassembly read and ported; sign of the light direction settled by emulator
- [x] Object classes for the savegame: 123 of 140 ODFs decoded from the exe
- [x] Config values (`RTS_CFG.h`, `game_cfg.h`) and main menu positions from the exe and the original files

## 2. Checked against real data (B)

- [x] ZFS archives (516 + 81 files)
- [x] SOD models: 272 of 273 parsed exactly to the last byte (open: `Wviracod.SOD`, v1.0)
- [x] ODF/TT: 489 + 27 files without deviation
- [x] BZN header, objects and paths of all 49 maps
- [x] TGA, SPR, GUI config, ZIP/Inflate (535 files by CRC)
- [x] UnitDef: 169 units, 118 weapons, 83 projectiles

## 3. To verify or rework (C)

### Rendering (see `model_audit.md`)
- [ ] LOD: the engine always picks the highest level; the original chooses by screen size (5 levels, detail cap, `0x57c810`)
- [ ] Visibility of `oq` (officer quarters upgrades), `damage`, subsystem nodes and the Borg overlay still to be rebuilt
- [ ] Winding: the engine guesses it from the volume sign; the original uses the polygon order (cull mode unchecked)
- [ ] Lighting is computed per fragment instead of per vertex; view vector for specular is approximated; lighting model 3 is treated like 1
- [ ] Team colours, alpha experiments and animations are not checked against the code
- [ ] Nebula glow lights: the original ties them to `ST3D_SHOW_LIGHT_SOURCE_INFO` (config = 0); whether they glow without that option is open
- [ ] Suns (`msun*`), fog-of-war light (`light.odf`), mapping of the map ambient (plausible, not proven)
- [ ] Remove old debug leftovers (alpha mode code in the core, `Ui.cpp` only as fallback, `ShipSim.cpp`)

### Movement and paths
- [ ] Path following `0x42ded0` is only rebuilt as "waypoint reached at < 150"
- [ ] Map bounds (`0x6343f0..`): currently estimated from objects ±1000
- [ ] Planet radius (`[this+0x60]`): the engine uses model radius × scale, the write site in the original was not found
- [ ] Behaviour when there is no path
- [ ] `Craft::Simulate 0x465cc0` (collision, position), the Borg branch of the physics, steering logic (Goto/Warp, `0x428000-0x42fb00`)
- [ ] Potential field / `Collision.cfg` (the construction ship flies through the starbase)
- [ ] Asteroid field: check whether the pathfinder makes the interim "hard sphere" workaround unnecessary

### Combat (damage model, debug-31)
- [ ] Assumption: 1 system point per penetrating hit
- [ ] Assumption: beam rate 10 crew/s; boarding: the attacker loses half as many as the defender
- [ ] Assumption: wireframe part i = system i
- [ ] Firing arcs are ignored
- [ ] The explosion is a sprite ("bigorange"), not the original model
- [ ] **Bug:** a ship under fire becomes unselectable, tumbles and never explodes

### HUD and menus
- [ ] HUD only checked in the preview render, not on a device
- [ ] Positions of newer menus (multiplayer, game setup) and colours of the options menu estimated from screenshots
- [ ] Align the command button slots with `game_cfg.h`

### Savegame
- [ ] `state.bzn` was never checked against a real original savegame (header, footer)
- [ ] Teams, AI state and mission footer are missing
- [ ] Linux client not connected yet

### Platform and quality
- [ ] Device test: sound, minimap, SAF/ZIP `pread`, new touch controls (many states are marked "not tested on device")
- [ ] Fuzz tests per parser and clang-tidy CI (both required by CLAUDE.md; not seen in the repo, not specifically searched for)

## 4. Not implemented yet (D)

- [ ] AI: the `.dsl` files are native x86 DLLs, behaviour has to be reconstructed via Ghidra; there is no computer opponent
- [ ] Campaign and mission logic
- [ ] Economy: building, harvesting, dilithium, crew, upgrades, rally (starting values are read but not used)
- [ ] Language files (LDL)
- [ ] Videos (Bink)
- [ ] Multiplayer
- [ ] Fog of war
- [ ] Special weapons (bar has no effect); commands patrol, scout, guard, search and destroy, repair
- [ ] Original tooltips, cinematic view, drag-selection box
- [ ] Options and load buttons, saving settings
- [ ] Web client (only planned as a reservation); Armada II (explicitly out of scope)

## 5. Suggested order

1. Game core: economy and AI block everything else; the AI is the largest reverse-engineering package.
2. Reduce the combat assumptions with the same emulation method as for the pathfinder.
3. Renderer deviations (LOD, `oq`, `damage`): manageable and well documented.
4. A device test pass before new features.
