# Test Grounds (the test module, #156) - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `python -I tools/testmodule/build.py` writes `n5test.mod` ("nwn5e Test Grounds") into the owner's modules
folder. It has three areas (a sunlit field, a pillared hall, an unlit mine room), each with a cast mark, and the bench's
Play in game stands the character on the mark.

**Architecture:**
- A layout file (`layout.json`) is turned into the module's GFF files (`module.ifo`, `repute.fac`, and per area
  `.are` / `.git` / `.gic`) by `tools/gff/gffjson.py`'s writer, then packed by a small ERF writer.
- The game's tilesets (`.set`) and base placeable blueprints (`.utp`) are read from the owner's install at build time
  (a KEY/BIF reader), never committed.
- An auto-tiler picks each tile from its corner terrains.

**Tech Stack:** Python 3 (`python -I`, stdlib only) in nwn5e-ee-anim `tools/`. The C++ dev link is in nwn5e-tools
`dev/spell_bench_link.cpp`.

**Spec:** `docs/superpowers/specs/2026-11-10-test-module-design.md`

## Global Constraints

- The repo carries the module's source (the layout and its generator), not game files. Nothing read from the game is
  committed.
- Base tilesets only (`ttr01` field, `tic01` hall, `tdm01` dark wing), no haks, no scripts in the module.
- A build is deterministic: the same layout and the same game give the same bytes.
- `!go` / `!reset` are the game session's (shapes sent 2026-11-10); this plan doesn't touch the game's command code.
- Process:
  - TDD: each module has `check_*.py` assertions, run by `python -I tools/testmodule/check.py`;
  - commit with `git commit --only <paths> -F -` (new files `git add`ed first); one git command per call;
  - never push;
  - end each commit message with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
- Checks that need the game skip with a line saying so when `--game` / the default install isn't found.

## Review Focus

1. **The tile rotation convention.** A wrong one builds rooms whose walls don't join, and only shows in game. Expect it
   proven on a shipped module's area: adjacent tiles' shared corners agree under the convention.
   *(Test: Task 3, `check_tiler.py` "the convention matches a shipped area".)*
2. **A layout corner grid the tileset can't tile** (a lone wall corner). Expect an error naming the area and the tile,
   not a module with holes. *(Test: Task 3, "an untileable grid names the tile".)*
