# GUI package (branch feature/gui-original)

Goal: bring the in-game GUI, menus and on-field displays in line with the original executable. Every item is checked against the
original code or data files first; what is only estimated stays marked as such.

## Order
1. Grid in 3D space
2. Displays in the play field (selection, shield/hull bars, target markers, selection frames)
3. In-game GUI (resources, command panel and rail, ship info field)
4. Minimap
5. Symbol view (not in the original; own design, needs rework)
6. In-game menus
7. Main menus (including the instant action / multiplayer setup screens)

## Rules for this package
- The debug text overlays (messages) stay as they are and are removed at the end; they are not part of the original.
- Touch controls do not exist in the original; they are designed alongside the work above.
- Not in this package: cinematic view (ShotDirector), boarding effects and sound, special weapons, computer player AI.
