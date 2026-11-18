# Plan: mesh textures that move, tinted textures, keys on a mesh beat

**Spec:** `docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md`. The owner, 2026-11-18: "for the
animated textures: 1. scrolling, scaling (breathing), rotating", "for the existing textures I'd like to add tinting
options ...", "for the breathing and rotating I was thinking about the textures on the meshes though", and "is it
possible to add keyframes to animate the shapes? Or make them follow emitters, just like other elements".

**Goal:** a tint for every texture the Composer places and for a mesh beat; textures that scroll, breathe and turn on any
Composer part; keys (move, turn, scale) on a mesh beat; the stage showing all of it as the game does.

**Architecture:** floatdisk.js stays the one implementation (the page, both generators and build_derived use it): it
gains a part's `tint` and `motion`, the UV motion maths, and the material and shader text a moving texture needs. A
moving texture reaches the game as an EE material (`n5u_<hash>.mtr`, by the mesh's bitmap name) driving one vertex
shader of ours (`vsn5uvmove.shd`) - data files, no DLL change. Keys on a mesh beat need WriteVfxModel to write a plane's
keys: the one game-side change, M4, announced first.

**Five tasks:**

1. **M1:** tint - parts, library-textured shapes, the stage's formula (anim, nwn5e-tools). No game change.
2. **M2:** moving textures in the generators - the maths, the material, the shader, the probe (anim). No game change.
   Ends at a gate: the owner's TMO-1.
3. **M3:** moving textures on the bench - the card, the stage, Play in game; cycle and additive TXIs (nwn5e-tools).
4. **M4:** a plane's keys in the model and the draft (anim `src/data`, `tests/data`). **Game-side: announced first; the
   game session merges and installs.**
5. **M5:** the Mesh card's keys, Breathe and Tint; the sim (nwn5e-tools).

M4-M5 don't need M1-M3: the owner's Q4 may put them first. M3 needs M2 and TMO-1; M5 needs M4 committed on anim (in game,
also installed).

## Before starting

- **An issue.** The coordinator opens one (or names #39) before M1; every commit names it - written `(#<n>)` below.
- **The owner's answers:** Q3 (Glow on a library texture) before M1; Q1 (the route) before M2 - this plan is written for
  route A; Q2 (a motion per beat) before M3 - written for "no"; Q4 the order.
- **SHP-3** (not yet run) answers whether the game repeats a texture past UV 1. Nothing here waits on it to be built, but
  TMO-1's reading of a scroll's edges does: run SHP-3 first or with it. If M1 lands first, SHP-3's library-texture step
  sees the new Glow mapping (TIN-1 is then its retest).
- **Today's counts** (2026-11-18): `floatdisk check: 68 checks pass`, `composer check: 16 checks pass`; nwn5e-tools
  `all composer checks pass`, `all parts checks pass`, `all beats checks pass`, `all effect checks pass`, `all chunk checks
  pass`, `check.js`: `521 drafts (1884 emitters) read and written back, 233 textures decoded, 0 failed`. A task that finds
  them moved records the new numbers in its Step 1 and moves its expectations with them.
- **Other work in the same files:** before a task edits a file, `git status --short -- <its files>` in that repo prints
  nothing; if it prints any, stop and report (`git commit --only` takes the whole file).

## Global constraints

- **Repos:** the anim worktree `C:\Users\Bruno\Desktop\Projects\nwn5e-ee-anim` on branch `anim` (`git branch
  --show-current` prints `anim` before each commit); nwn5e-tools `C:\Users\Bruno\Desktop\Projects\nwn5e-tools` on `main`.
- **Commits:** a new file `git add <path>` first; then `git commit --only <paths> -F -` with the message from a file;
  never `git add -A`, never `git -C`, one git command per call, never push. `git status --short` and each file's `git
  diff` checked to be only this task's before it. Don't touch the untracked `tools/data/composer/test.json` (the owner's).
- **No game-side change outside M4:** no M1-M3 or M5 commit names a file under `src` or `tests`; `git diff --stat -- src`
  on anim is recorded in each anim task's Step 1 and is the same at its end.
- **Game files are never committed.** `n5u_` materials (and, on the copy route, `n5u_` texture copies) are git-ignored
  build output, as `n5p_` and `n5s_` are. `vsn5uvmove.shd` is ours: original text naming the game's `inc_standard` by its
  name, committed in data\files and listed in made.txt.
- **floatdisk.js loads in a page:** no `require` at its top; Buffer only in what tgaBytes and diskTexture return.
- **What you see is what's built:** the page draws with floatdisk.js's own `tintMeshes`, `uvMotion` and `uvPivot`; the
  page's checks compare with the generators' output.
- **Checks write only into temporary copies,** never the real worktree, the real bench build or the game.

---

### Task M1: Tint - parts, library-textured shapes, the stage's formula

Working folders: anim, then nwn5e-tools. Needs Q3's answer.

**Files:**
- anim: `tools/data/floatdisk/floatdisk.js`, `check.js`, `README.md`; `tools/data/composer/project.js`, `check.js`,
  `check_build.js`, `README.md`; `docs/TOOLS-PLAN.md`.
- nwn5e-tools: `vfx_bench/public/composer.js`, `composer_page.js`, `index.html`, `view.js`; `vfx_bench/check_composer.js`,
  `check_chunks.js`, `composer_check.mjs`; `vfx_bench/README.md`.

**Interfaces** (floatdisk.js, pure):
- `readTint(t, where)` -> `[r, g, b]`, `[1, 1, 1]` when absent; error `` `${where}.tint: [r, g, b], each 0 to 1` ``.
- `readPart` returns `tint` (readTint's). A shape with `tint`: `` `${where}.tint: a shape has no tint - its Colour is its
  colour` ``.
- `tintMeshes(meshes, tint)` -> copies with `ambient`, `diffuse` and `selfillum` each times the tint, through `round`;
  white gives the same numbers.
- `placePart(part, ...)` applies `tintMeshes(meshes, part.tint)` (a part without `tint`: white). One place, so both
  generators and the page get it.
- `compositionText` and project.js `projectText`: `tint` after `tilt` and `colours` (PART_KEYS), white never written.
- `shapeModel`, a library texture: `diffuse` and `ambient` = Colour x (1 - Glow), `selfillum` = Colour x Glow (each
  through `round`). Soft and None unchanged.
- composer.js `clean(raw)`: `tint` to 0.001 a channel; white (or all three at 1) dropped; a shape's dropped.
- view.js `meshData`: a textured mesh's colour per vertex `min(1, selfillum[j] + diffuse[j] x shade)` (was `shade`); an
  untextured one unchanged.

**Steps:**

- [ ] **1.** anim: `git diff --stat -- src` (record it); `node tools/data/floatdisk/check.js` -> `floatdisk check: 68 checks
  pass`; `node tools/data/composer/check.js` -> `composer check: 16 checks pass`.
- [ ] **2. Failing checks** in floatdisk `check.js`:
  - `tint: read, defaulted, refused on a shape and out of range` (each error's text);
  - `tintMeshes multiplies ambient, diffuse and selfillum; white changes nothing`;
  - `placePart tints every mesh of a tinted part` (a two-mesh model, tint [0.5, 1, 0.25]);
  - `compositionText writes tint after colours, never white`;
  - `a library texture: Glow 1 gives selfillum = Colour and no diffuse or ambient; Glow 0.4 splits 0.4 / 0.6` (Colour
    [1, 0.5, 0], `fxpa_symbl_003`: selfillum [0.4, 0.2, 0], diffuse [0.6, 0.3, 0]);
  - `Soft and None keep their material` (today's numbers).
  Run `node tools/data/floatdisk/check.js` -> `floatdisk check: FAILED (68 passed)` or fewer.
- [ ] **3.** Implement. Run -> `floatdisk check: 74 checks pass`.
- [ ] **4.** composer `check.js`: `projectText writes tint after tilt, never white` -> fails, then implement project.js ->
  `composer check: 17 checks pass`.
- [ ] **5.** composer `check_build.js` (against the game, on its copies): a placeable with a crate tinted [0.5, 1, 0.5] -
  every crate mesh in its model has diffuse and ambient halved in r and b; a spell mesh's draft planes the same. Run
  `node tools/data/composer/check_build.js` -> `composer build check: all pass`; `node tools/data/floatdisk/check_build.js`
  -> `build check: all pass` (the Floating Disk's 13 files unchanged: no part has a tint); `node
  tools/data/check_derived.js` -> `derived check: all pass`.
- [ ] **6.** nwn5e-tools `check_composer.js`: `clean: tint rounded, white dropped, a shape's dropped`; `placeOne tints
  the stage's meshes as placePart does`. `check_chunks.js`: replace `a textured mesh as before (its shade, untinted)`
  with `a textured mesh: selfillum + diffuse x shade, clamped` (diffuse [0.1, 0.1, 0.1], selfillum [1, 0, 0] gives
  [1, 0.1 sh, 0.1 sh]) and `a game mesh with ambient and diffuse 1 and no selfillum: its shade, as before`. Run each ->
  fails; implement composer.js and view.js; `node vfx_bench/check_composer.js` -> `all composer checks pass`; `node
  vfx_bench/check_chunks.js` -> `all chunk checks pass`.
- [ ] **7. The card:** index.html - in `#part-fields`, after the tilts, `<label id="part-tint-l">Tint <input type="color"
  id="part-tint"></label>` and a `None` button (`part-tint-none`); composer_page.js - shown for any part but a shape, one
  undo step when changed (PART_FIELDS' pattern), `None` removes it. composer_check.mjs: steps `partTint` (a crate added,
  Tint #80ff80: the stage's crate colours change; Save and build: the project's part has `"tint": [0.502, 1, 0.502]`;
  Ctrl+Z: no `tint`) and `libraryTintShows` (a dome with `fxpa_symbl_003`, Colour orange, Glow 1: the stage mesh's
  vertex colours are the Colour). Run `node vfx_bench/composer_check.mjs` -> `all passed`; `node
  vfx_bench/browser_check.mjs` -> `all passed` (the effect mode's textured meshes: check no step relied on their colour
  being the shade alone; fix the expectation if one did, saying so in the commit).
- [ ] **8. Docs:** floatdisk and composer READMEs (`tint`; a library texture's Glow); vfx_bench README (the part card's
  Tint; the stage's formula); TOOLS-PLAN - M1's paragraph and rows TIN-1, TIN-2 (the design's table, written as the test
  board's rows: Setup naming the bench and the project, Do, Expect).
- [ ] **9. Commit** anim: `git add` none new; `git commit --only tools/data/floatdisk/floatdisk.js
  tools/data/floatdisk/check.js tools/data/floatdisk/README.md tools/data/composer/project.js tools/data/composer/check.js
  tools/data/composer/check_build.js tools/data/composer/README.md docs/TOOLS-PLAN.md -F -` ("feat: tint for the
  Composer's parts and library textures ... (#<n>)"). nwn5e-tools: `git commit --only vfx_bench/public/composer.js
  vfx_bench/public/composer_page.js vfx_bench/public/index.html vfx_bench/public/view.js vfx_bench/check_composer.js
  vfx_bench/check_chunks.js vfx_bench/composer_check.mjs vfx_bench/README.md -F -` (naming nwn5e-ee #<n>).
  `git diff --stat -- src` on anim as in Step 1.

---

### Task M2: Moving textures in the generators - the maths, the material, the shader, the probe

Working folder: anim. Needs Q1 = A (else stop: B and C are other plans).

**Files:**
- Modify: `tools/data/floatdisk/floatdisk.js`, `check.js`, `files.js`, `build.js`, `check_build.js`, `README.md`;
  `tools/data/composer/build.js`, `project.js`, `check.js`, `check_build.js`, `README.md`; `tools/data/build_derived.js`,
  `tools/data/check_derived.js`; `.gitignore`; `docs/TOOLS-PLAN.md`; `tools/data/floatdisk/made.txt`.
- Create: `data/files/vsn5uvmove.shd` (written by the generator from `motionShaderText()`, committed);
  `tools/data/composer/uvprobe.js` (the probe's writer).

**Interfaces** (floatdisk.js, pure):
- `MOTION_KEYS = ["scroll", "turn", "breathe", "rate"]`; `MOTION_INFO`: `scroll` two numbers -10..10 (repeats a second,
  u then v), `turn` -720..720 (degrees a second), `breathe` 0..0.9, `rate` 0.05..5 (breaths a second; needed when
  `breathe` > 0); labels as the design's sketch.
- `readMotion(m, where)` -> `{ scroll: [u, v], turn, breathe, rate }` (absent: all 0, rate 0.5); each error names its
  field (`` `${where}.motion.turn: a number from -720 to 720 (degrees a second)` `` ...). `cleanMotion(m)` -> only the
  non-zero keys, or `undefined` (no motion). `hasMotion(m)`.
- `readPart` returns `motion` (readMotion's); `compositionText` / `projectText` write it after `tint`, inline.
- `uvPivot(tverts)` -> the middle of the UV box `[(umin + umax) / 2, (vmin + vmax) / 2]`, through `round`.
- `uvMotion(motion, pivot, t)` -> `[a, b, c, d, e, f]` with u' = a u + b v + c, v' = d u + e v + f:
  s = 1 + breathe x sin(2 pi rate t); th = -turn x t x pi / 180; uv' = R(th) (uv - p) / s + p - scroll x t.
- `MOTION_TEXTURE = /^n5u_[0-9a-f]{12}$/`; `MOTION_SHADER = "vsn5uvmove"`.
- `motionName(texture, motion, pivot)` -> `n5u_` + 12 hex digits of cyrb53(`uv:<texture>:<numbers to 0.001>`), as
  `shapeTextureName` makes its.
- `MOTION_PARAMS = ["n5ScrollU", "n5ScrollV", "n5Turn", "n5Breathe", "n5Rate", "n5PivotU", "n5PivotV"]`.
- `motionMaterialText({ texture, motion, pivot }, { copy = false } = {})` -> the .mtr: `customshadervs vsn5uvmove`,
  `customshaderfs fslit`, `texture0 <texture>` (left out with `copy`), then `parameter float <name> <value>` for each of
  MOTION_PARAMS, LF line ends.
- `motionShaderText()` -> the .shd: a comment naming nwn5e and #<n>; `#define SHADER_TYPE 1`, `FOG 1`, `KEYHOLING 1`,
  `NORMAL_MAP 0`, `ENVIRONMENT_MAP 0`, `LIGHTING 1` (the defines the game's lit pair takes); `#include "inc_standard"`;
  `uniform float` each of MOTION_PARAMS; `main()` calls `ApplyStandardShader()`, then moves `vVertexTexCoords` by the
  formula with `t = float(worldtimerTimeOfDay) * 0.001` (declared by the include chain; if the compile needs it, `#include
  "inc_uniforms"`). No backtick, no game text copied.
- `moveTextures(meshes, motion)` -> `{ meshes (copies; each textured mesh's bitmap renamed motionName(bitmap, motion,
  uvPivot(its tverts))), materials: [{ name, texture, motion, pivot }] }` - each name once; no motion: the same meshes,
  no materials. Applied after a part's bakes and shape textures are named (the real texture is texture0).
- files.js: `motionMaterials(placed)` -> Map(name -> { texture, motion, pivot, text, label }); `motionWrites(materials,
  dir)` -> `<dir>\<name>.mtr`, kind `"material"`, plus `<dir>\vsn5uvmove.shd`, kind `"shader"`; `motionLines`,
  `motionJson`. `ownership`: `"material"` ours by its name (MOTION_TEXTURE: only the generator makes them, the name fixes
  the bytes); `"shader"` ours when made.txt lists it (data\files) or its bytes are `motionShaderText()`'s (--saved).
- Both build.js: write them beside what names them (a placeable's or the disk's data\files; a spell mesh's --saved),
  the summary's lines, `--json`'s `materials`. build_derived.js carries the `n5u_` materials as it carries bakes.
- `.gitignore`: `/data/files/n5u_*`.
- `uvprobe.js --saved <folder> [--game <dir>]`: writes `n5m_uvprobe.vfx` (a spell mesh draft carrying project.js's MARK)
  with six domes of radius 0.4 in a row along x, 1 m apart: (1) Soft, Wrapped, scroll [0, 0.25]; (2) `fxpa_symbl_003`
  From above, turn 30; (3) Soft, From above, breathe 0.3 rate 0.5; (4)-(6) the same by the copy route. Beside it: their
  soft textures, the six materials (1-3 with `texture0`, 4-6 without), the shader, and for 4-6 the texture copied under
  its `n5u_` name - a soft one made again, `fxpa_symbl_003` from the game with its TXI (reader.js's `image`). It refuses a
  folder that isn't a bench build's `vfxedit\saved`, and overwrites only files carrying its mark or `n5u_` / `n5s_` names.

**Steps:**

- [ ] **1.** `git diff --stat -- src` (record); the counts (`floatdisk check: 74 checks pass` after M1).
- [ ] **2. Failing checks** (floatdisk `check.js`):
  - `motion: read, defaulted, cleaned to its non-zero keys, each error`;
  - `uvMotion: none is identity; scroll [0, 0.25] at t 2 moves v by -0.5; turn 90 at t 1 maps p + (1, 0) to p + (0, -1)`
    (the picture turned counter-clockwise); `breathe 0.5 rate 1 at t 0.25 divides uv - p by 1.5`;
  - `uvPivot: a Wrapped dome with repeats 3 / 2 gives [1.5, 1]; From above gives [0.5, 0.5]`;
  - `motionName: n5u_ and 12 hex, the same for the same texture, motion and pivot, another for each change`;
  - `motionMaterialText: its lines, texture0 left out with copy, every MOTION_PARAMS name once`;
  - `motionShaderText: declares every MOTION_PARAMS name, calls ApplyStandardShader before the UVs, reads
    worldtimerTimeOfDay, has no backtick`;
  - `moveTextures: renames textured meshes, leaves untextured ones, one material a name`;
  - `compositionText and readPart carry motion`; `a page loads it` (as the shapes' page check).
  Run -> FAILED.
- [ ] **3.** Implement floatdisk.js -> `floatdisk check: 83 checks pass` (74 and these 9).
- [ ] **4.** files.js and both build.js; composer `check.js` (projectText's `motion`) -> `composer check: 18 checks pass`.
  composer `check_build.js`: `a spell mesh with a moving Soft dome: its draft's bitmap is the material's name; the .mtr
  and the shader beside it in --saved; the soft texture too`; `a placeable with a moving crate: n5u_ materials in
  data\files, the shader there, made.txt lists the shader`; `a build with no motion writes no material and no shader`;
  `a second build changes nothing`. `node tools/data/composer/check_build.js` -> `composer build check: all pass`.
  floatdisk `check_build.js` -> `build check: all pass` (the disk's 13 files unchanged). `check_derived.js`: a derived
  build carries a placeable's materials -> `derived check: all pass`.
- [ ] **5. The committed shader:** write `data/files/vsn5uvmove.shd` from `motionShaderText()` (a one-off `node -e`
  through floatdisk.js, line ends as the generator writes text there: `F.crlf`) and add its path to made.txt. A placeable
  build then finds it ours and leaves it. Check from the worktree:
  `node -e "const F=require('./tools/data/floatdisk/floatdisk.js');process.exit(F.sameText(require('fs').readFileSync('data/files/vsn5uvmove.shd'),F.motionShaderText())?0:1)"`
  exits 0.
- [ ] **6. The probe:** `uvprobe.js` and its check in composer `check_build.js` (`the probe writes six domes, six
  materials - three with texture0 - the shader and three copies, into a temporary saved\ only`) -> `composer build
  check: all pass`.
- [ ] **7. Docs:** READMEs (a part's `motion`; the files a moving texture brings; the probe), TOOLS-PLAN (M2's
  paragraph; rows TMO-1 to TMO-3; the probe's command for TMO-1's Setup: `node tools\data\composer\uvprobe.js --saved
  <nwn5e-ee>\build\vfxedit\saved` from the anim worktree, then restart vfx_bench.bat).
- [ ] **8. Commit** anim (`git add data/files/vsn5uvmove.shd tools/data/composer/uvprobe.js` first; then `git commit
  --only` with every file above). `git diff --stat -- src` as in Step 1.
- [ ] **9. Gate:** report to the coordinator: TMO-1 is the owner's next step. M3 starts only after TMO-1's result, with
  the route it shows (`texture0`, or the copy route: `motionMaterialText` called with `copy`, and moveTextures' caller
  writing the copies - one line in M3's Step 1 and a check), or stops if neither moved (Q1's fallback).

---

### Task M3: Moving textures on the bench - the card, the stage, Play in game; cycle and additive TXIs

Working folder: nwn5e-tools (and anim for the route's one line, if TMO-1 chose the copy route). Needs M2 and TMO-1.

**Files:**
- `vfx_bench/composer_server.js` (textures, the bake route), `vfx_bench/server.js` (if the effect mode's texture route
  is its own), `vfx_bench/public/composer.js`, `composer_page.js`, `index.html`, `view.js`, `app.js`;
  `vfx_bench/check_composer.js`, `check_composer_server.js`, `check_chunks.js`, `composer_check.mjs`, `browser_check.mjs`;
  `vfx_bench/README.md`; anim `docs/TOOLS-PLAN.md`.

**Interfaces:**
- **The server's texture info** (`textureInfo`): adds `cycle: { numx, numy, fps } | null` (TXI `proceduretype cycle`)
  and `additive` (TXI `blending additive`). An `n5u_` name resolves through its .mtr (beside: the checkout's data\files,
  saved\, vfxedit\): `{ draw: "texture", of: <texture0 or the copy>, motion, pivot }` - the picture served is the real
  texture's.
- **Play in game** (`POST /api/composer/bake`) takes `materials: [{ texture, motion, pivot }]` beside `textures` and
  `shapes`; writes each .mtr and the shader into vfxedit\ (and, on the copy route, the copies), checked first, all or
  nothing.
- **composer.js:** `clean` keeps `motion` (F.cleanMotion, numbers to 0.001; none: dropped); `placeOne` gives each stage
  mesh `uvMotion: { motion, pivot: F.uvPivot(its tverts) }` for a moving part (its bitmap stays the real texture).
- **view.js:** the mesh program gains `uniform mat3 u_uv` (`v_uv = (u_uv * vec3(a_uv, 1.0)).xy`; the particles' identity).
  `uvMatrix(item, t)` (pure, exported): from `F.uvMotion` with v flipped as the stage holds its UVs (u, 1 - v), or for a
  `cycle` texture the frame at t (scaled 1 / numx, 1 / numy, offset to frame floor(t x fps) mod (numx x numy), row-major
  from the top), or identity. `drawMeshes(list, cam, t)` sets it per mesh; additive meshes go in a pass of their own,
  blended `SRC_ALPHA, ONE`, after the see-through ones.
- **The effect mode** (app.js): `drawMeshes(..., sim.time)`; a plane whose bitmap is `n5u_` draws its texture with its
  motion (the server's info); a library `cycle` or additive texture likewise.
- **The Composer's clock:** composer_page.js redraws on `requestAnimationFrame` while a shown mesh has `uvMotion` or a
  `cycle` texture and the page is visible; otherwise as now.
- **The card:** a **Texture motion** group after the part's own fields (a shape's after Texture): Scroll round, Scroll
  up (repeats a second), Turn (degrees a second), Breathe and Breaths a second (MOTION_INFO's bounds and titles), each
  one undo step, refused past its bounds as typed (the shapes' rule); shown while the part has a textured mesh.
- **The Mesh card** (app.js `renderMeshCard`): a note naming its planes' motion when one is `n5u_` ("Its texture scrolls
  up 0.25 a second - from the spell mesh n5m_x; change it there").

**Steps:**

- [ ] **1.** `git status --short` (the files above print nothing); the counts. If TMO-1 chose the copy route: anim's one
  line (`MOTION_COPY = true` in floatdisk.js, read by moveTextures' callers) with its check in floatdisk `check.js`
  (`the copy route: materials without texture0, copies listed`) -> `floatdisk check: 84 checks pass`, committed on anim
  on its own.
- [ ] **2. Failing checks:** `check_chunks.js` - `u_uv is identity for every mesh without motion`; `uvMatrix equals
  F.uvMotion with v flipped (t 0, 1.3, 7)`; `a cycle texture's frame at t (4 x 4 at 32: t 0.1 -> frame 3)`; `an additive
  mesh is drawn in its own pass`. `check_composer.js` - `clean: motion rounded, none dropped`; `placeOne: a moving part's
  meshes carry uvMotion with F.uvPivot's pivot`. `check_composer_server.js` (against the game, on copies) - `an n5u_
  name serves its texture0's picture with its motion`; `fxpa_flame02's info has cycle 4 x 4 at 32`; `fxpa_symbl_003's is
  additive`; `Play's materials written into vfxedit\ with the shader; a bad one refuses all`. Run each -> fails.
- [ ] **3.** Implement the server, composer.js, view.js. `node vfx_bench/check_chunks.js` -> `all chunk checks pass`;
  `node vfx_bench/check_composer.js` -> `all composer checks pass`; `node vfx_bench/check_composer_server.js` -> its
  all-pass line.
- [ ] **4.** The card, the clock, Play, the effect mode. `composer_check.mjs` steps: `textureMotionCard` (a dome, Scroll
  up 0.25: two frames 0.5 s apart give different UV matrices; the saved project has `"motion": { "scroll": [0, 0.25] }`;
  Ctrl+Z: none); `motionPlays` (Play in game with a moving dome: the bake request carries its material); `motionBounds`
  (Breathe 0.95 refused as typed). `browser_check.mjs` steps: `beatTextureMoves` (+ mesh of a spell mesh with a moving
  dome: the stage's matrix changes with the preview's time; the card's note names the motion); `cycleRing` (a fixture
  effect whose plane names fxpa_flame02, as Elemental Shield's ring: one frame drawn, a sixteenth of the sheet). Run `node vfx_bench/composer_check.mjs` -> `all passed`;
  `node vfx_bench/browser_check.mjs` -> `all passed`.
- [ ] **5. Docs:** vfx_bench README (the group, the stage's clock, cycle and additive); TOOLS-PLAN (M3's paragraph; the
  rows TMO-2, TMO-3 updated to the card's steps).
- [ ] **6. Commit** nwn5e-tools (`git commit --only` the files above); anim TOOLS-PLAN.

---

### Task M4: A plane's keys in the model and the draft (game-side: announce first)

Working folder: anim. **Game-side:** `src/data/vfx_model.*` is shared with the game session. Before editing, the
coordinator sends the game session the announcement: the files (`src/data/vfx_model.h`, `src/data/vfx_model.cpp`,
`tests/data/vfx_model_test.cpp`, a new fixture `tests/data/fixtures/plane_moves.vfx`), what changes (below), and that
no model written today changes. Edit only after it's acknowledged.

**Interfaces:**
- `VfxKeys` gains `std::vector<float> scale;` - rows t s (a node's uniform scale, the model's `scalekey`, controller 36,
  as vdr_bigby_int keys its hand).
- `VfxPlane` gains `float tint[3] = {1.0f, 1.0f, 1.0f};` - the bench's (a mesh beat's tint: its material is already
  multiplied by it), in the draft only, never in the model.
- **The draft:** `DraftKeys` writes `<anim>_scale = t s ...` after `_alpha`; `DraftKeyLine` reads it (any section);
  WriteVfxDraft writes a plane's `tint = r g b` after `emit_seconds` when it isn't 1 1 1, ReadVfxDraft reads it. A draft
  without them is written back byte for byte.
- **`LastKeyTime`** counts a plane's position, orientation and scale keys as well as its alpha.
- **WriteVfxModel's animations:** a plane with any keys in that animation - position (>= 4 numbers), orientation (>= 5),
  scale (>= 2), alpha (>= 2) - gets `node trimesh <its name>`, `parent <its parent>`, then `positionkey` and
  `orientationkey` (the nodes' `key_lines`), `scalekey n` with its rows, `alphakey` as now, `endnode`. A plane without keys
  there is in no animation, as now. The geometry unchanged.

**Steps:**

- [ ] **1.** The announcement acknowledged. `git status --short -- src tests` prints nothing. From the worktree, `cmd /c
  "set NWN5E_PRIVATE=C:\Users\Bruno\Desktop\Projects\nwn5e-ee\private&& test.bat"` -> record its last line, `<N> tests,
  0 failed` (test.bat builds into this worktree's own build\, never the main checkout's).
- [ ] **2. Failing tests** in `vfx_model_test.cpp` (fixture `plane_moves.vfx`: one plane under `ring`, impact position
  keys `0 0 0 3 0.5 0 0 0`, orientation keys `0 0 0 1 0 1 0 0 1 3.14159`, scale keys `0 0.2 0.5 1`, alpha as
  plane_beat's; duration orientation keys for a turn):
  - `VfxModelPlaneMoveKeys`: impact's `node trimesh n5t_p0_beat` holds `parent n5t_n0_ring`, `positionkey 2`,
    `orientationkey 2`, `scalekey 2` with its rows, then `alphakey 5`; duration's holds its `orientationkey`;
    cessation has none (the plane has no keys there);
  - `VfxModelPlaneMoveKeysLength`: the impact is at least as long as the last of its keys;
  - `VfxDraftScaleAndTint`: the fixture read and written back byte for byte; `impact_scale` and `tint = 1 0.5 0.5` read;
  - `VfxModelUnchangedWithoutPlaneKeys`: for every fixture under `tests/data/fixtures/*.vfx` but plane_moves (none has a
    plane's move keys), the model has no `scalekey` and its animations' trimesh nodes are exactly its planes with alpha
    keys there, as before; the existing golden tests (plane_beat's) stay as they are.
  `test.bat` -> `<N + 4> tests, 3 failed` (or more: the last may pass already).
- [ ] **3.** Implement. `test.bat` -> `<N + 4> tests, 0 failed`.
- [ ] **4. Mutation check:** each a deliberate break, `test.bat` shows it failing, then restored: (a) `scalekey`'s count
  written one short; (b) LastKeyTime ignoring a plane's scale; (c) `tint` written at 1 1 1; (d) the plane node left out
  of an animation with only position keys. Each must fail a test; restored -> `0 failed`.
- [ ] **5.** TOOLS-PLAN: M4's paragraph (what the next install carries: a mesh beat's keys in game; nothing changes for
  existing effects). **Commit** on anim: `git add tests/data/fixtures/plane_moves.vfx`; `git commit --only
  src/data/vfx_model.h src/data/vfx_model.cpp tests/data/vfx_model_test.cpp tests/data/fixtures/plane_moves.vfx
  docs/TOOLS-PLAN.md -F -` ("feat: a mesh beat's move, turn and scale keys in the effect model (#<n>)"). The coordinator
  sends "anim ready at <commit>" with the matrix (MBK-1 to MBK-3) after M5; the game session merges, versions, builds,
  launch-checks and installs.

---

### Task M5: The Mesh card's keys, Breathe and Tint; the sim

Working folder: nwn5e-tools. Needs M4 committed on anim (its fixture).

**Files:** `vfx_bench/public/core.js`, `sim.js`, `parts.js`, `beats.js`, `app.js`, `index.html`; `vfx_bench/check.js`,
`check_beats.js`, `check_parts.js`, `browser_check.mjs`; `vfx_bench/README.md`; anim `docs/TOOLS-PLAN.md`.

**Interfaces:**
- **core.js:** `draftKeys` / `draftKeyLine` take `scale` (the regex gains it; written after `alpha`); `findKeys` creates
  `{ anim, position: [], orientation: [] }` as now (scale and alpha added where set); `plane()` gains `tint: [1, 1, 1]`,
  the draft's `tint = r g b` written and read as the C++ does.
- **sim.js `planesAt(t)`:** a plane's own keys in the animation playing - its position (`sample(keys.position, 4, ...)`)
  or `p.position`; its turn (`sample(keys.orientation, 5, ..., true)`) or `quat(p.orientation)`; its scale
  (`scalarAt(keys.scale, time)`) or 1 - its vertices scaled, then turned, then placed under its parent's place. `lastKey`
  counts a plane's position, orientation and scale keys.
- **parts.js:** `breatheKeys(length, amount, rate)` -> rows t s: 1 + amount x sin(2 pi n u / length) sampled 12 a breath,
  n = max(1, round(rate x length)) whole breaths, starting and ending at 1 (no jump at the loop).
- **app.js:**
  - `keyTable(item, anim, clearLabel, onChange)` - the redraw after a change is `onChange` (the Motion card's own today);
    the Mesh card passes one that copies the first plane's position, orientation and scale keys onto every plane of the
    group, then `renderMeshCard()`, `changed(true)`, `sim.restart()`.
  - The Mesh card's **Moves** box: the key tables (keyAnims of the first plane), Drop in (`parts.dropIn` on the first
    plane, copied), Make it spin (turn keys in the loop, whole turns, as a node's), **Breathe** (how much, breaths a
    second: `breatheKeys` into the loop's scale keys), Clear its keys (move, turn and scale; its fades kept).
  - **Tint:** `#mesh-tint` (a colour) and None; `tintMesh(key, tint)` sets each plane's material by beats.js's pure
    `tintPlanes(base, tint)` (the spell mesh's draft, `readMesh`, times the tint, through `core.num`'s rounding; `tint`
    kept on every plane); one undo step. A mesh changed since it was copied (as Scale tells it): the copy's own material
    divided by its old tint, times the new.
  - `rekeyMesh` keeps position, orientation and scale keys (it rebuilds only alpha); `refreshMesh` keeps the group's
    keys and tint (copied from the first plane onto the new planes, the tint applied to the mesh's material); `addMesh`
    gives tint 1.
  - `meshKeysLine` names move, turn and scale keys beside the alpha ones.

**Steps:**

- [ ] **1.** `git status --short` (the files above print nothing); the counts.
- [ ] **2. Failing checks:**
  - `check.js`: `the C++ fixture plane_moves.vfx (anim worktree: NWN5E_EE or ../nwn5e-ee-anim) read and written back
    byte for byte by core.js` (skip with a note when the worktree isn't there);
  - `check_beats.js`: `planesAt moves a plane by its position keys, turns it by its turn keys, scales it by its scale keys,
    under its parent's place`; `lastKey counts a plane's move, turn and scale keys`; `tintPlanes: the mesh's material
    times the tint; tint 1 gives it back`;
  - `check_parts.js`: `breatheKeys: whole breaths in the loop, 1 at both ends, its peak 1 + amount`.
  Run each -> fails. (Refresh keeping the keys and tint is app.js's: Step 4's browser steps check it.)
- [ ] **3.** Implement core.js, sim.js, parts.js, beats.js. `node vfx_bench/check.js` -> `521 drafts (1884 emitters) read
  and written back, 233 textures decoded, 0 failed` (plus the fixture's line); `node vfx_bench/check_beats.js` -> `all
  beats checks pass`; `node vfx_bench/check_parts.js` -> `all parts checks pass`; `node vfx_bench/check_effect.js` ->
  `all effect checks pass` (unchanged).
- [ ] **4. The card.** index.html: in `#mesh-card`, after the fades, `<label>Tint <input type="color" id="mesh-tint">
  </label>` with `mesh-tint-none`, and `<div id="mesh-moves"></div>` for the key box. `browser_check.mjs` steps:
  `meshKeys` (a beat: + key in Start, its z 1: the stage's mesh rises over the start; Ctrl+Z: no key); `meshDropIn`
  (Drop in 3 m: two keys, the mesh falls); `meshSpinBreathe` (Make it spin, Breathe 0.1: turn and scale keys in the loop,
  the stage turns and scales it, whole cycles); `meshFollows` (Hangs under an emitter with Make it orbit: the beat goes
  round with it); `meshTint` (Tint red: the planes' diffuse red; Refresh from the mesh: still red and its keys kept;
  Ctrl+Z: as the mesh made it). `node vfx_bench/browser_check.mjs` -> `all passed`.
- [ ] **5. Docs:** vfx_bench README (the Mesh card's Moves, Breathe and Tint; in game after the install carrying M4);
  TOOLS-PLAN (M5's paragraph; rows MBK-1 to MBK-3 and TIN-3, each Setup naming the bench, the spell mesh to make, and
  "after the install carrying anim <M4's commit>").
- [ ] **6. Commit** nwn5e-tools (`git commit --only` the files above); anim TOOLS-PLAN. The coordinator sends "anim ready
  at <commit>" (M4 and the docs) with the matrix.

---

## Review focus

1. **What you see is what's built:** M1 `placeOne tints the stage's meshes as placePart does`; M3 `uvMatrix equals
   F.uvMotion with v flipped`; M5 the core.js / C++ fixture round trip.
2. **Nothing written today changes:** no tint, no motion and no plane keys leave every model, draft and project byte for
   byte (M1 Step 5's disk check, M2 Step 4's no-motion build, M4's `VfxModelUnchangedWithoutPlaneKeys`, M5's check.js).
3. **The game-side change is only M4's,** announced and acknowledged before its first edit; `git diff --stat -- src` as
   it was through M1-M3 and M5.
4. **Game files never committed:** `n5u_` git-ignored; the shader ours; the probe's copies only in a bench build.
5. **Each new field refused past its bounds as typed** (the Composer's rule) and one undo step.
