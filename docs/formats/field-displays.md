# Displays in the play field (selection, bars) - first reading of the original

Status: first pass over `SelectionDisplay` (vtable 0x5ce2c8). Only what is stated below was read; the sizes and offsets of the bars are not yet decoded.

- **Class `SelectionDisplay`:** the init routine 0x499ac0 loads the sprites `selection.<i><j>` (the nine parts of a 3 x 3 frame, `gui_global.spr`, texture `GCselect`),
  `selectionCircle`, `selectionBillboard` (both texture `Gshadow`, "drop down selection circle"), `select_shield_bar`, `select_energy_bar`, `select_crew_dot`,
  `group_background` and `select_system` (system icons). Bars come from `Sprites/gui_global.spr` (texture `fguib04`: shield bar 126,184 96x10; energy bar 126,172 96x10; crew dot 214,74 14x14).
- **Configuration (RTS_CFG.h):** `USE_LIGHTING_FOR_SELECTION = 0` ("lighting or selection box": the box is used), `SELECTION_RECTANGLE_COMMIT_AREA = 100` (drag rectangle must
  reach this area), `SELECTION_PRIORITY = -5`, `SELECTION_CIRCLE_VERTICAL_OFFSET = -20` and `SELECTION_CIRCLE_SIZE_MULTIPLIER = 0.4` (both commented "INVALID" in the file).
- **Screen rectangle of an object (0x49a3f0, 0x49a4a0):** the eight corners of the object's bounding box are projected to the screen; the rectangle is their minimum / maximum.
- **Box (0x49a7d0):** four edges of that rectangle are drawn with the engine's line / quad routine (0x57ac50) in a colour stored in the object (+0x184, four values).
  Callers of the rectangle routine: 0x49a7d0, 0x49b3f0, 0x49b6a0 and 0x4a9650 (other displays; not read).
- **System icons (0x49a900):** per system flag (object bytes +0x1b5 .. ) an icon named `selectSystem_<n>` is drawn next to the rectangle.
- **Update routine 0x499f70** is the hover tooltip (it builds texts from `GUI_CP_CLASS_SEPARATE` / `GUI_CP_TOOLTIP_SEPARATE`), not a draw routine.

Not read yet: the position and size of the shield / energy bars and crew dots relative to the rectangle, the use of the nine-part frame, the group number display, the target marker
(`gridFXSprite`, config `GRID_FX_*`), the drag rectangle drawing, health bar thresholds.
