# The benches' frame and timeline: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** give the VFX, Spell & Attack and Mocap Benches one window-filling frame (top bar, stage always in view, selection panel, timeline), with time first. The VFX and Spell & Attack Benches move onto one shared timeline core and gain undo.

**Architecture:**
- A new `nwn5e-tools/common/` folder holds plain UMD modules and CSS:
  - the frame (`frame.css`, `frame.js`);
  - the timeline core (`timeline.js`: pure geometry and edits, plus a canvas widget);
  - an undo history (`history.js`).
- Each bench's server serves the folder at `/common/<file>` and stale-checks it.
- The VFX Bench gets a `beats.js`, which turns its effect into timeline rows, groups copies and edits shared fields.
- Each bench's page is then rebuilt on the frame, one bench at a time, keeping its checks green.

**Tech Stack:** browser JavaScript (UMD modules, canvas 2D, WebGL stages as now), Node checks (`require`), and the CDP browser checks (mocap_bench/checks/cdp.mjs `launch`).

**Spec:** `docs/superpowers/specs/2026-11-15-bench-frame-timeline-design.md`, approved by the owner 2026-11-15 (anim 0c692b4). Read it first: the plan argues from it.

## Global Constraints

- **Code:** plain browser JavaScript in UMD modules (`if (typeof module === "object" && module.exports) module.exports = api; else root.X = api;`). No build step, no framework, no new npm dependency.
- **Shared code:** `nwn5e-tools/common/`, served by every bench's server at `/common/<file>` and listed in each server's stale-code check (409 on `/api/*` other than `/api/state` while stale).
- **Theme tokens:** `--bg --panel --ink --muted --line --accent --danger --ok --display --body --mono`, light and dark. The Spell & Attack Bench's `--text`, `--panel2` and `--bad` move onto these.
- **Window:** at least 1100 × 700, and the page itself never scrolls.
- **Formats untouched:** what the benches make (drafts, looks, recipes, the game side) doesn't change.
- **Owner decisions to keep:**
  - the Spell & Attack Bench's 3D stage and resizable panels;
  - the Mocap Bench's one ruler in take time and its fixed-height timeline box;
  - the Mocap Bench's keys;
  - the VFX Bench's Space (pause) and R (restart);
  - "Try it in the preview" off by default.
