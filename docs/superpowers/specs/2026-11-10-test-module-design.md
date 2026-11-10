# The test module: Test Grounds (#156; the Spell & Attack Bench's piece 3)

Date: 2026-11-10. Issue: https://github.com/BrunoTCarreiro/nwn5e-ee/issues/156 (opened by the game session at the
owner's word: "yes, open the issue and pass it to animation"). The owner skipped the questions: "skip the questions, go
with your recommendations". So everything below not quoted from the issue is *the designer's call*, open to the owner's
veto.

## Intent and why

From the issue:
- The owner: "a simple map with well lit open areas, some obstacles but also at least a section in deep shadow to test
  stealth and lighting effects".
- Today the bench's Play in game and the manual tests spawn creatures wherever the character happens to stand. That
  means a different area, light and set of obstacles each time.
- One fixed module makes looks and tests repeatable: the same light, the same ground and the same distances.

## What it is

One module, **nwn5e Test Grounds** (`n5test.mod`), with three areas:

1. **The Field** (rural, `ttr01`):
   - flat grass, 8 by 8 tiles (80 m), always day, no weather, bright sun;
   - a cast mark near the south edge facing north, with 70 m of open ground ahead;
   - flags at 10 m, 20 m and 30 m ahead of the mark, off to the side, as distance markers.
2. **The Hall** (castle interior, `tic01`):
   - one stone-floored room inside walls, 6 by 4 tiles (60 m by 40 m), lit by the tiles' lights and four braziers;
   - two rows of pillars down its length, and two groups of crates, for obstacles, line of sight and cover;
   - a cast mark at the west end facing east.
3. **The Dark Wing** (dungeon, `tde01`; the mines tileset `tdm01` builds no plain rooms):
   - one floored room inside walls, 4 by 4 tiles;
   - no light at all: the tiles' main and source lights off, the ambient and diffuse colours black, the area flagged
     interior and underground;
   - a cast mark facing north. This is deep shadow for nwn5e's light-based stealth and for light spells.

## Decisions *(the designer's)*

- **Built, not drawn.** A generator in the repo, `tools/testmodule/build.py` (Python 3, `python -I`, no dependencies),
  turns a layout file, `tools/testmodule/layout.json`, into `n5test.mod`:
  - the areas' sizes, tilesets and terrain grids;
  - their light;
  - the placeables (by the game's base blueprint resref) and the marks.
- **The repo carries the source, not game files** (the issue's constraint). The generator reads the base game at build
  time, from the owner's install:
  - the tilesets' `.set`, to pick each tile by its corner terrains (a small auto-tiler);
  - the base placeable blueprints, for an instance's fields.

  It writes the module with nwn5e's GFF writer (`tools/gff/gffjson.py`) and an ERF writer of its own. Nothing from the
  game is committed.
- **The auto-tiler.** Each area is a grid of tile corners (for example floor or wall). For each tile it picks the
  tileset's tile whose four corner terrains match, in any of the four rotations. The plain one (no crossers, lowest id)
  is preferred, so a build is the same every time. A grid it can't tile is an error that names the tile.
- **The marks** are waypoints tagged `n5test_cast_field`, `n5test_cast_hall` and `n5test_cast_dark`, and the module's
  entry is the Field's mark.
- **Getting around** goes through two chat commands, the game session's (the issue: "spawn at a spot, reset"):
  - `!go <field|hall|dark>`: jump to that area's cast mark, facing its way;
  - `!reset`: remove every creature the bench or the tests spawned (tags `n5sb_target`, `n5test_spawn`) and jump back
    to the current area's mark.

  `!test` profiles and `!spawn` work there as anywhere.
- **Play in game uses the marks.** When the character's area has a cast mark, the bench's replay first jumps the
  character onto it, facing its way, then spawns the formation as today. So the same look plays at the same distances
  on the same ground every time. Elsewhere it spawns around the character, as now.
- **Installing:**
  - `python -I tools/testmodule/build.py` writes `n5test.mod` into the owner's modules folder
    (`Documents\Neverwinter Nights\modules`, with `--out` to choose).
  - The owner opens it like any module: New Game → Other Modules → nwn5e Test Grounds.
  - The game session's installs can run the build too *(to agree with it)*.

## Non-goals

- Doors, transitions between areas, scripts, conversations and encounters. `!go` moves between areas, and the module
  has no scripts.
- Haks, custom tiles or custom models.
- Anything the rules need. The module is a stage, nothing more.

## Left to the builder

- The exact placeable positions and the flags' blueprint.
- The tiles' light indices (bright for the Hall, 0 for the Dark Wing).
- The module's and areas' remaining GFF fields: copied from a minimal valid shape, with no game content.
- The test rows.
