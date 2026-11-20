# The target-count marker's probe: build.js

#171's target-count marker is four discs with a number round the head (`tools\data\tcount`). The owner, 2026-11-19:
"glowing tally marks that always face the camera, is that possible?" - then "go with the probe, white-gold on red". This
is that probe: glowing tally marks on **one quad that our own vertex shader turns to face the camera every frame**. It
sits beside the marker (rows 2200-2210 and `n5_tcnt_01..11` stay as they are); if the owner picks it, all 11 follow.

`build.js` writes 16 files into `data\files`:

| files | what |
|---|---|
| `n5_tally_03.tga`, `n5_tally_07.tga` | the art: 3 (`\|\|\|`) and 7 (a struck five and `\|\|`) |
| `n5_tprobe_s3`, `_c3`, `_s7`, `_c7` `.mdl` `.mtr` `.txi` | the four probes: spherical (`s`) and cylindrical (`c`), each count |
| `vsn5bill_s.shd`, `vsn5bill_c.shd` | the two vertex shaders |

Their rows are `data\rows\visualeffects.tprobe.txt`, a file of their own: 2211 (spherical, 3), 2212 (cylindrical, 3),
2213 (spherical, 7), 2214 (cylindrical, 7) - type D, the model on the head (`Imp_HeadCon_Node`), OrientWithObject 0, as
the marker's rows. In game: `!vfxnode 2211 600` puts one on you, `!vfxnode off` takes it off.

## Run

```
node tools\data\tprobe\build.js [--out <folder>]
node tools\data\tprobe\check.js
```