- **Snapping:** to targets within 6 px, else to the bench's frames, else to a 0.05 s grid. Wheel zooms about the pointer, Shift+wheel pans, F fits.
- **Undo:** the last 100 steps; a drag is one step; a field is one step on change. Play in game, Stop and Save are not undone. Opening something else clears the history.
- **Hooks kept:** `window.vfxBench` (`ready`, `fx`, `trying`, `area`, `slotFollowsEditor`, `areaAt`), `?open`, `?mix`, `?warm`, `?pause`, `spellBench.markerScreen` and the other hooks the checks read, and `window.bench` (mocap).
- **Git:**
  - commit in nwn5e-tools `main` with `git commit --only <paths> -F -`;
  - `git add` new files first, one git command per call;
  - never `git add -A`, never `git -C`;
  - end each message with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`;
  - never push.
- **TDD:** watch every test fail before the code that passes it.

## Review Focus

1. **Copies' names.** `glow`, `glow2` and `glow3` group, but `glow` and `glowing` don't. A lone `spark2`, whose base `spark` doesn't exist, is a group of one. Renaming a copy to something else takes it out of the group. Test: Task 4 `beats grouping`.
2. **Dragging past the edges.** A start below 0 clamps to 0, an end past the effect's end clamps to it, and a bar keeps at least 0.05 s. A burst's mark stays in [0, duration]. Test: Task 1 `dragTo clamps`, and Task 7's browser step.
3. **Undo across effects.** Undo after opening another effect does nothing: the history was cleared. Test: Task 8 `undoClearedOnOpen`.
4. **Effect length vs zoom.** A 0.2 s effect fits, even though the core's least view is 0.5 s (the view clamps to the duration). A 30 s one fits and zooms to 0.5 s. Test: Task 1 `clampView short and long`.
5. **Corrupted saved sizes.** Corrupted or out-of-range layout sizes in localStorage fall back to the defaults and limits. Splits can't squeeze the stage below 320 × 240. Test: Task 3 `frame sizes`.

---

### Task 1: The timeline core's pure functions (`common/timeline.js`)

**Files:**
- Create: `nwn5e-tools/common/timeline.js` (UMD, `root.BenchTimeline`)
- Create: `nwn5e-tools/common/checks/timeline_check.js`, `nwn5e-tools/common/checks/run.js` (runs every `*_check.js` in the folder, as spell_bench/checks/run.js does)

**Interfaces:**
- Produces, all pure:
  - `timeToX(t, box)` and `xToTime(x, box)`, where `box = { x0, w, view: { start, end } }`.
  - `clampView(view, duration, least = 0.5)`, giving `{ start, end }`. The width is at least `min(least, duration)` and stays inside `[0, duration]`.
  - `zoomView(view, t, factor, duration)`: keeps `t` at the same screen place.
  - `panView(view, dt, duration)` and `fitView(duration)`, giving `{ start: 0, end: duration }`.
  - `snap(t, { targets = [], frames = null, grid = 0.05 }, pxPerS, reach = 6)`: to the nearest target within `reach` px, else to the nearest of `frames`, else to a multiple of `grid`.
  - `layout({ width, height, rows, view, left = 110, header = 22, ruler = 20, rowH = 22 })`, giving `{ x0, w, pxPerS, view, ruler: { y, h }, rows: [{ id, y, h, row }], needed }`.
    - A row is `{ id, label, colour?, items: [{ id, kind: "bar" | "mark" | "span", start, end?, label?, colour?, locked?, edits?: ["move", "start", "end"] }], draw? }`.
  - `hitTest(lay, x, y, handle = 5)`, giving one of:
    - `{ kind: "ruler", t }`
    - `{ kind: "item", row, item, grab: "body" | "start" | "end" }`
    - `{ kind: "empty", row, t }`
    - `null`
    - A locked item hits as `"body"` with no drag. An open-ended bar (`end == null`) has no `"end"` handle.
  - `dragTo(item, grab, t0, t, { min = 0, max, least = 0.05 })`, giving `{ start, end }`:
    - `"body"` moves by `t - t0` and keeps the length, clamped into `[min, max]`;
    - `"start"` moves the start and keeps the end;
    - `"end"` moves the end;
    - every result keeps `end - start >= least`;
    - a mark gives `{ start, end: null }`.

- [ ] **Step 1: Write the failing check** in `common/checks/timeline_check.js`, using the check/fail style of `vfx_bench/check_parts.js`:

```js
const T = require("../timeline.js");
const box = { x0: 110, w: 900, view: { start: 0, end: 3 } };
check("timeToX / xToTime round trip", near(T.xToTime(T.timeToX(1.25, box), box), 1.25));
check("clampView short and long", near(T.clampView({ start: 0, end: 5 }, 0.2).end, 0.2) && near(T.clampView({ start: 3, end: 3.1 }, 30).end - 3, 0.5));
check("zoomView keeps t under the pointer", near(T.timeToX(1, { ...box, view: T.zoomView(box.view, 1, 0.5, 3) }), T.timeToX(1, box)));
check("snap to a target in reach", near(T.snap(1.02, { targets: [1.0] }, 300), 1.0));
check("snap to frames, else grid", near(T.snap(0.51, { frames: [0, 0.5, 1] }, 300), 0.5) && near(T.snap(0.53, {}, 300), 0.55));
// layout: width 1010, left 110, right 12 -> x0 110, w 888, 296 px a second over view 0-3; header 22 + ruler 20 -> the row at y 42, h 22
const bar = { id: "b", kind: "bar", start: 0.5, end: 1.5, edits: ["move", "start", "end"] }, mark = { id: "m", kind: "mark", start: 2, edits: ["move"] };
const open = { id: "o", kind: "bar", start: 0.5, end: null, edits: ["move", "start"] };
const lay = T.layout({ width: 1010, height: 200, view: { start: 0, end: 3 }, rows: [{ id: "r", label: "r", items: [bar, mark] }, { id: "s", label: "s", items: [open] }] });
const at = (x, y) => T.hitTest(lay, x, y);
check("layout", lay.x0 === 110 && lay.w === 888 && near(lay.pxPerS, 296) && lay.rows[0].y === 42 && lay.rows[0].h === 22);
check("hitTest body / start / end / mark / empty / ruler", at(406, 50).grab === "body" && at(259, 50).grab === "start" && at(553, 50).grab === "end"
  && at(702, 50).item.id === "m" && at(880, 50).kind === "empty" && near(at(880, 50).t, 2.6, 0.01) && at(406, 30).kind === "ruler");
