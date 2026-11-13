# Things you can test in the original game

OpenFleet re-implements the game logic from the original executable, but some behaviour cannot be settled by reading code.
If you own the original game (GOG edition, build of 13 Jan 2022, or a compatible one) you can help by checking one of the items below and reporting what you saw
(an issue on this repository is fine; please say which edition and patch level you used, and attach a short video or screenshots if you can).
Never attach game files. Each item says what is known and what is not. The list of smaller single-player checks the maintainers keep for themselves is in [original-check-list.md](original-check-list.md).

## 1. What happens to freighters when their mining station is destroyed
Hard to provoke, therefore the most valuable item.
- **Setup:** a skirmish (Instant Action) with a mining station, at least two freighters, and an enemy (or your own weapons) strong enough to destroy the station.
- **Cases:** (a) a freighter is waiting in the docking line, (b) a freighter is docked and unloading, (c) a freighter is flying towards the station with a full load, (d) the station is destroyed while the freighter is mining at the moon.
  Also: does it make a difference when a second mining station exists? Does it behave differently for a computer-controlled team?
- **Known:** the station code (`MiningStation` destructor) removes the queue managers, nothing re-targets the freighters in the code that was read. A first report from an experienced player says the freighter "just flies on".
- **Please report:** where the freighter goes, whether it keeps its cargo, whether it drops into idle, whether it goes to another station, and whether it ever gets stuck.

## 2. The docking animation and the glow on a mining station
- **Known:** while a freighter is docked the station plays an animation state (0x10) and its `oreglow` node (a ring of six beam meshes around the station in the Federation model) is switched on; the owner's camera may show a short cinematic shot.
- **Please report:** how the station looks while a freighter unloads (screenshot from above and from the side), how long the effect lasts, whether the cinematic shot happens (and how it is framed), and whether the ring is visible when no freighter is docked.

## 3. Where and how ships leave a shipyard
- **Known:** the new ship appears at the yard's `build` hardpoint and is pushed along its own front axis at `dockingSpeedFactor` times its impulse speed until it has passed a point 1.1 times (station radius plus ship radius) away; then it gets its orders.
- **Please report:** for each yard type (starbase, shipyard, advanced shipyard, all four races) the direction a ship leaves in, whether it ever collides with the yard, and what the ship does afterwards without a rally point (stands still, turns, flies on). What does the rally point button do (position, ship behaviour)?

## 4. Construction ships
- **Known:** the building cost is paid when the construction ship starts building at its site; a placeholder object keeps the footprint; six worker bees weld.
- **Please report:** whether the cost is paid on arrival or when you give the order, what the bees do (flight paths, beam colours, sparks, sounds), how the structure appears (fading in, growing, panels), the duration, and what happens if you cancel.
  Also: which areas block building (stations, planets, nebulae, asteroid fields, moons, wormholes, black holes) and how large the exclusion zone is (the footprint buffer in the ODF is 80 for stations, 0 for pods).

## 5. Freighter and moon details
- **Known:** one freighter mines a moon at a time; the others hold 30 units further out; an AI freighter skips moons with three or more freighters nearby. Mining beam: 15 units every 1.5 s.
- **Please report:** how many freighters can really mine one moon at the same time, what the waiting ones do, what the moon looks like when it runs dry, whether the supply of a moon is shown anywhere (info panel bar), and whether freighters of the human player are sent to the nearest moon automatically.

## 6. Start units and race selection
- **Known:** with "Minimal / Standard / Super" the map's own start objects of the player teams are removed and replaced by the race list of the chosen race; with "Map Units" they stay. The default of the lobby is "Standard Units".
- **Please report:** the real behaviour on maps with and without start objects (2duel, 4hub, 4first), for different slot races, "random" race, and a Computer slot.

## 7. Resource flow
- **Known:** crew income of a starbase is 3.5 per second (race ODF), 1.25 times with a planet near; dilithium comes from freighters only; officers are a unit cap.
- **Please report:** measured values over a minute for crew and dilithium (with one and with two starbases), whether crew income depends on the distance to a planet, and what the display looks like when a resource is capped.

## 8. Computer opponents
- **Known:** `AI/AIPs/*.aip` are plain text (parameters and build lists); the scripted part is inside native DLLs (not read).
- **Please report:** the order in which a Medium and a Hard computer player builds in the first ten minutes (video with game time), which race list it follows, whether it builds research, how it protects its mining stations, when it first attacks, and whether it gets cost or speed bonuses you can measure (build time of the same ship for human and AI).

## How to report
Open an issue titled `[test] <number> <short summary>` with: game edition, platform, steps you took, what you saw, optional video. Please keep original game content out of the report (describe, do not attach).
