# The VFX Bench's Composer: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a Composer mode in the VFX Bench, where the owner builds the Floating Disk's four looks (#39) on the stage.
He picks parts from the game and his own files, places, turns, scales and stacks them with handles, and sees four stage
tabs with an onion skin. **Save and rebuild** writes `composition.json` and runs the generator.

**Architecture:**
- **The generator** (nwn5e-ee `tools\data\floatdisk\`, branch `anim`):
  - `floatdisk.js` becomes loadable in a page;
  - a part's `model` may be a list (a weapon's pieces);
  - `compositionText` writes composition.json in its own layout;
  - the part and texture reading moves from build.js into `reader.js`;
  - `build.js --json` reports for the bench.
- **The bench's server** (`vfx_bench/composer_server.js`, nwn5e-tools `main`):
  - finds the checkout;
  - lists the parts;
  - reads meshes through `reader.js`;
  - serves textures and the checkout's `floatdisk.js`;
  - keeps thumbnails;
  - saves by running build.js on a temporary composition, replacing `composition.json` only when it succeeds.
- **The page:**
  - `public/composer.js`: the pure parts (placement through `FloatDisk.placePart`, the summary, picking, resting, the
    handles' maths);
  - `public/composer_page.js`: the mode itself;
  - `view.js`: the chunk path's mesh drawing for the parts, pointer handlers and a ray.

**Tech Stack:**
- browser JavaScript in UMD modules, WebGL as the stage draws now;
- Node 18 checks (`require`);
- CDP browser checks (`mocap_bench/checks/cdp.mjs` `launch`);
- mdlmesh (already built at `nwn5e-ee-anim\build\floatdisk\mdlmesh.exe`) through `reader.js`.

**Spec:** `docs/superpowers/specs/2026-11-16-vfx-bench-composer-design.md`. Read it first: the plan argues from it.
- It awaits the owner's approval and his answers to Q1 (DDS textures) and Q2 (Play in game for a stage).
- Tasks 11 and 12 are built only on a yes. Everything else stands without them.

## Global Constraints

- **TDD:** watch every check fail before the code that passes it. Each task's step 2 says how it fails.
- **Repos:**
  - Generator work goes on nwn5e-ee branch `anim`, in the worktree `C:\Users\Bruno\Desktop\Projects\nwn5e-ee-anim`.
    `git branch --show-current` must say `anim` before each commit there.
  - Bench work commits directly on nwn5e-tools `main` (`C:\Users\Bruno\Desktop\Projects\nwn5e-tools`).
- **Git:**
  - `git add` new files by path first;
  - commit with `git commit --only <paths> -F -`, the message on stdin (`printf '%s\n' ... | git commit ...`, or a file
    written with the Write tool and `< file`), never a heredoc;
  - every message ends `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`;
  - one git command per Bash call;
  - never `git add -A`, never `git -C`, never push.
- **Nothing installed:** no npm package, no game install, no game launch.
- **Never read `private/`.**
- **Another job is editing `nwn5e-tools\spell_bench`:** read it, never write it. The Composer only calls `game_data.js`'s
  `resources`, `resourceNames` and `gameTable`.
- **The real worktree is never a check's target:**
  - every server and browser check runs on a temporary copy of the checkout (`vfx_bench/composer_fixture.js`), started
    with `--ee <copy>`;
  - mdlmesh is given by `NWN5E_MDLMESH`;
  - the real `composition.json` and `data\` are compared before and after (Task 4).
- **Ports:** the bench's own 8766 is never used by a check. The server check uses 18776, the browser check 18778, and
  both stop their server after.
- **Restarts:** the benches restart from their .bat. After a bench change the owner closes the VFX Bench window and
  double-clicks `vfx_bench.bat`; the stale guard tells him so.
- **Code:** plain UMD (`if (typeof module === "object" && module.exports) module.exports = api; else root.X = api;`), no
  build step, no framework, no new dependency.
- **The generator writes nothing new for today's composition:** every task leaves `node
  tools/data/floatdisk/check_build.js` reporting 13 files unchanged (from Task 2 on).
- **composition.json stays LF**, as it is in git. The built models stay CRLF, as build.js writes them.
- **Game files and pictures of them are never committed.** Thumbnails live only in
  `<bench build>\vfxedit\composer\thumbs\`.
- **Kept as they are:**
  - the effect mode (its fields, its library, Try it in the preview off by default, Space / R, its undo);
  - the frame's rules (no page scroll, the stage in view, at least 1100 x 700);
  - `window.vfxBench`'s existing hooks.

## Review Focus

1. **What you see is what's built.** The page places parts with the generator's own `placePart`. The page's summary of
   each stage equals `build.js --check --json`'s for the real composition. Test: Task 5 `summaryMatchesGenerator`.
2. **Save is all or nothing:**
   - a refused build changes neither `composition.json` nor any built file;
   - a `composition.json` changed on disk since it was opened is refused unless forced.

   Tests: Task 4 `refusedBuildChangesNothing`, `changedFileRefused`.
3. **Checks never touch the real worktree.** Test: Task 4 `realWorktreeUntouched`. It hashes the real
   `composition.json` and records `data\files`' times before and after the server check.
4. **The generator's output is unchanged:**
   - `build.js --check` prints the same before and after Task 2;
   - a full rebuild of a copy reports 13 files unchanged.

   Tests: Task 2's diff step and `check_build.js`.
5. **Resting:**
   - a part over another rests on its top (within 1 cm);
   - moved off it, back on the disk;
   - on the higher of two;
   - with Rests off, a move keeps the height.

   Tests: Task 5's rest checks and Task 9's `restsOnPart` and `restOffKeepsHeight`.
6. **Undo across stages:** undoing a change made on another stage shows that stage. One drag is one step. Opening or
   leaving the Composer clears the history. Tests: Task 5 `changedStage`, Task 7 `undoShowsTheStage`, Task 9
   `oneDragOneUndo`.
7. **The effect mode is untouched.** `node vfx_bench/browser_check.mjs` stays "all passed" after Tasks 6, 7, 9 and 10.

---

### Task 1: floatdisk.js in a page, model lists, compositionText (nwn5e-ee, anim)

Working folder: `C:\Users\Bruno\Desktop\Projects\nwn5e-ee-anim` (Git Bash: `/c/Users/Bruno/Desktop/Projects/nwn5e-ee-anim`).

**Files:**
- Modify: `tools/data/floatdisk/floatdisk.js`, `tools/data/floatdisk/check.js`, `tools/data/floatdisk/README.md` (a part's
  model may be a list; the weapon example)

**Interfaces:**
- **floatdisk.js is UMD:**
  - `(function (root) { ... if (typeof module === "object" && module.exports) module.exports = api; else root.FloatDisk = api; })(typeof self !== "undefined" ? self : this);`
  - `require("path")` moves inside `resolvePartFile`;
  - `modelNodes`, `carriesMark` and `sameText` use Buffer and stay Node-only (their comments say so).
- **`diskTexture(disk, id = "")`** builds its bytes in a `Uint8Array`, without Buffer. It returns
  `Buffer.from(bytes.buffer, bytes.byteOffset, bytes.length)` when `typeof Buffer === "function"`, else the
  `Uint8Array`. The bytes are identical to today's.
- **`readComposition`:**
  - a part's `model` is a resref, or a list of 1 to 4 resrefs (a weapon's pieces);
  - a list of one reads as `{ kind: "model", model }`;
  - a longer one as `{ kind: "models", models: [lower-case resrefs] }`;
  - error: `` `${where}.model: a model's resref, or a list of them (a weapon's pieces)` ``.
- **Keys and labels:** `partKey` gives `"models:" + models.join("+")`, and `partLabel` gives `models.join(" + ")`.
- **`mergeModels(list)`:** a list of `modelSpaceMeshes` results becomes one `{ meshes (concatenated, in order), lo, hi
  (the union) }`.
- **`compositionText(c)`** gives composition.json's text in its own layout:
  - two-space indents;
  - `_about` a string a line;
  - the disk a key a line, its arrays inline (`[0.42, 0.62, 0.95]`);
  - each stage a part a line, `{ "note": ..., "model" | "file": ..., "at": [...], "turn": n, "scale": n }`, then `tilt`
    when the part has one, then any other key the part had;
  - LF line endings and a final newline;
  - numbers as `JSON.stringify` writes them.

- [ ] **Step 1: Write the failing checks** in `check.js`, in its `check(name, fn)` style:
  - `floatdisk.js loads in a page (no require, no module, no Buffer)`:
    `const self = {}; new Function("self", "require", "module", "Buffer", src)(self);`, then
    `assert.strictEqual(typeof self.FloatDisk.placePart, "function")`.
  - `the disk's texture: the same bytes in a page`: the page copy's `diskTexture(DISK)` is a `Uint8Array`, and
    `Buffer.from(it).equals(F.diskTexture(DISK))`.
  - `a part's model may be a list (a weapon's pieces)`:
    - `["WSWLS_B_011", "wswls_m_011", "wswls_t_011"]` gives `{ kind: "models", models: ["wswls_b_011", "wswls_m_011",
      "wswls_t_011"] }`, partKey `models:wswls_b_011+wswls_m_011+wswls_t_011` and partLabel `wswls_b_011 + wswls_m_011 +
      wswls_t_011`;
    - `["plc_b04"]` gives `{ kind: "model", model: "plc_b04" }`;
    - `[]` and five names throw `/a list of them/`.
  - `mergeModels: a weapon's pieces as one model`: two `cube()`s, the second moved 1 m up, give 2 meshes, lo z 0 and hi
    z 2. `placePart` of the merge keeps the second 1 m (times the scale) above the first.
  - `compositionText gives back composition.json (0.0 written 0)`:
    `F.compositionText(JSON.parse(text)) === text.replace("[0.0, 0.32, 0]", "[0, 0.32, 0]")`, where `text` is the
    committed file read with CRLF turned to LF.
  - `compositionText: a part's keys in order, tilt only when given, an own file`: a part written
    `{ scale, at, model, note, extra: 1 }` comes out `{ "note": ..., "model": ..., "at": ..., "scale": ..., "extra": 1 }`;
    a `file` part and a `tilt` part are kept.