check("dragTo clamps", eq(T.dragTo(bar, "body", 1, -1, { max: 3 }), { start: 0, end: 1 })
  && eq(T.dragTo(bar, "end", 1.5, 5, { max: 3 }), { start: 0.5, end: 3 })
  && near(T.dragTo(bar, "start", 0.5, 1.6, { max: 3 }).start, 1.45));
check("an open-ended bar has no end handle", at(997, 72).grab === "body" && at(259, 72).grab === "start");
```

`layout`'s signature gains `right = 12`. Bars run to `x(end)`, and open-ended bars to `x0 + w`.

- [ ] **Step 2: Run it.** `node common/checks/timeline_check.js`. Expected: it fails with "Cannot find module '../timeline.js'".
- [ ] **Step 3: Implement the functions in `common/timeline.js`.** Port `clampView`, `zoomView`, `panView`, `snap` and the `timeToX` / `xToTime` geometry from `mocap_bench/public/timeline.js` (34-99), generalised as above. `hitTest` and `dragTo` are new, for generic items.
- [ ] **Step 4: Run it.** `node common/checks/run.js`. Expected: `all common checks pass`.
- [ ] **Step 5: Commit.** `git add common/timeline.js common/checks/timeline_check.js common/checks/run.js`, then `git commit --only common -F -` with "feat: common/timeline.js - the benches' timeline core, its pure geometry and edits (bench frame, Task 1)".

### Task 2: The timeline widget (`BenchTimeline.create`)

**Files:**
- Modify: `nwn5e-tools/common/timeline.js` (add `create`)
- Create: `nwn5e-tools/common/checks/timeline_demo.html`, `nwn5e-tools/common/checks/timeline_browser.mjs`

**Interfaces:**
- Consumes: Task 1's functions.
- **Items** may carry `tail` (seconds), drawn as a thin line after a bar's end: a hint, not editable. **Rows** may carry `muted` (drawn dimmed) and a label box, whose click calls `onToggle(row)`.
- Produces: `create(canvas, { onSeek(t), onSelect(hit | null), onEdit(label, fn, phase), onAdd(row, t), onToggle(row), snapOf(item) })`, which returns:
  - `setModel({ rows, duration, playhead, bands?: [{ start, end, label }], selection?: { row, item } })`
  - `draw()`, `view()`, `setView(view)`, `fit()`, `layout()`
- `onEdit` phases are `"begin"`, `"update"` and `"commit"`, as in the Mocap Bench. A drag sends begin, then updates carrying `dragTo`'s result through `fn(itemEdit)`, then commit.
- `snapOf(item)` returns the snap options for `snap`.
- Wheel zooms, Shift+wheel pans, F fits while the canvas has focus, and a double-click on empty space calls `onAdd`.
- Colours are read from the theme tokens. Each row's `draw` hook runs after its items.

- [ ] **Step 1: Write the failing browser check.**
  - `timeline_demo.html` loads `../timeline.js` and mounts a widget with two rows: a bar 0.5-1.5 with `edits: ["move", "start", "end"]`, and a mark at 2. It logs every callback to `window.log`.
  - `timeline_browser.mjs` opens it through a `file://` URL, using mocap_bench/checks/cdp.mjs `launch`, and checks these steps:
    - `dragEmitsBeginUpdateCommit`
    - `clickRulerSeeks`
    - `wheelZoomKeepsTimeUnderPointer`
    - `shiftWheelPans`
    - `fFits`
    - `dblClickAdds`
    - `lockedDoesNotDrag`
- [ ] **Step 2: Run it.** `node common/checks/timeline_browser.mjs`. Expected: it fails with "create is not a function".
- [ ] **Step 3: Implement `create`.** Use pointer capture as the Mocap Bench's `create` does (timeline.js:291-...), and draw:
  - the ruler, with seconds ticks and its bands;
  - the rows' bars, marks and spans;
  - a selection outline;
  - the playhead.

  Nothing draws over the labels (the left `x0` px).
- [ ] **Step 4: Run it.** `node common/checks/timeline_browser.mjs`, then `node common/checks/run.js`. Expected: `all passed`.
- [ ] **Step 5: Commit.** `git add common/checks/timeline_demo.html common/checks/timeline_browser.mjs`, then commit `common` with "feat: the timeline widget - drag, seek, zoom, pan, fit, add (bench frame, Task 2)".

### Task 3: The frame, the tokens, the history, and serving `/common/`

