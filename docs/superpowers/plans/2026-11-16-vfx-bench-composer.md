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

---

## Addendum 2026-11-16: a general Composer

**Spec:** the design's "Addendum 2026-11-16: a general Composer". The owner said "go with A and those outputs" and yes to
Q1 and Q2.

This addendum replaces the task list above. **Build from the final order below, taking each task whole from here.** A1
and A2 are Tasks 1 and 2 above, unchanged. Every other task's full text is in this addendum.

### What happens to the tasks above

| task above | now |
|---|---|
| 1 floatdisk.js in a page, model lists, compositionText | **kept**: A1 (take Task 1's text above as it is) |
| 2 reader.js, build.js --json, check_build.js | **kept**: A2 (Task 2's text above) |
| 3 the server's routes | **changed**: A5 (projects) |
| 4 the server's Save | **changed**: A6 (per kind; spell meshes listed as chunks) |
| 5 the page's pure parts | **changed**: A7 (projects, variants, the guide, the disk's adapter) |
| 6 the stage | **changed**: A9 (no disk; the mesh texture loader) |
| 7 the mode | **changed**: A10 (projects, variants, the guide card replaces the disk card) |
| 8 the parts list | **changed**: A11 |
| 9 the handles | **changed**: A12 (plus the use point) |
| 10 Save in the page | **changed**: A13 |
| 11 DDS (was only on a yes) | **changed, now core**: A14 |
| 12 Play in game (was only on a yes) | **changed, now core**: A15 (any kind's variant) |
| 13 docs and tests | **changed**: A17 |
| none | **new**: A3 the project generator; A4 planes that fade (C++); A8 core.js's planes; A16 mesh beats |

### Global constraints (added)

The plan's Global Constraints hold for every task here, plus these:

- **A4 runs `test.bat`** in the anim worktree with `NWN5E_PRIVATE` unset. The worktree has no `private\`, and nothing reads
  `private/`. Its last line is `<n> tests, <failed> failed`.
- **A4 changes `src/data/vfx_model.*`**, the VFX reader and writer (the animation session's, as 7b3d4b5 was). The next
  "anim ready" names it. Its fades reach the game only with the game session's install of a DLL built from it.
- **No check writes the real worktree's `tools/data/composer/` or `data/`, or the bench's real `vfxedit\`.** Every check
  works on temporary copies (`vfx_bench/composer_fixture.js`, `tools/data/composer/check_build.js`).
- **Placeable rows come only from 14100-14199.**
- **Ports:** the server check uses 18776, the Composer's browser check 18778, and the effect mode's `browser_check.mjs`
  18766.

### Review Focus (added)

8. **A placeable's row is free everywhere:** `data/rows`, `data/looks`, the game's table and the served table. Tests:
   A3 `a row another rows file sets is refused`, A6 `rowTakenElsewhereSkipped`.
9. **Blocks is the guide's outline as material 7 faces;** Walk-through has no ground mesh. Test: A3's pwk checks.
10. **A plane without the new keys writes the draft and the model it wrote before.** Tests: A4
    `VfxModelPlaneWithoutKeysUnchanged`, A8 `plane beat: the defaults write no new lines`.
11. **A mesh beat is dark before its start and fades as a light does,** on the bench and in its keys. Tests: A16
    `meshKeys`, `meshBeatDrawn`.
12. **The Floating Disk's adapter gives back composition.json** (but for `0.0`), and its 13 files stay unchanged.
    Tests: A7 `the disk's adapter round trip`, A2 / A3 `check_build.js`.

### Final order

1. **A1** (Task 1): floatdisk.js in a page, model lists, compositionText. Anim.
2. **A2** (Task 2): reader.js, build.js --json, check_build.js. Anim.
3. **A3:** the project generator: a placeable's model, use point, walkmesh and rows; a spell mesh's draft. Anim.
4. **A4:** planes that fade: the draft's plane keys and the model writer's alphakey. Anim, C++, test.bat.
5. **A5:** the server's Composer routes: projects, the parts list, parts, textures, thumbnails.
6. **A6:** the server's Save per kind; spell meshes listed as chunks.
7. **A7:** the page's pure parts: projects, variants, the guide, the disk's adapter.
8. **A8:** core.js's planes as mesh beats (A4's draft keys, byte for byte).
9. **A9:** the stage: one mesh path, the mesh texture loader, pointer handlers, the ray.
10. **A10:** the mode: projects, variants, the guide card, the kind's card, the part card, undo.
11. **A11:** the parts list and its thumbnails.
12. **A12:** the handles and the use point.
13. **A13:** Save and build in the page.
14. **A14:** DDS textures.
15. **A15:** Play in game for a variant.
16. **A16:** mesh beats in the effect editor.
17. **A17:** the READMEs, TOOLS-PLAN and the owner's tests.

---

### Task A3: The project generator (nwn5e-ee, anim)

Working folder: the anim worktree (`/c/Users/Bruno/Desktop/Projects/nwn5e-ee-anim`). Needs A1 and A2.

**Files:**
- Create: `tools/data/composer/project.js`, `tools/data/composer/build.js`, `tools/data/composer/check.js`,
  `tools/data/composer/check_build.js`, `tools/data/composer/README.md`
- Modify: `tools/data/floatdisk/floatdisk.js`, `tools/data/floatdisk/check.js`

**Interfaces:**
- **floatdisk.js:** each new argument's default gives today's bytes.
  - `readPart(p, where, baseDir)`: one part read and checked, factored out of `readComposition`, with the same errors.
  - `pileModelText(name, meshes, lo, hi, about, mark = MARK)`.
  - `pileWalkmeshText(name, lo, hi, { mark = MARK, use = null, block = null } = {})`:
    - `use` is `[[x, y], ...]`; null gives today's two use points.
    - `block` is `[[x, y], ...]`, a convex outline. Before the use points it writes, as the game's PLC_A03.pwk has it:
      - `node trimesh <NAME>_wg` (the model's name upper-cased) under `<name>_pwk`;
      - `position 0 0 0`, `orientation 0 0 0 0`, `bitmap NULL`;
      - `verts`: the outline at z 0;
      - `faces`: a fan, each face `a b c  1  0 0 0  7` (material 7, non-walkable; counter-clockwise from above);
      - `endnode`.
    - The use points are named `<short>_pwk_use01`, `_use02`...
  - `rowsText(table, sourceRow, entries, { ..., mark = MARK })`.
  - `carriesMark(bytes, mark = MARK)`.
- **project.js** is UMD (`root.ComposerProject`), with no `require` at its top, as floatdisk.js after A1.
  - **Constants:**
    - `MARK = "# made by nwn5e-ee tools/data/composer/build.js from a Composer project - change it in the VFX Bench and
      save, not this file"`;
    - `KINDS = ["placeable", "spellmesh"]`;
    - `NAME = /^[a-z0-9]{1,8}$/`, `VARIANT = /^[a-z0-9]{1,4}$/`;
    - `ROWS = [14100, 14199]`, `SOURCE_ROW = 189`.
  - **`readProject(json, baseDir, F)`** gives `{ kind, name, guide: { shape, diameter, width, length, height },
    placeable: { copy, use, walk } | null, variants: [{ name, model, row, parts }] }`.
    - Parts are read through `F.readPart`.
    - Each error names its field: `guide.diameter: a number above 0`, `variants[1].name: 1-4 letters or digits,
      unique`, `variants[0].row: 14100-14199`, `placeable.walk: through or blocks`, `variants[0].parts[2].scale: a number
      above 0`.
  - **`modelName(kind, name, k, count)`:** `plc_n5<name>` / `n5m_<name>`, with `_<k>` (from 1) when `count` > 1.
  - **`labelOf(name, k, count)`:** `N5_<Name>` / `N5_<Name>_<k>`.
  - **`outline(guide, sides = 16)`:** a circle's 16 points at its radius (the first at +x, counter-clockwise); a
    rectangle's 4 corners (`width` along x, `length` along y); `[]` with no guide.
  - **`defaultUse(guide)`:** `[0, -(front + 0.1)]`, where `front` is the radius or half the length; `[0, -0.5]` with no
    guide.
  - **`newScale(lo, hi, guide)`:** `min(1, 0.4 x span / max(dx, dy))`, where `span` is the diameter or the longer side,
    rounded 0.001; 1 with no guide.
  - **`newProject(name, kind)`:** the JSON of a new project:
    - `_about`, `kind`, `name`;
    - a circle guide at 0, 1 m for a placeable and 0.5 m for a spell mesh;
    - for a placeable, `placeable: { copy: null, use: null, walk: "through" }`;
    - one variant `{ "name": "1", "parts": [] }`.
  - **`projectText(json)`:** compositionText's layout (two-space indents, `_about` a line each, a part a line, LF, a
    final newline).
  - **`rowsUsed(text)`:** the rows a rows file sets (a line starting with a number that has a value other than `****`).
  - **`freeRows(taken, count, own = [])`:** keeps `own`, and gives the rest from the lowest free of `ROWS`. Short of
    rows, it throws `the Composer's placeable rows (14100-14199) are all taken`.
- **build.js** (Node): `node tools/data/composer/build.js <project.json> [--check] [--json] [--saved <folder>] [--tools
  <nwn5e-tools>] [--game <folder>] [--reader <exe>] [--files <folder>] [--rows <folder>]`.
  - The defaults are floatdisk build.js's: files `data/files`, rows `data/rows`, the reader through `reader.js`'s
    `ensureReader`.
  - It reads parts through `reader.js` `create(...).part`, an own file resolved from the project's folder.
  - It places each part with `F.placePart(part, model, i, guide.height, "j")`, then lowers it by `guide.height`, so the
    model's base is at its own z 0.
  - **Placeable, per variant:**
    - `<files>/<model>.mdl`: `F.pileModelText(model, F.uniqueNames(meshes, []), lo, hi, about, P.MARK)`;
    - `<files>/<model>.pwk`: `F.pileWalkmeshText(model, lo, hi, { mark: P.MARK, use: [use ?? P.defaultUse(guide)],
      block: walk === "blocks" ? P.outline(guide) : null })`;
    - `<rows>/placeables.<name>.txt`: `F.rowsText(the game's placeables table, copy ?? the first placeable part's row ??
      189, entries, { labelColumn: "Label", modelColumn: "ModelName", mark: P.MARK, about })`.

      The first placeable part's row is the lowest `placeables.2da` row whose `ModelName` is its model.
  - **Spell mesh, per variant:**
    - `<saved>/<model>.vfx` = `P.MARK + "\n" + core.writeDraft(core.effect({ name: model, duration: true, seconds: 2,
      attach: "root", planes }))`, CRLF as the bench writes drafts;
    - `--saved` is required;
    - textures found beside the owner's files are copied into `<saved>`.
  - **It refuses, writing nothing:**
    - a placeable's variant with no row, a row outside `ROWS`, or a row used twice;
    - a row that any other `<rows>/placeables*.txt` or `<repo>/data/looks/placeables*` sets;
    - a model name the game has (`res(model, 2002)`), unless the file here carries the mark;
    - Blocks with no guide;
    - a file in its way without `P.MARK` (texts) or not listed in `tools/data/composer/made.txt` (textures).
  - **`--json`** prints one line: `{ kind, check, variants: [{ name, model, row, parts, meshes, triangles, size, past }],
    files: [{ state, file }] }`. An error prints `{ "error": message }` and exits 1, with `composer: <message>` on
    stderr.

- [ ] **Step 1: Write the checks.**
  - **`tools/data/composer/check.js`** (no game; `check(name, fn)` as floatdisk's; last line `composer check: <n> checks
    pass`):
    - `project.js loads in a page (no require, no module, no Buffer)`;
    - `readProject: a placeable and a spell mesh`;
    - `readProject: each error names its field`;
    - `model names and labels fit (16 characters)`;
    - `freeRows: the lowest free, own rows kept, never a taken one`;
    - `outline: a circle's 16 points, a rectangle's 4, none`;
    - `defaultUse: 0.1 m past the guide's front, at -y`;
    - `newScale: within 0.4 of the guide, never above 1`;
    - `a walk-through pwk: its use point, no ground mesh`;
    - `a blocking pwk: the outline as faces of material 7, then the use point`;
    - `a placeable's model and rows carry the Composer's mark; the rows copy the source row`;
    - `a spell mesh's draft reads back with its meshes as planes`;
    - `projectText: a part a line, and a round trip`.
  - **`tools/data/floatdisk/check.js`:** `the new arguments' defaults give today's text`. pileModelText, pileWalkmeshText,
    rowsText and carriesMark give the same with the defaults left out and spelled out.
  - **`tools/data/composer/check_build.js`** (needs the game; prints `skip: no game at <dir>` without it). It works on a
    temporary copy of `tools/data/floatdisk` and `tools/data/composer`, with empty `data/files`, `data/rows` and
    `data/looks`. Its last line is `composer build check: all pass`. Its checks:
    - `a placeable builds its model, pwk and rows`: a two-variant project (`plc_b04`; `plc_a03` with `plc_b06`), rows
      14100 and 14101. It writes four model files and `placeables.<name>.txt`, each with the mark. The rows are
      LootBag1's (row 13, the first placeable part's) with Label and ModelName changed.
    - `a second build changes nothing`;
    - `a row another rows file sets is refused, nothing written`;
    - `Blocks with no guide is refused`;
    - `a spell mesh writes its draft into --saved`;
    - `a file in the way it didn't make is refused`.
- [ ] **Step 2: Run them.**
  - `node tools/data/composer/check.js`. Expected: `Error: Cannot find module './project.js'`.
  - `node tools/data/floatdisk/check.js`. Expected: `FAIL the new arguments' defaults give today's text`, then
    `floatdisk check: FAILED (26 passed)`.
  - `node tools/data/composer/check_build.js`. Expected: `Cannot find module` (no build.js yet).
- [ ] **Step 3: Implement.** README.md covers the project file, the two kinds and what each builds, the row range and the
  command.
- [ ] **Step 4: Run.**
  - `node tools/data/composer/check.js`. Expected: `composer check: 13 checks pass`.
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 27 checks pass`.
  - `node tools/data/floatdisk/check_build.js`. Expected: `build check: all pass`; the disk's 13 files are unchanged.
  - `node tools/data/composer/check_build.js`. Expected: `composer build check: all pass`.
  - `git status --short`. Expected: the five new files, floatdisk.js and check.js; nothing in `data/`.
- [ ] **Step 5: Commit.**
  - `git add tools/data/composer/project.js tools/data/composer/build.js tools/data/composer/check.js tools/data/composer/check_build.js tools/data/composer/README.md`;
  - `git branch --show-current` (anim);
  - `git commit --only` those five and `tools/data/floatdisk/floatdisk.js tools/data/floatdisk/check.js` `-F -`, with
    "feat: the Composer's project generator - a placeable's model, use point, walkmesh and rows (14100-14199), a spell
    mesh's draft; floatdisk.js shares its placement and texts (#39)".

### Task A4: Planes that fade - the draft and the model writer (nwn5e-ee, anim, C++)

Working folder: the anim worktree. Independent of A1-A3.

**Files:**
- Modify: `src/data/vfx_model.h`, `src/data/vfx_model.cpp`, `tests/data/vfx_model_test.cpp`
- Create: `tests/data/fixtures/plane_beat.vfx` (LF, as `plane_material.vfx`)

**Interfaces:**
- **`VfxKeys`** gains `std::vector<float> alpha;` (rows `t a`).
- **`VfxPlane`** gains:
  - `std::string mesh;` (the spell mesh it came from: the bench's);
  - `float scale = 1.0f;` (the bench's; its verts are already scaled);
  - `float start_seconds = 0.0f;` and `float emit_seconds = 0.0f;` (the bench's, as a light's);
  - `std::vector<VfxKeys> keys;`
- **The draft's `[plane <name>]`:**
  - after `render` and before `verts`, each only when it isn't the default: `mesh = <name>`, `scale = <s>`,
    `start_seconds = <s>`, `emit_seconds = <s>`, then `DraftKeys(p.keys)`;
  - read back the same way, `<anim>_alpha` through `DraftKeyLine`;
  - a draft without them keeps its bytes.
- **`DraftKeys` / `DraftKeyLine`** gain `<anim>_alpha = t a t a ...`. **`LastKeyTime`** scans the planes' alpha rows too.
- **`WriteVfxModel`:** in each animation, after the lights, each plane with alpha keys in it gets:
  - `node trimesh <its geometry node name>` (`ModelNodeName(model, "p" + i, p.name)`, as in the geometry);
  - `  parent <its geometry parent>`;
  - `  alphakey <n>` with its rows `    t a`;
  - `endnode`.

  `mesh`, `scale` and the seconds aren't written into the model. A plane without keys appears in no animation, as now.

- [ ] **Step 1: Write the tests** in vfx_model_test.cpp's `TEST(...)` style, and the fixture: one effect with a node
  `ring` and a plane `beat` under it, carrying every new key: `mesh = n5m_test`, `scale = 0.5`, `start_seconds = 0.3`,
  `emit_seconds = 1.2`, and `impact_alpha = 0 0 0.3 0 0.5 0.8 1.3 0.8 1.5 0`. Written in WriteVfxDraft's order.
  - `VfxDraftPlaneBeatRoundTrip`: the plane through WriteVfxDraft and ReadVfxDraft comes back equal, its lines in order.
  - `VfxDraftPlaneBeatFixture`: `plane_beat.vfx` read and written is the file, byte for byte.
  - `VfxModelPlaneAlphaKeys`:
    - inside `newanim impact` there's a `node trimesh` for the plane, with `alphakey 5` and its rows;
    - no other animation has it;
    - the geometry's trimesh is as before;
    - the impact's length is at least 1.5.
  - `VfxModelPlaneWithoutKeysUnchanged`: `plane_material.vfx`'s model has no `node trimesh` after its first `newanim`.
- [ ] **Step 2: Run** `cmd //c test.bat`. Expected: the compile fails (`'mesh': is not a member of 'data::VfxPlane'`),
  and compile.log is printed.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `cmd //c test.bat`. Expected: `<n> tests, 0 failed`, with n four more than before this task.
- [ ] **Step 5: Commit.**
  - `git add tests/data/fixtures/plane_beat.vfx`;
  - `git branch --show-current` (anim);
  - `git commit --only src/data/vfx_model.h src/data/vfx_model.cpp tests/data/vfx_model_test.cpp tests/data/fixtures/plane_beat.vfx -F -`,
    with "feat: a plane can fade - the draft's planes keep alpha keys per animation (and, for the VFX Bench, the mesh they
    came from, their scale and timing); the model writer keys their alpha (#39)".
  - The next "anim ready" names `src/data/vfx_model.*`: the fades need the game session's next install.

### Task A5: The server's Composer routes - projects, the parts list, parts, textures, thumbnails (nwn5e-tools)

Working folder: `/c/Users/Bruno/Desktop/Projects/nwn5e-tools`. Needs A1-A3.

**Files:**
- Create: `vfx_bench/composer_server.js`, `vfx_bench/composer_fixture.js`, `vfx_bench/check_composer_server.js`
- Modify: `vfx_bench/server.js`:
  - the `--ee <checkout>` and `--game <folder>` options;
  - `/api/composer*` and `/composer/floatdisk.js` / `/composer/project.js` handed to `composer_server.js`;
  - the stale list `CODE` gains `composer_server.js` and the checkout's `floatdisk.js`, `reader.js` and `project.js`
    (when found).

**Interfaces:**
- **`composer_server.js`:** `create({ build, ee, game, tools })` returns `{ code(), handle(req, res, url, h) ->
  Promise<boolean> }`, where `h = { send, readBody, fromPage, writeWhole }`.
  - It loads lazily, on the first Composer request:
    - the checkout's `floatdisk.js`, `reader.js` and `project.js`;
    - `../spell_bench/game_data.js`;
    - the game (`--game`, else `anim_helper/helper.js` `findGameDir({ buildDir: build })`);
    - `reader.create(...)` with `ensureReader({ repo, reader: process.env.NWN5E_MDLMESH })`.

    A failure is reported by `GET /api/composer` as `{ ok: false, error }`. The effect mode never loads any of it.
- **`findCheckout({ ee, tools })`:**
  - `ee` when given;
  - else `<tools>\..\nwn5e-ee-anim` when it has `tools\data\floatdisk\composition.json`;
  - else `<tools>\..\nwn5e-ee`.
- **`branchOf(dir)`:**
  - `.git` a folder: its `HEAD`;
  - `.git` a file `gitdir: X`: `X\HEAD`;
  - `ref: refs/heads/anim` gives `anim`, and a detached HEAD its first 7 characters;
  - no git command.
- **Routes:**
  - **`GET /api/composer`:** `{ ok, ee, branch, warnBranch: branch !== "anim", projects, partsFolders, game }`.
    - `projects`: `{ id, kind, title, file }`. The Floating Disk comes first, `{ id: "floatdisk", kind: "floatdisk",
      title: "Floating Disk (#39)" }`, when composition.json exists. Then every `tools/data/composer/*.json`: `id` its
      name, `kind` from the file; one that doesn't read is listed with `error`.
    - `partsFolders`: `["composer/parts", "floatdisk/parts"]` (those that exist), from `tools/data/`.
  - **`GET /api/composer/project?id=<id>`:** `{ id, kind, folder (from tools/data/), text, hash (sha1 of text) }`.
  - **`POST /api/composer/new`** (`application/json`, from the page) takes `{ name, kind }`:
    - writes `tools/data/composer/<name>.json` as `P.projectText(P.newProject(name, kind))`, then 200 `{ id }`;
    - a bad name or kind gives 400; an existing project gives 409.
  - **`GET /api/composer/catalog`:** `{ placeables, items, yours }`, each a list of `{ id, label, models | path, kind,
    search }`, sorted by label.
    - **Placeables:** the models `placeables.2da`'s `ModelName` names that the archives have, one entry a model, labelled
      by its first row's `Label`, every label in `search`.
    - **Items, from `baseitems.2da`:**
      - `ModelType` 2: each number N with `<class>_b_N`, `_m_N` and `_t_N`, labelled `<label> N`, kind `weapon`, three
        models;
      - `ModelType` 0 / 1: `<class>_N`, kind `item` / `helmet`;
      - then `it_*` and `gi_*` models not already placeables, minus `gi_sound*`, `gi_waypoint*` and `gi_store`.
    - **Yours:** the `.mdl` files in `partsFolders`, `{ id: "own_" + sha1(path + mtime).slice(0, 12), path:
      "composer/parts/<name>.mdl", label, mtime }`.
  - **`GET /api/composer/part?models=a[,b,c]`** (each a resref) or **`?file=<path>&project=<id>`**:
    - the file is resolved by `F.resolvePartFile` from the project's folder, as the generators resolve it (a `.mdl`
      anywhere a project may name one);
    - answers `{ key, label, meshes, lo, hi, textures: { <bitmap>: { draw: "texture" | "flat", why: "dds-only" | "plt"
      | "missing" | "", shine } } }`;
    - `shine` is true when the texture's TXI has an `envmaptexture` line (draw it opaque);
    - kept in memory by key (an own file by path and mtime);
    - a bad name (not a resref, not a `.mdl`) gives 400; a missing file or a reader error gives 404 `{ error }`.
  - **`GET /api/composer/texture/<name>[?file=<path>&project=<id>]`:** the TGA (`application/octet-stream`), or 404
    `{ error, flat: "dds-only" | "plt" | "missing" }`. A14 adds DDS.
  - **`GET /api/composer/thumb/<key>`** (`/^[a-z0-9_+]{1,64}$/`): the PNG, or 404.
  - **`POST /api/composer/thumb/<key>`** (`image/png`, from the page, up to 262144 bytes): writes
    `<build>\vfxedit\composer\thumbs\<key>.png` whole.
  - **`GET /composer/floatdisk.js`** and **`/composer/project.js`:** the checkout's files, as `text/javascript`.