- [ ] **Step 2: Run it.** `node tools/data/floatdisk/check.js`. Expected:
  - six FAIL lines, among them `FAIL floatdisk.js loads in a page (no require, no module, no Buffer): require is not a
    function` and `F.mergeModels` / `F.compositionText is not a function`;
  - the last line `floatdisk check: FAILED (20 passed)`.
- [ ] **Step 3: Implement** the interfaces above in `floatdisk.js`. In README.md's "A part names either a game model or a
  model file of your own", add the weapon line:
  `{ "model": ["wswls_b_011", "wswls_m_011", "wswls_t_011"], "at": [0, 0, 0], "tilt": [0, 90] }`, "a weapon: its three
  pieces, read as one model".
- [ ] **Step 4: Run.**
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 26 checks pass`.
  - `node tools/data/floatdisk/build.js --check`. Expected: the same summary as before, ending `--check: nothing written
    (13 files would be)`, with `c_n5fd_large: 12 parts, 31 meshes, 1095 triangles`.
  - `git status --short`. Expected: only the three files.
- [ ] **Step 5: Commit.**
  - `git branch --show-current` (expect `anim`);
  - then `git commit --only tools/data/floatdisk/floatdisk.js tools/data/floatdisk/check.js tools/data/floatdisk/README.md -F -`,
    with "feat: Floating Disk's generator for the VFX Bench's Composer - floatdisk.js loads in a page, a part's model may
    be a weapon's pieces, compositionText writes composition.json in its own layout (#39)".

### Task 2: reader.js, build.js --json, check_build.js (nwn5e-ee, anim)

Working folder: the anim worktree.

**Files:**
- Create: `tools/data/floatdisk/reader.js`, `tools/data/floatdisk/check_build.js`
- Modify: `tools/data/floatdisk/build.js`, `tools/data/floatdisk/README.md` (`--json`, `check_build.js`, the files
  table)

**Interfaces:**
- **`reader.js`** (Node only), build.js's reading moved here unchanged in behaviour:
  - **`ensureReader({ repo, reader })`:** `reader` given must exist. Otherwise `<repo>\build\floatdisk\mdlmesh.exe`,
    built by `build_mdlmesh.bat` when it's missing or older than its sources (today's `ensureReader`).
  - **`create({ game, core, gameData, reader })`** returns:
    - `res`: `gameData.resources(game)`;
    - `part(part)`: `{ meshes, lo, hi }` in the model's space.
      - `model`: `res(model, 2002)`;
      - `models`: each piece read, then `F.mergeModels`;
      - `file`: its bytes, an ASCII one through `F.readableAscii`;
      - each through mdlmesh and `F.modelSpaceMeshes`, with today's error texts.
    - `findTexture(name, besideDirs)`: today's result (`{ where: "game" }`, `{ where: "beside", copies }` or `null`).
    - `image(name, besideDirs)`, for the bench: `{ where: "game" | "beside", tga: Buffer | null, dds: Buffer | null,
      plt: boolean, txi: string }`, or `null`.
      - The game's: `ovr\<name>.tga / .dds / .plt / .txi` first, then `res(name, 3)`, `res(name, 2033)`, `res(name, 6)`,
        `res(name, 2022)`.
      - Then beside: `F.texturesBeside`.
    - `close()`: removes its temporary folder.
- **`build.js`** requires `reader.js`. Its output and files are unchanged.
- **`--json`:**
  - the text summary isn't printed; one line is: `JSON.stringify(result)`;
  - `result = { check, stages: { <stage>: { model, parts, meshes, triangles, top, tall, pile: { model, tall } | null,
    list: [{ label, size: [x, y, z], from, to, past }] } }, shipped: [{ texture, from: [paths] }], files: [{ state,
    file }], wouldWrite, unused: [files] }`;
  - lengths are rounded to 0.001;
  - on an error it prints `{ "error": message }` on stdout, then exits 1 as now (stderr `floatdisk: <message>`).
- **`check_build.js`** (needs the game; prints `skip: no game at <dir>` without it). It copies the folder,
  `data/files/c_n5fd_*`, `plc_n5fd_*`, `n5t_forcedisk.tga` and `data/rows/*.floatdisk.txt` into
  `os.tmpdir()/n5_fdbuild-*`. It runs build.js there with `--tools <nwn5e-tools> --reader <this repo's mdlmesh.exe>`
  and removes the copy after. Its checks:
  - `a full rebuild of the committed composition changes nothing (13 files unchanged)`;
  - `--json says what the text says`: the stages' triangles and meshes equal, `tall` and `top` equal at 2 decimals;
  - `a weapon's pieces are read as one, end to end`: a composition whose medium is `wswls_b_011`, `_m_011`, `_t_011`
    builds with `--check --json`. Read alone through `reader.part`, the hilt's and the blade's boxes meet along the
    longest axis within 5 cm.

    If they don't, stop and report: the design's call 4 assumed a shared origin.
  - `a refused build writes nothing`: a part `plc_nope` exits 1 with `{ "error": ".*no such model" }`, and the copy's
    files keep their bytes and times.

  The last line is `build check: all pass`, or `build check: <n> failed`.

- [ ] **Step 1: Record today's output.** `node tools/data/floatdisk/build.js --check > /tmp/fd_check_before.txt`.
- [ ] **Step 2: Write `check_build.js`, then run it.** `node tools/data/floatdisk/check_build.js`. Expected: FAIL on all
  four (`build.js printed no JSON` for the first three, and the weapon also `no such model`), and `build check: 4
  failed`.
- [ ] **Step 3: Implement** `reader.js`, then build.js on it, then `--json`. README.md: `--json`, `check_build.js` and
  `reader.js` in "The files here".
- [ ] **Step 4: Run.**
  - `node tools/data/floatdisk/build.js --check > /tmp/fd_check_after.txt`, then
    `diff /tmp/fd_check_before.txt /tmp/fd_check_after.txt`. Expected: no output.
  - `node tools/data/floatdisk/check_build.js`. Expected: four `ok` lines, then `build check: all pass`.
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 26 checks pass`.
  - `git status --short`. Expected: build.js and README.md modified; reader.js and check_build.js new; nothing in `data\`.
- [ ] **Step 5: Commit.**
  - `git add tools/data/floatdisk/reader.js tools/data/floatdisk/check_build.js`;
  - `git branch --show-current` (anim);
  - `git commit --only tools/data/floatdisk/reader.js tools/data/floatdisk/check_build.js tools/data/floatdisk/build.js tools/data/floatdisk/README.md -F -`,
    with "feat: Floating Disk's generator - reader.js (the part and texture reading build.js and the VFX Bench share),
    build.js --json, check_build.js (#39)".

### Task 3: The server's Composer routes (nwn5e-tools)

Working folder: `C:\Users\Bruno\Desktop\Projects\nwn5e-tools`.

**Files:**
- Create: `vfx_bench/composer_server.js`, `vfx_bench/composer_fixture.js`, `vfx_bench/check_composer_server.js`
- Modify: `vfx_bench/server.js`:
  - the `--ee <checkout>` and `--game <folder>` options;
  - `/api/composer*` and `/composer/floatdisk.js` handed to `composer_server.js`;
  - its files and the checkout's `floatdisk.js` and `reader.js` added to the stale list `CODE` (when found).

**Interfaces:**
- **`composer_server.js`:** `create({ build, ee, game, tools })` returns `{ code(): [files for the stale list],
  handle(req, res, url, h) -> Promise<boolean> }`, where `h = { send, readBody, fromPage, writeWhole }`.
  - It loads lazily, on the first Composer request:
    - the checkout's `floatdisk.js` and `reader.js`;
    - `../spell_bench/game_data.js`;
    - the game (`--game`, else `anim_helper/helper.js` `findGameDir({ buildDir: build })`);
    - `reader.create(...)` with `ensureReader({ repo, reader: process.env.NWN5E_MDLMESH })`.

    A failure is reported by `GET /api/composer` as `{ ok: false, error }`. The effect mode never loads any of it.
- **`findCheckout({ ee, tools })`:**
  - `ee` when given;
  - else `<tools>\..\nwn5e-ee-anim` when it holds `tools\data\floatdisk\composition.json`;
  - else `<tools>\..\nwn5e-ee`.
- **`branchOf(dir)`:**
  - `.git` a folder: its `HEAD`;
  - `.git` a file `gitdir: X`: `X\HEAD`;
  - `ref: refs/heads/anim` gives `anim`, and a detached HEAD its first 7 characters;
  - no git command.
- **Routes:**
  - **`GET /api/composer`:** `{ ok, ee, branch, warnBranch: branch !== "anim", file, text, hash (sha1 of text),
    partsFolder, game }`.
  - **`GET /api/composer/catalog`:** `{ placeables, items, yours }`, each a list of `{ id, label, models | file, kind,
    search }` sorted by label.
    - **Placeables:** the models `placeables.2da`'s `ModelName` names that the archives have, one entry a model, its
      first row's `Label`, every label in `search`.
    - **Items, from `baseitems.2da`:**
      - `ModelType` 2: each number N with `<class>_b_N`, `_m_N` and `_t_N`, labelled `<label> N`, kind `weapon`, three
        models;
      - `ModelType` 0 / 1: `<class>_N`, kind `item` / `helmet`;
      - then `it_*` and `gi_*` models not already placeables, minus `gi_sound*`, `gi_waypoint*` and `gi_store`.
    - **Yours:** `parts\*.mdl` in the checkout's floatdisk folder, `{ id: "own_" + sha1(rel + mtime).slice(0, 12), file:
      "parts/<name>.mdl", label, mtime }`.
  - **`GET /api/composer/part?models=a[,b,c]`** (each a resref) or **`?file=<path>`** (resolved by `F.resolvePartFile`
    from the floatdisk folder as the generator resolves it: a `.mdl`, under `parts\` or anywhere composition.json may
    name one):
    - answers `{ key, label, meshes, lo, hi, textures: { <bitmap>: { draw: "texture" | "flat", why: "dds-only" | "plt"
      | "missing" | "", shine: bool } } }`;
    - `shine` is true when its TXI has an `envmaptexture` line (draw opaque);
    - kept in memory by key (an own file by path and mtime);
    - a bad name (not a resref, not a `.mdl`) gives 400; a missing file or a reader error 404 `{ error }`.
  - **`GET /api/composer/texture/<name>[?file=<rel>]`:** the TGA (`application/octet-stream`), or 404 `{ error, flat:
    "dds-only" | "plt" | "missing" }`.
  - **`GET /api/composer/thumb/<key>`** (`/^[a-z0-9_+]{1,64}$/`): the PNG, or 404.
  - **`POST /api/composer/thumb/<key>`** (`image/png`, from the page, up to 262144 bytes): writes
    `<build>\vfxedit\composer\thumbs\<key>.png` whole.
  - **`GET /composer/floatdisk.js`:** the checkout's file as `text/javascript`.
- **`composer_fixture.js`** (Node, for the checks):
  - `tempCheckout({ ee })` copies the floatdisk folder and the committed outputs into `os.tmpdir()/n5_composer-*/repo`.
    It writes `.git` as `gitdir: <tmp>/gitdir` with `HEAD` `ref: refs/heads/anim`, and puts `parts/n5_testcrate.mdl`
    (an ASCII 12-triangle cube, `bitmap n5_testcrate`) with a 2 x 2 `n5_testcrate.tga` beside it. It returns `{ repo,
    folder, cleanup() }`.
  - `tempBuild()` gives an empty bench build folder.

- [ ] **Step 1: Write `check_composer_server.js`** in `browser_check.mjs`'s ok / FAIL style. It prints `skip: no game at
  <dir>` without the game. It starts the server on 18776 with `--build <tempBuild> --ee <tempCheckout> --no-open` and
  `NWN5E_MDLMESH=<anim>\build\floatdisk\mdlmesh.exe`. Its steps:
  - `composerState`: ok, `ee` the copy, branch `anim`, `hash` the copy's composition.json's sha1.
  - `branchOf`: a `.git` folder whose HEAD is `ref: refs/heads/main` gives `main`; a 40-hex HEAD gives 7 characters.
  - `checkoutDefault`: `findCheckout({ tools })` is `...\Projects\nwn5e-ee-anim` (read-only).
  - `catalogKinds`:
    - placeables over 1,000, with `{ id: "plc_b04", label: "LootBag1" }`;
    - a weapon whose models are `wswls_b_011`, `wswls_m_011`, `wswls_t_011`;
    - no `gi_waypoint`;
    - Yours `parts/n5_testcrate.mdl`.
  - `partMeshesAreTheGenerators`: `part?models=plc_b04`'s meshes deep-equal `reader.create(...).part(...)`'s.
  - `weaponPieces`: three pieces' meshes, as many as the three read alone.
  - `ownFile`: 12 triangles, its texture served from beside it.
  - `badNames`: `models=../x` and `file=parts/x.txt` give 400; `file=../../nothing.mdl` gives 404.
  - `textures`:
    - `plc_rugdmtlbrk` decodes (`core.decodeTga`) at 512 x 512;
    - `tno01_post` gives 404 with flat `dds-only`;
    - `part?models=plc_c07` has `plc_gold` with shine true.
  - `thumbCache`: 404, then a POST with the page's Origin gives 200, then a GET returns the same bytes. A POST without an
    Origin gives 403.
  - `floatdiskServed`: 200, `text/javascript`, containing `placePart`.
  - `staleGuard`: the copy's `floatdisk.js` touched 5 s ahead gives `POST /api/composer/thumb/...` 409 with
    `vfx_bench.bat again`; its time is restored after.
  - `effectModeUnchanged`: `/api/state` has exactly the keys it had before this task.
- [ ] **Step 2: Run it.** `node vfx_bench/check_composer_server.js`. Expected: `composerState` FAIL (404 not found),
  the rest FAIL, the last line `<n> failed`.
- [ ] **Step 3: Implement** `composer_server.js`, `composer_fixture.js` and the server.js wiring.
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/check.js`. Expected: as before this task.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/composer_server.js vfx_bench/composer_fixture.js vfx_bench/check_composer_server.js`;
  - then `git commit --only vfx_bench/composer_server.js vfx_bench/composer_fixture.js vfx_bench/check_composer_server.js vfx_bench/server.js -F -`,
    with "feat: the VFX Bench's Composer, the server - the checkout, the parts list, a part's meshes through the
    generator's reader, textures, thumbnails (nwn5e-ee #39)".

### Task 4: The server's Save (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/composer_server.js`, `vfx_bench/check_composer_server.js`