`build.js` writes only those files, and only those whose bytes changed; a text file that differs only in its line ends is
unchanged (git's autocrlf checks them out CRLF). `check.js` builds twice in memory, checks everything below, then that
`data\files` and the rows file are in step. Exit 0 or 1; it writes nothing. The outputs are committed for the reasons in
`tools\data\tcount\README.md` (nwn5e's own art from numbers in this file, no game file, fixed names).

## Why a vertex shader

The target window runs paused. The engine animates no particle while paused, so a camera-facing sprite would appear only
on unpause; a mesh is drawn the moment its effect is attached (the TC diagnosis). The engine has no camera-facing mesh -
but a mesh can carry an EE material (`.mtr`) naming our own vertex shader, as the moving textures do (`vsn5uvmove.shd`,
TMO-1 / MV), and a vertex shader runs on every draw with no clock: the quad turns to the camera while paused too.

## The shader's inputs (the game's own shaders and the engine, read)

- **The game's shaders** (`inc_common`, `inc_transform`, `inc_standard`; read in memory from the keys, values only). The
  lit vertex shader `vslit` is its defines, `#include "inc_standard"` and `ApplyStandardShader()`. With lighting on,
  `inc_common` sets `POSITION_VIEW` and `VERTEX_NORMAL` and declares `uniform mat4 m_mv` (model-view), `m_proj`,
  `m_view`, `m_view_inv`, the attributes `vPos` (vec4), `vTcIn`, `vNormal`, and the varyings `vPosView`,
  `vVertexNormal`, `vVertexTexCoords`. `inc_transform`'s `TransformMainCoordinates` writes `vPosView = (m_mv * vPos).xyz`
  and `gl_Position = m_proj * vec4(vPosView, 1.0)`. `inc_fog`'s `SetupFog` writes `fFogFragCoord` from `-vPosView.z`;
  `inc_keyhole`'s `SetupKeyholeFactors` writes `fFragCamOffset` (only while `KEYHOLING` is on: `KEYHOLING_ENABLED 0`
  turns it off).
- **The engine** (nwmain-linux, the win32 build's twin): `GLRender::ConnectShaderData` looks up `m_mvp`, `m_mv`, `m_m`,
  `m_normal`, `m_texture`, `m_view`, `m_proj`, `m_vp`, `m_view_inv`, `m_proj_inv`, `m_vp_inv` for every program, ours
  too; `GLRender::SetUniforms` uploads `m_view` = the matrix stack's view, `m_m` = its model, `m_mv` = view x model,
  `m_proj` = its projection. The model matrix at a mesh's draw is the Gob's transform (`Gob::Render`; the attachment's
  behaviour moves the Gob to the head node every frame, paused too) times each part's own position, turn and scale
  (`Gob::PartDraw` pushes them before the part draws). So `m_mv * (0, 0, 0, 1)` is the mesh node's origin in view space,
  following the head - exactly where the game itself puts that node.
- **The header the engine puts on every shader** (a string in the binary): `#version 330 core`, the quality defines
  (`GAMMA_CORRECTION`, `FRAGMENT_LIGHTING`, `SHADER_QUALITY_MODE`, `KEYHOLING_ENABLED`, `NO_DISCARD`, ...), `varying` as
  `out` / `in`, `attribute` as `in`, `texture2D` as `texture`.

## The billboard

After `ApplyStandardShader()` (the game's whole vertex setup: lighting, texture coordinates), the shader writes again:

- **centre** = `m_mv * (0, 0, 0, 1)` - the quad's node is 0.55 m over the head node and the quad is made round its
  origin in the node's x-z plane, so each vertex's `vPos.x` is its offset across and `vPos.z` its offset up;
- **the world's up in view space** = `m_view`'s third column (the world's +z: `inc_keyhole` compares world z with the
  player's height); **right** = up x the view's +z, normalised - level with the world, across the screen (from straight
  above it is undefined: then the screen's x);
- **spherical** (`vsn5bill_s`): up = view +z x right - the screen's up; the quad lies in the view plane, facing the camera
  fully, from above too. **Cylindrical** (`vsn5bill_c`): up = the world's up - the quad stands upright and turns about it
  only (from high above it is seen foreshortened; from straight above, edge on);
- `vPosView = centre + scale (vPos.x right + vPos.z up)`, `scale` the model's own (`m_mv`'s first column's length);
  `gl_Position = m_proj * vPosView`; `vVertexNormal = right x up`, toward the camera;
- `fFogFragCoord = 0` and `fFragCamOffset = 0` (each under its own `#if`): no fog and no keyhole on it. An additive glow
  fogged would add the fog's colour as a box round it; the game's own glows (decal, drawn by `vsvt` / `fst`) have neither,
  and nor did the four-disc marker.

The corners are counter-clockwise across and up, and right x up points at the camera, so the quad is the front face the
game draws (it culls back faces of a plain mesh) from any side. `check.js` runs the shader's lines in JS
(`build.js billboard`) over 1080 cameras - all round, low to straight above, near and far, the head anywhere, the model
scaled - and checks each.

## The material, the TXI and the fragment shader

- **`n5_tprobe_*.mtr`:** `customshadervs vsn5bill_s` (or `_c`), `customshaderfs fslit`, `texture0 n5_tally_03` (or `_07`).
  The game finds it by the mesh's bitmap name, so the bitmap is the probe's own name, as the n5u_ moving textures' are.
- **`n5_tprobe_*.txi`:** `blending additive` - the engine takes a mesh's blend only from its bitmap's TXI (MV), and an
  additive blend (src alpha, one; depth not written) also makes the mesh not opaque, so it is drawn with the
  see-through ones (`Material::GetIsOpaque`). **No `decal`:** a decal mesh is drawn by the game's own unlit pair
  (`vsvt` / `fst`) and its material dropped (SHP-8) - our shader would never run.
- **Why `fslit`, not the game's unlit `fst`:** `fst` draws `globalColor x texture`, and `globalColor` is the engine's
  current colour (`GLRender::m_globalColor`, uploaded when it changes) - `PartTriMesh::Mat` sets it to white only for a
  decal mesh (SHP-8's reading); for ours it would be whatever set it last. `fslit` with the model's ambient, diffuse and specular 0 and selfillum 1 gives exactly the texture:
  lighting = selfillum (1) + 0, clamped; its specular colour comes from the albedo, texture x diffuse = 0, so no
  highlight; the day and the night are the same. It is the pair our moving textures already use in game. Its fog and
  keyhole are switched off by the vertex shader (above).

## The art

- 256 x 128 (the quad's 2:1), 32-bit, bottom-up, alpha 255 everywhere: additive, the colour is the light and black
  adds nothing.
- Bars 10 px wide, white-gold `#fff2c0` (255 242 192) with an antialiased edge; round them a soft glow in the picked
  glow's red - `VFX_DUR_GLOW_LIGHT_RED` (visualeffects.2da row 412) is progfx.2da row 408, AlphaLightRed, colour
  1.0 0.4 0.4 - 0.9 bright at a bar's edge, falling off as exp(-(e / 7.5 px)^2) and cut smoothly to black by 18 px; a black
  border of 7 px all round (filtering and the texture's repeat bring in nothing).
- Bars 20 px apart; a five is four bars and a strike rising left to right across them; the next bar 34 px on. Every
  count 1-11 fits inside the border (checked), so the same layout can carry the full set.
- Drawn here from strokes (the distance to each segment); no font, no image (`build.js` loads only `fs` and `path`).
