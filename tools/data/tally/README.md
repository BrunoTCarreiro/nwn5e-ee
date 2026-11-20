# The target-count marker's final look: build.js

#171's target window puts a marker over each picked target: how many darts or rays are assigned to it. It was four discs
with a number round the head (`tools\data\tcount`), then a probe of glowing tally marks on one quad our own vertex shader
turns to the camera (`tools\data\tprobe`, rows 2211-2214). The owner, 2026-11-20, after the probe: the face-on (spherical)
style - "camera level for max readability for every angle" - and "for now let's keep your generated style, but can we
do it black on blue/orange/violet glow for magic missile, scorching ray and eldritch blast respectively?". This is that:
**black tally bars on the spell's glow, facing the camera from every angle**, for counts 1-11 in three looks.

| spell | glow | rows | models |
|---|---|---|---|
| Magic Missile | blue `#3a7bff` | 2215-2225 | `n5_tf_mm01` .. `n5_tf_mm11` |
| Scorching Ray | orange `#ff8a1e` | 2226-2236 | `n5_tf_sr01` .. `n5_tf_sr11` |
| Eldritch Blast | violet `#a040ff` | 2237-2247 | `n5_tf_eb01` .. `n5_tf_eb11` |

Each colour clear and saturated, one channel full (the most an additive colour can add). The rows are
`data\rows\visualeffects.tally.txt` - type D, the model on the head (`Imp_HeadCon_Node`), OrientWithObject 0, as the
marker's and the probe's. The game's call: `TargetCountMarkerVfx(TargetCountStyle style, int count)`
(`src\core\anim_target_counts.h`; the rows `data::TargetTallyRow`, a test reads this file against it). In game:
`!vfxnode 2221 600` puts Magic Missile's 7 on you, `!vfxnode off` takes it off.

`build.js` writes 210 files into `data\files`:

| files | what |
|---|---|
| `n5_tf_<ss>NN.mdl` (33) | the models: two quads each, the bars and the glow |
| `n5_tfb_NN.mtr` / `.txi`, `n5_tfbp_NN.tga` / `.txi` (11 each) | the bars' material and its TXI (the blend), their picture and its TXI - one per count, the same for the three spells |
| `n5_tfg_<ss>NN.mtr` / `.txi`, `n5_tfgp_<ss>NN.tga` / `.txi` (33 each) | each glow's material and TXI, picture and its TXI |
| `vsn5bill_sl.shd` | the vertex shader: the probe's spherical billboard with the layer's nudge |

(`ss`: `mm`, `sr`, `eb`; `NN` the count, 01-11. Every name under 17 letters.)

**Every TXI says `downsamplemax 0` and `downsamplemin 0`** - the bitmaps' (beside their blend line) and the pictures'
(those two lines alone): the game's texture-quality setting shrinks a texture unless its TXI says so (the owner,
2026-11-20: "add the downsample lines to my textures too for now"). check.js checks every TXI build.js makes has them,
and every picture a TXI.

## Run

```
node tools\data\tally\build.js [--out <folder>]
node tools\data\tally\check.js
```

