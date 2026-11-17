# The Composer's projects (#39)

A project is something built from the game's parts: a **placeable**, or a **mesh for spells**. The VFX Bench's
Composer edits it, and `build.js` turns it into the game's files. Each project has one or more **variants**, each
one model.

The Floating Disk isn't one of these files. It keeps `tools\data\floatdisk\composition.json` and its own generator
(`tools\data\floatdisk\README.md`).

## The project file

`tools\data\composer\<name>.json`, committed with what it builds:

```json
{
  "_about": ["..."],
  "kind": "placeable",
  "name": "crates",
  "guide": { "shape": "circle", "diameter": 1.2, "height": 0 },
  "placeable": { "copy": 2, "use": [0, -0.7], "walk": "blocks" },
  "variants": [
    { "name": "1", "model": "plc_n5crates_1", "row": 14100, "parts": [
      { "note": "a crate", "model": "plc_a03", "at": [0, 0, 0], "turn": 15, "scale": 0.5 }
    ] },
    { "name": "2", "model": "plc_n5crates_2", "row": 14101, "parts": [
      { "model": "plc_b06", "at": [0, 0, 0] }
    ] }
  ]
}
```

- **`name`:** 1-8 lower-case letters or digits.
- **`kind`:** `placeable` or `spellmesh`.
- **`guide`:** an outline on the ground the parts are placed on, never part of the model.
  - `shape` is `circle` (`diameter`), `rect` (`width` along x, `length` along y) or `none`.
  - `height` is where it sits. A part's `at` z is measured from it.
  - The model's origin is the guide's centre at its height. A spell mesh's guide may sit below 0, so a chunk turns
    about its middle.
- **`variants`:** 1 to 9, each named with 1-4 letters or digits.
  - Each `model` is made from the project's name and isn't read back: `plc_n5<name>` or `n5m_<name>`, with `_<k>`
    (from 1) when there's more than one variant. Every model name fits 16 characters.
  - A variant with no parts can be saved in the file but isn't built.
- **The parts** are composition.json's: `note`, `model` (a game model, or a list of them for a weapon's pieces) or
  `file`, `at`, `turn`, `scale`, `tilt`.
  - A `file` is your own `.mdl`, relative to this folder, so `parts/my_crate.mdl` is `tools\data\composer\parts\my_crate.mdl`.
  - The rules for your own files (units, axes, textures) are the Floating Disk's (`tools\data\floatdisk\README.md`).

## What each kind builds

**A placeable:** each variant becomes one of the game's placeables.

- `data\files\<model>.mdl`: the parts' meshes, laid out as the game's junk placeables are (the `_hand`, `_head`,
  `_head_hit`, `_impact` and `_ground` nodes), its base on the ground at the guide's height.
- `data\files\<model>.pwk`: where a creature walks to use it.
  - `placeable.use` is the use point `[x, y]`. Left `null`, it's 0.1 m past the guide's front, at -y (the front, as
    the game's own placeables have it).
  - `walk: "through"` (the default) adds no ground mesh, as the game's junk has none.
  - `walk: "blocks"` adds the guide's outline as ground nothing walks through (`<MODEL>_wg`, faces of material 7), as
    the game's crates and barrels have. A circle is 16 sides; a rectangle its 4 corners. It needs a guide.
- `data\rows\placeables.<name>.txt`: one row a variant.
  - Each row is a copy of a game row with its Label (`N5_<Name>`, `N5_<Name>_<k>`) and ModelName changed.
  - The row copied is `placeable.copy` when set. Otherwise it's the first placeable part's own row (the lowest
    `placeables.2da` row with its model), else 189 (TreasureSml). A pile of crates sounds like a crate.
- Textures of your own files' parts that the game doesn't have are copied into `data\files`, and `made.txt` notes them.

**A spell mesh:** each variant becomes a draft of its meshes, `<saved>\<model>.vfx`, in the VFX Bench's
`vfxedit\saved\`. The game writes its model there when it runs. An emitter's chunk can name it, and an effect can
carry it as a mesh beat. Textures of your own files' parts are copied in beside the draft.

## Rows: 14100-14199

A placeable's rows come only from 14100-14199. The VFX Bench gives them at the first save, and the project keeps them.
A row must be free everywhere except this project's own rows file. `build.js` refuses one that's set by:

- another `data\rows\placeables*.txt`;
- a `data\looks\placeables*` file;
- the game's own `placeables.2da` (its blank user rows don't count).

A row taken twice makes the launch check skip a whole file.

## Build

The VFX Bench's **Save and build** runs it. By hand, from the repo's root:

```
node tools\data\composer\build.js tools\data\composer\crates.json
node tools\data\composer\build.js tools\data\composer\web.json --saved <nwn5e build>\vfxedit\saved
```

- `--check` prints what it would build and writes nothing. A variant without a row yet is fine there.
- `--json` prints the summary as one line of JSON instead of text (an error as `{ "error": ... }`). Its variants carry
  what the bench's stage shows: parts, meshes, triangles, how tall, the top, and each part's size and reach past the
  guide.
- `--game`, `--tools`, `--reader`, `--files` and `--rows` work as the Floating Disk's `build.js` options.
- It never overwrites a file it didn't make: a model, `.pwk`, rows file or draft must carry its mark line. A texture
  must be listed in `made.txt` or, beside a draft, named by this project's draft.
- Every file is checked before any is written: a refused build writes nothing.
- When a model's name changes (a second variant added: `plc_n5crates` becomes `plc_n5crates_1`), the old files are
  listed as no longer used. They aren't deleted.
- It refuses a model name the game already has.

`node tools\data\composer\check.js` checks the pure parts (`project.js`). It doesn't need the game.

`node tools\data\composer\check_build.js` builds on a temporary copy and checks the whole generator. It needs the
game, and prints `skip` without it.
