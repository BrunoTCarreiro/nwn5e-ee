# nwn5e Test Grounds (the test module, #156)

A module for repeatable tests: the same light, the same ground and the same distances every time. The design is
`docs/superpowers/specs/2026-11-10-test-module-design.md`.

| Area | Tileset | What it's for |
|---|---|---|
| **The Field** | rural (`ttr01`) | Flat grass, always day, bright sun. Flags 10, 20 and 30 m ahead of the mark. |
| **The Hall** | castle interior (`tic01`) | A lit stone hall: two rows of pillars, two groups of crates, four braziers; past them the cover zone (#166): twelve obstacles to look through. |
| **The Dark Wing** | dungeon (`tde01`) | A long room with no light at all, for light-based stealth and light spells. |
| **The Cells** | castle interior (`tic01`) | Two jail rooms, each round a tile with a block built into its walkmesh (#166's tile obstacles). |

Each area has a cast mark: a waypoint tagged `n5test_cast_field`, `n5test_cast_hall` or `n5test_cast_dark`. The module
starts on the Field's mark.

## Build it

```
python -I tools/testmodule/build.py
```

This writes `n5test.mod` into `Documents\Neverwinter Nights\modules`.
- `--out FILE` writes it somewhere else.
- `--game DIR` names the game's folder. Without it, the build uses `NWN_GAME`, then the usual Steam folders.

The build reads the game's tilesets and base placeables from your install. The repo holds only `layout.json` and the
generator; nothing of the game's is committed.

## Open it

In the game: **New Game → Other Modules → nwn5e Test Grounds**.
- `!test` profiles and `!spawn` work there as anywhere.
- `!go field`, `!go hall` and `!go dark` jump to a mark.
- `!reset` clears the spawns and puts you back on the mark. These two commands are the game session's.
- The Hall's cover zone (#166): `!go cover_<name>` stands you on a cover mark, its obstacle 2.25 m ahead; `!spawn
  nw_orcb` puts a guard 5 m ahead, beyond the obstacle, facing you; `!los` says which points of your body its sight
  line reaches and the cover it gives you. The names, with the cover a guard there (an orc: its eyes 1.6 m up) sees
  you behind, standing / crouched (hiding) - cover_objects=1; your seven body points, 4 blocked Half, 5-6
  Three-Quarters, 7 Total:
  - statue, bookcase: Total Cover either way; the guard's middle slices cut at the obstacle.
  - brokenpillar: Three-Quarters standing (your head shows over it), Total crouched; the slices cut at it.
  - door_closed: Total Cover - the engine's own sight line (a shut door).
  - On the mark, 2.25 m back from a low obstacle, it covers little (the guard looks over it): table, table_dim (the
    table in dim light), crate, barrel, bench, rubble - no cover, standing or crouched; the slices aren't cut.
  - Step up against the low obstacle and crouch (hide): crate Three-Quarters (unseen while hiding; Half a step
    back), table Half (+2 AC and Dexterity saves; still seen), barrel, bench and rubble none (the barrel is too
    narrow: your shoulders show). Standing there: none (a 0.9 m table covers up to about 0.85 m of you - just under
    half).
  - door_open, portcullis: no cover (the session of 2026-11-23: clear).
- Tile obstacles (#166's tile blockers): obstacles built into a tile's own walkmesh, not placeables, each with a cover
  mark set up as the others (the obstacle 2.25 m ahead, a guard's room 5 m ahead):
  - `!go cover_tile1` and `!go cover_tile2`, in The Cells (`!go cells`, two jail rooms): tic01_o06_01's block (2.3 x
    2.6 m, 3 m tall) and tic01_h11_02's (4.6 x 2.2 m, 4 m tall). Their walkmesh blocks sight (material Obscuring):
    the engine's line stops at them - Total Cover by the tileset.
  - `!go cover_tile3`, in the hall on its west wall: the wall feature there, drawn in the walkmesh as a flat hole in the
    floor (1.9 x 1.0 m) - creatures walk round it, but sight and spells pass over it.
  - breakable (#184): in the lower hall among the pillars, a bookcase that isn't plot (15 HP) - to break under a spell
    or a weapon: cover from it drops at once, and a lasting spell's look (a Fog Cloud) fills in behind it.
- The Spell & Attack Bench's Play in game stands you on the area's mark first.

## Change it

- Edit `layout.json`:
  - the areas' corner grids (one letter a corner, row 0 the south, 10 m a tile);
  - their light;
  - their placeables (any base blueprint resref) and free-standing doors (a door blueprint, shut or `"open": 1`);
  - their marks.
- Then rebuild.
- `python -I tools/testmodule/check.py` runs the checks.
  - The tile rotation is proven on a shipped module's areas.
  - A corner grid the tileset can't tile is an error naming the tile.
