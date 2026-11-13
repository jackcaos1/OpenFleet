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

## Map selection screen (0x530b70, section `multiplayer_map_select`)
Popup with the background `bitmaps/misc/DBmsBG.bmp` (393 x 290); elements relative to it: filter combo 242, 2, 128 x 20 (entries `all_maps`, `maximum_players_8` .. `_2`),
list rows x 15, width 345, height 14, first row y 30, pitch 16, 14 rows (loop 0x53128d: y += 16 while y < 254), scroll bar 376, 48, 14 x 187 with arrows 376, 27 (14 x 21) and 376, 235 (14 x 20),
buttons `select_map_button` 19, 258 and `cancel_button` 260, 258 (112 x 30, plates `navback` / `navmain`). The popup is a Win32 dialog (resource 2175, window procedure 0x530b70) that is moved to the owner's client origin + (120, 91) with size 393 x 288 (0x530d73), i.e. position (120, 91) in the 640 x 480 game window; it paints the bitmap `DBmsBG.bmp` (400 x 290 blit).

## Client (build 75)
Implemented after this specification: all widgets above with the label texts, tapping cycles a combo / toggle (the original opens a drop-down list), the map is chosen
through the "select map" popup, the options are stored (`game/Skirmish.*`); effective so far: default units (starting units from the race ODFs with the spiral placement). Colour key magenta (255, 0, 255) of the
bitmaps is transparent.

## Portrait layout (deviation from the original, client only)
In portrait format (height > width) the left half (players, messages; reference x 0..330, y 0..445) and the right half (map, options; x 333..640, y 0..445) are drawn
one above the other from two cut-outs of `mpsbg.bmp`; below them the buttons: previous menu and main menu side by side, launch squeezed to the screen width underneath.
Landscape stays as in the original. The map selection popup is scaled to the screen width in portrait. The same layout is to be used for the multiplayer match screen.

## Button bitmaps (verified from the constructor arguments)
`ShellButton` takes three bitmaps: normal, pressed / selected, disabled (e.g. `scrollup1`, `scrollup3`, `scrollup2`; `alfriend1`, `alfriend3`, `alfriend2`; the plain
buttons use `1, 2, 3` in that order). In the alliance column the selected relation uses state 3 (coloured), the others state 1, the own row state 2 (grey).
In "alliances" mode (toggle at y 276, state 1) the team combo of each row is replaced by the three relation buttons (enemy x 261, neutral x 280, friend x 299, 19 x 19);
the ready lamp (`mpsready.bmp`, 10 x 10) lies at x 326 on the lamp of the row.

## Defaults decoded from the setup code (0x539a00 - 0x539e80)
Toggle state 0 shows the first registered label, state 1 the second (0x525330). Defaults: lock game 0 (open); alliances / teams 0 (Teams); give units 0; infinite moons 0;
Ferengi 1; director's cut 0; hide minimap 1 (shows "Minimap Hidden"); fixed placement 0 (shows "Random Placement"). Game type: the default value 2 is not in the list, the
field stays empty. Maximum players: the field selects entry `8 - n` where n is the map's player count (set when a map is chosen); with the unset default 9 it is empty. Visibility:
default value 3 (the list of this field is rebuilt depending on setting +0x70, 0x539c3f; not decoded).

## Open
- Exact column x of the player row items and the row start (client uses y = 57 + 19 i from the background bitmap).
- The visibility field (list depends on +0x70).
- Behaviour of each option on the session (which options the single player instant action honours).
- The map selection screen (`multiplayer_map_select`) and the chat / message list.
