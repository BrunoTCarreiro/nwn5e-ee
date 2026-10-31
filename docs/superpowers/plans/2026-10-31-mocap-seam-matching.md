# Mocap Bench Seam Matching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A mocap clip starts and ends exactly on its start pose (the stance) and plays as performed in between (nwn5e-ee #100).

**Architecture:** All in the bench's solve (`solveTake`), baked into the recipe's tracks like the locks, the sink and the air legs (#97, #99). (1) After the root positions and before the lock pass, the upper body's and the root's gap to the stance at the first and last frames is added back as an offset that fades: out by a little before the hit, in after the recover moment. (2) In the lock pass, the contacts that cover the first or last frame lock on the stance's foot spots and turn, when near. The page turns it on per clip ("start and end on the stance", on by default); while it's on the recipe writes no blends, since the fade replaces the crossfade.

**Tech Stack:** Plain JS (UMD modules under `public/core`), Node checks (`checks/*.js`), headless-Chrome `browser_check.mjs`; the helper (`anim_helper`, the game's own assembler) for the clip.

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#100 (the decision, quoted, and the measurements); the owner's conversation of 2026-10-31 (a punch and a two-handed shove: the fade done before the strike; a lead-in; a stepped shove's last foot reported, not slid).

## Global Constraints

- Bench only: `nwn5e-tools/mocap_bench`. No game code (`nwn5e-ee/src`), no DLL, no kVersion bump, no install.
- `SEAM_FADE = 0.3` s, `SEAM_LEAD = 0.1` s, `SEAM_SHORT = 0.15` s, `SEAM_NEAR = 0.25` m (the doll's world); each fade window is at most half the kept part and at least 2 frames.
- `SEAM_BONES` (the faded parts) = `["pelvis_g", "torso_g", "neck_g", "head_g", "lbicep_g", "lforearm_g", "lhand_g", "rbicep_g", "rforearm_g", "rhand_g"]`, plus the root's position. Never the legs: their seam is the feet's spots and the #99 posture.
- `solveTake` with no `seam` option behaves exactly as today (the parity runs pass none).
- Edits go through anchored Python scripts in the job's tmp dir, run with `python -I`; every anchor asserted, line endings kept. Never `git add -A`; one git command per call; commit with `git commit --only <paths> -F -`; messages end with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
- Every new test fails first; after each task, deliberate breaks of its rules are caught (a breaks script like `floor_breaks.py`).
- Done means `node checks/run.js` 14/14 and `node mocap_bench/browser_check.mjs` all passed.

## Review Focus

1. A hit or recover moment outside the kept part, or none: the fades are the full 0.3 s, no NaN (Task 1, `seamWindowsClamp`).
2. A kept part shorter than 0.6 s: each window is half of it, and both ends are still the stance's exactly (Task 1, `seamWindowsClamp`).
3. A foot in the air at an end (a jump start): its leg and lock are left alone, and no "far" line (Task 2, `airFootAtEndUntouched`).
4. An "inplace" clip (a walk): the feet stay as performed; only the fade applies (Task 2, `inplaceFeetAsPerformed`).
5. A pose key at an end: the seam sits in the capture's track, so the key turns that end from the stance (the owner's edit wins) (Task 3, `keyOnSeamTurnsFromStance`).

## Rulings made in this plan (the owner sees them at hand-over)

- In the bench solve, not the core: the core's pose keys and IK targets act on top, so a key or target at an end moves that end. Cost: no guarantee once the owner keys an end.
- On by default, old projects too; while on, the recipe writes `blend_in = 0` and `blend_out = 0`. Cost: a project saved again loses its crossfades (turning the setting off brings them back).
- The feet move to the stance's spots only with place "return"; "inplace" clips keep the performer's feet. Cost: a walk's ends don't match a standing stance's feet (they shouldn't).
- Loops are matched too: both ends on the stance; the loop blend then joins equal poses. Cost: a walk cycle needs the setting off.

---

### Task 1: The fade at the ends (solve)

**Files:**
- Modify: `nwn5e-tools/mocap_bench/public/core/solve.js` (`solveTake`, the constants, the api)
- Test: `nwn5e-tools/mocap_bench/checks/solve_check.js`

**Interfaces:**
- Consumes: `solveTake(take, bones, options)` as today; `R.stanceLocal`, `home` (the stance's root) inside it; `smoothstep`, `qmix`, `qmul`, `qconj` from math.js.
- Produces:
  - option `seam: null | { hit: number|null, recover: number|null }` (take seconds);
  - result field `seam: null | { start: { from, to, short }, end: { from, to, short }, far: [] }` (take seconds; `far` filled by Task 2);
  - exports `S.SEAM_BONES` (the names above) and `S.SEAM = { FADE: 0.3, LEAD: 0.1, SHORT: 0.15, NEAR: 0.25 }`.

- [ ] **Step 1: Write the failing tests** (in `solve_check.js`, after `postureEases`, inside the same `try`; `ready` = nwreadyr as already defined there)

```js
  // Seam matching (nwn5e-ee #100): the clip starts and ends on the start pose exactly - the gap faded out by a
  // little before the hit, back in after the recover moment - and the middle is the solve's own. No locks here:
  // the fade alone (Task 2 is the feet).
  const none = { left: [], right: [] };
  const readyHome = S.stanceFrame(ready, bones).home;
  const seamBones = S.SEAM_BONES.map(at).filter((i) => i >= 0);
  const gapTo = (sol, k) => Math.max(...seamBones.map((i) => MM.qangle(sol.local[k].get(i), sol.R.stanceLocal[i]) * 180 / Math.PI));
  const rootGap = (sol, k) => MM.len(MM.sub(sol.root[k], readyHome));
  const loose = solve(plain, { stance: ready, locks: none });
  const seamed = solve(plain, { stance: ready, locks: none, seam: { hit: null, recover: null } });
  const lastK = seamed.count - 1;
  check("seamStartsAndEndsOnStance", gapTo(loose, 0) > 10 && gapTo(seamed, 0) < 0.1 && gapTo(seamed, lastK) < 0.1 &&
    rootGap(seamed, 0) < 0.001 && rootGap(seamed, lastK) < 0.001, ...);
  // Between the windows the solve is untouched: every seam bone and the root as without the seam.
  check("seamKeepsTheMiddle", /* frames with seamed.seam.start.to < t < seamed.seam.end.from: > 10 of them, qangle < 1e-6, root < 1e-9 */);
  // A hit 0.3 s in: the start's fade ends 0.1 s before it, and at the hit the arms are the performance's.
  const H = kept[0] + 0.3, hitSeam = solve(plain, { stance: ready, locks: none, seam: { hit: H, recover: null } });
  const kH = Math.round(H * plain.fps) - hitSeam.a;
  check("seamFadesBeforeHit", Math.abs(hitSeam.seam.start.to - (H - 0.1)) < 0.5 / plain.fps && gapTo(hitSeam, 0) < 0.1 &&
    ["rbicep_g", "rforearm_g", "rhand_g", "lbicep_g", "lforearm_g", "lhand_g"].map(at).every((i) => MM.qangle(hitSeam.local[kH].get(i), loose.local[kH].get(i)) < 1e-6), ...);
  // Too close to the ends: 2 frames, marked short; with no moments, the full 0.3 s, not short.
  const tight = solve(plain, { stance: ready, locks: none, seam: { hit: kept[0] + 0.12, recover: kept[1] - 0.05 } });
  check("seamShortFade", Math.abs(tight.seam.start.to - tight.seam.start.from - 2 / plain.fps) < 1e-9 && tight.seam.start.short && tight.seam.end.short &&
    Math.abs(seamed.seam.start.to - seamed.seam.start.from - 0.3) < 1e-9 && !seamed.seam.start.short && !seamed.seam.end.short, ...);
  // Moments outside the kept part are ignored (0.3 s); a 0.4 s kept part halves each window, both ends still exact.
  const outside = solve(plain, { stance: ready, locks: none, seam: { hit: kept[1] + 5, recover: -1 } });
  const shortKept = [kept[0], kept[0] + 0.4], brief2 = solve(plain, { stance: ready, kept: shortKept, locks: none, seam: { hit: null, recover: null } });
  check("seamWindowsClamp", Math.abs(outside.seam.start.to - outside.seam.start.from - 0.3) < 1e-9 && Math.abs(outside.seam.end.to - outside.seam.end.from - 0.3) < 1e-9 &&
    Math.abs(brief2.seam.start.to - brief2.seam.start.from - (brief2.count - 1) / plain.fps / 2) < 1e-9 && gapTo(brief2, 0) < 0.1 && gapTo(brief2, brief2.count - 1) < 0.1, ...);
```

(The `...` are the detail strings: the numbers each check compares, in the file's style.)

- [ ] **Step 2: Run them to see them fail**

Run: `cd nwn5e-tools/mocap_bench; node checks/solve_check.js`
Expected: FAIL (`S.SEAM_BONES` undefined, or the five seam checks), the earlier checks still ok.

- [ ] **Step 3: Implement the fade in `solveTake`** (`public/core/solve.js`), right after `solved.forEach((sv, k) => { sv.root = rootPosition(...) })` and before the locks block; `opt.seam` defaults to `null`. Times are take seconds, `s0 = a / fps`, `e1 = (a + count - 1) / fps`:

```
half = (e1 - s0) / 2;  hit, rec = the moments when inside (s0, e1), else null
startTo = max(s0 + 2/fps, s0 + min(FADE, half, hit !== null ? hit - LEAD - s0 : Infinity))
endFrom = min(e1 - 2/fps, e1 - min(FADE, half, rec !== null ? e1 - rec : hit !== null ? e1 - (hit + LEAD) : Infinity))
ws(t) = t >= startTo ? 0 : smoothstep(1 - (t - s0) / (startTo - s0));   we(t) = t <= endFrom ? 0 : smoothstep((t - endFrom) / (e1 - endFrom))
per SEAM bone i: Os = stanceLocal[i]·conj(local_0[i]), Oe = stanceLocal[i]·conj(local_last[i]) (from the unfaded solve)
local'_k[i] = qmix(I, Os, ws)·qmix(I, Oe, we)·local_k[i];   root'_k = root_k + ws·(home - root_0) + we·(home - root_last)
short = window length < SHORT
```

Return `seam: { start: { from: s0, to: startTo, short }, end: { from: endFrom, to: e1, short }, far: [] }` (or `null` without the option). Export `SEAM_BONES` and `SEAM`.

- [ ] **Step 4: Run them to see them pass**

Run: `node checks/solve_check.js`
Expected: all passed (the five new ones included).

- [ ] **Step 5: Commit**

`git commit --only mocap_bench/public/core/solve.js mocap_bench/checks/solve_check.js -F -` with `feat: the mocap bench's clips fade onto the start pose at their ends (nwn5e-ee #100, 1/3)`.

### Task 2: Feet start and end on the stance's spots (solve)

**Files:**
- Modify: `nwn5e-tools/mocap_bench/public/core/solve.js` (the lock pass in `solveTake`)
- Test: `nwn5e-tools/mocap_bench/checks/solve_check.js`

**Interfaces:**
- Consumes: Task 1's `opt.seam` and result `seam`; the lock pass's `stood` (the stance's first-frame pose, #97), `p0`/`p1`, `spot`, `turn`.
- Produces: `result.seam.far: [{ side: "left"|"right", end: "first"|"last", d: number (m) }]`.

- [ ] **Step 1: Write the failing tests** (after Task 1's)

```js
  // Feet (nwn5e-ee #100): a contact covering the first or last frame locks on the stance's foot spot and turn,
  // when its own spot is within 0.25 m (both ends of a contact over the whole clip); with the fade, the doll is
  // the stance at both ends - feet, legs and all.
  const standReady = L.worldPose(bones, ready, 0);
  const footOff = (sol, k, side) => { const w = S.posed(sol, bones, k)[feet[side]], s = standReady[feet[side]];
    return { xy: Math.hypot(w.p[0] - s.p[0], w.p[1] - s.p[1]), z: Math.abs(w.p[2] - s.p[2]), turn: MM.qangle(w.q, s.q) * 180 / Math.PI }; };
  const legGap = (sol, k) => Math.max(...["lthigh_g", "lshin_g", "rthigh_g", "rshin_g"].map(at).map((i) => MM.qangle(sol.local[k].get(i), sol.R.stanceLocal[i]) * 180 / Math.PI));
  const planted = solve(plain, { stance: ready, locks, seam: { hit: null, recover: null } }), plantedLoose = solve(plain, { stance: ready, locks });
  check("feetOnStanceSpots", ["left", "right"].every((s) => [0, planted.count - 1].every((k) => { const o = footOff(planted, k, s); return o.xy < 0.002 && o.z < 0.002 && o.turn < 0.5; })) &&
    footOff(plantedLoose, 0, "right").xy > 0.02 && planted.seam.far.length === 0 && legGap(planted, 0) < 5 && gapTo(planted, 0) < 0.1, ...);
  // A right foot 40 cm out to the side all take: its spot stays (as without the seam) and is reported, both ends.
  const wide = /* plain with the right ankle, heel and toe points 0.4 m along (right_hip - left_hip) normalized, the knee by 0.2 */;
  const wideSeam = solve(wide, { stance: ready, locks, seam: { hit: null, recover: null } }), wideLoose = solve(wide, { stance: ready, locks });
  check("farFootStays", MM.len(MM.sub(S.posed(wideSeam, bones, wideSeam.count >> 1)[feet.right].p, S.posed(wideLoose, bones, wideSeam.count >> 1)[feet.right].p)) < 0.002 &&
    ["first", "last"].every((e) => wideSeam.seam.far.some((f) => f.side === "right" && f.end === e && f.d > 0.25)) && !wideSeam.seam.far.some((f) => f.side === "left"), ...);
  // The right foot in the air at both ends (planted only in the middle): no far line, its middle lock as without the seam.
  const midOnly = { left: [kept], right: [[kept[0] + 0.4, kept[1] - 0.4]] };
  const air = solve(plain, { stance: ready, locks: midOnly, seam: { hit: null, recover: null } }), airLoose = solve(plain, { stance: ready, locks: midOnly });
  const kMid = Math.round((kept[0] + kept[1]) / 2 * plain.fps) - air.a;
  check("airFootAtEndUntouched", !air.seam.far.some((f) => f.side === "right") &&
    MM.len(MM.sub(S.posed(air, bones, kMid)[feet.right].p, S.posed(airLoose, bones, kMid)[feet.right].p)) < 0.002, ...);
  // "inplace": the feet as performed (as without the seam); the fade still applies.
  const inSeam = solve(plain, { stance: ready, locks, place: "inplace", seam: { hit: null, recover: null } }), inLoose = solve(plain, { stance: ready, locks, place: "inplace" });
  check("inplaceFeetAsPerformed", Math.abs(footOff(inSeam, 0, "right").xy - footOff(inLoose, 0, "right").xy) < 0.002 && gapTo(inSeam, 0) < 0.1, ...);
```

- [ ] **Step 2: Run them to see them fail**

Run: `node checks/solve_check.js`
Expected: FAIL `feetOnStanceSpots` and `farFootStays` (`airFootAtEndUntouched` and `inplaceFeetAsPerformed` may pass already: they guard what must not change; the Step 6 breaks prove them).

- [ ] **Step 3: Implement in the lock pass**: when `opt.seam && opt.place === "return"`, a lock with `p0 === 0` is "first" and one with `p1 === count - 1` is "last" (one lock can be both). For such a lock, `d` = the horizontal distance from its spot to the stance's foot (`stood.P[foot]`). If `d <= SEAM.NEAR`, then its spot's x and y become the stance's (the height keeps the floor rule) and its held turn becomes `stood.W[foot]` (when `holdTurn`). Otherwise push `{ side, end, d }` to `seam.far`, once per end. If `legGap(planted, 0) < 5` fails because the IK's knee plane differs from the stance's, rule on it in the ledger before changing anything.

- [ ] **Step 4: Run them to see them pass**

Run: `node checks/solve_check.js`, then `node checks/parity_check.js`
Expected: all passed in both.

- [ ] **Step 5: Commit**

`feat: the mocap bench's first and last contacts lock on the start pose's feet (nwn5e-ee #100, 2/3)`.

- [ ] **Step 6: Breaks** for Tasks 1-2, each caught: the fade not applied; the legs faded too; the end offset taken from frame 0; the hit's lead ignored; the half-clip clamp dropped; the spots moved regardless of distance; the spots moved under "inplace"; the turn not held. Record the results in the ledger.

### Task 3: The project, the recipe, the pipeline, the Save report and the page

**Files:**
- Modify: `nwn5e-tools/mocap_bench/public/core/project.js` (`CLIP_DEFAULTS`, `ORDER.clip`, `validate`), `public/core/recipe.js` (`build`), `public/core/pipeline.js` (the solve call, its key, `saveIssues`), `public/app.js` (`perBody`, a `seamGap` hook), `public/inspector.js` (the Clip tab), `README.md`
- Test: `checks/project_check.js`, `checks/recipe_check.js`, `checks/pipeline_check.js`, `browser_check.mjs`

**Interfaces:**
- Consumes: Task 1-2's `seam` option and result.
- Produces:
  - `project.clip.matchStance: boolean` (default `true`; `validate` makes anything but `false` true; `ORDER.clip` ends `..., "transtime", "matchStance"`);
  - `bench.seamGap(T)` → the largest angle (deg) over `S.SEAM_BONES` between the result's local turn at take time T and the stance's (`result.solved.frame.S`).

- [ ] **Step 1: Write the failing tests**
  - `project_check.js` `matchStanceDefault`: `newProject(...).clip.matchStance === true`. Validated: a clip with no `matchStance` is `true`, `false` stays `false`, and the clip's keys end with `transtime,matchStance`.
  - `recipe_check.js` `seamWritesNoBlends`: a project with `blendIn 0.2, blendOut 0.3`. With `matchStance: true`, its recipe text has `blend_in = 0` and `blend_out = 0` lines; with `false`, it has `blend_in = 0.2` and `blend_out = 0.3`.
  - `pipeline_check.js` `seamFromProject`: the fixture project with `matchStance: true` and `moments.hit = kept[0] + 0.3` gives `r.solved.solved.seam.start.to` within half a frame of `hit - 0.1`. Moving the hit 0.1 s gives solve +1. `matchStance: false` gives `seam === null`.
  - `pipeline_check.js` `keyOnSeamTurnsFromStance` (Review Focus 5): `matchStance: true` plus a push key `torso_g` at `kept[0]`, turn `[0, 0, 20]`, ease 0.2. In the assembled clip, the torso's local turn at clip time 0 is 20 ± 0.1 degrees from `frame.S` of torso_g.
  - `pipeline_check.js` `saveIssuesSeam` (beside the existing saveIssues cases): all 8 bodies with `seam: { start: { from: 0, to: 0.0667, short: true }, end: { from: 1.7, to: 2, short: false }, far: [{ side: "left", end: "first", d: 0.32 }] }`. The lines include exactly
    - `left foot starts 32.0 cm from the stance's spot: left where it was, so the clip's first frame doesn't match the stance there (all 8 bodies)`;
    - `the clip's start eases into the performance in only 0.07 s: start the kept part 0.2-0.3 s before the strike (all 8 bodies)`.

    An end-short line reads `the clip's end eases back to the stance in only <s> s: keep 0.2-0.3 s after the recovery in the kept part`.
  - `browser_check.mjs` `seamToggle` (after `contactHeight`, before any IK or pose step), on the Clip tab:
    - On (the default): `[data-field="matchStance"]` is checked, the result's recipe has `blend_in = 0`, and `bench.seamGap(kept0) < 1`.
    - Click it off: the recipe has `blend_in = <the project's blendIn>` and `bench.result().solved.solved.seam === null`.
    - Ctrl+Z turns it back on.

- [ ] **Step 2: Run them to see them fail**

Run: `node checks/project_check.js; node checks/recipe_check.js; node checks/pipeline_check.js` (from `mocap_bench`); `node mocap_bench/browser_check.mjs` (from `nwn5e-tools`)
Expected: each new check FAILs, everything else ok.

- [ ] **Step 3: Implement**
  - **project.js:** `matchStance` as in Interfaces.
  - **recipe.js `build`:** `blendIn`/`blendOut` are written as 0 when `clip.matchStance`.
  - **pipeline.js:**
    - the solve call passes `seam: clip.matchStance ? { hit: moments.hit, recover: moments.recover } : null`, and the solve's memo key gains that same value;
    - `saveIssues` reads each body's `seam`: a far foot is one group per side and end, its value `d`; a short window is one group per end, its value `SEAM.FADE - length`. The copy is as in Step 1.
  - **app.js:** `perBody` entries carry `seam: r.solved.solved.seam || null`; add the `seamGap` hook.
  - **inspector.js, the Clip tab:** a checkbox field `ends`, `data-field="matchStance"`, labelled "start and end on the stance". Its help: "The first and last frames are the stance's exactly; the performance fades in after the start and back out before the end, timed by the hit and the recovery. Off: the capture's own ends, joined by the blends." While it's on, the two blend inputs are disabled and their help says "off while the ends match the stance".
  - **README.md:** in step 4 (the clip), one paragraph saying the same.

- [ ] **Step 4: Run everything**

Run: `node checks/run.js` (14/14) and `node mocap_bench/browser_check.mjs` (all passed).

- [ ] **Step 5: Breaks**: `matchStance` not defaulted; the blends still written; the seam missing from the solve's key (a hit move doesn't re-solve); the Save report's far line unwritten; the checkbox not wired. Each caught.

- [ ] **Step 6: Measure the owner's saaaaa** (read-only, like `seam_gap.js`): pose keys off, under pause1, nwreadyr and 1hreadyr. Expect the first and last frames' upper body within 1 degree of the stance, the feet on its spots, and the legs' gap printed. Also the kick's straightest knee and highest ankle under nwreadyr, still 2 degrees and 96 cm (#99 kept).

- [ ] **Step 7: Commit and record**
  - nwn5e-tools: `feat: the mocap bench's seam matching on the page, the recipe and the Save report (nwn5e-ee #100, 3/3)`.
  - nwn5e-ee: a TOOLS-PLAN.md entry (`git commit --only docs/TOOLS-PLAN.md`).
  - Then the #100 comment with the test matrix, `needs-test`, and the board set to Needs test.
