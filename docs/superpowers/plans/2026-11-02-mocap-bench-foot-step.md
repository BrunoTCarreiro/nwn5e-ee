# Mocap Bench Foot Step Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A planted foot never slides at a clip's ends. A first or last contact within 5 cm of the start pose's foot spot shifts onto it; a farther one steps onto it (lifts, moves, plants) inside the seam's end window. Save names each step.

**Architecture:**
- The solve's contact locks (`mocap_bench/public/core/solve.js`, the `locks` block) replace #100's 25 cm move-or-leave cutoff.
- A step is a run of held ankle targets along an arc over the seam's start or end window (`startTo` / `endFrom`), with the turn blended from the landing's to the stance's.
- `seam.far` becomes `seam.steps`, and Save's line follows (`pipeline.js` `saveIssues`).

**Tech Stack:** Node checks; the bench's UMD core.

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#114: its analysis comment and the decision (owner, 2026-11-02: "Go with your recommendations" = the step rule).

## Global Constraints

- "A planted foot never slides": on every frame, a foot whose height above the floor is under 1 cm moves less than 0.5 cm a frame horizontally while it's in a contact or a step.
- Shift threshold `SEAM.SHIFT = 0.05` m. Above it, step. The step grows with the gap, so there's no flip point. The owner's rule: general for every clip, start pose and body.
- The cause, measured 2026-11-02: the game's loop crossfade (`loop` on, 0.4 s) pulls a last plant left off the stance toward the first frame's foot. a_fa's n5_rage foot is held still by the solve (0.0 cm) and dragged 25.5 cm by the crossfade; with loop off, 0.2 cm. With the step rule, both ends are on the stance's spot, so the crossfade moves nothing.
- `matchStance` off: no step (the ends needn't match). Loop with `matchStance` off is out of scope (Ruling).

## Review Focus

1. A gap just over 5 cm and one just under: both end with the foot on the stance's spot, and neither slides.
2. A contact a few frames off an end (its ease reaching the end frame, #100's review case): steps or shifts like one covering it.
3. A step window shorter than 0.15 s (a hit near the end): the foot still reaches the spot without sliding. Save already notes the short fade.
4. Both feet stepping at the same end: each its own arc.
5. `place: "inplace"` (the ground moving under the body): the step's targets shift with the ground like a lock's.

---

### Task 1: The step rule in the solve

**Files:**
- Modify: `mocap_bench/public/core/solve.js`: `SEAM` gains `SHIFT: 0.05`; the `locks` block's end handling; `seam.steps` replaces `seam.far`.
- Test: `mocap_bench/checks/solve_check.js`. Rework `feetOnStanceSpots`, `nearEndContactsHold` and `farFootStays`; add the new checks.

**Interfaces:**
- Produces:
  - `seam.steps: [{ side: "left"|"right", end: "first"|"last", d }]`, where d is the gap in metres, for each contact that steps. `seam.far` is removed.
  - A step's frames hold the ankle target on the arc.

- [ ] **Step 1: Failing checks in solve_check** (fixture `footOut(m)` moves the right foot m out after 0.6 s; `ready` stance; seam on). A frame "slides" when the foot is under 1 cm above the floor and moves over 0.5 cm horizontally since the frame before.
  - `shiftUnder5cm`: `footOut(0.03)` gives `steps` empty; the right foot is on the stance's spot (< 2 mm) at the first and last frames; no frame slides.
  - `nearStepsNotShifts`: `footOut(0.1)` gives `steps` with right/first and right/last, d ≈ 0.1 (± 0.03). The foot is on the spot at both ends (< 2 mm, < 0.5 deg). It lifts (≥ 2 cm above the floor) inside both windows. No frame slides.
  - `farFootSteps` (replaces `farFootStays`): `footOut(0.7)` gives `steps` at both ends with d > 0.25. Mid-clip the foot is where it is without the seam (< 2 mm). It ends on the spot; no frame slides.
  - `noFlipAtTheCut`: `footOut(0.045)` and `footOut(0.055)` both end on the spot (< 2 mm) and neither slides. The first shifts and the second steps.
  - `nearEndContactsHold` (reworked): the contact 2 frames late and 4 early on `footOut(0.1)` ends on the spot at both ends, and no frame slides.
  - `feetOnStanceSpots` (reworked): as before, but `stray` becomes "no frame slides", and `seam.far` → `seam.steps`.
- [ ] **Step 2: Run** `node mocap_bench/checks/solve_check.js` → the new and reworked checks FAIL.
- [ ] **Step 3: Implement.**
  - **Near** (`d <= SEAM.SHIFT`): today's move onto the spot, extended to the end.
  - **Farther, at the end:** the contact's lock ends at the step's first frame, kE = the frame of `seam.end.from`. Over kE..count-1 the held target follows `spot → at` horizontally by u = smoothstep((k - kE) / (count - 1 - kE)). The height is `lerp(spot.z, at.z, u) + lift · 4u(1-u)`, with `lift = clamp(0.3 · d, 0.03, 0.12)` m. The turn is `qmix(landing turn, stance turn, u)` and the weight is 1.
  - **Farther, at the start:** mirrored over 0..kS (the frame of `seam.start.to`). The foot starts on the stance's spot and the lock begins at kS.
  - **Both:** the steps' targets shift with the ground like a lock's (`sub(target, shift(k))`), and a step's entry wins over an easing lock on the same frame.
- [ ] **Step 4: Run** solve_check → all pass; `node mocap_bench/checks/run.js` → pipeline_check's seam test fails until Task 2 (expected).
- [ ] **Step 5: Commit** `fix: the mocap bench's planted feet step onto the start pose's spot instead of sliding (nwn5e-ee #114, 1/2)`.

### Task 2: Save names the steps

**Files:**
- Modify: `mocap_bench/public/core/pipeline.js` (`saveIssues`: the `seam far` line becomes a step line, ranked by `S.SEAM.SHIFT`).
- Test: `mocap_bench/checks/pipeline_check.js` (`saveIssuesSeam`).

**Interfaces:**
- Consumes: `seam.steps` (Task 1).
- Produces the Save line `"<foot label> steps <cm> onto the stance's spot at the clip's <start|end>"`, e.g. `"left foot steps 32.0 cm onto the stance's spot at the clip's start (all 8 bodies)"`.

- [ ] **Step 1: Failing check:** `saveIssuesSeam` with `seam.steps: [{ side: "left", end: "first", d: 0.32 }]` includes the line above.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** pipeline_check, run.js (15/15) and browser_check (all pass).
- [ ] **Step 5: Commit** `fix: the mocap bench's Save names a foot's step onto the start pose (nwn5e-ee #114, 2/2)`.
