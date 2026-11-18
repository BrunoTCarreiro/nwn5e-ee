# The Composer's projects (#39)

A project is something built from the game's parts: a **placeable**, or a **mesh for spells**. The VFX Bench's
Composer edits it, and `build.js` turns it into the game's files. Each project has one or more **variants**, each
one model.

The Floating Disk isn't one of these files. It keeps `tools\data\floatdisk\composition.json` and its own generator
(`tools\data\floatdisk\README.md`).

## The project file

`tools\data\composer\<name>.json`, committed with what it builds - except a placeable's models (`plc_n5<name>*.mdl`,
made of the game's parts) and the textures it generates (`n5p_*`, `n5s_*`), which `.gitignore` keeps out and
`build.bat` makes for every build (`tools\data\README.md`):

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
  - A variant with no parts can be in the file (New project writes one), but `build.js` refuses the whole build
    while one has none (`variants[k].parts: at least one part`): add a part before the first Save and build.
- **The parts** are composition.json's: `note`, `model` (a game model, or a list of them for a weapon's pieces),
  `file` or `shape` (Shapes, below), `at`, `turn`, `scale`, `tilt`, a dyed part's `colours`, a `tint`, and a `motion`
  (Moving textures, below).
  - A `file` is your own `.mdl`, relative to this folder, so `parts/my_crate.mdl` is `tools\data\composer\parts\my_crate.mdl`.
  - The rules for your own files (units, axes, textures) are the Floating Disk's (`tools\data\floatdisk\README.md`).
  - A dyed part (a helmet: the game has its texture only as a PLT) is baked into a texture of its own, `n5p_` and 12
    hex digits, in its `colours` (the Floating Disk's README, "Dyed parts"). The model or draft names the bake.
  - A `tint` `[r, g, b]` (each 0-1; white, none, isn't written) multiplies every one of the part's meshes' material
    (`ambient`, `diffuse`, `selfillumcolor`), so its textures take the colour in game: `"tint": [0.502, 1, 0.502]` is a
    pale green crate. A placeable's model and a spell mesh's draft alike, a dyed part over its dyes; a shape takes none
    (its `colour` is its colour). The Floating Disk's README, "Tint".
  - On a spell mesh, a mesh with no material of its own (the draft's defaults, ambient 0.2 and diffuse 0.8) is drawn at
    full in game - the game's model writer, `WriteVfxModel`, writes such a plane `ambient 1 1 1` / `diffuse 1 1 1` - so a
    tint multiplies that: its draft carries the tint itself (`"tint": [1, 1, 0.999]` gives `1 1 0.999`, not `0.2 0.2
    0.1998`). Untinted it's written as before. A placeable's model is written as it is: its 0.2 / 0.8 times the tint.

## Shapes

A part's `shape` is a shape the generator makes instead of a model - a dome (a half sphere) to show a spell's area, say:
`{ "type": "dome", "radius": <m> }` and, where they differ from the defaults:

- `height` (the radius: a half sphere), `segments` (32), `sides` (`both`: seen from inside too; or `outside`);
- `mapping` (`wrap`: round it and up it; or `above`: laid on from overhead), `repeat_around` (1, up to 32) and
  `repeat_up` (1, up to 16), which tile a wrapped texture;
- `colour` (`[0.6, 0.8, 1]`), `alpha` (0.35), `glow` (1), `texture` (`soft`, `none`, or a library texture's name);
- `blend` (`normal`; or `additive`, M1b): how the game draws a Soft texture. Additive writes a TXI beside it
  (`blending additive`, as the game's own faint globes and shields have): it adds light - faint, glowing, see-through,
  never cut at alpha 0.2, nothing behind hidden. The VFX Bench writes `additive` on every shape it adds; one written
  without it is Normal, as before. A library texture follows its own TXI; `none` has no texture to carry one.

```json
{ "note": "zone", "shape": { "type": "dome", "radius": 4.57, "alpha": 0.1, "blend": "additive" }, "at": [0, 0, 0] }
```

- It has no `scale` (its radius and height size it), no `colours` and no `tint` (its colour is on the shape).
- Only what differs from the defaults is written; under `above` the repeats aren't written either.
- A shape's soft texture is generated from numbers (original, nothing of the game's): it's named `n5s_` and 12 hex
  digits of its colour and blend (`n5s_8c757fb28f99` for the default colour, `n5s_617b220c1144` for it Additive), so
  shapes of one colour and blend share it; an Additive one has its `n5s_<...>.txi` beside it wherever it's written
  (`data\files`, `--saved`, the bench's Play), git-ignored and owned by its name as the texture is.
- `none` is its colour only. A library texture (the game's own, by name; never `null` or an `n5s_` name) is tinted by
  `colour`, lit by itself as far as `glow` goes: `selfillumcolor` `colour` x `glow`, `ambient` and `diffuse` `colour` x
  (1 - `glow`). At Glow 1 (the default) it's the texture times the colour by day and night; lower, the scene's light
  comes in. Nothing is generated or copied for it.

**A shape's texture.** `build.js` makes the shape's mesh from its numbers (no game model, no mesh reader), and writes
its soft texture beside what names it:

- a placeable's beside its model, in `data\files` (or `--files`). It's build output, as a bake is: `.gitignore` keeps
  it out (`/data/files/n5s_*`), `made.txt` doesn't list it, and `build.bat` makes it for every build. Its bytes follow
  from the recipe's colour alone, so the project is all that's committed; while you try colours, no stale texture
  piles up in the repo;
- a spell mesh's beside its draft, in `--saved`.

Its name says whose it is: only the generator makes `n5s_` names, and the name fixes the picture, so a file of that
name in the way is rewritten. `none` and a library texture write nothing: a library texture is only named, in the
model or the draft (the game has it). A library texture the game has only as a PLT (a dyed texture: a helmet's) is
refused - a shape takes no dyes; pick a TGA or DDS.

**See-through in game** (the owner, 2026-11-18): every mesh carries a `transparencyhint`, the game's word for a
see-through mesh (without one the game cuts a mesh at alpha 0.2 and draws the rest solid). A shape's is 1 when it's
see-through (a soft or library texture, or an alpha below 1), else 0; a game part's or your own file's mesh keeps its
model's own (0 to 5: the game's lanterns have 2 on their inner panes, 1 on their outer). A placeable's model and a spell
mesh's draft (`transparencyhint = N`, which the game's model writer, `WriteVfxModel`, writes onto the plane) both carry
it, by one rule (floatdisk.js `meshHint`).

**Limits** (the in-game test decides; no game-side change): a spell mesh's draft doesn't carry the dome's smoothing
groups, since the game's model writer (`WriteVfxModel`) writes none for a plane.

The Floating Disk's README ("Shapes") has every field and its bounds.

## Moving textures

A part's `motion` (M2) scrolls, turns and breathes the pictures on its textured meshes - a shape's too:

```json
{ "shape": { "type": "dome", "radius": 4.57, "alpha": 0.1, "blend": "additive" }, "at": [0, 0, 0], "motion": { "scroll": [0, 0.25] } }
```

- `scroll` `[u, v]` (repeats a second), `turn` (degrees a second), `breathe` (0-0.9) and `rate` (breaths a second). Only
  what moves is written; a part without it builds as before, byte for byte.
- NWN:EE has no setting for it, so each textured mesh names a material instead, `n5u_` and 12 hex digits (its texture,
  the motion and its pivot): `n5u_<...>.mtr` pairs our vertex shader, `vsn5uvmove.shd`, with the game's lit fragment
  shader and names the real texture (`texture0`).
- **What it brings:** the materials and the shader, beside what names them - a placeable's models in `data\files` (the
  materials git-ignored, `/data/files/n5u_*`, made for every build by `build.bat`; the shader committed there and listed
  in `tools\data\floatdisk\made.txt`), a spell mesh's draft in `--saved`. A material is ours by its name, the shader by
  its first line. A texture of your own file's part is still copied beside the draft, and stays ours there (its draft
  names the material, the material the texture).
- The VFX Bench's Composer shows and plays a moving texture from M3 on; until then only `build.js` makes the material.
- The maths, the bounds and the files: the Floating Disk's README, "Moving textures".

## The probe (TMO-1)

`uvprobe.js` writes the in-game test that decides how a moving texture reaches the game, before the bench's side is
built: `n5m_uvprobe`, a spell mesh of six faint domes 1 m apart along x, three by the `texture0` route (0.4 m high) and
three by the copy route (0.7 m high: no `texture0`, the texture copied under the material's name with its TXI). Each
side, from its end in: a Soft Additive dome scrolling (up / down), `fxpa_globe` From above turning (counter-clockwise /
clockwise), a Soft Additive dome From above breathing (a breath every 2 s / every second).

```
node tools\data\composer\uvprobe.js --saved <nwn5e-ee>\build\vfxedit\saved [--game <folder>] [--tools <nwn5e-tools>]
```

- It refuses a folder that isn't a bench build's `vfxedit\saved`, and writes only there: the draft (the Composer's
  mark), the Additive soft texture and its TXI, the six materials, the shader and the copy route's textures (the soft
  one made again; `fxpa_globe` from the game, its TXI with it - never into the repo). It adds `n5m_uvprobe` to the
  bench's `vfxedit\meshes.txt`, so **+ mesh** offers it once the bench is restarted.
- It overwrites only its own: the draft by its mark, the shader by its first line, the rest by their `n5s_` / `n5u_`
  names. It prints which dome is which.
- TMO-1 (TOOLS-PLAN): as a mesh beat in a new effect, Play in game; which domes move says the route.

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
- A dyed part's baked texture goes into `data\files` too, beside the model. It's made from the game's PLT, so
  `.gitignore` keeps it out (`/data/files/n5p_*`) and `made.txt` doesn't list it; `build.bat` makes it with the
  models for every build.
- A shape's soft texture (`n5s_*.tga`) goes into `data\files` beside the model too. Like a bake it's build output:
  `.gitignore` keeps it out (`/data/files/n5s_*`), `made.txt` doesn't list it, and `build.bat` makes it with the
  models for every build.
- A moving texture's material (`n5u_*.mtr`) goes there too, build output as well (`/data/files/n5u_*`); the shader it
  names, `vsn5uvmove.shd`, is committed there already.

**A spell mesh:** each variant becomes a draft of its meshes, `<saved>\<model>.vfx`, in the VFX Bench's
`vfxedit\saved\`. The game writes its model there when it runs. An emitter's chunk can name it, and an effect can
carry it as a mesh beat. Textures of your own files' parts are copied in beside the draft, and so are a dyed part's
baked textures, a shape's soft texture and a moving texture's materials and shader: `--saved` takes them all.

## Rows: 14100-14199

A placeable's rows come only from 14100-14199. The VFX Bench gives them at the first save, and the project keeps them.
A row must be free everywhere except this project's own rows file. `build.js` refuses one that's set by:

- another `data\rows\placeables*.txt`;
- a `data\looks\placeables*` file;
- the game's own `placeables.2da` (its blank user rows don't count).

A row taken twice makes the launch check skip a whole file. When the VFX Bench gives rows, it also skips those the
bench's build folder serves (`<build>\served\placeables.2da`).

## In the VFX Bench

The bench's Composer is where a project is made and edited (nwn5e-tools `vfx_bench\README.md`, "The Composer"):

- **The checkout:** it reads and saves the projects of the checkout it's started with (`--ee`; by default
  `..\nwn5e-ee-anim` beside nwn5e-tools), and its Composer card names that folder and its branch. The projects and
  what they build are committed on anim, but for what git ignores (the models, made of the game's parts, and the
  generated textures).
- **New project** (a name, then Placeable or Spell mesh) writes `<name>.json` here with one empty variant and a guide:
  a placeable's a 1 m circle, a spell mesh's a 0.5 m one, both at height 0. A placeable's `copy` and `use` start
  `null`, its `walk` `"through"`.
- **Opening** a project reads its file as it is on disk; the Composer card's **Reload** reads it again.
- **Your own parts** go in `parts\` here (make the folder). The bench lists them under Yours, with the Floating Disk's
  `parts\`, and writes a part's `file` from this folder.
- **The stage** loads this folder's `project.js`, and the Floating Disk's `floatdisk.js`, as the bench serves them.
  The guide's outline, the use point, the model names and the placement are the generator's own. A change to either,
  or to `reader.js`, refuses the Composer until the bench restarts (close its window, double-click
  `vfx_bench.bat`).
- **Save and build:**
  1. The page posts the project with the hash of the text it opened. The bench refuses it when it doesn't read as
     `readProject` reads it, or when the file changed on disk since (unless you say to save over it). One save runs
     at a time.
  2. A placeable's variants without a row get the lowest free ones (Rows, above), and each variant's `model` is set
     from its place.
  3. That text goes to a temporary file. The bench runs `node tools\data\composer\build.js <temp> --json --tools
     <nwn5e-tools> --game <game> --reader <mdlmesh>` in the checkout (a spell mesh with `--saved <build>\vfxedit\saved`),
     for at most 120 s.
  4. Only when it exits 0 is `<name>.json` written, with the rows and models. A spell mesh's models are added to
     `<build>\vfxedit\meshes.txt`, the bench's list of them. A refused build writes nothing.

  The bench shows `--json`'s summary (each variant's parts, triangles and height; the files) and its `unused`, as "no
  longer used, and not deleted".
- **Play in game** sends a variant to the running game as `build.js` would build it (lowered to the guide's height),
  0.85 m beside the character, before any install.
- **For the game:** a placeable's files and rows reach it as the Floating Disk's do: the animation session commits
  the project, its rows and its `.pwk` files on anim, the game session merges, and its build makes the models and
  the generated textures (`n5p_*`, `n5s_*`) from the project and the owner's own game (`tools\data\build_derived.js`)
  before it installs. A spell mesh's draft stays in the bench's `vfxedit\saved\`; the game writes its model there
  when it runs.

## Build

The VFX Bench's **Save and build** runs it. By hand, from the repo's root:

```
node tools\data\composer\build.js tools\data\composer\crates.json
node tools\data\composer\build.js tools\data\composer\web.json --saved <nwn5e build>\vfxedit\saved
```

- `--saved` is where a spell mesh's drafts go, with their textures, a dyed part's bakes and a shape's soft texture; a
  spell mesh needs it. A placeable doesn't: its bakes and soft textures go beside its models in `data\files` (or
  `--files`).
- `--check` prints what it would build and writes nothing. A variant without a row yet is fine there.
- `--json` prints the summary as one line of JSON instead of text (an error as `{ "error": ... }`). Its variants carry
  what the bench's stage shows: parts, meshes, triangles, how tall, the top, and each part's size and reach past the
  guide. Its `baked` lists a dyed part's textures (`texture`, `plt`, `colours`), as the text's `texture n5p_...` lines,
  its `shapes` a shape's soft textures (`texture`, `colour`), as the text's `texture n5s_...` lines, and its
  `materials` a moving texture's materials (`material`, `texture`, `motion`, `pivot`), as the text's `material n5u_...`
  lines.
- `--game`, `--tools`, `--reader`, `--files` and `--rows` work as the Floating Disk's `build.js` options.
- It never overwrites a file it didn't make: a model, `.pwk`, rows file or draft must carry its mark line. A texture
  must be listed in `made.txt` or, beside a draft, named by this project's draft (or by a material the draft names); a
  baked one, a shape's soft texture or a moving texture's material is ours by its name, the shader by its first line.
- Every file is checked before any is written: a refused build writes nothing.
- When a model's name changes (a second variant added: `plc_n5crates` becomes `plc_n5crates_1`), the old files are
  listed as no longer used. They aren't deleted.
- It refuses a model name the game already has.

`node tools\data\composer\check.js` checks the pure parts (`project.js`). It doesn't need the game.

`node tools\data\composer\check_build.js` builds on a temporary copy and checks the whole generator, a dyed part's
bakes and a dome's mesh and texture too (a spell mesh's and a placeable's; soft, none, a library texture, and a
PLT-only one refused), a moving texture's materials and shader (a spell mesh's dome, a placeable's crate, none without
a motion), and the probe (into a temporary `vfxedit\saved` only). It needs the game, and prints `skip` without it.