3. **The modules folder missing, or OneDrive-redirected Documents.** Expect the path from the Windows known folder
   (USERPROFILE\Documents, then the shell's Documents), `--out` overriding, and a clear error when neither exists.
   *(Test: Task 4, `check_build.py` "--out wins, the default is named".)*
4. **A module missing the factions** (`repute.fac`). `!spawn` and the bench's commoners need the standard five
   factions. *(Test: Task 4, "repute.fac has PC, Hostile, Commoner, Merchant, Defender".)*
5. **Play in game outside the Test Grounds.** Expect it unchanged: no mark, so no jump, and the formation is spawned
   around the character. *(Test: Task 5, `AnimBenchMarkScript` checks the no-mark branch.)*

---

### Task 1: The ERF writer (and reader, for the checks)

**Files:** Create `tools/testmodule/erf.py`, `tools/testmodule/check_erf.py`, `tools/testmodule/check.py` (runs every
`check_*.py`, prints each one's last line, exits 1 on any failure).

**Interfaces:**
- `write_erf(file_type: str, entries: list[tuple[str, int, bytes]]) -> bytes`:
  - ERF V1.0: a 160-byte header (`file_type`, `V1.0`, no localized strings, the entry count, offsets, build year /
    day 0, description strref `0xFFFFFFFF`);
  - the key list (resref 16 bytes, NUL-padded and lower-cased; res id; type; 2 unused);
  - the resource list (offset, size);
  - then the data.

  Entries are sorted by (resref, type) for determinism.
- `read_erf(data: bytes) -> list[tuple[str, int, bytes]]`.

- [ ] **Step 1: Write the check.**
  - `write_erf("MOD ", [("Module", 2014, b"ifo"), ("n5test_field", 2012, b"are")])` reads back as the two entries with
    lower-cased resrefs.
  - Bytes 0-8 are `b"MOD V1.0"`.
  - The entry count at offset 16 is 2.
  - Writing it twice gives identical bytes.
- [ ] **Step 2: Run** `python -I tools/testmodule/check.py`. Expected: FAIL (no `erf.py`).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: `all testmodule checks pass`.
- [ ] **Step 5: Commit** `feat: the test module's ERF writer (#156)`.

### Task 2: The game's resources and tilesets

**Files:** Create `tools/testmodule/gamedata.py`, `tools/testmodule/check_gamedata.py`.

**Interfaces:**
- `find_game(arg: str | None) -> str`: the argument, else `NWN_GAME`, else
  `X:\SteamLibrary\steamapps\common\Neverwinter Nights`, else the Steam default
  `C:\Program Files (x86)\Steam\steamapps\common\Neverwinter Nights`. Returns "" if none has `data\nwn_base.key`.
- `class Game(path)` with `.resource(resref: str, rtype: int) -> bytes | None`. It reads KEY V1 (`data\nwn_base.key`:
  BIF list, key table, id's top 12 bits = BIF index, low 20 = entry) and BIF V1 (the variable resource table).
- `read_set(text: str) -> dict`, returning `{"terrains": [...], "tiles": [{"id", "model", "corners": (tl, tr, br, bl),
  "heights": (tl, tr, br, bl), "crossers": (top, right, bottom, left)}]}` from the `.set` INI sections `[TILES]`,
  `[TILEn]` and `[TERRAINn]`.
- Types: SET 2013, UTP 2044.

- [ ] **Step 1: Write the check.**
  - `read_set` on an inline two-tile `.set` text gives the corners in (tl, tr, br, bl) order.
  - With the game: `ttr01` has 283 tiles and terrains Grass, Water, Trees; `tic01` has terrain `Stone`; `tdm01` has
    `Floor` and `Wall`; `.resource("plc_pillar1", 2044)` starts with `b"UTP "`.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass, or a skip line without the game.
- [ ] **Step 5: Commit** `feat: the test module reads the game's tilesets and blueprints at build time (#156)`.

### Task 3: The auto-tiler

**Files:** Create `tools/testmodule/tiler.py`, `tools/testmodule/check_tiler.py`.

**Interfaces:**
- `rotate(corners, k) -> corners`: a tile placed with `Tile_Orientation` k shows its `.set` corners turned k quarter
  turns. The direction is the one Step 1's real-area check proves; record it in a comment.
- `tile_grid(tileset: dict, corners: list[list[str]], heights=0) -> list[tuple[int, int]]`. `corners` is a
  (height+1) by (width+1) grid of terrain names, row 0 at the south. The result is one `(Tile_ID, Tile_Orientation)`
  per tile, row-major from the south-west, as `.are` `Tile_List` orders them.
  - Each pick has its four corners matching, all heights 0 and no crossers.
  - Among matches it takes the lowest id, then the lowest orientation.
  - It raises `ValueError("<area>: no tile for x,y with corners ...")` when nothing matches.

- [ ] **Step 1: Write the checks.**
  - Synthetic: a tileset with one tile `Wall, Wall, Floor, Wall` and one all-`Floor`, tiling a 2x2 room of floor
    inside walls, picks the expected (id, orientation) for each corner tile.
  - **The convention matches a shipped area** (Review Focus 1): read `Neverwinter Chess.mod`'s area via `erf.py` and
    `gffjson.read_gff`, and its tileset via `gamedata`. Every pair of horizontally and vertically adjacent tiles must
    agree on their shared corners under `rotate`. Skipped without the game.
  - **An untileable grid names the tile** (Review Focus 2).
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.** If the shipped-area check fails with one rotation direction, the other is the convention.
  Ledger the finding.
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Commit** `feat: the test module's auto-tiler, its rotation proven on a shipped area (#156)`.

### Task 4: The layout and the build

**Files:** Create `tools/testmodule/layout.json`, `tools/testmodule/build.py`, `tools/testmodule/check_build.py`,
`tools/testmodule/README.md` (how to build and open).

**Interfaces:**
- **`layout.json`:** `{"module": {"name", "tag": "n5test", "entry": "n5test_field"}, "areas": [{"resref", "name",
  "tileset", "size": [w, h], "corners": [...strings...], "light": {...}, "flags": n, "placeables": [{"template", "x",
  "y", "bearing"}], "waypoints": [{"tag", "x", "y", "facing"}]}]}`.
  - Corner rows are strings of single letters mapped per area (`"g": "Grass"`, `"#": "Wall"`, `".": "Stone"` / `"Floor"`).
  - Metres; 10 m a tile.
- **The three areas:**
  - **`n5test_field`:** `ttr01`, 8x8, all Grass. Sun ambient and diffuse bright, `DayNightCycle 0`, `IsNight 0`, no
    weather. Waypoint `n5test_cast_field` at (40, 6) facing north (90). Flags (`plc_flag1`) at (34, 16), (34, 26),
    (34, 36).
  - **`n5test_hall`:** `tic01`, 6x4, `Stone` inside a `Wall` border. Flags interior (1). Tile lights bright. Braziers
    (`plc_brazier`) in the four corners, 3 m in. Two rows of 5 pillars (`plc_pillar1`) at y = 13 and 27, x = 10, 20, 30,
    40, 50. Two crate groups (`x2_plc_bcrate1`, 3 each) around (25, 20) and (45, 20). Waypoint `n5test_cast_hall` at
    (5, 20) facing east (0).
  - **`n5test_dark`:** `tdm01`, 4x4, `Floor` inside a `Wall` border. Flags interior and underground (3). Every tile
    light 0. Sun and moon ambient / diffuse 0. Waypoint `n5test_cast_dark` at (20, 6) facing north.
- **`build.py [--game DIR] [--out FILE]`:**
  - Each area writes:
    - `.are`: the fields of the minimal shape (ID, Name, ResRef, Tag, Tileset, Width / Height, Flags, the sun and moon
      colours and fog, lighting fields, `Tile_List` from `tile_grid` with the area's light indices);
    - `.git`: `AreaProperties` with ambient sounds off, `Placeable List` from each template's UTP fields with position
      and bearing set, `WaypointList`, the other lists empty;
    - `.gic`: empty comment lists.
  - The module writes:
    - `module.ifo`: name, tag, the entry area / position / direction from the entry area's cast mark, `Mod_Area_list`,
      empty scripts, and `Mod_MinGameVer` from the game's own modules;
    - `repute.fac`: the standard five factions (PC, Hostile, Commoner, Merchant, Defender) with their standard
      reputations.
  - It packs everything with `write_erf("MOD ", ...)`.
  - The default `--out` is `<Documents>\Neverwinter Nights\modules\n5test.mod`, where Documents is `USERPROFILE\Documents`
    if it has a `Neverwinter Nights` folder, else `OneDrive\Documents`.
  - It prints the path and the areas' tile counts.

- [ ] **Step 1: Write the checks** (with the game; skipped without):
  - building to a temp `--out` succeeds;
  - `read_erf` finds `module.ifo`, `repute.fac` and each area's `.are` / `.git` / `.gic`;
  - `Mod_Entry_Area == "n5test_field"`;
  - each `.are` has `Width*Height` tiles;
  - each cast-mark waypoint is in its area's `.git` with the layout's position;
  - the dark area's tile lights are all 0;
  - `repute.fac` names the five factions;
  - building twice gives identical bytes;
  - "--out wins, the default is named": the parsed default path ends in `Neverwinter Nights\modules\n5test.mod`.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run, then build into the owner's modules folder.** Expected: the checks pass, and the build prints the
  path.
- [ ] **Step 5: Commit** `feat: Test Grounds - the test module, generated from a layout (#156)`.

### Task 5: Play in game stands on the mark

**Files:**
- Modify: `src/data/anim_bench_scene.h/.cpp`, `tests/data/anim_bench_scene_test.cpp` (nwn5e-ee-anim);
- Modify: `nwn5e-tools/dev/spell_bench_link.cpp`.

**Interfaces:**
- `std::string BenchMarkScript()`: run as the player. It finds the nearest object whose tag starts `n5test_cast_`, from
  `GetNearestObjectByTag` on each of the three tags. If found, it jumps the player there, facing the mark's way
  (`ClearAllActions(); JumpToLocation(GetLocation(mark))`), and sets the local string `n5sb_mark` to the mark's tag.
  Else it sets `n5sb_mark` to "".
- The link runs it before spawning. When a mark was found, the formation is spawned around the mark's location and
  facing, not the character's (the spawn script takes an optional anchor location). The cast waits `kCastDelayMs` as now.

- [ ] **Step 1: Write the failing test** `AnimBenchMarkScript`: the script names the three tags and `JumpToLocation`,
  sets `n5sb_mark`, and sets it to "" when none is found (Review Focus 5).
- [ ] **Step 2: Run.** Expected: compile error.
- [ ] **Step 3: Implement** the data and the link.
- [ ] **Step 4: Run** the game tests and the build. Expected: `0 failed`, `built`.
- [ ] **Step 5: Commit** the anim and tools sides: `feat: the bench's Play in game stands on the Test Grounds' mark (#156)`.

### Task 6: Hand-off

- [ ] **Step 1:** Run `check.py`, the game tests and the build. Expected: all green.
- [ ] **Step 2: Test rows** on the board, each standing on its own:
  - **TG-1:** build and open the module; the field is sunlit and flat.
  - **TG-2:** `!go hall`: the pillars, the crates and the braziers; the walls join.
  - **TG-3:** `!go dark`: pitch black; stealth works there.
  - **TG-4:** Play in game in the field: the character stands on the mark and the formation is ahead.
  - **TG-5:** `!reset` clears the spawns.
- [ ] **Step 3: Message the game session:** "anim ready at <commit> + tools ready at <commit>", the install, and `!go` /
  `!reset` against these tags. Comment on #156 with what was done and the files.
