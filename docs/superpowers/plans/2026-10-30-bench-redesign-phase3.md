# Bench redesign, phase 3: IK targets and pins — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The owner drags a hand, a foot or the hips on the Mocap Bench's doll and the limb follows. That keys an IK target at the marker, which eases in and out ("reach"); a pinned key holds the limb still in the world until the next key. The game's own core applies these edits at startup, on all eight bodies by proportion, with extension damping and the hips sinking when a foot can't reach.

**Architecture:**
- The IK pass is step 8 of the assembled recipe, in the C++ core (`src/data/anim_assemble.cpp`), so the game and the bench's preview (the helper) run the same code.
- The pose math the crouch step already has (FK, the two-bone leg solve) moves to `src/data/anim_pose.*`. It is generalised for arms and legs, with a pole and damping.
- The recipe carries the targets in take time with a `take_start`.
- The bench mirrors only the frames that turn a dragged place into a stored offset (`public/core/ik.js`). A check proves through the helper that the core lands the limb where the bench says.

**Tech Stack:** C++17 (VS2019 `cl`, test.bat's harness), Node 22 (no packages), browser canvas 2D, headless Chrome for the browser check.

**Spec:** `docs/BENCH-REDESIGN.md` — "The workflow" 4 (edit), "The model" (targets; the hips move only through their target; targets carry by proportion), "Shipping" step 8, "Checks", "Build order" 3. The research is `docs/research/reports/Mocap cleanup and editing workflow.md` ("IK targets with keyed reach"). Issue #89 (epic #86).

**Owner's go:** 2026-10-30, "can you go ahead and do the IK phase? I'll look at phases 2 and 3 together". So this plan is executed without a separate review, and the owner tests #88 and #89 together.

## Global Constraints

- **Shared checkout** (nwn5e-ee CLAUDE.md, "Two sessions in one checkout"):
  - The core files are game-owned or shared, and the game session confirmed them free (2026-10-30): `src/data/anim_recipe.*`, new `src/data/anim_pose.*`, `src/data/anim_assemble.*`, `tests/data/anim_*_test.cpp`, `build.bat`, `core_stamp.bat`, `src/core/dllmain.cpp`.
  - Commit them with `git commit --only <paths> -F -`, then tell the game session they're committed.
  - kVersion = the next after HEAD's, read just before committing.
  - Build to scratch (`NWN5E_OUT`). Install into `build\` only by agreement.
- A recipe without `target` lines assembles exactly as before: phase 1 and 2's checks stay green.
- **Limbs:**
  - `left_hand` (lbicep_g, lforearm_g, lhand_g), `right_hand` (rbicep_g, rforearm_g, rhand_g);
  - `left_foot` (lthigh_g, lshin_g, lfoot_g), `right_foot` (rthigh_g, rshin_g, rfoot_g);
  - `hips` (rootdummy's position).
  - A body missing a limb's bones skips that limb.
- **Targets carry by proportion:**
  - a hand: from its shoulder (the bicep's joint), in the torso's frame (torso_g), as a fraction of the arm's length (|forearm offset| + |hand offset|, from the library's rest);
  - a foot: from its hip joint (the thigh's joint), in the pelvis's frame (pelvis_g), as a fraction of the leg's length;
  - the hips: from the stance's first-frame root, in world axes, as a fraction of the hip height (the thighs' mean rest height, the bench solve's `hipHeight`).
- **Numbers (builder's):**
  - reach eases over **0.2 s** outside a limb's keys;
  - damping softens the last **5 %** of a limb's length;
  - the hips sink by at most **0.25 × hip height**;
  - a shortfall over **3 cm** is reported.
- Public repos, SRD only; fixtures are made at check time. Process as phase 2: TDD, deliberate breaks for every check, headless Chrome on its own profile killed by PID, edit scripts run with `python -I`, stage by explicit path.

## Rulings made up front (builder's calls the spec leaves open)

1. **Foot contacts stay pinned by the bench's solve** (phase 2's locks, verified). The core's IK pass does the owner's targets and pins, and while the hips move, the feet keep the places the solve gave them (already locked). Cost: two leg solvers (the bench's for contacts, the core's for targets) until phase 6 moves contacts into the core with the balance.
2. **A target key** is `{ t, offset[3], pinned, reach }` in take time:
   - the offset is the target's place in the limb's frame, divided by the limb's length;
   - between two keys the place and the reach interpolate (smoothstep);
   - before the first key and after the last, the reach eases from 0 over 0.2 s, so a single key is a bump in and out of the capture;
   - a pinned key's place is fixed in the world from its time to the next key, easing toward the next key's place over the last 0.2 s;
   - the hips key's offset is world-relative already, so its pin flag does nothing.
3. **IK runs on the pre-blend pose,** as the spec orders it (8, then 9). A target keyed inside a blend is pulled back toward the stance by the blend. The bench says so when the marker is inside one.
4. **Hands and feet keep their turn in the world** while their chain is solved. Poles:
   - the knee follows the core's existing rule (the bend it had; the thigh's facing when straight);
   - the elbow follows the bend it had, or the torso's backward when the arm is straight.
5. **Authoring:** a drag on the doll keys the limb at the marker (snapped to a frame) with reach 1, unpinned, computed from the shown pose. One drag is one undo step. K keys the selected limb where it is now.

## Review Focus

1. **A target dragged inside a blend zone or the loop's crossfade.** It lands short, says so, and doesn't snap. (T3 `blendPullsTargetBack`; the page's note in T10.)
2. **A target far beyond reach, on every body** (a short body, a tall one). The limb points at it and stops at full length, softened by the damping; the hips sink at most their cap; there's no NaN; the shortfall is reported per body. (T3 `farTargetSafe`; T10 readout.)
3. **Keys of one limb on the same frame, or dragged past each other.** Validate merges them a frame apart and in order; the core never sees two keys at one time. (T7 `targetKeysValidated`.)
4. **A body without a limb's bones,** or a stance without a frame 0 for a bone. That limb is skipped and the others still solve. (T3 `missingLimbSkipped`.)
5. **Targets on a looping clip.** The loop's crossfade happens after IK, so the start and end still meet. (T3 `loopStillMeets`.)

---

## Part A — the core (nwn5e-ee, game-owned files, announced)

### Task 1: The shared pose math and the two-bone solver

**Files:** create `src/data/anim_pose.h`, `src/data/anim_pose.cpp`. Modify `src/data/anim_recipe.cpp`: its private Vec3/Quat helpers, `Pose`, `ClipIndex`, `Forward`, `PoseAt`, `FromTo`, `SolveLeg` move out, and the crouch step calls them. `build.bat` (line 60 list) and `core_stamp.bat` (line 9 list) gain `anim_pose.*`. Test: `tests/data/anim_ik_test.cpp`.

**Interfaces — Produces** (namespace `data::pose`, internal to the core):
- `Vec3`, `Quat`, `Add Sub Scale Dot Cross Length Normalized QConj QNormalized Rotate FromTo AxisAngleQ`; `struct Pose { lp, wp, lq, wq }`; `ClipIndex`, `Forward`, `PoseAt` (as today).
- `struct Chain { int root, mid, end; }`
- `float SolveTwoBone(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Chain& c, const Vec3& goal, const Vec3& pole_hint, float damping)`:
  - turns root and mid so the end reaches `goal` (softened within `damping` × length of full extension);
  - keeps the end's world turn from `before`;
  - bends in the plane of `before`'s bend, else `pole_hint`;
  - returns the shortfall `max(0, |goal − root| − length)`.
- `void SolveLeg(bones, before, now, Leg)` keeps its behaviour: the crouch step's call.
- `float RestHipHeight(const std::vector<AnimNode>& bones)`; `float ChainLength(bones, Chain)`.

- [ ] Write the tests (`anim_ik_test.cpp`) on a hand-built three-bone chain and on a leg fixture:
  - `twoBoneReaches`: a goal inside reach gets the end within 1e-4 m;
  - `twoBoneShortfall`: a goal 0.3 m past reach returns 0.3 ± 1e-4 and the chain points at it;
  - `poleKeepsBendPlane`: the mid joint stays on the before-bend side;
  - `dampingSoftensNearStraight`: at 99 % of the length, with damping 0.05, the end stops short (by 0.1–5 mm) and the solve doesn't snap;
  - `kneesNeverBackward`: the knee's distance from the hip-ankle line keeps its sign over a sweep of goals.
- [ ] Run test.bat. Expected: compile fails (no `anim_pose.h`).
- [ ] Move the math (the bodies verbatim) and generalise `SolveLeg` into `SolveTwoBone`.
- [ ] Run test.bat. Expected: all pass, including the existing crouch tests unchanged.
- [ ] Deliberate breaks: damping ignored; the end's turn not kept; the pole sign flipped. Each is caught.
- [ ] Commit `feat: the recipe core's pose math in one place, a two-bone solver for arms and legs (#89)`.

### Task 2: Targets in the recipe

**Files:** `src/data/anim_recipe.h`, `.cpp`; `tests/data/anim_recipe_test.cpp`.

**Interfaces — Produces:**
- `struct AnimTargetKey { float t; std::array<float, 3> offset; float reach = 1; bool pinned = false; }`
- `AnimRecipe::take_start` (float, take seconds at the clip's 0)
- `AnimRecipe::targets` (`std::map<std::string, std::vector<AnimTargetKey>>`, limb → keys sorted by t)
- `const std::vector<std::string>& AnimTargetLimbs()` = the five limbs
- Text lines: `take_start = <s>` and `target <limb> <t> <x> <y> <z> <reach> [pin]`. Times to 4 decimals, offsets to 5.

- [ ] Tests:
  - `targetsRoundTrip`: write → read gives the same keys, sorted;
  - `badTargetRefused`: an unknown limb, a missing number, or a non-finite value fails the read with the line named;
  - `reachClamped`: reach outside 0..1 is clamped;
  - `oldRecipesUnchanged`: phase 1's fixtures write byte-identical when they have no targets.
- [ ] RED, implement, GREEN, breaks (a pin flag dropped on write; keys left unsorted), commit `feat: assembled recipes carry IK targets in take time (#89)`.

### Task 3: The IK pass (step 8) and its report

**Files:** `src/data/anim_assemble.h`, `.cpp`; `tests/data/anim_ik_test.cpp`, `anim_assemble_test.cpp`.

**Interfaces — Produces:**
- `struct AnimIkShort { std::string limb; float metres; float t; }`
- `bool AssembleAnimClip(recipe, library, bones, stance, out, err, std::vector<AnimIkShort>* report = nullptr)`: the report holds each limb's worst shortfall over 3 cm while its reach is ≥ 0.999.

**Behaviour, per frame:**
1. The composed pose (steps 1 and 6, as today).
2. The hips: the root moves toward `home + offset × hipHeight` by the reach.
3. Each foot:
   - its goal is where the composed pose had it, blended toward its target by the reach (the target from the moved hip joint, in the moved pelvis frame);
   - the legs are solved when the root moved or a foot target is active;
   - if a leg falls short, the root drops by the larger shortfall (capped), and the legs are solved again (two passes).
4. Each hand: its goal is blended toward its target (from the shoulder, in the torso's frame) by the reach, then solved.
5. Pinned keys: the target's world place at the key's time (on this body), held as rule 2 says.
6. The loop and the blends (step 9) then run on the result as today.

All bones are keyed at 30 fps (a solved chain isn't a blend of its neighbours).

- [ ] Tests (on the leg/arm fixture and on two library-like bodies of different scale):
  - `noTargetsIdentical`: a recipe without targets gives the same clip as before, bit for bit;
  - `handTargetReached`: at the key's time the hand is within 1 mm of the place;
  - `reachEases`: the influence is 0 at ±0.2 s outside a single key, 1 at the key, and smooth in between;
  - `pinHoldsWorld`: between a pinned key and the next, the hand's world place moves under 1 mm while the torso moves;
  - `hipsTargetByProportion`: on a body 1.25× the size, the root moves 1.25× as far, and the feet stay within 1 mm;
  - `sinkingWhenFootTooFar`: a foot target 10 cm past reach lowers the root by the shortfall (within the cap) and the foot reaches;
  - `proportionAcrossBodies`: a hand offset lands at the scaled place on the large body;
  - `farTargetSafe`: a target 2 m away gives no NaN, the hips ≤ the cap, and the shortfall reported;
  - `blendPullsTargetBack`: inside blend_in, the hand is between the stance and the target, by the blend's weight;
  - `missingLimbSkipped`: a body without lbicep_g still solves the other limbs;
  - `loopStillMeets`: a loop's first and last frames match (within the loop blend's rule) with targets active.
- [ ] RED, implement, GREEN, breaks:
  - the reach ease removed;
  - pins re-evaluated every frame (not held);
  - the hips not scaled by hip height;
  - no sinking;
  - the report dropped.
- [ ] Commit `feat: the assembled recipe's IK pass - targets, pins, reach, damping, the hips sinking (#89)`.

### Task 4: The DLL build

- [ ] Read HEAD's kVersion; bump to the next in `src/core/dllmain.cpp`.
- [ ] Run test.bat (all green).
- [ ] Build to scratch with `NWN5E_OUT`. The DLL's `core_stamp.txt` should equal the rebuilt helper's stamp (Task 5).
- [ ] Commit with `--only`, and tell the game session the files are committed.
- [ ] Agree the install with the game session. After it, a launch check: the log shows the recipes built for 8 bodies and the new stamp.

---

## Part B — the bench (nwn5e-tools)

### Task 5: The helper reports shortfalls

**Files:** `anim_helper/main.cpp` (AssembleToJson adds `"ik":[{"limb","short","t"}]`), `anim_helper/build_helper.bat` (adds `anim_pose.cpp`), `anim_helper/serve_check.js`.

- [ ] Test `ikReported`: a recipe with a far foot target answers an `ik` entry over 0.03.
- [ ] RED, rebuild, GREEN, a break, commit.

### Task 6: The bench's mirror of the frames

**Files:** create `mocap_bench/public/core/ik.js` (`MocapIK`) and `checks/ik_check.js`.

**Interfaces — Produces:**
- `LIMBS`: `{ left_hand: { chain: [...], frame: "torso_g", from: "lbicep_g" }, ... }`.
- `limbLength(bones, limb)`, `hipHeight(bones)`, `home(bones, stanceClip)`.
- `offsetOf(limb, bones, pose, place, home) -> [x,y,z]` and `placeOf(limb, bones, pose, offset, home) -> world`; the two are inverses.
- `reachAt(keys, t) -> 0..1`.
- `EASE = 0.2`.

- [ ] Tests:
  - `offsetPlaceInverse`;
  - `reachCurveLikeCore` (the same samples as T3's `reachEases`);
  - `landsWhereDragged`: an offset from `offsetOf` on a_ba's posed fixture, written into a recipe and assembled by the helper, puts the hand within 1 mm of the place at the key's time on a_ba;
  - `carriesByProportion`: the same recipe on a_dfa lands at `placeOf` of that body's pose;
  - `hipsByProportion`.
- [ ] RED, implement, GREEN, breaks (the torso's frame swapped for the pelvis's; the length unscaled), commit.

### Task 7: Targets in the project, the recipe writer and the pipeline

**Files:** `public/core/project.js` (`targets` per limb; `validate` sorts, clamps, and keeps one limb's keys a frame apart inside the take; `serialize` order; `muteEdits` drops targets: the ghost), `recipe.js` (`take_start` and `target` lines), `pipeline.js` (the clip's memo key has the targets; `result.ik` = the helper's report); their checks.

- [ ] Tests:
  - `targetKeysValidated` (Review Focus 3);
  - `targetsRoundTrip` (project file);
  - `ghostHasNoTargets`;
  - `recipeShipsTargets` (lines parsed back by the core through the helper);
  - `targetEditReassemblesOnly` (the solve isn't re-run: stats).
- [ ] RED, GREEN, breaks, commit.

### Task 8: Picking and dragging on the stage

**Files:** `public/stage.js` (`project(p) -> [x,y,depth]`; `planePoint(x, y, through) -> world`, on the plane facing the camera through `through`; `pick(x, y, places) -> name | null` within 18 px, nearest first; handles drawn for limbs with keys and for the selected one), `checks/stage_check.js`.

- [ ] Tests:
  - `projectRoundTrip` (`planePoint` of `project(p)` is `p`);
  - `pickNearest` (the nearer of two, and null beyond 18 px);
  - `handlesDrawn`.
- [ ] RED, GREEN, breaks, commit.

### Task 9: Target keys on the timeline and in the inspector

**Files:** `public/timeline.js` (a row per limb that has keys, plus the selected limb's; keys as diamonds; `hitTest` returns `{ kind: "target", limb, index }`; `dragTo` moves a key in time, inside the take, a frame clear of its neighbours, the reach curve drawn faintly), `public/inspector.js` (a target key: time, reach 0..1, "hold it here until the next key" (pinned), the place in cm on this body, Delete), `checks/timeline_check.js`.

- [ ] Tests:
  - `targetRowsOnlyWhenUsed`;
  - `hitTestFindsTargetKey`;
  - `targetDragClamps`.
- [ ] RED, GREEN, breaks, commit.

### Task 10: The page wiring and the readouts

**Files:** `public/app.js`, `index.html`, `style.css`, `README.md`.

- **A drag on the doll** that starts within 18 px of a hand, a foot or the hips:
  - pointer down: `history.begin("move the <limb>")`;
  - each move: the place on the camera plane → `offsetOf` → upsert the key at the marker → `update`;
  - pointer up: `commit`.
  - Elsewhere, the drag orbits as before.
- **Keys:**
  - K keys the selected limb at the marker where it is now;
  - Delete removes a selected target key;
  - W is the move mode (the only mode until phase 4 adds E, turn).
- **Readouts under the stage, for the shown body:**
  - limbs out of reach (the helper's report, over 3 cm);
  - the worst foot slide during contacts (from the assembled clip: each contact's foot drift from its start, cm);
  - a note when the marker is inside a blend.
- **Save** lists, per body, the shortfalls and any foot slide over 2 cm.

- [ ] This task's test is Task 11.
- [ ] Commit `feat: the mocap bench - drag a hand, a foot or the hips on the doll (#89)`.

### Task 11: The browser check grows

**Files:** `mocap_bench/browser_check.mjs`.

- [ ] Steps, with real mouse and keys:
  1. `dragHand`: drag the right hand 60 px at the marker → a key at the marker. The assembled hand at the marker is within 1.5 cm of the drop point's place on the camera plane.
  2. `reachEases`: 0.3 s from the key, the hand is back on the ghost's (within 1 mm).
  3. `pinHolds`: key it again 0.4 s later and pin the first; between them the hand's world place moves under 1 cm.
  4. `hipsDrag`: drag the hips 40 px down → the root lowers; feet in contact stay within 1 cm.
  5. `keyHere`: K on the left foot → a key whose place is where the foot was (the result unchanged within 1 mm).
  6. `targetUndo`: Ctrl+Z / Ctrl+Y step through the drags.
  7. `outOfReach`: drag a foot far down → the readout names it on this body.
  8. `saveShipsTargets`: Save; the recipe has `target` lines and `take_start`; reopen keeps the keys.
  - The phase 2 steps keep passing.
- [ ] Breaks:
  - the drag not snapped to the marker's frame;
  - the ghost drawn with targets;
  - K keying the wrong limb.
- [ ] Commit `feat: the mocap bench's browser check covers IK (#89)`.

### Task 12: Docs and hand-over

- [ ] Update the docs:
  - mocap_bench `README.md`: the IK section, the keys W/K, the readouts;
  - `docs/BENCH-REDESIGN.md`: the status, and phase 3 built;
  - `docs/TOOLS-PLAN.md`: the phase 3 entry with the rulings.
- [ ] Run everything: test.bat, serve_check, `checks/run.js`, `browser_check.mjs`.
- [ ] The final review (a fresh reviewer on the whole branch), then one fix pass.
- [ ] Hand over on #89:
  - a comment;
  - the `needs-test` label;
  - the board → Needs test;
  - a matrix (I1–I8) printed in chat, written to be run after #88's M1–M8.
