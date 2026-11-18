# Floating Disk's looks (#39)

`composition.json` says what each stage's pile is made of. `build.js` turns it into the game's files:

- `data\files`: the four creature models (`c_n5fd_empty`, `_small`, `_med`, `_large`), the three piles that fall
  when the disk ends (`plc_n5fd_small`, `_med`, `_large`, with their `.pwk`), the disk's texture
  (`n5t_forcedisk.tga`), a dyed part's baked texture (`n5p_*.tga`, Dyed parts below), and a shape's soft texture
  (`n5s_*.tga`, Shapes below);
- `data\rows`: `appearance.floatdisk.txt` (rows 14000-14003) and `placeables.floatdisk.txt` (rows 14001-14003).

The seven models are made from the game's files, and none is committed (the owner, 2026-11-17: "go with 1, remove the
six files from main", then "go with your recs for the three questions"; `.gitignore`):

- six of them are made of the game's own meshes: `c_n5fd_small`, `_med`, `_large` and `plc_n5fd_small`, `_med`,
  `_large`;
- `c_n5fd_empty`'s disk mesh is original, but it carries `c_invsguy`'s node names and positions, which its
  supermodel needs.

Nor is a baked texture: it's painted from the game's own PLT and palettes. Nor is a shape's soft texture: it's
original, but build output, its bytes a pure function of its colour (Shapes, below). `build.bat` makes all of them for
every build from this file and your own game (`tools\data\build_derived.js`, `tools\data\README.md`). The `.pwk`
files, the disk's texture and the rows are made from scratch and stay committed.

## Compose it in the VFX Bench

The VFX Bench's Composer edits `composition.json` on a stage and rebuilds from it (nwn5e-tools `vfx_bench\README.md`,
"The Composer"):

1. Double-click nwn5e-tools' `vfx_bench\vfx_bench.bat` (close an open VFX Bench window first), then press **Composer**
   in the bar. It opens the Floating Disk on Small, or the project you had open last: pick **Floating Disk (#39)** in
   the bar's list.
2. The tabs on the stage are the stages: Empty, Small, Medium, Large. Empty takes no parts: the generator makes it, the
   disk alone.
3. The guide card's circle is the disk: its Diameter and Height are `disk.diameter` and `disk.height`. The disk's
   other settings (colour, rim, alpha, glow, thickness, sides, texture) stay in this file as they are, and no disk is
   drawn on the stage.
4. Add parts from the drawer (the game's placeables and items, and your own files - Swap in your own pile, below), and
   move, turn, scale and stack them with the handles. The stage places each with `floatdisk.js`'s own `placePart`, so
   what it shows is what's built.
5. **Play in game** shows the stage beside your character in the running game, before any install.
6. **Save and build** writes `composition.json` (with `compositionText`) and runs `build.js --json` on it. The status
   says what was rebuilt.
   - If the build is refused, its reason is shown, and neither `composition.json` nor any built file changes.
   - If the file changed on disk since the bench opened it, you're asked before it's saved over.
7. For the game: the animation session commits `composition.json` and the rebuilt files git doesn't ignore (the rows,
   the `.pwk` files, the texture) on anim; the game session merges, and its build makes the seven game-derived models
   before it installs.

**The first Save rewrites two numbers.** `compositionText` writes a number as JSON does, so the first Save changes
`"rim": [0.8, 0.92, 1.0]` to `[0.8, 0.92, 1]` and the large stage's `"at": [0.0, 0.32, 0]` to `[0, 0.32, 0]`. Commit
that once. After it, a Save with nothing changed leaves the file as it is.

The Composer's generator changes left the disk's files as they were: a full rebuild of the committed composition still
reports all 13 files unchanged, and its generated textures (`check_build.js`).

## Rebuild

From the repo's root:

```
node tools\data\floatdisk\build.js
```

- It reads the game's models from your own install. It builds its mesh reader (`mdlmesh`) on the first run, which
  needs the VS 2019 Build Tools that `build.bat` uses.
- `--check` prints what it would build and writes nothing.
- `--json` prints the summary as one line of JSON instead of text (an error as `{ "error": ... }`); the VFX Bench's
  Composer reads it.
- It never overwrites a file it didn't make.
- The new files reach the game at the next install: `build.bat` makes the models from this recipe and your own game
  into its own folder (`tools\data\build_derived.js`, `tools\data\README.md`).

`node tools\data\floatdisk\check.js` checks the generator's own parts. It doesn't need the game.

`node tools\data\floatdisk\check_build.js` rebuilds on a temporary copy and checks the whole generator: a rebuild
changes nothing, `--json` agrees with the text, a weapon's pieces read as one, a refused build writes nothing, a file in
the way it didn't make is refused (named from the repo's root), a file it made that changed is rebuilt, a setup
failure is still one JSON error line, a dyed part is baked beside the models, which name it, and a dome's soft
texture is written beside the models and (with `--drafts`) the drafts, owned by its name. It never writes the real
`data\files` or the bench build's `vfxedit\saved\`. It needs the game (it prints `skip` without it).