**Files:**
- Create: `nwn5e-tools/common/frame.css`, `nwn5e-tools/common/frame.js`, `nwn5e-tools/common/history.js`
- Create: `nwn5e-tools/common/checks/history_check.js`, `nwn5e-tools/common/checks/serve_check.js`
- Modify: `vfx_bench/server.js`, `spell_bench/server.js`, `mocap_bench/server.js`. Each serves `GET /common/<file>` from `../common` (`.js`, `.css`, `.html` only, no `..`), and adds `common/*.js` to its `CODE` list. The Mocap Bench's server also gets the `STARTED` / `CODE` / `stale()` guard as in vfx_bench/server.js:50-66 and :200.

**Interfaces:**
- **`frame.css`:**
  - The theme tokens (light and dark, as in vfx_bench/style.css).
  - The grid: `body` is `bar / (drawer | stage | panel) / timeline` at `100vh` with no page scroll. The drawer, panel and timeline scroll on their own.
  - Classes `.frame-bar`, `.frame-drawer` (with `.collapsed`), `.frame-stage`, `.frame-panel`, `.frame-timeline` and `.frame-split[data-split=drawer|panel|timeline]`.
  - Sizes come from `--drawer`, `--panel` and `--timeline`.
- **`frame.js`:** `BenchFrame.mount({ bench, limits: { drawer: [d, least, most], panel: [...], timeline: [...] } })`.
  - It returns `{ setStatus(text, kind), toggleDrawer(), sizes() }` and keeps the sizes in `localStorage["<bench>.layout"]`.
  - It is the Spell & Attack Bench's split code (app.js:586-618), generalised.
  - A double-click resets a split. Bad or out-of-range saved values fall back to the default.
  - The stage keeps at least 320 × 240: a split stops where the stage would go below that.
- **`history.js`:** `BenchHistory(limit = 100)`, with:
  - `push(snapshot)`: drops the redo branch, keeps `limit`;
  - `undo()` and `redo()`: return the snapshot to restore, or `null`;
  - `canUndo()`, `canRedo()`, `clear(snapshot)`.

  Snapshots are strings (JSON) the bench makes and restores.

- [ ] **Step 1: Write the failing checks.**
  - `history_check.js`:
    - `push / undo / redo order`;
    - `limit 100 keeps the newest`;
    - `a push after undo drops the redo branch`;
    - `clear starts again`.
  - `serve_check.js` starts each server on a test port (vfx 18766, spell 18768, mocap 18770, with their `--no-open` and `--build <tmp>` options as their own checks do) and checks:
    - `GET /common/frame.css 200`;
    - `GET /common/../server.js 404`;
    - with `common/timeline.js`'s mtime moved 5 s ahead (and restored), `/api/<a bench POST> 409`;
    - for the Mocap Bench: `the stale guard`.
  - `frame sizes`: in `timeline_browser.mjs`'s style, `frame_browser.mjs` mounts a demo page with `localStorage["demo.layout"] = "{bad"` and `{"panel": 9999}`, then checks the defaults, the limits and the stage at least 320 × 240.
- [ ] **Step 2: Run them.** `node common/checks/run.js`, then `node common/checks/frame_browser.mjs`. Expected: they fail (modules missing, `/common/` 404).
- [ ] **Step 3: Implement** `frame.css`, `frame.js`, `history.js` and the three servers' `/common/` route and stale lists.
- [ ] **Step 4: Run.**
  - `node common/checks/run.js`, `node common/checks/frame_browser.mjs`
  - Then each bench's own checks: `node vfx_bench/check.js`, `node spell_bench/checks/run.js`, `node mocap_bench/checks/run.js`
  - Expected: all pass.
- [ ] **Step 5: Commit.** `git add` the new common files, then commit `common vfx_bench/server.js spell_bench/server.js mocap_bench/server.js` with "feat: the benches' shared frame, theme tokens and undo history, served at /common/ and stale-checked (bench frame, Task 3)".

### Task 4: The VFX Bench's beats (`vfx_bench/public/beats.js`)

**Files:**
- Create: `vfx_bench/public/beats.js` (UMD, `root.VfxBeats`), `vfx_bench/check_beats.js`
- Modify: `vfx_bench/server.js` `FILES` and `CODE` (beats.js)

