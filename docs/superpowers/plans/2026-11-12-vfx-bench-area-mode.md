# VFX Bench Area Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The VFX Bench designs area effects (a `vfx_persistent.2da` row and its effect models): open the game's, see the
area on the stage at its real size, play it at the character's feet in game, save it and ship it.

**Architecture:**
- **Proof first:** the dev layer rewrites a reserved vfx_persistent row in memory, as it does visualeffects row 1600, and
  places that area at the player's feet. It also reads how the client scatters the copies.
- **Twins:** the area draft (`.area`) is a C++ / JS twin pair (`src/data/vfx_area`, `vfx_bench/public/area.js`), as
  `anim_look` / `look.js` are.
- **The library:** the export gains the area rows and their models' emitters and flat planes.
- **The stage:** draws an area as copies of its slots' effects, scattered by the client's own rule.
- **Save and ship:** Save serves the row from the looks overlay in a bench range (200-299) at the next start, and
  `ship_area.js` moves an accepted area into the repo.

**Tech Stack:**
- C++17: nwn5e-ee `src/data` (tested by test.bat) and nwn5e-tools `dev/` (the dev layer, compiled into the DLL by build.bat).
- Node 18+ and hand-written WebGL: `vfx_bench/`.
- Disassembly with VS 2019's `dumpbin`; the symbols of the unstripped Linux build `bin/linux-x86/nwmain-linux`.

**Spec:** issue #161 (the owner's words, the parts in order, the recon), with the owner's picks of 2026-11-12, "go with
your recs":
- **Q1 A:** Entangle's flat planes are drawn, static and shown only.
- **Q2 A:** a saved area is served from the looks overlay in a bench range.

There's no separate design document.

## Global Constraints

- Never read any folder named `private/`.
- Commits:
  - `git commit --only <paths> -F -`, new files `git add`ed first, one git command per call; never `git add -A`.
  - End each message with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
  - Never push.
  - Don't commit `docs/tests.csv` on anim.
- nwn5e-ee work goes on branch `anim`, worktree `C:\Users\Bruno\Desktop\Projects\nwn5e-ee-anim`. Tools work goes on
  nwn5e-tools `main`. The game session merges, versions and installs.
- `src/data/look_rows` changes only after the game session's OK (asked 2026-11-12). Announce any change in `src/core`
  outside the dev layer and anim_* and wait.
- The dev layer is in dev builds only (`build.bat` without `public`); nothing of it reaches a public build.
- Rows:
  - visualeffects 1600 stays the editor's effect preview row.
  - vfx_persistent 199 becomes the editor's area preview row, and 200-299 the bench's saved areas.
  - nwn5e's own vfx_persistent rows stay where they are (100-115).
- The bench draws TGA textures only; a texture it can't read gets a flat stand-in colour, never an error.
- Areas are measured in metres, as vfx_persistent's RADIUS / WIDTH / LENGTH are; DURATION is milliseconds in the row and
  seconds on the bench.
- TDD: watch every test fail before the code that passes it.
- Scripts with backslashes are written with the Write / Edit tools, never a heredoc.
- Test rows stand on their own: the version, the setup, the bench to open.

## Review Focus

