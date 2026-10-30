# Bench redesign, phase 2: the bench's foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new Mocap Bench in `nwn5e-tools/mocap_bench/` that opens a FreeMoCap take as a re-editable project, cleans it (clean-up v2), solves it onto the eight bodies with today's carried-over solve, previews it through the game's own assembly code (the helper), shows it on one stage with a ghost and the capture's points on the doll's proportions, edits the kept and standing spans, the moments and the foot contacts on one timeline with undo, and saves a project file plus an assembled recipe the installed DLL (0.58.3) builds.

**Architecture:** The page is plain browser JavaScript split into small UMD modules (`public/core/*.js`) that Node checks `require` directly, plus three UI modules (stage, timeline, inspector) wired by `app.js`. A dependency-free Node server serves the page, lists recordings, reads and writes projects and recipes under nwn5e's `build\animedit\`, and keeps one long-running `nwn5e_anim_helper serve` process that assembles recipes with nwn5e-ee's C++ core (phase 1's `AssembleAnimClip`), so the preview is the game's math. A memoized pipeline (clean → suggest → solve → recipe → assemble) re-runs only the stages an edit touches.

**Tech Stack:** Node 22 (installed v22.14.0; nothing beyond Node 18 is required except the browser check's built-in `WebSocket`), no npm packages; browser canvas 2D; C++17 (VS2019 `cl`) for the helper; headless Chrome (`C:\Program Files\Google\Chrome\Application\chrome.exe`) driven over the DevTools protocol for the browser check.

**Spec:** `nwn5e-ee/docs/BENCH-REDESIGN.md` — "Build order" 2, "The workflow", "The screen", "The model", "Shipping", "Clean-up, from the research", "Checks". Issue #88 (epic #86).

## Global Constraints

- Everything lives in `nwn5e-tools` (`mocap_bench/`, `anim_helper/`); the old `recipe_bench/` is not touched (it stays until phase 7). No nwn5e-ee `src\` change in this phase and no `kVersion` bump. If one turns out to be needed, it is announced to the game session first with the files named (nwn5e-ee CLAUDE.md, "Two sessions in one checkout").
- Local and dependency-free: `127.0.0.1` only, started by `mocap_bench\mocap_bench.bat`, Node with no npm installs, works offline. Every request's `Host` must be `127.0.0.1:<port>` or `localhost:<port>`; every POST's `Origin` must be the page's own. Never hosted or published as an artifact.
- **Port 8767.** The spec says 8766, but that is the VFX Bench's port (8765 is the old recipe bench). Task 16 corrects the spec's line.
- Public repos, SRD only: no game file and nothing made from one is committed. Check fixtures made from NWN animations are generated at check time into a temp folder.
- Projects: `<build>\animedit\projects\<name>.json`; recipes: `<build>\animedit\recipes\<name>.recipe` (`<build>` defaults to `..\nwn5e-ee\build`, `--build` / `NWN5E_BUILD` override, as the old bench).
- Names: `^[a-z0-9_]{1,16}$` (`!anim` plays at most 16 characters). Slots: `n5_shove`, `n5_rage`, `n5_cridle`.
- One clock for edits: every edit is stored in take time (seconds from the recording's first frame after resampling).
- Key tolerance fixed in code: 0.75° turns, 4 mm root; a third of each for a locked foot's leg and for the root when any foot is locked (today's rule).
- One line of help per selection; the README holds the rest. No paragraphs of explanation in the page.
- Shortcuts in this phase: Space, ← →, Delete, Ctrl+Z / Ctrl+Y, 1-5. (W / E / K come with phases 3-4.)
- Save refuses when the helper's stamp differs from the `core_stamp.txt` beside the DLL in `<build>`, or when that file is missing.
- Process: TDD; every check gets deliberate breaks it catches (run them, ledger them); headless Chrome only with its own temp profile, killed by its own PID; edit scripts are files run with `python -I`; stage by explicit path, never `git add -A`; nwn5e-ee docs committed with `git commit --only <paths> -F -`.

## Review Focus

1. **A recording whose timestamps don't match its body file's frame count.** The bench must say the clock is a guess (the take's report line) and still open the take at 30 a second, not misplace time silently. (Task 3 test `stampsMismatchReported`.)
2. **A project whose recording folder was moved or deleted.** Opening it must say which folder is missing and keep the project's edits untouched; nothing gets re-suggested over them. (Task 11 test `missingRecordingKeepsProject`, Task 15 browser step.)
3. **Save while the helper and the DLL are different builds, or the DLL was never built.** Refuse, say how to fix it, write nothing. (Task 11 tests `saveRefusedOnStampMismatch`, `saveRefusedWithoutDllStamp`.)
4. **Spans dragged to nothing or past the take's ends.** The kept span must stay at least 2 frames long and inside the take; the standing span at least 0.2 s. Never a NaN pose and never a crash. (Task 9 `validateClamps`, Task 13 `dragClamps`.)
5. **A take with no usable floor contact** (a jump, or no standing part). Contacts come out empty, the solve still runs with no NaN, and the report says how the floor was found. (Task 6 `noContactsNoCrash`, Task 5 `floorFromWholeTake`.)

---

## File structure (nwn5e-tools)

| File | Responsibility |
|---|---|
| `anim_helper/main.cpp` (modify) | adds `serve <libraries_dir>`: a long-running line protocol for `assemble` and `stamp` |
| `anim_helper/helper.js` (new) | Node side of the helper: find the game, extract the libraries, read the two stamps, run `serve` |
| `anim_helper/serve_check.js` (new) | checks the `serve` protocol |
| `mocap_bench/mocap_bench.bat`, `server.js`, `README.md` | the bench's start, server, docs |
| `mocap_bench/public/core/math.js` | the one vector/quaternion module (`MocapMath`) |
| `mocap_bench/public/core/library.js` | compiled-model reader, clip lookup up the chain, sampling, the one FK (`MocapLibrary`) |
| `mocap_bench/public/core/take.js` | the capture readers, the landmark table, recording file picking (`MocapTake`) |
| `mocap_bench/public/core/cleanup.js` | clean-up v2: clock, repair, outliers, gaps, swaps, filtering, axes, floor (`MocapCleanup`) |
| `mocap_bench/public/core/suggest.js` | suggested spans, contacts, hit, loop end (`MocapSuggest`) |
| `mocap_bench/public/core/solve.js` | the carried-over solve, foot locks, the stance frame, tracks as deltas (`MocapSolve`) |
| `mocap_bench/public/core/recipe.js` | the assembled recipe's text (`MocapRecipe`) |
| `mocap_bench/public/core/project.js` | the project model, validation, slots, history (undo/redo) (`MocapProject`) |
| `mocap_bench/public/core/pipeline.js` | memoized stages from a project to poses (`MocapPipeline`) |
| `mocap_bench/public/stage.js`, `timeline.js`, `inspector.js`, `app.js`, `index.html`, `style.css` | the page |
| `mocap_bench/checks/*.js`, `checks/run.js` | Node checks; `run.js` runs them all |
| `mocap_bench/browser_check.mjs`, `checks/cdp.mjs` | the headless-browser check (the spec's path) and its DevTools client |

Every `public/core/*.js` file uses the UMD pattern of `recipe_bench/public/core.js` (`module.exports` in Node, a `window` global in the page), and takes its dependencies with `typeof require === "function" ? require("./x.js") : root.X`. `index.html` loads them in dependency order.

---

## Part A — the engine

### Task 1: The helper's `serve` mode and its Node client

**Files:**
- Modify: `anim_helper/main.cpp`, `anim_helper/README.md`
- Create: `anim_helper/helper.js`, `anim_helper/serve_check.js`

**Interfaces:**
- Consumes: phase 1's `Assemble` path in `main.cpp` (`FindAnimClip`, `AssembleAnimClip`), `extract-libraries`, `stamp`.
- Produces:
  - `nwn5e_anim_helper serve <libraries_dir>`, a line protocol on stdin/stdout. Libraries are loaded on first use and kept.
    - Request `assemble <library> <n>\n` followed by exactly `n` bytes of recipe text. Response: one line, phase 1's JSON with `"ok":true` first: `{"ok":true,"library","length","fps":30,"frames","nodes":[...]}`.
    - Request `stamp\n`. Response: `{"ok":true,"stamp":"<hex>"}`.
    - Request `quit\n`: exits 0.
    - Any failure: `{"ok":false,"error":"<text>"}`. The process keeps serving. Strings are JSON-escaped (`"` `\` and control characters), and non-finite numbers are written as `null`.
  - `helper.js` (CommonJS):
    - `findGameDir({ game, buildDir }) -> string`: `--game`, else `NWN_GAME_DIR`, else nwn5e.ini's `[loader] game_dir`, else the Steam default (as `recipe_bench/server.js`).
    - `extractLibraries({ helper, gameDir, out }) -> { ok, error, names }`
    - `readStamps({ helper, buildDir }) -> { helper, dll, match }`: `dll` is null when `<build>\core_stamp.txt` is missing.
    - `startServe({ helper, libraries }) -> { assemble(library, text) -> Promise<object>, stamp() -> Promise<string>, close() }`: one child process, requests queued, answered in order. A dead child rejects pending requests with "the helper stopped" and is restarted on the next request.

- [ ] **Step 1: Write `serve_check.js`** (spawns the built exe; the fixture recipe is nwn5e-ee `tests/data/fixtures/n5_asmtiny.recipe`). Test names, with asserts:
  - `stampAnswers`: `stamp` equals `nwn5e_anim_helper stamp`'s line.
  - `assembleAnswers`: `assemble a_ba` of the fixture gives `ok:true`, `frames === 31`, and a `lthigh_g` orientation node.
  - `errorsKeepServing`: `assemble a_fa` gives `ok:false` with an error containing "no keys", and a following `stamp` still answers.
  - `badLengthRefused`: `assemble a_ba 99999999` gives `ok:false` (over a 16 MB cap), and the process keeps serving.
  - `errorsAreJson`: a recipe whose `source` holds `"` and `\` that fails to read still answers parseable JSON.
  - `clientQueues`: `startServe` with 20 interleaved `assemble` / `stamp` calls resolves each with its own answer.
  - `clientRestarts`: the child killed by PID rejects a pending call, and the next call succeeds.
- [ ] **Step 2: Run** `node anim_helper/serve_check.js`. Expected: FAIL (no `serve` command; `helper.js` missing).
- [ ] **Step 3: Implement** `serve` in `main.cpp`: reuse `Assemble`'s body as `AssembleToJson(...) -> std::string`, and add a `JsonString(const std::string&)` escaper. Implement `helper.js`.
- [ ] **Step 4: Rebuild** (`anim_helper\build_helper.bat`) **and run** the check. Expected: all pass.
- [ ] **Step 5: Deliberate breaks:**
  - the escaper doesn't escape `"`;
  - a failed request ends the loop;
  - the client answers requests out of order (resolves the newest first).
  
  Each must fail a named test.
- [ ] **Step 6: Commit** (nwn5e-tools) `feat: the helper's serve mode and its Node client (#88)`.

### Task 2: One math module and one library reader

**Files:**
- Create: `mocap_bench/public/core/math.js`, `mocap_bench/public/core/library.js`, `mocap_bench/checks/library_check.js`, `mocap_bench/checks/run.js`

**Interfaces:**
- Produces:
  - `MocapMath`: `add sub scale dot cross len norm mid lerp`, `IDENTITY`, `qmul qconj qnorm qmix(a,b,f)` (the sign-flipped nlerp), `qangle(a,b)` (radians), `rotate(q,v)`, `fromTo(a,b)`, `fromBasis(x,y,z)`, `axisAngle(axis,rad)`, `smoothstep(x)`. Each is carried over from `recipe_bench/public/mocap.js` (lines 78-141) and `core.js` (`mix`, `sample`), with the same results.
  - `MocapLibrary`:
    - `readLibrary(ArrayBuffer) -> { name, supermodel, bones:[{name,parent,position?,orientation?}], clips: Map }` and `readClip(lib, name)`, both carried from `core.js`;
    - `sample(track, t, quaternion)`;
    - `findClip(libs: Map, body, name) -> clip|null`: up the chain, at most 8 deep, names in lower case;
    - `chainNames(libs, body) -> string[]`;
    - `worldPose(bones, clip|null, t) -> [{p:[x,y,z], q:[x,y,z,w]}]`: the one forward kinematics; a bone without a key in the clip takes its rest;
    - `STANCES`: `[name, label]` pairs, `recipe_bench/public/app.js:150-165` carried.
  - `checks/run.js`: runs every `checks/*_check.js` in a child process, prints each one's last line, and exits 1 when any fails.

- [ ] **Step 1: Write `library_check.js`**, run on `anim_helper/cache/libraries`. Asserts:
  - `readsTen`: 10 libraries read; a_ba has 57 bones.
  - `chainFinds`: `findClip(libs,"a_fa","nwstab")` is found through a_ba's chain, and `findClip(libs,"a_ba","no_such")` is null.
  - `fkMatchesOld`: `worldPose` of a_ba `walk` at 0.5 s equals the old `mocap_check.js` `posed()` within 1e-9 for every bone.
  - `mathMatchesOld`: 1,000 random quaternion pairs give `qmix` and `qmul` equal to `mocap.js`'s within 1e-12.
- [ ] **Step 2: Run** `node mocap_bench/checks/library_check.js`. Expected: FAIL (modules missing).
- [ ] **Step 3: Implement** both modules, carrying the code over and tidying it, plus `run.js`.
- [ ] **Step 4: Run** `node mocap_bench/checks/run.js`. Expected: library_check passes.
- [ ] **Step 5: Deliberate breaks:** `qmix` without the sign flip; `findClip` without lower-casing. Both caught.
- [ ] **Step 6: Commit** `feat: mocap_bench's one math module and library reader (#88)`.

### Task 3: Take readers, recording files, the check fixture

**Files:**
- Create: `mocap_bench/public/core/take.js`, `mocap_bench/checks/fixture.js`, `mocap_bench/checks/take_check.js`

**Interfaces:**
- Produces:
  - `MocapTake`:
    - `LANDMARKS`, `P`, `COUNT`, `CONNECTIONS`, `point(frame,i)`;
    - `parseCapture(text) -> { format: "mediapipe"|"rtmpose"|"points", frames: Float64Array[], times? }`, `addHand(capture, text, side)`, `timesFrom(text) -> number[]|null`, `frameRateFrom(text)`: all carried from `mocap.js`;
    - `pickRecordingFiles(relativePaths: string[]) -> { body, left, right, stamps, json }`: `recipe_bench/public/app.js` `sortRecordingFiles`'s rules;
    - `readTake(files: {body, left?, right?, stamps?, json?} as texts) -> { capture, clock: { source: "stamps"|"json"|"points"|"guess", note } }`. A stamps file whose row count differs from the body's frames is not used: `clock.source = "guess"`, and the note names both counts.
  - `checks/fixture.js`: carried from `recipe_bench/mocap_check.js` (`landmarksFrom`, `makeRtm`, `madeTake`).
    - `makeRecording(libsDir, { body, clip, rate: 15, jitter: 0.25, spikes: [], gaps: [], swaps: [], standLead: 0.6 }, outDir) -> { dir, truth }`: writes a FreeMoCap-shaped folder (`output_data/rtmpose_body_3d_xyz.csv` and both hand files, `synchronized_videos/timestamps/<name>_timestamps.csv`) into `outDir` (a temp folder, never in the repo).
    - `spikes: [{frame, point, metres}]`, `gaps: [{from, frames, point}]`, `swaps: [{from, frames, group: "legs"|"arms"}]` inject faults at raw frames.

- [ ] **Step 1: Write `take_check.js`**. Asserts:
  - `readsFixture`: the fixture of a_ba `walk` read through `pickRecordingFiles` + `readTake`: 33 points, `clock.source === "stamps"`, and the frame count equals the fixture's.
  - `stampsMismatchReported`: a stamps file with one row cut gives `clock.source === "guess"`, and the note contains both counts. *(Review Focus 1.)*
  - `picksLikeOldBench`: a list holding the per-camera `*.camera6c44.timestamps.csv` and the take's `<name>_timestamps.csv` picks the take's.
  - `oldReadersSame`: `parseCapture` on the owner's take (when `C:\Users\Bruno\freemocap_data\recordings\2026-10-29_21-20-50_GMT` exists; skipped with a note otherwise) equals `mocap.js` `parseCapture` frame for frame.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement** `take.js` and `fixture.js`.
- [ ] **Step 4: Run** `checks/run.js`. Expected: pass.
- [ ] **Step 5: Deliberate breaks:** the mismatched stamps used anyway; the per-camera stamps file picked. Both caught.
- [ ] **Step 6: Commit** `feat: mocap_bench's take readers and the check fixture (#88)`.

### Task 4: Clean-up v2, part 1: the clock, repair, outliers, gaps, swaps

**Files:**
- Create: `mocap_bench/public/core/cleanup.js`, `mocap_bench/checks/cleanup_check.js`

**Interfaces:**
- Produces, in `MocapCleanup`:
  - `CLEANUP_VERSION = 2`.
  - `hampel(samples: Float64Array[] /*one point's xyz per raw frame*/, metre) -> { dropped: number[] }`, on **raw** frames (before resampling):
    - window ±3 samples;
    - residual = the distance from the window's per-axis median;
    - a sample is an outlier when its residual is over `max(3 × 1.4826 × MAD of the window's residuals, 0.03 m)`;
    - drop only runs of 1-2 outlier samples whose neighbours on both sides are not outliers (a spike that snaps back).
  - `fillGaps(frames, metre, maxGap = 8) -> { filled: number, held: [{point, from, to}] }`: NaN runs of at most 8 raw frames get a cubic Hermite (Catmull-Rom tangents from the two good samples on each side; linear when fewer). Longer runs, and runs at either end, hold the nearest good value and are listed.
  - `fixSwaps(frames) -> number`, per side group:
    - legs: knee, ankle, heel, foot_index;
    - arms: elbow, wrist, pinky, index, thumb;
    - relabel the group at frame f when the crossed cost (L to the previous R, plus R to the previous L) is under 0.6 × the uncrossed cost, and the uncrossed cost is over 0.05 m per point;
    - frames are compared with the corrected previous one;
    - returns the frames relabelled.
  - Carried over unchanged from `mocap.js`: `resample(frames, times, fps)`, `dropStrayHands`, `standIns`, `dropImpossible`, `fillFeetFromAnkles`.

- [ ] **Step 1: Write `cleanup_check.js` (part 1)** on synthetic tracks: a 30 fps sine plus noise σ 5 mm, and fixtures. Asserts:
  - `spikeDropped`: a single raw-frame jump of 0.3 m on `left_wrist` is dropped and refilled within 5 mm of the truth.
  - `strikeKept`: a wrist excursion of 0.5 m over 4 raw frames, held 3, returning over 4, has 0 samples dropped.
  - `noiseKept`: σ 5 mm noise alone drops nothing.
  - `shortGapCubic`: a 5-frame gap in a 1 Hz sine is filled within 2 mm.
  - `longGapHeld`: a 12-frame gap is held at the last good value and listed in `held` with its frames.
  - `edgeGapHeld`: a gap at frame 0 holds the first good value.
  - `swapFixed`: legs swapped for raw frames 10-12 are relabelled (count 3) and match the truth within 1 mm.
  - `crossingNotSwapped`: feet passing each other slowly over 20 frames give count 0.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement** the functions above.
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - drop runs of any length (catches `strikeKept`);
  - linear fill (catches `shortGapCubic`);
  - swap threshold 1.0 instead of 0.6 (catches `crossingNotSwapped`);
  - no comparison with the corrected previous frame (catches `swapFixed`).
- [ ] **Step 6: Commit** `feat: clean-up v2 - outliers that snap back, cubic gaps, held long gaps, left/right swaps (#88)`.

### Task 5: Clean-up v2, part 2: filtering, axes, the floor, the take

**Files:**
- Modify: `mocap_bench/public/core/cleanup.js`, `mocap_bench/checks/cleanup_check.js`

**Interfaces:**
- Produces:
  - `butterworth2(values: Float64Array, fc, fs) -> Float64Array`: 2nd-order low-pass by the bilinear transform with prewarping, run forward and backward (zero phase), with 12 samples of reflected padding at each end.
  - `cutoffs(smoothing: 0..1, realRate) -> { trunk, limbs, ends }`, in Hz:
    - trunk 2→5 Hz, limbs 3→7 Hz, ends 4→9 Hz, linear in `smoothing` (0 is steady, 1 is sharp);
    - each capped at `0.25 × realRate`.
  - Groups:
    - trunk: shoulders, hips, ears, eyes, nose, mouth;
    - limbs: elbows, knees;
    - ends: wrists, pinky, index, thumb, ankles, heels, foot_index.
  - `strikePeaks(frames, fps) -> [{frame, side, limb: "arm"|"leg"}]`: a local maximum of wrist or ankle speed of at least 2.0 m/s and at least 1.5× the median speed over ±0.5 s.
  - `filterTake(frames, fps, realRate, smoothing) -> { frames, cutoffs }`:
    - filters each group at its cutoff;
    - around each strike peak (±3 frames), blends that limb's points (arm: elbow, wrist, pinky, index, thumb; leg: knee, ankle, heel, foot_index) toward a light filter at `min(2 × uncapped ends cutoff, 12 Hz)`, weight 1 at the peak and 0 at 3 frames away, linear.
  - `clean(capture, { smoothing = 0.5, standing = null, clock }) -> Take`, in this order:
    1. units (carried over);
    2. stray hands;
    3. Hampel on raw frames;
    4. gaps (raw);
    5. swaps (raw);
    6. resample to 30 fps from the clock's times;
    7. `dropImpossible` with the feet refilled (carried over);
    8. axes from the standing span, else `[0, 0.33]`, carried from `prepare` (floor-up, mirroring);
    9. game space;
    10. `filterTake`;
    11. the floor (the standing part's median low point, else the whole take's 5th percentile).

    The returned `Take` is today's `prepare` shape, plus `report` and `poor`:
    - `{ fps: 30, frames, unit, up, right, forward, mirrored, floor, duration, reference: {start,end,pose}, hipHeight, report, poor }`;
    - `report = { version: 2, realRate, slowest, fastest, rawFrames, outliers, swaps, filled, held: [{point, t0, t1}], floor: "standing"|"whole take", cutoffs, clock }`;
    - `poor = [[t0, t1]]`: the held stretches merged, in take time.

- [ ] **Step 1: Add asserts:**
  - `zeroPhase`: a 1 Hz sine at fs 30 through `butterworth2(…, 6, 30)` has its peaks at the same frames and amplitude ≥ 0.98.
  - `stopsHigh`: a 10 Hz sine through `butterworth2(…, 3, 30)` comes out at amplitude ≤ 0.12.
  - `capByRealRate`: `cutoffs(1, 13)` gives every value ≤ 3.25.
  - `smoothingMoves`: `cutoffs(0, 60)` < `cutoffs(0.5, 60)` < `cutoffs(1, 60)` in every group.
  - `strikeSurvives`: a fixture punch peaking at 4 m/s on the wrist, real rate 15, keeps a filtered peak speed of at least 0.85× the input peak.
  - `floorFromWholeTake`: a take with no standing span has `report.floor === "whole take"` and the lowest foot point within 1 cm of 0. *(Review Focus 5.)*
  - `takeShapeLikeToday`: the fixture's `clean` result has every field the old `prepare` result has, with the same types.
  - `axesLikeToday`: on a clean fixture with no faults (`smoothing` 0 and `realRate` 30), `up`, `right` and `forward` match `prepare`'s within 0.5°.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass. If `strikeSurvives` needs other strike numbers, calibrate them, ledger a ruling, and keep the property (the peak survives, the noise is cut).
- [ ] **Step 5: Deliberate breaks:**
  - no backward pass (catches `zeroPhase`);
  - no cap (catches `capByRealRate`);
  - no strike crossfade (catches `strikeSurvives`).
- [ ] **Step 6: Commit** `feat: clean-up v2 - zero-phase Butterworth per joint group, capped by the real rate, strikes kept (#88)`.

### Task 6: Suggestions: spans, contacts, hit, loop end

**Files:**
- Create: `mocap_bench/public/core/suggest.js`, `mocap_bench/checks/suggest_check.js`

**Interfaces:**
- Produces, in `MocapSuggest`:
  - `suggestSpans(take) -> { standing: [t0,t1], kept: [t0,t1] }`: carried from `mocap.js` `suggestTrim` + `motionProfile`.
  - `suggestContacts(take, kept) -> { left_foot: [[t0,t1]], right_foot: [[t0,t1]] }`:
    - per foot, `h` = the lowest of ankle, heel and foot_index; `v` = that point's speed, averaged over 3 frames;
    - a contact begins when `h < 0.05 m` and `v < 0.4 m/s` for 2 frames;
    - it ends when `h > 0.09 m` or `v > 0.9 m/s` (a kick releases the moment the foot speeds up);
    - intervals shorter than 0.15 s are dropped, and gaps shorter than 0.1 s are merged;
    - only inside `kept`.
  - `suggestHit(take, kept) -> number|null`: the frame time of the fastest wrist or ankle inside `kept`.
  - `suggestLoopEnd(solved) -> number`: carried from `retarget`'s loop cut (the closest pose to the first in the last 40%), in take time.

- [ ] **Step 1: Write `suggest_check.js`.** Asserts:
  - `walkContacts`: a synthetic foot planted for frames 0-20, swinging 21-35 and planted 36-60 gives `[[0, 20/30], [36/30, 60/30]]`, each end within 1 frame.
  - `kickReleases`: a foot leaving at 3 m/s while still low releases within 1 frame of the speed-up.
  - `shortDropped`: a 3-frame touch is dropped.
  - `noContactsNoCrash`: a foot that never comes down gives `[]`. *(Review Focus 5.)*
  - `hitOnFixture`: on the fixture of a_ba `nwstab`, the hit lands within 2 frames of that clip's fastest-wrist frame.
  - `spansLikeToday`: `suggestSpans` equals `mocap.js` `suggestTrim` on the fixture.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - no speed release (catches `kickReleases`);
  - no minimum duration (catches `shortDropped`);
  - single thresholds with no hysteresis (`walkContacts` must catch it through chatter at the ends; if not, add `noChatter`: a foot hovering at 0.07 m gives one interval, not many).
- [ ] **Step 6: Commit** `feat: suggested spans, foot contacts with hysteresis, the hit (#88)`.

### Task 7: The solve carried over; acceptance part 1

**Files:**
- Create: `mocap_bench/public/core/solve.js`, `mocap_bench/checks/parity_check.js`, `mocap_bench/checks/compare.js`

**Interfaces:**
- Consumes: `MocapMath`, `MocapLibrary`, `MocapTake`, and the `Take` shape (Task 5).
- Produces, in `MocapSolve`:
  - `rig(bones, stance)`: carried over.
  - `solveTake(take, bones, { stance, kept: [t0,t1], place: "return"|"inplace", sway: true, locks: { left: [[t0,t1]], right: [[t0,t1]] }, cache }) -> Solved`.
    - It carries over `mocap.js` `retarget` lines 1228-1379: the reference, the knee rule, the legs' neutral, the solve per frame, the root (`rootPosition`, `placeGround`) and the foot locks (`legIK`, with the in and out ramps).
    - It leaves out halves, poses, balance, blends, loop, fit and reduction.
    - `Solved = { fps, start, count, driven: number[], rootIndex, local: Map<number,quat>[], root: vec3[], locks: [{side, t0, t1, short}] }`.
  - `stanceFrame(stance, bones) -> { S: quat[] /*per bone*/, home: vec3 }`: phase 1's rule (the stance's key at 0, else the library's rest), identical to `data::AssembleAnimClip`'s.
  - `toTracks(solved, bones, frame) -> [{bone, position, times, values}]`:
    - each driven bone's `conj(S) * L`, and the root's `P - home`;
    - times relative to `kept[0]`;
    - reduced with `reduceTrack` (carried over) at the fixed tolerances;
    - a locked foot's thigh, shin and foot at a third, and the root at a third when any lock exists.
  - `compare.js`: `compareClips(assembledJson, keysClip) -> { turn: {deg, bone, t}, root: {mm, t}, problems: string[] }`, moved out of `recipe_bench/compare_assembled.js`.

- [ ] **Step 1: Write `parity_check.js`.** The reference is today's pipeline: `mocap.js` `prepare` + `suggestPlants` + `retarget`, loop off. For every body:
  - `solveMatchesRetarget`: `toTracks(solveTake(...))` on the *same* prepared take and plants, with the deltas recomposed as `S * delta`, equals `retarget`'s pre-blend nodes (run with blends 0) key for key within 1e-6. Same code carried over, so the match is near exact.
  - `roundTripOnFixture`: carried from `mocap_check.js` `roundTrip` / `rtmRoundTrip`. An NWN animation made into a recording (Task 3's fixture) and solved back onto the same body lands its joints within that check's limits (a few cm), with knees never behind (`kneesBehind` is 0).

  Task 8 adds `assembledMatchesToday`, acceptance part 1.
- [ ] **Step 2: Run.** Expected: FAIL (`solve.js` missing).
- [ ] **Step 3: Implement** `solve.js` (carried over, tidied, with no behaviour change) and `compare.js`.
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - the knee rule off (`straightenKnees` skipped);
  - a lock's out-ramp removed;
  - `S * L` instead of `conj(S) * L`.

  Each must fail `solveMatchesRetarget` or `roundTripOnFixture`.
- [ ] **Step 6: Commit** `feat: the solve carried over into mocap_bench, matched against today's retarget (#88)`.

### Task 8: The assembled recipe writer

**Files:**
- Create: `mocap_bench/public/core/recipe.js`, `mocap_bench/checks/recipe_check.js`
- Modify: `mocap_bench/checks/parity_check.js` (use `recipe.js`)

**Interfaces:**
- Produces:
  - `MocapRecipe.writeAssembled({ name, stance, length, transtime, blendIn, blendOut, loop, loopBlend, source, keys: { [library]: tracks } }) -> string`: phase 1's layout, with times to 4 decimals, quaternions to 5 and positions to 4.
  - `MocapRecipe.build(project, solvedByBody: {[library]: {solved, bones, frame}}) -> string`: length `(count - 1) / fps`, the clip settings from `project.clip`, and `source` from the take and the spans.

- [ ] **Step 1: Write `recipe_check.js`.** Asserts:
  - `readByCore`: a recipe built for the fixture on all 8 bodies is assembled by the helper (`serve`) with `ok:true` on each body.
  - `namesTheStance`: the text contains `kind = assembled` and `stance = 1hreadyr`.
  - `noStanceInFile`: a capture that holds the stance pose exactly gives deltas of identity. Every `o` value in the file is `0 0 0 1` within 1e-5, so the stance's keys are not in it. *(The anti-goal "NWN data inside a recipe meant to ship".)*
  - `nameRule`: names of 17 characters, or with capitals, are refused.
  - `roundTripCore`: the helper's `assemble` of the written recipe matches `S * delta` recomposed in JS within 1e-4.
  - In `parity_check.js`, `assembledMatchesToday` (**acceptance part 1**, "fed today's cleaned frames and plants"): Task 7's tracks, written by `build` with blends 0.1 / 0.3 and assembled by the helper, are within **1.5° and 8 mm** of today's baked keys recipe on all 8 bodies (`compare.js`). Run on the fixture always, and on the owner's take 7.15-9.75 s on 1hreadyr when it is present. Print the worst numbers per body.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run `checks/run.js`.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - absolute `L` written instead of the delta (catches `noStanceInFile` and `assembledMatchesToday`);
  - the stance line dropped (catches `namesTheStance` and `readByCore`).
- [ ] **Step 6: Commit** `feat: mocap_bench writes the assembled recipe (#88)`.

### Task 9: The project model and its history

**Files:**
- Create: `mocap_bench/public/core/project.js`, `mocap_bench/checks/project_check.js`

**Interfaces:**
- Produces, in `MocapProject`:
  - The format, as JSON:
    ```
    { format: "nwn5e-mocap-project", version: 1, name, slot,
      base: { kind: "take", recording, files: {body,left,right,stamps,json}, standing: [t0,t1], kept: [t0,t1] },
      clean: { smoothing: 0.5, version: 2 },
      clip: { stance: "pause1", blendIn: 0.2, blendOut: 0.3, loop: false, loopBlend: 0.25, place: "return", transtime: 0.25 },
      moments: { hit: null, recover: null },
      contacts: { left_foot: [{t0, t1, mode}], right_foot: [...] } }
    ```
    `mode` is `"auto"` (suggested), `"always"` (the owner's own) or `"never"` (muted, still on the lane). Effective locks are the intervals that aren't `never`. "Re-suggest" replaces only the `auto` intervals.

    Phase 2's scope:
    - `base.kind` is `"take"` only; projects from an NWN animation come with phase 6;
    - the clip has no crouch or lean yet (phase 6, when the core does them for assembled recipes);
    - contacts are feet only; hands come with pins in phase 3.

    Later phases add their tracks (targets, poses, time, sources, balance) as optional fields, so a version 1 file stays readable.
  - `SLOTS = { n5_shove: {blendIn: 0.1, blendOut: 0.15, loop: false, place: "return"}, n5_rage: {blendIn: 0.2, blendOut: 0.3, loop: false, place: "return"}, n5_cridle: {blendIn: 0, blendOut: 0, loop: true, place: "inplace"} }`. These are the old bench's slot presets; the fit waits for the time track (phase 5).
  - `newProject({ name, slot, recording, files, stance }) -> project`
  - `validate(project, duration) -> { project, problems: string[] }`, clamping:
    - kept inside `[0, duration]`, at least 2 frames;
    - standing at least 0.2 s inside the take;
    - contacts sorted, non-overlapping, inside the take;
    - moments inside kept, or null;
    - blends at least 0.
  - `serialize(project) -> string` (stable key order, 2-space indent); `parse(text) -> { project, warnings }`.
    - An unknown `format`, or a `version` over 1, throws.
    - `clean.version` other than `CLEANUP_VERSION` adds the warning `"made with clean-up v<n>; this bench's v2 changes the take - check the spans and contacts"`.
  - `effectiveLocks(project) -> { left: [[t0,t1]], right: [[t0,t1]] }`.
  - `muteEdits(project, suggestions) -> project`: the ghost's project, with contacts set to the suggested ones, all `auto`.
  - `createHistory(project, { limit: 200 }) -> { current(), apply(label, fn), begin(label), update(fn), commit(), cancel(), undo(), redo(), canUndo(), canRedo() }`. History holds snapshots (deep copies); `begin` … `commit` is one step however many `update`s fall between; a new `apply` after an `undo` clears the redo list.

- [ ] **Step 1: Write `project_check.js`.** Asserts:
  - `slotPresets`: `newProject({slot: "n5_cridle"})` has `loop:true` and `place:"inplace"`.
  - `validateClamps`: kept `[-1, 999]` on a 10 s take gives `[0, 10]`; kept `[5, 5]` gives at least 2 frames; standing `[3, 3.05]` widens to 0.2 s; NaN is never present. *(Review Focus 4.)*
  - `roundTrip`: `parse(serialize(p))` deep-equals `p`, with no warnings.
  - `versionWarning`: `clean.version: 1` gives the warning.
  - `undoRedo`: after 10 `apply`s, 10 `undo`s deep-equal the start and 10 `redo`s deep-equal the end; a new `apply` after 3 undos makes `canRedo()` false.
  - `dragIsOneStep`: `begin` + 50 `update`s + `commit` is undone by one `undo`.
  - `limit`: 250 applies keep 200 steps.
  - `resuggestKeepsOwn`: replacing the `auto` intervals keeps `always` and `never`.
  - `locksFromModes`: `never` intervals are left out of `effectiveLocks`.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - shallow snapshots (catches `undoRedo`);
  - redo not cleared (catches `undoRedo`);
  - no clamp (catches `validateClamps`).
- [ ] **Step 6: Commit** `feat: the mocap project model with undo and redo (#88)`.

### Task 10: The pipeline, stage by stage

**Files:**
- Create: `mocap_bench/public/core/pipeline.js`, `mocap_bench/checks/pipeline_check.js`

**Interfaces:**
- Consumes: Tasks 3-9.
- Produces: `MocapPipeline.createPipeline({ loadFiles(project) -> Promise<texts>, libraries: Map, assemble(library, recipeText) -> Promise<json> }) -> { run(project, body, { ghost }) -> Promise<Result>, forget() }`.
  - `Result = { take, report, suggestions: {spans, contacts, hit}, solved, recipe, clip /*the helper's JSON*/, ghost?: {solved, clip}, warnings }`.
  - Each stage is memoized on the inputs it reads:
    - files: `base.recording`, `base.files`;
    - clean: + `clean.smoothing`, `base.standing`;
    - suggest: + `base.kept`;
    - solve: + body, `clip.stance`, `base.kept`, `clip.place`, `effectiveLocks`;
    - recipe and assemble: + `clip.*`.
  - A project with no contacts yet gets the suggested ones from the first run (returned in `suggestions`; the caller applies them as one undoable step).

- [ ] **Step 1: Write `pipeline_check.js`** with a counting `assemble` stub and the real helper. Asserts:
  - `cachesSolve`: two runs of one project solve once.
  - `blendOnlyReassembles`: changing `clip.blendIn` re-runs recipe and assemble, not solve or clean.
  - `contactsResolve`: muting a contact re-runs solve, not clean.
  - `smoothingRecleans`: changing `clean.smoothing` re-runs clean.
  - `ghostUnedited`: `ghost` solves with the suggested contacts; it differs from the run where a contact is set to `never`, and equals the result when none is.
  - `endToEnd`: on the fixture, `run` gives a `clip` whose `frames` match the kept span's length at 30 a second.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:**
  - the solve key leaves out `effectiveLocks` (catches `contactsResolve`);
  - the clean key leaves out `smoothing` (catches `smoothingRecleans`).
- [ ] **Step 6: Commit** `feat: the mocap pipeline, re-running only what an edit touches (#88)`.

### Task 11: The server

**Files:**
- Create: `mocap_bench/server.js`, `mocap_bench/mocap_bench.bat`, `mocap_bench/checks/server_check.js`

**Interfaces:**
- Consumes: `anim_helper/helper.js` (Task 1), `MocapProject` / `MocapRecipe` validation (names).
- Produces, at `node mocap_bench/server.js [--port 8767] [--build <dir>] [--game <dir>] [--helper <exe>] [--recordings <dir>] [--no-open]` (the recordings default is `%USERPROFILE%\freemocap_data\recordings`):
  - `GET /` and the static files under `public/`.
  - `GET /api/state` → `{ stamps: {helper, dll, match}, libraries: [names], libraryError, recordingsDir, buildDir, projects: [names] }`.
  - `GET /api/recordings` → `[{ name, modified, files: {body,left,right,stamps,json} }]`, newest first; only folders whose body file is found.
  - `GET /api/recording/file?name=<folder>&kind=body|left|right|stamps|json` → the text. `name` must be a direct child of the recordings folder (no `..`, no separators), or 400.
  - `GET /api/library/<name>.mdl` → the bytes from `anim_helper/cache/libraries`.
  - `GET /api/project/<name>` → the JSON, 404 when missing; `POST /api/project` (body: the project) → validates the name, writes atomically (temp file, then rename).
  - `POST /api/assemble` `{library, recipe}` → the helper's JSON.
  - `POST /api/save` `{project, recipe}`:
    - 409 with `"the helper and the game's DLL are different builds: rebuild anim_helper (build_helper.bat) or the DLL"` on a stamp mismatch;
    - 409 with `"no core_stamp.txt beside the DLL in <build>: build nwn5e first"` when the DLL's stamp is missing;
    - otherwise assembles the recipe for each of the 8 bodies (refused with the helper's error if any fails) and writes the project and recipe atomically;
    - → `{ projectPath, recipePath, replaced, bodies: [{library, ok}] }`.
  - Every request checks `Host`; every POST checks `Origin`.

- [ ] **Step 1: Write `server_check.js`.** It starts the server on port 8790 with a temp build folder (holding a `core_stamp.txt` equal to the helper's), a temp recordings folder (Task 3's fixture) and `--no-open`. Asserts:
  - `hostChecked`: `Host: evil.example` gets 403.
  - `originChecked`: a POST with `Origin: http://evil.example` gets 403.
  - `listsRecordings`: the fixture is listed with its body file.
  - `traversalRefused`: `name=..%5C..%5Cx` gets 400.
  - `projectRoundTrip`: POST then GET returns the same.
  - `saveWrites`: both files are written; the recipe assembles on 8 bodies.
  - `saveRefusedOnStampMismatch`: a `core_stamp.txt` of `deadbeef` gives 409, and no file is written. *(Review Focus 3.)*
  - `saveRefusedWithoutDllStamp`: no `core_stamp.txt` gives 409, and no file is written. *(Review Focus 3.)*
  - `missingRecordingKeepsProject`: a saved project whose recording folder was then renamed is still returned by `GET /api/project`, and `GET /api/recording/file` gives 404 with the folder's name. *(Review Focus 2.)*
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement** `server.js` (patterned on `recipe_bench/server.js`: static files, `send`, the Host and Origin checks, opening the browser) and the `.bat` (`node "%~dp0server.js" %*`).
- [ ] **Step 4: Run `checks/run.js`.** Expected: all pass. The server and its helper are killed by PID at the check's end.
- [ ] **Step 5: Deliberate breaks:**
  - no Host check;
  - no stamp check on save;
  - the traversal check removed.

  Each caught.
- [ ] **Step 6: Commit** `feat: the mocap bench's server - recordings, projects, the helper, Save (#88)`.

## Part B — the page

### Task 12: The stage

**Files:**
- Create: `mocap_bench/public/stage.js`, `mocap_bench/checks/stage_check.js`

**Interfaces:**
- Produces: `MocapStage.create(canvas) -> { setFigures({ result: {bones, pose}, ghost?: {bones, pose}, points?: vec3[], locks?: [{at: vec3}] }), setView(preset|{yaw,pitch,distance,target}), draw(), PRESETS }`.
  - Renderer carried over from `recipe_bench/public/app.js:985-1155`: the orbit camera, depth-sorted sticks and joints, the head, shadows, rings under locked feet, drag to orbit, wheel to zoom. `pose` is `MocapLibrary.worldPose` output.
  - The ghost is drawn translucent (alpha 0.35) in the side colour, at the same spot, under the result.
  - `PRESETS`, `[yaw, pitch, distance]` in radians and metres:
    - `front` `[0, 0.12, 7.2]`;
    - `side` `[π/2, 0.12, 7.2]`;
    - `three4` `[π/4, 0.2, 7.2]`;
    - `stance` (the stance's first frame, from the front) `[0, 0.12, 6]`;
    - `game` `[π/4, 0.85, 9]`.

    Keys 1-5, in that order.
  - `pointsOnDoll(capturePoints: vec3[33], dollRest: {name → vec3 world}, dollPose: {name → vec3 world}) -> vec3[33]`: the capture rebuilt on the doll's proportions, rooted at the posed hips' midpoint. Each edge keeps the capture's direction and takes the doll's rest length:

    | Edge (capture) | Doll length (rest, between bone origins) |
    |---|---|
    | hips mid → hip | half of lthigh_g ↔ rthigh_g |
    | hip → knee | thigh_g → shin_g |
    | knee → ankle | shin_g → foot_g |
    | ankle → heel, foot_index | capture length × (doll leg ÷ capture leg) |
    | hips mid → shoulders mid | thigh mid → bicep mid |
    | shoulders mid → shoulder | half of lbicep_g ↔ rbicep_g |
    | shoulder → elbow | bicep_g → forearm_g |
    | elbow → wrist | forearm_g → hand_g |
    | wrist → index, pinky, thumb | capture length × (doll arm ÷ capture arm) |
    | shoulders mid → nose, ears, eyes, mouth | capture length × (doll neck_g → head_g ÷ capture shoulders-mid → ears-mid) |

- [ ] **Step 1: Write `stage_check.js`.** Pure functions, plus a recording fake 2D context. Asserts:
  - `pointsIdentity`: a capture made from a_ba's own rest joints maps onto them within 1 mm.
  - `pointsScaleFree`: that capture scaled ×1.25 about the hips still maps within 1 mm.
  - `pointsFollowDirection`: a forearm turned 90° in the capture comes out at 90°, with the doll's forearm length.
  - `presetsKeys`: the five presets in the order of keys 1-5.
  - `drawsGhostUnder`: with a ghost, the fake context records the ghost's strokes at alpha 0.35, before the result's sticks are painted over them.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:** the capture's own lengths kept (catches `pointsScaleFree`); the ghost drawn over the result (catches `drawsGhostUnder`).
- [ ] **Step 6: Commit** `feat: the mocap bench's stage - the ghost, the capture on the doll's proportions, camera presets (#88)`.

### Task 13: The timeline

**Files:**
- Create: `mocap_bench/public/timeline.js`, `mocap_bench/checks/timeline_check.js`

**Interfaces:**
- Produces: `MocapTimeline.create(canvas, { onEdit(label, fn, phase /*"begin"|"update"|"commit"*/), onSeek(t), onSelect(sel) }) -> { setModel({ project, duration, fps, poor, speed: {t[], v[]}, markerT, playing }), draw() }`, plus these pure functions, exported for checks:
  - `layout(width, height, project, duration) -> { rows: [{id, y, h}], overview: {t0, t1}, ruler: {t0, t1} }`:
    - rows, top to bottom: `overview`, `ruler`, `time`, `moments`, `contacts.left_foot`, `contacts.right_foot`;
    - the ruler spans the kept part ± 0.25 s; the overview spans the whole take.
  - `timeToX(layout, row, t)` and `xToTime(layout, row, x)`: inverses of each other.
  - `snap(t, fps, markerT, pxPerS) -> t`: the marker within 6 px, else the nearest whole frame.
  - `hitTest(layout, project, x, y) -> sel`: `{kind: "span", which: "kept"|"standing", part: "start"|"end"|"body"}`, `{kind: "contact", limb, index, part}`, `{kind: "moment", which}`, `{kind: "ruler"}` or null. Handles are 5 px wide.
  - `dragTo(project, sel, t, fps, duration) -> project`, clamped:
    - kept at least 2 frames;
    - standing at least 0.2 s;
    - inside the take;
    - a contact's end stops at its neighbour, at least 2 frames long;
    - moments inside kept.

  What the rows draw:
  - **Overview:** the hands' and feet's speed curve, the standing and kept spans (shaded), and poor stretches in red.
  - **Ruler:** take seconds, and the marker.
  - **Time:** animation seconds ticked (in phase 2, animation t = take t − kept[0]).
  - **Moments:** the hit and recover handles.
  - **Contact lanes:** `auto` filled, `always` filled with an outline, `never` hollow.

  Double-clicking an empty lane adds an `always` interval of 0.3 s, snapped.
- [ ] **Step 1: Write `timeline_check.js`.** Asserts:
  - `inverse`: `xToTime(timeToX(t)) === t` within 1e-9.
  - `snapMarker`: within 5 px snaps to the marker; at 7 px it doesn't.
  - `snapFrames`: times land on k/30.
  - `hitTestFinds`: each kind is found at its drawn place.
  - `dragClamps`: kept start dragged past its end stops 2 frames before it; past 0, at 0; standing kept at 0.2 s; a contact's end stops at its neighbour. *(Review Focus 4.)*
  - `rowsOnlyWhenUsed`: the `moments` row is absent when both moments are null.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement** the pure functions, then the drawing and pointer handling: pointerdown → `onEdit(label, fn, "begin")`, move → `"update"`, up → `"commit"`.
- [ ] **Step 4: Run.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:** no clamp in `dragTo`; snapping to frames with `floor` instead of `round`. Both caught.
- [ ] **Step 6: Commit** `feat: the mocap bench's timeline - overview, spans, moments, contact lanes (#88)`.

### Task 14: The page: header, inspector, transport, keys

**Files:**
- Create: `mocap_bench/public/index.html`, `style.css`, `inspector.js`, `app.js`

**Interfaces:**
- Consumes: everything above; the server API (Task 11).
- Produces:
  - **Header:** project select (open), New (recording list → name or slot → creates the project), body select (8 bodies), stance readout, Save, status line.
  - **New project:**
    - clean with no standing span, suggest the spans, re-clean with the suggested standing span, then suggest contacts and the hit;
    - one history step labelled "new project".
  - **Inspector** (`MocapInspector.render(sel, project, ctx) -> void`), the selection's settings only, one help line each:
    - the take: smoothing (a range labelled steady ↔ sharp), Re-suggest (spans, `auto` contacts, hit), and the report lines (real rate, lost stretches, floor, standing part);
    - the clip: stance (the `STANCES` list, then every animation, as the old bench), blend in, blend out, loop (on: "cut at the closest pose" sets kept end from `suggestLoopEnd`), loop blend, place (returns / in place), transtime;
    - a contact: mode (auto / always / never), start and end (numbers in seconds);
    - a moment: its time, and clear.
  - **Transport:** play / pause, step, readout `animation 0.40 s · frame 12 · take 8.31 s`. Playback runs in animation time and moves the marker in take time.
  - **Keys:**
    - Space: play / pause;
    - ← →: a frame;
    - Delete: delete the selected contact interval or moment;
    - Ctrl+Z / Ctrl+Y: undo / redo;
    - 1-5: presets.

    Keys are ignored while an input has focus.
  - **Toggles:** ghost and points (stage overlay).
  - **Open:** a project's `parse` warnings show in the status line. A missing recording shows "recording <name> not found in <dir> - the project is kept as it was", and the stage stays empty (Review Focus 2).
  - **Save:** runs the pipeline for all eight bodies, then POSTs `/api/save`. The status shows:
    - the paths, and "saved over" when it replaced;
    - any body that failed;
    - per body, any foot lock out of the leg's reach (`short` over 3 cm), as today's readout.

    The foot-slide readout comes with pins (phase 3).
  - **Not in this phase (rulings):**
    - dropping a take's files on the page: a project must re-read its take from the server, so only the recordings list opens a take;
    - picking a bone on the stage: it serves dragging, which is phases 3-4.
- [ ] **Step 1: The browser check (Task 15) is this task's test.** Write the page so the browser check's steps can run. This task's own verification is `node mocap_bench/checks/run.js` (all still pass) plus a manual load in headless Chrome with no console errors (`--dump-dom` of `/` shows the header).
- [ ] **Step 2: Implement** the files.
- [ ] **Step 3: Run** `checks/run.js`, then headless Chrome `--dump-dom http://127.0.0.1:8790/` against a check server. Expected: no errors; the header is present.
- [ ] **Step 4: Commit** `feat: the mocap bench's page - header, inspector, transport, keys (#88)`.

### Task 15: The browser check, end to end

**Files:**
- Create: `mocap_bench/browser_check.mjs`, `mocap_bench/checks/cdp.mjs`

**Interfaces:**
- Produces:
  - `cdp.mjs`:
    - `launch({ chrome, profileDir, width: 1400, height: 900 }) -> { pid, ws }`: Chrome headless with `--remote-debugging-port=0` and its own temp profile; the port is read from `DevToolsActivePort`;
    - `send(method, params)`, `evaluate(expr)`, `mouse(type, x, y)`, `key(type, key, modifiers)`, `screenshot() -> PNG bytes`, `close()` (kills its own PID only).
  - `browser_check.mjs`: starts a check server (as Task 11), drives the page, kills both by PID, and exits 1 on any failure.

- [ ] **Step 1: Write the steps**, each a named assertion:
  1. `loads`: `/api/state` shows the stamps match; no console errors.
  2. `newProject`: New → the fixture recording → name `n5_bchk`. The report shows the real rate; the overview has a kept span; both contact lanes have intervals.
  3. `dragKept`: mouse-drag the kept span's start 40 px right; `project.base.kept[0]` grows.
  4. `undo`: Ctrl+Z restores it; Ctrl+Y redoes it.
  5. `contactNever`: click a contact interval, set never in the inspector; the result's left-foot keys change; ghost on still shows the old ones.
  6. `play`: Space; after 300 ms the readout's animation time is above 0; Space stops it.
  7. `step`: → moves one frame.
  8. `presets`: key 2 sets the view to `side`.
  9. `points`: the points toggle draws the capture (the screenshot's pixels differ from the toggle off).
  10. `save`: Save; the project and recipe files exist in the temp build; the recipe assembles on 8 bodies.
  11. `reopen`: reload, open `n5_bchk`; the spans, contacts and clip equal what was saved.
  12. `missingRecording`: rename the fixture folder and reopen; the status names the folder, and the project is unchanged on disk. *(Review Focus 2.)*
- [ ] **Step 2: Run** `node mocap_bench/browser_check.mjs`. Expected: steps fail until the page does each. Fix the page (Task 14's code) until all pass.
- [ ] **Step 3: Deliberate breaks:**
  - Ctrl+Z not bound;
  - the ghost drawn from the edited project;
  - Save posting without `Origin` (the page's `fetch` from another port).

  Each must fail its step.
- [ ] **Step 4: Commit** `feat: the mocap bench's browser check (#88)`.

### Task 16: Docs, hand-over

**Files:**
- Create: `mocap_bench/README.md` (start, the workflow, keys, the checks; capture habits from the spec's clean-up section; the unfiltered-points finding below)
- Modify: nwn5e-tools `README.md` (the tools table: `mocap_bench/`, port 8767); nwn5e-ee `docs/BENCH-REDESIGN.md` ("port 8766" → 8767; phase 2 status), `docs/TOOLS-PLAN.md` (the phase 2 entry: what was built, the parity numbers, the clean-up v2 numbers, rulings)

- [ ] **Step 1: The unfiltered-points finding** (the spec's builder check): FreeMoCap's `output_data` on this install holds only processed points. `freemocap_data_by_frame.csv` has the trajectories `3d_xyz` and `rigid_3d_xyz` and nothing raw, so the bench filters already-filtered points. The README says so, and that skipping FreeMoCap's own filter in its processing settings (if its version offers it) lets the bench filter at the true rate.
- [ ] **Step 2: Run everything:**
  - `node anim_helper/serve_check.js`
  - `node mocap_bench/checks/run.js`
  - `node mocap_bench/browser_check.mjs`

  Expected: all pass. Print the parity numbers.
- [ ] **Step 3: Commit:**
  - nwn5e-tools: `docs: the mocap bench's README (#88)`;
  - nwn5e-ee: `git commit --only docs/BENCH-REDESIGN.md docs/TOOLS-PLAN.md -F -` with `docs: bench redesign phase 2 built (#88)`.
- [ ] **Step 4: Hand over on #88:** a comment (what was done, files, the parity numbers, caveats), the `needs-test` label, the board to Needs test. Print this matrix in chat:

  | # | Do | Expect |
  |---|----|--------|
  | M1 | Start `nwn5e-tools\mocap_bench\mocap_bench.bat` | The page opens on 127.0.0.1:8767; the status says the helper and the DLL match |
  | M2 | New → 2026-10-29_21-20-50_GMT → name `n5_asm2` | The take's report (real rate, lost stretches, floor, standing); spans on the overview; red where the capture was poor |
  | M3 | Look at the contact lanes and the hit; set a foot contact to never | The legs change; the ghost shows them as they were |
  | M4 | Points on; keys 1-5 | The capture on the doll's proportions; the five views |
  | M5 | Drag spans; Ctrl+Z / Ctrl+Y | Undo and redo step through the edits |
  | M6 | Save; restart the game; `!anim n5_asm2` | It plays as the bench showed |
  | M7 | The same take in the old bench (8765) beside the new | Clean-up v2's result looks as good or better (your call) |
  | M8 | Close the bench, start it again, open `n5_asm2` | Everything as saved |