**Interfaces:**
- Consumes: core.js (`emitter`, `effect`), sim.js `Sim.anims()` lengths.
- Produces:
  - **`groupsOf(emitters)`** gives `[{ key, members: [names] }]` in list order.
    - A name `base + digits` joins `base`'s group when an emitter named `base` exists.
    - Members are ordered `base, base2, base3…`.
    - Anything else is a group of one.
  - **`rowsOf(fx, duration)`** gives the timeline rows (Task 1's row shape):
    - **One row per group:** `id = "beat:" + key`, `label = key (+ " ×n")`. Its item:
      - a `bar` from the members' least `start_seconds` to their greatest end;
      - an emitter's end is `start + emit_seconds` when that is above 0;
      - else a one-shot's `seconds`;
      - else `null` (open-ended) for a lasting effect;
      - or a `mark` when every member's `update` is Explosion;
      - `edits: ["move", "start", "end"]` (a mark: `["move"]`);
      - `tail` is the members' greatest particle life (none for −1).
    - The row is `muted` when every member is hidden in the preview.
    - **A light row per light:** a locked bar over `[0, duration]`.
  - **`retime(fx, key, { start, end })`:** shifts every member's `start_seconds` by `start - oldStart`. With `end != null`, it sets every member's `emit_seconds = end - its new start`, and `0` when `end >= duration`. It returns the changed emitters.
  - **`shared(members, key)`** gives the common value or `MIXED`, a unique symbol, compared with deep equality for arrays.
  - **`setShared(members, key, value)`.**
  - **`PER_COPY = ["name", "position", "orientation", "keys", "parent"]`:** fields shown per copy only.

- [ ] **Step 1: Write `check_beats.js`:**
  - `beats grouping`: `["glow", "glow2", "glow3", "glowing", "spark2"]` gives `[glow: 3], [glowing], [spark2]`.
  - `rowsOf: a one-shot bar`: start 0.5, On for 1, seconds 2, giving `0.5-1.5`.
  - `rowsOf: an always-on one-shot ends at seconds`.
  - `rowsOf: a lasting always-on is open-ended (end null)`.
  - `rowsOf: a burst is a mark at its start`.
  - `rowsOf: a light is a locked bar`.
  - `retime moves all copies keeping offsets`: copies starting 0.2 and 0.3; moving the group to 0.5 gives 0.5 and 0.6.
  - `retime end to the effect's end clears On for`.
  - `shared / MIXED / setShared`: particle `texture` shared, `birthrate` mixed, then set.
- [ ] **Step 2: Run it.** `node vfx_bench/check_beats.js`. Expected: it fails, module missing.
- [ ] **Step 3: Implement `beats.js`.**
- [ ] **Step 4: Run it.** `node vfx_bench/check_beats.js`. Expected: `all beats checks pass`.
- [ ] **Step 5: Commit.** `git add vfx_bench/public/beats.js vfx_bench/check_beats.js`, then commit with `vfx_bench/server.js`: "feat: the VFX Bench's beats - copies grouped by name, timeline rows, group retiming and shared fields (bench frame, Task 4)".

### Task 5: A look never touches a beat's timing (`parts.js`)

**Files:**
- Modify: `vfx_bench/public/parts.js` `applyEmission`, `vfx_bench/check_parts.js`

**Interfaces:**
- Produces: `applyEmission(e, part, given, { keepTiming = false } = {})`. With `keepTiming`, the emitter's `start_seconds` and `emit_seconds` survive the reset to the defaults. Task 7's "Put on this beat" passes `keepTiming: true`. Add as a new emitter does not.

- [ ] **Step 1: Add a check to `check_parts.js`:** `applyEmission keepTiming keeps Starts after and On for`. An emitter with start 0.7 and On for 0.4, with "stream" put on it using `keepTiming`, gives 0.7 and 0.4. Without the flag, it gives the part's own values.
- [ ] **Step 2: Run it.** `node vfx_bench/check_parts.js`. Expected: FAIL on the new check.
- [ ] **Step 3: Implement** the option.
- [ ] **Step 4: Run** `node vfx_bench/check_parts.js`, then `node vfx_bench/check_effect.js`. Expected: both pass.
- [ ] **Step 5: Commit** `vfx_bench/public/parts.js vfx_bench/check_parts.js` with "feat: Put keeps a beat's timing (parts.applyEmission keepTiming; bench frame, Task 5)".

### Task 6: The VFX Bench on the frame (layout only)

**Files:**
- Modify: `vfx_bench/public/index.html`, `vfx_bench/public/style.css` (keeps only bench-specific rules, loads `/common/frame.css`), `vfx_bench/public/app.js` (mount the frame, move the transport), `vfx_bench/browser_check.mjs`, `vfx_bench/README.md`

**Interfaces:**
- Consumes: Task 3's `BenchFrame.mount({ bench: "vfxbench", limits: { drawer: [270, 0, 420], panel: [370, 280, 640], timeline: [200, 120, 480] } })`.
- **Layout:**
  - **Drawer:** the Effects library, with "Open" in the bar toggling it.
  - **Bar:** the effect's name, type and seconds (bound to the Effect card's fields), Play in game, Stop in game, Save, status, the game link.
  - **Stage:** the canvas and overlay.
  - **Panel:** today's right-rail cards plus the Build from parts card, the Area card and the draft text, which move out of the middle column.
  - **Timeline:** a header with the transport (Restart, Pause, Speed, Loop, Walk, Figure, Backdrop, the clock) and an empty `<canvas id="timeline">` until Task 7.
