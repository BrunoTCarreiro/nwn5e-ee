# Mesh textures that move, tinted textures, and keys on a mesh beat

The design for three asks about the VFX Bench and its Composer (#39): textures on meshes that scroll, breathe and turn;
a colour for textures that have none today; and keys (and following) for a mesh beat. Written 2026-11-18, after the
Composer's shapes (S1-S5) were built and before their in-game rows (SHP-1 to SHP-5) were run. Recon only so far: nothing
here is built.

## The owner's words

- 2026-11-17: "any plans for animated textures?" - proposed: scrolling (the game's water and lava move their textures),
  a flipbook on a mesh (unconfirmed), a pulsing glow or colour; scrolling after SHP-3's in-game repeat test. Then "where
  did we land with animated textures in the 3d meshes?" - "not started; waits on SHP-3 and your go".
- 2026-11-18:
  - "for the animated textures: 1. scrolling, scaling (breathing), rotating."
  - "Another thing, for the existing textures I'd like to add tinting options to them as right now when using a texture
    there's no way to change the color"
  - "for the breathing and rotating I was thinking about the textures on the meshes though"
  - "is it possible to add keyframes to animate the shapes? Or make them follow emitters, just like other elements"

## What the game's files say

Read 2026-11-18 from the game's keys and archives (through nwn5e-tools' `spell_bench/game_data.js`), the compiled
models' layout `src/data/vfx_model.cpp` reads, the game's shader texts (type 2069) and the strings in
`bin/win32/nwmain.exe` and `nwtoolset.exe`. Values were printed and read; the probes are in the job's tmp folder; nothing
of the game's was written or kept. **Proven** means the files show it; **inferred** and **unknown** say so.

### Scrolling: NWN:EE has no UV-scroll setting

- **`animateuv`, `uvdirectionx`, `uvdirectiony`, `uvjitter` and `uvjitterspeed` aren't NWN:EE's** (they're the KotOR
  model format's). Proven:
  - none of the game's 7,235 ASCII models uses any of them (of its 32,832 models; the other 25,597 are compiled);
  - nwmain.exe's mesh keyword table (the words its ASCII reader matches: `verts`, `tverts`, `faces`, `diffuse`,
    `transparencyhint`, `rotatetexture`, `tilefade`, `selfillumcolor(key)`, `alpha(key)`, `materialname`,
    `sampleperiod`, `animverts`, `animtverts` ...) has none of them, and neither nwmain.exe nor nwtoolset.exe holds those
    strings at all.
  - So there's nothing for our reader to read. The 24 bytes at the compiled mesh header's +0x1DC (Torlack's "texture
    animation data") are six offsets into the raw data on 583 of the game's 216,780 compiled meshes - all water
    (TCN01_water01, TDM01_water01 ...) - and empty (0xFFFFFFFF) on the rest: per-vertex arrays for the water, not a scroll.
- **How the game moves its water, lava and globes: the texture's own TXI** (`proceduretype`, proven; 735 game TXIs):
  - `arturo` (38): every tileset's water, the lava (tde01_lava01 on 85 tiles, tsw01_molten01), the elementals' skins, and
    `fxpa_globe` / `fxpa_globe2` - the spheres of Minor Globe and Globe of Invulnerability (vdr_globemin, vdr_globemax). A
    rippling distortion over time (`distort`, `distortangle`, `distortionamplitude`, `speed`, `arturowidth` /
    `arturoheight`, four-number `channelscale` / `channeltranslate`), not a slide.
  - `cycle` (7): a flipbook - `numx`, `numy`, `fps`. The waterfalls (tcm02_wtfall01: 1 x 8 frames at 12 a second) and
    **fxpa_flame02 (4 x 4 at 32), the texture of Elemental Shield's flame ring - vdr_eleshield's Cylinder02, a mesh of an
    effect**. So a flipbook on an effect's mesh is native.
  - `water` (4): fxpa_spellturn, shinywater, tds01_waterb, tsw01_waterb.
  - The exe's TXI words also include `wave`, `perlin`, `life`, `forcecyclespeed` and `anglecyclespeed`; no game TXI uses
    them. Unknown what they do.
- **Animated meshes:** 395 of the ASCII models (tile water: tss13, tno01) have `animmesh` nodes whose loop animation holds
  samples (`sampleperiod 2`, `animverts`, `animtverts`). In every readable one only the vertices move - the UVs are the
  same in every sample. No readable game model shows the engine playing UVs that change. (vff_wilting, Horrid Wilting's
  sphere, is a compiled animmesh in an effect; its samples' layout wasn't decoded.)
- **`rotatetexture` isn't motion:** 3,345 ASCII models set it to 1, all tileset tiles (trs 1,203, ttz 444, tss 350 ...) -
  a tile's texture turned with the tile (inferred). The stock vertex shader's texture matrix is `mat2(m_texture)`
  (inc_transform): a turn and a scale, no move.

### Breathing and turning a texture: no setting either; two routes the game provably has

- **EE materials** (.mtr, type 2072; the game ships 7):
  - A material names shaders and parameters. tti01_iceclear's: `customshadervs vslit_sm`, `customshaderfs fslit_sm`,
    `parameter float Metallicness 0.001`, `parameter float Specularity 0.0179`. A parameter is a uniform of that name
    (inc_material reads `Specularity`, `Roughness`, `Metallicness`).
  - **It attaches by the mesh's texture name** (proven): 42 compiled ice tiles name TTI01_IceClear as their texture, the
    compiled layout has no material field, and tti01_iceclear.mtr exists; c_yold_f's head (oldf_head) alike.
  - **Even a name no texture has** (proven): ctl_cg_btn_col's Plane105 names `gui_cg_color` in a texture slot; the game
    has no gui_cg_color TGA or DDS, only gui_cg_color.mtr (`customshadervs vsvt`, `customshaderfs fs_cg_btn_col`).
  - **A material's shader can move with time** (proven): fs_cg_btn_col moves with `sceneCurrentFrame`; the water's
    vswater takes `worldtimerTimeOfDay * 0.001` as seconds. Both are uniforms any shader may declare (inc_uniforms).
  - nwmain.exe's material words: `bitmap`, `texture0` to `texture10`, `renderhint`, `customShaderVS/FS/GS`, `parameter`
    (float, int), `transparency`, `twosided`. The ASCII mesh word `materialname` exists; no ASCII game model uses it.
  - The shaders are text (92 in base_shaders.bif and ovr.bif). A vertex shader sets the UVs (`vVertexTexCoords =
    mat2(m_texture) * vTcIn.xy`, in `ApplyStandardShader`); one of ours can transform them after that - move, scale,
    turn, any mix - and hand them to the game's own fragment shader (`fslit`), as tti01_icetrim pairs `vslit` with
    `fslit`.
  - **Not proven:** a shader of ours compiling and running in game, on an effect's mesh. That's the first in-game test
    (TMO-1).
- **A flipbook** (`proceduretype cycle`, proven above): a sheet of frames, each the texture moved, grown or turned a
  step further.
- **The mesh, not its texture** (the owner's fallback only): keyed meshes are in the game's own effects - orientation
  keys in 20 effect models (vdr_bigby_cle, vdr_stone01, vff_combansh ...), position keys in 27 (fx_rainice,
  vdr_magearmor2 ...), and a scale key (controller 36, one value: uniform) in vdr_bigby_int, its hand 1 -> 10 in 0.33 s
  and back in the cessation.

### Tint: how the game lights a textured mesh

- **The formula** (inc_standard and inc_lighting, the lit shaders; proven):
  - colour = texture x clamp(selfillum + material ambient x the area's ambient + material diffuse x the lights), each
    channel; the clamp to 1 when the engine runs without gamma correction (otherwise brighter, then tone-mapped);
  - alpha = the texture's alpha x the mesh's alpha.
  - So a mesh's material does tint its texture, in the game's light.
- **The game does it:** vim_cntglobe's sphere has a grey texture (fxpa_symbl_003: every pixel r = g = b) and ambient and
  diffuse 0.18 0.52 0.84, selfillum 0 0.26 0.58, alpha 0.8: a blue glyph globe.
- **Particles:** colour x texture (fsparticle: `VertexColor * texture2D(...)`): an emitter's colours tint its texture
  already.
- **Our writers already write what a tint needs** (no change): WriteVfxModel a plane's ambient, diffuse, selfillumcolor
  and alpha; floatdisk.js's meshLines a mesh's.
- **Why today's library-texture tint washes out:** a shape's library texture writes ambient = diffuse = Colour and
  selfillum = Colour x Glow. At Glow 1 that's Colour x (1 + the light), clamped: Colour 0.6 0.8 1 under lights of 0.5
  is 0.9 1 1 - nearly white by day, the colour only at night. The stage shows none of it: it draws a texture as it is.

### Where a texture has no colour control today

| where | today |
|---|---|
| a shape, Soft | Colour (it's in the generated texture) |
| a shape, a library texture | Colour, in the material - shown only in game, and washed out (above) |
| a Composer part (a game model, an item, your own .mdl) | none (a dyed PLT part has its six dyes, nothing else) |
| a mesh beat | none: the effect's copy keeps the spell mesh's material |
| an emitter's Chunk model | none for a game chunk; a spell mesh used as one takes its Composer colours |
| an emitter's particles | colour start / mid / end |
| a game effect's own planes (Entangle's vines) | none (kept whole; out of scope) |
| the Floating Disk | its colours in composition.json |

### Following an emitter, and keys on a mesh beat

- **Following works already.** The Mesh card's Hangs under lists the effect's nodes and emitters. WriteVfxModel writes a
  plane's `parent` (the node or emitter, else the model's root), in the geometry and in each animation. The bench's sim
  (`planesAt`) places a plane under its parent's place at that moment (`placesAt`, with the parent's keys), so a hung
  mesh follows on the stage. In game a mesh under a keyed parent moves with it (vdr_bigby_cle's finger meshes under its
  keyed hand). A mesh under an emitter is untested in game.
- **Keys on the mesh itself don't reach the game.** The draft already carries them (C++ `DraftKeys` / `DraftKeyLine` and
  core.js `draftKeys` read and write `<anim>_position` and `_orientation` for a plane as for a node). But:
  - WriteVfxModel writes only a plane's `alphakey` in its animations;
  - `LastKeyTime` (and the sim's `lastKey`) count only a plane's alpha;
  - nothing has scale keys (`VfxKeys` has no scale; nothing writes `scalekey`);
  - the sim moves a plane by its parent only;
  - the Mesh card has no key table, and Refresh from the mesh drops a beat's keys but its fades.

## What the owner sees

**A part's card** (the Composer), for a shape: after its Texture, a **Texture motion** group, shown while the part has a
textured mesh (a shape's Soft or library texture, a model's):

```
| Texture [fxpa_symbl_003   v]         |
| Colour [####]  Opacity [0.35]  Glow [1] |
| TEXTURE MOTION                         |
|  Scroll round [0   ] Scroll up [0.25 ] |  repeats a second (+: round it counter-clockwise from above / up it)
|  Turn [0    ] °/s                      |  counter-clockwise seen from above, round the texture's middle
|  Breathe [0.2 ]  Breaths [0.5 ] /s     |  how much bigger and smaller it gets (0-0.9), how often
```

**A part's card** for a game model, an item or your own .mdl: **Tint** (a colour; white: none) and the same **Texture
motion** group, for every textured mesh of the part.

**The Mesh card** (an effect's mesh beat), below its fades:

```
| Tint [####]  (white: as the spell mesh made it)                                 |
| Its texture: scrolls up 0.25 a second (from the spell mesh; change it there)    |
| MOVES      Start (impact): move keys  t x y z  [+ key]   turn keys: 0            |
|            Loop (duration): ...                                                  |
|  Drop in: start this high [5] Land after [0.5] In [Start v] [Drop in]            |
|  Turns a second [0.5] [Make it spin]   Breathe: how much [0.1] a second [0.5] [Breathe] |
|  [Clear its keys]                                                                |
```

- The stage shows all of it: a texture tinted as the game's formula has it, a texture moving, the beat moving, turning
  and breathing by its keys, under whatever it hangs from.

## How each works

### Tint (no game-side change)

- **A Composer part:** a part may carry `"tint": [r, g, b]` (0-1; white is no tint and isn't written). floatdisk.js
  multiplies every one of the part's meshes' ambient, diffuse and selfillum by it, so both writers write a tinted
  material (meshLines for a placeable or the Floating Disk, the draft's planes for a spell mesh). A dyed (PLT) part is
  tinted over its dyes. A shape takes no `tint`: its Colour is its tint.
- **A shape with a library texture:** its material becomes selfillum = Colour x Glow, ambient = diffuse = Colour x (1 -
  Glow) (call 2). Glow 1 is the texture times the Colour in any light; Glow 0 is a placeable's: the texture times the
  Colour, lit by the scene.
- **A mesh beat:** a beat-wide `tint`. Its planes' material is the spell mesh's times the tint, made again whenever the
  tint changes or the beat is refreshed from its mesh; the draft keeps `tint = r g b` on each plane (the bench's own,
  never in the model) so Refresh can apply it again. The game sees only the plane's material, so a tinted beat shows in
  game with today's DLL.
- **The stage:** a textured mesh is drawn as texture x min(1, selfillum + diffuse x the stage's shade) - the game's
  formula with the stage's one soft light standing for the scene's. A game mesh with ambient and diffuse 1 and no
  selfillum looks as today (texture x shade); a tinted one shows its tint; at Glow 1 the stage is exact. An untextured
  mesh keeps today's formula.

### Texture motion (no game-side change; data files the game reads)

- **What moves:** a part's `motion: { scroll: [u, v], turn, breathe, rate }` - repeats a second along u and v, degrees a
  second, how much bigger (0-0.9) and breaths a second. Any part may carry it, a shape too: it moves every textured mesh
  of the part. Absent or all 0: no motion, nothing written changes.
- **The maths** (one function, floatdisk.js `uvMotion`), at time t in seconds, round the pivot p (the middle of the
  mesh's UV box, worked out by the generator):
  - s = 1 + breathe x sin(2 pi x rate x t);
  - uv' = R(-turn x t) (uv - p) / s + p - scroll x t.
  - So + scroll moves the picture along +u / +v, + turn turns it counter-clockwise (From above, seen from above), and
    breathing grows and shrinks the picture, not the mesh.
- **In game - a material per moving texture:**
  - The mesh's bitmap becomes the material's name, `n5u_` and 12 hex digits of the texture's name, the motion and the
    pivot (cyrb53, as `n5s_` and `n5p_` names are made). Our writers write a bitmap already: neither changes.
  - `n5u_<hash>.mtr` (generated, git-ignored as `n5s_` is): `customshadervs vsn5uvmove`, `customshaderfs fslit`,
    `texture0 <the real texture>`, and `parameter float` lines for the scroll, the turn, the breathing and the pivot.
  - `vsn5uvmove.shd` (one file, ours, committed in data\files): the game's lit vertex shader's setup by its own include
    (`inc_standard`, by name - nothing of the game's copied), then the UVs moved by the formula above, with
    `worldtimerTimeOfDay * 0.001` as t.
  - The files go where the mesh's texture would: data\files (served to the game whole, as every file there is), a spell
    mesh's vfxedit\saved, Play in game's vfxedit\.
  - **If `texture0` doesn't bind in game** (TMO-1 shows the mesh untextured): the generator also writes the texture under
    the `n5u_` name - the soft one made again; a library one copied from the game with its TXI into the build output,
    git-ignored, as the dyed parts' `n5p_` bakes are made from the game's PLTs. The probe builds both, so one test
    decides.
- **The time:** the game's world clock, not the effect's. The motion runs on through the effect's start, loop and end
  with no jump at the loop (nothing to match to the loop's length). It isn't started by the effect: two casts don't move
  in step from their starts. Inferred from its name, not seen: `worldtimerTimeOfDay` wraps at the end of a game day, so a
  scroll would jump once a game day (48 minutes at the default 2 minutes an hour). TMO-1 can't see it; it's a limit.
- **On the stage:** the mesh program gains one uniform, the UV matrix (`uvMotion` at the stage's time, v flipped as the
  stage holds its UVs); identity for everything else. The effect mode's time is the preview's; the Composer runs a clock
  while anything shown moves. A beat's planes name an `n5u_` material: the server reads it and gives the page the real
  texture and the motion.
- **Library textures that already move** (found on the way): the stage draws a `cycle` texture's whole sheet (Elemental
  Shield's ring shows all 16 flames) and an additive one (fxpa_symbl_003's `blending additive`) blended normally. The
  server reads both from the TXI; the stage shows the current frame through the same UV matrix and draws additive
  textures additively. An `arturo` ripple stays still on the stage (a limit).

### Keys on a mesh beat (a game-side change, announced first)

- **WriteVfxModel** writes a plane's own keys in each animation, beside its alpha: `positionkey`, `orientationkey` and a
  new `scalekey` (controller 36, uniform, as vdr_bigby_int keys its hand). A plane without keys stays out of the
  animations: every model written before is byte for byte the same.
- **The draft** gains `<anim>_scale = t s ...` (any section's keys; written after `_alpha`, so older drafts keep their
  bytes) and a plane's `tint = r g b` (only when it isn't 1 1 1). `LastKeyTime` counts a plane's every key.
- **The bench:** core.js the same; the sim moves, turns and scales each plane by its keys under its parent; the Mesh card
  gets the Motion card's key table, Drop in and Make it spin, and **Breathe** (scale keys in the loop, whole breaths); a
  beat's keys are its planes' (one edit, every plane of the group, as its fades are); Refresh from the mesh keeps the
  beat's keys and tint.
- **Until the game session installs it:** the stage plays the keys at once; in game the beat shows unmoved (today's DLL
  writes only its alpha keys; an older draft reader skips `_scale` and `tint` without an error).

## Calls I made

1. **Texture motion goes through an EE material and a vertex shader of ours** - one route for scroll, breathing and
   turning together, smooth, and the stage can show the same maths. *Why:* NWN:EE has no UV-motion setting (above); a
   material attached by the texture's name is proven three ways in the game's own files; a time-driven material shader
   is the game's own practice (fs_cg_btn_col). A flipbook is the fallback (Q1).
2. **A library texture's Glow means "lit by itself"**: selfillum = Colour x Glow, ambient = diffuse = Colour x (1 -
   Glow). *Why:* the game's formula clamps light at 1, so today's mapping washes a pale tint to white by day; with this
   one Glow 1 is exactly texture x Colour in any light (and the stage can be exact there), and lower Glows bring in the
   scene's light. Soft and None shapes keep today's mapping: they look as they do.
3. **Tint is a multiply** of the part's or beat's material, never a new texture. *Why:* the game multiplies (formula and
   vim_cntglobe); nothing new to ship; the colour changes freely.
4. **Texture motion belongs to the spell mesh** (a Composer part's, shapes included); a beat shows it and doesn't change it.
   *Why:* the motion is in the mesh's material name; a beat that wanted its own would need its own material set per beat.
   A second variant gives a second motion (Q2 asks).
5. **The pivot is the middle of the mesh's UV box.** *Why:* From above's middle is the shape's centre (a glyph turns and
   breathes round it); a Wrapped shape with repeats turns round the middle of its repeats.
6. **Keys on the planes themselves** (the coordinator's direction), not a keyed node the beat hangs under. *Why:* a node
   would give position and turn keys with no game change, but complicate Hangs under and Refresh, and scale needs the
   writer change anyway; one announced change covers all three.
7. **The mesh as a texture's stand-in is offered only as Q1's fallback, named as one:** turning a round shape about its
   own axis looks exactly like its Wrapped texture scrolling round (or its From above texture turning); breathing the
   mesh grows and shrinks the shape itself - the zone's edge moves - where breathing the texture leaves the shape still.
8. **The game's world clock is the motion's time** (`worldtimerTimeOfDay`), not `sceneCurrentFrame` (frames: faster at a
   higher frame rate).

## Limits

- **In game, untested until TMO-1:** our shader in game, on an effect's mesh and a placeable's; whether `texture0` binds;
  whether a library texture's TXI (additive, its own flipbook) applies under the `n5u_` name - if not, the copy route
  takes it along.
- **A texture that repeats past UV 1** is what a scroll, a turn or a shrinking breath shows at its edges: SHP-3 step 4
  answers it. The game's TXIs point to repeat as the default (`clamp` is an opt-in in 15 TXIs, 14 of them the GUI's).
- **The phase:** the game's motion starts where the world clock is, not at the effect's start; the stage's starts at 0.
  The clock may wrap once a game day (a jump; inferred).
- **The stage's light** is one soft light; the game's tint under its lights is close, exact only at Glow 1. An `arturo`
  ripple doesn't move on the stage.
- **A changed shader** may need the game restarted (shaders are compiled once).
- **Mesh beat keys in game** need the install (M4); until then the beat stands still in game.
- **A mesh under an emitter** in game is untested (the writer writes it; MBK-3 checks it).
- **Shipping** a spell mesh whose material is `n5u_` needs the route addendum (c) left open for `n5s_` textures.

## In-game tests (rows to write later, each standing on its own)

| ID | what |
|---|---|
| TIN-1 | A dome with a library texture (fxpa_symbl_003), Colour orange, Glow 1, Play in game: the glyph is orange by day and night; Glow 0.3: orange, darker at night |
| TIN-2 | A placeable project with a tinted game part (a crate, Tint pale green): Save and build, install, place it: a green-tinted crate |
| TIN-3 | A mesh beat with Tint red, Play in game: the beat is red; Refresh from the mesh: still red |
| TMO-1 | The probe (`n5m_uvprobe`: three domes - scroll up, turn, breathe - texture0 route; three more by the copy route) as a mesh beat, Play in game: which move; no shader error in the game's log |
| TMO-2 | A dome with fxpa_symbl_003 From above, Turn 30, Breathe 0.2: the glyph turns and pulses; additive as the game's own globe |
| TMO-3 | A placeable with a moving texture: Save and build, install: it moves (data\files route) |
| MBK-1 | A beat with Drop in from 3 m: it falls in game as on the stage |
| MBK-2 | A beat with Make it spin and Breathe in the loop: it spins and breathes, no jump at the loop |
| MBK-3 | A beat hung under an emitter that orbits (Make it orbit): the mesh goes round with it |

## Questions only the owner can answer

- **Q1. The route for moving textures.** NWN:EE has no scroll, breathe or turn setting for a texture. Two routes the game
  provably has, and a stand-in:
  - **A (recommended): a material and a small shader of ours** - smooth, any mix, the stage shows the same; it needs one
    in-game probe first (TMO-1), and no DLL change.
  - **B: a flipbook** (`proceduretype cycle`, as Elemental Shield's ring) - certain to work, but in steps (frames), blurrier
    (each frame a fraction of the sheet) and a bigger file per motion.
  - **C: move the mesh instead** (keys): fine for turning a round shape about its axis; breathing would grow the shape,
    not the pattern.
  - And if A's probe fails: B or C?
- **Q2. A motion per beat?** As planned, a texture's motion is the spell mesh's (one motion per variant). Should a beat be
  able to change it (the same dome scrolling in one effect and turning in another)?
- **Q3. Glow on a library texture** (call 2): Glow 1 keeps the tint's colour in any light. If you'd rather it stays as now
  (brighter, paler by day), say so before M1.
- **Q4. Which first?** M1 (tint, about half a session) needs nothing else. Then either the moving textures (M2-M3, after
  the probe) or the beat's keys (M4-M5, which need the game session's install).

## Size

Five tasks, **about 3½ sessions**:

| task | what | where | about |
|---|---|---|---|
| M1 | tint: parts, library-textured shapes, the stage's formula | anim, nwn5e-tools | ½ session |
| M2 | moving textures in the generators: the maths, the material, the shader, the probe | anim | 1 session |
| M3 | moving textures on the bench: the cards, the stage, Play in game; cycle and additive TXIs | nwn5e-tools | 1 session |
| M4 | keys on a plane in the model and the draft (announced) | anim src/data, tests | ⅓ session |
| M5 | the Mesh card's keys, Breathe and Tint; the sim | nwn5e-tools | ⅔ session |

## Addendum 2026-11-18 (a): the owner's answers

The owner, 2026-11-18, on Q1-Q4 as put to them with recommendations: **"accept your recommendations"**.

- **Q1: route A** - an EE material and our own vertex shader (`n5u_<hash>.mtr`, `vsn5uvmove.shd`), proven first by the
  in-game probe TMO-1 at the end of M2. **If the probe fails, the fallback is B, a flipbook** (the motion baked into a
  sheet the game cycles); C (moving the mesh) is not the fallback.
- **Q2: no** - a texture's motion stays the spell mesh's (set in the Composer); a mesh beat doesn't override it. An
  override can come later.
- **Q3: yes** - the Glow remap for a library texture (call 2): Glow 1 shows texture x Colour in any light; a library
  texture saved earlier at Glow 1 will look a little darker and more coloured.
- **Q4: M1 (tint) first, then the moving textures (M2, the TMO-1 gate, M3), then the beat's keys (M4, M5).** The
  coordinator may run M4 while TMO-1 waits on the owner (it shares no files with M2-M3), as long as the order of what the
  owner gets to test stays tint, moving textures, keys.
- **The issue:** #39 (the Composer's), named by every commit.