## Swap in your own pile

A part names a game model, a model file of your own, or a shape - just one of them:

```json
{ "model": "plc_b04", "at": [-0.07, 0.03, 0], "turn": 20, "scale": 0.85 }
{ "file": "parts/my_pile.mdl", "at": [0, 0, 0] }
{ "model": ["wswls_b_011", "wswls_m_011", "wswls_t_011"], "at": [0, 0, 0], "tilt": [0, 90] }
{ "shape": { "type": "dome", "radius": 0.3 }, "at": [0, 0, 0] }
```

The third is a weapon: its three pieces, read as one model. The last is a shape the generator makes (Shapes, below).

To make a whole stage your own pile, give that stage a single part:

```json
"medium": [ { "file": "parts/medium_pile.mdl", "at": [0, 0, 0] } ]
```

Then rebuild. A helmet (a dyed part) takes its colours in the same line: Dyed parts, below.

**The file**

- The path is relative to this folder: `parts/my_pile.mdl` is `tools\data\floatdisk\parts\my_pile.mdl`.
- In the VFX Bench's Composer, the parts list's Yours tab lists this folder's `parts\` and
  `tools\data\composer\parts\`. A file from the second is written `../composer/parts/<file>`.
- An ASCII `.mdl` (what NeverBlender exports) or a compiled one.
- Every mesh in it is used: trimesh, danglymesh, skin and animmesh, in its rest pose. Meshes with `render 0` and
  walkmeshes are left out.
- Animations, emitters and lights are dropped.

**Units and axes**

- Metres, z up.
- Model the base at z 0, centred on the origin. +y is the disk's front, the way the creature faces.
- The generator doesn't need that exactly. It always centres the pile's footprint on the part's `at` x and y, and puts
  its lowest point `at` z above the disk's top.
- `turn`, `tilt` and `scale` work as they do for a game model.
- Apply any scale in Blender before exporting. A node scaled other than 1 is refused.

**Textures**

- Each texture is looked up by name, first in the game's archives, then beside the file (`<name>.tga` or `.dds`,
  with its `.txi` if there is one).
- One found beside the file is copied into `data\files` so it ships with the models, and `made.txt` notes it.
- A texture found in neither place stops the build.

**Sizes**

- The disk is 0.9 m across (`disk.diameter`), and its top is 0.9 m above the ground (`disk.height`).
- A pile that reaches past 0.45 m from the centre hangs over the rim. The summary says by how much.
- The figure beside it is 2 m tall.

## Dyed parts

A dyed part is one whose texture the game has only as a PLT: 33 of its helmets, and `gi_cloak01`. A PLT is NWN's
dyeable texture, painted from an item's colours, and placeables have none - so the game would draw it untextured.

- **Its colours:** `colours` holds palette numbers 0-175 for `metal1`, `metal2`, `leather1`, `leather2`, `cloth1` and
  `cloth2`. Left out, the defaults (steel, a darker steel trim, browns, dark grey). Only the ones that differ are
  written.

  ```json
  { "model": "helm_001", "at": [0, 0, 0], "colours": { "metal1": 5, "cloth2": 40 } }
  ```

- **The bake:** `build.js` paints the PLT from your game's palettes in the part's colours into a texture of its own,
  `n5p_` and 12 hex digits of the PLT's name and its colours (`helm_001` at the defaults is `n5p_2cd3d725dedb`). The
  models name it instead of the PLT. Other colours make another name, and parts with the same PLT and colours share
  one file.
- **Where it goes:** into `data\files` beside the models, with nothing more to give (`node tools\data\floatdisk\build.js`
  as ever). It's art made from the game's files, so `.gitignore` keeps it out of the repo (`/data/files/n5p_*`), and
  `made.txt` doesn't list it. Like the models, `build.bat` makes it for every build (`tools\data\build_derived.js`).
- **The summary** names each bake, `--json` lists them under `baked`:

  ```
    texture n5p_2cd3d725dedb: helm_001 dyed with metal 1 0, metal 2 3, leather 1 3, leather 2 2, cloth 1 22, cloth 2 23 - baked into data/files
  ```

- **Whose it is:** its name says. Only the bake makes such names, and the name fixes the picture, so a file of that
  name in the way is rewritten. Bakes of colours no longer used stay in `data\files` (ignored, never installed: the
  build makes its own).

## Tint

A part's `tint` is a colour `[r, g, b]` (each 0-1) its textures take:

```json
{ "model": "plc_a03", "at": [0, 0, 0], "tint": [0.502, 1, 0.502] }
```

- **What it does:** every one of the part's meshes has its material - `ambient`, `diffuse` and `selfillumcolor` -
  multiplied by it (to 0.00001). The game lights a textured mesh as texture x (selfillum + ambient x the area's light +
  diffuse x the lights), so the material tints the texture. The game's own blue glyph globe (`vim_cntglobe`) is a grey
  texture tinted that way.
- **White is none:** `[1, 1, 1]`, the default, isn't written, and leaves the material as the model has it.
- **Where:** any game model, weapon or file of yours, both generators alike (`floatdisk.js`'s `placePart` applies it). A
  dyed part is tinted over its dyes. A shape takes none (it's refused): its `colour` is its colour.
- **A spell mesh's meshes** (the Composer's) become a draft's planes, and the game's model writer draws a plane at the
  draft's default material (ambient 0.2, diffuse 0.8: a mesh with no material of its own) at full, 1 1 1
  (`drawnMaterial`). So there a tinted mesh at that default is lifted to full before its tint (`placePart`'s `planes`);
  untinted it's left as it is. The Floating Disk's models, like a placeable's, are written as they are.
- In the VFX Bench's Composer it's the part card's **Tint** (None takes it off).

## Shapes

A part's `shape` is a shape the generator makes instead of a model: `{ "type": <dome, sphere, cylinder, cone, disc or
ring>, "radius": <m> }` and, where they differ from the defaults:

- its own fields: `height` (a dome's the radius, a sphere's, a cylinder's and a cone's twice it; a disc and a ring have
  none), a ring's `inner` (0.85 of the radius, to the millimetre), `segments` (32), `sides` (`both`);
- every type's: `mapping` (`wrap`), `repeat_around` (1), `repeat_up` (1), `colour` (`[0.6, 0.8, 1]`), `alpha` (0.35),
  `glow` (1), `texture` (`soft`).

It has no `scale` (its radius and height size it) and no `colours` (its colour is on the shape). Only what differs
from the defaults is written; a key not its type's (a disc's `height`, a dome's `inner`) is refused.

```json
{ "note": "zone", "shape": { "type": "dome", "radius": 4.57 }, "at": [0, 0, 0] }
{ "shape": { "type": "dome", "radius": 1, "height": 0.4, "colour": [1, 0.85, 0.4] }, "at": [0, 0, 0] }
{ "note": "edge", "shape": { "type": "ring", "radius": 4.57, "inner": 4.2 }, "at": [0, 0, 0.02] }
```

- **The types**, each with its base on the ground at z 0, centred on the origin:
  - `dome`, a half sphere, `height` up;
  - `sphere`, the dome's rings mirrored below it: `height` tall (twice the radius: a ball), its middle at half that;
  - `cylinder`, an open tube `height` tall (Moonbeam's and Flame Strike's columns);
  - `cone`, a ring on the ground to a point `height` up, its base open (a cone spell's, tilted onto its side);
  - `disc`, flat on the ground (a glyph's place);
  - `ring`, a flat band from its radius in to `inner` (0.01 to below the radius): a zone's edge on the ground.
  - A disc and a ring are flat: their outside is their top. Their `at` is 2 cm up in the VFX Bench's Composer, so they
    don't flicker with the ground at its own height.
- **Its mesh:**
  - `segments` (8-96) go round it, and a dome has a quarter of that many rings up it (a sphere twice that, less one, and
    a point at each end); a cylinder, a cone, a disc and a ring have their two edges only.
  - `sides: "both"` (the default) is seen from inside too (a flat one's from below): the inside's faces come first, each
    side with its own vertices and its own smoothing group (the inside 2, the outside 1). `"outside"` is seen from
    outside only.
- **How its texture lies on it** (`mapping`):
  - `wrap` (the default) goes round it from +x and up it from its base: a dome's and a cone's from the ground ring to
    the top, a sphere's from its bottom to its top, a cylinder's from the ground to its top. A disc's and a ring's run
    from the rim in: from the rim (a ring's outer edge) to a disc's centre, or a ring's inner edge. `repeat_around`
    (1-32) and `repeat_up` (1-16) tile it, so a pattern keeps its size on a big shape.
  - `above` lays it on from overhead, the picture's square across the shape's width (a ring's outer). The repeats don't
    apply there, and aren't written. It suits a low dome, a disc or a ring: a glyph shows as a sigil seen from above.
- **Its texture** (`texture`):
  - `soft` is generated: its colour, brighter and more opaque where `wrap` starts (the ground ring; a sphere's bottom,
    a disc's rim, a ring's outer edge: an area's edge), fading towards where it ends, made for `wrap`. It's named `n5s_`
    and 12 hex digits of its colour (to 0.001: `n5s_8c757fb28f99` for the default colour),
    and it's original, made from numbers. It's painted from the colour as it's named, so a name fixes the picture.
  - `none` is its colour only.
  - A texture's name (lower-case letters, digits and `_`, at most 16: the game's own, from the bench's library) is
    tinted by `colour`, lit by itself as far as `glow` goes: its `selfillumcolor` is `colour` x `glow`, its `ambient`
    and `diffuse` `colour` x (1 - `glow`). At Glow 1 it's the texture times the colour in any light (the game clamps a
    mesh's light at 1, so a colour on top of its glow washed a pale one out to white by day); at 0 it's lit by the
    scene, as a placeable is. Nothing is generated or copied for it. It can't be `null` (the game reads that as no
    texture: use `none`) or start `n5s_` (those names are made, not picked). One the game has only as a PLT (a dyed
    texture: a helmet's) is refused at build time: a shape takes no dyes, so pick a TGA or DDS.
- **Its look:** `colour` `[r, g, b]` (each 0-1), `alpha` (0.02-1: the game's globes go to about 0.3), `glow` (0-1: how
  much it lights itself, at 1 the same by day and night). Soft and `none` keep `ambient` and `diffuse` at white (Soft)
  or the colour (`none`), `selfillumcolor` lit by `glow`. A soft or library texture, or an alpha below 1, is drawn
  see-through.

**A shape's texture**

- **Where it goes:** `build.js` makes the mesh from the shape's numbers (no game model, no mesh reader). It writes a
  soft texture beside what names it: into `data\files` with the models (or `--files`), and with `--drafts` beside the
  stages' drafts too. Shapes of one colour share one file.
- **It's build output,** like a bake: original (generated from numbers), but its bytes follow from the recipe's
  colour alone, so `.gitignore` keeps it out (`/data/files/n5s_*`) and only the recipe is committed - no stale
  textures pile up while colours are tried. `made.txt` doesn't list it, and `build.bat` makes it with the models for
  every build (`tools\data\build_derived.js`).
- **Whose it is:** its name says. Only the generator makes `n5s_` names, and the name fixes the picture, so a file of
  that name in the way is rewritten.
- **The summary** names it, and `--json` lists it under `shapes` (`texture`, `colour`):

  ```
    texture n5s_a3caa0709dda: a shape's soft texture, colour 1 0.85 0.4 - written into data/files
  ```

- `none` and a library texture write nothing: the model names the library's texture, and the game has it.

## The files here

| file | what it is |
|---|---|
| `composition.json` | the stages (yours to edit) |
| `build.js` | the generator |
| `floatdisk.js` | its pure parts: placing, the disk, the shapes (their meshes and soft textures), the models' and rows' text (it loads in a page too: the VFX Bench's Composer places with it) |
| `check.js` | checks `floatdisk.js` |
| `reader.js` | reads the parts' meshes (a shape's from `floatdisk.js`) and the textures from the game or beside a file, and bakes a dyed part's PLT (what `build.js` and the VFX Bench share) |
| `files.js` | the file plumbing this `build.js` and the Composer's share: the options, loading nwn5e-tools, a dyed part's bakes, a shape's soft texture, shipping your own textures, the one ownership rule they write by |
| `check_build.js` | checks `build.js` end to end on a temporary copy (needs the game) |
| `mdlmesh.cpp`, `build_mdlmesh.bat` | the mesh reader, built from the repo's `src\data\vfx_model.cpp` into `build\floatdisk\` |
| `made.txt` | the textures the generator wrote (they can't carry its mark) |
