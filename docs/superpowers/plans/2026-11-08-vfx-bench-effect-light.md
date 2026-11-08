# VFX Bench: an Effect's Light (#145) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** an effect made or opened in the VFX bench can carry a light, set on a Light card:
- colour, radius and brightness;
- a flicker, or a fade in and out;
- kept by Save and Play in game;
- shown approximately in the preview.

**Architecture:**
- A light is a new part of the effect, beside its nodes and emitters, in the draft as a `[light <name>]` section.
- **The page side** (nwn5e-tools/vfx_bench) and **the game side** (nwn5e-ee `src/data/vfx_model.*`) read and write it, byte for byte alike.
- The game's model writer turns it into a light node of the effect's model, with its keys in the start / loop / end animations.
- The model readers keep a game model's light as a light instead of a dummy.
- The preview draws a glow, not lighting.

**Tech Stack:**
- Page: browser JS (core.js, sim.js, view.js, app.js), checks with node (check.js, check_effect.js).
- Game: C++17 in src/data (test.bat, tests/data/vfx_model_test.cpp).
- The dev layer (vfx_editor.cpp) only passes the effect through.

**Spec:** issue #145, quoting the owner ("can the vfx bench edit the light source of an effect?", "plan it"), and this session's proposal:
- a Light card with colour, radius and brightness;
- a flicker or a fade in and out over the start, loop and end;
- Save writes it, Play in game shows it;
- an approximate glow in the preview.

## Global Constraints

- **Shared file:** `src/data/vfx_model.h/.cpp` also holds the game session's tile-light code (light_sources).
  - Announce each edit to the game session first, naming the functions touched.
  - The work lands on `anim`; the game session merges and installs it.
- **Draft parity:**
  - core.js and the C++ WriteVfxDraft write the same bytes for the same effect.
  - check.js still reads and writes back every draft the game wrote (411 today) byte for byte.
  - A draft without a light is unchanged.
- **Every new rule gets a check that fails first,** plus a deliberate break (test.bat for C++, check_*.js for the page).
- **Never commit game files.** Facts read from them go in comments only.

## Review Focus

1. **A game effect with a light survives.** Opened in the bench, saved and played, it still lights, with the same colour and radius.
2. **No light, no change.** Drafts and models of effects without a light come out byte for byte as before.
3. **Seamless loops.** A flicker or a fade loops without a jump: whole cycles in the loop, the start and end keys continuous.
4. **A light under a moving node follows it.** That covers orbits, wobbles, and a hand via #144.
5. **Many lights, or a huge radius.** The game draws only so many dynamic lights at once, so the page warns past a sane limit instead of failing silently.

---

### Task 0: Research gate (read-only, then one game check)

**Find out (record each answer as a dated Addendum at the end of this plan):**
- **The game's own effect models that have a light node.** Use ReadVfxBinaryModel's node walk (GeometryNodes) with the node's type flags: a light is flag 0x002.
- **One such light's settings:**
  - flags: ambientonly, dynamic type, affect dynamic, shadow, light priority, fading light, flare radius;
  - controllers: colour 76, radius 88, multiplier 140, shadow radius 96, vertical displacement 100 - confirm the numbers;
  - animated keys, and in which animations.
- **The ASCII spelling the game accepts.** Write one test model by hand with a `node light` (static colour, radius, multiplier, plus a `multiplierkey` in its loop animation), Play it in game, and check that it lights the character and the ground.

**Stop if:** an ASCII light doesn't load, or doesn't light in game. Then report the finding before any further task.

### Task 1: The light in the draft (page and game, alike)

**Files:**
- nwn5e-tools `vfx_bench/public/core.js`, `vfx_bench/check_effect.js`, `vfx_bench/check.js`;
- nwn5e-ee `src/data/vfx_model.h/.cpp` (VfxLight, ReadVfxDraft, WriteVfxDraft - announced), `tests/data/vfx_model_test.cpp`.

