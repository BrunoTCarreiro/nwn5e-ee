# Mocap Bench Gear Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The mocap bench shows a weapon and a shield on the doll as simple shapes measured from the game's own models, offers a gear picker, and warns (timeline spans and Save notes) when gear passes into the body.

**Architecture:**
- A measuring script reads the game's weapon and shield models once and writes a gear table: per item, boxes in its attach frame.
- A core module places the boxes on a posed body and measures their clearance from the body's capsules.
- The stage draws them; the app owns the picker, the live timeline spans and the Save lines.

**Tech Stack:** Node (checks, the measuring script), browser JS (UMD modules under `mocap_bench/public`), the bench's 2D-canvas stage.

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#111 and its decision comment (owner, 2026-11-02: "go with recs" = option (a)).

## Global Constraints

- Bench only (nwn5e-tools/mocap_bench); no change to nwn5e-ee or the game.
- Shapes are "sized and pointed from the game's own models (measured, not guessed)". Only measured numbers are committed; no game file or mesh is copied.
- The gear picker is "not saved into the recipe" (nor into the project).
- Warnings: "a warning (Save note and timeline mark) when the weapon or shield passes into the body".
- Measured attach frames (2026-11-02, mesh boxes through node transforms):
  - Weapons sit on the `rhand` node and point along its +Y: longsword -0.20..0.93 m, greataxe -0.40..0.83 m with a +/-0.24 m head.
  - Shields sit on `lforearm`: large shield y -0.44..0.25, z +/-0.26, x -0.17..0.04.
- The owner's rule (2026-11-02): a fix is a general rule that holds for every animation, start pose and body, not tuned to one clip.

## Review Focus

1. A body library without an attach node (`rhand` / `lforearm`): its gear is skipped, nothing throws.
2. Two-hander chosen while a shield is on: the off-hand select clears and disables. The same applies when a start pose's default is a two-hander.
3. Start-pose change or project open: the picker goes to that start pose's default. Mid-session picks hold until then.
4. The "stance" view (`state.view === "stance"`) draws the gear too. The ghost never carries gear.
5. A gear item whose box lies exactly on a capsule's surface: depth 0 is not an intrusion; only depth > `GE.MIN_DEPTH` (0.01 m) counts.

---

### Task 1: The measured gear table

**Files:**
- Create: `mocap_bench/gear_measure.js`, the measuring script (reads the game install found by `anim_helper/helper.js` `findGameDir()`).
- Create: `mocap_bench/public/core/gear_table.js`, generated, committed, UMD (`root.MocapGearTable` / `module.exports`).
- Test: `mocap_bench/checks/gear_check.js` (new; `checks/run.js` picks up `*_check.js`).

**Interfaces:**
- Produces `gear_table.js`: `{ version: 1, measured: "YYYY-MM-DD", items: { <name>: { slot: "weapon"|"shield", hands: 1|2, blueprint, attach: "rhand"|"lforearm", parts: [{ min: [x,y,z], max: [x,y,z] }] } } }`.
  - Names: `dagger`, `longsword`, `battleaxe`, `greatsword`, `greataxe`, `halberd`, `quarterstaff`, `spear`, `small shield`, `large shield`, `tower shield`.
  - Base items 22, 1, 2, 13, 18, 10, 12, 58, 14, 56, 57.
  - Blueprints: nw_wswdg001, nw_wswls001, nw_waxbt001, nw_wswgs001, nw_waxgr001, nw_wplhb001, nw_wdbqs001, nw_wplss001, nw_ashsw001, nw_ashlw001, nw_ashto001.
- Produces `gear_measure.js` exports: `measure(gameDir) -> table` (and, run directly, writes `public/core/gear_table.js`).

- [ ] **Step 1: Write the failing check** `gear_check.js` with:
  - `tableShape`: every name above present; each has >= 1 part; weapons have 3 parts (bottom, middle, top), shields 1; every min < max on all axes.
  - `measuredAsTheGame`: longsword parts span y -0.20..0.93 (+/-0.01); greataxe's top part is wider in z than its bottom (the head); large shield spans y -0.44..0.25 and z -0.26..0.26 (+/-0.01); greatsword, greataxe, halberd, quarterstaff and spear have `hands: 2`, the rest 1.
  - `tableMatchesGame`: when `findGameDir()` holds `data/nwn_base.key`, `measure()` equals the committed table's items (to 1e-4); else print `skip` and pass.
