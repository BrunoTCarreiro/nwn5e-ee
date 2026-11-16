# The benches: one frame, time first

The design for making nwn5e-tools' three benches (the VFX Bench, the Spell & Attack Bench and the Mocap Bench) easier to work in. Written 2026-11-15.

## Intent and why

The owner, 2026-11-15:

- "we need to make the benches in general more user friendly. the previews shoudl always be visible, organizing timing for the effects/attacks or major motions for the mocap should be first and foremost"
- Asked how they build an effect: "the beats in time first, then the look of each piece"
- On the layout sketch: "looks right, go with that layout for all benches"
- Of the three approaches: "go with 2"

The owner works by laying out *when* things happen, then shaping *what* each thing looks like. In each bench, the timeline is where the work is organised, and the stage shows the result at all times. The settings of whatever is selected sit beside the stage, not in a long stack the stage scrolls away from.

Where the benches stand today:

- **The VFX Bench** is the outlier:
  - Its middle column scrolls. The stage, then the Build from parts card, then the draft text are stacked in it, so working on the card scrolls the preview out of view.
  - It has no timeline. An emitter's timing (Starts after, On for: #164) is two fields deep in its card.
- **The Spell & Attack Bench** already has the shape:
  - A top bar and a stage that never scrolls.
  - Side panels that scroll on their own and can be resized.
  - A fixed timeline of lanes along the bottom.
- **The Mocap Bench** was redesigned to this shape (docs/BENCH-REDESIGN.md, "The screen") and largely matches it.

## Goals

1. **One frame in every bench.** It fills the window and the page never scrolls. It has:
   - a top bar;
   - the stage, always in view;
   - a selection panel beside the stage;
   - a timeline along the bottom.

   The panels and the timeline scroll on their own and can be resized. Their sizes are kept per bench. (The owner, 2026-11-10, Spell & Attack Bench: "make it so I can resize the proview area".)
2. **Time first in the VFX Bench.** An effect is built on the timeline:
   - one row per emitter, a bar from its start to its end, a burst as a mark;
   - drag a bar to move it, drag its ends to set Starts after and On for;
   - "+ new beat" puts a new emitter at the playhead;
   - then the selection panel gives it its look.
3. **One timeline core for the VFX and Spell & Attack Benches** (approach 2), so dragging, zooming, panning and snapping behave the same in both. It's built from the Mocap Bench's timeline, which already does these well.
4. **The Mocap Bench gets the shared frame now**, and moves its timeline onto the core when mocap work resumes. Mocap work is on hold (owner, 2026-11-08).
5. **Undo and redo** (Ctrl+Z / Ctrl+Y) for every change in the VFX and Spell & Attack Benches. The Mocap Bench already has them.

## Non-goals

- Changing what any bench makes. Drafts, looks, recipes and the game side are untouched; this is the page and its timeline.
- New effect, look or mocap features beyond the timeline editing in goal 2.
- Moving the Mocap Bench's timeline onto the core now, or changing its rows or inspector.
- Narrow windows or touch. The frame needs a sensible minimum, about 1100 × 700.

## Anti-goals (a failure even if it works)

- The stage scrolling out of view, or a scrollbar on the page itself.
- The preview showing something the effect doesn't hold without the user asking for it (owner, 2026-11-15, d50fff4: "Try it in the preview" stays off by default).
- The VFX and Spell & Attack timelines behaving differently: zoom, pan, snap, drag and seek.
- Losing a setting. Every field reachable today stays reachable, in the selection panel or a drawer.
- Breaking the browser checks' hooks without moving the checks with them.

## Constraints

- **Code:** plain browser JavaScript in UMD modules, as the benches use now. No build step, no framework. Each bench's Node checks `require()` the same files the page loads.
- **Shared code** lives in `nwn5e-tools/common/`:
  - each bench's server serves it under `/common/<file>`;
  - each server lists it in its stale-code check (the guard from tools dd55e71 / d585b80);
  - the Mocap Bench's server gets that guard too;
  - checks reach it with `require("../common/...")` or `require("../../common/...")`, as they already reach across benches.