- **`composer_fixture.js`** (Node, for the checks):
  - `tempCheckout({ ee })` copies `tools/data/floatdisk`, `tools/data/composer` and the disk's committed outputs into
    `os.tmpdir()/n5_composer-*/repo`. It also:
    - writes `.git` as `gitdir: <tmp>/gitdir`, with `HEAD` `ref: refs/heads/anim`;
    - adds `tools/data/composer/testpile.json`: a placeable, circle guide 1 m, variants `1` (`plc_b04`) and `2`
      (`plc_a03`, `plc_b06`), no rows yet;
    - adds `tools/data/composer/testmesh.json`: a spell mesh, one variant (`plc_b06`);
    - adds `tools/data/composer/parts/n5_testcrate.mdl` (an ASCII 12-triangle cube, `bitmap n5_testcrate`) with a 2 x 2
      `n5_testcrate.tga` beside it;
    - returns `{ repo, cleanup() }`.
  - `tempBuild()` gives an empty bench build folder, with `vfxedit\saved\`.

- [ ] **Step 1: Write `check_composer_server.js`** in browser_check.mjs's ok / FAIL style (`skip: no game at <dir>`
  without the game). It starts the server on 18776 with `--build <tempBuild> --ee <tempCheckout> --no-open`, and
  `NWN5E_MDLMESH=<anim>\build\floatdisk\mdlmesh.exe`. Its steps:
  - `composerState`: ok, `ee` the copy, branch `anim`.
  - `projectsListed`: `floatdisk` first, then `testmesh` and `testpile`, each with its kind.
  - `projectRead`: `testpile`'s text, and `hash` its sha1.
  - `newProject`:
    - `{ name: "crates", kind: "placeable" }` writes `crates.json` with one empty variant;
    - the same again gives 409;
    - `"Bad-Name"` gives 400.
  - `branchOf`: a `.git` folder whose HEAD is `ref: refs/heads/main` gives `main`; a 40-hex HEAD gives 7 characters.
  - `checkoutDefault`: `findCheckout({ tools })` is `...\Projects\nwn5e-ee-anim` (read-only).
  - `catalogKinds`:
    - placeables over 1,000, with `{ id: "plc_b04", label: "LootBag1" }`;
    - a weapon whose models are `wswls_b_011`, `wswls_m_011` and `wswls_t_011`;
    - no `gi_waypoint`;
    - Yours `composer/parts/n5_testcrate.mdl`.
  - `partMeshesAreTheGenerators`: `part?models=plc_b04`'s meshes deep-equal `reader.create(...).part(...)`'s.
  - `weaponPieces`: three pieces' meshes, as many as the three read alone.
  - `ownFile`: `part?file=parts/n5_testcrate.mdl&project=testpile` gives 12 triangles and its texture from beside it.
  - `badNames`: `models=../x` and `file=parts/x.txt` give 400; `file=../../nothing.mdl` gives 404.
  - `textures`:
    - `plc_rugdmtlbrk` decodes (`core.decodeTga`) at 512 x 512;
    - `tno01_post` gives 404 with flat `dds-only`;
    - `part?models=plc_c07` has `plc_gold` with shine true.
  - `thumbCache`: 404, then a POST with the page's Origin gives 200, then a GET returns the same bytes. A POST without an
    Origin gives 403.
  - `scriptsServed`: `/composer/floatdisk.js` and `/composer/project.js` give 200 `text/javascript`, containing
    `placePart` and `outline`.
  - `staleGuard`: the copy's `project.js` touched 5 s ahead gives `POST /api/composer/thumb/...` 409 with
    `vfx_bench.bat again`; its time is restored after.
  - `effectModeUnchanged`: `/api/state` has exactly the keys it had before this task.
- [ ] **Step 2: Run** `node vfx_bench/check_composer_server.js`. Expected: `composerState` FAIL (404 not found), the rest
  FAIL, then `<n> failed`.
- [ ] **Step 3: Implement** `composer_server.js`, `composer_fixture.js` and the server.js wiring.
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/check.js`. Expected: as before.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/composer_server.js vfx_bench/composer_fixture.js vfx_bench/check_composer_server.js`;
  - then `git commit --only` those three with `vfx_bench/server.js` `-F -`, with "feat: the VFX Bench's Composer, the
    server - projects, the parts list, a part's meshes through the generator's reader, textures, thumbnails (nwn5e-ee
    #39)".

### Task A6: The server's Save per kind; spell meshes as chunks (nwn5e-tools)

Needs A5.

**Files:**
- Modify: `vfx_bench/composer_server.js`, `vfx_bench/server.js` (`state()` and `/api/draft` for the spell meshes),
  `vfx_bench/check_composer_server.js`

**Interfaces:**
- **`POST /api/composer/save`** (`application/json`, from the page) takes `{ id, project, base, force? }`. For
  `floatdisk`, `project` is the composition object. In order:
  1. **Read it:** `F.readComposition` (floatdisk) or `P.readProject` (the others). If it throws: 400 `{ error }`.
  2. **Changed on disk:** if the file's text no longer hashes to `base` and `force` isn't set: 409 `{ error: "<file>
     changed on disk since you opened it", changed: true }`.
  3. **Running:** if a save is running: 409 `{ error: "a save is running" }`.
  4. **Placeable rows:** variants without a row are given rows by `P.freeRows`. The rows taken are:
     - those set in `data/rows/placeables*.txt` (but its own `placeables.<name>.txt`);
     - those in `data/looks/placeables*`;
     - the game table's rows in the range that aren't blank or `USER`;
     - those in `<build>/served/placeables.2da`, when it's there.
  5. **The temporary file:** the text (`F.compositionText` or `P.projectText`) goes to `os.tmpdir()/n5_composer-<n>.json`.
  6. **The build** (`execFile`, cwd the checkout, 120 s):
     - floatdisk: `node tools/data/floatdisk/build.js --json --composition <temp> --tools <nwn5e-tools> --game <game>
       --reader <exe>`;
     - placeable: `node tools/data/composer/build.js <temp> --json --tools ... --game ... --reader ...`;
     - spell mesh: the same, plus `--saved <build>/vfxedit/saved`.
  7. **Exit 0:**
     - the file is written whole with the temp's text (a placeable's with its rows), and the temp is deleted;
     - a spell mesh's models join `<build>/vfxedit/meshes.txt` (one a line, sorted, unique);
     - 200 `{ ok: true, result, hash, text }`.
  8. **Otherwise:** the temp is deleted, then 422 `{ error: <its "floatdisk: ..." / "composer: ..." line>, output }`. The
     file is untouched.
- **server.js:**
  - `state().meshes` is meshes.txt's names;
  - `state().chunks` is the library's chunks plus those names;
  - `state().saved` leaves them out;
  - `GET /api/draft?source=chunks&name=<n>` serves `saved/<n>.vfx` when `<n>` is in meshes.txt and the library has
    none.

- [ ] **Step 1: Add steps** to `check_composer_server.js` (on the copy):
  - `saveWritesAndRebuilds` (floatdisk): Medium's first part turned 30 more degrees gives 200. The copy's composition.json
    is `compositionText` of what was posted. `c_n5fd_med.mdl` and `plc_n5fd_med.mdl` are rebuilt, the other 11
    unchanged.
  - `saveUnchangedIsUnchanged`: the file's own composition gives all 13 unchanged.
  - `refusedBuildChangesNothing`: a part `plc_nope` gives 422 matching `/no such model/`. The file's bytes and
    `data/files`' times are unchanged.
  - `invalidRefused`: `scale: 0` gives 400 matching `/scale/`.
  - `changedFileRefused`: the copy's file rewritten gives 409 `changed`; then `force: true` gives 200.
  - `placeableSaveBuilds`:
    - `testpile` gives 200, and its file now has rows 14100 and 14101;
    - `data/files/plc_n5testpile_1.mdl`, `_2.mdl` and their `.pwk`, and `data/rows/placeables.testpile.txt`, are new;
    - the rows file's rows are 14100 and 14101.
  - `placeableRowsStay`: saved again, the same rows, and every file unchanged.
  - `rowTakenElsewhereSkipped`: with the copy's `data/rows/placeables.other.txt` setting 14100 before `crates`' first
    save, `crates` gets 14101.
  - `spellMeshSaves`:
    - `testmesh` gives 200, and `<tempBuild>/vfxedit/saved/n5m_testmesh.vfx` starts with the Composer's mark;
    - `meshes.txt` lists `n5m_testmesh`;
    - `/api/state` has it in `chunks` and `meshes`, not in `saved`;
    - `/api/draft?source=chunks&name=n5m_testmesh` gives 200.
  - `saveFromElsewhereRefused`: no Origin gives 403.
  - `realWorktreeUntouched`: at the end of the check, these are as they were at its start:
    - the real anim worktree's `composition.json` sha1;
    - its `tools/data/composer/` listing;
    - its `data/files` times;
    - the real bench build's `vfxedit/saved` listing.
