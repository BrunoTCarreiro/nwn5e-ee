# Floating Disk's looks (#39)

`composition.json` says what each stage's pile is made of. `build.js` turns it into the game's files:

- `data\files`: the four creature models (`c_n5fd_empty`, `_small`, `_med`, `_large`), the three piles that fall
  when the disk ends (`plc_n5fd_small`, `_med`, `_large`, with their `.pwk`), and the disk's texture
  (`n5t_forcedisk.tga`);
- `data\rows`: `appearance.floatdisk.txt` (rows 14000-14003) and `placeables.floatdisk.txt` (rows 14001-14003).

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
7. For the game: the animation session commits `composition.json` and the rebuilt files on anim; the game session
   merges and installs.

**The first Save rewrites two numbers.** `compositionText` writes a number as JSON does, so the first Save changes
`"rim": [0.8, 0.92, 1.0]` to `[0.8, 0.92, 1]` and the large stage's `"at": [0.0, 0.32, 0]` to `[0, 0.32, 0]`. Commit
that once. After it, a Save with nothing changed leaves the file as it is.

The Composer's generator changes left the disk's files as they were: a full rebuild of the committed composition still
reports all 13 files unchanged (`check_build.js`).

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
- The new files reach the game at the next install.

`node tools\data\floatdisk\check.js` checks the generator's own parts. It doesn't need the game.

`node tools\data\floatdisk\check_build.js` rebuilds on a temporary copy and checks the whole generator: a rebuild
changes nothing, `--json` agrees with the text, a weapon's pieces read as one, a refused build writes nothing, a file in
the way it didn't make is refused (named from the repo's root), a file it made that changed is rebuilt, and a setup
failure is still one JSON error line. It needs the game (it prints `skip` without it).

## Swap in your own pile

A part names either a game model or a model file of your own:

```json
{ "model": "plc_b04", "at": [-0.07, 0.03, 0], "turn": 20, "scale": 0.85 }
{ "file": "parts/my_pile.mdl", "at": [0, 0, 0] }
{ "model": ["wswls_b_011", "wswls_m_011", "wswls_t_011"], "at": [0, 0, 0], "tilt": [0, 90] }
```

The last is a weapon: its three pieces, read as one model.

To make a whole stage your own pile, give that stage a single part:

```json
"medium": [ { "file": "parts/medium_pile.mdl", "at": [0, 0, 0] } ]
```

Then rebuild.

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

## The files here

| file | what it is |
|---|---|
| `composition.json` | the stages (yours to edit) |
| `build.js` | the generator |
| `floatdisk.js` | its pure parts: placing, the disk, the models' and rows' text (it loads in a page too: the VFX Bench's Composer places with it) |
| `check.js` | checks `floatdisk.js` |
| `reader.js` | reads the parts' meshes and the textures from the game or beside a file (what `build.js` and the VFX Bench share) |
| `files.js` | the file plumbing this `build.js` and the Composer's share: the options, loading nwn5e-tools, shipping your own textures, the one ownership rule they write by |
| `check_build.js` | checks `build.js` end to end on a temporary copy (needs the game) |
| `mdlmesh.cpp`, `build_mdlmesh.bat` | the mesh reader, built from the repo's `src\data\vfx_model.cpp` into `build\floatdisk\` |
| `made.txt` | the textures the generator wrote (they can't carry its mark) |
