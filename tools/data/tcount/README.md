# The target-count marker: build.js

#171's target window (Magic Missile's darts, Scorching Ray's rays, each assigned to a creature). The owner,
2026-11-19: "it should be a permanently visible indicator on top of each enemy saying how many darts/bolts are
assigned to them, not how many are left"; the look, the glow and the size: "A, A, as you said" - a bold white number
with a dark outline on a small dark-red disc, about 0.4 m, 0.35 m clear above the head; the red glow stays the "picked"
mark.

`build.js` makes the 11 markers, counts 1 to 11, into `data\files`: `n5_tcnt_01` .. `n5_tcnt_11`, each a texture
(`.tga`), its TXI (`.txi`) and an effect model (`.mdl`). Their rows are `data\rows\visualeffects.tcount.txt` (2200-2210);
the game's call is `TargetCountMarkerVfx(count)` (src/core/anim_target_counts.h).

## Run

From anywhere:

```
node tools\data\tcount\build.js [--out <folder>]
node tools\data\tcount\check.js
```

- `build.js` writes into `data\files` (or `--out`), only the 33 files, and only those whose bytes changed (`written` /
  `unchanged`; a model or TXI that differs only in its line ends is unchanged - git's autocrlf checks them out CRLF).
- `check.js` builds twice in memory and checks everything below, then that `data\files` holds exactly those bytes. Exit
  0 or 1. It writes nothing.

## Why the outputs are committed

They are nwn5e's own art, made from numbers in this file, and the same on every run - the pattern of the death-save
marker (`death_save_marker.py`: `n5_ds_*.tga` and models), the stealth rings and cones and the Magic Mark, all committed
in `data\files`. `build_derived.js` is for **art made from the game's files**, built from the player's own game at build
time and never committed; the Composer's soft textures (`n5s_*`) are ignored only because each is a function of a
recipe's colour, re-made at every build while colours are tried, so stale ones would pile up. Neither applies: 11 fixed
names, no game file, no recipe. Committed, `build.bat` copies them with `data\` (unchanged), a `NWN5E_DERIVED=skip` or
`public` build still has them, and `check.js` keeps them in step with the generator.

## The textures (`n5_tcnt_NN.tga`)

- 128 x 128, 32-bit, uncompressed, bottom-up with 8 alpha bits (descriptor 0x08), as the game's own TGAs.
- A dark-red disc (124 18 22), 122 px across with a darker 3 px rim (70 8 12), alpha 0 outside it (its colour there the
  rim's, so filtering bleeds nothing bright).
- The number in white, stroked 11 px wide, with a 4 px dark outline (18 6 6) - readable on grass, snow, stone and night
  (`check.js` checks no white pixel has the disc's red within 2 px).
- **The digits are drawn here:** each a few centrelines - straight lines and circular arcs, in a box one unit tall -
  stroked round with an antialiased edge (`GLYPHS`, `distance`). No font, no image; `build.js` loads only `fs` and
  `path` (checked). 10 and 11 use narrow 1 and 0 so both digits fit the disc at the single digits' height (checked: as
  many separate white shapes as digits, all inside the disc).

## The TXI (`n5_tcnt_NN.txi`)

`decal 1`. The game draws a mesh whose bitmap's TXI says decal **unlit** - `globalColor (1, 1, 1, alpha) x texture`, its
material and any glow on the creature skipped, no fog (SHP-8's diagnosis of `Material::ParseField`, `PartTriMesh::Mat`,
`RenderFlat`) - so the marker reads the same by day and night, with or without enhanced lighting. The models' material
(selfillum 1, ambient and diffuse 0) gives the same picture should the TXI be missing.

## The models (`n5_tcnt_NN.mdl`)

**Geometry, not a camera-facing sprite: the target window runs paused.** The TC diagnosis (the engine read; docs/
TOOLS-PLAN.md, "The target-count marker"): pausing calls `Scene::DisableAnimations`, and `Scene::DoEmitterBucket` ticks an emitter only while the
scene animates (or its Gob is force-animated - the walk-to ring only); particles are born only in that tick, and an
emitter with none isn't drawn. So a particle applied while paused appears only on unpause. A mesh is drawn the moment the
client attaches the effect (`Gob::Render` waits for nothing), and removing the effect takes it out of the scene at once
(`StopVisualEffect` -> `DetachVisualEffect`): the number appears, changes and vanishes while paused.

The engine has no camera-facing mesh, so each model is **four discs facing out round the head** - north, east, south,
west: four trimeshes, each a 0.4 m square carrying the disc, 0.2 m out from the head's axis, its centre 0.55 m over the
head node (0.35 m clear above it).

- **Never mirrored:** each disc is one-sided (counter-clockwise from outside; the game culls back faces of a plain
  textured mesh), so a disc is only ever seen from the side it faces, where its u runs to the viewer's right and its v
  up. The far discs are culled; nothing shows through.
- **Never upside down:** the row's head attach is position only and the client turns the effect to the world's axes
  (OrientWithObject 0, #144's finding): the discs stand upright whatever the head does.
- **From any side:** one disc faces the camera within 45 degrees; between two (45 degrees off both) both show, each at
  71% of its width - the number on two faces of a box. From above (a steep camera) the discs are seen foreshortened.
- **No keys:** a model's animation waits for the clock, which stops while paused, so nothing in it is keyed (impact,
  duration are empty) - what is drawn is the rest geometry.
- **No cessation (#171, TC-1):** while a model's cessation runs the client re-attaches it every frame, on the object's clock
  (stopped while paused), then lingers 3 s - so a removed marker stayed on screen and the old numbers overlapped the new.
  With no cessation the client detaches it and deletes it the next frame, paused or not. Never a cessation of length 0: its
  timer would be 0, the effect would never finish - invisible, never deleted. check.js fails any model with one.
- **One inert emitter** (`_anchor`: Single, birthrate 0 - it never makes a particle - and lifeExp 0 - the game draws
  none): the VFX Bench's library (the dev layer's export) lists only models with an emitter, so this lists the markers
  on the bench's nwn5e tab (and the Game tab, as rows 2200-2210 on the head). It shows nothing anywhere.
- Material: opaque cut-out (transparencyhint 0: the disc's clear corners are cut at alpha 0.2), alpha 1, shadow 0.
- Each starts with nwn5e's mark (`# nwn5e (tools/data/tcount/build.js)`), so the model readers take it as nwn5e's own.

## The bench

The nwn5e tab lists `n5_tcnt_01` .. `_11` after a game start (opens on the feet: pick **the head** in the Effect card to
see it where the game puts it), the Game tab rows 2200-2210 (on the head already). Their texture reaches the bench's
library with the game's effects - `!vfxexport` in the game's chat copies it once the install has it.