- [ ] **Step 2: Run it** — `node mocap_bench/checks/gear_check.js` → FAIL (no gear_table.js).
- [ ] **Step 3: Implement `measure(gameDir)`.**
  - Read `nwn_base.key` and its BIFs (KEY V1: 22-byte key entries, `id >>> 20` = BIF, `id & 0xfffff` = index; BIF variable table at +16, 16-byte entries).
  - `baseitems.2da` (type 2017): `ItemClass`, `ModelType`, `WeaponSize`.
  - The blueprint's UTI (2025): `ModelPart1..3`.
  - Each part model (2002) is `<itemclass>_<b|m|t>_<NNN>` for ModelType 2, `<itemclass>_<NNN>` for 0.
  - Each part's box is the union over mesh nodes (node flags `+0x6c & 0x20`) of the mesh header's box (`+0x70+0x14` min, `+0x70+0x20` max), carried through each node's first position (controller 8) and orientation (20) key into the model's frame.
  - `hands: 2` when `WeaponSize >= 3` (a Medium wielder), else 1; shields `hands: 1`.
  - Write the table with 4-decimal numbers and a header comment: generated, how to re-run.
- [ ] **Step 4: Run** `node mocap_bench/gear_measure.js` then the check → all pass.
- [ ] **Step 5: Commit** (nwn5e-tools) `feat: the mocap bench's gear table, measured from the game's weapon and shield models (nwn5e-ee #111, 1/4)`.

### Task 2: Placing gear and measuring its clearance

**Files:**
- Create: `mocap_bench/public/core/gear.js` (UMD: `root.MocapGear` / `module.exports`; requires math.js, gear_table.js).
- Test: `mocap_bench/checks/gear_check.js` (extend).

**Interfaces:**
- Consumes: the gear table (Task 1).
- Produces (`GE` = gear.js):
  - `GE.ITEMS`: the table's names in the order above.
  - `GE.defaultsFor(stanceName) -> { weapon: name|null, shield: name|null }`: 1hreadyr gives longsword + large shield; 2hreadyr gives greataxe and no shield; anything else gives none.
  - `GE.normalize(choice) -> { weapon, shield }`: drops the shield when the weapon has `hands: 2`; unknown names become null.
  - `GE.place(choice, bones, pose) -> [{ item, part, center, axes: [x,y,z unit], half: [hx,hy,hz], corners: [8 points] }]`: world boxes from the attach node's world `{p, q}`; an item whose attach node the body lacks is skipped.
  - `GE.clearance(boxes, bones, pose) -> [{ item, part, region, depth }]`: depth = capsule radius - the distance from the capsule's segment to the box, only when > `GE.MIN_DEPTH` (0.01 m).
  - `GE.CAPSULES`: `{ region, from, to, r }`, with r in metres at a hip height of 1.0, scaled by the body's `IK.hipHeight(bones)`:
    - torso: pelvis_g to neck_g, 0.13
    - head: neck_g to head_g, 0.10
    - each thigh: thigh_g to shin_g, 0.07
    - each shin: shin_g to foot_g, 0.05
    - each upper arm: bicep_g to forearm_g, 0.045

    Exclusions: a weapon skips the right arm's capsules (and both arms' for `hands: 2`); a shield skips the left arm's.
  - `GE.span(choice, bones, clip, length, fps=30) -> { spans: [[a0,a1]], worst: [{ item, region, depth, t }] }`: per frame through `L.worldPose`; spans merge frames with any intrusion; worst is the deepest per item+region.

- [ ] **Step 1: Failing checks:**
  - `defaultsByStance`: the three cases above; `normalize({weapon:"greataxe", shield:"large shield"})` gives no shield.
  - `placedOnTheAttachNode`: at pause1's first frame on a_ba, the longsword's middle-part box center lies within 0.25 m of `rhand`'s world point, and its +Y axis equals `rhand`'s world +Y.
  - `stancesAreClear`: for every body in `["a_ba","a_fa","a_da","a_dfa"]`, each stance's own first frame with its default gear gives an empty clearance. pause1 is checked with longsword + large shield, since nothing may intrude in the idle either.
  - `crossedArmIntrudes`: on a_ba's 1hreadyr pose with the left forearm turned 80 deg in toward the torso (its local turn composed with Rz(80)), the large shield intrudes into `torso` by more than 0.02 m.
  - `missingAttachSkips`: bones without `lforearm` place no shield and don't throw.
- [ ] **Step 2: Run** → FAIL (no gear.js).
- [ ] **Step 3: Implement** gear.js.
  - Point-to-oriented-box distance; capsule-to-box as the minimum over 9 points spaced evenly along the segment.
  - If `stancesAreClear` fails on a radius, lower that region's radius until the stances pass, and record the ruling with the measured margins. NWN's own stances define "clear".
