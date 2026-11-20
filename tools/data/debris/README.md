# Destruction effects: build.js

#172 made Plot dressing breakable; the owner then broke a small bench, a lectern ("a fountain of debris") and a brazier
("it vanished"). The destruction recon (2026-11-20) read why: a placeable's debris is **its own model's "die"
animation** - Explosion emitters that throw the game's chunk models on a `detonate` event - and of the game's 1,304
placeable models 525 throw chunks, 181 only a smoke puff, 71 have a "die" with nothing and 491 no "die" at all; a lit
light's light is cut the moment it dies (the client's `UnloadLight`), and the brazier lost its "die" to nwn5e's own lamp
script (the game session's fix). The owner, 2026-11-20: **"yes on all"** - consistent destruction:

- **debris matched to the material** for a breakable whose model has none of its own: wood, stone, metal (with sparks),
  cloth (dust and scraps) and glass, each small and large;
- **a lit light's death**, on top of its own debris: ember sparks, a short rising smoke column and a warm light fading
  out over about 1.5 s.

| effect | row | model | the game's own it starts from |
|---|---|---|---|
| wood, small / large | 2248 / 2249 | `n5_db_wood_s` / `_l` | the bench PLC_X01: a puff, `plc_chunk_w02` x 1, `plc_chunk_w01` x 2 |
| stone, small / large | 2250 / 2251 | `n5_db_stone_s` / `_l` | the sarcophagus PLC_B10 (`plc_chunk_s01`, `s02`), VFX_COM_CHUNK_STONE's rocks (`fx_rockchunksml`, `med`) |
| metal, small / large | 2252 / 2253 | `n5_db_metal_s` / `_l` | the brazier PLC_I05 (`plc_chunk_m01` x 3), VFX_COM_SPARKS_PARRY's sparks |
| cloth, small / large | 2254 / 2255 | `n5_db_cloth_s` / `_l` | the bolt of cloth PLC_I03 (a puff), the sacks PLC_L03 (a puff, `plc_chunk_m01` x 3) |
| glass, small / large | 2256 / 2257 | `n5_db_glass_s` / `_l` | the bottles dag_btl* (`dag_chunk_g01` x 12), the crystal ball px2_d01 (`px2_gls` x 4, a glint, a blue-grey puff) |
| a lit light's death | 2258 | `n5_db_light` | vim_explflame's sparks, vim_exp2flame's sparks01 and light, the brazier's smoke fire!06 |

The rows are `data\rows\visualeffects.debris.txt` - type F (one-shots: no cessation), the model in the four root
columns, OrientWithGround 0. The game's call: `DestructionDebrisVfx(DebrisMaterial, DebrisSize)` and `LightDeathVfx()`
(`src\core\anim_destruction.h`; the rows `data::DebrisRow` / `kLightDeathRow`, a test reads this file against them).

## What each is

Every emitter starts from one of the game's own, read from the player's install (the source named in `build.js`; the
values only - nothing of the game's is kept here). The chunks are **the game's chunk models, thrown by name**.

- **The chunks** - the game's placeable "die" recipe ("ChunkyWood", the same on its 525 models): an Explosion on the
  impact's `detonate`, life 0.75 s, velocity 8, spread 1.222, mass 3, bouncing at 0.5, from an 89 x 40 cm patch. Glass
  keeps its own pieces' spread (2.967) and patches (the bottles' 5 x 5 cm, the crystal ball's 22 x 24 cm).
- **The rocks** (stone) - VFX_COM_CHUNK_STONE's "medrock": they lie 4 s, thrown wide (spread 4.189), bouncing at 0.2.
- **The dust** - the game's die smoke ("fire!"): 10 puffs, 4 m shrinking to 1 m, fading out over 2.5 s, in the colours
  of one of the game's own pieces of that material: wood brown from the bench, stone from the sarcophagus, cloth from the
  sacks, glass blue-grey from the crystal ball (5 puffs there). Metal's is the brazier's, which is **the game's common
  grey puff, not a metal tint** - the same colours on the bolt of cloth (fire!49), the campfire, the candelabra and the
  mirror (fix round 1, the review). Metal is told apart by its sparks.
- **Metal's sparks** - VFX_COM_SPARKS_PARRY's: specks streaking (motion blur, added light), falling and bouncing for 3 s,
  white-yellow to orange.
- **Glass's glint** - the crystal ball's: white streaks turning ice blue, gone in 1 s.
- **Heights.** The debris plays at the breakable's own location - its base, on the floor - so a small effect's emitters
  sit where the game's small pieces have theirs, 0.25-0.7 m up; a large one adds the same kind of debris again 0.6 m
  higher (to 1.3 m: the bookshelf's, the armoire's and the sarcophagus's upper emitters), with the larger chunk where
  the game has one (`plc_chunk_s02`, `fx_rockchunkmed`, `px2_gls`), so it throws more, from higher.
- **The light's death** - at the light's own point (the game session places it there):
  - a burst of ember sparks: vim_explflame's (VFX_IMP_FLAME_S) orange streaks, slower and longer than a flame's hit
    (velocity 5, 0.25 s, 20 of them);
  - rising embers: vim_exp2flame's (VFX_IMP_FLAME_M) sparks01 - fxpa_flare, added light - drifting up (mass -0.1, as
    vim_explflame's smoke rises) at 1.2 m/s, shrinking 0.1 -> 0.04 m, yellow cooling to the game's deep fire orange
    (vim_strkfire's light, 0.92 0.44 0.11), 12 a second for 1.2 s;
  - the smoke column: the brazier's own (PLC_I05 fire!06: 20 a second rising 1 m/s in a narrow column, growing 1 -> 2 m,
    grey to dark, life 3 s), thicker as the fire chokes (alpha 0.3 where the brazier's is 0.1), on for 2 s - it climbs
    past 1 m;
  - the glow: VFX_IMP_FLAME_M's light colour and reach (0.945 0.741 0.42, 5 m), in place of the light the client cuts -
    on at once (`fadingLight 0`: the engine's fade-in would leave a dark gap), cooling to an ember red as it fades: 0.85
    0.55 0.25 at 0.5 s, 0.5 0.25 0.08 at 1 s, black at 1.5 s. **It differs from the game's light on purpose:** the game's
    is ambient-only and not dynamic; ours lights its surroundings and the creatures near it (`ambientonly 0`, `isdynamic
    1`), as the placeable's own light did until it was cut.
- **One-shots.** Each model is an impact alone (no duration, no cessation): it ends with its last particle (3.5 s for
  wood, 5 s for stone's rocks and the light's smoke), nothing left behind.

## How the game plays them

At a location (`ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(row), <location>)`), so the effect
outlives the placeable - the engine removes it 2 s after its death:

- the debris at **the placeable's own location** (its position and facing); the models lift their emitters themselves;
- the light's death at **its light's point** - the position, plus placeables.2da's LightOffsetX / Y turned by its
  facing, and LightOffsetZ up (brazier 0.875 m, candelabra 1.90 m, lamp post 3.87 m). The engine keeps a location
  effect's height (read from nwmain-linux: `CNWSArea::ApplyEffect` hands the position to `PlayVisualEffect`, which
  sends x, y and z as given; the client's `CNWCVisualEffect::SetPosition` puts the model there - its ground ray only
  turns an effect whose row has OrientWithGround, and these have 0).

## Run

```
node tools\data\debris\build.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>]
node tools\data\debris\check.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--game <folder>]
```

`--tools`: nwn5e-tools (default `NWN5E_TOOLS`, else beside this repo) - the VFX Bench's `core.js` writes the drafts.
`--reader`: `NWN5E_MDLMESH`, else `build\floatdisk\mdlmesh.exe` (built by `..\floatdisk\build_mdlmesh.bat` when missing
or older than its sources).

`build.js` writes 22 files, and only those whose bytes changed (compared without CRs: git checks them out CRLF):

| files | what |
|---|---|
| `tools\data\vfx\n5_db_*.vfx` (11) | the drafts, by the VFX Bench's own writer (`core.js` `writeDraft`: byte for byte the game's `WriteVfxDraft`) |
| `data\files\n5_db_*.mdl` (11) | the models, by the game's own writer (`WriteVfxModel`, through `mdlmesh --model`) - the Detect Magic way |

`check.js` builds twice in memory and checks without writing: the effects, rows and names; every emitter against the
game's recipe it starts from (stated in the check); each material's chunks; the heights and counts small and large; the
light's death's emitters and light keys; the rows file; the drafts and models in step (the drafts read back by the bench
byte for byte, the models the game's writer's); on the bench's stage (its own `sim.js`) the bursts, nothing outliving
an effect, the embers rising, the smoke climbing, the light warm then black; and, with the game, every chunk model read
from the install as the bench's library export reads a chunk and drawn on the stage as its real meshes (`view.js`
`chunkModel` / `chunkMeshes`), never the stand-in cube.

`tests\data\anim_debris_test.cpp` (test.bat) checks the call and its bounds, the names, the rows file against the
constants, the drafts writing the models, and the models as the game and the bench read them.

## On the VFX Bench

They have emitters, so the bench lists them: the **nwn5e** tab (the dev layer copies served `n5_*.mdl` there at each
start) and the **Game** tab (rows 2248-2258, after `!vfxexport` - the dev layer's export reads the served rows). The
stage draws a chunk as its model once the library has it: the export copies every chunk model an effect's emitter throws,
so after `!vfxexport` it holds `plc_chunk_w01` / `w02`, `dag_chunk_g01` and `px2_gls` too (the game's own effects
throw none of them); before that, those show as small cubes and the note on the stage says which.
