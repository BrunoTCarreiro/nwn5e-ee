# Mocap Bench Stance Posture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The same animation fits any start pose and any body: the start pose shapes only the clip's ends, and a per-clip "stance posture" amount (default 0%) keeps some of its lean and crouch through the middle when wanted (nwn5e-ee #107).

**Architecture:** In `solveTake`, a second per-frame solve maps the take onto the body's upright skeleton: the rig with no stance, its bind posture and its bind root. The clip's locals and root are blended between that solve (posture 0) and today's stance-mapped one (posture 1), before #100's seam fade. The seam then lands the ends exactly on the stance, as now. The project carries `clip.posture`, the pipeline hands it to the solve, and the Clip tab has a slider.

**Tech Stack:** Plain JS (UMD under `public/core`), Node checks, headless-Chrome `browser_check.mjs`, the helper (the game's assembler).

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#107 (the measurements and the decision: "go for it with your recommendations" - options 1 + 2, default 0%).

## Global Constraints

- Bench only: `nwn5e-tools/mocap_bench`. No game code, no DLL, no install.
- `solveTake`'s `posture` defaults to 1, today's solve, so the parity runs and older callers are unchanged. The project's `clip.posture` defaults to 0 (older projects too), and the pipeline passes it.
- The upright reference is `rig(bones, null)`, with the legs' neutral taken from it and the root's home at the bind root (`R.restPos[rootdummy]`). That's exactly what `solveTake` does with no stance today.
- The blend covers every driven bone's local (nlerp, `qmix(plain, stance, posture)`) and the root (`lerp`). It happens after #99's air-leg blend, which belongs to the stance solve, and before the seam.
- Edits go through anchored Python scripts in the job's tmp dir (`python -I`), every anchor asserted, line endings kept. Never `git add -A`; one git command per call; `git commit --only`; messages end with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
- Every new test fails first; each rule gets a deliberate break that a check catches. Done = `node checks/run.js` 14/14 and `node mocap_bench/browser_check.mjs` all passed.

## Review Focus

1. Posture 0 makes the middle independent of the stance, but the floor and the end contacts still use the stance's feet. Expect the planted legs mid-clip to reach the stance's floor height from an upright body (the sink still works) (Task 1, `postureZeroPlanted`).
2. A posture between 0 and 1 blends locals, not world directions. The arms (absolute in both solves) should stay near the performance's direction; check that a hand's world direction at 0.5 is within a few degrees of both ends' average (Task 1, `postureHalfway`).
3. Changing the posture must re-solve (it belongs in the solve's memo key), and the ghost should follow the clip's settings (Task 2, `postureFromProject`).
4. A stance with no root keys, or `opt.stance` null: posture has no effect (Task 1, `postureZeroIsUpright` covers null).
5. #99's air legs and #101's rebase under posture 0: the air-leg path is skipped (no stance posture to lift out of), and a stance change still rebases keys (S changes) (Task 2: the existing `stanceKeepsPoseKeys` browser step must stay green).

## Rulings made in this plan

- **Default 0 for older projects too:** the owner's n5_rage is what the fix is for. Cost: an older project's middle loses its stance flavour until the slider is raised.
- **The upright reference is the bind pose,** not a stance: no NWN stance is upright on every body (a_fa's pause1 is tilted 10 deg).
- **Posture blends locals:** exact at 0 and 1, approximate in between.

---

### Task 1: The posture blend in the solve

**Files:**
- Modify: `nwn5e-tools/mocap_bench/public/core/solve.js` (`solveTake`)
- Test: `nwn5e-tools/mocap_bench/checks/solve_check.js`

**Interfaces:**
- Consumes: `solveTake(take, bones, options)` as today; `rig`, `legNeutral`, `solveFrame`, `rootPosition`; the frame cache `c` (#99 added `c.upright`).
- Produces: option `posture: number` (0..1, default 1).

- [ ] **Step 1: Write the failing tests** (solve_check, after the seam tests; `ready` = nwreadyr, `none` = no locks)

```js
  // Stance posture (#107): 0 maps the take onto the body's upright skeleton (the rig with no stance) - with no
  // seam, the same solve as with no stance at all; 1 is today's (the default); 0.5 lies between; the stance
  // doesn't stack on a big pose (a bow under 2hreadyr reaches as low as under pause1 at 0, far lower at 1); with
  // the seam, the ends are still the stance's.
  const same = (a, b) => a.count === b.count && a.local.every((m, k) => [...m.keys()].every((i) => MM.qangle(m.get(i), b.local[k].get(i)) < 1e-6)) &&
    a.root.every((r, k) => MM.len(MM.sub(r, b.root[k])) < 1e-9);
  check("postureZeroIsUpright", same(solve(plain, { stance: ready, locks: none, posture: 0 }), solve(plain, { stance: null, locks: none })), ...);
  check("postureOneIsToday", same(solve(plain, { stance: ready, locks: none, posture: 1 }), solve(plain, { stance: ready, locks: none })), ...);
  // halfway: at a mid frame the pelvis's local lies between (each side 40-60% of the whole), the whole > 5 deg;
  // a hand's world direction within 3 deg of halfway between the ends' (Review Focus 2).
  check("postureHalfway", ...);
  // bow: the fixture with everything above the hips turned 50 deg forward about the hips' line over 0.6 s mid-take.
  // Lowest head (head_g): |2hreadyr - pause1| < 0.005 m at posture 0, > 0.10 m at posture 1.
  check("stanceDoesNotStack", ...);
  // with the seam, posture 0 under 2hreadyr: both ends within 0.1 deg of the stance's world pose, the pelvis within 1 mm.
  check("seamWithPostureZero", ...);
  // planted (both feet locked, posture 0, 2hreadyr): the feet on the stance's floor height (2 mm), never leaving their spots (Review Focus 1).
  check("postureZeroPlanted", ...);
```

(The `...` are the detail strings and bodies the comments specify, in the file's style.)

- [ ] **Step 2: Run them to see them fail**

Run: `cd nwn5e-tools/mocap_bench; node checks/solve_check.js`
Expected: FAIL `postureZeroIsUpright`, `postureHalfway` and `stanceDoesNotStack` (posture ignored). The others may pass already: they guard what must hold, and the Step 6 breaks prove them.

- [ ] **Step 3: Implement in `solveTake`**:
  - The option is `posture`, default 1, clamped to 0..1, and only acts when `opt.stance` is set.
  - When it's below 1, `Rp = rig(bones, null)` and `refP = { waist, frames: ref.frames, legs: legNeutral(Rp, refL) }`, with frames memoised in `c.plain` (reset with the cache, as `c.upright`).
  - At posture 0 the stance's frame solve is skipped; at 1 the plain one is skipped.
  - Each driven bone's local is `qmix(plain, stance, posture)`, after #99's air blend on the stance side. The root is `lerp(rootPosition(..., Rp.restPos[rootIndex]), rootPosition(..., home), posture)`.
  - All of this comes before the seam.

- [ ] **Step 4: Run them to see them pass**

Run: `node checks/solve_check.js`, then `node checks/parity_check.js`
Expected: all passed in both.

- [ ] **Step 5: Commit** - `feat: the mocap bench's start pose shapes only the clip's ends; a stance posture amount (nwn5e-ee #107, 1/2)`.

- [ ] **Step 6: Breaks**, each caught:
  - the blend weights swapped;
  - the root not blended;
  - the plain legs' neutral taken from the stance rig;
  - the default set to 0;
  - the plain solve memoised under the stance's cache.

### Task 2: The project, the pipeline and the page

**Files:**
- Modify: `public/core/project.js` (`CLIP_DEFAULTS.posture = 0`, `ORDER.clip` ends `"matchStance", "posture"`, validate clamps to 0..1 with a non-number giving 0), `public/core/pipeline.js` (the solve gets `posture: project.clip.posture ?? 0`, and the solve key gains it), `public/inspector.js` (the Clip tab), `README.md`
- Test: `checks/project_check.js`, `checks/pipeline_check.js`, `browser_check.mjs`

**Interfaces:**
- Consumes: Task 1's `posture` option.
- Produces: `project.clip.posture: number` (0..1).

- [ ] **Step 1: Write the failing tests**
  - `project_check` `postureDefault`:
    - `newProject().clip.posture === 0`;
    - validate turns an absent value into 0, -1 into 0, 2 into 1, and "x" into 0;
    - the clip's keys end with `matchStance,posture`.
  - `pipeline_check` `postureFromProject`, on the fixture project (1hreadyr):
    - posture 0 and posture 1 give pelvis locals more than 2 deg apart mid-clip;
    - each equals `S.solveTake` with that posture and the pipeline's other options;
    - a posture change runs the solve again (solve +1).
  - `pipeline_check`: #99's `startPoseKeepsAirLegs` sets `ready.clip.posture = 1`, because it tests the stance-mapped solve's air legs. That's a ruling.
  - `browser_check` `postureSlider`, on the Clip tab:
    - `[data-field="posture"]` reads 0;
    - setting it to 100 changes the result's recipe and the project's `clip.posture` to 1;
    - Ctrl+Z puts both back.

- [ ] **Step 2: Run them to see them fail**: `node checks/project_check.js; node checks/pipeline_check.js`; `node mocap_bench/browser_check.mjs`. Expected: each new check FAILs.

- [ ] **Step 3: Implement**:
  - **project.js, pipeline.js:** as in Files.
  - **Inspector:** a range input under the stance select, `data-field="posture"`, 0-100 in steps of 5, stored as 0..1. Label "stance posture (%)". Help: "How much of the start pose's lean and crouch holds through the middle. 0: the performance as performed - the start pose only at the ends. 100: the start pose's posture throughout."
  - **README:** step 4's paragraph says the same.

- [ ] **Step 4: Run everything**: `node checks/run.js` (14/14), `node mocap_bench/browser_check.mjs` (all passed).

- [ ] **Step 5: Breaks**:
  - posture not passed to the solve;
  - missing from the solve's key;
  - the default 1 in the project;
  - the slider not wired.

  Each caught.

- [ ] **Step 6: Measure the owner's n5_rage** (read-only, like `rage_fit.js`), with posture 0 and the keys as saved, on a_ba, a_fa, a_da and a_dfa under pause1 and 2hreadyr:
  - the peak head height;
  - the weight off the feet at the peak;
  - the arms' clearance from the legs;
  - the ends against the stance.

  Expect the peak height and the weight close across stances and bodies, no clipping, and the ends within 0.1 deg.

- [ ] **Step 7: Commit and record**:
  - nwn5e-tools: `feat: the mocap bench's stance posture on the page (nwn5e-ee #107, 2/2)`;
  - a TOOLS-PLAN.md entry (`git commit --only`);
  - the #107 comment with the test matrix, `needs-test`, and the board set to Needs test.
