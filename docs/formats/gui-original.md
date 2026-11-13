# Original GUI: layout files and control panel logic

Verified in the GOG `Armada.exe` and `misc/gui_*.cfg` / `game_cfg.h` (13.11.2026). **Open** marks things not read.

## Layout files
- `misc/gui_glob.cfg` (race independent: pop-ups, selection display, comm display, ...) is included by `gui_fed.cfg`, `gui_kli.cfg`, `gui_rom.cfg`, `gui_bor.cfg`. All values are in a 640 x 480 reference.
- Main panels (Federation): speed rail 146,327 (247 x 40); info 146,367 (247 x 113); minimap 0,334 (146 x 146); control 524,334 (116 x 146); cinematic 393,356 (131 x 124); dilithium 395,0 / crew 477,0 / officer 559,0 (81 x 16 each).
- The four race files have the same keys (about 190 to 200) but different **panel piece geometry** (each panel is cut into pieces, e.g. `controlBackgroundPanel_0..5`), slightly different panel areas
  (control panel: Federation 524,334 116 x 146; Klingon 524,329 116 x 151; Romulan 524,332 116 x 148; Borg 523,329 117 x 151), shifted button rows (`controlButtonN` y offset 8 / 13 / 10 / 13), extra pieces (Klingon: `minimapBackgroundPanel_8`, `infoBackgroundPanel_8/9`, `infoSingleBackground_6..8`, `cinematicBackgroundPanel_6/7`)
  and, for Romulan, Klingon and Borg, **text colours** `infoSingleClassTextColor`, `infoSingleNameTextColor`, `infoSingleCrewTextColor`, `infoSingleOfficerTextColor` plus tooltips (`officerTooltip`, Borg `officerVerboseTooltip`; note the typo key `infoSingleNameTextBackgroound`). The client reads the race file as a whole, so all of this applies automatically; the text colours are now used.
- Control panel buttons: `controlButton1..12` (3 columns x 4 rows, row by row); `BUTTON_LOCATION_*` of `game_cfg.h` are these numbers.
- Info panel: construction bar and dilithium bar at the energy bar position (see field-displays.md and `infoSingleConstructionBar`, `infoSingleDilithiumBar`).

## Where the button table comes from
Each craft owns a **button table** at object +0x1c8: 13 entries (locations 0..12) of 28 bytes: command id, enabled flag (bit mask +0x178), a second flag (+0x17c, "active"), parameter. `0x458570` sets an entry (location, command, enabled, active, parameter),
`0x483350` sets only the enabled bit, `0x45d0a0` sets an entry with a build item. A second table at +0x350 (8 entries, `0x469c30` / `0x469d30`) holds the **speed rail** buttons.
The object's page number (+0x34c: 0 main, 1 orders, 2 build, other: back only) selects which entries are filled; the control panel shows the filled entries of the selected object at their location. `0x462ec0` is the base initialisation (`Craft`), `0x45c2a0` / `0x45d7c0` extend it for producers, `0x4583b0` for freighters.

## Command ids (event codes of the buttons)
2 move, 3 attack, 4 stop, 5 transport (rail), 6 transport special, 7 recrew, 8 orders page, 9 stand down, 0xa repair, 0xb guard, 0xc patrol, 0xd scout, 0xe search and destroy, 0xf decommission, 0x10 / 0x11 / 0x12 green / yellow / red alert,
0x13 and 0x18 build page, 0x14 harvest (freighter: issues order 24 = mine), 0x15 rally point, 0x17 research / upgrade, 0x19 back, 0x1a cancel, 0x1b (rail), 0x1c.. build item i (0x1c + i).
Locations come from `game_cfg.h`: move 1, attack 2, stop 3, repair 4, orders 5, stand down 6, harvest / recrew 7, transport special 8, upgrade 9, build 10, rally 11, cancel 11, decommission 10, guard 7, patrol 4, scout 5, search and destroy 6,
green 1, yellow 2, red 3, self destruct 11, back 12. `RESEARCH_UPGRADE` and `TRANSPORT` are -1 (hidden) in the GOG files.

## Rules (all objects; `Craft::InitButtons`, 0x462ec0)
Facts used: `movable` (class +0x51c), `armed` (weapon 0 exists with a non-zero count), `repair station` (0x45ebe0: the team owns a station that can repair).
- **Main page:** move (enabled: movable), attack (armed), stop (movable or armed), orders (always), stand down (always), repair (only shown for movable craft, enabled when a repair station exists).
- **Orders page:** decommission (enabled when a repair station exists or the craft is not movable); for movable craft guard, patrol, scout (always) and search and destroy (armed); for movable or armed craft the three alert buttons (enabled when the class field +0x554 is set, the active one is highlighted: the process alert value 2 = green, 1 = yellow, 0 = red);
  **self destruct** only when the craft's weapon list contains the weapon `gselfdes` (battleships, some heroes); back.
- **Other pages:** back only.
- **Producers** (stations, yards, construction ships; `0x45c2a0`, `0x45d7c0`): the main page buttons follow a per-class command mask (+0x340) and are disabled while a build is running; build (location 10, shown when the class has build items),
  rally point (location 11, for producers with a queue, i.e. yards and bases) and recrew (location 7, for stations whose crew is below the maximum while the team still has crew). **Build page:** item i at location i + 1,
  enabled when its technology is available, the queue is not full (yards: fewer than 5 entries, otherwise: nothing running) and the team can pay; **cancel** (location 11) only while a build runs; back (location 12).
  The decommission button of a producer is enabled only when nothing is being built.
- **Freighter** (`0x4583b0`): harvest (location 7, always enabled) sends the freighter to mine.
- **Rail table (+0x350):** slot 0 command 1, slot 6 command 0x1b, slot 7 command 5 (enabled when the craft has crew and is not flagged); weapons with the ODF key `special` and a `buttonSlot` > 0 occupy the rail slot given by `buttonSlot`
  (enabled by the research check 0x449790 / 0x449830 of the team). **Open:** the speed rail draws six buttons (the panel is 247 wide), how slots 6 and 7 map onto the rail, the meaning of commands 1 and 0x1b.

## Client status (build 105)
`Game::FillCommands` follows the rules above (locations, enabled flags, pages, self destruct only with `gselfdes`, producer build page with affordability, harvest, freighters wait after stop / move until harvest). Commands whose effect is not implemented
(guard, patrol, scout, search and destroy, repair, recrew, rally point) are shown but disabled. The text colours of the race files are used.
**Open for the next steps:** multi-selection (the control panel shows the commands shared by the selection; hotkeys use a separate flag block at 0x65ac64..0x65ac8f), tooltips (strings from `string*.ldl`, keys such as `officerTooltip`), cursors, the minimap logic (fog, shroud),
the ship display for several selected objects, the speed rail, rally points and the 16:9 / portrait layouts (own design on top).