**Interfaces (Produces):**
- **VfxLight** (`core.light()` on the page):
  - name, parent, position[3];
  - color[3] (0-1);
  - radius (m);
  - multiplier, shown as "Brightness";
  - shadow (bool);
  - keys: per animation, rows `t r g b` for colour, `t v` for radius and multiplier.
- **VfxEffect.lights:** a list.
- **The draft:** a `[light <name>]` section after the emitters, with the key lines
  - `parent`, `position`, `color`, `radius`, `brightness`, `shadow`,
  - `colorkey <anim>`, `radiuskey <anim>`, `brightnesskey <anim>`,

  written only when set.

- [ ] Checks first, all failing:
  - `LightDraftRoundTrips` (C++) and its page twin in check_effect.js;
  - `DraftWithoutLightUnchanged`;
  - check.js writes a lit draft with both writers and compares the bytes.
- [ ] Implement both readers and writers. Then test.bat, check.js and check_effect.js all pass.
- [ ] Breaks: a key line not written; brightness read as radius; the section after a light's keys lost.
- [ ] Commit (`feat: the VFX bench's effects carry a light in their draft (#145, 1/4)`).

### Task 2: The light in the model (game side)

**Files:** `src/data/vfx_model.cpp` (WriteVfxModel - announced), `tests/data/vfx_model_test.cpp`.

**Writes:**
- each VfxLight as an ASCII `node light` under its parent, in Task 0's spelling;
- its flags as Task 0 found the game's own: dynamic, affecting creatures, no flare;
- its keys in the impact / duration / cessation animations, as the emitters' are.

- [ ] `VfxModelWritesLight` (keywords, values, each animation's keys) and `VfxModelWithoutLightUnchanged`. Watch them fail first.
- [ ] Implement; test.bat passes; breaks caught.
- [ ] Commit (2/4).

### Task 3: A game model's light kept (game side)

**Files:** `src/data/vfx_model.cpp` (ReadVfxBinaryModel, ReadVfxAsciiModel - announced), tests.

**Reads:**
- a light node (flag 0x002 / `node light`) as a VfxLight instead of a dummy;
- its static controllers and its animations' keys.

**Fixtures:** a small hand-built binary light node and an ASCII one, in tests/data/fixtures. These are made by us, never copied from the game.

- [ ] Checks fail, then pass. The existing fixtures read as before.
- [ ] The dev layer's library export (`!vfxexport`) writes the game effects' lights into their drafts. check.js still round-trips every draft.
- [ ] Commit (3/4).

### Task 4: The Light card, the preview, Play in game

**Files:** nwn5e-tools `vfx_bench/public/app.js`, `index.html`, `sim.js`, `view.js`, `README.md`, `check_effect.js`.

**The card:**
- **Add a light** and **Remove**;
- **Hangs under** (the root, a node, an emitter's node);
- position, colour picker, radius (m), brightness, shadow;
- **Flicker:** how much, how fast, pattern. It writes brightness keys in the loop, in whole cycles, seeded like the wobble.
- **Fade in / fade out:** brightness keys in the start and the end.

**The preview:** a soft glow on the ground (radius-wide, its colour, fading out) and a small halo at the light, both following its keys.
- **Warning past a sane limit:** more than a few lights, or a radius over about 20 m.

- [ ] Checks first:
  - the flicker loops seamlessly;
  - the fades reach 0 at their ends;
  - sim's light value at time t;
  - the card's draft round-trips.
- [ ] Implement; all checks pass; breaks caught.
- [ ] Play in game. Save makes the model with the DLL's writer (Tasks 2-3), so the game session's next install carries it. Send the test rows with "anim ready".
- [ ] Commit (4/4). Then the README, a TOOLS-PLAN entry, and #145's comment with the test matrix.

### After the tasks

- A fresh reviewer (the most capable model) on the whole range, with this Review Focus.
- One fix pass under TDD.