- **No ids change.** The existing browser steps keep working.

- [ ] **Step 1: Add browser steps to `vfx_bench/browser_check.mjs`:**
  - `frameNoPageScroll`: `document.scrollingElement.scrollHeight <= innerHeight + 1`.
  - `stageStaysInViewWhilePanelScrolls`: the panel's scrollTop is set to its max, and the stage's rect stays fully in the window.
  - `splitsRemembered`: drag the panel split 60 px, reload, same width.
- [ ] **Step 2: Run** `node vfx_bench/browser_check.mjs`. Expected: the three new steps fail.
- [ ] **Step 3: Rebuild** `index.html` and `style.css` on the frame, and move the elements without renaming ids. The README's "The page" section describes the frame.
- [ ] **Step 4: Run** `node vfx_bench/browser_check.mjs` and the Node checks (`check.js`, `check_area.js`, `check_parts.js`, `check_effect.js`, `check_beats.js`). Expected: all pass.
- [ ] **Step 5: Commit** the five files: "feat: the VFX Bench on the frame - the stage always in view, the library in a drawer, the settings in the panel (bench frame, Task 6)".

### Task 7: The VFX Bench's timeline - beats, drags, "+ new beat", selection

**Files:**
- Modify: `vfx_bench/public/app.js`, `vfx_bench/public/index.html`, `vfx_bench/browser_check.mjs`, `vfx_bench/README.md`

**Interfaces:**
- Consumes: Tasks 1-2 (`BenchTimeline.create`), 4 (`VfxBeats.rowsOf`, `retime`, `groupsOf`, `shared`, `setShared`, `MIXED`, `PER_COPY`), 5 (`keepTiming`).
- **The model:** `rowsOf(state.fx, playLength)`.
  - Ruler bands, for a lasting effect, from `sim.anims()`: start | loop | end; for a one-shot, the impact.
  - The playhead is the sim's time. `onSeek` sets it (the sim's seek), and `onEdit` calls `VfxBeats.retime`, then `changed(true)`.
- **"+ new beat"** (a button under the rows), or `onAdd(row, t)`, does Add as a new emitter with `start_seconds = t` (the playhead for the button) and selects the new group.
- **Selection,** `state.selection = { kind: "beat" | "light" | "node" | "effect", key }`. The panel shows:
  - **a beat:** timing (Starts after / On for, in step with the bar), then the look (the parts card, pointed at this beat: Put on this beat with `keepTiming`; chip and setting changes re-apply to it live; its mix remembered per beat key for the session), then the field groups collapsed;
  - **a group:** fields through `shared` / `setShared`, with "mixed" shown, and `PER_COPY` only in the expanded copy rows;
  - **nothing:** the Effect card, the budget, the Nodes and Lights lists, the parts card for a new beat, and the draft text.
- **Expanding:** a group row's arrow expands into one row per copy, each copy's bar retiming only itself.
- **Hiding in the preview:** `onToggle(row)` hides or shows the row's members in the preview (`sim.hidden`, today's "Shown in the preview (not saved)").
- **Hooks:** `window.vfxBench.timeline()` returns the widget's `layout()`; `window.vfxBench.select(kind, key)`.