`build.js` writes only those files, and only those whose bytes changed (a text file that differs only in its line ends is
unchanged: git's autocrlf checks them out CRLF). `check.js` builds twice in memory, checks everything below, then that
`data\files` and the rows file are in step. Exit 0 or 1; it writes nothing. The outputs are committed for the reasons in
`tools\data\tcount\README.md` (nwn5e's own art from numbers in this file, no game file, fixed names). 44 pictures of
256 x 128 x 4 bytes: 5.8 MB.

## Why two quads

An additive glow can't draw black: black adds nothing. So each model has two quads, both 0.5 x 0.25 m, made round their
node's origin in its x-z plane, the node 0.55 m over the head node (as the probe's and the four discs'):

- **The bars** (`<model>_bars`, bitmap `n5_tfb_NN`): black bars with the strokes' alpha, clear elsewhere - drawn
  alpha-blended (its TXI `blending default`: the engine reads only `additive` and `punchthrough`, anything else keeps
  a mesh's alpha blend). Its material says `transparency 1` too: never opaque (`Material::GetIsOpaque`), so whatever the
  picture's alpha is taken for, it's drawn with the blended meshes - a pixel at alpha 0.2 or under cut - never as an
  opaque box. The black is `#010101`, never 0 0 0 (the owner: "can you make them 99.9% black so they don't get seen as
  transparent?"; checked: no pixel with alpha over 0 is pure black).
- **The glow** (`<model>_glow`, bitmap `n5_tfg_<ss>NN`): the spell's colour round the strokes - additive (its TXI
  `blending additive`) - and **black wherever the bars are opaque**. Made at y -0.01: 1 cm in front of the bars.

## How the bars stay in front (the engine, read)

The engine doesn't sort see-through meshes by distance. Each mesh goes into a draw bucket by its material
(`Scene::AddPartsTreeToDrawBuckets`, nwmain-linux): an additive one (blend dst ONE) into **bucket 4**; a blended one
that isn't opaque into **bucket 3**; within a bucket, in the model's tree order. The frame (`Scene::RenderSinglePass`)
draws the dynamic geometry (`Scene::RenderDynamicGeometry`, after the area's static meshes and the dynamic shadows) -
bucket 2 (opaque), then **3** (blended: blend on, alpha discard on, `fAlphaDiscardValue` 0.2) - and **4** ("Additive":
blend on, no discard) last of the scene. So the bars are always drawn before the glow, every frame. Two things make that
give black bars on the glow:

- the glow is **black where the bars are opaque**, so drawn over them it adds nothing there (and fades in across the
  bars' antialiased rim as the bars fade out);
- the glow lies **1 cm nearer the camera** (below), so the bars' depth - a blended mesh writes depth - never cuts it.

Should the order ever turn round (the glow first: it writes no depth; then the bars over it), a bar's core is the same
`#010101` and the clear parts the same; only the 1-pixel rim differs. `check.js` composites both orders over night,
grass, snow and sky.

## The shader (`vsn5bill_sl`)

The probe's spherical billboard (`vsn5bill_s`; its inputs and maths: `tools\data\tprobe\README.md`), with one term
more. After `ApplyStandardShader()`: centre = `m_mv * (0, 0, 0, 1)` (the node's origin in view space, following the
head); right = the world's up (`m_view`'s third column) x view +z, normalised; up = view +z x right (the screen's up);
then

    vPosView = centre + scale * (vPos.x * right + vPos.z * up - vPos.y * toEye)

where `toEye = -centre / |centre|`, the eye's direction from the centre. A vertex's y in its node (the quad's front is
-y) moves it along the eye's line through the centre: the glow, made at y -0.01, lies 1 cm (times the model's scale)
nearer the camera, and - moved along that line - its picture is the bars' scaled about the same point on the screen by
|centre| / (|centre| - 1 cm): 0.5% from 2 m away, under a pixel at the quad's edge. (A move along the view's z instead
would slide it off the bars off the middle of the screen.) `gl_Position`, the normal (toward the camera) and the fog and
keyhole switched off as the probe's.

`check.js` runs the shader's lines in JS (`build.js billboard`) over 1800 cameras - all round, low to straight above,
near and far, looking at the marker and past it, the head anywhere, the model scaled: both layers in the view plane,
facing the camera, upright, centred, counter-clockwise; the glow nearer at every corner; its picture over the bars'.
The shader compiled and linked with the game's `fslit` on this machine's GL driver under the engine's header and
includes, in all 48 combinations of the quality defines (the TP rig: `tmp\animtex\tpdiag\assemble.js`, not in the tree).

## The art

The probe's layout and glow, kept: 256 x 128 (the quad's 2:1), 32-bit, bottom-up. Bars 10 px wide (2 cm), 68 px tall, 20
px apart; a five is four bars and a strike rising left to right across them; the next bar 34 px on; centred; a clear
border of 7 px all round (filtering and the repeat bring in nothing); every count 1-11 fits (checked). The glow 0.9
bright at a bar's edge, falling off as exp(-(e / 7.5 px)^2), cut smoothly to nothing by 18 px. Drawn here from strokes (the
distance to each segment); no font, no image (`build.js` loads only `fs` and `path`).

- **Bars** (`n5_tfbp_NN`): every pixel `#010101`; alpha the strokes' cover - 255 inside, 0 outside, an antialiased edge.
- **Glow** (`n5_tfgp_<ss>NN`): alpha 255; the colour times the glow, which is 0 where the bars' alpha is 255 and
  `glow(e) x (1 - cover)` on their rim.

On a bright ground (snow, sky) the additive glow washes out - the black bars don't: they read the more. On a dark one the
glow outlines the bars. The models: ambient, diffuse, specular 0, selfillum 1, alpha 1 (`fslit` gives the picture
exactly, day and night); only impact and duration, never a cessation (a removed marker would linger, TC-1) and never one
of length 0; no keys (a key waits for the clock, stopped while paused); no emitter.
