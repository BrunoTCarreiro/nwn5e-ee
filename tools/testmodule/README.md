# nwn5e Test Grounds (the test module, #156)

A module for repeatable tests: the same light, the same ground and the same distances every time. The design is
`docs/superpowers/specs/2026-11-10-test-module-design.md`.

| Area | Tileset | What it's for |
|---|---|---|
| **The Field** | rural (`ttr01`) | Flat grass, always day, bright sun. Flags 10, 20 and 30 m ahead of the mark. |
| **The Hall** | castle interior (`tic01`) | A lit stone hall: two rows of pillars, two groups of crates, four braziers. |
| **The Dark Wing** | dungeon (`tde01`) | No light at all, for light-based stealth and light spells. |

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
- The Spell & Attack Bench's Play in game stands you on the area's mark first.

## Change it

- Edit `layout.json`:
  - the areas' corner grids (one letter a corner, row 0 the south, 10 m a tile);
  - their light;
  - their placeables (any base blueprint resref);
  - their marks.
- Then rebuild.
- `python -I tools/testmodule/check.py` runs the checks.
  - The tile rotation is proven on a shipped module's areas.
  - A corner grid the tileset can't tile is an error naming the tile.
