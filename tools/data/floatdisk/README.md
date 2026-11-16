# Floating Disk's looks (#39)

`composition.json` says what each stage's pile is made of. `build.js` turns it into the game's files:

- `data\files`: the four creature models (`c_n5fd_empty`, `_small`, `_med`, `_large`), the three piles that fall
  when the disk ends (`plc_n5fd_small`, `_med`, `_large`, with their `.pwk`), and the disk's texture
  (`n5t_forcedisk.tga`);
- `data\rows`: `appearance.floatdisk.txt` (rows 14000-14003) and `placeables.floatdisk.txt` (rows 14001-14003).

## Rebuild

From the repo's root:

```
node tools\data\floatdisk\build.js
```

- It reads the game's models from your own install. It builds its mesh reader (`mdlmesh`) on the first run, which
  needs the VS 2019 Build Tools that `build.bat` uses.
- `--check` prints what it would build and writes nothing.
- It never overwrites a file it didn't make.
- The new files reach the game at the next install.

`node tools\data\floatdisk\check.js` checks the generator's own parts. It doesn't need the game.

## Swap in your own pile

A part names either a game model or a model file of your own:

```json
{ "model": "plc_b04", "at": [-0.07, 0.03, 0], "turn": 20, "scale": 0.85 }
{ "file": "parts/my_pile.mdl", "at": [0, 0, 0] }
```

To make a whole stage your own pile, give that stage a single part:

```json
"medium": [ { "file": "parts/medium_pile.mdl", "at": [0, 0, 0] } ]
```

Then rebuild.

**The file**

- The path is relative to this folder: `parts/my_pile.mdl` is `tools\data\floatdisk\parts\my_pile.mdl`.
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
| `floatdisk.js` | its pure parts: placing, the disk, the models' and rows' text |
| `check.js` | checks `floatdisk.js` |
| `mdlmesh.cpp`, `build_mdlmesh.bat` | the mesh reader, built from the repo's `src\data\vfx_model.cpp` into `build\floatdisk\` |
| `made.txt` | the textures the generator wrote (they can't carry its mark) |