- **One set of theme tokens:** the VFX and Mocap Benches' light/dark set (`--bg --panel --ink --muted --line --accent --danger --ok --display --body --mono`). The Spell & Attack Bench's dark-only names (`--text`, `--panel2`, `--bad`) move onto it.
- **Owner decisions this design keeps:**
  - the Spell & Attack Bench's 3D stage and resizable panels (2026-11-10);
  - the Mocap Bench's one ruler in take time;
  - its fixed-height timeline box ("so rows appearing … never resize the stage under a drag", #89);
  - its keys;
  - the VFX Bench's Space (pause) and R (restart);
  - "Try it in the preview" off by default.

## Approaches (the owner picked 2)

1. **One shared frame and one shared timeline core; all three benches moved now.**
   - For: the most consistent.
   - Against: the biggest job, and it reworks the Mocap Bench while mocap work is on hold.
2. **(Picked.) Shared frame and core for the VFX and Spell & Attack Benches now.** The Mocap Bench gets the frame now and the core when mocap work resumes.
   - For: the consistent result where the owner is working today, without touching paused work.
3. **The shared frame only, each bench keeping its own timeline.**
   - For: the fastest.
   - Against: zoom, snap and drag differ between benches and drift further apart.

## The frame (`common/frame.css`, `common/frame.js`)

```
+---------------------------------------------------------------------------------+
| [Open] name/pick · its kind      [Play in game] [Stop] [Save]   status   (o)link |
+-------------+---------------------------------------------+---------------------+
| drawer      |                                             |  SELECTION          |
| (optional,  |                 STAGE                       |  the selected       |
|  collapses: |          (never scrolls away)               |  item's settings,   |
|  library /  |                                             |  in sections        |
|  scene)     |                                             |  (scrolls)          |
+-------------+---------------------------------------------+                     |
| > || [restart] speed loop ...   0.40 s                     |                     |
| ruler ----------|----------------------------------------- |                     |
| rows (scroll inside the timeline box)                      |                     |
+------------------------------------------------------------+---------------------+
```

- **Grid:** top bar / (drawer | stage | selection) / timeline.
  - Split handles size the drawer, the selection panel and the timeline. They are kept in `localStorage` under `<bench>.layout`, and a double-click resets them (the Spell & Attack Bench's behaviour, generalised).
  - The drawer collapses to a strip ("Open" in the top bar toggles it).
- **Top bar:** what's open (its name or a pick), its kind, the bench's main actions, the status line and the game-link dot.
- **The timeline's header row** is the transport: play/pause, restart, step, speed, loop, the bench's view toggles and the time readout. It sits there, not under the stage, so the stage gets the height.
- **`frame.js`** builds the grid from a bench's parts. It also does the splits and their memory, the drawer, and a shared status line ("good" / "bad" / plain). It knows nothing about any bench's data.

## The timeline core (`common/timeline.js`)

The core is made generic from the Mocap Bench's timeline: its view maths, snapping, pointer handling and edit protocol. Each bench supplies its own rows and what its items mean.

**Pure functions (Node-checked):**

- `timeToX`, `xToTime`
- `clampView`, `zoomView` (about a time), `panView`, `fitView(start, end)`
- `snap(t, targets, pxPerS)`: to the nearest target within a few pixels (markers, other items' ends, the playhead), else to a grid (frames when the bench has them, else 0.05 s)
- `layout(rows, size)`
- `hitTest(layout, x, y)`: an item's body, its start or end handle, a mark, the ruler, empty space in a row
- `dragTo(item, grab, t, limits)`: move, set start, set end, with a minimum length and the bench's limits

**Rows** are given by the bench: `{ id, label, colour?, items: [...] }`.

- Each item is `{ id, kind: "bar" | "mark" | "span", start, end?, label?, colour?, locked?, edits: ["move", "start", "end"] }`.
- A row can carry a `draw(ctx, rowBox, view)` hook for anything special the bench draws (the Spell & Attack Bench's flight shading, for example).

**Ruler bands:** shaded sections, such as a lasting effect's start | loop | end.

**Interaction:**

| Action | Does |
|---|---|
| Wheel | zooms about the pointer |
| Shift+wheel | pans |
| F | fits the whole |
| Click the ruler, or drag on it | seeks (scrubs) |
| Click an item | selects it (`onSelect`) |
| Drag an item | edits it (`onEdit(label, fn, "begin" / "update" / "commit")`) |
| Double-click a row's empty space | `onAdd(row, t)`, when the bench allows it |

- **Edits:** the edit protocol is the Mocap Bench's, so a bench can undo whole drags.
- **Playhead:** set by the bench every frame.
- **Colours:** from the theme tokens.

## The VFX Bench on the frame

- **Drawer:** the effects library (Yours / nwn5e / Game / Areas, search, New effect, Reload).
- **Top bar:** the effect's name, type (one-shot or lasting) and seconds; Play in game, Stop in game, Save; the status line and the game link.
- **Stage:** the current WebGL preview and its overlay.
  - The transport row (Restart, Pause, Speed, Loop, Walk, Figure, Backdrop, the clock) moves to the timeline's header.

**Timeline rows:**

- **One row per beat, in the Emitters list's order.** A beat is one emitter, or the copies one Add or "+ new beat" made.
  - The owner, 2026-11-15, picked "1.A": copies added together share a row.
  - **Grouping:** the bench recognises copies from their names, the way the parts card names them: a name, then the same name with a number added (`glow`, `glow2`, `glow3`…). So the grouping survives saving and reopening, and the draft format doesn't change.
  - **The row:** a group's row has one bar that drives all its copies. Moving it shifts each copy's start by the same amount, and its ends set every copy's On for. An arrow expands the row into the copies' own rows, for a copy on its own.
  - It's a bar from its start (`start_seconds`) to where it goes off:
    - with On for: its start plus `emit_seconds`;
    - otherwise: the effect's end.
  - A lasting effect's always-on emitter runs to the timeline's end, drawn open-ended (no end handle).
  - **Dragging:**
    - the bar's body moves its start and keeps its On for;
    - its end handle sets On for (dragging it to the effect's end clears On for to 0, "while the effect plays");
    - its start handle moves the start and keeps the end where it is.
  - A burst (Explosion) is a mark at its start; dragging it moves the start.
  - A thin trail after a bar shows its particles' life, as a hint only, not editable.
  - The row's checkbox is "Shown in the preview (not saved)", as now.
- **Lights get rows too:** a bar for the whole effect with their fade in and out drawn at the ends. They're not dragged in time (lights have no start).
- **Nodes get no rows.** They're reached from the emitter they move (its Parent), and in the selection panel.
- **Ruler:** for a lasting effect, the ruler's bands show its start | loop | end animation lengths, and a one-shot shows its impact. The play length is the preview's (Seconds; Permanent repeats the loop).

**"+ new beat"** sits under the last row. It adds an emitter whose start is at the playhead, with the Build from parts card's current mix, selects it and opens its look. Double-clicking a row's empty space at a time does the same. The new beat is in the effect at once: it's the user's action that adds it (the 2026-11-15 rule).

**Selection panel:**

- **A beat (an emitter, or a group of copies):**
  1. Its timing: Starts after and On for, kept in step with the bar.
  2. Its look, from parts: Build from parts' particle, emission and motion chips with their settings. "Put on this beat" replaces "Put on the selected emitter".
     - **Live:** selecting a beat points the card at it, so a chip or setting changed here changes that beat live in the preview. (Today the card stays tied to whatever was added last, #163.)
     - **Remembered:** the bench remembers each beat's mix for the session. A beat opened from a file shows no chip picked until one is.
     - **Timing kept:** a look change or a Put never touches the beat's timing. `start_seconds` and `emit_seconds` stay, though today's `applyEmission` resets them with the other emission keys.
  3. The current field groups, collapsed: Kind, Texture, Emission, Motion, Point to point, Lightning, Its own motion, Look.
- **A group, as above, applied to all its copies** (the owner, 2026-11-15: "selecting the group allows the user to change common settings, like, for example the particle used if they all share it"):
  - A field shows its value when every copy shares it, and "mixed" when they differ.
  - Changing a field sets it on every copy.
  - Fields that differ by design (each copy's position and keys from its motion) are shown per copy only, in the expanded rows.
- **A light:** its fields.
- **A node:** its fields and motion. It's reached from an emitter's Parent, or from a "Nodes" list at the panel's foot.
- **Nothing selected:** the effect's settings (today's Effect card and the particle budget), the Nodes and Lights lists, the Build from parts card for making a new beat, and the draft text, collapsed.
- **An open area:** the area card (name, shape and size, slots, Back to the effect).

**Area mode:**

- One timeline row per slot. Its copies are drawn as repeating bars, each copy's start delay and life as the game places them (area.js `copiesAt`).
- The slot's Copies and Each lasts are edited in the panel. Dragging is off in area mode.

**Hooks and URL parameters** (`window.vfxBench`, `?open`, `?mix`, `?warm`, `?pause`) are kept. Two hooks are added for the checks: `timeline()` (its layout) and `select(kind, name)`.

## The Spell & Attack Bench on the frame

- **Already there:** the frame's shape. What changes:
  - the shared frame CSS and the theme tokens;
  - the left Scene panel becomes the drawer;
  - the transport (Restart, Pause, sounds, speed) moves to the timeline's header.
- **The timeline moves onto the core.**
  - **Lanes become rows:** Animation, Cast VFX, Attack, Hit VFX, Sounds.
  - **SpellLook.timeline's bars and markers become items:**
    - unlocked markers (the release, sounds) drag;
    - the Cast VFX bar moves;
    - locked markers follow as now.
  - **Snapping:** the clip's frames (look.js `snap`) become snap targets.
  - **It gains** zoom, pan and F to fit.
- **Clicking a lane or item selects its card:** the six numbered cards are the selection panel's sections, and the selected one opens and scrolls into view.
- **Kept:** `spellBench.markerScreen` and the checks' other hooks.

## Undo in the VFX and Spell & Attack Benches

The owner, 2026-11-15, picked "2.A": Ctrl+Z / Ctrl+Y for every change, fields and drags alike.

- **History:** each bench keeps a snapshot of what's open (the effect, or the area and its slots' effects; the look) after each change, up to 100. Opening something else starts a new history.
- **One drag, one step:** a drag is a single step (the core's begin/commit), and so is a field's typing (on change, not on every key).
- **Not undone:** Play in game, Stop and Save. Undo changes the open work, not the game or the saved files. Save writes what's open now.
- **Shortcuts:** they're ignored while typing in a field, as Space and R are now.
- **Buttons:** the top bar shows Undo and Redo, greyed out when there's nothing to undo or redo.

## The Mocap Bench

- The shared frame CSS, top bar style, tokens and split handles. Its stage, inspector and timeline sizes become resizable, with the timeline box keeping its fixed height while rows appear.
- Its server gets the stale-code guard.
- Its timeline, rows, inspector and keys are unchanged. Moving its timeline onto the core is a later, separate piece, when mocap work resumes.

## Build order

1. `common/`: frame.css, frame.js, the theme tokens and the timeline core with its pure functions, plus a Node check of the core. Each server serves `/common/` and stale-checks it.
2. The VFX Bench onto the frame and the core: its rows, drags, "+ new beat", the selection panel and area mode. Its browser check updated.
3. The Spell & Attack Bench onto the frame and the core. Its browser check updated.
4. The Mocap Bench onto the frame. Its browser check updated.

Each step leaves every bench working and its checks green. The benches ship from nwn5e-tools without an install, apart from steps that change the dev layer (none planned).

## Checks

- **`common/`'s Node check:** `timeToX` / `xToTime` round trips, zoom about a time, pan limits, snap (to a target within reach, else to the grid), `hitTest` (body, handles, marks, the ruler, empty space), and `dragTo` (move keeps the length, the end clamps to the effect, the minimum length).
- **Every bench's browser check:**
  - the page has no scrollbar;
  - the stage stays in view while the selection panel scrolls;
  - the splits resize and are remembered.
- **The VFX Bench's browser check:**
  - dragging a beat's bar changes its `start_seconds`;
  - dragging its end changes `emit_seconds`;
  - "+ new beat" adds an emitter at the playhead and selects it;
  - a burst's mark drags its start;
  - area mode shows a row per slot.
  - A copied part (three copies) is one row. Dragging it moves all three, and expanding it shows three.
  - With the group selected, a shared field shows its value. A differing one shows "mixed", and changing it sets all three.
  - A look change on a selected beat leaves its Starts after and On for alone.
  - Ctrl+Z undoes a drag and then a field change, and Ctrl+Y redoes them.
- **Copy grouping, a Node check:** names to groups, including an emitter named with a number on its own (a group of one).
- **The Spell & Attack Bench's browser check:** its marker drags still retime the look, zoom keeps the playhead's time under the pointer, and Ctrl+Z undoes a marker drag.

## What the builder decides

- Exact sizes, spacing, icons and colours within the tokens.
- Which panel sections start open.
- The snap reach in pixels, and the grid when a bench has no frames (0.05 s proposed).
- File names inside `common/`.
- How the drawer collapses (a strip with its title, or an icon).
- Whether a beat's row shows its texture as a small swatch.
- Where the Emitters list's Add, Copy, Remove and ↑ / ↓ go: a row's menu, or the selection panel.
- The timeline's span for a Permanent effect or an area: some loops of it.
- How the selection panel orders the field groups after the look.