**Interfaces:**
- **`POST /api/composer/save`** (`application/json`, from the page) takes `{ composition, base, force? }`:
  1. `F.readComposition(composition, folder)`. If it throws: 400 `{ error }`.
  2. If composition.json's text no longer hashes to `base` and `force` isn't set: 409 `{ error: "composition.json changed
     on disk since you opened it", changed: true }`.
  3. If a save is running: 409 `{ error: "a save is running" }`.
  4. It writes `F.compositionText(composition)` to `os.tmpdir()/n5_composer-<n>.json`.
  5. It runs `node <folder>/build.js --json --composition <temp> --tools <nwn5e-tools> --game <game> --reader <exe>`
     (`execFile`, cwd the checkout, 120 s).
  6. **Exit 0:** `writeWhole(composition.json, text)`, the temp file deleted, then 200 `{ ok: true, result, hash, text }`.
  7. **Otherwise:** the temp file deleted, then 422 `{ error: <its "floatdisk: ..." line>, output }`. composition.json is
     untouched.

- [ ] **Step 1: Add steps** to `check_composer_server.js` (on the copy):
  - `saveWritesAndRebuilds`: Medium's first part turned 30 more degrees gives 200. The copy's composition.json equals
    `compositionText` of what was posted. `result.files` has `rebuilt` for `data/files/c_n5fd_med.mdl` and
    `plc_n5fd_med.mdl`, and `unchanged` for the other 11.
  - `saveUnchangedIsUnchanged`: the file's own composition gives all 13 unchanged.
  - `refusedBuildChangesNothing`: a part `plc_nope` gives 422 matching `/no such model/`. composition.json's bytes and
    `data/files`' times are unchanged.
  - `invalidRefused`: `scale: 0` gives 400 matching `/scale/`, and nothing changed.
  - `changedFileRefused`: the copy's file rewritten (a note changed) gives 409 `changed`; then `force: true` gives 200.
  - `saveFromElsewhereRefused`: no Origin gives 403.
  - `realWorktreeUntouched`: the real anim worktree's `composition.json` sha1 and `data/files/c_n5fd_*.mdl` times are
    the same at the end of the check as at its start.
