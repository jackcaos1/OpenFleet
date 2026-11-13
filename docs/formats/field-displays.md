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

## Bars and dot (decoded, 0x49b3f0 draws one object; all in screen pixels of the game window)
- **Rectangle:** integer left / top / right / bottom of the projected bounding box (0x49a3f0 + 0x5a2ad0).
- **Shield bar (0x49abf0):** only if the object has a shield (flag +0xbc & 8) and shield maximum > 0.1. Height 4 (global 0x60c49c), x = left, y = top - 5, width = right - left + 1.
  Background grey 0.2; fill width x f with f = shield ratio (+0x468, clamped), colour (0.5 (1 - f), 0.5 f, 0) (the factor 0.5 is "alpha - 0.5" with alpha 1). The bar sprite is a white strip (`fguib04`).
- **Energy bar (0x49ad50):** for an object class with a second pair of values (+0x474 / +0x478, maximum > 0.1): same width, height 4, fill colour (1, 1, 0) (yellow), at the bottom. The client has no such value.
- **Crew dot (0x49afe0):** one dot 7 x 7 at x = right, y = bottom - 8; crew ratio +0x4d8 / +0x4c8; colour green, yellow when the ratio is at or below the class value `crewYellowStatus`
  (+0x204), red at or below `crewRedStatus` (+0x208), multiplied by 0.5.
- **Names:** when the global 0x60c4b0 (initial value 1) is set the object name (+0xe8) and the class name (+0xdc of the class) are drawn at the rectangle; position not decoded.
- **Box lines:** drawn only when the global 0x663c9c is set (debug switch), together with a debug sphere (0x663ca0 / 0x663ca4): the normal game shows no rectangle.
- **Colour constants (0x57fa40..):** 0x690b00 magenta, 0x690b10 cyan, 0x690b20 yellow, 0x690b30 blue, 0x690b40 green, 0x690b50 red, 0x690b60 white, 0x690b70 black.
- Client (build 83): shield bar and crew dot follow this; the energy bar and the names are missing.

Not read yet: the position and size of the shield / energy bars and crew dots relative to the rectangle, the use of the nine-part frame, the group number display, the target marker
(`gridFXSprite`, config `GRID_FX_*`), the drag rectangle drawing, health bar thresholds.
