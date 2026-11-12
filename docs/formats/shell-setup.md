# Game setup screen (multiplayer setup / Instant Action) - original layout and behaviour

Source: the screen is built by the routine at 0x536250 in Armada.exe (GOG edition). Reference size 640 x 480. Texts come from `label.map` (plain text,
section `multiplayer_setup`; lookup routine 0x4740f0), widget rectangles are constants in the code. The widget classes are `ShellButton` (0x5239a0),
`ShellToggleButton` (0x525170), `ComboBox` (0x51e900, uses Windows combo box messages: 0x143 add string, 0x14e set selection), `ComboBoxArray` (0x5202d0, the player
rows), `RadioBox` (0x522bf0), a text label (0x5255a0), an image (0x5234d0, 0x521ff0) and a scroll bar (0x523160). Status: widget rectangles, texts and option
lists confirmed; defaults partly (see below).

## Background and fixed widgets
| Widget | Rect (x, y, w, h) | Bitmaps (`bitmaps/multiset/`) |
|---|---|---|
| Background | 0, 0 | `mpsbg.bmp` |
| Title (`title`) | 64, 6, 178 x 20 | text |
| Previous menu (`previous_menu`) | 6, 446, 112 x 30 | `navback1..3` |
| Launch (`launch`) | 124, 446, 395 x 30 | `mpsstart1..3` |
| Main menu (`main_menu`) | 524, 446, 112 x 30 | `navmain1..3` |
| Column headers | `player_name` 5,32,140 / `side` 167,32,75 / `teams` 273,32,35 / `game_name` 385,7,100 / `messages` 5,234,80 | text |
| Map name and description texts | 385,104,130x14 and 385,118,130 | text |
| Map preview picture | 385, 138 | `mapback.bmp` frame, preview image 128 x 128 |
| Select map button (`select_map`) | 385, 272, 128 x 20 | `mps128tg1..3` (opens the map selection screen, section `multiplayer_map_select`) |
| Chat target combo | 563, 421, 58 x 19 | `mpsteapd1..3`; entries `all`, `private`, `allies`, `enemies` |
| Map list scroll bar (elsewhere) | 325, 271, 14 x 107 with arrows 325,250 and 325,377 (14 x 21/20) | `scrollup/scrolldn` |

## Option widgets (right column x = 524, width 111, height 20; step 24)
Entries are `combo box` unless marked toggle. Labels are the `label.map` keys that the code adds (in order).
| y | id | Meaning | Entries |
|---|---|---|---|
| 60 | 2134 | game type | `normal_game_type`, `head_to_head_type` |
| 84 | 2136 | maximum players | `maximum_players_8` .. `maximum_players_2` |
| 108 | 2135 | ping filter | `ping_filter_off`, `_200`, `_400`, `_600` |
| 132 | toggle | game lock | `lock_game_off`, `lock_game_on` |
| 156 | 2144 | starting dilithium (settings +0x58) | `sd_value_1..5` = 3000, 4000, 5000, 7500, 10000 |
| 180 | 2142 | starting crew (settings +0x5c) | `sc_value_1..5` = 3500, 5000, 7500, 10000, 99999 |
| 204 | 2143 | maximum officers (settings +0x60) | `mo_value_1..5` = 100, 150, 200, 250, 999 |
| 228 | 2140 | default units (settings +0x80) | `default_units_map`, `_minimal`, `_standard`, `_super` |
| 252 | 2119 | tech level (settings +0x64) | `tech_level_one` .. `_seven` |
| 276 | toggle | alliances / teams | `alliances_teams_off`, `_on` |
| 300 | toggle | give units | `give_units_on`, `_off` |
| 324 | toggle | infinite moons | `infinite_moons_on`, `_off` |
| 348 | toggle | Ferengi | `ferengi_on`, `_off` |
| 372 | toggle | director's cut | `directors_on`, `_off` |

Left column (x = 385, width 128): toggle 295 `hide_minimap_on/off`; toggle 318 `fixed_placement_on/off`; combo 341 (id 2130, settings +0x74) `show_all`,
`show_available`, `show_allies`, `show_none`; combo 364 (id 2139, settings +0x4f/+0x50) `shroud_fog_on_on`, `_off_on`, `_on_off`, `_off_off`.

## Player rows
Eight rows (`ComboBoxArray` items, row height 19, column x 5 / 150 / 262 as measured from the background), each with: a ready indicator (x 326, 10 x 10,
`mpsnready.bmp` / `mpsready.bmp`), the three alliance markers (19 x 19, x 261 / 280 / 299: `alenemy`, `alneutr`, `alfriend` 1..3), a name combo (id 5, 145 x 19,
entries `empty`, `computer_easy`, `computer_medium`, `computer_hard`, and `kick` for other players' rows), a race combo (id 150, 111 x 19: `federation`, `borg`,
`romulan`, `klingon`, `random`) and a team combo (id 261, 58 x 19: `team_1` .. `team_8`). A bullet text (15, 19 x 19) marks the row.

## Defaults (settings constructor 0x4bd200)
Byte flags +0x4c..+0x57: 1,1,0,1,1,0,0,1,0,1,0,0 (+0x4e = handicaps off); ints: +0x64 tech level 0, +0x68 = 9, +0x70 = 1, +0x74 = 3, +0x78 = 2, +0x7c = 1, +0x80 default
units = 1, +0x84 = 0; per slot 0x4bf800 (kind 0, team 2?, handicap 100). Dilithium / crew / officers start with the RTS_CFG.h values (3500 / 4000 / 0x448d40); when
the value is not one of the five list entries the host's list selection falls back to the first entry (0x539a71 .. 0x539ac5: found -> select it, not found -> select
entry 0 and write it to the settings). The meaning of some default numbers (+0x68 = 9 for the maximum players combo, +0x78 = 2 for the game type) was not decoded.

## Open
- Exact column x of the player row items and the row start (client uses y = 57 + 19 i from the background bitmap).
- Behaviour of each option on the session (which options the single player instant action honours).
- The map selection screen (`multiplayer_map_select`) and the chat / message list.
