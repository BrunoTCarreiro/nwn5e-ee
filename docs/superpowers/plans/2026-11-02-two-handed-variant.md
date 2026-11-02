# Two-Handed Variant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A bench clip gets a two-handed version (`<clip>_2h`) whose off hand holds the weapon's haft all through; the game plays it instead of the clip when the character wields a two-hander.

**Architecture:**
- The recipe gains `grip = 1`.
- The game's assembly (the recipe core, `src/data/anim_assemble.cpp`) holds the left hand where the start pose holds it against the right hand, every frame, after the other IK. The relation is lhand_g in rhand_g's frame, measured from the stance's first frame on each body; the two-handed version's start pose is 2hreadyr.
- `AnimationVariant(name, two_handed)` in the animation code picks `<name>_2h` when it was built.
- The bench writes the line and makes the version.
- The game session swaps the one call where Rage plays n5_rage.

**Tech Stack:** C++ (the DLL's recipe core, tests in test.bat), Node / browser JS (the bench).

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#112 and its decision comment (owner, 2026-11-02: "Go with your recommendations" = (a) a grip rule in the recipe, one two-handed version for now).

## Global Constraints

- The animation session owns `src/data/anim_*`, `src/core/anim_packs.cpp`, `tests/data/anim_*` and the bench. `src/core/dllmain.cpp` (the version) is shared: announce, then the next kVersion after HEAD's.
- The game session edits `src/core/rules_barbarian.cpp` itself (it agreed), after this plan's core commit.
- One installer (the game session). The bench's helper is rebuilt for the new core stamp only after the install; until then the owner's bench keeps the installed helper.
- A grip with a stance whose hands aren't on one weapon still holds that relation (the bench offers the version with 2hreadyr).
- Recipe names stay within 16 characters: `<name>_2h` needs the name to be 13 or less.

## Review Focus

1. A body without rhand_g or a left arm chain: no grip, nothing throws, the clip as without it.
2. A left-hand IK target together with the grip: the grip wins (documented), with no fight between them.
3. A recipe with `grip = 1` and no other targets: it takes the IK path (the early path would skip the grip).
4. The grip at the clip's ends: the start pose already holds it, so there's no jump.
5. `<name>_2h` over 16 characters: the bench refuses with a reason, and no project is made.

---

### Task 1: The grip in the recipe core

**Files:**
- Modify: `src/data/anim_recipe.h` / `.cpp` (`AnimRecipe::grip` bool; read `grip = 1`; written by `WriteAnimRecipe` for assembled recipes).
- Modify: `src/data/anim_assemble.cpp` (the IK path taken when `recipe.grip`; after the hands' IK, the left arm solved to the grip).
- Modify: `src/core/dllmain.cpp` (kVersion: the next after HEAD's).
- Test: `tests/data/anim_ik_test.cpp`, `tests/data/anim_recipe_test.cpp`.

**Interfaces:**
- Produces:
  - `AnimRecipe::grip` (false by default); the recipe line `grip = 1`.
  - The assembled clip's lhand_g holds `rel = inverse(W(rhand_g)) · W(lhand_g)` from the stance's first frame, position and turn.

- [ ] **Step 1: Failing tests.**
  - `AnimRecipeGripRoundTrip`: a recipe with `grip = 1` reads back `grip == true` and writes `grip = 1`; without it, false and no line.
  - `AnimIkGripHoldsTheOtherHand` (WholeBody, IkStance; the recipe turns the torso 30 deg and the right forearm 40 deg over the second): with `grip`, at t = 0, 0.25, 0.5, 0.75 and 1 the left hand's place in rhand_g's frame equals the stance's within 1 mm, and its turn within 0.5 deg. Without grip, the place is over 5 cm off at t = 1 (the control).
  - `AnimIkGripMissingHandSkipped`: a body whose rhand_g is renamed gives a clip identical to the same recipe without grip.
- [ ] **Step 2: Run** the anim tests (the scratch `animtest.bat`) → the new tests FAIL.
- [ ] **Step 3: Implement.**
  - Read and write the line.
  - `if (limbs.empty() && !pelvis_keys && !recipe.grip)` keeps the early path.
  - In the per-frame IK loop, after the hands: if grip and the body has rhand_g and the lbicep/lforearm/lhand chain, `goal = W(rhand_g) · rel.p` and `hand = W(rhand_g) · rel.q`, then `SolveTwoBone` on the left arm (elbow way as the hands', damping as theirs). Mark the chain solved. A left_hand target's result is overridden (ruling).
- [ ] **Step 4: Run** the anim tests → pass; test.bat → all pass (the game session isn't mid-run; ask first).
- [ ] **Step 5: Commit** `feat: 0.60.x - a recipe's grip holds the off hand on a two-hander's haft (#112, 1/3)` (`git commit --only` the files named; announce dllmain).

### Task 2: The variant pick in the animation code

**Files:**
- Modify: `src/core/anim_packs.cpp`: `std::string AnimationVariant(const std::string& name, bool two_handed)` returns `name + "_2h"` when two_handed and `AnimationRecipeBuilt(name + "_2h")`, else name. Its pure part goes in `src/data/anim_recipe.cpp`: `std::string VariantName(const std::string& name, bool two_handed, const std::function<bool(const std::string&)>& built)`.
- Test: `tests/data/anim_recipe_test.cpp` `AnimVariantName`: (n5_rage, true, built has n5_rage_2h) gives n5_rage_2h; two_handed false gives n5_rage; not built gives n5_rage; a name over 13 characters gives the name.

- [ ] **Steps:** failing test, implement, the anim tests and test.bat pass, commit with Task 1's version (same commit or the next one). Send the game session the exact signature and declaration.

### Task 3: The bench makes the two-handed version

**Files:**
- Modify: `mocap_bench/public/core/project.js` (`clip.grip`, default false; validated; ORDER), `mocap_bench/public/core/recipe.js` (writes `grip = 1`), `mocap_bench/public/inspector.js` (a "two-handed grip" checkbox; a "two-handed version" button), `mocap_bench/public/app.js` (the button: a new project `<name>_2h`, a clone with stance 2hreadyr, grip on, opened unsaved; refused over 16 characters).
- Test: `checks/project_check.js` (`gripDefault`), `checks/recipe_check.js` (`gripWritten`, and through the helper on every body: `gripAssembled`, the left hand within 1 cm of the right hand's grip place at 5 times; this needs the rebuilt helper), `browser_check.mjs` (`twoHandedVersion`: the button opens `n5_bchk_2h`-style named projects... the fixture's name gives the version's name, 2hreadyr, grip on, the gear picker a greataxe; over 16 characters it refuses).

- [ ] **Steps:** failing tests, implement, run.js and browser_check pass (with the 0.60.x helper built for the checks, the installed one put back for the owner), commit.

---

## Addendum 2026-11-02: the weapon hand gives way, the grip after the blends, the polearm version

**Why:** the final review (ledger "Final:" lines) found the plan's grip too weak for a one-handed take. On n5_rage made
into the version, the left arm stretches straight at the haft and falls short on 87-94 of 105 frames (20-47 cm) - C1.
It also found the grip solved before Step 9's blends, so it drifts in unseamed clips and in-place loops (I1). The owner,
2026-11-02: "go with a, and c for the important clips" ((a) the weapon hand gives way; (c) recorded two-handed takes,
his own); "yes, go with the polearm version too. For the crossbow let's leave it at 1 handed rage for now".
Measured grips (NWN's ready stances, 4 bodies): 2hreadyr hands 13-25 cm apart, the left just below the right;
plreadyr 64-89 cm, the left 61-87 cm up the shaft; 2wreadyr / bowrdy / 1hreadyr no shared grip; xbowrdy the
crossbow in the left hand (left as one-handed, ruled).

**Global Constraints (added):**
- The grip's rule takes its relation from the start pose, so each grip kind is a start pose: the two-handed version
  is made on 2hreadyr (`<name>_2h`), the polearm version on plreadyr (`<name>_pl`). No rule per weapon.
- The off arm's limit is the straighter of 95% of its length (`kGripBend`) and its span in the start pose, so a
  clip's ends - the start pose - never move.
- Only a recipe with the grip changes; every other clip assembles exactly as before.
- The bool `AnimationVariant(name, two_handed)` stays (the game's 0.60.3 calls it); the grip-kind form is added.

### Task 4: The grip over the finished frames (core 0.60.4)

**Files:** `src/data/anim_assemble.cpp`, `src/data/anim_recipe.h/.cpp` (`WeaponGrip`, `VariantName` by kind),
`src/core/anim_packs.cpp` (`AnimationVariant(name, data::WeaponGrip)`), `src/core/dllmain.cpp` (the next kVersion
after HEAD's, read just before committing), tests in `tests/data/anim_ik_test.cpp`, `tests/data/anim_recipe_test.cpp`.

- The in-loop grip goes. One pass, `hold_grip`, runs after Step 9 and the unkeyed nodes, on both paths (the early
  path is no longer forced by the grip). For each frame `f`:
  - Take the finished pose (`pose::PoseAt`) and the goal, `W(rhand_g) * grip_place`.
  - Where the goal lies beyond the off arm's limit from lbicep_g, the weapon hand gives way. rhand_g moves toward
    the left shoulder by the excess, along the shoulder-to-goal line, keeping its world turn
    (`SolveTwoBone` on the right arm, `kLimbDamping`). The goal moves with it.
  - Then the left arm: the goal with `W(rhand_g) * grip_turn` (`kLimbDamping`; the limit keeps it short of the
    damped zone).
  - The arms' local turns are written back per frame, each on the same side as the one before.
- Report: `grip`, the weapon hand's largest move and its time (a notice), and `left_hand`, any shortfall left.
- A `left_hand` IK target is skipped under the grip (the grip wins, documented).
- `enum class WeaponGrip { kOneHand, kTwoHand, kPolearm }`:
  - `VariantName(name, WeaponGrip, built)`: kPolearm gives `_pl`, else `_2h`, else the name; kTwoHand gives `_2h`,
    else the name; kOneHand gives the name.
  - The bool form delegates to it.
- Tests:
  - `AnimGripWeaponHandGivesWay`: a right arm out of the left's reach. Every frame the left hand is on the grip
    (1 mm, 0.5 deg) and the left arm's span is within its limit. The right hand keeps its world turn and moves
    toward the left shoulder. The report names `grip`.
  - `AnimGripUntouchedWhereItReaches`: frames the left arm reaches within its limit keep the right arm's keys of
    the clip without the grip.
  - `AnimGripHoldsThroughTheBlends`: blend_in / blend_out over 0; every frame on the grip.
  - `AnimGripEndsOnTheStance`: a stance with the off arm straighter than 95%; the first and last frames are the
    stance's.
  - `AnimGripSkipsALeftHandTarget`: the clip is the grip's alone; no `left_hand` target shortfall.
  - `AnimVariantName`, extended to the grip kinds.
- Breaks: the give-way off, the move without the turn kept, the limit without the start pose's span, the pass on one
  path only, the target not skipped, `_pl` without its `_2h` fallback.

### Task 5: The bench makes both versions

**Files:** `mocap_bench/public/core/project.js` (`versionName(name, existing, kind)` replacing `twoHandedName`;
`VERSIONS = { twohand: { suffix: "_2h", stance: "2hreadyr" }, polearm: { suffix: "_pl", stance: "plreadyr" } }`),
`core/gear.js` (plreadyr's default: a halberd), `inspector.js` (two buttons; the hint says the game plays a version
where it asks for one - Rage does), `app.js` (`makeVersion(kind)`; a failed rebase says so; the unsaved-edit word says
they go into the version), the readout and Save (`grip`: "the weapon hand comes in up to N cm for the grip at T s";
`left_hand` under the grip: "the left hand can't reach the grip by N cm", not a target's).
- Checks:
  - `versionName` replaces `twoHandedName`.
  - `gripAssembled` is reworked: every frame on the grip within 1 cm and the turn within 1 deg, the left arm's span
    within its limit + 1 cm. Frames whose clip without the grip already held the left hand within the limit keep
    the right hand (1 mm). Run on 2hreadyr seamed, on 2hreadyr with ends off and blends 0.2 / 0.3, and on plreadyr.
  - `gripNotice`: Save names the weapon hand's move.
  - browser `twoHandedVersion` makes the polearm version too (plreadyr, `_pl`, a halberd).
- The helper is rebuilt for the checks; the installed one is put back for the owner.

### Task 6 (the game session's): Rage passes the grip kind

`rules_barbarian.cpp` calls `AnimationVariant("n5_rage", kind)`: WeaponWield 4 gives kPolearm, a standard weapon
larger than its wielder kTwoHand, else kOneHand. Sent to the game session after Task 4's commit.

---

## Addendum 2 (2026-11-02): the weapon turns first; the arms solve from the start pose (core 0.60.6)

**Why:** the second final review (ledger "Final 2:") found, on n5_rage through 0.60.4:
- **C1:** the off arm flips at the strike on the polearm version (lbicep up to 179 deg/frame; the hands 13-38 cm apart between frames), and the weapon elbow pops 12-25 cm/frame on the two-handed one. Both arm solves start from the performance's arms, which are ill-conditioned for 20-100 cm moves.
- **I1:** the left elbow sits 2-4 cm off the start pose at the ends.
- **I2:** the bench's preview cache ignores the grip.
- **I3:** no test covers the grip on the IK path.

The owner picked "p2" (the weapon turns first), then t3 (the turn shared between the forearm's roll and the wrist) after the measurements in the ledger.

### Task 7: The grip's turn and arm references (core 0.60.6)

`hold_grip`, per frame, where the goal lies beyond the off arm's limit (`excess > 1e-4`):
1. **The weapon turns first.** Along the shortest turn that carries the grip toward the left shoulder (about `d x e`, with d the grip's direction from the hand and e the shoulder's), as far as needed but no further than its split about the forearm's axis allows: twist (the forearm's roll) <= 80 deg (`kGripRoll`), swing (the wrist) <= 60 deg (`kGripWrist`).
2. **The hand moves only for the rest**, toward the shoulder, with the turned weapon. The right arm solves from a reference blended from the performance's arm toward the start pose's (carried on the frame's torso) by how far the hand moves (`smoothstep(move / (0.3 x arm length))`), and bends the way that reference does.
3. **The twist goes to the forearm's roll** (rforearm_g turns about its own axis); the wrist keeps only the swing.
4. **The left arm solves from the start pose's left arm** carried on the frame's torso, bending the way it does (no swivel: the ends are the start pose exactly).

Tests:
- `AnimGripTurnsBeforeItMoves`: frames turning alone covers keep the hand's place (1 mm); the turn stays within the caps; the hand moves only past them.
- `AnimGripForearmTakesTheRoll`: the wrist (the hand against the forearm) differs from the take's by at most the swing cap.
- `AnimGripArmsTurnSmoothly`: the four arm bones step no faster than the take's plus 20 deg/frame; between frames the grip holds within 3 cm.
- `AnimGripElbowsOnTheStanceAtTheEnds`: a stance whose elbow bends sideways; both elbows on the stance at the ends (1 mm).
- `AnimGripOnTheIkPath`: a hips target; every frame on the grip.
- `AnimGripThroughALoop`: an in-place loop's crossfade.

`AnimGripWeaponHandGivesWay` now asserts the turn's caps instead of a kept turn.

### Task 8: The bench reads it

- `pipeline.js`: the clip cache keys on the grip (I2), with a `pipeline_check` case.
- `gripAssembled`:
  - The wrist within the swing cap.
  - The arm bones' steps within the plain clip's plus 25 deg/frame.
  - Between frames the grip within 3 cm.
- The browser readout check names the grip only as a notice (M6).

---

## Addendum 3 (2026-11-02): the weapon held by the body, with a follow share

**Why:** the owner's test of 0.60.6: "the pole arm flips in the middle of the animation and then flips again in the same
direction to finish". Measured: the weapon's direction follows the one-handed take's fist. On a_dfa the polearm turns up
to 136 deg from its start direction against the torso, and the fist itself 167. A fist's turn is not a weapon's.
Owner: "go with f1".

### Task 9: The follow share (core)

- **Recipe:** `grip_follow = <0..1>`. Absent means 1 (the take's hand: older recipes unchanged). Written only when the
  grip is on and the share is below 1.
- **`hold_grip`, per frame, before the turn and the move:** the weapon hand's target against the torso is the start
  pose's place and turn, mixed toward the take's by the share (lerp for the place, slerp for the turn). The right arm
  solves to it from its arm on the frame before, bending toward the start pose's bend. At a share of 1 it isn't
  touched. The turn, the move and the left arm then run from that pose as before.
- **Tests:**
  - `AnimGripFollowNothingHoldsTheWeapon`: share 0; the right hand on the start pose's place and turn against the torso
    on every frame; the left hand on the grip.
  - `AnimGripFollowHalf`: share 0.5; where nothing else gives way, the hand halfway in place and turn.
  - `AnimGripFollowAllIsTheTake`: share 1, or none, gives the same keys.
  - `AnimRecipeGripFollowRoundTrip`.

### Task 10: The bench's follow slider

- **project.js:** `clip.gripFollow` (0..1, default 0.5), ordered after `grip`; `VERSIONS` gain `follow`
  (two-handed 0.5, polearm 0.25), set by `makeVersion`.
- **recipe.js:** writes `grip_follow`.
- **inspector.js:** the slider "weapon follows the arms (%)", shown with the grip.
- **pipeline.js:** the cache keys on it.
- **Checks:** `gripFollowDefault`, `gripFollowWritten`; `gripAssembled`'s cases at share 1 and a polearm case at 0.25
  (the weapon's direction within the share of the take's); the browser version opens with its share.
