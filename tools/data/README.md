# The game-derived files: build_derived.js

`build_derived.js` builds the models made from the game's own art - the Floating Disk's and the Composer's placeables -
and a dyed part's baked texture, from their recipes, from your own game, into a build's folder. The repo doesn't keep
them. It carries a shape's soft texture (`n5s_*`) with the models too, as it carries a bake, and a moving texture's
material (`n5u_*.mtr`) with the shader it names. And it builds the game's 16 placeable models that break with a
fountain of debris, without it ("One burst", below).

## Why

nwn5e-ee's CLAUDE.md: "Game files (and art made from them), logs and saves are never committed or attached." The
Floating Disk's models committed on 2026-11-16 copy the game's placeable meshes. The owner, 2026-11-17: **"go with 1,
remove the six files from main"**:

- The repo keeps only the recipes (`floatdisk\composition.json`, the Composer's `composer\<project>.json`), the
  generators and original art.
- The game-derived models, and the baked PLT textures they name (a dyed part's: a helmet's, painted from the game's
  PLT and palettes), are built on the owner's machine at build time, from the owner's own game, into the build's
  `data\files`.

## What git keeps out

`.gitignore` keeps these out of the repo:

- the Floating Disk's six models made of the game's meshes: `c_n5fd_small`, `_med`, `_large`, and `plc_n5fd_small`,
  `_med`, `_large`;
- `c_n5fd_empty.mdl`: its disk mesh is original, but it carries `c_invsguy`'s node names and positions, which its
  supermodel needs (the owner, 2026-11-17: "go with your recs for the three questions");
- any Composer placeable's model: `plc_n5*.mdl`;
- any baked texture: `n5p_*` (`n5p_` and 12 hex digits, `.tga`: the Floating Disk's README, "Dyed parts");
- any shape's soft texture: `n5s_*` (the Floating Disk's README, "Shapes"). It's original, but build output like a
  bake: its bytes follow from the recipe's colour alone, so only the recipe is committed, and no stale ones pile up
  while colours are tried;
- any moving texture's material: `n5u_*` (the Floating Disk's README, "Moving textures"), build output as a soft
  texture is - and a clamped one's copy (W1: `n5u_*.tga` / `.dds` and its `.txi`, the texture's picture under the
  material's name - the game's own art for a library texture, so never committed), or an unclamped one's own TXI (MV:
  `n5u_*.txi`, its texture's blending and map lines);
- any library texture's copy: `n5l_*` (DC: a still shape's decal or flipbook texture - MV: a moving one's too -
  `.tga` / `.dds` and its `.txi` -
  the game's picture under our own name with its TXI minus `decal`, + `clamp 2`: the Floating Disk's README, "A library
  texture's copy"), the game's own art, so never committed;
- the 16 one-burst models, each by its name (`plc_k02.mdl` and the rest: "One burst", below) - the game's own models,
  changed, so never committed.

The generators' own runs (the VFX Bench's Save and build) still write them into `data\files`, where they stay,
ignored. What's made from scratch stays committed: the `.pwk` files, `n5t_forcedisk.tga`, the rows, and the moving
textures' shader, `vsn5uvmove.shd` (the Floating Disk's `made.txt` lists it).

## In build.bat