- [ ] **Step 2: Run it.** `node vfx_bench/check_composer_server.js`. Expected: the new steps FAIL (404), the last line
  `6 failed`. `realWorktreeUntouched` passes already.
- [ ] **Step 3: Implement** the route.
- [ ] **Step 4: Run** `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
- [ ] **Step 5: Commit** `vfx_bench/composer_server.js vfx_bench/check_composer_server.js` with "feat: the VFX Bench's
  Composer saves - composition.json replaced only after the generator rebuilt from it, a file changed on disk refused
  (nwn5e-ee #39)".

### Task 5: The page's pure parts - public/composer.js (nwn5e-tools)

**Files:**
- Create: `vfx_bench/public/composer.js` (UMD, `root.VfxComposer`), `vfx_bench/check_composer.js`
- Modify:
  - `vfx_bench/server.js`: `FILES` and `CODE` gain `composer.js`;
  - `vfx_bench/check_composer_server.js`: `summaryMatchesGenerator`.

**Interfaces:**
- **`VfxComposer.with(F)`** binds the checkout's FloatDisk and gives the functions below. In Node, F is
  `require(<ee>/tools/data/floatdisk/floatdisk.js)`, with `--ee` defaulting to `../../nwn5e-ee-anim`, as
  browser_check.mjs's does.
- **The composition and its parts:**
  - `keyOf(raw)`: `"model:x"`, `"models:a+b+c"` or `"file:<rel>"` (the server's part key).
  - `norm(raw)`: `{ at, turn: 0, scale: 1, tilt: [0, 0] }` defaults.
  - `clean(raw)`: rounds `at` to 0.001, `turn` and `tilt` to 0.1, `scale` to 0.001. It drops a `[0, 0]` tilt and keeps
    the note and unknown keys.
  - `previous(stage)`: `empty` for small, `small` for medium, `medium` for large, `null` for empty.
- **The stage:**
  - `placed(comp, stage, models)`: `[{ index, raw, meshes (world), lo, hi, reach, triangles }]` through
    `F.placePart(norm(raw), models.get(keyOf(raw)), i, comp.disk.height, "j")`. A part whose model isn't loaded is
    skipped (`missing` listed).
  - `summary(comp, stage, models)`: `{ parts, meshes, triangles, tall, top, list: [{ label, size, from, to, past }] }`,
    as build.js `--json` gives it, the disk's 160 triangles included.
  - `ghosts(comp, stage)`: the previous stage's parts. `copyPrevious(comp, stage)` appends clones of them.
- **Picking and resting:**
  - `pick(ray, placedList)`: the index of the nearest part whose triangle the ray hits (Moller-Trumbore), or -1.
  - `restHeight(moving, others, disk)`: the `at` z (0 or more) at which `moving` rests on the highest surface under its
    vertices and its triangles' centres.
    - `moving` is placed with z 0;
    - `others` are placed;
    - a surface is the disk's top or another part's triangle seen from above.
- **The handles:**
  - `moveBy(start, from, to)`: points on the foot's plane give the new `at` x and y.
  - `ringOf(placed)`: `{ centre: [at.x, at.y, lo.z], radius: half the footprint's diagonal + 0.03 }`.
  - `turnBy(start, centre, from, to, step)`: degrees. `step` 15 rounds.
  - `scaleBy(start, centre, from, to)`: clamped 0.05 to 4.
  - `liftBy(startZ, z0, ray, centre)`: the closest point between the ray and the vertical through the centre, 0 or
    more.
- **New parts:**
  - `newScale(lo, hi)`: `min(1, 0.35 / max(dx, dy))`, rounded 0.001.
  - `layFlat(lo, hi)`: `[0, 90]` when z is the box's longest side, else `[0, 0]`.
  - `partFrom(entry, [x, y], box)`: a new raw part `{ note: entry.label, model | file, at: [x, y, 0], turn: 0, scale:
    newScale, tilt: layFlat for a weapon }`.
- **Undo and the frame:**
  - `changedStage(a, b)`: the first stage whose parts differ (`empty` for a disk change), or null.
  - `snapshot(comp)`: `JSON.stringify(comp)`.
  - `frameBox(lo, hi)`: a camera `{ yaw, pitch, distance, target }` fitting the box at a 3/4 view (for thumbnails).

- [ ] **Step 1: Write `check_composer.js`** (ok / FAIL lines, last line `all composer checks pass`). Synthetic cubes,
  no game:
  - `composer.js loads under node and in a page`;
  - `placed equals placePart`;
  - `the stage summary as build.js prints it` (two cubes: tall, top, past);
  - `pick: the nearest part under the ray, none off it`;
  - `rest on the disk: z 0`;
  - `rest on a part: its top under the footprint`;
  - `rest beside a part: back on the disk`;
  - `rest on the higher of two`;
  - `move keeps the grab offset`;
  - `turn by the angle swept; step 15 rounds`;
  - `scale by the distance ratio, clamped`;
  - `lift along the vertical, never below the disk`;
  - `layFlat lays a standing weapon down`;
  - `a new part's scale keeps it within 0.35 m`;
  - `ghosts are the previous stage's parts`;
  - `copyPrevious appends clones`;
  - `clean rounds and drops a zero tilt`;
  - `changedStage names the stage an undo shows`;
  - `frameBox keeps the box in view`;
  - `the composition round trip`: the real composition.json opened, then through `F.compositionText`, is the file but
    for `0.0`.

  In `check_composer_server.js`, `summaryMatchesGenerator`: for each stage of the copy's composition, `summary` with the
  server's part meshes equals `build.js --check --json` on the copy. Triangles and meshes are exact; tall, top, sizes and
  past are within 0.001.
- [ ] **Step 2: Run** `node vfx_bench/check_composer.js`. Expected: `FAIL composer.js loads ...: Cannot find module
  './public/composer.js'`, the rest FAIL, then `<n> failed`. `node vfx_bench/check_composer_server.js`. Expected:
  `summaryMatchesGenerator` FAIL.
- [ ] **Step 3: Implement** `public/composer.js`.
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/public/composer.js vfx_bench/check_composer.js`;
  - then commit those with `vfx_bench/server.js vfx_bench/check_composer_server.js`, with "feat: the VFX Bench's
    Composer, its pure parts - placement through the generator's placePart, the stage's summary, picking, resting on
    what's below, the handles' maths (nwn5e-ee #39)".

### Task 6: The stage - one mesh path, pointer handlers, the ray (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/view.js`, `vfx_bench/check_chunks.js`, `vfx_bench/check_composer.js`

**Interfaces:**
- **`meshData(mesh, place)`:** chunkMeshes' per-particle vertex data (x y z u v r g b a; shade by `CHUNK_LIGHT`; an
  untextured mesh in its material), factored out. `chunkMeshes` calls it per particle; its output doesn't change.
