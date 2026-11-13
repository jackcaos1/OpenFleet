# Future features (beyond a faithful re-implementation)

The first goal of OpenFleet is a faithful, playable re-implementation of the original game. This list collects ideas that go beyond that. None of them may replace the original behaviour:
each must be an option that can be switched off, and the game must stay playable in its original form.

## Planned / partly there
- **Strategic zoom view (Supreme Commander style).** Zoomed out far enough, objects are replaced by symbols (already built: wireframe sprites of the info panel, team colour, selection corners, shield bar and crew dot, parts blinking in the damage colour, a build-progress bar underneath, dilithium moons as symbols with a supply bar).
  Ideas: smooth cross-fade between model and symbol, configurable switch size (default 100 px), friend/foe and colour-blind colour modes that apply to all team colours, grouped icons for large fleets, queue/orders drawn as lines, a minimap that follows the same symbols, simplified weapon fire at distance.
- **Random map generator.** Create new `.bzn` / `.mdf` map pairs in the original file format so that they also load in the original game.
  Design notes: a seed plus parameters (players 2 to 8, map size, resource richness, hazards); start positions on a ring or on opposite sides with the `Camera_<n>` paths and start objects of each race; dilithium moons per player and a contested central moon (finite or infinite);
  planets (for starbase bonuses), asteroid belts and nebulae placed with the original clearances (path stamps and footprint buffers are known); fairness checks (equal resources and path distances per player); preview bitmap (128 x 128) and `.mdf` header; background and skybox choice; deterministic output for a seed.
  A small tool in `tools/` first, in-game "Random Map" entry in the setup screen later.
- **Touch-first interface (own design).** Drag-select rectangle, double tap for "all of this type", long-press command menu, control groups with on-screen buttons, placement preview with confirm tap (built), larger hit areas, tablet layouts, optional one-handed layout.

## Ideas
- **Better presentation:** improved shaders (specular from material values, emissive glow, bloom), soft team-colour tinting, higher-resolution UI option, optional weapon and engine particle effects, modern anti-aliasing.
- **Map tools:** a map viewer/editor that reads and writes the original formats, map statistics (resources, balance), community map browser.
- **Skirmish options:** more AI personalities (mix the original AIPs), handicaps, team games, rally points and waypoints, patrol / guard orders, set-up scenarios (start fleets).
- **Quality of life:** replay recording (the simulation is meant to be deterministic), pause and speed control, build queue improvements, idle-unit finder, notifications ("freighter idle", "station under attack"), configurable key bindings, accessibility options (text size, colour-blind palettes).
- **Multiplayer:** LAN and online play for the simulation core (needs determinism work; only after the single-player game is complete).
- **Modding:** loading additional data folders on top of the original data (already possible with the data source stack), documented ODF extensions, scripting hooks for missions.
- **Campaign:** mission scripts are native DLLs in the original; a new, data-driven mission format could allow community campaigns.
- **Platform:** desktop builds for Linux, Windows, macOS (SDL2); a web build (WebGL2) with local file selection; controller support.