1. **A rectangle area** (SHAPE R, WIDTH x LENGTH, e.g. Wall of Fire's 10 x 2): the stage draws it at its size and
   facing, the scatter fills the rectangle, and the row writes WIDTH / LENGTH, not RADIUS (tests in Tasks 2 and 4).
2. **An area with no models, only an effect for its whole duration** (Web 8, Darkness 11): it opens, draws and plays
   (Tasks 3 and 4).
3. **Play in game again while an area is up, or Stop after it ran out:** the new one replaces it, never two; Stop with
   none is a no-op that says so (Task 5).
4. **A slot model with planes and no emitters, or a plane whose texture is DDS only:** it opens, draws a stand-in, and
   saves its planes unchanged (Task 3).
5. **Save when the bench range is full, or with an area's name already shipped:** Save says so and writes nothing
   half-way (Task 6).

---

### Task 1: The proof (part 0) - a live area row and the client's scatter

**Files:**
- Modify: `nwn5e-tools/dev/vfx_editor.cpp` (the area request, the reserved row's rewrite, placing the area, Stop).
- Create: `nwn5e-ee/data/rows/vfx_persistent.editor.txt` (row 199, NWN5E_VFXED_AREA, a circle of 1 m, no models).
- Research notes: the plan's ledger, then TOOLS-PLAN.md.

**Interfaces:**
- Produces:
  - `!vfxarea <saved draft> <radius m> <copies> [lifetime s] [edge 0..1]` in game chat (dev only).
  - The request line `request = area <seconds|permanent> <shape C|R> <radius> <width> <length>`, followed by up to
    three `slot <k> <count> <lifetime s> <edge>` lines, each followed by its effect draft (Task 5 sends it).
  - The scatter rule written down for Task 4: positions per copy, how EDGEWGHT weights them, how DURATION cycles a
    copy, and how many are alive at once.

- [ ] **Step 1: Find where the game keeps vfx_persistent.**
  - From the Linux build's symbols, find the functions that read vfx_persistent (the area object's load and its visual
    setup on the client).
  - In the Windows build, `dumpbin /disasm` over `?Load2DArrays@CTwoDimArrays@@...` finds a C2DA member for it (as
    +0x280 is visualeffects), or the area code loads it on demand.
  - Note where the MODEL / NUMACT / DURATION / EDGEWGHT / RADIUS columns are read: when an area is created, or cached
    at load.
  - Expected: one of the two answers, with addresses, in the ledger.
- [ ] **Step 2: Rule on the mechanism.**
  - Read per area → rewrite row 199 with `C2DA::SetCExoStringEntry`, as row 1600.
  - Cached at load → the issue's fallback: served rows with fixed model names whose files the bench rewrites (the
    editor folder is watched), the shape and size from the nearest of a few reserved rows.
  - Ledger: `Task 1: Ruling: ...`.
- [ ] **Step 3: Read the client's scatter.** Read the placing function (from Step 1) and write the rule down:
  - the random positions (uniform in the disc or rectangle?);
  - EDGEWGHT's weighting;
  - DURATION's cycle (each copy re-placed when its time runs out?);
  - the start (all at once, or staggered).
- [ ] **Step 4: Build `!vfxarea` and the area request in the dev layer.**
  - The saved draft's model (`saved\<name>.mdl`) is written as `n5vaNNNN_1.mdl` in the editor folder.
  - Row 199 gets SHAPE, RADIUS / WIDTH / LENGTH and the slot's MODEL01, NUMACT01, DURATION01 and EDGEWGHT01; the other
    slots are blanked.
  - NWScript runs `ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY | PERMANENT, EffectAreaOfEffect(199, "", "", "",
    "nwn5e_vfxarea"), GetLocation(player), seconds)`.
  - An area already up (tag `nwn5e_vfxarea`) is destroyed first.
  - `!vfxarea stop` destroys it.
  - Each step answers in chat and in the log.
- [ ] **Step 5: Build and run the game's tests.**
  - Run: `cmd //c 'C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\anim_build.bat'` and `...anim_test.bat`.
  - Expected: builds; test.bat passes; the dev layer compiles in.
- [ ] **Step 6: Commit both repos.** Add the test row VA-1 (the owner judges by eye):
  - Save a green fog in the VFX Bench as `n5_proof_fog` (Build from parts: Rising from an area).
  - In game: `!vfxarea n5_proof_fog 3 5`, then `!vfxarea n5_proof_fog 8 15`, then `!vfxarea n5_proof_fog 8 15 2 0.5`,
    then `!vfxarea stop`.
  - Expected: a green fog area of 3 m radius, then 8 m with more copies, then copies pushed toward the edge, each
    replacing the last without a restart; Stop clears it.
  - Hand off to the game session for a build and install: "anim ready + tools ready".
- [ ] **Step 7: Gate.** Stop until VA-1 passes. A fail is diagnosed from the log before anything is changed.

### Task 2: The area draft - C++ and JS twins