- [ ] **Step 2: Run** `node vfx_bench/check_composer_server.js`. Expected: the new steps FAIL (404); only
  `realWorktreeUntouched` passes.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** `vfx_bench/composer_server.js vfx_bench/server.js vfx_bench/check_composer_server.js`, with
  "feat: the VFX Bench's Composer saves each kind - the Floating Disk through its generator, a placeable's files and rows
  (14100-14199), a spell mesh into saved\ and listed as a chunk; a refused build writes nothing (nwn5e-ee #39)".

### Task A7: The page's pure parts - projects, variants, the guide, the disk's adapter (nwn5e-tools)

Needs A1 and A3 (the checkout's floatdisk.js and project.js).

**Files:**
- Create: `vfx_bench/public/composer.js` (UMD, `root.VfxComposer`), `vfx_bench/check_composer.js`
- Modify:
  - `vfx_bench/server.js`: `FILES` and `CODE` gain `composer.js`;
  - `vfx_bench/check_composer_server.js`: `summaryMatchesGenerator`.

**Interfaces:**
- **`VfxComposer.with(F, P)`** binds the checkout's FloatDisk and ComposerProject. In Node, they're required from `--ee`
  (default `../../nwn5e-ee-anim`).
- **The Floating Disk's adapter:**
  - `fromComposition(comp)` gives `{ kind: "floatdisk", name: "floatdisk", folder: "floatdisk", guide: { shape: "circle",
    diameter: disk.diameter, height: disk.height }, variants }`:
    - `variants` are `empty` (locked, no parts), `small`, `medium` and `large`;
    - each `{ name, parts, locked? }` from `stages`.
  - `toComposition(project, comp)` gives a copy of `comp` with `stages` and `disk.diameter` / `disk.height` from the
    project. Every other key is kept.
- **A project and its parts:**
  - `fromProject(json, id)`: the project for the page.
  - `toProject(project)`: its file's JSON.
  - `keyOf(raw)`: the server's part key.
  - `norm(raw)` / `clean(raw)`: defaults, and rounding as before (`at` 0.001, turns 0.1, `scale` 0.001; a `[0, 0]` tilt
    dropped).
  - `relFile(projectFolder, path)`: a part's `file` from the project's folder (`parts/x.mdl`, `../composer/parts/x.mdl`).
- **Variants:**
  - `addVariant(project)` (the next free name, `1`, `2`...);
  - `removeVariant(project, v)` (never the disk's);
  - `ghosts(project, v)`: the previous variant's parts;
  - `copyPrevious(project, v)`.
- **The variant on the stage:**
  - `placed(project, v, models)`: through `F.placePart(norm(raw), model, i, guide.height, "j")`. Missing models are
    skipped and listed.
  - `summary(project, v, models)`: `{ parts, meshes, triangles, tall, top, list: [{ label, size, from, to, past }] }`.
    - `past` is how far the part reaches past the guide: a circle's `reach - r`; a rectangle's largest corner overshoot;
      0 with no guide.
    - For the Floating Disk the disk's own mesh (`F.diskMesh`) is counted in `meshes` and `triangles`, as build.js
      counts it.
- **Picking, resting and the handles:**
  - `pick(ray, placedList)`;
  - `restHeight(moving, others, floor)` (floor the guide's height);
  - `moveBy`, `ringOf`, `turnBy`, `scaleBy`, `liftBy`: as Task 5 above;
  - `newScale` is `P.newScale`; `layFlat(lo, hi)`;
  - `partFrom(entry, [x, y], box, project)`.
- **The use point:** `useOf(project)` (`placeable.use` or `P.defaultUse(guide)`) and `moveUse(project, [x, y])`.
- **Undo and the frame:**
  - `changedVariant(a, b)`: the first variant that differs (the guide or the kind's settings: the variant shown), or
    null;
  - `snapshot(project)`: its JSON;
  - `frameBox(lo, hi)`.

- [ ] **Step 1: Write `check_composer.js`** (ok / FAIL lines, last line `all composer checks pass`). Synthetic cubes and
  the real composition.json; no game.
  - `composer.js loads under node and in a page`;
  - `placed equals placePart, the floor the guide's height`;
  - `the summary as the generators print it (a circle guide, a rectangle guide)`;
  - `pick: the nearest part under the ray, none off it`;
  - the rest checks:
    - `rest on the guide's height: z 0`;
    - `rest on a part: its top under the footprint`;
    - `rest beside a part: back on the floor`;
    - `rest on the higher of two`;
  - the handles:
    - `move keeps the grab offset`;
    - `turn by the angle swept; step 15 rounds`;
    - `scale by the distance ratio, clamped`;
    - `lift along the vertical, never below the floor`;
  - `layFlat lays a standing weapon down`;
  - `a new part's scale within 0.4 of the guide`;
  - `variants: add, remove, the disk's fixed, Empty takes no parts`;
  - `ghosts and copyPrevious`;
  - `the use point: the guide's front by default, moved`;
  - `relFile from a project's folder`;
  - `clean rounds and drops a zero tilt`;
  - `changedVariant names the variant an undo shows`;
  - `frameBox keeps the box in view`;
  - `the disk's adapter round trip`: composition.json through `fromComposition`, `toComposition` and `F.compositionText`
    is the file but for `0.0`.

  In `check_composer_server.js`, `summaryMatchesGenerator`: for the copy's Floating Disk (against `build.js --check
  --json`) and `testpile` (against `composer/build.js --check --json`), `summary` with the server's meshes equals the
  generator's. Triangles and meshes are exact; the lengths within 0.001.
- [ ] **Step 2: Run** `node vfx_bench/check_composer.js`. Expected: `FAIL composer.js loads ...: Cannot find module`, the
  rest FAIL, then `<n> failed`. `node vfx_bench/check_composer_server.js`. Expected: `summaryMatchesGenerator` FAIL.
- [ ] **Step 3: Implement** `public/composer.js`.
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/public/composer.js vfx_bench/check_composer.js`;
  - then commit them with `vfx_bench/server.js vfx_bench/check_composer_server.js`, with "feat: the VFX Bench's
    Composer, its pure parts - projects and variants, the guide, the Floating Disk's adapter, placement through the
    generator's placePart, resting, the handles' maths (nwn5e-ee #39)".

### Task A8: core.js's planes as mesh beats (nwn5e-tools)

Needs A4 (its fixture).

**Files:**
- Modify: `vfx_bench/public/core.js`, `vfx_bench/check_chunks.js`

**Interfaces:**
- **`core.plane()`'s defaults** gain `mesh: ""`, `scale: 1`, `start_seconds: 0`, `emit_seconds: 0` and `keys: []`.
- **`writeDraft` / `readDraft`** write and read them in A4's order, only when they aren't the default. `<anim>_alpha`
  joins the key lines (`draftKeys`, `draftKeyLine`).

- [ ] **Step 1: Add** to `check_chunks.js`, beside the `plane_material.vfx` check:
  - `plane beat: nwn5e-ee's plane_beat.vfx read and written byte for byte`;
  - `plane beat: the defaults write no new lines` (the material fixture's bytes unchanged).
- [ ] **Step 2: Run** `node vfx_bench/check_chunks.js`. Expected: `FAIL plane beat: nwn5e-ee's plane_beat.vfx ...`, the
  line `mesh = n5m_test` missing.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_chunks.js`. Expected: `all chunk checks pass`.
  - `node vfx_bench/check.js`. Expected: as before: every draft round trips, every texture decodes.
- [ ] **Step 5: Commit** `vfx_bench/public/core.js vfx_bench/check_chunks.js`, with "feat: the VFX Bench's drafts keep a
  plane's mesh, scale, timing and alpha keys - core.js in step with vfx_model.cpp (nwn5e-ee #39)".

### Task A9: The stage - one mesh path, the mesh texture loader, pointer handlers, the ray (nwn5e-tools)

**Files:**
- Modify: `vfx_bench/public/view.js`, `vfx_bench/public/app.js` (the View's loaders), `vfx_bench/check_chunks.js`,
  `vfx_bench/check_composer.js`

**Interfaces:**
- **`meshData(mesh, place)`:** chunkMeshes' per-particle vertex data, factored out. `chunkMeshes` calls it per particle,
  and its output doesn't change.
- **`worldMeshes(list)`:** the same data for meshes already placed in the world. An item may carry `opaque` (the
  texture's alpha ignored) and `ghost` (a pale tint at alpha 0.25, blended, no depth write).
- **The fragment shader** gains `u_opaque` (`if (u_opaque > 0.5) c.a = v_color.a;`).
- **`View.drawMeshes(list, cam)`:** opaque and cut-out meshes first, then those under alpha 1 (blended), then ghosts.
  `draw()` uses it for chunks.
- **`View.drawScene({ figure, meshes, ghosts, lines })`:** the Composer's frame. It draws the ground, the figure, the
  meshes, the ghosts, then the lines (the guide's outline, the use point and the handles) with the depth test off. No
  disk.
- **`View.putTexture(name, image)`.**
- **Mesh textures:** `new View(canvas, loadTexture, loadChunk, loadMeshTexture)`.
  - Meshes (chunks, the Composer's, planes) take their textures through `meshTexture(name)` with `loadMeshTexture`.
  - Particles keep `loadTexture`, the library's.
  - app.js gives `chainLoaders(libraryImage, gameImage)`: the library's copy, else `/api/composer/texture/<name>`.
    `chainLoaders` is a pure export of view.js.
- **Pointer:** `view.handlers = { grab, drag, drop, click }` as Task 6 above; `view.minDistance` (1; the Composer 0.6).
- **Pure helpers:** `rayAt(camera, w, h, x, y)`, `projectAt(camera, w, h, p)`; `View.ray(x, y)`, `View.project(p)`.

- [ ] **Step 1: Add checks.**
  - `check_chunks.js`:
    - `a world mesh draws as a still chunk`;
    - `chainLoaders: the library first, then the game, then missing`.
  - `check_composer.js`:
    - `rayAt and projectAt round trip`;
    - `the centre ray is the camera's forward`.
- [ ] **Step 2: Run** `node vfx_bench/check_chunks.js` and `node vfx_bench/check_composer.js`. Expected: the four new
  steps FAIL (`V.worldMeshes is not a function`, `V.chainLoaders is not a function`, `V.rayAt is not a function`).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_chunks.js`. Expected: `all chunk checks pass`.
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** `vfx_bench/public/view.js vfx_bench/public/app.js vfx_bench/check_chunks.js
  vfx_bench/check_composer.js`, with "feat: the VFX Bench's stage - one mesh path for chunks and placed meshes, their
  textures from the library or the game, pointer handlers for handles, a ray from the pointer (nwn5e-ee #39)".

### Task A10: The mode - projects, variants, the guide card, the kind's card, undo (nwn5e-tools)

Needs A5, A6, A7 and A9.

**Files:**
- Create: `vfx_bench/public/composer_page.js` (`root.VfxComposerPage`), `vfx_bench/composer_check.mjs`
- Modify: `vfx_bench/public/index.html`, `vfx_bench/public/style.css`, `vfx_bench/public/app.js`, `vfx_bench/server.js`
  (`FILES` and `CODE`: `composer_page.js`)

**Interfaces:**
- **index.html** (no existing id changes; `body.composer` shows `.composer-only` and hides `.effect-only`):
  - **the bar:** `#composer-open` "Composer"; Composer only: `#comp-project` (a select), `#comp-new` "New project",
    `#composer-save` "Save and build", `#composer-close` "Back to effects";
  - **the drawer:** `#parts` (A11);
  - **the panel:**
    - `#comp-card`: what's open, "Saving to <folder> (<branch>)" with a warning off `anim`, the last result;
    - `#guide-card`: `#guide-shape` (circle / rectangle / none), `#guide-diameter`, `#guide-width`, `#guide-length`,
      `#guide-height`;
    - `#variant-card`: name, Remove, Copy the previous one in;
    - `#placeable-card`: the variant's model and row (shown), `#plc-copy` (the row copied), `#plc-use-x`, `#plc-use-y`,
      `#plc-walk` (Walk-through / Blocks);
    - `#mesh-card-c`: the variants' mesh names;
    - `#part-card`;
    - `#stage-card`: the summary;
  - **the stage:** `#variant-tabs` (and `#variant-add`);
  - **the footer:** `#comp-foot` (`#comp-onion`, `#comp-rest`, `#comp-view-34`, `-side`, `-top`, `-front`,
    `#comp-strip`).
- **composer_page.js:** `VfxComposerPage.mount({ view, image, setStatus, getJson, postJson, history: { push, clear,
  setDragging } })` returns `{ enter(id?, variant?), leave(), frame(dt), snapshot(), restore(text), dirty(), save(),
  key(ev) -> bool, hook() }`.
  - `enter` loads `/composer/floatdisk.js` and `/composer/project.js` (script tags, once), `GET /api/composer`, then the
    project (the last one opened, else the Floating Disk) and its variant's part meshes.
- **New project:** a name and kind asked in a small dialog, `POST /api/composer/new`, then opened.
- **Opening a project** (or leaving the Composer) clears the history.
- **Variant tabs:** a tab per variant, with its part count. A switch isn't an undo step. For the Floating Disk, Empty,
  Small N, Medium N, Large N, fixed, and Empty takes no parts.
- **Cards:**
  - the guide card's fields change `project.guide` (rounded); the outline follows;
  - the placeable card's fields change `placeable`;
  - the strip: a chip per part; a click selects it;
  - the part card is as before;
  - every field is one undo step on change.
- **The figure:**
  - a placeable's stands on the use point, facing the guide's centre;
  - the others' stand 0.85 m to the left, facing +y.
- **The stage draws:** the variant's parts (`worldMeshes`), the ghosts (Onion skin, on by default), and the guide's
  outline (amber where a part reaches past it).
- **app.js:**
  - `state.composer`;
  - the Composer button runs `leaveOk()`, then `enter`;
  - `tick` hands the frame over while it's open;
  - snapshots carry `{ composer }`;
  - `save()`, `leaveOk()`, `beforeunload` and `keydown` go to the Composer while it's open;
  - `?compose=<id>[&variant=<name>]` opens it at start;
  - Back to effects restores the effect as it was.
- **Undo:** `restore` switches to `changedVariant(now, restored)` when there is one, and keeps the selection by index.
- **`window.vfxBench.composer()`:** `{ open, project: { id, kind }, variant, variants: [{ name, parts }], parts: [{
  key, at, turn, scale, tilt, note }], selected, ghosts, guide, use, past, drawn: { parts, triangles, textured, flat,
  names }, dirty, ee, branch, screenOf(i), handle(kind), last }`.
  - `handle(kind)` covers `"ring"`, `"scale"`, `"lift"` and `"use"`;
  - `names` are the drawn meshes' names.

- [ ] **Step 1: Write `composer_check.mjs`**: headless Chrome as browser_check.mjs, port 18778, `tempBuild` and
  `tempCheckout`, `NWN5E_MDLMESH`, and `window.confirm` stubbed to answer true. It prints `skip: no Chrome or Edge` or
  `skip: no game at <dir>` when it can't run. Its steps:
  - `composerOpens`: `?compose=floatdisk&variant=medium` gives `open`, the variant `medium`, 7 parts drawn textured, and
    no `forcedisk` in `names`.
  - `projectsListed`: `#comp-project` holds Floating Disk (#39), `testmesh` and `testpile`.
  - `newProject`: `crates` (placeable) opens with one empty variant; `crates.json` is in the copy.
  - `variantsAndCounts`:
    - the Floating Disk's tabs read `Empty`, `Small 2`, `Medium 7`, `Large 12`;
    - `testpile`'s read `1 1`, `2 2`;
    - `+ variant` gives a third.
  - `onionSkin`: on Large, `ghosts` 7; Onion skin off gives 0; on Small, 0.
  - `guideCard`: the Floating Disk's diameter 1 gives `guide.diameter` 1 and the outline's radius 0.5. Saved,
    `disk.diameter` would be 1 (`toComposition` through the hook).
  - `emptyTakesNoParts`: on Empty, the parts can't be added and the card says why.
  - `placeableCard`:
    - `testpile`'s use point is `[0, -0.6]` (the default);
    - Blocks sets `walk`;
    - the figure stands at the use point.
  - `partFieldsEdit`: the strip's first chip, Turn 45, gives `parts[0].turn` 45. Ctrl+Z gives the old turn, and Ctrl+Y
    45.
  - `undoShowsTheVariant`: an edit on Large, a switch to Small, then Ctrl+Z gives the variant `large` and the edit undone.
  - `backToEffects`: `body.composer` is gone, the effect is as before, and `#undo` is disabled.
  - `frameNoPageScroll` (in the Composer).
  - `no errors in the page`.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: `composerOpens` FAIL (`vfxBench.composer is not a
  function`), the rest FAIL, then `<n> failed`.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  - `node common/checks/run.js`. Expected: `all common checks pass`.
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
- [ ] **Step 5: Commit.**
  - `git add vfx_bench/public/composer_page.js vfx_bench/composer_check.mjs`;
  - then commit them with `vfx_bench/public/index.html vfx_bench/public/style.css vfx_bench/public/app.js
    vfx_bench/server.js`, with "feat: the VFX Bench's Composer mode - projects and their variants, the guide on the
    ground, a placeable's use point and walk, the onion skin, undo across variants (nwn5e-ee #39)".

### Task A11: The parts list and its thumbnails (nwn5e-tools)

Needs A10.

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/index.html`, `vfx_bench/public/style.css`,
  `vfx_bench/composer_check.mjs`

**Interfaces:**
- **`#parts`:**
  - tabs Placeables / Items / Yours, with counts;
  - `#parts-search`, matching each entry's `search`, label and model names;
  - `#parts-list`: an entry is a thumbnail, its label, its model(s) and its kind;
  - Yours' note names the folders (`tools\data\composer\parts\`, and `tools\data\floatdisk\parts\` when there), with
    `#parts-reload`.
- **Thumbnails:**
  - entries coming into view (an IntersectionObserver) queue, four at a time;
  - `GET thumb/<id>` first;
  - on a 404, the page fetches the part's meshes, waits for its textures, and draws them on one offscreen 128 x 128
    canvas of its own (a second `View`, no ground or figure, `frameBox`'s camera);
  - the PNG is posted, then shown.
- **Adding a part:**
  - a click adds `partFrom(entry, [0, 0], box, project)` on the current variant (not a locked one), rests it when Rests
    is on, and selects it: one undo step;
  - an own file's `file` is `relFile(project's folder, path)`;
  - an entry dragged onto the stage (`dragover` / `drop` on `#canvas`) adds it at the ray's point on the guide's plane.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `listKinds`:
    - Placeables count over 1,000;
    - "LootBag1" finds `plc_b04` first;
    - Items with "longsword" finds entries of three models;
    - Yours lists `n5_testcrate`.
  - `thumbnailDrawnAndKept`: the first entry's thumbnail is set, and `<tempBuild>\vfxedit\composer\thumbs\<id>.png`
    exists.
  - `addByClick`: on the Floating Disk's Medium, LootBag1 gives 8 parts. The new one is selected, at x 0 y 0, its height
    the surface there, and its footprint within 0.4 x 0.9 m.
  - `addByDrag`: LootBag3 dropped on a stage point lands within 2 cm of the guide-plane point under the drop.
  - `weaponLiesFlat`: "longsword 011" added has tilt `[0, 90]`, and its box's z is no longer its longest side.
  - `undoAdd`: Ctrl+Z removes the last add, one step each.
  - `ownFileAdds`: in `testpile`, Yours' `n5_testcrate` is added with `file` `parts/n5_testcrate.mdl` and drawn
    textured. In the Floating Disk its `file` is `../composer/parts/n5_testcrate.mdl`.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL (no `#parts-list` entries).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** the four files with "feat: the VFX Bench's Composer, the parts list - the game's placeables
  and items and your own files, searched, with thumbnails drawn once and kept; added by a click or dragged onto the
  stage (nwn5e-ee #39)".

### Task A12: The handles and the use point (nwn5e-tools)

Needs A11.

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/view.js` (only if the lines need more than `lines`),
  `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Selection:** a click on a part (`pick`) or on its chip selects it; a click on empty space, or Esc, deselects.
- **The selected part's lines:**
  - its box;
  - the ring (64 segments at `ringOf`);
  - a 3 cm square on the ring at 45 degrees;
  - the lift arrow from 5 to 20 cm above its top.
- **The use point** (placeables): a 10 cm cross on the ground with a ring, always shown. Dragging it moves `placeable.use`
  on the ground: one undo step. The figure follows.
- **`grab`, in order:**
  1. within 8 px of the use point: use;
  2. within 8 px of the square: scale;
  3. within 7 px of the ring: turn;
  4. within 8 px of the arrow: lift;
  5. else `pick` on the selected part: move;
  6. else no grab (the view orbits).
- **A drag:** `setDragging(true)` on the grab; then on each move `moveBy`, `turnBy` (Shift: 15), `scaleBy`, `liftBy` or
  `moveUse`. While Rests is on, a move or a scale rests the part again (`restHeight`, the floor the guide's height). On
  drop: `setDragging(false)` and one push.
- **The guide's outline** is amber while any part reaches past it; the stage card lists them by how much.
- **The part card's buttons:** Drop it, Duplicate, Remove.
- **Keys:** Delete removes; Ctrl+D duplicates (+0.05 m x, rested); the arrows nudge 0.01 m (Shift 0.05 m); Esc
  deselects. They're ignored while typing in a field.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `clickSelects`: `screenOf(3)` clicked gives `selected` 3; empty space gives null.
  - `dragMoves`: a part dragged 80 px right has its x grown, and its height is the surface under it.
  - `restsOnPart`: on Medium, the small bag moved over the crate's centre has its height at the crate's top (0.31 m)
    within 0.01.
  - `ringTurns`: a quarter of the ring gives +90 within 3.
  - `shiftTurnSteps`: with Shift, a multiple of 15.
  - `squareScales`: dragged out to 1.5 times its distance, the scale grows x1.5 within 0.05.
  - `arrowLifts`: 40 px up raises the height; Drop it returns it.
  - `restOffKeepsHeight`: with Rests off, a lifted part moved keeps its height.
  - `useHandleDrags`: in `testpile`, the use point dragged moves `use` (y more negative) and the figure with it; Ctrl+Z
    returns it.
  - `pastGuideIsAmber`: a part moved 1 m out gives `past` naming it.
  - `deleteAndDuplicate`: Delete gives n-1, Ctrl+Z n, Ctrl+D n+1 at +0.05 x.
  - `nudges`: ArrowRight gives x +0.01.
  - `oneDragOneUndo`: a drag, then Ctrl+Z, gives exactly the `at` before it.
  - `emptyDragOrbits`: a drag on empty space changes the camera's yaw and no part.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL (`selected` stays null).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer, the handles - move on the guide's plane, the ring turns,
  the square scales, the arrow lifts, a placeable's use point drags; a part rests on what's below (nwn5e-ee #39)".

### Task A13: Save and build in the page (nwn5e-tools)

Needs A12.

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/app.js` (if routing needs more than A10 gave),
  `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Save and build:** `POST /api/composer/save { id, project, base }` (`toComposition(...)` for the Floating Disk,
  `toProject(...)` for the others).
- **200:** the status says:
  - **Floating Disk:** "Saved and rebuilt in <folder> (<branch>): <files rebuilt>, <n> unchanged. For the game: the
    animation session commits it on anim; the game session merges and installs."
  - **Placeable:** "Saved and built in <folder> (<branch>): <model files> and placeables.<name>.txt (rows <rows>). For
    the game: as the disk's - commit on anim, merge, install."
  - **Spell mesh:** "Saved your meshes <models> in vfxedit\saved. The game writes their models when it runs. Use one as
    an emitter's Chunk model, or with + mesh in an effect."

  The Composer card shows each variant's parts, triangles and height from `result`. The project takes the saved text
  (a placeable's rows), and the page isn't dirty after.
- **409 `changed`:** asks "<file> changed on disk since you opened it: save over it?" Yes posts again with `force`. No
  leaves it, and **Reload** (in the Composer card) reads the file again, after asking about unsaved changes.
- **422 / 400:** the status says "Not built: <message>. Nothing was written." in the bad style.
- **Dirty:** the snapshot differs from the one last opened or saved. The title starts `•`, and leaving or reloading asks.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `saveRebuilds` (Floating Disk): a part moved on Medium, then Save, gives a good status. The copy's composition.json
    parses to the page's. `last.result.files` has `c_n5fd_med.mdl` rebuilt, and `dirty` is false.
  - `placeableSaves`: `testpile` saved, the hook's variants have rows 14100 and 14101, and the status names
    `plc_n5testpile_1.mdl`.
  - `spellMeshSaves`: `testmesh` saved, the status names `n5m_testmesh`. After Back to effects, `vfxBench`'s server state
    lists it among the chunks.
  - `saveRefusedSaysWhy`: `n5_testcrate.mdl` added, then deleted from disk, then Save, gives a bad status matching `/no
    such file/`. The project file is unchanged.
  - `changedOnDiskAsks`: the check rewrites the copy's file, then Save with `confirm` answering false. The status says it
    changed, and nothing is written.
  - `reloadReadsTheFile`: Reload shows the file's project.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the new steps FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: `all passed`.
  - `node vfx_bench/check_composer_server.js`. Expected: `all composer server checks pass`.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer, Save and build - per kind, the generator's summary, a
  changed file asked about, a refused build shown and nothing written (nwn5e-ee #39)".

### Task A14: DDS textures (nwn5e-tools)

Needs A5 and A9.

**Files:**
- Modify: `vfx_bench/public/core.js` (`decodeDds`), `vfx_bench/composer_server.js` (the texture route),
  `vfx_bench/public/composer_page.js` and `vfx_bench/public/app.js` (decode by the header),
  `vfx_bench/check_composer.js`, `vfx_bench/check_composer_server.js`, `vfx_bench/composer_check.mjs`

**Interfaces:**
- **`core.decodeDds(bytes)`** gives `{ width, height, data }` (RGBA, rows top-down as `decodeTga`'s), mip 0 only. It
  reads:
  - Bioware's 20-byte header (u32 width, u32 height, u32 bytes a pixel: 3 is DXT1, 4 is DXT5; u32 size; f32);
  - a standard `DDS ` header with FourCC `DXT1` / `DXT5`.
- **`GET /api/composer/texture/<name>`** serves the DDS when the game has one (what the game draws, often the bigger
  copy), with `X-Texture: dds`, else the TGA (`X-Texture: tga`). The `flat` reason `dds-only` goes.
- **The page's game loader** (A9's `gameImage`, used by the Composer and the effect mode's meshes) decodes by that
  header.

- [ ] **Step 1: Add checks:**
  - `check_composer.js`:
    - `decodeDds: a DXT1 block` (colour0 0xF800, colour1 0x001F, indices 0, gives 4 x 4 red);
    - `decodeDds: a DXT5 block's alpha` (alpha0 255, alpha1 0, indices 1, gives alpha 0);
    - `decodeDds: Bioware's header and the standard one`.
  - `check_composer_server.js`: `ddsServed` (`tno01_post` gives 200 `dds` at 256 x 256; `plc_gold` gives `dds` at 64 x
    64).
  - `composer_check.mjs`: `ddsDraws` (the Floating Disk's Large: `tnp_barrel_002` is drawn textured).
- [ ] **Step 2: Run** the three. Expected: the new steps FAIL (`core.decodeDds is not a function`; 404).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the three, and `node vfx_bench/check.js` (every library texture still decodes). Expected: all
  pass.
- [ ] **Step 5: Commit** with "feat: the VFX Bench reads DDS textures - DXT1 and DXT5, the copy the game draws, for the
  Composer and the effects' meshes (nwn5e-ee #39)".

### Task A15: Play in game for a variant (nwn5e-tools)

Needs A13.

**Files:**
- Modify: `vfx_bench/public/composer_page.js`, `vfx_bench/public/app.js` (Play / Stop in the Composer),
  `vfx_bench/server.js` (`/api/play` takes up to 1 MB), `vfx_bench/composer_check.mjs`

**Interfaces:**
- **Play in game** in the Composer posts the selected variant as a draft: `core.effect({ name: "n5_composer", duration:
  true, seconds: 10, attach: "root", planes })`.
  - There's one `core.plane` per placed mesh. For the Floating Disk the disk mesh (`F.diskMesh`, its texture
    `n5t_forcedisk`) is added last, as its creature model has it.
  - Each is moved +0.85 m along x and +0.15 m along y, beside the character.
  - Permanent applies as in the effect mode.
- **Stop in game** is as now. The status follows the game's answer as the effect mode's does.

- [ ] **Step 1: Add steps** to `composer_check.mjs`:
  - `playSendsTheVariant`: the Floating Disk's Large, Play gives the temp build's `vfxedit\bench\request.vfx` starting
    `request = play`. Its draft reads with 31 planes and 1095 triangles, as `c_n5fd_large` has.
  - `playAnyKind`: `testpile`'s variant 2 gives its parts' planes (no disk).
  - `stopStops`: Stop writes `request = stop`.
- [ ] **Step 2: Run.** Expected: FAIL (no request written).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `node vfx_bench/composer_check.mjs` and `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  The in-game look is the owner's test (CO-6).
- [ ] **Step 5: Commit** with "feat: the VFX Bench's Composer plays a variant in game - what would be built, beside your
  character, before any install (nwn5e-ee #39)".

### Task A16: Mesh beats in the effect editor (nwn5e-tools)

Needs A6, A8 and A9. Its fades work in the game after A4's install.

**Files:**
- Modify:
  - `vfx_bench/public/app.js`, `vfx_bench/public/index.html`, `vfx_bench/public/style.css`;
  - `vfx_bench/public/beats.js`, `vfx_bench/public/parts.js`, `vfx_bench/public/sim.js`;
  - `vfx_bench/check_beats.js`, `vfx_bench/check_effect.js`, `vfx_bench/browser_check.mjs`.

**Interfaces:**
- **`+ mesh`** (`#mesh-new`, under the timeline beside `+ light`) lists the bench's spell meshes (`state.server.meshes`).
  - One picked is read (`/api/draft?source=chunks&name=<mesh>`), and its planes are copied into the open effect.
  - They're named `<mesh>`, `<mesh>2`... (`B.freshBase` / `B.copyName`), with `mesh` set, scale 1, at the effect's
    point, hanging under nothing.
  - They're selected: `state.selection = { kind: "mesh", key }`, `key` the group's.
- **beats.js:**
  - `groupsOf` works on planes too;
  - `rowsOf` adds a row per mesh group (`id = "mesh:" + key`): a bar from its start to its end (start + On for, else the
    effect's end, open for a lasting one), its fades drawn as a light's, `edits: ["move", "start", "end"]`;
  - `retimeMesh(fx, key, { start, end })` sets every plane of the group.
- **parts.js:**
  - `meshKeys(fx, impact, alpha, timing)`: `lightKeys`' rows with alpha in place of the radius, an unset fade out none
    (it goes out at the effect's end);
  - `meshFadesOf(fx, plane)`: the fades read back as `fadesOf` reads a light's.
- **The mesh card** (`#mesh-card`):
  - Hangs under, x y z, Turn (degrees about z: the planes' orientation), Scale;
  - Starts after, On for (0: while the effect plays), Fade in, Fade out;
  - **Refresh from the mesh**, Remove.

  Every change sets every plane of the group and is one undo step. Scale rewrites each plane's verts as its mesh's
  verts times the scale (the mesh's draft read again); `scale` is kept on the plane. Timing writes the planes'
  `start_seconds`, `emit_seconds` and `<anim>_alpha` keys (`meshKeys`).
- **sim.js `planesAt(t)`:** each plane's world triangles at time t. Its parent's place comes from the node places, then
  its own position and turn. Its alpha comes from its keys in the animation playing at t (linear between rows), else its
  own alpha.
- **The stage in the effect mode** draws `sim.planesAt(sim.time)` through `view.drawMeshes` (textures through A9's mesh
  loader). Every effect's planes draw now, a saved one's too.
- **`window.vfxBench.planesDrawn()`:** `{ planes, triangles, alpha: { <name>: a } }`.

- [ ] **Step 1: Add checks.**
  - `check_beats.js`:
    - `mesh rows: a bar per mesh group, its fades`;
    - `retimeMesh moves every plane of the group`.
  - `check_effect.js`:
    - `meshKeys: off until its start, up over the fade in, down over the fade out (as a light's)`;
    - `meshKeys: a lasting effect's always-on mesh fades out in the cessation`;
    - `planesAt: a plane hangs under its node and fades with its keys`.
  - `browser_check.mjs`. The fixture puts `n5m_test` (two planes) in the temp build's `vfxedit\saved\` and
    `vfxedit\meshes.txt`. Its steps:
    - `meshBeatAdds`: + mesh, `n5m_test`, gives 2 planes added, selected, a `mesh:` row;
    - `meshBeatDrawn`: Starts after 0.5 gives `planesDrawn()` 0 triangles at 0.2 s and more than 0 at 0.8 s;
    - `meshBeatTiming`: dragging the bar moves every plane's `start_seconds`; Fade in 0.3 writes `impact_alpha` rows
      rising over 0.3 s in the draft;
    - `meshBeatScale`: Scale 2 gives every vert twice the mesh's;
    - `meshBeatUndo`: Ctrl+Z after Scale gives the verts back;
    - `meshBeatRefresh`: the mesh's draft changed on disk, then Refresh from the mesh, gives the new verts.
- [ ] **Step 2: Run** the three. Expected: the new steps FAIL (`B.retimeMesh is not a function`,
  `parts.meshKeys is not a function`, no `#mesh-new`).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_beats.js` (`all beats checks pass`);
  - `node vfx_bench/check_effect.js` (`all effect checks pass`);
  - `node vfx_bench/check_parts.js` (`all parts checks pass`);
  - `node vfx_bench/browser_check.mjs` (`all passed`);
  - `node vfx_bench/check.js` (as before).
- [ ] **Step 5: Commit** with "feat: the VFX Bench's mesh beats - a spell mesh placed, turned and scaled in an effect,
  with Starts after, On for and fades as a light's; the stage draws an effect's meshes (nwn5e-ee #39)".

### Task A17: The READMEs, TOOLS-PLAN and the owner's tests

**Files:**
- Modify:
  - `nwn5e-tools/vfx_bench/README.md`: "The Composer" (projects, kinds, variants, guide, outputs, Play in game) and
    "Mesh beats", the options `--ee` / `--game`, and the new files;
  - nwn5e-ee-anim `tools/data/floatdisk/README.md`: "Compose it in the VFX Bench" at the top;
  - nwn5e-ee-anim `tools/data/composer/README.md`: the bench's side;
  - nwn5e-ee-anim `docs/TOOLS-PLAN.md`: a section, "The VFX Bench's Composer and mesh beats, built".
- Create: the owner's test rows, a CSV in the job's tmp folder; the parent session adds them to the board. Each row
  stands on its own: the bench restart, what to do, what to see.
  - **CO-1, open it:** close the VFX Bench window and double-click `vfx_bench.bat`, then press Composer. Expect the
    Floating Disk's Small (two bags on a 0.9 m circle at 0.9 m, no disk) and "Saving to ...nwn5e-ee-anim (anim)".
  - **CO-2, build:** on Medium, search "barrel", click it, drag it, turn it with the ring, stack a bag on it.
  - **CO-3:** the onion skin on Large; Undo and Redo across variants.
  - **CO-4, a placeable:** New project, Placeable; a few parts; move the use point; Blocks. Save and build, and read the
    files and rows it names.
  - **CO-5, a spell mesh:** New project, Spell mesh. Save, then in an effect use it as a Chunk model and with + mesh
    (Starts after, fades).
  - **CO-6, Play in game:** a variant beside your character (the game running, a character in play).
  - **After the install carrying A4:** a mesh beat fades in the game. Before it, it shows for the whole effect.

- [ ] **Step 1: Run every check once more:**
  - nwn5e-ee-anim:
    - `node tools/data/floatdisk/check.js` (`floatdisk check: 27 checks pass`);
    - `node tools/data/floatdisk/check_build.js` (`build check: all pass`);
    - `node tools/data/composer/check.js` (`composer check: 13 checks pass`);
    - `node tools/data/composer/check_build.js` (`composer build check: all pass`);
    - `cmd //c test.bat` (`<n> tests, 0 failed`).
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
  - In nwn5e-tools: `vfx_bench/README.md`, with "docs: the VFX Bench's Composer and mesh beats in the README (nwn5e-ee
    #39)".
  - In the anim worktree, after `git branch --show-current` (anim): `tools/data/floatdisk/README.md
    tools/data/composer/README.md docs/TOOLS-PLAN.md`, with "docs: the Composer and mesh beats - READMEs, TOOLS-PLAN
    (#39)".
- [ ] **Step 4: Report:**
  - the commits and the test rows;
  - the bench changes need a restart of `vfx_bench.bat`;
  - the "anim ready" must name A4's `src/data/vfx_model.*` change: the mesh beats' fades need the game session's next
    install;
  - the generators' changes build nothing new for the disk.

---

## Addendum 2026-11-17: PLT parts baked

**Spec:** the design's "Addendum 2026-11-17: PLT parts baked". The owner: "helms don't have textures? Not in the bench
and not in-game", then "go with A, per-part colours".

Four tasks, after A17: **P1** the bake (anim), **P2** the generators bake (anim), **P3** the server (nwn5e-tools), **P4**
the page (nwn5e-tools). Each task's full text is here.

### Before starting

- **The owner's piles are saved but uncommitted** in the anim worktree (2026-11-17): `tools/data/floatdisk/composition.json`
  and the 8 rebuilt `data/files/c_n5fd_*` / `plc_n5fd_*` files. Their Large holds `helm_001`.
  - P1 doesn't touch them.
  - **P2 rebuilds them, so it starts only once they are committed** (the coordinator commits them as their own commit).
    If `git status --short` lists any of them when P2 starts, stop and report.
- **Known answers** (computed 2026-11-17 by a prototype of P1's functions on the game's files):
  - `bakedName("helm_001", the defaults)` is `n5p_2cd3d725dedb`;
  - with `metal1` 5 it is `n5p_f15efdade06c`;
  - helm_001 baked is 128 x 128, so its TGA is 49170 bytes (18 + 128 x 128 x 3).

### Global constraints (added)

The plan's Global Constraints and the first addendum's hold, plus these:

- **No baked texture is ever written into a repo or committed** (CLAUDE.md: "Game files (and art made from them) ... are
  never committed"). The generators write them only into the `--saved` they're given: the bench build's
  `vfxedit\saved\`, or a check's temporary folder.
  - `git status --short` after each anim task shows no `n5p_*` file.
- **No check writes the real bench build** (`..\nwn5e-ee\build`). The generators' checks pass `--saved <their copy>` and
  compare the real `vfxedit\saved\` listing before and after.
- **The disk's repo files stay 13.** A stage holding a dyed part changes its models, and nothing else changes.
- **The Composer's checks read the anim worktree's composition** (composer_fixture copies it). The owner's piles of
  2026-11-17 changed Small and Large, so steps that count the committed piles' parts (variantsAndCounts,
  playSendsTheVariant's 31 planes and 1095 triangles...) may already be red.
  - P3 and P4 run their checks once before any change (step 0) and keep the ok / FAIL list.
  - "Expected" after a change means: every step that was ok in step 0 is ok, and the new steps are ok.

### Review Focus (added)

13. **What you see is what's built, for dyed parts.**
    - The page's bake of helm_001 equals reader.js's (P4 `pltBakeIsTheGenerators`).
    - The server's bake for Play is byte for byte the page's function applied to the served PLT and palettes (P3
      `bakeForPlay`).
    - Play's draft names the textures the built model names (P4 `playSendsTheVariant`).
14. **Nothing of the game's in a repo:** P2's checks and `git status`; no `n5p_` in `data/` or made.txt.
15. **The real build untouched by checks:** P2's real-saved comparison; `realWorktreeUntouched` (it already compares the
    real `vfxedit\saved\` listing).
16. **A default colour isn't written:** P1 `cleanColours`, P4 `pltColoursRebake` (toSave after an undo).

### Order

1. **P1:** floatdisk.js - a part's colours, the PLT reader, the bake, the name, the TGA. Anim; no game needed.
2. **P2:** the generators bake - reader.js, files.js, both build.js (`--saved`), both check_build.js, and the real
   rebuild of the disk's Large. Anim; needs the game and the owner's piles committed.
3. **P3:** the server - the PLT served, Play's bake route, Save's `--saved`, the thumbnails' v2. nwn5e-tools.
4. **P4:** the page - the dyed meshes renamed, the bake loader, the Colours card, Play's bakes; the READMEs, TOOLS-PLAN
   and the owner's test rows. nwn5e-tools, and TOOLS-PLAN on anim.

---

### Task P1: The bake - a part's colours, the PLT reader, the painting, the name, the TGA (nwn5e-ee, anim)

Working folder: the anim worktree (`/c/Users/Bruno/Desktop/Projects/nwn5e-ee-anim`). Needs nothing else.

**Files:**
- Modify: `tools/data/floatdisk/floatdisk.js`, `tools/data/floatdisk/check.js`, `tools/data/composer/check.js`,
  `tools/data/floatdisk/README.md` (a part's `colours`)

**Interfaces** (floatdisk.js, all pure: they load in a page as the rest does, no Buffer but in `tgaBytes`' return):
- **Constants:**
  - `PLT_LAYERS = ["skin", "hair", "metal1", "metal2", "cloth1", "cloth2", "leather1", "leather2", "tattoo1",
    "tattoo2"]`: a PLT's layer ids 0-9;
  - `PLT_PALETTES = { skin: "pal_skin01", hair: "pal_hair01", metal1: "pal_armor01", metal2: "pal_armor02", cloth1:
    "pal_cloth01", cloth2: "pal_cloth01", leather1: "pal_leath01", leather2: "pal_leath01", tattoo1: "pal_tattoo01",
    tattoo2: "pal_tattoo01" }`;
  - `PLT_FIELDS = ["metal1", "metal2", "leather1", "leather2", "cloth1", "cloth2"]`: the card's, in the owner's order;
  - `PLT_LABELS = { metal1: "Metal 1", metal2: "Metal 2", leather1: "Leather 1", leather2: "Leather 2", cloth1: "Cloth 1",
    cloth2: "Cloth 2" }`;
  - `PLT_DEFAULTS = { skin: 0, hair: 0, metal1: 0, metal2: 3, cloth1: 22, cloth2: 23, leather1: 3, leather2: 2,
    tattoo1: 0, tattoo2: 0 }`;
  - `PLT_MAX = 175`;
  - `BAKED = /^n5p_[0-9a-f]{12}$/`.
- **`readColours(c, where)`** gives the ten layers' colours: `PLT_DEFAULTS` with `c`'s over them.
  - `c` undefined or null gives the defaults.
  - Otherwise `c` must be a plain object whose keys are among `PLT_FIELDS`, each a whole number 0-175.
  - Else it throws `` `${where}: { "metal1": 0-175, ... } - metal1, metal2, leather1, leather2, cloth1 or cloth2, each a
    palette number 0-175` ``.
- **`readPart`** gives `colours: readColours(p.colours, `${where}.colours`)` too. Nothing else changes; `partKey` doesn't
  look at colours (a part's meshes don't hang on them).
- **`cleanColours(c)`** gives the `PLT_FIELDS` whose value differs from `PLT_DEFAULTS`, in `PLT_FIELDS`' order, or
  `undefined` when none does (or `c` is undefined).
- **`readPlt(bytes)`** (a Buffer or Uint8Array) gives `{ width, height, shade, layer }`.
  - `shade` and `layer` are Uint8Arrays of width x height, in the file's order: its first row is the picture's bottom.
  - The header is `PLT V1  `; the width is the u32 at 16 and the height the u32 at 20, read little-endian by hand.
  - Errors: `not a PLT (no "PLT V1  " header)`; `` `a PLT cut short: ${w} x ${h} needs ${24 + 2 * w * h} bytes, it has
    ${n}` ``.
- **`bakePlt(plt, palettes, colours)`** gives `{ width, height, data }`: a Uint8ClampedArray RGBA, rows top-down (as
  core.decodeTga gives a picture).
  - `palettes` is `{ <palette name>: { width, height, data } }`, decoded top-down.
  - The pixel at column x of the picture's row y is file row `height - 1 - y`'s pixel `i`. Its layer `L =
    PLT_LAYERS[layer[i]]` (a layer id past 9 counts as `skin`). From `palettes[PLT_PALETTES[L]]` it takes the RGB at row
    `min(colours[L], its height - 1)`, column `shade[i]`. Alpha is 255.
  - A palette missing throws `` `${name}: a palette the bake needs` ``.
- **`bakedName(plt, colours)`** gives `"n5p_" + cyrb53(key).toString(16).padStart(14, "0").slice(-12)`.
  - `key` is `` `${plt.toLowerCase()}:${PLT_LAYERS.map((l) => full[l]).join(",")}` ``, where `full` is `Object.assign({},
    PLT_DEFAULTS, colours)`.
  - cyrb53, verbatim:

    ```js
    function cyrb53(str, seed = 0) {
      let h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
      for (let i = 0, ch; i < str.length; i++) { ch = str.charCodeAt(i); h1 = Math.imul(h1 ^ ch, 2654435761); h2 = Math.imul(h2 ^ ch, 1597334677); }
      h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507); h1 ^= Math.imul(h2 ^ (h2 >>> 13), 3266489909);
      h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507); h2 ^= Math.imul(h1 ^ (h1 >>> 13), 3266489909);
      return 4294967296 * (2097151 & h2) + (h1 >>> 0);
    }
    ```
- **`tgaBytes(image)`** gives a 24-bit uncompressed TGA of a top-down RGBA picture.
  - The header is `[0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w & 255, w >> 8, h & 255, h >> 8, 24, 0]`: no image ID, rows
    bottom-up as the game's.
  - Then BGR, the picture's bottom row first.
  - It returns a Buffer under Node (`typeof Buffer === "function"`), else the Uint8Array, as `diskTexture` does.
- **`bakeBitmaps(meshes, colours, isPlt)`** gives `{ meshes, bakes }`.
  - `meshes` are copies: a mesh whose lower-case bitmap `isPlt(bitmap)` holds gets `bitmap: bakedName(bitmap,
    colours)`; the others keep theirs. The input isn't changed.
  - `bakes` is `[{ name, plt, colours: cleanColours(colours) || {} }]`, each name once, in the meshes' order.
- **`compositionText`** writes a plain object value inline as `{ "metal1": 5 }` (keys in the object's order), as
  project.js's `projectText` does. Arrays and the rest are as before, and `colours` follows a part's known keys.
- **The api** gains every name above (cyrb53 stays private).

- [ ] **Step 1: Write the failing checks.** In `tools/data/floatdisk/check.js` (its `check(name, fn)` style), with these
  helpers:
  - `pltOf(w, h, pixels)`: a Buffer, `PLT V1  ` with u32 10 at 8, 0 at 12, w at 16, h at 20, then each `[shade, layer]`;
  - `PLT2 = pltOf(2, 2, [[10, 2], [20, 3], [30, 4], [40, 6]])`;
  - `palette(id)`: 256 x 176, each pixel `[row, column, id, 255 - id]`;
  - `PALS`: `pal_armor01` 1, `pal_armor02` 2, `pal_cloth01` 3, `pal_leath01` 4, `pal_skin01` 5, `pal_hair01` 6,
    `pal_tattoo01` 7.

  The checks:
  - `a part's colours: the six fields as palette numbers, the defaults filled`:
    - `{ model: "helm_001", at: [0, 0, 0] }` reads `colours` deep-equal to `F.PLT_DEFAULTS`;
    - with `colours: { metal1: 5, cloth2: 40 }`, the defaults with those two;
    - `{ metal1: 176 }`, `{ metal1: 1.5 }`, `{ skin: 3 }` and `[]` each throw `/stages\.small\[0\]\.colours: .*0-175/`.
  - `cleanColours: only what differs from the defaults, in the card's order`:
    - `{ cloth2: 40, metal1: 0, metal2: 5 }` gives `{ metal2: 5, cloth2: 40 }`, keys in that order;
    - `F.PLT_DEFAULTS` and `undefined` give `undefined`.
  - `readPlt: the header, the size, a shade and a layer a pixel, in the file's order`:
    - PLT2 gives width 2, height 2, shade `[10, 20, 30, 40]`, layer `[2, 3, 4, 6]` (the same from a Uint8Array);
    - `PLT V2  ` throws `/not a PLT/`;
    - PLT2 less its last byte throws `/cut short: 2 x 2 needs 32 bytes, it has 31/`.
  - `bakePlt: each pixel its layer's palette, the row its colour, the column its shade; the first row at the bottom`:
    - with `readColours({ metal1: 7, metal2: 8, cloth1: 9, leather1: 11 })`, PLT2's data is `[9, 30, 3, 255, 11, 40, 4,
      255, 7, 10, 1, 255, 8, 20, 2, 255]`;
    - a layer-12 pixel `[5, 12]` gives `[0, 5, 5, 255]` (skin's palette, row 0);
    - `metal1` 200 (given directly) takes row 175;
    - without `pal_armor02` it throws `/pal_armor02: a palette the bake needs/`.
  - `bakedName: n5p_ and 12 hex digits, one per PLT and colours`:
    - `bakedName("helm_001", F.PLT_DEFAULTS)` and `bakedName("HELM_001", {})` are `n5p_2cd3d725dedb`;
    - `bakedName("helm_001", { metal1: 5 })` is `n5p_f15efdade06c`;
    - `helm_002`'s differs;
    - each matches `F.BAKED`.
  - `tgaBytes: a 24-bit TGA, rows bottom-up as the game's`: of PLT2's bake (the colours above), a Buffer of 30 bytes. Its
    header is `[0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 0]`, then `[1, 10, 7, 2, 20, 8, 3, 30, 9, 4, 40, 11]`.
  - `bakeBitmaps: a PLT-only texture named for its colours, the others kept, each bake once`: meshes with bitmaps
    `helm_001`, `plc_x`, `HELM_001` and `""`, with `isPlt = (b) => b === "helm_001"` and `readColours({ metal1: 5 })`.
    - The bitmaps become `n5p_f15efdade06c`, `plc_x`, `n5p_f15efdade06c`, `""`.
    - `bakes` is `[{ name: "n5p_f15efdade06c", plt: "helm_001", colours: { metal1: 5 } }]`.
    - The input meshes keep their bitmaps.
  - `compositionText: a part's colours inline, after its known keys`: the small stage's one part `{ colours: { metal1: 5
    }, model: "helm_001", at: [0, 0, 0], tilt: [0, 90] }` gives the line `      { "model": "helm_001", "at": [0, 0, 0],
    "tilt": [0, 90], "colours": { "metal1": 5 } }`.
  - `the bake in a page: the same name and bytes`: floatdisk.js loaded through `new Function("self", "require",
    "module", "Buffer", src)`.
    - Its `tgaBytes(bakePlt(readPlt(new Uint8Array(PLT2)), PALS, colours))` is a Uint8Array, not a Buffer, with Node's
      bytes.
    - Its `bakedName("helm_001", {})` is Node's.

  In `tools/data/composer/check.js`: `readProject: a part's colours, read as the disk's`.
  - A placeable whose part has `colours: { leather1: 7 }` reads `variants[0].parts[0].colours.leather1` 7 and `.metal2` 3.
  - `{ leather1: -1 }` throws `/variants\[0\]\.parts\[0\]\.colours: /`.
- [ ] **Step 2: Run them.**
  - `node tools/data/floatdisk/check.js`. Expected: nine FAIL lines, among them `FAIL readPlt: ... F.readPlt is not a
    function`, then `floatdisk check: FAILED (27 passed)`.
  - `node tools/data/composer/check.js`. Expected: `FAIL readProject: a part's colours, read as the disk's: ...`, then
    `composer check: FAILED (13 passed)`.
- [ ] **Step 3: Implement** the interfaces in floatdisk.js. In README.md's part description, add `colours`: "a dyed part's
  (a helmet's: its texture is a PLT) colours, palette numbers 0-175 for metal1, metal2, leather1, leather2, cloth1 and
  cloth2; left out, the defaults (steel, a darker steel trim, browns, dark grey)".
- [ ] **Step 4: Run.**
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 36 checks pass`.
  - `node tools/data/composer/check.js`. Expected: `composer check: 14 checks pass`.
  - `node tools/data/floatdisk/build.js --check`. Expected: the summary as before (no part has colours, and nothing bakes
    yet).
  - `node tools/data/floatdisk/check_build.js`. Expected: `build check: all pass`.
  - `node tools/data/composer/check_build.js`. Expected: `composer build check: all pass`.
  - `git status --short`. Expected: this task's four files modified; the owner's piles as they were.
- [ ] **Step 5: Commit.**
  - `git branch --show-current` (anim);
  - `git commit --only tools/data/floatdisk/floatdisk.js tools/data/floatdisk/check.js tools/data/composer/check.js
    tools/data/floatdisk/README.md -F -`, with "feat: the Composer's dyed parts, the bake - floatdisk.js reads a PLT and
    paints it from the game's palettes into a texture of its own (n5p_ and a hash of its colours); a part's colours
    (#39)".

### Task P2: The generators bake - reader.js, files.js, both build.js (nwn5e-ee, anim)

Working folder: the anim worktree. Needs P1, the game, and **the owner's piles committed** (Before starting).

**Files:**
- Modify:
  - `tools/data/floatdisk/reader.js`, `tools/data/floatdisk/files.js`;
  - `tools/data/floatdisk/build.js`, `tools/data/composer/build.js`;
  - `tools/data/floatdisk/check_build.js`, `tools/data/composer/check_build.js`;
  - `tools/data/floatdisk/README.md`, `tools/data/composer/README.md` (dyed parts, `--saved`).
- Rebuilt by Step 4's real build: the models of the stages holding a dyed part. With the owner's piles of 2026-11-17
  that's `data/files/c_n5fd_large.mdl` and `data/files/plc_n5fd_large.mdl`.

**Interfaces:**
- **reader.js:**
  - `image(name, dirs)`'s `plt` is the PLT's bytes (ovr\ first, then `res(name, 6)`), or null; it was true / false. A
    texture beside a file has `plt: null`.
  - **`pltOnly(name)`:** true when the game has `name` (lower-cased) only as a PLT: `image(name, [])` is the game's with
    `plt` and neither `tga` nor `dds`. Kept per name.
  - **`bake(plt, colours)`** gives `{ name: F.bakedName(plt, colours), image: F.bakePlt(F.readPlt(PLT), palettes,
    colours), bytes: F.tgaBytes(image) }`.
    - The palettes are read once per reader through `image(pal, [])`: a DDS by `core.decodeDds`, else the TGA by
      `core.decodeTga`.
    - Errors: `` `${plt}: not a texture the game has only as a PLT` ``; `` `${pal}: no such palette in the game` ``.
- **files.js:**
  - **`bakeParts(placed, rd, baked = new Map())`:** for each `x` of `placed` (`{ part, meshes }`, placePart's with its
    part), `b = F.bakeBitmaps(x.meshes, x.part.colours, rd.pltOnly)` and `x.meshes = b.meshes`. Each bake `baked` lacks is
    set: `name -> { plt, colours (b's), bytes: rd.bake(plt, x.part.colours).bytes, label: F.partLabel(x.part) }`.
    Returns `baked`.
  - **`shipTextures(placed, rd, baked = new Map())`** passes over a bitmap `baked` has (it was renamed, so the game
    lookup would miss it).
  - **`ownership`** knows the kind `baked`: ours when the file's name without `.tga` matches `F.BAKED`. The name fixes
    the bytes, and only the bake makes such names.
  - **`bakedWrites(baked, saved)`** gives `[{ file: <saved>\<name>.tga, bytes, kind: "baked" }]`.
  - **`needSaved(baked, saved, check)`** throws when `baked` has any, `saved` is empty and it isn't `--check`: `` `${label}:
    its texture ${plt} is dyed (a PLT) - its baked copy (${name}.tga) goes into the bench build's vfxedit\saved: give
    --saved <folder>` ``.
- **floatdisk build.js:**
  - **`--saved <folder>`:** where the baked textures go; no default.
  - In each stage, right after its parts are placed, `W.bakeParts(parts, rd, baked)`, before the creature's and the
    pile's meshes are made of them. `shipTextures` gets `baked`.
  - Then `W.needSaved(...)`, before anything is written.
  - The writes gain `W.bakedWrites(baked, SAVED)`.
  - The text summary gains, per baked texture: `` `  texture ${name}: ${plt} dyed with metal 1 0, metal 2 3, leather 1 3,
    leather 2 2, cloth 1 22, cloth 2 23 - baked into ${SAVED}` `` (the part's numbers, by `PLT_FIELDS` and
    `PLT_LABELS` lower-cased).
  - `--json` gains `baked: [{ texture, plt, colours }]` (`colours` as bakeBitmaps gives it).
  - The files' states name a saved file in full (outside the repo).
- **composer build.js:** the same, in `built`, before its meshes are lowered. Baked textures go into `--saved` for both
  kinds (a spell mesh already needs it).
- **Neither** puts a baked texture in made.txt or `data\`.

- [ ] **Step 1: Write the checks.**
  - **floatdisk `check_build.js`:**
    - `SAVED = <root>\saved` (made empty);
    - `build(args, saved = SAVED)` adds `--saved <saved>` when `saved` is given;
    - `snapshot(dirs = [FILES, ROWS, SAVED])`;
    - `REAL_SAVED = <NWN5E_BUILD or ..\nwn5e-ee\build>\vfxedit\saved`, its listing taken when the check starts.

    The first check becomes `a full rebuild of the committed composition changes nothing (13 repo files unchanged)`:
    - the lines naming `data/...` are 13, all `unchanged`;
    - `snapshot([FILES, ROWS])` is as before;
    - a baked texture (the owner's helmet) may be `new` in SAVED.

    New, last:
    - `a dyed part is baked into --saved; the models name it, data\ doesn't hold it`: a composition whose medium is
      `helm_001` at `[0, 0, 0]` and `helm_001` at `[0.2, 0, 0]` with `colours: { metal1: 5 }`, built with `--json`.
      - `j.baked` deep-equals `[{ texture: "n5p_2cd3d725dedb", plt: "helm_001", colours: {} }, { texture:
        "n5p_f15efdade06c", plt: "helm_001", colours: { metal1: 5 } }]`.
      - The copy's `c_n5fd_med.mdl` and `plc_n5fd_med.mdl` contain `\r\n  bitmap n5p_2cd3d725dedb\r\n` and `\r\n  bitmap
        n5p_f15efdade06c\r\n`, and no `bitmap helm_001`.
      - SAVED holds both, 49170 bytes each, with byte 16 being 24. Its `n5p_2cd3d725dedb.tga` equals
        `reader.create({ game, core, gameData, reader: null }).bake("helm_001", F.readColours()).bytes`.
      - No `n5p_` file in FILES; made.txt as it was.
      - Built again: both `unchanged`.
      - The real `vfxedit\saved\` listing as at the start.
    - `a dyed part without --saved is refused, nothing written`: the same composition, built with `saved` null.
      - Exit 1, the error matching `/helm_001: its texture helm_001 is dyed \(a PLT\).*give --saved/`.
      - `snapshot()` unchanged.
  - **composer `check_build.js`**, new, last, each build with `--saved SAVED` but the third:
    - `a placeable's dyed part is baked into --saved; its model names it`: `chkhelm` (placeable, row 14150) with
      `helm_001` and `helm_011` (`colours: { leather1: 7 }`).
      - `j.baked`'s textures are `n5p_2cd3d725dedb` and `F.bakedName("helm_011", { leather1: 7 })`.
      - `plc_n5chkhelm.mdl` names both.
      - SAVED holds both; FILES and made.txt hold neither.
      - Built again: every state `unchanged`.
    - `a spell mesh's dyed part: its draft names the baked texture beside it`: `chkhelmm` (spell mesh, `helm_001`).
      - Its draft's plane bitmap is `n5p_2cd3d725dedb`.
      - The texture's state is `unchanged`: the same file, ours by its name.
    - `a dyed part without --saved is refused, nothing written`: `chkhelm` without `--saved` exits 1 with `/give
      --saved/`, and `snapshot()` is unchanged.
- [ ] **Step 2: Run them.** (Before Step 3 the generators bake nothing, so nothing reaches any `saved\`.)
  - `node tools/data/floatdisk/check_build.js`. Expected: the two new checks FAIL (`j.baked` undefined, `bitmap helm_001`
    still there; exit 0 where 1 was expected), then `build check: 2 failed`.
  - `node tools/data/composer/check_build.js`. Expected: the three new checks FAIL, then `composer build check: 3 failed`.
- [ ] **Step 3: Implement.** The READMEs:
  - floatdisk: "Dyed parts", with the command `node tools\data\floatdisk\build.js --saved ..\nwn5e-ee\build\vfxedit\saved`
    for a composition with one;
  - composer: `--saved` for dyed parts.
- [ ] **Step 4: Run, then the real rebuild.**
  - `node tools/data/floatdisk/build.js --check`. Expected: the summary, plus `  texture n5p_2cd3d725dedb: helm_001 dyed
    with metal 1 0, metal 2 3, leather 1 3, leather 2 2, cloth 1 22, cloth 2 23 - baked into ...`.
  - **The real rebuild:** `node tools/data/floatdisk/build.js --saved ../nwn5e-ee/build/vfxedit/saved` (the bench
    build's own folder, where the bench's Save writes it too). Expected:
    - `rebuilt` for `data/files/c_n5fd_large.mdl` and `data/files/plc_n5fd_large.mdl`;
    - `unchanged` for the other 11 `data/` files;
    - `new` (or `unchanged`, if the bench baked it first) for
      `C:\Users\Bruno\Desktop\Projects\nwn5e-ee\build\vfxedit\saved\n5p_2cd3d725dedb.tga`.

    If the owner's piles changed since, the stages holding a dyed part are the ones rebuilt.
  - `node tools/data/floatdisk/check_build.js`. Expected: `build check: all pass`.
  - `node tools/data/composer/check_build.js`. Expected: `composer build check: all pass`.
  - `node tools/data/floatdisk/check.js` (36) and `node tools/data/composer/check.js` (14): all pass.
  - `git status --short`. Expected: the eight files above and the rebuilt models; no `n5p_` anywhere; made.txt unchanged.
- [ ] **Step 5: Commit.**
  - `git branch --show-current` (anim);
  - `git commit --only` the eight files and the rebuilt `data/files/*.mdl` `-F -`, with "feat: the Composer's dyed parts,
    the generators - a PLT-only part's texture baked from your game into the bench build's vfxedit\saved (--saved),
    never into the repo; the models name it; the Floating Disk's Large rebuilt for its helmet (#39)".
  - The next "anim ready" says: the disk's Large names `n5p_2cd3d725dedb`; the game shows it through the dev layer's
    `saved\` folder (nothing else to install).

### Task P3: The server - the PLT served, Play's bake, Save's --saved, thumbnails v2 (nwn5e-tools)

Working folder: `/c/Users/Bruno/Desktop/Projects/nwn5e-tools`. Needs P1 and P2.

**Files:**
- Modify: `vfx_bench/composer_server.js`, `vfx_bench/check_composer_server.js`, `vfx_bench/composer_check.mjs` (only
  `thumbnailDrawnAndKept`'s path)

**Interfaces:**
- **A part's textures:** a PLT-only texture is `{ draw: "plt", why: "", shine: false }`. `flat` is only for one the game
  doesn't have (`missing`).
- **`GET /api/composer/texture/<name>`:**
  - a PLT-only texture is 200 with its PLT's bytes and `X-Texture: plt`;
  - without `?file=`, the lookup beside goes through the parts folders, then `<build>\vfxedit\saved\` (a baked texture:
    the effect mode's spell meshes and mesh beats draw it);
  - the rest is as now.
- **`POST /api/composer/bake`** (from the page; `application/json`, up to 65536 bytes) takes `{ textures: [{ plt,
  colours }] }`, 1 to 64 of them. For each, in order:
  - `plt` must be a resref and `pltOnly`. Else 400: `` `textures[${i}].plt: ${plt} isn't a texture the game has only as
    a PLT` ``.
  - `colours` goes through `F.readColours(colours, `textures[${i}].colours`)`; its error is a 400.
  - Then `loadImages().bake(plt, colours)`. Its file `<build>\vfxedit\saved\<name>.tga` is written whole when missing or
    different.
  - Answer: 200 `{ ok: true, textures: [{ plt, name, state: "new" | "rebuilt" | "unchanged" }] }`. The server logs
    `composer: baked <names> for Play in game`.
  - No Origin from the page: 403. More than 64: 400 `at most 64 textures`.
- **Save:** every kind's build gets `--saved <build>\vfxedit\saved`: the Floating Disk's, a placeable's and a spell
  mesh's (as now).
- **Thumbnails:** `<build>\vfxedit\composer\thumbs\v2\`. The first folder stays, the bench's own cache.

- [ ] **Step 0: Record today's runs.** `node vfx_bench/check_composer_server.js > /tmp/p3_server_before.txt` and `node
  vfx_bench/composer_check.mjs > /tmp/p3_browser_before.txt`.
- [ ] **Step 1: Add steps** to `check_composer_server.js`.
  - `pltServed`:
    - `texture/helm_001` is 200 with `X-Texture: plt`; its bytes equal the copy's reader.js `image("helm_001",
      []).plt`, and `F.readPlt` of them is 128 x 128;
    - `part?models=helm_001`'s `textures.helm_001` deep-equals `{ draw: "plt", why: "", shine: false }`;
    - `texture/pal_armor01` is 200 `tga`, decoding at 256 x 176.
  - `bakeForPlay`: POST `{ textures: [{ plt: "helm_001", colours: {} }, { plt: "helm_001", colours: { metal1: 5 } }] }`.
    - It gives 200, the names `n5p_2cd3d725dedb` and `n5p_f15efdade06c`, both `new`.
    - `<tempBuild>\vfxedit\saved\` holds both, 49170 bytes each.
    - The first equals `F.tgaBytes(F.bakePlt(F.readPlt(<the served PLT>), <the seven palettes decoded from their served
      TGAs>, F.readColours({})))`: the page's path.
    - Posted again: both `unchanged`, their times kept.
    - No Origin: 403. `{ plt: "plc_rugdmtlbrk" }`: 400 `/only as a PLT/`. `colours: { metal1: 176 }`: 400 `/0-175/`.
  - `bakedServedFromSaved`: `texture/n5p_2cd3d725dedb` is 200 `tga`, its bytes the saved file's.
  - `saveBakesIntoSaved`, after the other save steps: the copy's Floating Disk with `{ note: "a helmet", model:
    "helm_001", at: [0.2, 0.2, 0] }` added to Medium.
    - It gives 200, and `result.baked` has `n5p_2cd3d725dedb`.
    - The copy's `data/files/c_n5fd_med.mdl` contains `  bitmap n5p_2cd3d725dedb`.
    - `<tempBuild>\vfxedit\saved\n5p_2cd3d725dedb.tga` is there.
    - No `n5p_` file is in the copy's `data/files`.
  - **Changed:** `saveWritesAndRebuilds` and `saveUnchangedIsUnchanged` count only the states of `data/` files (13). A
    baked texture's own state, outside the repo, may be `new` on the first save.

  In `composer_check.mjs`, `thumbnailDrawnAndKept` looks for `thumbs\v2\<id>.png`.
- [ ] **Step 2: Run** `node vfx_bench/check_composer_server.js`. Expected:
  - the four new steps FAIL: `pltServed` (404, `"flat":"plt"`), `bakeForPlay` (404 not found), `bakedServedFromSaved`
    (404 missing), `saveBakesIntoSaved` (422 `floatdisk: helm_001: its texture helm_001 is dyed ... give --saved`);
  - with the owner's helmet on the copy's Large, every save of the Floating Disk is that same 422 (P2's refusal) until
    Step 3.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer_server.js`. Expected: every step ok in Step 0's run, and the four new ones; with
    nothing else red, `all composer server checks pass`.
  - `node vfx_bench/composer_check.mjs`. Expected: every step ok in Step 0's run (thumbnailDrawnAndKept on v2).
    `ddsDraws` stays red if Step 0 had it red with `helm_001` among its flat textures: P4 draws it.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
- [ ] **Step 5: Commit** `vfx_bench/composer_server.js vfx_bench/check_composer_server.js vfx_bench/composer_check.mjs`,
  with "feat: the VFX Bench's Composer, dyed parts on the server - a PLT-only texture served as itself for the page to
  bake, Play in game's textures baked into vfxedit\saved, every Save given --saved, the thumbnails drawn again once
  (nwn5e-ee #39)".

### Task P4: The page - dyed meshes renamed, the bake loader, the Colours card, Play's bakes (nwn5e-tools)

Needs P3.

**Files:**
- Modify:
  - `vfx_bench/public/composer.js`, `vfx_bench/public/composer_page.js`, `vfx_bench/public/app.js`;
  - `vfx_bench/public/index.html`, `vfx_bench/public/style.css`;
  - `vfx_bench/check_composer.js`, `vfx_bench/composer_check.mjs`;
  - `vfx_bench/README.md`;
  - nwn5e-ee-anim `docs/TOOLS-PLAN.md`.
- Create: the owner's test rows, a CSV in the job's tmp folder (the parent session adds them to the board).

**Interfaces:**
- **composer.js:**
  - **`placeOne`** renames dyed textures as the generators do. With `answer = modelOf(models, key)`, `F.bakeBitmaps(p.meshes,
    F.readColours(raw.colours, "colours"), (b) => !!(answer.textures && answer.textures[b] && answer.textures[b].draw ===
    "plt"))`. It returns those meshes and `bakes`.
  - **`bakesOf(list)`:** a `placed()` list's bakes, each name once.
  - **`clean(raw)`:** `colours` through `F.cleanColours`; the key is dropped when it gives undefined.
- **composer_page.js:**
  - **The bake loader `bakedImage(name)`** (returned by `mount`):
    - a name met in a scene's or a thumbnail's bakes resolves to `F.bakePlt(F.readPlt(<the PLT's bytes>), palettes,
      F.readColours(colours))`;
    - the PLT's bytes come from `fetch("/api/composer/texture/<plt>")` (`X-Texture: plt`), kept per PLT;
    - the palettes come through `image(<palette>)`, the page's mesh loader;
    - each name is baked once;
    - an unknown name is rejected, so the chain goes on.
  - **`build()`** registers each placed part's bakes, the ghosts' too. The stage note's "Drawn flat" is only for missing
    textures.
  - **The part card's Colours** (`#part-colours`, a fieldset):
    - shown when the selected part's answer has a texture with `draw: "plt"`;
    - six number fields `#part-metal1`, `#part-metal2`, `#part-leather1`, `#part-leather2`, `#part-cloth1`,
      `#part-cloth2` (min 0, max 175, step 1), labelled by `F.PLT_LABELS`, showing the part's colours with the defaults
      filled;
    - beside each, `#part-<field>-swatch`: its palette's colour at that row, shade 160;
    - `#part-colours-note`: "<plt names> is dyed (a PLT): the game paints it from these colours, as an item's. Colours
      left at the defaults aren't saved."
    - A change sets `raw.colours = F.cleanColours(...)` (no key at the defaults): one undo step.
    - A number that isn't a whole 0-175 puts the field back, with the status "<label>: a palette number, 0 to 175."
  - **Thumbnails:** `drawThumb` renames the answer's meshes with `F.bakeBitmaps(..., F.readColours(), plt test)`,
    registers their bakes, and waits for them as for any texture.
  - **`toPlay()`** also gives `bakes: [{ plt, colours }]`: the variant's, each once.
  - **Save's status** for any kind adds "Dyed textures baked into vfxedit\saved: <name> (<plt>), ..." when
    `result.baked` has any.
  - **The hook:**
    - `parts[i].colours`: the part's own, or null;
    - `drawn.baked`: `[{ name, plt }]`;
    - `bakedImage(name)`: the loader.
- **app.js:**
  - the mesh loader is `chainLoaders((n) => (comp ? comp.bakedImage(n) : Promise.reject(new Error("no Composer"))),
    gameImage, image)`;
  - `gameImage` rejects `X-Texture: plt` (`<name>: dyed (a PLT) - the Composer bakes it`);
  - in the Composer, Play in game posts `/api/composer/bake { textures: play.bakes }` first when there are any. An error
    gives "Not sent: <error>".
- **index.html / style.css:** the fieldset in `#part-card` after the tilt row; `.swatch` (14 x 14 px, a 1 px border).

- [ ] **Step 0: Record today's run.** `node vfx_bench/composer_check.mjs > /tmp/p4_browser_before.txt`.
- [ ] **Step 1: Write the checks.**
  - **`check_composer.js`**, with a model answer whose cube's bitmap is `helm_x` and `textures: { helm_x: { draw: "plt"
    } }`:
    - `placed: a dyed texture named for its part's colours`:
      - at the defaults the bitmap is `F.bakedName("helm_x", F.PLT_DEFAULTS)`, and `bakes` is `[{ name, plt:
        "helm_x", colours: {} }]`;
      - with `colours: { metal1: 5 }`, `F.bakedName("helm_x", { metal1: 5 })`;
      - `draw: "texture"` keeps `helm_x`;
      - the summary is the same either way.
    - `clean: default colours dropped, the rest in the card's order`: `{ cloth2: 40, metal1: 0 }` gives `{ cloth2: 40
      }`; all at the defaults gives no `colours` key.
    - `bakesOf: each bake once`: two parts with the same PLT and colours give one.
  - **`composer_check.mjs`**: these steps after `saveIsntUndone`, then Ctrl+Z until testpile's variant 1 has its one part
    again:
    - `pltDrawsBaked`: testpile, variant 1; Items, "helmet 001" (`helm_001`) clicked gives 2 parts, the new one
      selected. Once settled:
      - `drawn.textured` is true and `drawn.flat` empty;
      - `drawn.baked` has `{ name: "n5p_2cd3d725dedb", plt: "helm_001" }`;
      - `parts[1].colours` is null;
      - `#part-colours` is shown, its six fields reading 0, 3, 3, 2, 22, 23;
      - `#part-colours-note` names helm_001.
    - `pltColoursRebake`: the canvas pixel at `screenOf(1)` read, then Metal 1 set to 8 (gold).
      - `parts[1].colours` is `{ metal1: 8 }`, and `drawn.baked` has `F.bakedName("helm_001", { metal1: 8 })` (the
        copy's floatdisk.js, in Node).
      - The pixel differs from before by more than 12 in some channel.
      - Ctrl+Z gives null and `n5p_2cd3d725dedb`; Ctrl+Y gives `{ metal1: 8 }`.
      - Metal 1 176 leaves `{ metal1: 8 }`, with the status `Metal 1: a palette number, 0 to 175.`
      - `toSave()`'s part has `"colours": { "metal1": 8 }`.
    - `pltBakeIsTheGenerators`: in the page, `await vfxBench.composer().bakedImage("n5p_2cd3d725dedb")` gives width,
      height, the sum of its data and its first 16 values. They equal the copy's reader.js `bake("helm_001",
      F.readColours()).image`'s, in Node.
    - `playSendsBakes`: Play in game on that variant.
      - `<tempBuild>\vfxedit\saved\<the metal1-8 name>.tga` is 49170 bytes, its time not after `request.vfx`'s.
      - The request's draft has a plane with that bitmap, and none with `helm_001`.
    - `pltThumbBaked`: Items, "helmet 011" (`helm_011`).
      - Its thumbnail becomes ready, and some pixel of it is brown (R - B > 40: the leather cap at Leather 1 3).
      - `<tempBuild>\vfxedit\composer\thumbs\v2\helm_011.png` is there.
  - **Changed:** `playSendsTheVariant` also checks that the draft's planes' bitmaps, in order, are the copy's
    `c_n5fd_large.mdl` trimeshes' (`trimeshes` reads `bitmap` too): a dyed part plays under the name the built model
    gives it.
- [ ] **Step 2: Run.**
  - `node vfx_bench/check_composer.js`. Expected: the three new steps FAIL (`C.bakesOf is not a function`, the bitmap
    still `helm_x`), then `3 failed`.
  - `node vfx_bench/composer_check.mjs`. Expected: the five new steps FAIL (no `drawn.baked`, no `#part-colours`).
    `playSendsTheVariant` FAILs if the copy's Large holds a dyed part (the draft says `helm_001`, the model
    `n5p_2cd3d725dedb`).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer.js`. Expected: `all composer checks pass`.
  - `node vfx_bench/composer_check.mjs`. Expected:
    - every step ok in Step 0's run, and the five new ones;
    - `ddsDraws` too, if Step 0 had it red with `helm_001` flat;
    - `playSendsTheVariant`'s bitmaps match.
  - `node vfx_bench/check_composer_server.js`. Expected: as after P3.
  - `node vfx_bench/browser_check.mjs`. Expected: `all passed`.
  - `node common/checks/run.js`. Expected: `all common checks pass`.
- [ ] **Step 5: Write the docs and the rows.**
  - **README.md** (the Composer): "Dyed parts". A helmet's texture is a PLT, baked from your game. Its Colours card.
    Where the baked textures live (`vfxedit\saved\`, never a repo), and that the game reads them through the dev layer.
    The thumbnails folder `thumbs\v2\`.
  - **TOOLS-PLAN.md:** under "The Composer's dyed parts", a *Built* list: the commits; the defaults; the limits (matte
    metal, metal 2's palette, dev layer only).
  - **The rows (CSV):** CO-7 to CO-10 (#39, stream animation). Each stands on its own:
    - **CO-7 (the bench):**
      - Setup: close the VFX Bench window and double-click `vfx_bench.bat`; press Composer (the Floating Disk opens);
        pick Large.
      - Do: look at the helmet on the pile. Then Items, search "helmet 011", click it.
      - Expect: the helmet is textured (steel, a darker trim), not white. Helmet 011 comes as a brown leather cap. Its
        card shows Colours: Metal 1 0, Metal 2 3, Leather 1 3, Leather 2 2, Cloth 1 22, Cloth 2 23, with swatches. The
        helmets' thumbnails show their colours.
    - **CO-8 (the bench):**
      - Setup: as CO-7's, Large picked.
      - Do: click the helmet; set Metal 1 to 8; Ctrl+Z, Ctrl+Y; Save and build.
      - Expect: the helmet turns gold as you type, and back and forth with Undo and Redo. The status names the baked
        texture (`n5p_...`) in vfxedit\saved. composition.json's helmet line ends `"colours": { "metal1": 8 }`.
    - **CO-9 (in game, Play in game):**
      - Setup: the game running a development build of nwn5e with a character in play; the bench as CO-7's, Large
        picked.
      - Do: Play in game.
      - Expect: beside you, the pile's helmet is textured in its colours (its texture written into vfxedit\saved just
        before).
    - **CO-10 (in game, installed):**
      - Setup: after the game session's install carrying anim's rebuilt `c_n5fd_large`; the game started fresh; the
        Floating Disk loaded to Large (the parent copies the disk's own setup from its #39 rows).
      - Do: look at the pile.
      - Expect: its helmet is textured as on the bench, not white.
- [ ] **Step 6: Commit.**
  - In nwn5e-tools: `vfx_bench/public/composer.js vfx_bench/public/composer_page.js vfx_bench/public/app.js
    vfx_bench/public/index.html vfx_bench/public/style.css vfx_bench/check_composer.js vfx_bench/composer_check.mjs
    vfx_bench/README.md`, with "feat: the VFX Bench's Composer draws dyed parts - a helmet's PLT baked in the page as the
    generators bake it, a Colours card per part (palette numbers, the defaults unsaved), Play in game sends its baked
    textures (nwn5e-ee #39)".
  - In the anim worktree, after `git branch --show-current` (anim): `docs/TOOLS-PLAN.md`, with "docs: the Composer's dyed
    parts built - TOOLS-PLAN (#39)".
- [ ] **Step 7: Report:**
  - the commits and the rows;
  - the bench needs a restart of `vfx_bench.bat`;
  - the "anim ready" for P2 (the disk's Large rebuilt) and the textures' home in `vfxedit\saved\` (Q3 still open).

---

## Addendum 2026-11-17: basic shapes

**Spec:** the design's "Addendum 2026-11-17: basic shapes". The owner: "is it possible to use basic 3d geometry for VFX?
... I wanted to have a translucent sphere so the player could see the affected area", then "go with the shapes, start
with the dome".

Five tasks, after P4. Each task's full text is here.

1. **S1:** the dome and its texture in floatdisk.js; a shape part (anim).
2. **S2:** the generators and build_derived (anim).
3. **S3:** the server and the page's pure parts (nwn5e-tools).
4. **S4:** the page, the stage's front faces, the docs and the test rows (nwn5e-tools, TOOLS-PLAN on anim).
5. **S5:** sphere, cylinder, cone, disc and ring (anim, nwn5e-tools).

### Before starting

- **Q4 is answered before S4 starts** (the coordinator asks the owner): whether the bench draws only front faces
  everywhere, or only in the Composer. S4 is written for the recommendation, everywhere. The other answer is one line:
  the effect mode sets `view.cullBack = false`.
- **Another job is working in both repos** (an emitter's blast radius and length).
  - Its C++ is committed on anim (9934e61).
  - Its bench side is uncommitted in nwn5e-tools, as of 2026-11-17: `vfx_bench/public/app.js`, `core.js`, `sim.js`,
    `view.js`, `vfx_bench/check.js`, `check_effect.js`, `browser_check.mjs` and `dev/*`.
  - S1-S3 and S5 touch none of them.
  - **S4 edits view.js and app.js** (and may adjust browser_check.mjs). It starts only once `git status --short --
    vfx_bench/public/view.js vfx_bench/public/app.js vfx_bench/browser_check.mjs` prints nothing. If it prints any of
    them, stop and report.
  - `git commit --only <file>` takes the whole file, so it would commit the other job's lines with this task's.
  - On anim, the same job's `src/data/vfx_model.*` and `tests/data/vfx_model_test.cpp` may be uncommitted too (they
    were when this plan was written). Never commit them: each commit here names only its own paths.
- **Known answers** (computed 2026-11-17 with floatdisk.js's cyrb53):
  - `shapeTextureName({ colour: [0.6, 0.8, 1] })` (the default) is `n5s_8c757fb28f99`;
  - `[1, 0.85, 0.4]` gives `n5s_a3caa0709dda`;
  - a 128 x 128 texture with alpha is a 65554-byte TGA (18 + 128 x 128 x 4);
  - the default dome is 296 vertices and 480 triangles a side: 592 and 960 with both sides.
- **Today's counts:** `floatdisk check: 39 checks pass` and `composer check: 15 checks pass`. If they've moved by the
  time a task starts, its expected numbers move with them.

### Global constraints (added)

The plan's Global Constraints and the earlier addenda's hold, plus these:

- **No game-side change** (the design's call 7).
  - No commit here names a file under `src` or `tests`.
  - `git diff --stat -- src` is the same before and after each anim task (the other job's uncommitted lines may be
    in it: not this task's). Each anim task records it in its first step.
  - test.bat isn't needed.
- **floatdisk.js stays loadable in a page:** no `require` at its top, and Buffer only in what tgaBytes and diskTexture
  return.
- **mdlmesh rebuilds itself** whenever vfx_model.* is newer than it (the other job is changing them): `ensureReader`
  runs build_mdlmesh.bat into the anim worktree's `build\floatdisk\`. That's expected.
- **Shape textures are original** (generated from numbers).
  - The generators write them beside what names them: the checkout's data\files (committable), a spell mesh's
    `--saved`.
  - Checks write only into their temporary copies. No check writes the real worktree or the real bench build, as
    before.
- **Before each nwn5e-tools commit:** `git status --short`, and each committed file's `git diff` checked to be only this
  task's.

### Review Focus (added)

17. **What you see is what's built, for shapes:**
    - S1 `shapes in a page`;
    - S3 `shapeSummaryMatchesGenerator`;
    - S4 `domeSaves` (the saved draft's plane is the stage's mesh).
18. **Both sides, the inside first, each side its own vertices:** S1's dome checks; S4 `domeInsideAndOut`.
19. **Defaults never written:** S1 `a shape: read with its defaults, written without them`; S3 `clean: a shape's
    numbers rounded ...`; S4 `domeCardEdits` (back to `{ type, radius }` after the undos).
20. **No game-side change:** no commit names `src` or `tests`, and `git diff --stat -- src` is as it was before each
    anim task; S2's draft carries the material WriteVfxModel writes.
21. **The other job's lines never committed:** S4's Step 0 and each commit's diff check.

---

### Task S1: The dome - floatdisk.js's shape, its mesh and texture; a shape part (nwn5e-ee, anim)

Working folder: the anim worktree (`/c/Users/Bruno/Desktop/Projects/nwn5e-ee-anim`). Needs nothing else.

**Files:**
- Modify:
  - `tools/data/floatdisk/floatdisk.js`, `tools/data/floatdisk/check.js`, `tools/data/floatdisk/README.md` (a part's
    `shape`);
  - `tools/data/composer/project.js` (projectText's part keys), `tools/data/composer/check.js`,
    `tools/data/composer/README.md` (shapes).

**Interfaces** (floatdisk.js, all pure; they load in a page as the rest does):
- **Constants:**
  - `SHAPE_TYPES = ["dome"]` (S5 adds the rest);
  - `SHAPE_LABELS = { dome: "Dome (half sphere)" }`;
  - `SHAPE_KEYS = ["type", "radius", "height", "segments", "sides", "colour", "alpha", "glow", "texture"]`: the order
    they're written in;
  - `SHAPE_FIELDS = { dome: ["radius", "height", "segments", "sides"] }`: a type's own fields, the card's order;
  - `LOOK_FIELDS = ["colour", "alpha", "glow", "texture"]`: every type's;
  - `SHAPE_INFO`, a field's card label and bounds (what readShape enforces, and what the page shows):
    - `radius: { label: "Radius (m)", min: 0.05, max: 100, step: 0.05 }`;
    - `height: { label: "Height (m)", min: 0.05, max: 100, step: 0.05 }`;
    - `segments: { label: "Smoothness", min: 8, max: 96, step: 4, whole: true, title: "Segments round it; the rings up
      it are a quarter of that" }`;
    - `sides: { label: "Sides", choices: [["both", "Both: seen from inside too"], ["outside", "Outside only"]] }`;
    - `colour: { label: "Colour", colour: true }`;
    - `alpha: { label: "Opacity", min: 0.02, max: 1, step: 0.05, title: "Its alpha: 1 is solid; the game's globes go to
      about 0.3" }`;
    - `glow: { label: "Glow", min: 0, max: 1, step: 0.05, title: "How much it lights itself (selfillum): at 1 it looks
      the same by day and night" }`;
    - `texture: { label: "Texture", choices: [["soft", "Soft: brighter at the ground"], ["none", "None: its colour
      only"]] }`;
  - `SHAPE_TEXTURE = /^n5s_[0-9a-f]{12}$/`.
- **`shapeDefaults(type, radius)`:** `{ type, radius, height: radius, segments: 32, sides: "both", colour: [0.6, 0.8, 1],
  alpha: 0.35, glow: 1, texture: "soft" }`.
- **`readShape(s, where)`** gives the full shape: `shapeDefaults(s.type, s.radius)` with `s`'s keys over it (`colour`
  copied). Each error names its field:
  - not a plain object: `` `${where}: a shape ({ "type": "dome", "radius": <metres> })` ``;
  - `type`: `` `${where}.type: dome` `` (the types, with "or" before the last);
  - a key that isn't `type`, its type's fields or LOOK_FIELDS: `` `${where}.${k}: not a dome's - type, radius, height,
    segments, sides, colour, alpha, glow or texture` ``;
  - `radius` (required) and `height`: `` `${where}.radius: a number from 0.05 to 100 (metres)` ``;
  - `segments`: `` `${where}.segments: a whole number from 8 to 96` ``;
  - `sides`: `` `${where}.sides: both or outside` ``;
  - `colour`: `` `${where}.colour: [r, g, b], each 0 to 1` ``;
  - `alpha`: `` `${where}.alpha: a number from 0.02 to 1` ``;
  - `glow`: `` `${where}.glow: a number from 0 to 1` ``;
  - `texture`: `` `${where}.texture: soft or none` ``.
- **`cleanShape(s)`** (`s` a valid shape, whole or not) gives `type` and `radius`, then each other key whose value
  differs from `shapeDefaults(s.type, s.radius)`'s (a colour by its three numbers), in SHAPE_KEYS' order.
- **`shapeModel(shape)`** (readShape's) gives `{ meshes: [mesh], lo, hi, textures }`, in the model's space, its base
  at z 0, centred on x 0 y 0 (as modelSpaceMeshes gives a model).
  - **The dome:**
    - `n = segments`, `m = max(2, round(n / 4))` rings.
    - Ring `j` (0 to m - 1) is at latitude `phi = (PI / 2) j / m`. Its vertex `i` (0 to n: the seam's twice) is at
      angle `theta = 2 PI i / n`: `(r cos(phi) cos(theta), r cos(phi) sin(theta), h sin(phi))`, UV `(i / n, j / m)`.
    - Then the top: n vertices at `(0, 0, h)`, UV `((i + 0.5) / n, 1)`.
    - That's `(n + 1) m + n` vertices a side.
    - **The outside faces** (counter-clockwise from outside):
      - for each band `j` (0 to m - 2) and segment `i`, with `a = v(j, i)`, `b = v(j, i + 1)`, `c = v(j + 1, i + 1)`,
        `d = v(j + 1, i)`: `(a, b, c)` and `(a, c, d)`;
      - then the top fan: `(v(m - 1, i), v(m - 1, i + 1), top(i))`;
      - that's `n (2m - 1)` triangles a side.
    - **Both sides:** a copy of the vertices with the inside faces (each outside face's first, third and second). The
      inside's vertices and faces come first, then the outside's.
    - Vertices and UVs rounded as diskMesh's (`round`).
    - `lo = [-r, -r, 0]`, `hi = [r, r, h]`.
  - **The mesh:** `{ name: <its type>, bitmap, verts, tverts, faces, diffuse, ambient, selfillum, alpha, transparent }`.
    - Textured (`texture: "soft"`): `bitmap` is `shapeTextureName(shape)`; diffuse and ambient `[1, 1, 1]`; selfillum
      `[glow, glow, glow]`.
    - `texture: "none"`: `bitmap` is `""`; diffuse and ambient the colour; selfillum the colour times the glow.
    - `alpha` is the shape's. `transparent` is `texture === "soft" || alpha < 1`.
  - `textures` is `[{ name, colour }]` for a textured shape, else `[]`.
- **`shapeTexture({ colour })`** gives `{ width: 128, height: 128, data }`: a Uint8ClampedArray RGBA, rows top-down (as
  bakePlt gives a picture).
  - At the picture's row `y` (0 the top) and column `x`: `u = (x + 0.5) / 128`, `v = 1 - (y + 0.5) / 128`.
  - `ground = 1 - smooth(0, 0.2, v)` (diskTexture's `smooth`): 1 at the ground ring, 0 from a fifth up.
  - `shimmer = 0.05 sin(2 PI 3u + 7v) sin(2 PI 5u - 3v)`: whole turns in u, so it wraps round seamlessly.
  - Each colour channel: `clamp01(colour[c] (0.75 + 0.25 (1 - v)) + 0.35 ground + shimmer)`.
  - Alpha: `clamp01(0.3 + 0.5 (1 - v)^2 + 0.2 ground + 0.5 shimmer)`.
  - Each value rounded to 0-255.
- **`shapeTextureName({ colour })`:** `"n5s_" + cyrb53(key).toString(16).padStart(14, "0").slice(-12)`, where `key =
  "soft:" + colour.map((c) => String(Math.round(c * 1000) / 1000)).join(",")`.
- **`tgaBytes(image, { alpha = false } = {})`:**
  - with `alpha`, a 32-bit TGA: header `[0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w & 255, w >> 8, h & 255, h >> 8, 32,
    8]`, then BGRA, the picture's bottom row first;
  - without it, P1's 24-bit bytes, as now.
- **`readPart`** takes a third source, `shape`:
  - `source: { kind: "shape", shape: readShape(p.shape, `${where}.shape`) }`;
  - exactly one of `model`, `file` and `shape`, else `` `${where}: either "model" (a game model), "file" (your own
    .mdl) or "shape" (a dome and the like) - just one` `` (it still starts `either "model"`, so the existing checks
    hold);
  - `scale` given on a shape: `` `${where}.scale: a shape has no scale - its radius and height size it` ``; its scale
    is 1.
- **`partKey`:** `"shape:" + inline(cleanShape(source.shape))`. **`partLabel`:** `` `${type} ${num(radius)} m` ``
  (`dome 4.57 m`).
- **`compositionText`'s part keys:** `["note", "model", "file", "shape", "at", "turn", "scale", "tilt", "colours"]`.
- **The api** gains every name above (cyrb53 stays private).
- **project.js `projectText`'s `PART_KEYS`:** `["note", "model", "file", "shape", "at", "turn", "scale", "tilt"]`.

- [ ] **Step 0: Record** `git diff --stat -- src > /tmp/s1_src_before.txt` (the other job's lines, if any).
- [ ] **Step 1: Write the failing checks.** In `tools/data/floatdisk/check.js` (its `check(name, fn)` style), with
  `normals(mesh)`: each triangle's `(b - a) x (c - a)` dotted with its centroid (the dome's centre is the origin), its
  sign a triangle.
  - `a shape: read with its defaults, written without them`:
    - `F.readShape({ type: "dome", radius: 4.57 }, "s")` deep-equals `{ type: "dome", radius: 4.57, height: 4.57,
      segments: 32, sides: "both", colour: [0.6, 0.8, 1], alpha: 0.35, glow: 1, texture: "soft" }`;
    - `F.cleanShape` of it is `{ type: "dome", radius: 4.57 }`;
    - of that with `height: 3` and `colour: [1, 0.85, 0.4]`, `{ type: "dome", radius: 4.57, height: 3, colour: [1, 0.85,
      0.4] }`, keys in that order.
  - `a shape's errors name the field`:
    - `{ type: "cube", radius: 1 }` throws `/^Error: s\.type: dome/` (S5's longer list still matches);
    - radius `0`, `101` and none throw `/s\.radius: a number from 0\.05 to 100/`;
    - segments `7`, `97` and `12.5` throw `/s\.segments: a whole number from 8 to 96/`;
    - sides `"inside"` throws `/s\.sides: both or outside/`;
    - colour `[1, 1]` and `[2, 0, 0]` throw `/s\.colour: \[r, g, b\], each 0 to 1/`;
    - alpha `0` throws `/s\.alpha: a number from 0\.02 to 1/`; glow `1.5` throws `/s\.glow: a number from 0 to 1/`;
    - texture `"glyph"` throws `/s\.texture: soft or none/`;
    - `{ type: "dome", radius: 1, size: 2 }` throws `/s\.size: not a dome's/`;
    - `[]` throws `/^Error: s: a shape/`.
  - `a part may be a shape: its source, key and label; never a scale`:
    - `F.readPart({ shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0] }, "stages.small[0]", BASE)` has `source`
      deep-equal to `{ kind: "shape", shape: F.shapeDefaults("dome", 4.57) }`, scale 1 and turn 0;
    - `F.partKey` of it is `shape:{ "type": "dome", "radius": 4.57 }`, and `F.partLabel` is `dome 4.57 m`;
    - with `scale: 2` it throws `/stages\.small\[0\]\.scale: a shape has no scale/`;
    - with `model: "plc_b04"` beside its shape it throws `/either "model"/`.
  - `the dome: 32 segments, 8 rings, both sides - 592 vertices, 960 triangles, the inside first`:
    - `F.shapeModel(F.readShape({ type: "dome", radius: 2 }, "s"))` has one mesh, named `dome`, with 592 vertices and
      960 triangles;
    - `lo` is `[-2, -2, 0]` and `hi` `[2, 2, 2]`;
    - `normals` is negative for triangles 0-479 and positive for 480-959;
    - triangles 0-479 use only vertices 0-295, and the rest only 296-591;
    - every vertex has `(x^2 + y^2) / 4 + z^2 / 4` within 1e-4 of 1 and z at least 0.
  - `the dome's UVs: u round it from +x, v from the ground ring (0) to the top (1)`:
    - on that dome, the outside's vertices at `(2, 0, 0)` have UVs `(0, 0)` and `(1, 0)`;
    - every vertex with z 2 has v 1, and its u is `(i + 0.5) / 32` for some i;
    - the outside's vertex at `(1.41421, 0, 1.41421)` has v 0.5;
    - every u and v is in 0..1.
  - `a lower dome, outside only, 8 segments: 26 vertices, 24 triangles, all facing out`: `{ type: "dome", radius: 1,
    height: 0.4, segments: 8, sides: "outside" }` gives 26 vertices and 24 triangles, `hi[2]` 0.4, and every `normals`
    positive.
  - `the dome's material: its texture's colour lit by its glow (soft), or its own colour (none)`:
    - radius 1 at the defaults: bitmap `n5s_8c757fb28f99`, diffuse and ambient `[1, 1, 1]`, selfillum `[1, 1, 1]`, alpha
      0.35, transparent true, and `textures` `[{ name: "n5s_8c757fb28f99", colour: [0.6, 0.8, 1] }]`;
    - `{ texture: "none", colour: [1, 0.85, 0.4], glow: 0.5, alpha: 1 }`: bitmap `""`, diffuse and ambient `[1, 0.85,
      0.4]`, selfillum `[0.5, 0.425, 0.2]`, alpha 1, transparent false, `textures` `[]`.
  - `shapeTexture: 128 x 128, brighter and more opaque at the ground, seamless round it; its name n5s_ and a hash of its
    colour`:
    - `F.shapeTexture({ colour: [0.6, 0.8, 1] })` is 128 x 128 with 65536 values;
    - the bottom row's mean alpha is over the top row's by more than 100, and its mean R + G + B is over the top's;
    - on every row, columns 0 and 127 differ by less than 12 in each channel;
    - `F.shapeTextureName({ colour: [0.6, 0.8, 1] })` is `n5s_8c757fb28f99`, and `[1, 0.85, 0.4]` gives
      `n5s_a3caa0709dda`;
    - `F.SHAPE_TEXTURE` matches both and not `n5p_2cd3d725dedb`.
  - `tgaBytes with alpha: a 32-bit TGA, 8 alpha bits, rows bottom-up`:
    - the 2 x 2 picture with rows top-down `[10, 20, 30, 40, 50, 60, 70, 80]` and `[90, 100, 110, 120, 130, 140, 150,
      160]`;
    - `F.tgaBytes(img, { alpha: true })` is 34 bytes: the header `[0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 32,
      8]`, then `[110, 100, 90, 120, 150, 140, 130, 160, 30, 20, 10, 40, 70, 60, 50, 80]`;
    - `F.tgaBytes(img)` is 30 bytes with byte 16 being 24, as before.
  - `compositionText: a shape part on one line, its shape after note`: the small stage's one part `{ at: [0, 0, 0],
    shape: { type: "dome", radius: 4.57 }, note: "zone" }` gives the line `      { "note": "zone", "shape": { "type":
    "dome", "radius": 4.57 }, "at": [0, 0, 0] }`.
  - `shapes in a page: the same mesh, texture bytes and name`:
    - floatdisk.js loaded through `new Function("self", "require", "module", "Buffer", src)`, as P1's check does;
    - its `shapeModel(readShape({ type: "dome", radius: 2 }, "s"))` deep-equals Node's;
    - its `tgaBytes(shapeTexture({ colour: [0.6, 0.8, 1] }), { alpha: true })` is a Uint8Array, not a Buffer, with
      Node's bytes;
    - its `shapeTextureName` is Node's.

  In `tools/data/composer/check.js`, `readProject: a spell mesh's dome; projectText writes its shape after note, without
  its defaults`:
  - `{ kind: "spellmesh", name: "zotest", guide: { shape: "circle", diameter: 9.14, height: 0 }, variants: [{ name: "1",
    parts: [{ note: "zone", shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0] }] }] }` reads, and its
    `variants[0].parts[0].source.shape.height` is 4.57;
  - `P.projectText` of it has the line `      { "note": "zone", "shape": { "type": "dome", "radius": 4.57 }, "at": [0,
    0, 0] }`;
  - radius 0 throws `/variants\[0\]\.parts\[0\]\.shape\.radius: a number from 0\.05 to 100/`.
- [ ] **Step 2: Run them.**
  - `node tools/data/floatdisk/check.js`. Expected: eleven FAIL lines, among them `FAIL a shape: read with its
    defaults, written without them: F.readShape is not a function`, then `floatdisk check: FAILED (39 passed)`.
  - `node tools/data/composer/check.js`. Expected: `FAIL readProject: a spell mesh's dome ...`, then `composer check:
    FAILED (15 passed)`.
- [ ] **Step 3: Implement** the interfaces in floatdisk.js and project.js.
  - floatdisk README.md, a part's description: `shape`, "a shape the generator makes instead of a model: { "type":
    "dome", "radius": <m> } and, where they differ from the defaults, height (the radius: a half sphere), segments (32),
    sides (both), colour ([0.6, 0.8, 1]), alpha (0.35), glow (1), texture (soft); no scale".
  - composer README.md: "Shapes", the same, and that a shape's soft texture is named `n5s_` and a hash of its colour.
- [ ] **Step 4: Run.**
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 50 checks pass`.
  - `node tools/data/composer/check.js`. Expected: `composer check: 16 checks pass`.
  - `node tools/data/floatdisk/build.js --check`. Expected: the summary as before (the disk has no shape).
  - `node tools/data/floatdisk/check_build.js`. Expected: `build check: all pass`.
  - `node tools/data/composer/check_build.js`. Expected: `composer build check: all pass`.
  - `node tools/data/check_derived.js`. Expected: `derived check: all pass`.
  - `git diff --stat -- src`. Expected: what Step 0 recorded.
  - `git status --short`. Expected: this task's six files modified; anything else listed is the other job's, as
    before.
- [ ] **Step 5: Commit.**
  - `git branch --show-current` (anim);
  - `git commit --only tools/data/floatdisk/floatdisk.js tools/data/floatdisk/check.js tools/data/floatdisk/README.md
    tools/data/composer/project.js tools/data/composer/check.js tools/data/composer/README.md -F -`, with "feat: the
    Composer's shapes, the dome - floatdisk.js makes a dome's mesh (both sides, the inside first, UVs up from the ground
    ring) and its soft texture (n5s_, original); a part may be a shape, its defaults unwritten (#39)".

### Task S2: The generators - a shape's mesh and its texture, build_derived (nwn5e-ee, anim)

Working folder: the anim worktree. Needs S1 and the game.

**Files:**
- Modify:
  - `tools/data/floatdisk/reader.js`, `tools/data/floatdisk/files.js`;
  - `tools/data/floatdisk/build.js`, `tools/data/composer/build.js`, `tools/data/build_derived.js`;
  - `tools/data/composer/check_build.js`, `tools/data/check_derived.js`;
  - `tools/data/floatdisk/README.md`, `tools/data/composer/README.md`, `tools/data/README.md` (shape textures).

**Interfaces:**
- **reader.js `part(p)`:** a `shape` source gives `F.shapeModel(p.source.shape)`, with no mesh reader and no game.
- **files.js:**
  - **`shapeTextures(placed, made = new Map())`:** for each `x` of `placed` whose part is a shape, each of
    `F.shapeModel(x.part.source.shape).textures` that `made` lacks is set: `name -> { colour, bytes:
    F.tgaBytes(F.shapeTexture({ colour }), { alpha: true }), label: F.partLabel(x.part) }`. Returns `made`.
  - **`shapeWrites(made, dir)`:** `[{ file: <dir>\<name>.tga, bytes, kind: "shape" }]`.
  - **`shapeLines(made, where)`:** `` `  texture ${name}: a shape's soft texture, colour ${colour.join(" ")} - written
    into ${where}` ``.
  - **`shapeJson(made)`:** `[{ texture, colour }]`.
  - **`ownership`** knows the kind `shape`: ours when the file's name without `.tga` matches `F.SHAPE_TEXTURE`. Only the
    generator makes such names, and the name fixes the bytes. Never in made.txt.
- **composer build.js:**
  - In `built`, right after a variant's parts are placed, `W.shapeTextures(parts, shaped)` (one `shaped` for the
    project).
  - The writes gain `W.shapeWrites(shaped, placeable ? FILES : SAVED)`. Under `--check` without `--saved`, nothing is
    written, as now.
  - The text summary gains `W.shapeLines`, and `--json` gains `shapes: W.shapeJson(shaped)`.
- **floatdisk build.js:** the same, into FILES.
- **build_derived.js:** a file the generator wrote whose name matches `F.SHAPE_TEXTURE` is kind `shape` (after `text`
  and `baked`, before `texture`), owned by its name through `W.ownership`.

- [ ] **Step 0: Record** `git diff --stat -- src data > /tmp/s2_before.txt` (the other job's lines, if any).
- [ ] **Step 1: Write the checks.**
  - **composer `check_build.js`**, new, last:
    - `a spell mesh's dome: its draft and its soft texture into --saved`:
      - the project is `chkdome`: a spell mesh, guide circle 9.14 at 0, one variant `{ note: "zone", shape: { type:
        "dome", radius: 4.57 }, at: [0, 0, 0] }`;
      - built with `--json --saved SAVED`, `j.variants[0]` has meshes 1, triangles 960, tall 4.57;
      - `j.shapes` deep-equals `[{ texture: "n5s_8c757fb28f99", colour: [0.6, 0.8, 1] }]`;
      - `SAVED\n5m_chkdome.vfx` read by `core.readDraft` after its mark line has one plane: bitmap `n5s_8c757fb28f99`,
        alpha 0.35, selfillum `[1, 1, 1]`, 592 vertices and 960 faces;
      - `SAVED\n5s_8c757fb28f99.tga` is 65554 bytes, bytes 16 and 17 are 32 and 8, and it equals
        `F.tgaBytes(F.shapeTexture({ colour: [0.6, 0.8, 1] }), { alpha: true })`;
      - built again, every state is `unchanged`;
      - FILES and made.txt are as they were.
    - `a placeable's dome: its model is see-through (transparencyhint 1) and names its texture, written beside it`:
      - the project is `chkdomep`: a placeable, guide circle 2, row 14152, one variant `{ shape: { type: "dome",
        radius: 1, colour: [1, 0.85, 0.4] }, at: [0, 0, 0] }`;
      - `plc_n5chkdomep.mdl` contains `\r\n  bitmap n5s_a3caa0709dda\r\n`, `\r\n  alpha 0.35\r\n`, `\r\n
        transparencyhint 1\r\n` and `\r\n  selfillumcolor 1 1 1\r\n`;
      - FILES holds `n5s_a3caa0709dda.tga`, and made.txt is as it was;
      - built again, every state is `unchanged`.
    - `a dome without a texture: bitmap NULL, its colour as its material, no texture written`: `chkdomep` with `texture:
      "none"`.
      - The model has `\r\n  bitmap NULL\r\n` and `\r\n  diffuse 1 0.85 0.4\r\n`.
      - `j.shapes` is `[]`, and no `n5s_` file is in FILES or SAVED.
  - **`check_derived.js`**, new, before the real-worktree check: `a placeable project's dome: its model and its shape
    texture come out into --out, the same on a second build`.
    - Set up as its `a placeable project: ...` check does (a project and its rows in the temporary copy), with a dome
      part.
    - `--out` gains its model and `n5s_a3caa0709dda.tga`, its bytes the generator's.
    - A second build says both `unchanged`.
- [ ] **Step 2: Run them.**
  - `node tools/data/composer/check_build.js`. Expected: the three new checks FAIL (the build exits 1: reader.js reads
    a shape as a file), then `composer build check: 3 failed`.
  - `node tools/data/check_derived.js`. Expected: the new check FAILs, then `derived check: 1 failed`.
- [ ] **Step 3: Implement.** The READMEs:
  - floatdisk and composer: "A shape's texture", where it goes and that its name says whose it is;
  - tools/data/README.md: build_derived carries `n5s_` textures as it carries bakes.
- [ ] **Step 4: Run.**
  - `node tools/data/composer/check_build.js`. Expected: `composer build check: all pass`.
  - `node tools/data/check_derived.js`. Expected: `derived check: all pass`.
  - `node tools/data/floatdisk/check_build.js`. Expected: `build check: all pass`.
  - `node tools/data/floatdisk/check.js` (50) and `node tools/data/composer/check.js` (16): all pass.
  - `git diff --stat -- src data`. Expected: what Step 0 recorded (the checks build in copies).
  - `git status --short`. Expected: this task's ten files; anything else listed is the other job's, as before.
- [ ] **Step 5: Commit.**
  - `git branch --show-current` (anim);
  - `git commit --only` the ten files `-F -`, with "feat: the Composer's shapes, the generators - a shape part's
    mesh from floatdisk.js (no mesh reader), its soft texture written beside what names it (data\files, or a spell
    mesh's --saved) and owned by its name; build_derived carries it (#39)".
  - The next "anim ready" says: tools only, nothing for the game's DLL; shape textures are original and may be
    committed.

### Task S3: The server and the page's pure parts (nwn5e-tools)

Working folder: `/c/Users/Bruno/Desktop/Projects/nwn5e-tools`. Needs S1 and S2.

**Files:**
- Modify: `vfx_bench/public/composer.js`, `vfx_bench/composer_server.js`, `vfx_bench/check_composer.js`,
  `vfx_bench/check_composer_server.js`

**Interfaces:**
- **composer.js:**
  - **`sourceOf(raw)`:** `raw.shape` gives `{ kind: "shape", shape: F.readShape(raw.shape, "shape") }`. So `keyOf` is
    F.partKey's `shape:...`, and `labelOf` F.partLabel's.
  - **`shapeAnswer(raw)`:** `F.shapeModel` of its shape as a part's answer: `{ key, label, meshes, lo, hi, textures: {
    <name>: { draw: "shape", colour } } }`. It's kept by key (the last 64).
  - **`placeOne`** places a shape from `shapeAnswer(raw)`, never from `models`. It also returns `shapes: [{ name,
    colour }]`, the textures the page draws from F.shapeTexture.
  - **`shapesOf(list)`:** a `placed()` list's shapes, each name once.
  - **`clean(raw)`:** a shape's radius and height to the millimetre, its colour to 0.001, its alpha and glow to 0.01,
    then `F.cleanShape`; any `scale` dropped.
  - **`shapeRadius(guide)`:** a circle's radius, half a rectangle's shorter side, 1 with no guide; to the millimetre.
  - **`shapeEntries()`:** `F.SHAPE_TYPES` as parts list entries: `{ id: "shape_" + type, label: F.SHAPE_LABELS[type],
    kind: "shape", shape: type, models: [], search: type + " " + label }`.
  - **`partFrom(entry, at, box, project)`** for a `shape` entry: `{ note: entry.label, shape:
    F.cleanShape(F.shapeDefaults(entry.shape, shapeRadius(project.guide))), at: [x, y, 0], turn: 0 }`. `box` isn't
    used.
- **composer_server.js:**
  - **`besideDirs(name)`:** a name matching `F.SHAPE_TEXTURE` is also looked for in the checkout's data\files and the
    build's `vfxedit\saved\`, as a bake's.
  - **`POST /api/composer/bake`** takes `{ textures?, shapes? }`, 1 to 64 entries between them.
    - Each `shapes[i]` is `{ colour }`, checked by `F.readShape({ type: "dome", radius: 1, colour }, `shapes[${i}]`)`
      (a bad colour is a 400 naming `shapes[i].colour`).
    - Each is written into `<build>\vfxedit\<F.shapeTextureName>.tga` as `F.tgaBytes(F.shapeTexture({ colour }), {
      alpha: true })`, whole when missing or different.
    - The answer's `textures` gain `{ shape: true, name, state }`. The server logs `composer: made <names> for Play in
      game`.
    - The error for an empty or wrong body: `` `{ textures: [{ plt, colours }], shapes: [{ colour }] }, 1 to 64 of
      them` ``.

- [ ] **Step 0: Record today's runs.** `node vfx_bench/check_composer.js > /tmp/s3_pure_before.txt` and `node
  vfx_bench/check_composer_server.js > /tmp/s3_server_before.txt`.
- [ ] **Step 1: Add steps.**
  - **`check_composer.js`** (the copy's floatdisk.js and project.js, as its other checks):
    - `a shape part: its key and label, placed from floatdisk.js's own mesh - no server answer`:
      - a spell mesh project, guide circle 1 at 0.2; raw `{ shape: { type: "dome", radius: 0.5 }, at: [0.1, 0, 0] }`;
      - `keyOf` is `shape:{ "type": "dome", "radius": 0.5 }`, and `labelOf` is `dome 0.5 m`;
      - `placeOne(project, raw, new Map())` isn't null: its mesh `j01_dome`, 960 triangles, `lo` `[-0.4, -0.5, 0.2]`,
        `hi[2]` 0.7;
      - its `shapes` is `[{ name: "n5s_8c757fb28f99", colour: [0.6, 0.8, 1] }]`;
      - `placed()`'s `missing` is empty.
    - `clean: a shape's numbers rounded, its defaults and any scale dropped`: `{ shape: { type: "dome", radius:
      4.5700004, height: 4.57, alpha: 0.3049, colour: [0.6, 0.8, 1] }, at: [0, 0, 0], scale: 1 }` gives `{ shape: {
      type: "dome", radius: 4.57, alpha: 0.3 }, at: [0, 0, 0] }`.
    - `partFrom: a Shapes entry sized to the guide, no scale`:
      - circle 9.14: `shape` `{ type: "dome", radius: 4.57 }`, `at` `[0, 0, 0]`, no `scale`, `note` `Dome (half
        sphere)`;
      - a rectangle 2 x 3: radius 1;
      - no guide: radius 1.
    - `shapeEntries and shapesOf: the types listed, each texture once`:
      - `shapeEntries()` is one entry, `shape_dome`;
      - two domes of the default colour, placed, give one shape texture.
    - `the summary counts a shape as the generators do`: a variant of the dome and nothing else: parts 1, meshes 1,
      triangles 960, tall 0.5, its list's label `dome 0.5 m`.
  - **`check_composer_server.js`**, after the save steps:
    - `shapeSummaryMatchesGenerator`:
      - the copy gains `tools/data/composer/zotest.json`: a spell mesh, guide circle 9.14, one variant with `{ note:
        "zone", shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0] }` and `{ model: "plc_b06", at: [1, 0, 0], scale:
        0.5 }`;
      - the page's `summary` (the server's meshes for plc_b06, none asked for the dome) equals `composer/build.js
        --check --json`'s, as summaryMatchesGenerator compares.
    - `shapeTextureServedFromSaved`:
      - `zotest` saved gives 200, `result.shapes` has `n5s_8c757fb28f99`, and `<tempBuild>\vfxedit\saved\` holds it;
      - `GET /api/composer/texture/n5s_8c757fb28f99` is 200 with `X-Texture: tga` and its bytes.
    - `bakeShapesForPlay`:
      - POST `{ shapes: [{ colour: [0.6, 0.8, 1] }] }` gives 200, `textures` `[{ shape: true, name:
        "n5s_8c757fb28f99", state: "new" }]`;
      - `<tempBuild>\vfxedit\n5s_8c757fb28f99.tga` is 65554 bytes, equal to the copy's F's bytes;
      - posted again: `unchanged`, its time kept;
      - `{ shapes: [{ colour: [2, 0, 0] }] }` gives 400 `/shapes\[0\]\.colour/`;
      - no Origin: 403;
      - `{ textures: [{ plt: "helm_001", colours: {} }], shapes: [{ colour: [0.6, 0.8, 1] }] }` gives both.
- [ ] **Step 2: Run.**
  - `node vfx_bench/check_composer.js`. Expected: the five new steps FAIL (the shape's key `model:`, placeOne null,
    `C.shapeEntries is not a function`), then `5 failed`.
  - `node vfx_bench/check_composer_server.js`. Expected: `shapeSummaryMatchesGenerator` FAIL (the page places no dome),
    `shapeTextureServedFromSaved` FAIL (404 missing), `bakeShapesForPlay` FAIL (400).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/check_composer.js`. Expected: every step ok in Step 0's run, and the five new ones.
  - `node vfx_bench/check_composer_server.js`. Expected: every step ok in Step 0's run, and the three new ones.
  - `node vfx_bench/composer_check.mjs`. Expected: as before this task (the page doesn't use these yet).
- [ ] **Step 5: Commit** `vfx_bench/public/composer.js vfx_bench/composer_server.js vfx_bench/check_composer.js
  vfx_bench/check_composer_server.js`, after the diff check, with "feat: the VFX Bench's Composer, shapes on the server
  and in the page's pure parts - a shape placed from floatdisk.js with no part request, cleaned, added sized to the
  guide; its texture served from where it's built and written for Play in game (nwn5e-ee #39)".

### Task S4: The page - the Shapes tab, a shape's card, the stage's front faces; the docs and the rows (nwn5e-tools)

Needs S3, Q4's answer, and the other job's bench work committed (Before starting).

**Files:**
- Modify:
  - `vfx_bench/public/composer_page.js`, `vfx_bench/public/index.html`, `vfx_bench/public/style.css`;
  - `vfx_bench/public/view.js` (front faces), `vfx_bench/public/app.js` (Play's shapes);
  - `vfx_bench/composer_check.mjs`, `vfx_bench/README.md`;
  - `vfx_bench/browser_check.mjs`, only if a step that was ok in Step 0 needs its expectation changed by the front
    faces (said in the report);
  - nwn5e-ee-anim `docs/TOOLS-PLAN.md`.
- Create: the owner's test rows, a CSV in the job's tmp folder (the parent session adds them to the board).

**Interfaces:**
- **view.js:**
  - `View.cullBack` (default true).
  - While it's true, `drawMeshes` draws with `gl.enable(gl.CULL_FACE)` and `gl.cullFace(gl.BACK)` (front faces
    counter-clockwise, as the game's are from outside; the stage's lookAt keeps the world's turn), and disables it
    after.
  - Every mesh drawMeshes draws is culled: the Composer's parts and ghosts, the thumbnails, the effect mode's chunks and
    planes. Particles, lines, the ground and the figure aren't.
  - With Q4's other answer, app.js sets `view.cullBack = false` outside the Composer.
- **composer_page.js:**
  - **The parts list:** `PART_TABS` gains `shapes` ("Shapes"), its entries `C.shapeEntries()`. They're listed whether or
    not the server's catalog came.
    - A shape entry's thumbnail is drawn from `C.shapeAnswer` at the defaults (radius 1), and never posted (cheap, and
      it follows floatdisk.js).
    - Its line says "a shape: made by the generator".
  - **Adding a shape** (a click, or a drag onto the stage): no part request. `C.partFrom(e, at, null, project)`, rested
    when Rests is on, selected; one undo step. The status: "Added a dome (radius 4.57 m) to variant 1 at x 0 y 0."
  - **`fetchPart` and `meshesOf`** pass over a shape (composer.js places it itself).
  - **The part card**, for a shape:
    - `#part-what`: "<label> - <x> x <y> x <z> m, <n> triangles";
    - the Scale field's label `#part-scale-l` is hidden;
    - `#part-shape` (a fieldset, its legend the type's label) holds a control per field of `F.SHAPE_FIELDS[type]`, then
      `F.LOOK_FIELDS`, each from `F.SHAPE_INFO`:
      - a number: `#part-shape-<field>`, its min, max and step;
      - choices: a select;
      - colour: `<input type="color">` (hex to `[r, g, b]` to 0.001, and back);
      - the input's title from the field's `title`.
    - A change sets `raw.shape = F.cleanShape(Object.assign(F.readShape(raw.shape, "shape"), { [field]: value }))`
      (rounded as `clean`): one undo step, and the stage redraws.
    - A value out of bounds puts the field back, with the status `` `${label}: ${min} to ${max}.` ``.
  - **Handles:** no scale square for a shape (`handlesOf` leaves it out; `handle("scale")` is null), so grabbing never
    scales one.
  - **The mesh loader** `bakedImage(name)` (P4's) also resolves a registered shape texture: `F.shapeTexture({ colour
    })`, each made once. `build()` registers the placed shapes and the ghosts' (`C.shapesOf`); thumbnails register
    theirs.
  - **`toPlay()`** also gives `shapes: [{ colour }]`, the variant's, each once.
  - **Save's status**, for any kind, adds "Shape textures: <names> (beside the models | in vfxedit\saved)." when
    `result.shapes` has any.
  - **The view buttons** (3/4, side, top, front) fit the variant: when its box is bigger than the default view, the
    distance grows to `C.frameBox`'s for it, at most 40 m.
  - **The hook:**
    - `parts[i].shape`: the part's own, or null;
    - `drawn.shapes`: `[{ name, colour }]`;
    - `setCamera({ yaw, pitch, distance, target })`, for the checks.
- **app.js:** in the Composer, Play in game posts `/api/composer/bake { textures: play.bakes, shapes: play.shapes }`
  first when either has any (P4's line, widened).
- **index.html / style.css:**
  - the Shapes tab button in `#parts-tabs`, after Yours;
  - `#part-shape` in `#part-card` after the tilt row, before Colours;
  - `id="part-scale-l"` on the Scale label;
  - `.shape-grid` as `.colours-grid`; the colour input 2.5 em wide.

- [ ] **Step 0: Check and record.**
  - `git status --short -- vfx_bench/public/view.js vfx_bench/public/app.js vfx_bench/browser_check.mjs` prints
    nothing. If it prints any, stop and report: the other job's bench work isn't committed yet.
  - Re-read view.js `drawMeshes` and app.js's Play in game: the other job changed both files.
  - `node vfx_bench/composer_check.mjs > /tmp/s4_composer_before.txt` and `node vfx_bench/browser_check.mjs >
    /tmp/s4_browser_before.txt`.
- [ ] **Step 1: Add steps** to `composer_check.mjs`, last:
  - `shapesTab`: Shapes lists one entry, "Dome (half sphere)", its thumbnail ready, and no `shape_dome.png` in
    `<tempBuild>\vfxedit\composer\thumbs\v2\`.
  - `domeAdds`:
    - New project `zotest`, Spell mesh; the guide's Diameter 9.14; Shapes, Dome clicked.
    - `parts` is one part, its `shape` `{ type: "dome", radius: 4.57 }`, at `[0, 0, 0]`, selected.
    - `drawn.triangles` is 960, `drawn.textured` true, and `drawn.shapes` `[{ name: "n5s_8c757fb28f99", colour: [0.6,
      0.8, 1] }]`.
    - `#part-shape` is shown: Radius 4.57, Height 4.57, Smoothness 32, Sides both, Colour `#99ccff`, Opacity 0.35, Glow
      1, Texture soft.
    - `#part-scale-l` is hidden, and `handle("scale")` is null.
  - `domeCardEdits`:
    - Height 3 gives `parts[0].shape` `{ type: "dome", radius: 4.57, height: 3 }`.
    - Colour `#ffd966` gives `colour` `[1, 0.851, 0.4]`, and `drawn.shapes[0].name` is
      `F.shapeTextureName({ colour: [1, 0.851, 0.4] })` (the copy's floatdisk.js, in Node).
    - Ctrl+Z twice gives `{ type: "dome", radius: 4.57 }`. Ctrl+Y gives the height 3 back, and Ctrl+Z again.
    - Radius 0 leaves it 4.57, with the status `Radius (m): 0.05 to 100.`
  - `domeInsideAndOut`:
    - `#comp-figure` unticked. The camera inside, almost level (`setCamera({ yaw: -0.9, pitch: 0.05, distance: 1, target: [0,
      0, 1.5] })`), so the middle ray meets the wall, not the ground. The stage's middle `pixel` with the dome is P_both,
      and with it removed (Delete, read, Ctrl+Z) P_none.
    - Sides Outside only: P_out.
    - Expected: P_both differs from P_none by more than 8 in some channel. P_out is within 2 of P_none in every
      channel: its faces are backs from inside.
    - The camera outside (distance 25): P_out differs from P_none by more than 8.
    - Ctrl+Z back to both sides.
  - `domeSaves`:
    - Save and build gives the status naming `n5m_zotest` and `n5s_8c757fb28f99`.
    - `<tempBuild>\vfxedit\saved\n5s_8c757fb28f99.tga` is 65554 bytes.
    - `saved\n5m_zotest.vfx`, read after its mark line, has one plane: bitmap `n5s_8c757fb28f99`, alpha 0.35, 960
      faces.
  - `domePlays`:
    - Play in game writes `<tempBuild>\vfxedit\n5s_8c757fb28f99.tga`, its time not after `vfxedit\bench\request.vfx`'s.
    - The request's draft has a plane with that bitmap and 960 faces.
  - `domeAsMeshBeat`:
    - Back to effects; New; Type Stays (D); + mesh, `n5m_zotest`; Fade in 0.5.
    - `vfxBench.planesDrawn()` at 1.0 s: at least 960 triangles, the dome's plane at alpha 0.35.
    - At 0.1 s: its alpha above 0 and below 0.35. The time is set as browser_check's `meshBeatDrawn` sets it.
- [ ] **Step 2: Run** `node vfx_bench/composer_check.mjs`. Expected: the seven new steps FAIL (no
  `#parts-tabs [data-tab=shapes]`).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.**
  - `node vfx_bench/composer_check.mjs`. Expected: every step ok in Step 0's run, and the seven new ones.
  - `node vfx_bench/browser_check.mjs`. Expected: every step ok in Step 0's run.
    - If one fails only because a game mesh's back is no longer drawn, change its expectation to the game's (front
      faces only) and say which in the report.
  - `node vfx_bench/check_composer.js`, `node vfx_bench/check_composer_server.js`, `node vfx_bench/check_chunks.js`,
    `node common/checks/run.js`. Expected: as after S3, `all chunk checks pass`, `all common checks pass`.
- [ ] **Step 5: Write the docs and the rows.**
  - **README.md** (the Composer): "Shapes":
    - the tab;
    - the card's fields and their defaults;
    - no Scale;
    - both sides and the front faces;
    - the soft texture: `n5s_`, where it's written, original;
    - Play in game;
    - Zone of Truth's recipe (the design's).
  - **TOOLS-PLAN.md:** a section "The Composer's shapes: the dome (2026-11-17, #39)":
    - the owner's words;
    - the calls, short;
    - *Built:* the commits;
    - Q4's answer, and Q5 and Q6 open;
    - the limits.
  - **The rows (CSV):** SH-1 to SH-4 (#39, stream animation). Each stands on its own:
    - **SH-1 (the bench, the dome):**
      - Setup: close the VFX Bench window and double-click `vfx_bench.bat`; press Composer; New project `zotruth`,
        Spell mesh; on the guide card, Diameter 9.14.
      - Do: Parts, Shapes, click Dome. Change its Colour, Opacity 0.25, Height 3, then Height 4.57. Orbit round it and
        zoom into it. Ctrl+Z and Ctrl+Y.
      - Expect: a see-through dome 9.14 m across round the figure, brighter and more solid near the ground, fading up.
        Seen from inside, its inner side shows. Each change is one undo step. Its card has no Scale.
    - **SH-2 (the bench, a mesh beat):**
      - Setup: close the VFX Bench window and double-click `vfx_bench.bat`; Composer; open `zotruth` (SH-1 made it;
        if it isn't there, make it as SH-1 says); Save and build.
      - Do: Back to effects; New; Type Stays (D); + mesh, `n5m_zotruth`; Fade in 0.6, Fade out 0.8; play the
        preview.
      - Expect: the dome fades in over 0.6 s and stays. The timeline shows its row with both fades. The Save status
        named its `n5s_` texture.
    - **SH-3 (in game, Play in game):**
      - Setup: the game running a development build of nwn5e with a character in play; the VFX Bench with SH-2's
        effect open (made as SH-2 says) and Permanent ticked.
      - Do: Play in game. Zoom the game's camera out, then in close to your character. Stop in game.
      - Expect: the dome round your character, see-through and brighter at the ground; its inside shows from inside;
        nothing flickers or goes dark. Stop fades it out. Note any other effect drawn wrongly through it.
    - **SH-4 (in game, at your feet):**
      - Setup: the game running a development build of nwn5e with a character in play; SH-2's effect saved in the VFX
        Bench as `n5_zotruth` (made as SH-2 says, then Save).
      - Do: in chat, `!vfxarea n5_zotruth 0.6 1 0`. Walk out of the dome and back in.
      - Expect: the dome stays where it was put for 60 s, see-through from outside and from inside.
- [ ] **Step 6: Commit.**
  - In nwn5e-tools, after the diff check (each file only this task's): `vfx_bench/public/composer_page.js
    vfx_bench/public/index.html vfx_bench/public/style.css vfx_bench/public/view.js vfx_bench/public/app.js
    vfx_bench/composer_check.mjs vfx_bench/README.md` (and `vfx_bench/browser_check.mjs` if Step 4 changed it), with
    "feat: the VFX Bench's Composer builds with shapes - the Shapes tab, a dome sized to the guide, its card (radius,
    height, smoothness, sides, colour, opacity, glow, texture), its texture drawn in the page and sent with Play in
    game; the stage draws only front faces, as the game (nwn5e-ee #39)".
  - In the anim worktree, after `git branch --show-current` (anim): `docs/TOOLS-PLAN.md`, with "docs: the Composer's
    shapes, the dome built - TOOLS-PLAN (#39)".
- [ ] **Step 7: Report:**
  - the commits and the rows;
  - the bench needs a restart of `vfx_bench.bat`;
  - nothing for the game's DLL: the "anim ready" for S1, S2 and S4's docs carries tools only;
  - Zone of Truth's next steps (the design's recipe), Q5 and Q6.

### Task S5: Sphere, cylinder, cone, disc and ring (nwn5e-ee anim, nwn5e-tools)

Needs S4. The page shows each new type with no page change (F.SHAPE_TYPES, F.SHAPE_FIELDS), but for the flat ones'
lift.

**Files:**
- Modify:
  - nwn5e-ee-anim: `tools/data/floatdisk/floatdisk.js`, `tools/data/floatdisk/check.js`,
    `tools/data/floatdisk/README.md`, `docs/TOOLS-PLAN.md`;
  - nwn5e-tools: `vfx_bench/public/composer.js`, `vfx_bench/public/composer_page.js`, `vfx_bench/check_composer.js`,
    `vfx_bench/composer_check.mjs`, `vfx_bench/README.md`.
- Create: the owner's test row SH-5, in the job's tmp folder.

**Interfaces:**
- **floatdisk.js:**
  - `SHAPE_TYPES = ["dome", "sphere", "cylinder", "cone", "disc", "ring"]`;
  - `SHAPE_LABELS` gains `sphere: "Sphere"`, `cylinder: "Cylinder (open)"`, `cone: "Cone (point up)"`, `disc: "Disc
    (flat, on the ground)"`, `ring: "Ring (flat band)"`;
  - `SHAPE_KEYS` gains `inner` after `height`;
  - `SHAPE_INFO.inner = { label: "Inner radius (m)", min: 0.01, max: 100, step: 0.05 }`;
  - `SHAPE_FIELDS`: sphere, cylinder and cone `["radius", "height", "segments", "sides"]`; disc `["radius", "segments",
    "sides"]`; ring `["radius", "inner", "segments", "sides"]`;
  - `SHAPE_FLAT = ["disc", "ring"]`.
  - **`shapeDefaults`:** height is 2 x the radius for a sphere, cylinder and cone. A ring's inner is 0.85 x the radius,
    to the millimetre. A disc and a ring have no height, and only a ring has an inner (readShape refuses the key, as
    any key not in its type's fields). Everything else as the dome's.
  - **`readShape`:** a ring's `inner` must be from 0.01 to below its radius: `` `${where}.inner: a number from 0.01 to
    below the radius` ``.
  - **`shapeModel`** for each type: its base at z 0, centred, each side its own vertices, the inside first (as the
    dome's), `n` segments, the seam's column twice, `m = max(2, round(n / 4))`. Counts with both sides at 32 segments:

    | type | the mesh | UV v | vertices, triangles (both sides) |
    |---|---|---|---|
    | sphere | the dome's rings mirrored below: a bottom point a segment, 2m - 1 rings, a top point a segment; height h, centre at h / 2 | 0 at the bottom, 1 at the top | 1118, 1920 |
    | cylinder | an open tube: a ring at 0 and one at h | 0 at the ground, 1 at the top | 132, 128 |
    | cone | a ring at 0 to a point a segment at h, its base open | 0 at the ground, 1 at the point | 130, 64 |
    | disc | flat at z 0, the rim to a centre point a segment; its outside faces up | 0 at the rim, 1 at the centre | 130, 64 |
    | ring | flat at z 0, the rim to the inner edge; its outside faces up | 0 at the rim, 1 at the inner edge | 132, 128 |

    So the soft texture is brightest at the ground, a disc's rim and a ring's outer edge.
- **composer.js:** **`liftOf(raw)`** is 0.02 for a disc or ring, else 0. `partFrom` sets their `at` z to it.
- **composer_page.js:** resting a part (an add, a move, Drop it) adds `C.liftOf(raw)` to `C.restHeight`. *Why:* a flat
  shape at the ground's own height flickers with it. The vision cones were lifted for the same reason (PLAN 0.31.1).

- [ ] **Step 0: Record** `git diff --stat -- src > /tmp/s5_src_before.txt` in the anim worktree, and `node
  vfx_bench/composer_check.mjs > /tmp/s5_composer_before.txt` in nwn5e-tools.
- [ ] **Step 1: Write the checks.**
  - **floatdisk `check.js`**, one a type, each on its defaults at radius 1, 32 segments, both sides, with the dome
    checks' `normals` (about the shape's centre: `[0, 0, h / 2]` for the sphere, the origin for the rest; for the flat
    ones, the normal's z):
    - `a sphere: 1118 vertices and 1920 triangles, the inside first, its lowest point at 0, v from the bottom pole` (lo
      z 0, hi z 2);
    - `a cylinder: an open tube, 132 vertices and 128 triangles, v up its side` (every vertex at radius 1, z 0 or 2);
    - `a cone: a ring to a point, 130 vertices and 64 triangles, the point at its height`;
    - `a disc: flat at 0, 130 vertices and 64 triangles, its outside facing up, v from the rim to the centre` (the last
      32 triangles' normals have z > 0);
    - `a ring: a flat band, 132 vertices and 128 triangles, its inner edge 0.85 of the radius, v across the band`. Inner
      1 or 0 throws `/inner: a number from 0\.01 to below the radius/`.
  - **`check_composer.js`:** `a flat shape rests 2 cm up: liftOf, and partFrom's at`. A disc's `liftOf` is 0.02 and its
    `partFrom` at is `[0, 0, 0.02]`; a dome's is 0.
  - **`composer_check.mjs`**, last: `otherShapes`. In `zotest`, each of Sphere, Cylinder, Cone, Disc and Ring is
    clicked in Shapes.
    - Each adds a part drawing its triangles (1920, 128, 64, 64, 128).
    - Its card shows its own fields (the Ring's Inner radius 3.885 at the guide's 4.57).
    - The disc rests 0.02 m up.
- [ ] **Step 2: Run.**
  - `node tools/data/floatdisk/check.js`. Expected: the five new checks FAIL (`s.type: dome` refuses them), then
    `floatdisk check: FAILED (50 passed)`.
  - `node vfx_bench/check_composer.js`. Expected: the new step FAILs (`C.liftOf is not a function`).
  - `node vfx_bench/composer_check.mjs`. Expected: `otherShapes` FAILs (Shapes lists one entry).
- [ ] **Step 3: Implement**, type by type in the table's order (each type's check green before the next).
- [ ] **Step 4: Run.**
  - `node tools/data/floatdisk/check.js`. Expected: `floatdisk check: 55 checks pass`.
  - `node tools/data/composer/check.js`, `node tools/data/composer/check_build.js`, `node tools/data/check_derived.js`.
    Expected: all pass.
  - `node vfx_bench/check_composer.js`, `node vfx_bench/check_composer_server.js`, `node vfx_bench/composer_check.mjs`,
    `node vfx_bench/browser_check.mjs`. Expected: all as after S4, and the new steps.
  - `git diff --stat -- src` in the anim worktree. Expected: what Step 0 recorded.
- [ ] **Step 5: Write** the READMEs' new types, TOOLS-PLAN's *Built* line, and the row:
  - **SH-5 (the bench, the other shapes):**
    - Setup: close the VFX Bench window and double-click `vfx_bench.bat`; press Composer; New project `shapes`, Spell
      mesh; on the guide card, Diameter 3.
    - Do: Parts, Shapes: add Sphere, Cylinder, Cone, Disc and Ring one by one, moving each aside with its handles. Tilt
      the cone 90 degrees about x.
    - Expect: each is see-through and brighter at the ground (a disc at its rim, a ring at its outer edge). The disc
      and the ring sit just above the ground without flicker. The tilted cone lies on its side.
- [ ] **Step 6: Commit.**
  - In the anim worktree, after `git branch --show-current` (anim): `tools/data/floatdisk/floatdisk.js
    tools/data/floatdisk/check.js tools/data/floatdisk/README.md docs/TOOLS-PLAN.md`, with "feat: the Composer's shapes,
    the rest - sphere, cylinder, cone, disc and ring, each both sides and textured from the ground up (#39)".
  - In nwn5e-tools, after the diff check: `vfx_bench/public/composer.js vfx_bench/public/composer_page.js
    vfx_bench/check_composer.js vfx_bench/composer_check.mjs vfx_bench/README.md`, with "feat: the VFX Bench's Composer,
    the other shapes - a flat shape rests 2 cm up; the Shapes tab and the card follow floatdisk.js (nwn5e-ee #39)".
- [ ] **Step 7: Report:** the commits and SH-5; the bench needs a restart of `vfx_bench.bat`.
