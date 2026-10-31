# Bench redesign, phase 4: poses — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On the Mocap Bench the owner presses E, grabs any part of the doll (torso, head, an upper arm, a shin, a hand...) and turns it. That keys a pose at the marker: a push on top of the capture, or a replace for frames the capture smeared. Each key has an ease and its exact numbers in the inspector. The game's core applies the pose keys at startup (step 4), on every body.

**Architecture:**
- Pose keys are step 4 of the assembled recipe, in the C++ core (`src/data/anim_assemble.cpp`), between the capture (step 1) and the IK pass (step 8). The game and the bench's preview (the helper) therefore run the same code.
- A key's turn is stored as degrees about the part's stance axes: the frame the capture's deltas already live in (L = S · delta).
- The bench mirrors only the conversion from a turn dragged on screen into a stored key (`public/core/pose.js`). A check proves through the helper that the core turns the part exactly as dragged.
- Turning the pelvis keeps the feet where the capture put them (the IK pass re-solves the legs), because "pose keys turn parts; they don't move the body".

**Tech Stack:** C++17 (VS2019 `cl`, test.bat's harness), Node 22 (no packages), browser canvas 2D, headless Chrome for the browser check.

**Spec:** `docs/BENCH-REDESIGN.md`:
- "The workflow" 4: "Any other part: turn it. That's a pose key, a push on top of the capture. For frames the capture smeared, switch the key to replace instead."
- "The screen": picking the nearest bone; dragging turns in the plane facing the camera; the inspector's pose key (push or replace, ease, the exact turn); the POSES rows; W / E = move / turn; K = key here.
- "The model": `poses per part: keys { t, turn (deg about the stance axes), push|replace, ease }`; "Pose keys turn parts; they don't move the body".
- "Shipping": step 4, "Pose keys".
- "Build order" 4.
- Issue #90 (epic #86).

**Owner's go:** 2026-10-31, "looks good, next phase" (after #88/#89's fixes).

## Global Constraints

- **Shared checkout** (nwn5e-ee CLAUDE.md, "Two sessions in one checkout"):
  - Announce to the game session, with the files named, before editing `src/data/anim_recipe.*`, `src/data/anim_pose.*`, `src/data/anim_assemble.*`, `tests/data/anim_*_test.cpp` and `src/core/dllmain.cpp`.
  - While the game session runs test.bat or builds, build only in a scratch copy: never let a red-first test land in the tree under its runs.
  - Commit with `git commit --only <paths> -F -`.
  - kVersion = the next after HEAD's, read just before committing.
  - The install is done by the game session (or by agreement), after the owner's "game closed".