- [ ] **Step 1: Add browser steps:**
  - `dragBeatMovesStart`: Δ0.3 s on a bar changes `start_seconds` by 0.3.
  - `dragBeatEndSetsOnFor`.
  - `dragEndToEffectEndClearsOnFor`.
  - `newBeatAtPlayhead`: playhead 0.8, "+ new beat", new emitter `start_seconds` 0.8, selected.
  - `burstMarkDragsStart`.
  - `groupIsOneRowAndExpands`: the Breathe motion with 3 copies is 1 row, expanding to 3.
  - `groupDragMovesAll`.
  - `groupSharedAndMixed`: texture shows its value, birthrate "mixed", setting it sets all 3.
  - `lookKeepsTiming`: Put on this beat leaves Starts after and On for.
  - `dragClampsToEffect`: a drag past the end stops at the end, and before 0 at 0.
  - `rowToggleHidesInPreview`: a click on a row's label box hides its members from the sim, and the draft is unchanged.
- [ ] **Step 2: Run** `node vfx_bench/browser_check.mjs`. Expected: the new steps fail.
- [ ] **Step 3: Implement** in app.js and index.html. The README gets a "The timeline" section.
- [ ] **Step 4: Run** `node vfx_bench/browser_check.mjs` and the Node checks. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: the VFX Bench's timeline - a row per beat, drag its start and On for, + new beat, groups and their shared settings (bench frame, Task 7)".

### Task 8: Undo in the VFX Bench

**Files:**
- Modify: `vfx_bench/public/app.js`, `vfx_bench/public/index.html` (Undo / Redo in the bar), `vfx_bench/browser_check.mjs`

**Interfaces:**
- Consumes: `BenchHistory`.
- **Snapshots:** the history holds `JSON.stringify({ fx, area })`.
- **When to push:** `changed()` pushes after a field change. A drag pushes on `commit` only. Opening an effect or area calls `clear(snapshot)`.
- **Keys and buttons:** Ctrl+Z / Ctrl+Y (and Ctrl+Shift+Z), ignored while typing. The buttons are disabled by `canUndo` / `canRedo`.

- [ ] **Step 1: Add browser steps:**
  - `undoRedoDragAndField`: drag a bar, change a field, Ctrl+Z twice gives the original, Ctrl+Y twice gives both changes.
  - `undoClearedOnOpen`: open another effect, then Ctrl+Z changes nothing and the Undo button is disabled.
  - `saveNotUndone`: Save, then Ctrl+Z undoes the last edit, and the saved file is unchanged.
- [ ] **Step 2: Run** the browser check. Expected: the new steps fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the browser and Node checks. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: undo and redo in the VFX Bench (bench frame, Task 8)".

### Task 9: Area mode on the timeline

**Files:**
- Modify: `vfx_bench/public/app.js`, `vfx_bench/browser_check.mjs`

**Interfaces:**
- Consumes: area.js `copiesAt(area, k, t, seed)`.
- **Rows:** in area mode, one row per slot with a model (`id = "slot:" + k`). Its items are each copy's lives as locked bars (born to dies) over the play length, which is a few cycles of the longest slot lifetime, at least 6 s.
- **No drags.** The panel shows the Area card. Selecting a slot row does what "Edit its effect" does.

- [ ] **Step 1: Add browser steps:**
  - `areaRowsPerSlot`: VFX_PER_ENTANGLE gives 3 rows.
  - `areaRowsLocked`: a drag changes nothing.
  - `areaSlotSelectEdits`.
- [ ] **Step 2: Run.** Expected: the new steps fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the browser and Node checks. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: the VFX Bench's area mode on the timeline - a row per slot, its copies' lives (bench frame, Task 9)".

### Task 10: The Spell & Attack Bench on the frame

