# Mocap Bench: legs from the stance, with a keyframed twist - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An attack clip whose legs are the stance's (first and last frames match it by construction), the take driving
only the upper body, plus a keyframed twist about the vertical (torso all of it, pelvis 35%, the legs solved again).

**Architecture:** Two new assembled-recipe lines - `stance_legs = 1` and `twist <take s> <degrees> <ease s>` - read and
written by src/data/anim_recipe and applied by AssembleAnimClip (src/data/anim_assemble): the legs half held at the
stance's first frame (bound as constant tracks, the root's fold included, so every step sees them), the twist a new
step after the legs' (step 8) and before the hands. The Mocap Bench writes the lines, previews them through its helper
(the same C++), and gets a legs switch, a twist slider that keys at the marker, a twist row and its inspector.

**Tech Stack:** C++ (nwn5e-ee anim: test.bat), JS (nwn5e-tools mocap_bench: checks/run.js, browser_check.mjs), the helper
(anim_helper/build_helper.bat with NWN5E_EE=the anim worktree until the merge).

**Spec:** issue #159 (the owner's words and the agreed design; the conversation 2026-11-12). No separate design doc:
the owner decided in chat ("mostly planted, go with legs from the stance, but just like we have a "lean" slider in the
recipe bench allow for a twist here as well that can be keyframed").

## Global Constraints

- Recipes and projects without the new keys build exactly as before (byte-identical recipes; existing tests green).
- New projects default to legs from the stance; a project saved before has no `legs` and keeps the take's.
- The twist is zero at the clip's first and last frames (the ends stay the stance's).
- The twist's split mirrors the lean's hinge: the torso all of it, the pelvis 35%; positive turns counter-clockwise seen
  from above (to the performer's left); degrees -90..90, ease 0..2 s.
- Commits name #159; git: `git commit --only <paths> -F -`, one command per call, never `git add -A`.

## Review Focus

1. **The root's fold:** pelvis_g (a child of rootdummy) held at the stance's first frame must include the stance's root
   turn folded in (as key_unkeyed_turns does) - else a combat stance's 17-27 degree root turn is lost and the hips face
   off the torso. Test: the held pelvis's world turn equals the stance's first-frame world turn.
2. **A take with leg tracks, foot targets, contacts and leg pose keys** under `stance_legs`: none of them may move the legs.
3. **A twist key near an end:** the twist still reads 0 at frames 0 and n.
4. **A body without torso_g or pelvis_g** (a_da / a_dfa quirks): the twist skips what's missing, no crash.
5. **Old projects:** no `legs` key reads as "take"; their recipes don't change.

---

### Task 1: The recipe lines (C++)

**Files:** src/data/anim_recipe.h/.cpp, tests/data/anim_recipe_test.cpp (or the assembled-recipe test file it uses).

**Produces:** `AnimRecipe::stance_legs` (bool), `struct AnimTwistKey { float t; float degrees; float ease = 0.2f; }`,
`AnimRecipe::twist` (vector, by time); `float TwistAt(const std::vector<AnimTwistKey>&, float T)` - between two keys
their degrees blend by smoothstep, before the first and after the last it eases from 0 over the key's ease (0: a step).

- [ ] Failing tests: round trip of `stance_legs = 1` and two `twist` lines (written after the pose lines, before the keys
  lines; absent when unset - an assembled recipe without them writes back unchanged); refusals naming the line
  (degrees 91, ease 3, a bad number); TwistAt values (at a key, halfway between two, past the last within/after its ease).
- [ ] Implement; test.bat green; commit `feat: assembled recipes - stance_legs and twist keys (#159)`.

### Task 2: The assembly (C++)

**Files:** src/data/anim_assemble.h/.cpp, tests/data/anim_assemble_test.cpp.

- [ ] Failing tests (a synthetic library/stance as the existing assemble tests build):
  - `AnimAssembleStanceLegs`: with leg tracks, a root position track, foot targets and a pelvis pose key in the recipe,
    `stance_legs` leaves rootdummy's place, pelvis_g and the legs at the stance's first frame on every frame (the
    pelvis's world turn = the stance's, the fold included), and torso_g's tracks as without it.
  - `AnimAssembleTwist`: a 30 degree key mid-clip: at the key the torso's world turn about up is 30 degrees from
    without it, the pelvis's 10.5, each foot's world place within 1 mm and its turn kept; frames 0 and n untouched.
- [ ] Implement: under `stance_legs` skip the take's tracks for rootdummy's position and pelvis_g's subtree, the foot and
  hips targets and those parts' pose keys, and bind constant tracks at the stance's first frame (the fold for the
  root's children) so composed_pose sees them; the twist step after `stand`: turn torso_g by w*theta and pelvis_g by
  0.35*w*theta about world up at their joints (w the end fade: 0 at frames 0 and n, smoothstep over the first and last
  0.2 s, shorter clips proportionally), re-solve both legs (SolveTwoBone) to their pre-twist foot places and turns, mark
  the solved bones. Header comment: step 8b.
- [ ] test.bat green, the DLL builds; commit `feat: assembled recipes - legs from the stance and the twist (#159)`.

### Task 3: The bench's project and recipe (JS)

**Files:** mocap_bench/public/core/project.js, recipe.js; checks/project_check.js, recipe_check.js.

- [ ] Failing checks: newProject's clip.legs is "stance"; a parsed project without it reads "take"; clip.twist keys
  validate (degrees -90..90, ease 0..2, t finite) and serialize; recipe.build writes `stance_legs = 1` and the twist lines
  (take time, 4 decimals) exactly as the C++ reads them; legs "take" and no twist write the recipe as before.
- [ ] Implement; checks/run.js green; commit (tools).

### Task 4: The bench's controls (JS)

**Files:** public/inspector.js (clip tab: legs, twist slider), public/timeline.js (the twist row), public/app.js (keying
at the marker, hiding the leg controls, selection/delete/undo), public/index.html/style.css as needed;
browser steps in mocap_bench/browser_check.mjs.

- [ ] Failing browser steps: the legs switch shows on the clip tab, "from the stance" hides the contact lanes and the
  foot / hip drags; the twist slider at a marker adds a key on the twist row (moving it again at the same frame updates
  it), the row's key drags in time and Delete removes it; with a 30 degree key the doll's torso turns at that time
  (through the helper) and the feet stay put.
- [ ] Implement; build the helper from the anim worktree (NWN5E_EE); run.js and browser_check green; commit (tools).

### Task 5: Hand-off

- [ ] Final review (a fresh reviewer, both repos); fix Critical/Important with tests.
- [ ] Test rows (bench + game: a seamless attack with legs from the stance and a twist, `!anim` in game after Save and a
  restart); TOOLS-PLAN entry; #159 comment; "anim ready at X + tools ready at Y" to the game session (core changed: an
  install; the helper rebuilt from main after the merge).