- A recipe without `pose` lines assembles exactly as before: every existing test stays green unchanged.
- **Pose parts** (16, lower case, matched to a library's bones in any case):
  - `torso_g neck_g head_g pelvis_g`;
  - `lbicep_g lforearm_g lhand_g rbicep_g rforearm_g rhand_g`;
  - `lthigh_g lshin_g lfoot_g rthigh_g rshin_g rfoot_g`.
  - Not `rootdummy`: the hips' place and the body's travel stay the IK target's.
  - A body missing a part skips that part's keys.
- **A pose key** is `{ t (take s), turn [x, y, z] (deg), mode push|replace, ease (s) }`:
  - `turn` is the rotation R = Rz(z)·Ry(y)·Rx(x): X first, then Y, then Z, each about the part's stance axes (fixed, not moving with the capture).
  - With the capture's turn delta(T) relative to the stance's frame 0 (the recipe's own deltas):
    - push gives delta' = R · delta(T);
    - replace gives delta' = R.
  - Between two keys of a part, the two keys' results blend: Mix(res_i(T), res_{i+1}(T), smoothstep((T − t_i)/(t_{i+1} − t_i))).
  - Before the first key and after the last, the result blends from the capture over that key's `ease` (smoothstep): in over [t − ease, t], out over [t, t + ease]. Ease 0 is a step at the key's frame.
- **Numbers (builder's):**
  - ease default **0.2 s**, clamped to **[0, 2] s**;
  - a turn component is any finite number of degrees (the inspector offers −180..180);
  - parts are picked within **12 px** of their drawn segment;
  - a turn drag starts after **3 px**, like the move drag.
- New keys are **push**. Switching a key's mode keeps the pose at the key's own time: push→replace stores R·delta(t), replace→push stores R·conj(delta(t)).
- Public repos, SRD only; fixtures are made at check time. Process as phases 2-3: TDD, deliberate breaks for every check, headless Chrome on its own profile killed by PID, edit scripts run with `python -I`, stage by explicit path.

## Rulings made up front (builder's calls the spec leaves open)

1. **Stance axes and Euler XYZ.** These are the numbers the inspector shows and the owner types. The capture's own deltas are already turns about the stance axes, so push and replace are one multiplication each. Cost if wrong: near Y = ±90° two numbers trade off (gimbal). The stored turn is still exact; only the readout of three numbers is ambiguous there.
2. **The pelvis keeps the feet.** When `pelvis_g` has pose influence at T, the IK pass solves the legs so each foot stays where the capture put it before the poses. A leg whose own bones have pose influence goes to its posed foot instead (turning a leg moves its foot on purpose). The torso is the pelvis's sibling (rootdummy → torso_g, rootdummy → pelvis_g on every body), so it never moves the legs. Cost: legs re-solve on posed-pelvis frames (bend kept, as for the hips' target).
3. **IK wins on its chains.** Step 8 follows step 4, so while a limb's target has reach, IK re-solves the bicep and forearm (or thigh and shin). A pose key on those parts is overridden there, and the page says so. A hand's or foot's own turn survives IK: SolveTwoBone keeps the end's turn, unless a held foot key holds it (0.58.9).
4. **Picking in turn mode** picks the nearest drawn segment. Each segment belongs to the bone that turns it: a bone's segment toward its child, a clavicle to `torso_g`, a heel or toe to its foot, the head's to `head_g`.
5. **Authoring:**
   - A turn drag rotates the part about the camera's view axis through its joint, by the angle the pointer sweeps around the joint's screen point. It keys at the marker (snapped to a frame) as one undo step, with key turn R_new = Q_s · d_now · conj(d_cap) (push) or Q_s · d_now (replace), where:
     - Q_s is the drag's turn moved into the stance axes;
     - d_now is the part's delta as shown;
     - d_cap is the capture's delta, from the recipe's reduced track sampled as the core samples it.
   - K in turn mode keys the part where it is (Q_s = identity).

## Review Focus

1. **A pose on an IK chain whose limb has an active target.** IK wins (ruling 3), deterministically: no jitter, and the readout names it. (T3 `poseUnderIkTargetDeterministic`; T9 `ikOverridesNote`.)
2. **Turns near gimbal (Y = ±90°) typed in the inspector.** The quaternion round trip gives the same turn, so the part doesn't flip. (T2 `eulerRoundTripAtGimbal`.)
3. **Two keys of one part on the same frame, or dragged past each other.** Validate keeps them a frame apart and in order; the core never sees two at one time. (T6 `poseKeysValidated`; T1 reader sorts.)
4. **A pose key inside a blend or the loop's crossfade.** It is pulled back toward the stance or the loop's start, the loop still meets, and the existing note shows. (T3 `loopStillMeetsWithPoses`.)
5. **A pelvis turned while a foot has its own held key, or the hips a target.** The feet keep the key's place, or the capture's; no NaN, and no sinking past the cap. (T4 `pelvisPoseWithFootPinAndHips`.)

---

## Part A — the core (nwn5e-ee, game-owned files, announced)

### Task 1: Pose keys in the recipe

**Files:** modify `src/data/anim_recipe.h` and `anim_recipe.cpp`. Test: `tests/data/anim_recipe_test.cpp`.

**Interfaces — Produces:**
- `struct AnimPoseKey { float t; std::array<float,3> turn; bool replace; float ease; };`
- `const std::vector<std::string>& AnimPoseParts();` (the 16, in the order above).
- `AnimRecipe::poses`: `std::map<std::string, std::vector<AnimPoseKey>>`, part → keys by time.
- Line format: `pose <part> <take t> <x> <y> <z> <push|replace> <ease>`, written after the target lines, parts in AnimPoseParts order, keys by time, numbers by `Num`.

- [ ] **Step 1: Write the failing tests.**
  - `AnimRecipePoseRoundTrip`: two parts, three keys; write then read gives the same keys, sorted.
  - `AnimRecipeBadPoseRefused`: an unknown part, a missing number, mode `blend`, a non-finite number, extra words. Each is refused with "line N: ...".
  - `AnimRecipePoseEaseClamped`: ease −1 reads as 0, and 5 reads as 2.
  - `AnimRecipeNoPosesUnchanged`: a recipe without poses writes byte-identical text to today's.
- [ ] **Step 2:** Run `test.bat` (the scratch copy while the game session builds). Expected: the four fail to compile or fail.
- [ ] **Step 3:** Implement `PoseLines(const AnimRecipe&)` and `ReadPoseLine(words, line, r, error)` beside TargetLines / ReadTargetLine, sorting after reading.
- [ ] **Step 4:** Run test.bat. Expected: all pass. Commit (`--only` the three files): "feat: the assembled recipe carries pose keys (#90)".

### Task 2: Turns about the stance axes

**Files:** modify `src/data/anim_pose.h` and `anim_pose.cpp`. Test: `tests/data/anim_ik_test.cpp` (the pose math's file).

**Interfaces — Produces** (`data::pose`):
- `Quat TurnFromDegrees(const Vec3& xyz)` = Rz·Ry·Rx.
- `Vec3 DegreesFromTurn(const Quat& q)`: inverse, Y in [−90, 90].

- [ ] **Step 1: Write the failing tests.**
  - `TurnFromDegreesAxes`: (90,0,0) is a quarter turn about X, (0,90,0) about Y, and (0,0,90) about Z, each within 1e-5.
  - `TurnFromDegreesOrder`: (90,90,0) maps +Y to the turn of X-then-Y, not Y-then-X.
  - `eulerRoundTripAtGimbal`: for (10,20,30), (−170,45,120), (25,90,−40) and (0,−90,10), DegreesFromTurn then TurnFromDegrees is the same turn within 0.01°.
- [ ] **Step 2:** Run them. Expected: they don't compile.
- [ ] **Step 3:** Implement both functions.
- [ ] **Step 4:** Run them. Expected: they pass. Commit: "feat: the core's turns in stance-axis degrees (#90)".

### Task 3: Step 4, pose keys

**Files:** modify `src/data/anim_assemble.cpp`. Test: `tests/data/anim_ik_test.cpp` (it has the whole-body fixture).

**Interfaces:**
- Consumes: T1's `AnimRecipe::poses`, T2's `TurnFromDegrees`, `Mix`, and the existing `Bound` / `Composed` / `StanceValue`.
- Produces (file-local) `Value PosedDelta(const std::vector<AnimPoseKey>& keys, float T, const Value& capture)` per the Global Constraints' rules. Every place that takes a bone's composed turn uses `S · PosedDelta(...)` for a posed part: the no-target fast path and `composed_pose`. A posed part without a capture track joins `bound` with an identity delta, its home its stance value.

- [ ] **Step 1: Write the failing tests** (WholeBody fixture, IkRecipe):
  - `AnimPosePushAddsToCapture`: the torso turning 30° about Z in the capture, plus a push key (0,0,15) at 2.5 with ease 0.2. At t 0.5 the torso is at the capture's turn plus 15° about its stance Z (within 0.05°); at 0.25 and 0.75 it's the capture's.
  - `AnimPoseReplaceIgnoresCapture`: the same key as replace gives exactly 15° from the stance at 0.5, whatever the capture's turn.
  - `AnimPoseEaseAndBetweenKeys`: ease 0 is a step at the key's frame; two keys 0.4 s apart blend by smoothstep (at the midpoint, the halfway turn); a push key followed by a replace key blends between their results.
  - `AnimPoseNoKeysIdentical`: an empty part list, or a part this body lacks, gives clips with SameKeys as the plain recipe.
  - `AnimPoseCarriesInStanceAxes`: on a body whose pelvis rests turned 22° (an a_dfa-like bind), the same key turns the pelvis 15° about its own stance Z.
  - `poseUnderIkTargetDeterministic`: a forearm pose plus a left-hand target. The hand reaches its target; assembling twice gives SameKeys.
  - `loopStillMeetsWithPoses`: a looping recipe with a key near the end; the first and last frames meet, as AnimIkLoopStillMeets does.
- [ ] **Step 2:** Run test.bat (scratch). Expected: the new tests fail.
- [ ] **Step 3:** Implement `PosedDelta`, and the posed composition in both paths.
- [ ] **Step 4:** Run test.bat. Expected: all pass, with the 281 old ones unchanged. Commit: "feat: the assembled recipe's step 4 - pose keys, push and replace, eased (#90)".

### Task 4: The pelvis keeps the feet

**Files:** modify `src/data/anim_assemble.cpp` (the `stand` lambda, and the fast-path condition: the IK pass also runs when `pelvis_g` has pose keys). Test: `tests/data/anim_ik_test.cpp`.

**Interfaces — Consumes:** T3's posed composition and the unposed composed pose for the same frame (the capture's foot places).

- [ ] **Step 1: Write the failing tests.**
  - `AnimPosePelvisKeepsFeet`: a pelvis push (0,0,25) at 2.5. Both feet stay within 1 mm of the capture's places at 0.5, and the pelvis turned 25°.
  - `AnimPoseLegPoseMovesItsFoot`: a left thigh push (40,0,0). The left foot moves more than 10 cm, and the right foot stays within 1 mm.
  - `pelvisPoseWithFootPinAndHips`: a pelvis pose, a held left-foot key and a hips target together. The left foot is at its held place within 1 mm, the right at the capture's within 1 mm, nothing is NaN, and the sink is within its cap.
- [ ] **Step 2:** Run them. Expected: the first and third fail.
- [ ] **Step 3:** Implement: the legs solve when the pelvis has influence; the planted place comes from the unposed pose unless the leg's own bones have influence.
- [ ] **Step 4:** Run test.bat. Expected: all pass.
- [ ] **Step 5:** Mutation check: each of T1-T4's tests has a deliberate break it catches (listed in the commit).
- [ ] **Step 6:** Bump kVersion (the next after HEAD's). Build to scratch with `NWN5E_OUT`; core_stamp changes. Rebuild the helper (`nwn5e-tools/anim_helper/build_helper.bat`; rename a running helper aside if the owner's bench holds it) and run `serve_check.js`. Commit the core: "feat: 0.59.x - pose keys (#90)". Then ask the game session to install.

## Part B — the bench (nwn5e-tools/mocap_bench)

### Task 5: `core/pose.js`, the bench's mirror

**Files:** create `public/core/pose.js` (UMD as `MocapPose`, loaded after `ik.js`) and `checks/pose_check.js`.

**Interfaces — Produces:**
- `PARTS` (the 16) and `PART_LABELS` (torso, neck, head, pelvis, L upper arm, L forearm, L hand, R upper arm, ..., L thigh, L shin, L foot, ...).
- `turnFromDegrees(xyz) -> quat` and `degreesFromTurn(q) -> [x, y, z]`, the core's convention.
- `influenceAt(keys, T) -> 0..1`: the ease curve for the timeline.
- `posedDelta(keys, T, capture) -> quat`: the core's PosedDelta.
- `keyTurn(mode, Qs, dNow, dCap) -> [x, y, z]`, per ruling 5.
- `stanceTurn(Qworld, Wparent, S) -> Qs`: the world drag turn moved into the stance axes, conj(S)·conj(Wp)·Q·Wp·S.
- `switchMode(key, dCapAtKey) -> key`, per the Global Constraints.

- [ ] **Step 1: Write the failing checks.**
  - `conventionMatchesCore`: the same values as T2's tests.
  - `posedDeltaMatchesCore`: through the helper, as `ik_check.js` uses it. A recipe with push and replace keys on the torso and a forearm, assembled by the helper on a_ba: every frame's part turn equals S·posedDelta within 0.05°.
  - `landsWhereDragged`: a turn of 30° about a view axis at t, made into a key by `keyTurn`. The helper's clip has the part at exactly Q·W at t, within 0.05°: on a_ba, and with an existing key's influence there.
  - `switchKeepsPose`: switching modes at the key's time leaves the shown turn within 0.05°.
- [ ] **Step 2:** Run `node mocap_bench/checks/pose_check.js`. Expected: FAIL, no module.
- [ ] **Step 3:** Implement `pose.js`.
- [ ] **Step 4:** Run it. Expected: all pass. Commit: "feat: the mocap bench's pose math, mirrored from the core (#90)".

### Task 6: Poses in the project, the recipe and the pipeline

**Files:** modify `public/core/project.js`, `recipe.js` and `pipeline.js`. Checks: `project_check.js`, `recipe_check.js`, `pipeline_check.js`.

**Interfaces — Produces:**
- `project.poses`: part → keys `{ t, turn, mode, ease }`, all 16 parts present. `newProject` makes them empty; `parse` fills them for older files; `validate` (below); `muteEdits` clears them for the ghost; `ORDER` gains `poses`.
- `recipe.js` writes `pose` lines exactly as T1's format.
- The pipeline's clip cache key includes `project.poses`, so a pose edit re-assembles and never re-solves.

- [ ] **Step 1: Write the failing checks.**
  - `poseKeysValidated`: keys outside the take are dropped; two keys on one frame keep the later; out-of-order keys are sorted; a bad mode becomes push; ease is clamped to [0, 2]; a non-finite turn component becomes 0.
  - `oldProjectGetsPoses`.
  - `poseLinesLikeCore`: recipe.js's lines read back by the helper's reader unchanged.
  - `poseEditReassemblesOnly`: in the pipeline stats, solve +0 and assemble +1.
- [ ] **Step 2:** Run them. Expected: they fail.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run `node mocap_bench/checks/run.js`. Expected: all pass. Commit.

### Task 7: The stage's turn mode

**Files:** modify `public/stage.js`. Check: `checks/stage_check.js`.

**Interfaces — Produces** (on the stage object):
- `pickPart(x, y, pose, bones) -> part | null`: per ruling 4, within 12 px.
- `jointOf(part) -> bone index`.
- `viewAxis() -> vec3`: the camera's forward.
- `angleAt(x, y, center)`: the pointer's angle about a screen point.
- `create`'s handlers gain a `mode` getter ("move" | "turn"). In turn mode the selected part's joint draws a ring with an arc.

- [ ] **Step 1: Write the failing checks** (stage_check's projected fixture pose):
  - `pickPartFindsSegments`: the middle of the forearm's segment is `lforearm_g`, a clavicle is `torso_g`, a toe is `lfoot_g`, the head is `head_g`, and 40 px from everything is null.
  - `angleAtSweeps`: a quarter circle about the centre is 90°.
- [ ] **Step 2:** Run it. Expected: it fails.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run it. Expected: it passes. Commit.

### Task 8: Pose rows on the timeline, and the key's inspector

**Files:** modify `public/timeline.js` and `inspector.js`. Check: `checks/timeline_check.js`.

**Interfaces — Produces:**
- `layout` adds a row `poses.<part>` per part with keys, plus the focused part's, after the target rows, labelled by PART_LABELS.
- `hitTest` returns `{ kind: "pose", part, index }`.
- `dragTo` handles kind `pose`: inside the take, a frame clear of its neighbours.
- Drawn: push keys as filled diamonds, replace keys hollow, the influence faint behind.
- The inspector's `poseSection`:
  - the part's name;
  - time (s);
  - mode (push | replace), whose change goes through `switchMode`;
  - ease (s);
  - turn X / Y / Z (deg);
  - one line of help each. The replace help: "for frames the capture smeared: the capture is ignored here".

- [ ] **Step 1: Write the failing checks.**
  - `poseRowsOnlyWhenUsed`.
  - `hitTestFindsPoseKey`.
  - `poseDragClamps`.
- [ ] **Step 2:** Run them. Expected: they fail.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run them. Expected: they pass. Commit.

### Task 9: The page: E to turn, keys, the readout

**Files:** modify `public/app.js` and `index.html` (load `core/pose.js`; a move | turn toggle beside the view buttons). Check: `browser_check.mjs`.

**Interfaces — Consumes:** T5-T8.

- [ ] **Step 1: Write the failing browser steps** (real input):
  - `turnForearm`: E, then a drag around the left forearm's joint sweeps 40°. A key appears on the forearm's row at the marker, and the forearm's world turn at the marker is the dragged one within 0.5° (the helper's clip).
  - `replaceKey`: set that key to replace in the inspector. The shown pose at the key is unchanged within 0.5°, and 0.1 s later it no longer follows the capture.
  - `easeAndNumbers`: type turn X = 20 and ease 0.5. The part turns, and the influence row widens.
  - `poseUndo`: Ctrl+Z / Ctrl+Y step through the edits.
  - `pelvisKeepsFeet`: turning the pelvis by drag leaves both feet within 1 cm.
  - `ikOverridesNote`: a forearm key while that hand has a target. The readout says the target overrides it.
  - `wMovesAgain`: W, then a drag on the hand, moves it (phase 3 unchanged).
- [ ] **Step 2:** Run `node mocap_bench/browser_check.mjs`. Expected: the new steps fail.
- [ ] **Step 3:** Implement:
  - E and W, and the toggle;
  - the turn drag (one undo step, `keyTurn` at the snapped marker) and K in turn mode;
  - Delete for pose keys;
  - selection of a part and of its keys;
  - the readout note when a target overrides the selected part.
- [ ] **Step 4:** Run the browser check and checks/run.js. Expected: all pass. Commit.

### Task 10: Docs and the hand-over

**Files:** `mocap_bench/README.md` (a "Poses" section: E, push and replace, ease, the numbers, the pelvis rule, IK wins), `nwn5e-ee/docs/BENCH-REDESIGN.md` (phase 4's *Built* note) and `nwn5e-ee/docs/TOOLS-PLAN.md` (the record).

- [ ] **Step 1:** Write the docs. Commit each repo, docs only (`--only` in nwn5e-ee).
- [ ] **Step 2:** One fresh review of the whole branch (both repos' commits since this plan), then one fix pass, each fix red first.
- [ ] **Step 3:** The hand-over on #90: what was done, files, the test matrix, caveats, rulings. Board → Needs test.