**Files:**
- Modify: `spell_bench/public/index.html`, `spell_bench/public/style.css` (onto the tokens and `/common/frame.css`), `spell_bench/public/app.js` (its split code replaced by `BenchFrame.mount({ bench: "spellbench", ... })` with today's limits, and the `spellbench.layout` key kept), `spell_bench/checks/browser_check.mjs`, `spell_bench/README.md`

**Interfaces:**
- **Drawer:** the Scene panel.
- **Bar:** today's header, without the transport.
- **Timeline's header:** Restart, Pause, sounds, speed.
- **Panel:** the six numbered cards.
- **Kept:** ids and `window.spellBench`.

- [ ] **Step 1: Add browser steps:** `frameNoPageScroll`, `stageStaysInViewWhilePanelScrolls`, `splitsRemembered` (as Task 6), and `tokensApplied` (the body's computed `--ink` is defined).
- [ ] **Step 2: Run** `node spell_bench/checks/browser_check.mjs`. Expected: the new steps fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `node spell_bench/checks/browser_check.mjs` and `node spell_bench/checks/run.js`. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: the Spell & Attack Bench on the shared frame and tokens (bench frame, Task 10)".

### Task 11: The Spell & Attack Bench's timeline on the core, its selection and undo

**Files:**
- Modify: `spell_bench/public/editor.js` (the timeline wiring), `spell_bench/public/app.js`
- Delete (after the move): `spell_bench/public/timeline.js`. Ask the owner's OK before deleting (the project's rule); keep it until then.
- Modify: `spell_bench/checks/browser_check.mjs`

**Interfaces:**
- Consumes: `BenchTimeline.create` and `BenchHistory`.
- **Rows:** `SpellLook.timeline(...)`'s lanes become rows (Animation, Cast VFX, Attack, Hit VFX, Sounds, colours as now). Bars become `bar` items, and markers become `mark` items:
  - locked markers stay locked;
  - the release, the sounds and the Cast VFX bar are editable;
  - `onEdit` maps to today's `onChange({ kind, sound, t })` / `{ kind: "castvfx", t }`, then `{ kind: "done" }`.
- **Snapping:** `snapOf` returns `{ frames: <the cast clip's key times + rule_at> }`, as editor.js:50-80 does now.
- **Selection:** `onSelect` opens and scrolls to the lane's card (1-6).
- **Undo:** `JSON.stringify(look)` snapshots, Ctrl+Z / Ctrl+Y, Undo and Redo in the bar.
- **Kept:** `spellBench.markerScreen(kind, index)` returns the marker's screen point from the new layout.

- [ ] **Step 1: Add browser steps:**
  - `markerDragsRetime`: the existing release and sound drag steps, unchanged.
  - `zoomKeepsTimeUnderPointer`.
  - `fFitsTheLook`.
  - `laneClickOpensItsCard`.
  - `undoMarkerDrag`.
- [ ] **Step 2: Run.** Expected: the new steps fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the browser check and `node spell_bench/checks/run.js`. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: the Spell & Attack Bench's timeline on the shared core - zoom, pan, fit, lane to card, undo (bench frame, Task 11)".

### Task 12: The Mocap Bench on the frame

**Files:**
- Modify: `mocap_bench/public/index.html`, `mocap_bench/public/style.css` (onto `/common/frame.css` and the tokens), `mocap_bench/public/app.js` (`BenchFrame.mount({ bench: "mocapbench", limits: { drawer: [0, 0, 0], panel: [300, 240, 560], timeline: [clamp default, 160, 420] } })`), `mocap_bench/browser_check.mjs`, `mocap_bench/README.md`

**Interfaces:**
- **Bar:** the project select, New, body, stance, Save and status, in the shared bar style. No drawer.
- **Unchanged:** the stage, inspector, transport, timeline, keys and `window.bench`. The timeline box keeps a fixed height while rows appear, and now resizes only through its split.

- [ ] **Step 1: Add browser steps:**
  - `frameNoPageScroll`.
  - `splitsRemembered`.
  - `timelineHeightFixedWhileRowsAppear`: adding a contact doesn't resize the stage.
- [ ] **Step 2: Run** `node mocap_bench/browser_check.mjs`. Expected: the new steps fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the browser check and `node mocap_bench/checks/run.js`. Expected: all pass.
- [ ] **Step 5: Commit:** "feat: the Mocap Bench on the shared frame (bench frame, Task 12)".

### Task 13: Hand-over

**Files:**
- Modify: `nwn5e-ee-anim/docs/TOOLS-PLAN.md` (a section: the benches' frame, built)
- Board rows CSV (in the job's tmp; the parent session adds them):
  - BF-1, the VFX Bench's frame, timeline and undo;
  - BF-2, the Spell & Attack Bench;
  - BF-3, the Mocap Bench.
  - Each is self-contained: the bench restart, what to do, what to see.

- [ ] **Step 1: Run every check once more:**
  - `node common/checks/run.js`, the three browser checks;
  - `node vfx_bench/check.js`, `check_area.js`, `check_parts.js`, `check_effect.js`, `check_beats.js`;
  - `node spell_bench/checks/run.js`, `node mocap_bench/checks/run.js`.

  Expected: all pass.
- [ ] **Step 2: Write** the TOOLS-PLAN section and the rows CSV.
- [ ] **Step 3: Commit** TOOLS-PLAN on anim: "docs: TOOLS-PLAN - the benches' frame and timeline, built".