- **`worldMeshes(list)`:** the same data for meshes already placed in the world (the Composer's). Each item may carry:
  - `opaque`: the texture's alpha ignored;
  - `ghost`: drawn in a pale tint at alpha 0.25, blended, no depth write.
- **The fragment shader** gains `u_opaque` (`if (u_opaque > 0.5) c.a = v_color.a;`).
- **`View.drawMeshes(list, cam)`:** opaque and cut-out meshes first (depth written, cut at 0.2 unless opaque), then those
  under alpha 1 (blended, no depth write), then ghosts. `draw()` uses it for chunks.
- **`View.drawScene({ figure, meshes, see, ghosts, lines })`:** the Composer's frame. It draws the ground and the figure
  (`figure: { x: -0.85, y: -0.15, heading: 0 }`), the meshes, then `see` (the disk), then the ghosts, then the handle
  lines with the depth test off.
- **`View.putTexture(name, image)`:** a texture set from an image (the disk's, made in the page).
- **Pointer:** `view.handlers = { grab(x, y, ev) -> bool, drag(x, y, ev), drop(ev), click(x, y, ev) }`.
  - On a left pointerdown, `grab` answering true makes the drag the page's (after 3 px; without a move it's a `click`).
  - Otherwise the drag orbits as now, and an orbit that didn't move is a `click`.
  - `view.minDistance` is 1 by default; the Composer sets 0.6.
- **Pure helpers:** `rayAt(camera, w, h, x, y)` gives `{ origin, dir }`; `projectAt(camera, w, h, p)` gives `[x, y,
  depth]`. `View.ray(x, y)` and `View.project(p)` use the canvas's size.

- [ ] **Step 1: Add checks:**
  - `check_chunks.js`: `a world mesh draws as a still chunk`. `worldMeshes` of a mesh equals `chunkMeshes` of one still
    particle at the origin with Spin 0, value for value.
  - `check_composer.js`:
    - `rayAt and projectAt round trip`: a point projected and then rayed passes within 1e-6 of it;
    - `the centre ray is the camera's forward`.
- [ ] **Step 2: Run.** `node vfx_bench/check_chunks.js`. Expected: `FAIL a world mesh draws as a still chunk:
  V.worldMeshes is not a function`. `node vfx_bench/check_composer.js`. Expected: the two new steps FAIL.
- [ ] **Step 3: Implement** in view.js.
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_chunks.js`. Expected: `all chunk checks pass`, every earlier step included.
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed` (`chunkDrawsItsMesh`, `chunkUntexturedDrawsItsMaterial`
    included).
- [ ] **Step 5: Commit** `vfx_bench/public/view.js vfx_bench/check_chunks.js vfx_bench/check_composer.js` with "feat: the
  VFX Bench's stage - one mesh path for chunks and placed meshes, pointer handlers for handles, a ray from the pointer
  (nwn5e-ee #39)".

### Task 7: The mode - the frame's parts, the tabs, the onion skin, the cards, undo (nwn5e-tools)

**Files:**
- Create: `vfx_bench/public/composer_page.js` (`root.VfxComposerPage`), `vfx_bench/composer_check.mjs`
- Modify: `vfx_bench/public/index.html`, `vfx_bench/public/style.css`, `vfx_bench/public/app.js`, `vfx_bench/server.js`
  (`FILES` and `CODE`: `composer_page.js`)

**Interfaces:**
- **index.html:**
  - the bar: `#composer-open` "Composer", and `#composer-save` "Save and rebuild" and `#composer-close` "Back to effects"
    (Composer only);
  - the drawer: `#parts` (filled in Task 8);
  - the panel: `#comp-card` (what's open, "Saving to <folder> (<branch>)" with a warning off `anim`, the last result),
    `#disk-card`, `#part-card`, `#stage-card`;
  - the stage: `#stage-tabs`;
  - the footer: `#comp-foot` (`#comp-onion`, `#comp-rest`, `#comp-view-34`, `-side`, `-top`, `-front`, `#comp-strip`);
  - `body.composer` shows the `.composer-only` parts and hides the `.effect-only` ones. No existing id changes.
- **composer_page.js:** `VfxComposerPage.mount({ view, image, setStatus, getJson, postJson, history: { push, clear,
  setDragging } })` returns `{ enter(stage?), leave(), frame(dt), snapshot(), restore(text), dirty(), save(), key(ev) ->
  bool, hook() }`.
  - `enter` loads `/composer/floatdisk.js` (a script tag, once), `GET /api/composer` and the stage's part meshes.
  - It places the camera at the 3/4 view and draws through `view.drawScene`.
- **app.js:**
  - `state.composer`;
  - the Composer button runs `leaveOk()`, then `enter`;
  - `tick` hands the frame to the Composer while it's open, as the area mode does;
  - `snapshot()` / `restoreSnapshot()` carry `{ composer }`;
  - `save()`, `leaveOk()`, `beforeunload` and `keydown` (the Composer's `key` first) go to the Composer while it's open;
  - `?compose=1[&stage=<stage>]` opens it at start;
  - Back to effects restores the effect as it was and clears the history.
- **The tabs:** Empty, Small N, Medium N, Large N. A switch isn't an undo step.
- **The onion skin:** `ghosts()` drawn with `ghost` set.
- **The disk card:** diameter, thickness, height, sides, colour, rim, alpha, glow. It sets `comp.disk` (rounded), and
  the disk mesh and `n5t_forcedisk` (`F.diskTexture`, `core.decodeTga`, `view.putTexture`) follow live.
- **The strip** (`#comp-strip`): a chip per part of the stage, by its note, in order. A click selects it and the part
  card shows it (Task 9 adds selecting on the stage).
- **The part card:** note, x, y, height, turn, scale, tilt x and y; one undo step on change.
- **Undo:** `restore` switches to `changedStage(now, restored)` when there is one, and keeps the selection by index
  (clamped).
- **`window.vfxBench.composer()`:** `{ open, stage, parts: [{ key, at, turn, scale, tilt, note }], selected, ghosts,
  drawn: { parts, triangles, textured, flat }, disk, dirty, ee, branch, screenOf(i), handle(kind), last }`.
  - `screenOf(i)` gives page px of part i's centre;
  - `handle(kind)` gives page px of the selected part's `"ring"`, `"scale"` or `"lift"` handle;
  - `last` is the last save's answer.

- [ ] **Step 1: Write `composer_check.mjs`** (headless Chrome as browser_check.mjs; port 18778; `tempBuild` and
  `tempCheckout` from `composer_fixture.js`; `NWN5E_MDLMESH`; `window.confirm` stubbed to answer true). It prints
  `skip: no Chrome or Edge` or `skip: no game at <dir>` when it can't run. Its steps:
  - `composerOpens`: `?compose=1&stage=medium` gives `open`, stage `medium`, and `drawn.parts` 7 with `textured` true.
  - `tabsAndCounts`: the tab texts are `Empty`, `Small 2`, `Medium 7`, `Large 12`.
  - `onionSkin`: on Large, `ghosts` 7; Onion skin off gives 0; on Small, 0.
  - `emptyIsTheDisk`: Empty shows `#disk-card`. Diameter 1 gives `disk.diameter` 1, and on Medium the disk mesh reaches
    0.5.
  - `partFieldsEdit`: the strip's first chip selected, Turn 45 gives `parts[0].turn` 45. Ctrl+Z gives 12 back, and
    Ctrl+Y 45.
  - `undoShowsTheStage`: an edit on Large, a switch to Small, then Ctrl+Z gives stage `large` and the edit undone.
  - `backToEffects`: `body.composer` is gone, `vfxBench.fx()` is the effect as before, and `#undo` is disabled.
  - `frameNoPageScroll` (in the Composer).
  - `no errors in the page`.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: `composerOpens` FAIL (`vfxBench.composer is not a
  function`), the rest FAIL, then `<n> failed`.
- [ ] **Step 3: Implement** composer_page.js, the HTML and CSS, and the app.js wiring.
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  - `node common/checks/run.js`. Expected: `all common checks pass`.
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/public/composer_page.js vfx_bench/composer_check.mjs`;
  - then commit those with `vfx_bench/public/index.html vfx_bench/public/style.css vfx_bench/public/app.js
    vfx_bench/server.js`, with "feat: the VFX Bench's Composer mode - the four stage tabs, the onion skin, the disk on
    the empty stage, a part's card, undo across stages (nwn5e-ee #39)".

### Task 8: The parts list and its thumbnails (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/index.html`, `vfx_bench/public/style.css`,
  `vfx_bench/composer_check.mjs`

**Interfaces:**
- **`#parts`:**
  - tabs Placeables / Items / Yours, with counts;
  - `#parts-search`, matching each entry's `search`, label and model names;
  - `#parts-list`: an entry is a thumbnail, its label, its model(s) and its kind;
  - Yours' note with the folder, and `#parts-reload`.
- **Thumbnails:**
  - entries coming into view (an IntersectionObserver) queue, four at a time;
  - `GET thumb/<id>` first;
  - on a 404, the page fetches the part's meshes, waits for its textures, and draws them on one offscreen 128 x 128
    canvas of its own (a second `View`, no ground or figure, `frameBox`'s camera);
  - the PNG is posted, then shown.
- **Adding a part:**
  - a click adds `partFrom(entry, [0, 0], box)` on the current stage (not Empty), rests it (when Rests is on) and selects
    it, one undo step;
  - an entry dragged onto the stage (`dragover` / `drop` on `#canvas`) adds it at the ray's point on the disk's top
    plane.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `listKinds`:
    - Placeables count over 1,000;
    - "LootBag1" finds `plc_b04` first;
    - Items with "longsword" finds entries of three models;
    - Yours lists `n5_testcrate`.
  - `thumbnailDrawnAndKept`: the first entry's thumbnail is set, and `<temp build>\vfxedit\composer\thumbs\<id>.png`
    exists.
  - `addByClick`: on Medium, LootBag1 gives 8 parts. The new one is selected, at x 0 y 0, its height the surface there,
    and its footprint within 0.35 m.
  - `addByDrag`: LootBag3 dropped on a stage point lands within 2 cm of the disk-plane point under the drop.
  - `weaponLiesFlat`: "longsword 011" added has tilt `[0, 90]`, and its box's z is no longer its longest side.
  - `undoAdd`: Ctrl+Z removes the last add, one step each.
  - `ownFileAdds`: Yours' `n5_testcrate` added is drawn textured.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL (no `#parts-list` entries).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** the four files with "feat: the VFX Bench's Composer, the parts list - the game's placeables
  and items and your own files, searched, with thumbnails drawn once and kept; added by a click or dragged onto the
  stage (nwn5e-ee #39)".

### Task 9: The handles (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/view.js` (only if the handle lines need more than
  `lines`), `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Selection:** a click on a part (`pick`) or on its strip chip selects it; a click on empty space or Esc deselects.
- **The selected part's lines:**
  - its box;
  - the ring (64 segments at `ringOf`);
  - a 3 cm square on the ring at 45 degrees;
  - the lift arrow from 5 to 20 cm above its top.
- **`grab`:**
  - within 8 px of the square: scale;
  - within 7 px of the ring: turn;
  - within 8 px of the arrow: lift;
  - else `pick` on the selected part: move;
  - else no grab (the view orbits).
- **A drag:** `setDragging(true)` on the grab, then `moveBy`, `turnBy` (Shift: 15), `scaleBy` or `liftBy` on each move.
  While Rests is on, a move or a scale rests the part again (`restHeight` against the stage's other parts). On drop,
  `setDragging(false)` and one push.
- **Buttons:** Drop it (rests the part), Duplicate and Remove in the part card.
- **Keys:** Delete removes; Ctrl+D duplicates (0.05 m along x, rested); the arrows nudge 0.01 m (Shift 0.05 m); Esc
  deselects. They're ignored while typing in a field.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `clickSelects`: `screenOf(3)` clicked gives `selected` 3; empty space gives null.
  - `dragMoves`: a part dragged 80 px right has its x grown, and its height is the surface under it.
  - `restsOnPart`: on Medium, the small bag moved over the crate's centre has its height at the crate's top (0.31 m)
    within 0.01.
  - `ringTurns`: a quarter of the ring dragged gives +90 within 3.
  - `shiftTurnSteps`: with Shift, the turn is a multiple of 15.
  - `squareScales`: the square dragged out to 1.5 times its distance gives scale x1.5 within 0.05.
  - `arrowLifts`: 40 px up raises the height; Drop it returns it to the surface.
  - `restOffKeepsHeight`: with Rests off, a lifted part moved keeps its height.
  - `deleteAndDuplicate`: Delete gives n-1, Ctrl+Z n, Ctrl+D n+1 at +0.05 x.
  - `nudges`: ArrowRight gives x +0.01.
  - `oneDragOneUndo`: a drag, then Ctrl+Z, gives exactly the `at` before the drag.
  - `emptyDragOrbits`: a drag on empty space changes the camera's yaw and no part.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL (`selected` stays null).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** the changed files with "feat: the VFX Bench's Composer, the handles - move on the disk,
  the ring turns, the square scales, the arrow lifts; a part rests on what's below (nwn5e-ee #39)".

### Task 10: Save and rebuild in the page (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/app.js` (if `save()` routing needs more than Task 7
  gave), `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Save and rebuild:** `POST /api/composer/save { composition, base }`.
  - **200:** the status says "Saved and rebuilt in <folder> (<branch>): <files rebuilt>, <n> unchanged. For the game:
    the animation session commits it on anim; the game session merges and installs." The Composer card shows each
    stage's parts, triangles and height from `result`. The page isn't dirty after, and `base` is the new hash.
  - **409 `changed`:** "composition.json changed on disk since you opened it: save over it?" Yes posts again with
    `force`. No leaves it, and **Reload** (in the Composer card) reads the file again, after asking about unsaved
    changes.
  - **422 / 400:** the status says "Not rebuilt: <message>. Nothing was written." in the bad style.
- **Dirty:** the composition's snapshot differs from the one last opened or saved. The title starts `•`, and leaving or
  reloading asks.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `saveRebuilds`: a part moved on Medium, then Save, gives a good status. The copy's composition.json parses to the
    page's composition. `last.result.files` has `c_n5fd_med.mdl` rebuilt, and `dirty` is false.
  - `saveRefusedSaysWhy`: the fixture's `n5_testcrate.mdl` added, then deleted from disk, then Save, gives a bad status
    matching `/no such file/`. The copy's composition.json is unchanged.
  - `changedOnDiskAsks`: the check rewrites the copy's composition.json, then Save with `confirm` answering false. The
    status says it changed, and nothing is written.
  - `reloadReadsTheFile`: Reload shows the file's composition.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer, Save and rebuild - the generator's summary, a changed
  file asked about, a refused build shown and nothing written (nwn5e-ee #39)".

### Task 11 (only if the owner says yes to Q1): DDS textures (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/core.js` (`decodeDds`), `vfx_bench/composer_server.js` (the texture route),
  `vfx_bench/public/composer_page.js`, `vfx_bench/check_composer.js`, `vfx_bench/check_composer_server.js`,
  `vfx_bench/composer_check.mjs`

**Interfaces:**
- **`core.decodeDds(bytes)`** gives `{ width, height, data }` (RGBA, rows top-down as `decodeTga`'s), mip 0 only. It
  reads:
  - Bioware's 20-byte header (u32 width, u32 height, u32 bytes a pixel: 3 is DXT1, 4 is DXT5; u32 size; f32);
  - a standard `DDS ` header with FourCC `DXT1` / `DXT5`.
- **`GET /api/composer/texture/<name>`** serves the DDS when the game has one (what the game draws, often the bigger
  copy), with the header `X-Texture: dds`, else the TGA. The `flat` reason `dds-only` goes.
- **The page** decodes by that header.

- [ ] **Step 1: Add checks:**
  - `check_composer.js`:
    - `decodeDds: a DXT1 block` (colour0 0xF800, colour1 0x001F, indices 0, gives 4 x 4 red);
    - `decodeDds: a DXT5 block's alpha` (alpha0 255, alpha1 0, indices 1, gives alpha 0);
    - `decodeDds: Bioware's header and the standard one`.
  - `check_composer_server.js`: `ddsServed` (`tno01_post` gives 200 `dds`, 256 x 256).
  - `composer_check.mjs`: `ddsDraws` (Large's barrel `tnp_barrel_002` drawn textured).
- [ ] **Step 2: Run** the three. Expected: the new steps FAIL (`core.decodeDds is not a function`; 404).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the three, and `node vfx_bench/check.js` (every library texture still decodes). Expected: all
  pass.
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer reads DDS textures - DXT1 and DXT5, the copy the game draws
  (nwn5e-ee #39)".

### Task 12 (only if the owner says yes to Q2): Play in game for a stage (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/app.js` (Play / Stop in the Composer),
  `vfx_bench/server.js` (`/api/play` takes up to 1 MB for a Composer draft), `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Play in game** in the Composer posts the stage as a draft, `core.effect({ name: "n5_fdcompose", duration: true,
  seconds: 10, attach: "root", planes })`:
  - one `core.plane` per placed mesh and the disk, each moved +0.85 m along x and +0.15 m along y (beside the
    character, where the stage shows the figure);
  - with the bench's Permanent as the effect mode does.
- **Stop in game** is as now. The status follows the game's answer as the effect mode's does.

- [ ] **Step 1: Add** `playSendsTheStage` to `composer_check.mjs`. On Large, Play gives the temp build's
  `vfxedit\bench\request.vfx` starting `request = play`. Its draft reads (`core.readDraft`) with 31 planes (the pile's
  30 and the disk) and 1095 triangles, as `c_n5fd_large` has.
- [ ] **Step 2: Run.** Expected: FAIL (no request written).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `node vfx_bench/composer_check.mjs` and `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  The in-game look is the owner's test (CO-6).
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer plays a stage in game - the pile beside your character
  through the dev layer, before any install (nwn5e-ee #39)".

### Task 13: The READMEs, TOOLS-PLAN and the owner's tests

**Files:**
- Modify:
  - `nwn5e-tools/vfx_bench/README.md`: a "Composer" section, the options `--ee` / `--game`, and the new files in
    "Files";
  - nwn5e-ee-anim `tools/data/floatdisk/README.md`: "Compose it in the VFX Bench" at the top;
  - nwn5e-ee-anim `docs/TOOLS-PLAN.md`: a section, "The VFX Bench's Composer, built".
- Create: the owner's test rows, a CSV in the job's tmp folder; the parent session adds them to the board. Each row
  stands on its own: the bench restart, what to do, what to see.
  - **CO-1, open it:** close the VFX Bench window and double-click `vfx_bench.bat`, then press Composer. Expect the disk
    and Small's two bags beside the figure, the tabs `Empty / Small 2 / Medium 7 / Large 12`, and "Saving to
    ...nwn5e-ee-anim (anim)".
  - **CO-2, build:** on Medium, search "barrel", click it, drag it, turn it with the ring, stack a bag on it.
  - **CO-3:** the onion skin on Large, and the disk's colour on Empty.
  - **CO-4:** Undo and Redo across stages.
  - **CO-5:** Save and rebuild, read the summary, and tell the animation session.
  - **After the install:** the disk in game wears your piles. CO-6 is Q2's Play in game, if built.

- [ ] **Step 1: Run every check once more:**
  - nwn5e-ee-anim:
    - `node tools/data/floatdisk/check.js` (`floatdisk check: 26 checks pass`);
    - `node tools/data/floatdisk/check_build.js` (`build check: all pass`).
  - nwn5e-tools:
    - `node vfx_bench/check_composer.js`;
    - `node vfx_bench/check_composer_server.js`;
    - `node vfx_bench/composer_check.mjs`;
    - `node vfx_bench/browser_check.mjs`;
    - `node vfx_bench/check.js`;
    - `node vfx_bench/check_area.js`;
    - `node vfx_bench/check_parts.js`;
    - `node vfx_bench/check_effect.js`;
    - `node vfx_bench/check_beats.js`;
    - `node vfx_bench/check_chunks.js`;
    - `node common/checks/run.js`.

  Expected: all pass.
- [ ] **Step 2: Write** the READMEs, the TOOLS-PLAN section and the CSV.
- [ ] **Step 3: Commit.**
  - In nwn5e-tools: `vfx_bench/README.md`, with "docs: the VFX Bench's Composer in the README (nwn5e-ee #39)".
  - In the anim worktree, after `git branch --show-current` (anim): `tools/data/floatdisk/README.md docs/TOOLS-PLAN.md`,
    with "docs: the Floating Disk's piles are composed in the VFX Bench - README, TOOLS-PLAN (#39)".
- [ ] **Step 4: Report** the commits and the test rows. The bench changes need a restart of `vfx_bench.bat`. The generator
  changes reach the game session with the next "anim ready". They change nothing it builds, so no install is needed for
  them.