`build.bat` runs it once, right after copying `data\` into its output, with `--out "%OUT%\data\files"`. So
`NWN5E_OUT` scratch builds get the models too, and so does `build.bat public`.

- **It stops the build** when anything is missing (Node on PATH, the game, nwn5e-tools, the mesh reader) or anything
  else fails (exit 1 or 2), naming what and how to fix it. An install must never lack the Floating Disk's models.
- **`NWN5E_DERIVED=skip`** builds without them, with a warning. It's for a build not meant to be installed.

## Run

From the repo's root:

```
node tools\data\build_derived.js --out <folder> [--game <folder>] [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--check]
```

- **`--out`:** where the models, walkmeshes and textures go - the build's `data\files` (`%OUT%\data\files` in
  build.bat). Never this repo's `data\`: that's refused.
- **What it builds:** the Floating Disk (`floatdisk\build.js`), and every placeable project in `composer\*.json`
  (`composer\build.js`). Each generator runs into a scratch folder of its own, then what it made is written into
  `--out`: the models, the walkmeshes, the textures, and a dyed part's baked texture (`n5p_*.tga`) with the models
  that name it. Two generators may bake the same texture (the same PLT and colours): it's written once.
- **A shape's soft texture** (`n5s_*.tga`, a dome's) comes out with the model that names it, as a bake does: the
  same name, the same bytes, written once, git-ignored and rebuilt for every build. A library texture is only named:
  the game has it.
- **A moving texture's material** (`n5u_*.mtr`) comes out with the model that names it, and the shader it names
  (`vsn5uvmove.shd`) with it - the committed one, byte for byte (build.bat's copy of `data\` has it already). A
  clamped one (W1: it breathes, doesn't scroll, its mesh within one repeat) brings its copy too - the texture's picture
  under the material's name (`n5u_*.tga` or `.dds`) and its TXI (`n5u_*.txi`: the texture's own lines and the game's
  `clamp 3`) - owned by the `n5u_` name as the material is. An unclamped one with a TXI of its own (MV: `n5u_*.txi`,
  its texture's blending and map lines - the engine takes a mesh's blend only from its bitmap's TXI) brings it likewise.
- **A library texture's copy** (DC: `n5l_*.tga` or `.dds` and `n5l_*.txi` - a still shape's decal or flipbook texture
  - MV: a moving shape's too, its material's texture0 -
  the game's picture with its TXI minus `decal`, + `clamp 2` within one repeat) comes out with the model that names it,
  owned by its `n5l_` name; a still Soft shape's clamped soft texture (its own `n5s_` name) with its TXI (`clamp 2`).
- **The one-burst models** (`oneburst.js`, "One burst" below): the game's 16 placeable models whose looping "dead"
  re-fires a detonate, each without it, under its own name - owned in `--out` by that name. It says how many, and which
  it skipped.
- **Spell meshes are skipped**, and it says so. Their drafts live in the VFX Bench's saved folder, and the dev layer
  writes their models when the game runs.
- **A project never built is skipped**, and it says so. That's one with no `data\rows\placeables.<name>.txt` and no
  variant with a row, as the VFX Bench's New project writes it until its first Save and build. Nothing installed uses
  its models. A project with either is built, and a refusal stops the build.
- **The rows stay the repo's.** `data\rows\appearance.floatdisk.txt`, `placeables.floatdisk.txt` and each project's
  `placeables.<name>.txt` are committed text that names the models. It never writes them. It checks that the rows each
  recipe gives equal the committed ones. If they don't (or one is missing), it stops and writes nothing. Either a
  recipe changed without its rows, or a game update changed the game's `appearance.2da` or `placeables.2da` (the rows
  copy the game's rows).
  - The message gives the command that rebuilds them (`--files <a scratch folder>` keeps that generator's models out
    of `data\files`); the VFX Bench's Save rebuilds them too. Commit them with the recipe.
  - For a build that won't be installed, `NWN5E_DERIVED=skip` gets past it (build.bat).
- **Never a file it didn't make.** In `--out` it keeps the generators' rule: a model or `.pwk` is overwritten only
  when it carries its generator's mark, a texture only when that generator's `made.txt` lists it, a baked texture, a
  shape's soft texture, a moving texture's material or a library texture's copy by its name (only the generators make
  `n5p_`, `n5s_`, `n5u_` and `n5l_` names, and the name fixes the bytes), the shader by its first line, a one-burst
  model by its name (one of `oneburst.js`'s 16: only it makes them there, and the game's model fixes the bytes).
  Any other file in the way stops it. Every file is checked before any is written.
- **What it says:** a line per generator (its files, whether its rows agree), one for the one-burst models (how many,
  how many events taken out, compiled and ASCII) and one per model it skipped, a line per file (`written` or
  `unchanged`), then a total.
- **`--check`** builds and checks everything in the scratch folders, writes nothing into `--out`, and says what it
  would write (`would write` or `unchanged`).
- **Defaults**, as the generators':
  - `--game`: `NWN_GAME_DIR`, `nwn5e.ini`'s `[loader] game_dir`, else the Steam folder.
  - `--tools`: `NWN5E_TOOLS`, else `..\nwn5e-tools`.
  - `--reader`: `NWN5E_MDLMESH`, else `build\floatdisk\mdlmesh.exe`. It's built with `floatdisk\build_mdlmesh.bat`
    (VS 2019 Build Tools, as build.bat) when it's missing or older than its sources.

| exit | means |
|---|---|
| 0 | done (with `--check`: nothing stops it) |
| 2 | a prerequisite is missing: nwn5e-tools, the game or the mesh reader. One line says which and how to fix it. |
| 1 | anything else: a refused build, rows that disagree, a file in `--out` it didn't make, no `--out` |

build.bat stops on either (above).

## One burst

The owner, 2026-11-20, on the Academy's lectern (PLC_K02), which "fountained a huge amount of debris" when broken: "For
the lectern A" - the game's models that fountain, served without it, built from the owner's game and never committed.

- **Why they fountain.** A broken placeable's debris is its model's own "die" animation: its `detonate` events set off
  its Explosion emitters (the chunk models, a smoke puff). The client plays "die" once, then loops "dead" until the
  server deletes the object 2 s later. 16 of the game's placeable models have a `detonate` in that "dead" too, so every
  loop (the lectern's is 0.033 s long) throws the debris again for those 2 s.
- **The 16** (`oneburst.js`'s `MODELS`): `plc_c03`, `plc_c04`, `plc_c10`, `plc_h10`, `plc_i02`, `plc_k02`, `plc_q03`,
  `plc_x0_fgs`, `plc_x0_grw`, `plc_x0_tds`, `plc_x0_wds`, `pwc_mage_001`, `px2_b04`, `px2_c10`, `px2_e01`, `px2_g02` -
  every model `placeables.2da` names whose "dead" has a `detonate` and which has an Explosion emitter for it to set off.
  One more has a `detonate` in "dead" and no emitter (`plc_x0_grs`): nothing to fountain, left as the game's. The check
  recomputes the list from the game and fails when it drifts (a game update).
- **The copy:** the game's model (its keys, `ovr\` over them, as the generators read the game), with every `detonate`
  event of its "dead" animation taken out by the repo's own model code (`src\data\vfx_model.cpp`'s
  `DropAnimationEvents`, run through `mdlmesh --drop-events`) - nothing else. Written in the form the game has it:
  - **compiled** for 14: changed in place, as the held torch's flame is (`PatchEmitter`: the game's compiled model with
    its emitter changed, written under its own name). The "dead" header's events array becomes the one the game's own
    models keep for an animation with none (offset, count and allocated count 0 - the bench's, `PLC_X01`), its freed
    entries zeroed; every other byte is the game's. In all 16 every "dead" event is a `detonate`.
  - **ASCII** for 2 (`plc_c10`, `pwc_mage_001`: the game has them as text): the "dead" block's `event ... detonate`
    lines taken out, every other line and line end as the game's.
  - The "die" is untouched - its `detonate`s, its emitters and their keys - so the piece breaks with that burst once.
- **Served** as the game's model's own name in the build's `data\files`; nwn5e copies `data\files` into its served
  folder, which the engine reads over the game's keys (src\core\data_serving.cpp) - where the torch's flame goes too.
- **No burst left on three:** `plc_h10` (Cot), `plc_i02` (Birdbath) and `plc_q03` (Flag3) have no event in their
  "die" - their debris lives only in "dead" (the Cot's and the Flag's "dead" also sinks the mesh, their "die" holds it
  sunk: the two look swapped) - so without it they break with no debris: the mesh still sinks under the floor and the
  Destroyed sound plays. Moving their "dead" detonates into "die" would give them one burst; the owner's call.
- **Skipped, and said:** a listed model the game hasn't, or whose "dead" has no `detonate` now (a game update fixed it -
  the game's own is right then). The build goes on; the check fails until the list is updated.

## Check

`node tools\data\check_derived.js` runs it end to end on a temporary copy, never this worktree's `data\` or
`tools\data\`. It needs the game (it prints `skip` without it).

- The copy's recipes and `made.txt` come from `HEAD` (`git cat-file --filters`), never the worktree, so uncommitted
  Composer edits are never the baseline. The generators' code comes from the worktree.
- It checks that:
  - the Floating Disk's seven game-derived models (the six and `c_n5fd_empty.mdl`) aren't at `HEAD` and are
    git-ignored, as are a placeable's model, a baked texture and a shape's soft texture, while the original art isn't;
  - the committed recipes build `HEAD`'s `data\files` byte for byte (the Floating Disk's 4 committed files and every
    committed placeable project's), into `--out` only. What's built but not committed is git-ignored. Each removed model
    comes out byte for byte as it was last committed, while the recipe and the generator are unchanged since its
    removal. The Floating Disk's dyed parts' bakes and its shapes' soft textures come out with its models, which name
    them;
  - a second build leaves every file unchanged;
  - a placeable project's dyed parts: their bakes come out with its model into `--out`, the bake's bytes, unchanged on
    a second build; one the Floating Disk also makes is written once; a file of a bake's name in `--out` is rewritten;
  - a placeable project's dome: its model and its soft texture (git-ignored) come out into `--out`, the generator's
    bytes, unchanged on a second build; a file of the texture's name in `--out` is rewritten;
  - a placeable project's moving dome: its material and the shader come out beside its model, the shader the
    committed one; a material of its name or a shader with its first line in `--out` is rewritten, someone's
    `vsn5uvmove.shd` stops it;
  - `--check` writes nothing;
  - rows that disagree, or a recipe edited without its rows, give exit 1 and say which;
  - a spell mesh is skipped, a refused build and a file in the way give exit 1, and each missing prerequisite gives
    exit 2;
  - one burst: the 16 aren't at `HEAD` and are git-ignored (another game model isn't); they're the game's fountains,
    recomputed from its `placeables.2da` and models (it fails, naming them, when the game's differ); each copy is the
    game's model with its "dead"'s `detonate`s taken out - its "die" (events, emitters) and every other animation as the
    game's, a compiled one's every byte but the "dead"'s events and their three fields, an ASCII one's every line but
    the "dead" block's `detonate` lines - 14 compiled and 2 ASCII; without them (the copy's `oneburst.js` listing none)
    the build writes every other file byte for byte the same; a listed model the game hasn't, or whose "dead" has no
    `detonate`, is skipped and said; a file of a one-burst name in `--out` is rewritten;
  - the real worktree is untouched.
