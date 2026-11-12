# Game setup screen (Instant Action / multiplayer setup), original layout

Source: the screen is built by the routine at 0x536250 (Armada.exe). Texts come from `label.map` (plain text, section `multiplayer_setup`, 118 lookups);
widget rectangles are constants in the code (reference size 640 x 480). Status: first part read, not complete.

## Confirmed (from the constructor calls)
| Widget | Rect (x, y, w, h) | Bitmaps (state 1/2/3) |
|---|---|---|
| Background | 0, 0 | `bitmaps\multiset\mpsbg.bmp` |
| Options button | 524, 446, 112 x 30 | `navmain1..3.bmp` |
| Start / launch button | 124, 446, 395 x 30 | `mpsstart1..3.bmp` |
| Previous menu button | 6, 446, 112 x 30 | `navback1..3.bmp` |
| Map preview frame | 385, 272, 128 x 20 | `mps128tg1..3.bmp` |
| Ready indicator | x 326, per player row, 10 x 10 | `mpsnready.bmp` / `mpsready.bmp` |
| Alliance markers (ally / neutral / enemy) | x 299 / 280 / 261, per row, 19 x 19 | `alfriend`, `alneutr`, `alenemy` 1..3 |
| Map list scroll up / down | 325, 250, 14 x 21 / 325, 377, 14 x 20 | `scrollup1/3`, `scrolldn1/3` |

The client previously used positions measured from the background bitmap (e.g. start button 126, 449, options 527, 450, back 9, 450); the code gives
the exact values above (the visible bitmap content sits a few pixels inside the widget rectangle).

## Open
Text fields, drop-down lists (player name, race, team), the map list, the description fields and the message area use further constructors
(0x522fa0, 0x51fda0, 0x5203e0, 0x5204c0, 0x520310, 0x5202d0, 0x522df0, 0x522c30, 0x522bf0, 0x525xxx) that still have to be read widget by widget.