**Files:**
- Create: `src/data/vfx_area.h/.cpp`, `tests/data/vfx_area_test.cpp` (nwn5e-ee; added to the build's source lists).
- Create: `vfx_bench/public/area.js`, `vfx_bench/check_area.js` (nwn5e-tools; `check_area.js` is run by the bench's
  check list).

**Interfaces:**
- Produces:
  - C++ `struct VfxAreaSlot { std::string model; int count = 0; float lifetime = 0; float edge = 0; std::string low_model; }`.
  - C++ `struct VfxArea { std::string name; char shape = 'C'; float radius = 1, width = 1, length = 1;
    VfxAreaSlot slots[3]; std::string duration_effect; std::string sound_impact, sound_duration, sound_cessation,
    sound_oneshot; float oneshot_chance = 0; bool orient_ground = false; }`.
  - C++ `std::string WriteVfxAreaDraft(const VfxArea&)` and `bool ReadVfxAreaDraft(const std::string&, VfxArea*, std::string* err)`.
  - C++ `std::vector<std::pair<std::string, std::string>> VfxAreaRowColumns(const VfxArea&)`: the vfx_persistent column
    values, DURATION in ms, `****` for blanks.
  - JS `area.js` exports `writeDraft(area)`, `readDraft(text) -> {area, problems}` and `rowColumns(area)`: the same text,
    byte for byte.

- [ ] **Step 1: Write the failing tests.**
  - `VfxAreaDraftRoundTrip`: a circle, two slots, sounds.
  - `VfxAreaRectangleRow`: an R shape writes WIDTH / LENGTH and RADIUS `****`.
  - `VfxAreaNoModelsRow`: only `duration_effect` (Web's case).
  - `VfxAreaBadLineRefused`: a malformed line is named.
  - `VfxAreaClamps`: count 0..30, edge 0..1, lifetime 0..60 s, radius 0.5..30 m, each clamped, not refused (as #159's
    twist).
  - check_area.js: the same five cases in JS, plus `twinsAgree`: the JS writer's text equals a fixture text the C++ test
    also reads.
- [ ] **Step 2: Run them and watch them fail** (test.bat: the symbols are missing; check_area.js: the module is
  missing).
- [ ] **Step 3: Implement both twins.** Draft lines in order:
  - `area = <name>`, `shape = C|R`, `radius`, `width`, `length`;
  - `slot <k> <model> <count> <lifetime s> <edge> [low <model>]`;
  - `duration_effect`, `sound_impact` / `sound_duration` / `sound_cessation` / `sound_oneshot <chance>`, `orient_ground`.

  Defaults are left out, so a minimal area is three lines.
- [ ] **Step 4: Run both and watch them pass.** test.bat: 0 failed. `node vfx_bench/check_area.js`: all passed.
- [ ] **Step 5: Commit both repos.**

### Task 3: The library - the game's areas, their models' emitters and planes

**Files:**
- Modify: `nwn5e-tools/dev/vfx_editor.cpp` (ScanGameAreas: `kLibraryFormat` 14; `library\areas.txt`; the area models'
  drafts; the planes' textures).
- Modify: `src/data/vfx_model.h/.cpp`: the model reader keeps plain trimesh nodes as planes (vertices, faces, UVs,
  bitmap, the node's place and turn, its parent), and the draft and the ASCII writer carry them.
- Test: `tests/data/vfx_model_test.cpp`.
- Modify: `vfx_bench/server.js` and `public/` (the area list read from `areas.txt`).

**Interfaces:**
- Consumes: Task 2's `VfxArea` (`areas.txt` holds one area draft per row, after its row number).
- Produces: the library's `areas.txt` and `game\<model>.vfx` drafts with `plane` blocks. `VfxEffect` gains
  `std::vector<VfxPlane> planes` (shown and saved, never edited).

- [ ] **Step 1: Write the failing tests.**
  - `VfxModelPlanesRead`: a fixture model with emitters and one trimesh reads one plane with its UVs and bitmap.
  - `VfxModelPlanesOnly`: a model with planes and no emitters reads, rather than failing "no emitters"; Review Focus 4.
  - `VfxDraftPlanesRoundTrip`: draft, then model, then draft again is unchanged.
  - The bench's `check.js` gains `areasListed`: a fixture `areas.txt` lists Grease, Entangle and Web with their slots.
- [ ] **Step 2: Watch them fail.**
- [ ] **Step 3: Implement.**
  - The reader keeps flag-0x21 trimesh nodes.
  - ScanGameAreas reads vfx_persistent rows that have a model or a DurationVFX: base rows 0-46 and nwn5e's.
  - Each MODEL / MODELMIN is exported as a draft, emitters and planes; a model with neither is skipped and named in
    the log.
  - Plane textures are exported as TGA. A DDS-only texture gets no file, and the bench draws a stand-in colour.
- [ ] **Step 4: Watch them pass.** test.bat 0 failed; the bench's checks pass.
- [ ] **Step 5: Commit, and check in game.** `!vfxexport` writes areas.txt with Grease (32), Entangle (9) and Web (8)
  (in the next install's test row).

### Task 4: The area on the stage

**Files:**
- Modify: `vfx_bench/public/view.js` (the ground grown to the area; the shape drawn; textured planes),
  `public/app.js` (the Effect / Area switch; the area panel; slots), `public/area.js` (`scatter`).
- Test: `vfx_bench/check_area.js`, and a new `vfx_bench/browser_check.mjs` (headless, as the Spell Bench's).

**Interfaces:**
- Consumes: Task 1's scatter rule; Task 2's `area.js`; Task 3's library.
- Produces: JS `scatter(area, slotIndex, seed, t) -> [{x, y, born, dies}]`, the copies alive at bench time t, by the
  client's rule.

- [ ] **Step 1: Write the failing tests.**
  - check_area.js:
    - `scatterInside`: every copy is inside the circle or rectangle (Review Focus 1).
    - `scatterCount`: the count alive matches NUMACT by the rule.
    - `scatterEdge`: edge 0.5 has a larger mean distance than edge 0.
    - `scatterCycles`: a copy is re-placed after its lifetime.
    - `scatterSeeded`: the same seed gives the same copies.
  - browser_check.mjs:
    - `areaOpens`: Grease from the area list shows 21 copies within 6 m.
    - `areaRadius`: radius 3 redraws the circle.
    - `webOpens`: Web shows its whole-duration effect, no copies (Review Focus 2).
    - `entanglePlanes`: Entangle draws planes.
    - `noPageErrors`.
- [ ] **Step 2: Watch them fail.**
- [ ] **Step 3: Implement.**
  - Each copy is one `Sim` at its place; a particle budget across the copies keeps the stage smooth (ruled in-task:
    the number).
  - Planes are drawn as textured triangles.
  - The panel has: shape, radius / width / length; per slot the effect (pick or edit), count, lifetime and edge; the
    whole-duration effect; low detail on a toggle; the sounds by name.
- [ ] **Step 4: Watch them pass.**
- [ ] **Step 5: Commit (tools).**

### Task 5: Play in game for an area

**Files:**
- Modify: `vfx_bench/server.js` (`/api/play` with an area), `public/app.js`, and `dev/vfx_editor.cpp` (the request's
  slots, from Task 1's single slot).
- Test: `check_area.js`, `browser_check.mjs`.

- [ ] **Step 1: Write the failing tests.**
  - `areaRequestText`: the request file for a two-slot area holds both drafts, then the slot lines.
  - browser `playArea`: Play in game in Area mode writes it; Stop writes `stop`.
  - Review Focus 3's case: the dev layer replaces an area that's up; Stop with none says "no area up".
- [ ] **Step 2: Watch them fail.**
- [ ] **Step 3: Implement.** All three slots, the whole-duration effect and the sounds go into row 199.
- [ ] **Step 4: Watch them pass.**
- [ ] **Step 5: Commit both repos; test row VA-2** (Grease edited, played at your feet).

### Task 6: Save and ship

**Files:**
- Modify (after the game session's OK): `src/data/look_rows.h/.cpp`, which accepts vfx_persistent rows 200-299.
- Test: `tests/data/look_rows_test.cpp`.
- Modify: `vfx_bench/server.js` (`/api/save` with an area).
- Create: `vfx_bench/ship_area.js` (an accepted area into a checkout: its models to `data/files/n5a_<name>_<k>.mdl`, its
  row to `data/looks/vfx_persistent.look.txt`, the drafts deleted, as `spell_bench/ship.js`).

**Interfaces:**
- Produces:
  - Save writes `saved\<name>.area` and its slots' drafts, and the slots' models as `n5a_<name>_<k>.mdl` in the editor
    folder (found in a dev build).
  - It also writes its row into `build\spellbench\looks\vfx_persistent.<name>.look.txt` at the first free row in
    200-299.

- [ ] **Step 1: Write the failing tests.**
  - `LookRowsAcceptAreaRange`: 200-299 accepted; 199, 300 and 100 refused.
  - check_area.js:
    - `saveWritesRow`: the row is in the overlay file, the first free row taken.
    - `saveFullRange`: a full range refuses with the reason (Review Focus 5).
    - `saveShippedName`: an area named as a shipped one is refused.
    - `shipMoves`: ship_area.js moves it into a fixture checkout and deletes the drafts.
- [ ] **Step 2: Watch them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Watch them pass.** test.bat 0 failed; the bench's checks pass.
- [ ] **Step 5: Commit both repos; test row VA-3** (a saved fog's area after a restart, its row served; then shipped).

### Task 7: Hand-off

- [ ] The final review: a fresh reviewer on both repos; fix Critical and Important findings with tests.
- [ ] Test rows VA-1..VA-3 on the board, each standing alone.
- [ ] The TOOLS-PLAN section filled in (what was built, the scatter rule, the rulings).
- [ ] A #161 comment and `needs-test`.
- [ ] Tell the game session "anim ready at X + tools ready at Y" (the dev layer changed: a build and install).
