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
