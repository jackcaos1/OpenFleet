# Grid in 3D space (original behaviour)

Status: confirmed from the executable (class and routine addresses of Armada.exe, GOG edition).

- The grid belongs to the terrain object (global 0x6d6740, drawn by 0x5b83a0 from the render pass 0x5b8220 that uses the clipping
  distance `GRID_FAR_CLIP_PLANE` = 10000 from ART_CFG.h).
- Visible switch: byte 0x631de8, default on, toggled by the key `G` (Input.map entry `grid_toggle`, handler 0x4affa0).
  The per-detail switches `DETAIL_n_main_show_grid` (all 1 in the GOG ART_CFG.h) and `..._cinematic_show_grid` (0 only for the slowest
  detail level) decide whether the pass runs in the main and in the cinematic view.
- Extent: the four map bounds read into 0x6343f0 (x min), 0x6343f8 (z min), 0x6343fc (x max), 0x634404 (z max), i.e. the bounds
  from the map header (see bzn.md). Setup routine 0x5b81a0.
- Plane: y = 0 (the y field of the terrain object is never written).
- Cell size 100 (float at 0x631cdc). Lines start at the map bound, not at multiples of 100: x lines at `xmin + i*100` for
  `i = 0 .. trunc((xmax-xmin)/100)`, running from z min to z max; z lines at `zmin + j*100` for `j = 0 .. trunc((zmax-zmin)/100)`, running
  from x min to x max. All lines have the same colour (no brighter every fifth line).
- Colour: float triple (0.149, 0.128, 0.0) at 0x690ac0 (set by 0x57fb60), a dark olive/brown line colour.
- Lines are drawn with the engine's clipped 3D line routine 0x57a6c0 (view transform, frustum clipping).
- Unknown: the alpha/fourth argument of the line call ([0x6d4aa0 + 0x14]), whether lines are depth-tested against ships.

Client (game/Game.cpp): follows these rules; the earlier white grid with 75-unit cells around the object centre was an own estimate.
Not implemented: the placement grid for stations (STATION_PLACEMENT_GRID_SIZE 50, "footprint"/"valid_build" feature at 0x516200).
