# Mocap Bench: Whole-Pose Keys and Simplify to Main Poses (#116) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In the mocap bench, key the whole body at a moment as one key (Shift+K), and turn a clip into a few such whole poses with flowing motion between them ("simplify to main poses", one detail slider). Every whole pose can be moved, deleted and adjusted.

**Architecture:**
- **One kind of key.** A whole pose is the parts' ordinary pose keys sharing an id (`whole`) and a time. Rebasing (#101), versions (#117), validation, the recipe and the core all treat them as the keys they already handle.
- **Flowing motion.** Whole-pose keys flow from one to the next: a part keeps moving through a key unless it turns back or holds there. A part's own keys still stop, as today. The bench bakes that motion into the recipe as frame-by-frame pose lines at the clip's own frame times, so the game's core plays it exactly, unchanged.
- **Simplify** is a pure module. It fits whole-pose keys to the motion the doll shows now.

**Tech Stack:** plain browser JS (UMD modules in `mocap_bench/public/core/`), Node checks (`mocap_bench/checks/*_check.js`, the helper `anim_helper/bin/nwn5e_anim_helper.exe` = the game's core), the browser check (`mocap_bench/browser_check.mjs`, Chrome over CDP).

**Spec:** issue #116 (https://github.com/BrunoTCarreiro/nwn5e-ee/issues/116): its body (holds, main poses, a detail slider, editable keys) and the decision comments of 2026-11-02: "116 after 112, before 113", then the scope widened to whole-pose keys ("yes to both, go with your recommendations"). The owner approved this description:
- Shift+K keys every part at the marker, exactly as the doll stands, as one key on a single row.
- It moves, deletes, and takes its mode (push or replace) and ease as one unit.
- Turning one part at that moment adjusts that part inside it.
- Replace means pose to pose, the capture ignored between. Push keeps the capture underneath.
- A leg whose foot is planted by a contact stays with the contact.
- Simplify suggests whole-pose keys at the main poses, with a detail slider.

## Global Constraints

- **Scope:** bench only (nwn5e-tools `mocap_bench/`).
  - No change to the game's core, its recipe format or the install. The recipe's `pose` lines keep today's form: `pose <part> <take t> <x y z> <push|replace> <ease>`.
  - A project with no whole-pose keys builds a byte-identical recipe and plays as before.
- **General rules only** (the owner, 2026-11-02): "any "fix" that's done to these animations needs to be based on rules that are extensible to other animations". Every rule here applies to every clip and every part. No per-clip numbers, no flip points.
- **Edits:** in take time; one undo step per user action.
- **Words on the page:** "whole pose", "simplify to main poses", "detail". Never "group".
- **Commits:** nwn5e-tools, `git commit --only <paths> -F -` (a new file `git add <path>` first). One git command per call, never `git add -A`. Messages end with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
- **Checks:** `node mocap_bench/checks/run.js` (every Node check) and `node mocap_bench/browser_check.mjs`. Every new rule gets a deliberate break (a script reverting it) that a check catches.

## Rulings in this plan (the owner may veto any)

- **R1 Flow.** Whole-pose keys flow; a part's own keys stop at each key (today).
  - Flow is a cubic through the keys' turns as rotation vectors, monotone per component (Fritsch-Carlson). Its tangent is 0 where a part turns back or holds, and at a whole pose with a non-whole neighbour or one in the other mode.
  - Why: keys shared by every part at one moment would otherwise stop every part there. That is a stutter, and it lowers quality.
  - Cost if wrong: one rule in `pose.js` (`bake`).
- **R2 Legs.** A whole pose keys a leg (thigh, shin, foot) only while its foot is off the ground: outside the project's effective locks, within half a frame.
  - Around a whole pose's leg keys, the bake adds release keys at the edges of the foot's plants. These are push keys turning 0°, so during a plant the leg is the capture, exactly.
  - A whole pose's leg key that lands inside a plant (contacts edited later) is left out of the bake.
- **R3 Mode.** Whole poses are made in replace (pose to pose); push is in the inspector.
- **R4 Simplify** keys what the doll shows now: the parts' step-4 turns on the shown body, the IK and the grip after. It replaces every key inside the clip, except a planted leg's own keys. Pressed again, or the slider moved, right after, it replaces its own result (one step).
- **R5 Editing.** The whole-pose row moves the whole pose. A part's own row edits that part only: moving its key in time takes it out of the whole pose (overlap, offsetting a part).
- **R6 The hips' place.** Not keyed: pose keys turn parts (as today).

## Review Focus

1. **An older project** (n5_rage's file, no `whole` anywhere): its recipe text is identical to before, and it opens without problems.
2. **Contacts edited after whole poses exist:** a planted leg stays exactly on the capture during every plant. It never jumps where a release key meets an own key.
3. **A whole pose with mixed modes,** or a part's own key between two whole poses: no jump, no flow across the mode change, no NaN.
4. **Undo and redo** across Shift+K, a whole-pose drag, a part-row detach, simplify and simplify-again: the selection survives or falls back to nothing, with no page errors.
5. **A version shown (#117)** when Shift+K or simplify is pressed: refused with the version's word. Each version rebases whole poses keeping their id and shared time.

---

### Task 1: Whole-pose keys in the model, flowing, in the recipe

**Files:**
- Modify: `mocap_bench/public/core/pose.js`, `mocap_bench/public/core/project.js`, `mocap_bench/public/core/recipe.js`, `mocap_bench/public/index.html` (project.js before recipe.js)
- Test: `mocap_bench/checks/pose_check.js`, `mocap_bench/checks/project_check.js`, `mocap_bench/checks/recipe_check.js`

**Interfaces (Produces):**
- `PO.LEG_SIDE`: `{ lthigh_g: "left", lshin_g: "left", lfoot_g: "left", rthigh_g: "right", rshin_g: "right", rfoot_g: "right" }`.
- `PO.planted(locks, side, T) -> bool`: `locks` as `PR.effectiveLocks` (`{ left: [[t0, t1, h]], right }`). True within `HALF_FRAME` of a lock.
- `PO.clipTimes(start, length) -> number[]`: the core's frame clock. `frames = max(1, round(length * 30))`, `T_f = start + length * f / frames` for f = 0..frames.
- `PO.bake(poses, { times, locks }) -> { part: [{ t, turn, mode, ease }] }`: what the recipe writes and what `posedDelta` reads.
  - Each part's own keys, as they are.
  - Its whole-pose keys (without `whole`), a planted leg's dropped.
  - A leg's release keys (`turn [0,0,0]`, push, ease 0) at the plant edges bounding each whole-pose key's air span, unless the part has a key within half a frame there.
  - Dense keys at the `times` strictly inside every segment where either end has a nonzero tangent: the segment's mode, ease 0, value = the flow.
  - Without whole-pose keys it returns the same keys, so the recipe is identical.
- `PO.wholePoses(poses) -> [{ whole, t, parts: string[], mode: "push"|"replace"|"mixed", ease: number|null }]` (by time; `ease` null when mixed). `PO.nextWhole(poses) -> integer` (max id + 1, else 1).
- Pose keys may carry `whole`: a positive integer.
  - `validate` keeps it. A non-integer is dropped with a problem.
  - A whole pose's key off its pose's time (the time most of its keys share; ties the earliest) loses `whole`, with a problem.
  - `ORDER.poseKey` gains `"whole"`.
- `recipe.build` writes `PO.bake(project.poses, { times: PO.clipTimes(takeStart, length), locks: PR.effectiveLocks(project) })`. The comment line above the pose lines gains: "lines between whole poses are their motion, frame by frame".

- [ ] **Step 1: Write the failing checks**
  - **`pose_check.js`**, through the helper (its `recipe()` and `assembled()`). A torso's three replace whole-pose keys turning one way (0°, 20°, 50° about X at 2.2, 2.5, 2.8):
    - `flowMatchesCore`: every frame 2.2-2.8 of the core's clip equals `S * posedDelta(bake(...).torso_g, T, capture)` within 0.05°.
    - `flowsThroughWholePose`: the torso's speed at 2.5 (frames either side) is over 50% of the mean speed 2.2-2.8. The same three keys without `whole` stop: under 10%.
    - `stopsAtATurn`: keys at 0°, 40°, 10°: the speed at the middle key is under 5% of the mean.
    - `holdStaysFlat`: keys at 30°, 30°, 30°, 60°: no frame between the first and third is more than 0.01° off 30°.
    - `mixedModesStop`: middle key push, ends replace. Flow only where modes match: the speed at the middle key is under 10% of the mean, and no frame is NaN.
  - **`pose_check.js`**, `plantedLegStays` (through the helper). Locks: left foot planted 2.0-2.3 and 2.6-3.0. A whole pose with `lthigh_g` 40° replace at 2.45. The bake has release keys at 2.3 and 2.6. Every frame of 2.0-2.3 and 2.6-3.0 has the left thigh at the capture's turn within 0.01°; at 2.45, 40° within 0.05°.
  - **`pose_check.js`**, `legInAPlantLeftOut`: the same whole pose at 2.1 (inside the plant) bakes no lthigh key.
  - **`project_check.js`:**
    - `wholeKept`: a `whole: 3` key round-trips through validate and serialize.
    - `wholeBadDropped`: `whole: "x"` and `whole: 1.5` are dropped, with problems.
    - `wholeStrayDetached`: two keys of whole 2 at 2.5 and one at 2.6. The 2.6 one loses `whole`, with a problem naming it.
  - **`recipe_check.js`:**
    - `noWholeSameRecipe`: n5_rage's project (`build/animedit/projects/n5_rage.json`'s poses and contacts, if present, else the fixture's own keys) builds the same text through `bake` as the pre-change writer. The writer is a copy of today's poses loop inside the check.
    - `wholeWrittenFrameByFrame`: three whole poses on the torso write their keys plus a line at every clip frame between them.
- [ ] **Step 2: Run them; they fail** — `node mocap_bench/checks/pose_check.js` (and the other two). Expected: FAIL with `PO.bake is not a function` / `whole` dropped by validate.
- [ ] **Step 3: Implement** `planted`, `clipTimes`, `bake`, `wholePoses`, `nextWhole` in pose.js; `whole` in project.js; the bake in recipe.js; the script order in index.html.
  - **Flow (in `bake`):**
    - Per part, `v_k = log(R_k)` (`R_k` with `w >= 0`), where `R_k = turnFromDegrees(turn)`.
    - Tangent `m_k = 0`, unless key k is a whole-pose key whose both neighbours are whole-pose keys in its mode. Then Fritsch-Carlson per component, using the gaps on both sides: 0 where the slopes on either side differ in sign or one is 0, else the weighted harmonic mean.
    - Cubic Hermite in v between two keys; `exp` back. The dense values become `degreesFromTurn(exp(v(T)))`.
- [ ] **Step 4: Run them; they pass.** Then `node mocap_bench/checks/run.js`: every check passes.
- [ ] **Step 5: Breaks.** A script in the job's tmp reverts each rule and re-runs its check, printing CAUGHT or MISSED:
  - the tangent's whole-only rule;
  - the turn-back zero;
  - the release keys;
  - the planted drop;
  - the validate stray rule;
  - the recipe's bake call.

  Every break must be CAUGHT.
- [ ] **Step 6: Commit** — `feat: the mocap bench's whole-pose keys flow from one to the next, baked into the recipe frame by frame (nwn5e-ee #116, 1/4)`.

### Task 2: Simplify to main poses (the module)

**Files:**
- Create: `mocap_bench/public/core/simplify.js` (UMD, `root.MocapSimplify`)
- Modify: `mocap_bench/public/index.html` (after pose.js), `mocap_bench/public/core/project.js` (`clip.detail`)
- Test: `mocap_bench/checks/simplify_check.js`

**Interfaces:**
- Consumes: `PO.bake`, `PO.posedDelta`, `PO.planted`, `PO.LEG_SIDE`, `PO.HALF_FRAME` (Task 1).
- Produces: `SI.simplify({ times, shown, capture, locks, detail }) -> { keys: number[] (frame indices, ascending), values: { part: (quat|null)[] per key }, worst: degrees }`.
  - `shown` and `capture` are `{ part: quat[] per frame }`: the doll's step-4 turns, and the recipe's capture turns.
  - A leg's value is null where its foot is planted.
  - Error is measured on every frame for the upper parts and on airborne frames for the legs. The model is `posedDelta(bake(candidate)[part], T, capture[part][f])` with replace keys.
  - Greedy: keys at the first and last frame. Then, while the worst error > `detail`, add a whole pose at the worst frame. Keys stay at least `SI.MIN_GAP` frames apart (2); the next-worst allowed frame is taken. At most `ceil(frames / 2) + 1` keys.
  - Then the holds: for each part, the longest runs of consecutive keys (a leg's within one air span) all within `detail / 2` of their mean turn, with the shown turns between within `detail`, are set to the mean.
- Produces: `SI.toWholePoses(result, times, firstWhole) -> { part: [{ t, turn, mode: "replace", ease: PO.EASE, whole }] }`.
- Produces: `clip.detail`, degrees, 1..20, default `SI.DETAIL` (Step 5 sets it). It is validated, and older projects get the default. `ORDER.clip` gains `"detail"`.

- [ ] **Step 1: Write the failing checks** (`simplify_check.js`, synthetic, 30 fps, 2 s):
  - `keysAtTurnsAndHolds`: a forearm swinging 0 → 60° (ease, 0.0-0.6 s) → back to 10° (0.6-1.2) → held with 1.5° jitter (1.2-2.0). At detail 5:
    - keys within 2 frames of 0, 0.6, 1.2 and 2.0;
    - no key strictly inside 1.25-1.95;
    - the hold's frames are flat (its keys equal);
    - worst ≤ 5.
  - `errorWithinDetail`: the same plus sine jitter on all 16 parts. At 2, 5 and 10: `worst ≤ detail`.
  - `coarserFewer`: key counts at 2, 5 and 10 strictly decreasing.
  - `legsOnlyAirborne`: the left leg planted 0-0.8 and 1.2-2.0, swinging in between. Its values are null on keys inside the plants and set on keys inside 0.8-1.2. The right leg, planted throughout, is null everywhere.
  - `wholePosesShareTimes`: `toWholePoses` gives every part's key at key i the time `times[keys[i]]` and the id `firstWhole + i`.
  - `detailValidated`: `clip.detail` 0 → 1, 50 → 20, missing → `SI.DETAIL`, each with a problem except "missing".
- [ ] **Step 2: Run** `node mocap_bench/checks/simplify_check.js`. Expected: FAIL (`Cannot find module '../public/core/simplify.js'`).
- [ ] **Step 3: Implement** `simplify.js` and `clip.detail`.
- [ ] **Step 4: Run it, then `node mocap_bench/checks/run.js`.** All pass.
- [ ] **Step 5: Measure on n5_rage** (a scratch script in the job's tmp, not committed: the pipeline in Node on the owner's take, as `pipeline_check.js` builds one).
  - For details 2, 3, 4, 6 and 8: keys per part per second, the worst error, and the share of arm frames at rest (under 3°/s) in the core's clip.
  - Compare with NWN's own clips (the issue's table: 2.5-8.7 keys per bone per second, 21-58% at rest).
  - Set `SI.DETAIL` to the smallest detail landing inside NWN's range. Ledger it as a ruling with the numbers.
- [ ] **Step 6: Breaks:**
  - the min gap;
  - the holds snap;
  - the airborne mask;
  - the stop condition (`<` for `≤`);
  - the clamp of `detail`.

  All must be CAUGHT.
- [ ] **Step 7: Commit** — `feat: the mocap bench simplifies a clip to its main poses - whole poses where its motion turns or holds (nwn5e-ee #116, 2/4)`.

### Task 3: Whole poses on the page (Shift+K, the row, editing)

**Files:**
- Modify: `mocap_bench/public/timeline.js`, `mocap_bench/public/inspector.js`, `mocap_bench/public/app.js`, `mocap_bench/README.md`
- Test: `mocap_bench/checks/timeline_check.js`, `mocap_bench/browser_check.mjs`

**Interfaces:**
- Consumes: `PO.bake`, `PO.wholePoses`, `PO.nextWhole`, `PO.planted`, `PO.LEG_SIDE`, `PO.clipTimes` (Task 1).
- **The timeline:**
  - Row id `"whole"`, labelled "whole pose", shown while any whole pose exists. It sits above the part rows.
  - `hitTest` on it gives `{ kind: "whole", whole, t }`.
  - `dragTo(project, { kind: "whole", whole }, t, fps, duration)` moves every key of that whole pose by one offset, clamped so each stays a frame clear of its part's other keys and inside the take.
  - `dragTo` of `{ kind: "pose" }` on a key with `whole` removes `whole` when its time changes.
  - Drawing: a whole pose is a 6 px diamond, hollow when all its keys are replace. A whole pose's key on a part row is the usual diamond with a centre dot.
- **The selection:** `{ kind: "whole", whole, t }`. `keepSelection` keeps it by id, else drops it.
- **The page (app.js):**
  - `bakedPoses()`: `PO.bake(current().poses, { times: clip times of state.result, locks: PR.effectiveLocks(current()) })`, cached by the result and the poses' and contacts' text.
  - Every `posedDelta` / `keyForTurn` read of a part's keys (the turn drag, K) takes `bakedPoses()[part]`.
  - The turn drag and K at a whole pose's time update the part's key there. A part with no key there joins the whole pose: its id, and the whole pose's mode (replace when mixed). A planted leg never joins.
  - `keyWholePose()` (Shift+K, either mode), one undo step "key the whole pose here":
    - It has K's guards: paused, a result, no version shown.
    - Every part the body has (a leg only when not planted at T) is keyed as it stands at the marker: `keyForTurn(mode, identity, …, bakedPoses()[part], T, captureDelta(part, T))`.
    - The mode is replace for a new whole pose, or each part's existing one when re-keying a whole pose at T (it keeps the id). An own key at T joins.
    - Status: "keyed the whole pose at 1.23 s: 13 parts (the left leg stays with its contact)".
  - Delete on a whole selection removes its keys, with "Ctrl+Z brings it back".
- **The inspector:**
  - `wholeSection`: the title "Whole pose at <t> s"; time `data-field="wholeTime"` (moves it, as the row's drag); mode `data-field="wholeMode"` (switching converts each part with `PO.switchMode` at its key; "mixed" shown and kept until chosen); ease `data-field="wholeEase"`; the parts it holds `data-field="wholeParts"`; help with the row, the doll and Delete.
  - A part key in a whole pose shows `data-field="poseWhole"` "in the whole pose at <t> s - moving it on its own row takes it out".

- [ ] **Step 1: Write the failing checks.**
  - **`timeline_check.js`:**
    - `wholeRowShown`: the whole row exists only with whole poses, above the part rows.
    - `wholeHit`: hitting it gives `{ kind: "whole" }`.
    - `wholeDragMovesAll`: a whole pose of 3 parts at 2.5 dragged to 2.7 puts all 3 at 2.7, clamped a frame before a part's next own key at 2.65.
    - `partDragDetaches`: dragging one part's key moves it and drops its `whole`; the other two keep theirs.
  - **`browser_check.mjs`** (after `poseUndoSelection`, on the existing test take):
    - `shiftKWholePose`: Shift+K at poseT makes one whole pose. Its keys at poseT are all replace with the same id. Legs are included only where the take's foot is off the ground (`PO.planted`). The doll's parts move under 0.5° at poseT. It shows on the whole row; the status names the parts.
    - `turnInsideWholePose`: in E mode, dragging the right forearm at the whole pose's time keeps one key there with the whole pose's id, its turn changed.
    - `wholeRowMoves`: dragging the whole row's diamond 0.1 s later moves every part's key.
    - `partRowDetaches`: dragging the head's key on its own row drops its `whole`.
    - `deleteWholePose`: Delete removes every key of the whole pose; Ctrl+Z brings them back; no page errors.
    - `versionShownRefusesWhole`: with the two-handed version shown, Shift+K is refused with the version's word.
- [ ] **Step 2: Run them; they fail** — `node mocap_bench/checks/timeline_check.js`, `node mocap_bench/browser_check.mjs`. Expected: the new steps FAIL, the old ones pass.
- [ ] **Step 3: Implement** the row, the selection, the editing, Shift+K and the inspector. Add Shift+K and the whole-pose row to README's keys.
- [ ] **Step 4: Run** `node mocap_bench/checks/run.js` and `node mocap_bench/browser_check.mjs`; every step passes, no page errors.
- [ ] **Step 5: Breaks:**
  - the joining rule;
  - the planted-leg refusal;
  - the whole drag's clamp;
  - the detach;
  - the baked keys for the drag (sparse instead);
  - the version guard.

  All must be CAUGHT.
- [ ] **Step 7: Commit** — `feat: the mocap bench keys the whole pose (Shift+K) - one key on its own row that moves, deletes and adjusts as one (nwn5e-ee #116, 3/4)`.

### Task 4: Simplify on the page

**Files:**
- Modify: `mocap_bench/public/inspector.js`, `mocap_bench/public/app.js`, `mocap_bench/README.md`
- Test: `mocap_bench/browser_check.mjs`

**Interfaces:**
- Consumes: `SI.simplify`, `SI.toWholePoses`, `SI.DETAIL` (Task 2); `bakedPoses`, `captureDelta` (Task 3).
- **The inspector:** the clip section's field "simplify to main poses".
  - A detail slider `data-field="detail"` (1-20°, step 1), showing "<n>°" with the ends "performed" and "simplified".
  - A button `data-field="simplify"`.
  - Help: "Keys the clip as a few whole poses, where its motion turns or holds; movements smaller than the detail are dropped. It keys what the doll shows now and replaces the keys inside the clip (a planted leg's own keys stay). Ctrl+Z undoes it; right after, the slider tries another detail."
- **`ctx.simplify(detail)` (app.js):**
  - It has K's guards.
  - It samples `shown` and `capture` at `PO.clipTimes`, with the shown body's `captureDelta` and `bakedPoses()`.
  - It runs `SI.simplify` and removes the keys R4 names.
  - It adds `SI.toWholePoses(…, PO.nextWhole(poses))` and sets `clip.detail`, as one undo step "simplify to main poses (<n>°)".
  - Status: "simplified to <n> whole poses at <d>° - <k> keys per part per second, at most <w>° off what it showed; Ctrl+Z undoes".
  - **Replacing itself:** after it, the page keeps the project's text. The next `simplify(d)` while the project still equals that text undoes the step first, so the result replaces it.
  - **The slider:** right after a simplify, moving it calls `simplify(d)`. Otherwise it only sets `clip.detail` (its own step).

- [ ] **Step 1: Write the failing browser steps:**
  - `simplifyButton`: pressing simplify makes whole poses spanning the kept part's first and last frames, all replace. No part has own keys inside the clip except a planted leg's. The status matches `/simplified to \d+ whole poses/`.
  - `simplifyAgainReplaces`: setting the slider to 12 right after leaves fewer whole poses. One Ctrl+Z gives back the project from before the first simplify (not the 5° result).
  - `simplifyUndo`: Ctrl+Y redoes it; no page errors.
  - `simplifyRefusedOnVersion`: with a version shown it is refused with the version's word.
- [ ] **Step 2: Run** `node mocap_bench/browser_check.mjs`. The new steps FAIL.
- [ ] **Step 3: Implement** the field, `ctx.simplify`, the slider rule, and README's simplify paragraph.
- [ ] **Step 4: Run** `node mocap_bench/checks/run.js` and `node mocap_bench/browser_check.mjs`; all pass.
- [ ] **Step 5: Breaks:**
  - the removal rule (a planted leg's keys removed);
  - the replace-itself rule;
  - the slider calling simplify when not right after;
  - the guards.

  All must be CAUGHT.
- [ ] **Step 6: Commit** — `feat: the mocap bench's "simplify to main poses" - a detail slider that keys the clip as a few whole poses (nwn5e-ee #116, 4/4)`.

### After the tasks

- The final whole-branch review: a fresh reviewer on the most capable model (`code-reviewer.md`), with this plan, the ledger's rulings and the Review Focus. One fix pass, each finding RED → GREEN.
- **The measurement for the owner:** n5_rage simplified at the default detail.
  - Keys per bone per second and arm frames at rest, before and after, beside NWN's clips.
  - The worst error.
  - Through the helper, so it is the game's own assembly.
- **Docs:** TOOLS-PLAN gets a #116 entry (nwn5e-ee); #116 gets a completion comment; the board goes to "Needs test" (bench only: restart the bench, no install).