- [ ] **Step 4: Run** → pass; `node mocap_bench/checks/run.js` → all pass.
- [ ] **Step 5: Commit** `feat: the mocap bench places gear on the doll and measures its clearance (nwn5e-ee #111, 2/4)`.

### Task 3: Drawing the gear and the picker

**Files:**
- Modify: `mocap_bench/public/stage.js` (figure items: `figures.gear` boxes as projected edges).
- Modify: `mocap_bench/public/index.html` (two selects in the stage's view bar, beside `ghost` / `points`: `#gear-weapon`, `#gear-shield`).
- Modify: `mocap_bench/public/app.js` (`state.gear`, defaults, `figures.gear` for the result and stance views).
- Test: `mocap_bench/checks/stage_check.js` (extend), `mocap_bench/browser_check.mjs` (new step `gearPicker`).

**Interfaces:**
- Consumes: `GE.defaultsFor`, `GE.normalize`, `GE.place`, `GE.ITEMS` (Task 2).
- Produces:
  - `figures.gear: [{ corners: [8 points], item }]` drawn as the box's 12 edges, at 1.5 px minimum, in `--figure-side`'s shade toward black 0.35, depth-sorted with the figure's items.
  - `window.bench.gear() -> { weapon, shield }` for the browser check.

- [ ] **Step 1: Failing checks:**
  - stage_check `gearEdgesDrawn`: a figure with one gear box adds 12 edge items in the drawn list.
  - browser `gearPicker`, which checks each of these:
    - The project's start pose is pause1, so both selects read `none`.
    - Changing the start pose to 1hreadyr gives longsword + large shield.
    - Choosing greataxe clears and disables the shield select.
    - The project JSON has no gear field.
    - The stance view draws gear.
- [ ] **Step 2: Run** `node mocap_bench/checks/stage_check.js` and `node mocap_bench/browser_check.mjs` → the new ones FAIL.
- [ ] **Step 3: Implement.** The selects list `none` then the table's weapons (weapon select) or shields (shield select). `state.gear` resets to `GE.defaultsFor(stance)` on project open and on a start-pose change; it's never written to the project. No ghost gear.
- [ ] **Step 4: Run** both → pass; run.js → all pass.
- [ ] **Step 5: Commit** `feat: the mocap bench draws a weapon and a shield on the doll, picked per session (nwn5e-ee #111, 3/4)`.

### Task 4: The warnings

**Files:**
- Modify: `mocap_bench/public/app.js` (live spans for the shown body; Save's per-body `gear`).
- Modify: `mocap_bench/public/timeline.js` (draw `model.gear` spans).
- Modify: `mocap_bench/public/core/pipeline.js` (`saveIssues` gear lines).
- Test: `mocap_bench/checks/pipeline_check.js`, `mocap_bench/checks/timeline_check.js`, `mocap_bench/browser_check.mjs` (extend `save`).

**Interfaces:**
- Consumes: `GE.span` (Task 2), `state.gear` (Task 3).
- Produces:
  - `timeline.setModel({... gear: [[t0, t1]] })` in take seconds (kept0 + animation seconds), drawn in the take's overview row like `poor`, in `C.warn` (or the accent if none), alpha 0.5.
  - `perBody[i].gear = GE.span(...).worst`.
  - The `saveIssues` line: `"<item> passes up to <cm> into the <region> at <t> s"` (t in animation seconds, 2 decimals), grouped per item+region, worst value across bodies.

- [ ] **Step 1: Failing checks:**
  - pipeline_check `gearSaveLine`: `saveIssues([{ body:"a_dfa", gear:[{item:"large shield", region:"torso", depth:0.062, t:1.234}] }])` includes `"large shield passes up to 6.2 cm into the torso at 1.23 s (a_dfa)"`.
  - timeline_check `gearSpansDrawn`: a model with `gear:[[1,2]]` paints the overview between x(1) and x(2).
  - browser `save`: with the large shield picked and the fixture project, Save's status or issues list names the shield when `GE.span` finds an intrusion, else none. The check asserts the line matches `GE.span` on the shown body (same depth to 0.1 cm).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** Live spans are recomputed when the result, the body or `state.gear` changes, cached on that triple. Save computes per body with the session's gear.
- [ ] **Step 4: Run** pipeline_check, timeline_check, run.js, browser_check → all pass.
- [ ] **Step 5: Commit** `feat: the mocap bench warns when gear passes into the body - timeline spans and Save lines (nwn5e-ee #111, 4/4)`.
