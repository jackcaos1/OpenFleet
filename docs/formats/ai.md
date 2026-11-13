# Computer player

## What is in the installation
- `AI/AIPs/*.aip`: **plain text** parameter files ("AIP = Artificial Intelligence Personality", C-like `int NAME = value;`, `double NAME = value;` and a
  `the_build_list` array). 87 files, all parsed by `engine/src/game/Ai.cpp` (`tools`-free check: every file yields its parameters, 54 carry a build list).
  `Aipdef.h` declares `Build_List_Element {name, units_to_build}` and `Better_Build_List_Element {name, units_to_build, base_num}` ("which base do we put it at, only used for stations").
- `AI/AIPs/Scheduler_README.txt`: the authors' description of the strategic scheduler (12 steps, goal types, raw priority formulas, match utility, strength rule).
- `AI/<Race>_Medium_Script.dsl` and `AI/<Race>_Medium_Rules.drl`: **native x86 DLLs** (40 KB each, Klingon script 221 KB). Not read. They probably hold the scripted part of the
  AI (`AI_Medium`, `AI_Team_Load_AIP` are in the executable's symbols). Whatever they do on top of the AIP data is therefore **unknown**.
- Executable: RTTI classes `AI_Team`, `AI_Base_Building`, `ResourceGoal`, `AttackGoal`, `AttackBaseGoal`, `DefendGoal`, `EscortGoal`, `ExploreGoal`, `AiMission`, `Goal`, `STGoal`; the planning
  budget routines 0x41cfa0 / 0x41d090 subtract the three build costs (dilithium, officers, crew) from a team budget.

## What the client does (assumptions marked)
- Parameters from `medium.aip` (the only computer kind the client has; **assumed** to be the file for "computer_medium"), build list from `<race>_build_list.aip`
  (**assumed** to be the list the build AI walks; the older, shorter lists inside `federation.aip` etc. are not used). Race = prefix letter of the team's first map object, else the slot's race.
- **Build order:** each list entry names a unit and a count. `units_to_build` is read as a running total (**assumption**): the n-th occurrence of a name means "have n of these".
  An entry is done when the team owns or has queued that many; this makes the lists usable on maps that start with a base. The AI looks every 2 s: it finds a producer that has the unit
  in its `buildItem` list (construction ship for stations, yards and the starbase for ships, research stations for pods), waits until the three costs can be paid (the executable's
  check, `CanStart`), then queues the unit. At most two queued per fixed producer. An entry no producer can build is skipped after 90 s.
- **Station sites** (**own design**, the original places them through `base_num` in the DLL): mining stations next to the n-th moon (n = `base_num`, nearest to the first starbase first), turrets next to
  the n-th mining station, everything else on the start spiral around the first starbase. The construction ship flies there and pays when it arrives (verified rule, economy.md).
- **Strategy** every 6 s, after `Scheduler_README.txt`: 400 unit grid; goals Attack Base (cell with enemy stations), Attack (threat, no stations) and Defend (own station and threat); raw priority =
  distance to home * `distance_from_home_priority_modifier` + distance to enemy * `distance_from_enemy_priority_modifier` + threat * `attack_troops_priority` (+ `perimeter_priority`) +
  stations * `attack_enemy_base_priority` / `defend_priority`; match utility = distance * `distance_from_goal_match_modifier` + raw priority; units are committed nearest first until their
  total shield strength exceeds the enemy shield strength in the cell; goals that do not reach it are rolled back. A committed unit gets an attack command on the nearest enemy of the cell.
  **Assumed:** threat = number of armed enemy ships in the cell; perimeter flag never set; explore and escort goals are not implemented; the squad is all armed ships.
- No cheating beyond the verified cost factor of `economy.md` (0.8 for the client's medium slot, only when a game type is chosen in the setup).

## Open
- Everything the DLLs do (research order, base building, retreats, cloaking, special weapons).
- Which AIP the real game loads per difficulty and race, and how `base_num` is meant.
- Explore and escort goals, the threat map relaxation (`relaxation_cycles`, `relaxation_coefficient`), `persistence_modifier`, `patrol_one_in_n`.
