# How the game runs and draws particles (nwmain EE 89.8193.37)

Read from the game's own code for the VFX Bench (0.42.2; owner, 2026-10-26: "reading the
game's code would be the most straightforward way"). The preview (`public/sim.js`,
`public/view.js`) follows these rules; addresses are RVAs in nwmain.exe. No game file is
copied here - only what the code does.

---

## PartEmitter animatable fields (MdlNodeEmitter::InternalParseField 0x9a030 -> controller offsets)
alphaEnd 6c, alphaStart 70, birthrate 74, bounce_co 78, colorEnd 7c(3), colorStart 88(3), combinetime 94,
drag 98, fps 9c, frameEnd a0, frameStart a4, grav a8, lifeExp ac, mass b0, p2p_bezier2 b4, p2p_bezier3 b8,
particleRot bc, randvel c0, sizeStart c4, sizeEnd c8, sizeStart_y cc, sizeEnd_y d0, spread d4,
threshold d8, velocity dc, xsize e0, ysize e4, blurlength e8, lightningDelay ec, lightningRadius f0,
lightningScale f4, detonate 108, alphaMid 214, colorMid 218(3), percentStart 224(byte), percentMid 225,
percentEnd 226, sizeMid 228, sizeMid_y 22c.
Controllers (MdlNode::InternalControl 0x51c50): list at node+0x78 (12-byte entries: +0 Part offset,
+0xA type nibble 1 float / 3 vector / 4 quaternion), count node+0x80.
## Particle (this): +8 PartEmitter*, +0x10 position(3), +0x34 direction quaternion(4)
## Particle::randomDirection 0x68800
half = spread * 0.5; theta = (rand() % (1 + int(half*100))) * 0.01  -> uniform ANGLE in [0, spread/2]
  (steps 0.01), rotation about emitter axis at PartEmitter+0x140 (vec3);
phi = (rand() % 628) * 0.01 about axis PartEmitter+0x158; q = q_phi * q_theta -> particle+0x34.
**deadspace** (node+0xa0, radians; 0x68a25-0x68d0f, read 0.42.4): if != 0, with
V = normalize(particle position - camera position (cam+0xb0)) and Y = the particle's own y
(axisY(q); the quaternions here are laid out w x y z - Quaternion::operator* 0x22ab00 is the
Hamilton product, the axis-angle constructor puts cos(half) first):
```
limit = cos(pi/2 - D)                       // = sin D; cos is the double CRT cos 0xc79d30
d = dot(V, Y)
if (limit > |d|):                           // Y within D of square to the camera line
  n = normalize(cross(V, Y))                // cross 0x22fba0 = a x b
  loop:
    a = d < 0 ? D + pi/2 - d : d - pi/2 + D
    q' = Quaternion(n, a) * q               // always from the ORIGINAL q
    d = |dot(V, axisY(q'))|                 // later passes: always the d >= 0 branch
  while (limit > d)
  q = q'
```
The direction (axisZ) turns with it. The loop has no cap: simulated, for D up to 0.39 it always
ends after one pass; from 0.40 some particles never leave it (the game would hang). The game's
effects use 0.0037 (1 emitter), 0.1745 (19) and 0.2094 (78); ASCII models read it with `%f`
(radians too). The VFX Bench does the same (at most 64 passes) and holds drafts and written
models at 0.39.
## Particle::initialize 0x66450 (this, float, float scale)
dir quaternion = emitter orientation (+0x1d0); frame = int(frameStart) (+0x44)
speed = velocity + (randvel > 0.01 ? +-(rand() % int(randvel*100)) * 0.01 : 0)  -> UNIFORM IN (-randvel, +randvel)
area: xs = int(xsize*100*scale), ys = int(ysize*100*scale); randomPosition 0x68d80:
  a = rand() % int(xs*0.5) (xs > 3), random sign, * 0.0001 along emitter X axis (+0x140); same ys along Y (+0x14c)
  -> offset in (-xsize/2, +xsize/2) cm: xsize / ysize = full width in cm (as the bench had)
position = emitter world position (+0x1e0) + offset; start position copy at particle+0x64
flags (node +0x174) bit 7 -> inheritVelocity; spread != 0 -> randomDirection
direction = rotate((0,0,1)) by the quaternion -> +0x4c; velocity vector = dir * speed -> +0x58
## FountainEmitter::Update 0x64b30 (this PartEmitter, dt)
on = birthrate > 0 (+0x23c). chkParticleLife(dt). render mode (+0x1a8) == 6 (Motion_Blur?): each
particle's trail point (+0x64, set to the spawn position) = lerp(trail, pos, k), k = min(dt, blurlength)/blurlength
-> the tail lags ~blurlength seconds behind (blur 10: the tail stays near the spawn point).
spin: particle +0x1c += dt * particleRot (+0xbc), wrapped to +-2pi.
move: call [this+0x170](dt) (the move function).
spawn (spawntype 0; node+0xb4 == 0): n = int(birthrate); n <= 0 -> none. acc (+0x198) += dt;
if acc >= 1/birthrate: count = int(acc*birthrate) % (n+1); spawn count; acc = 0 (fraction DROPPED):
at 60 fps birthrate 100 -> 1 a frame (60/s), 200 -> 3 a frame.
spawntype 1 (node+0xb4 != 0): count by distance the emitter moved (* birthrate) - trails.
particle = new Particle(this, dt) then vtable+0x10 initialize(dt, scale = gob scale [this+0x50]->vf 0x2d0);
chkRandom(particle) (the random frame).
## chkParticleLife 0x65f10: lifeExp >= 0 and age (+0x74) >= lifeExp -> removed; else age += dt (lifeExp < 0: never dies)
## chkRandom 0x660c0: flag 0x20 (random): frame = frameStart + rand() % (frameEnd - frameStart + 1)
## moveParticleStandard 0x68010 (dt); node flags +0x174: 0x4 wind, 0x10 bounce, 0x40 inherit, 0x100 / 0x400 (inherit_part / local)
inherit: position and velocity turned with the emitter's rotation change and carried with its move.
pos += dt * vel (+ decaying inherited velocity p+0x20 * vec p+0x24, p20 *= (1 - dt)); wind if 0x4;
GRAVITY: vel.z -= mass * 9.81 * dt. (drag / grav not used here.)
bounce (0x10): segment old->new pos tested against the scene; on a hit: bounce_co == 0 -> vel = 0 (stops);
else v = reflect(v, n) * 0.75, then v.z *= bounce_co * clamp(0.033/dt, 0.5, 1); if |v|^2 < 0.5 -> STOPPED
(+0x70 = 1): orientation = RotationArc(z, n) (lies on the surface), pos += n * 0.02 (chunky: n * sizeStart).
Stopped particles no longer move.
## AnimateParticles 0x61210: dt = min(dt, 0.5); emitter axes X +0x140, Y +0x14c, Z +0x158 from the orientation; vf 0x100 Update(dt)
## ExplosionEmitter::Update 0x647c0: chkParticleLife, trail, spin, move; when detonate (+0x108) rises (prev +0x1c8 < 1):
spawn int(birthrate) particles at once (int truncation); then +0x1c8 = detonate, detonate = 0.
## SingleEmitter::Update 0x65cd0: int(birthrate) != 0: no particle -> create one (initialize: normal speed/direction,
random frame); else age += dt; lifeExp > 0 && age > lifeExp && node loop (+0x16c) -> age = 0; spin, trail, move.
int(birthrate) == 0 -> the particle is removed. (No chkParticleLife: it never dies by age.)
## PartEmitter ctor 0x60420 defaults: colorMid 0, percentStart/Mid/End = 0xff, fps 0, frameEnd 0, frameStart 0,
grav 0, sizeMid 0, sizeMid_y 0. MdlNodeEmitter ctor 0x875a0: node+0x98 = 5 (?), flags 0.
## Point to point (read 0.42.5)
- **Target.** PartEmitter::Initialize (0x61f1c): node flag 0x1 (p2p) registers SetEmitterTarget
  (0x643c0) on the gob as "_EmitterTarget"; Gob::Append fires it after CreateReferenceObjects. It
  takes the emitter's first child whose vf 0x70 (its reference object) isn't null: that object
  (+0x68) goes to PartEmitter+0x1c0 (vf 0xc0 its position, vf 0xc8 its rotation). Every p2p
  emitter of the game's (117 compiled) has exactly one child, a reference node (header flags 0x11)
  of model fx_ref; ASCII: `node reference` with `refModel` / `reattachable`, and in the
  animations `node dummy` (the game's own ASCII effects). No target: +0x1c0 stays null and the
  move calls into it - a crash. ASCII flags: `p2p` and `p2p_sel` read as %s (true / false / 1 / 0).
- Flag 0x2 (p2p_sel): **moveParticleP2PBezier** (0x66940); else **moveParticleP2PGrav** (0x67340;
  +0x210 = grav).
- Controllers: combinetime 120 (+0x94), drag 124 (+0x98), grav 140 (+0xa8), p2p_bezier2 152
  (+0xb4), p2p_bezier3 156 (+0xb8), threshold 188 (+0xd8).
- **Bezier**, per particle: inherit (0x40) only; free = pos + dt vel (+ wind);
  t = min(1, age / lifeExp); P0 = the emitter (+0x1e0), P1 = P0 + p2p_bezier2 * emitter z
  (+0x158), P3 = the target, P2 = P3 + p2p_bezier3 * axisZ(target rotation);
  s = powf(t^3, combinetime) (0xc7c810); pos = (1 - s) free + s (u^3 P0 + 3tu^2 P1 + 3t^2u P2 + t^3 P3),
  u = 1 - t; then the decaying inherited velocity, vel.z -= mass 9.81 dt, bounce.
- **Gravity**, per particle: inherit, or inherit_local, and inherit_part - the last two also turn
  the position (about the previous gob / emitter position) and the velocity by the gob's turn
  since the frame before (the standard move only shifts); next = pos + dt vel (+ wind);
  d = target - pos; threshold^2 >= |d|^2: age = lifeExp and the function RETURNS (the
  particles after it don't move this frame, the inherit bookkeeping is skipped); else
  vel *= powf(drag, dt); g = normalize(d) grav dt; next += g dt; vel += g; +0x4c =
  normalize(next - pos); segmentproject(pos, next, target) (0x231aa0, clamped to the step)
  within threshold: age = lifeExp; pos = next (+ the inherited decay); vel.z -= mass 9.81 dt;
  bounce.
- Neither checks the stopped flag (+0x70): a particle at rest is still moved.

---

# NWN:EE particle DRAWING — reverse-engineered (nwmain.exe 64-bit, build 89.8193.37)

Everything below comes from `PartEmitter::RenderEmitter` (RVA 0x62900–0x64170; `RenderStandard`
0x643b0, `RenderLinked` 0x64170 and `RenderMotionBlurred` 0x64180 are all `jmp 0x62900`), plus the
helpers it calls. Addresses are RVAs.
The particle shaders (`vsparticle` / `fsparticle`) were read where the game has them, in
`data/ovr.bif` (via `nwn_retail.key`); `ovr.bif` overrides the older
`base_shaders.bif` copies. The override is the version that matches the uniform names in the exe
(`particleSoftness`, at 0xaebe9 in `GLRender::ConnectShaderData`).

Conventions:
- The `Quaternion` struct is **(w, x, y, z)**. Evidence: identity constant at 0xd81960 = (1,0,0,0).
  Spin quaternion built as `[+0]=cos(θ/2)`, `[+4..+c]=axis·sin(θ/2)` (0x63aa6–0x63b38). Axis
  extraction formulas match w-first at 0x62b02, 0x639d6 and `Initialize` 0x62065. The product at
  0x22ab00 is the Hamilton product `p⊗q` (rdx=p, r8=q).
- `axisX(q)`, `axisY(q)` and `axisZ(q)` are the columns of q's rotation matrix, i.e. `rotate(e_x)` etc.
- Camera: `CurrentCamera+0xbc` = camera world quaternion. Its X = screen right, Y = screen up,
  Z = **towards the viewer** (GL convention). This follows from the CCW winding below being
  front-facing under `glFrontFace(GL_CCW)` (`ResetRenderStates` 0xb6204) with back-face culling.
  In three.js, these are columns 0/1/2 of `camera.matrixWorld`.

---------------------------------------------------------------------------------------------------
## 1. Enums and where they are set

`PartEmitter::Initialize` 0x61d80 compares the MdlNodeEmitter strings `render` (node+0xd8) and
`blend` (node+0xf8). The parser `InternalParseField` reads them with `sscanf("%s")` at
0x9ad1b/0x9ad9c. The value comparison is **case-sensitive strcmp** (0xc799d0, or inline 7-byte
compares for "Normal"/"Linked"). Field names, by contrast, are matched with `_stricmp`.

| `render` string | PartEmitter+0x1a8 | evidence |
|---|---|---|
| `Normal` | **0** | 0x61df9 |
| `Billboard_to_Local_Z` | **1** | 0x61e3a |
| `Billboard_to_World_Z` | **2** | 0x61e18 |
| `Linked` | **3** (draw fn RenderLinked) | 0x61e63 |
| `Aligned_to_World_Z` | **4** | 0x61e89 |
| `Aligned_to_Particle_Dir` | **5** | 0x61ea8 |
| `Motion_Blur` | **6** (draw fn RenderMotionBlurred) | 0x61ece |
| anything else | not written; the ctor 0x60420 never initialises +0x1a8 → undefined (treat as 0) | |

If `chunkName` (node+0x158) is non-empty, the draw fn becomes `RenderNothing` (0x61eed). Chunky
particles are drawn as models elsewhere.

| `blend` string | PartEmitter+0x68 | evidence |
|---|---|---|
| `Normal` | **0** | 0x61f98 |
| `Punch-Through` | **1** | 0x61f9e–0x61fbb |
| anything else (`Lighten`, typos, empty) | **2** | 0x61fb6 `setne cl; inc ecx` |

Other node fields used by the renderer:
- `xgrid` → node+0xac (int), parsed at 0x9af1f.
- `ygrid` → node+0xb0 (int), parsed at 0x9af8e.
- `twosidedtex` → node+0x168 (int), parsed at 0x9b0eb.
- `renderorder` → node+0x170 (word), parsed at 0x9abc4.
- `deadspace` → node+0xa0 (float), parsed at 0x9b06c. Used **only** by `Particle::randomDirection`
  (0x68a25…), not by rendering.
- Node flags dword at node+0x174, from `MdlNodeEmitter::Enable*` at 0x61a40–0x61ae0:
  - 0x1 p2p, 0x2 p2p_sel, 0x4 affectedByWind
  - **0x8 m_isTinted**
  - 0x10 bounce, 0x20 random, 0x40 inherit, 0x80 inheritvel, 0x100 inherit_local
  - **0x200 splat**, 0x400 inherit_part

---------------------------------------------------------------------------------------------------
## 2. Age fraction and SIZE

### Age fraction t (0x62e7c–0x62ec7, 0x634a0–0x634ef)
- `invLife = 1/lifeExp` if lifeExp > 0, else 0.01. The 0.01 is a dead value, because per-particle
  interpolation only runs when lifeExp > 0.
- `t = min(1, particle.age * invLife)`. The age is particle+0x74. **No mid/percent keys.**
- Exception: when word PartEmitter+0x1cc == 0, `t = i / count` (array index instead of age).
  `Initialize` sets +0x1cc = 1 (0x61f06); only `LightningEmitter::Initialize` sets it to 0
  (0x61d6c), and it also forces lifeExp = 1.
- lifeExp == 0 → the whole emitter draws **nothing** (0x62a47–0x62a55).
- lifeExp < 0 (immortal) → nothing interpolates; start values are used.

### The *Mid / percent* fields are NOT used for drawing
alphaMid, colorMid, sizeMid, sizeMid_y and percentStart/Mid/End are parsed and can be animated by
controllers. In the whole binary, however:
- PartEmitter offsets 0x214–0x226 are only written by the ctor (0x60509–0x60537; percents default
  0xff).
- 0x228/0x22c (sizeMid, sizeMid_y) are read only by `CalculateRadius` (0x618a2/0x618ce), which
  computes the bounding radius.

So in EE 8193.37 everything is a **2-key linear lerp start→end**.

Render reads the emitter's **current** start/end fields every frame. Keyed controllers
(e.g. `colorStartkey`) therefore change all live particles at once; values are not baked at birth.

### Size (0x62aa2–0x62ab0, 0x633a3–0x63459, per particle 0x63593–0x6362c)
Definitions:
- `S = gob scale` (Gob vf+0x2d0, or 1 if there is no gob).
- Half sizes: `hs0 = S*sizeStart*0.5`, `hs1 = S*sizeEnd*0.5`, `hy0 = S*sizeStart_y*0.5`,
  `hy1 = S*sizeEnd_y*0.5`.

Flags:
- `sizeVaries = lifeExp>0 && sizeStart != sizeEnd && !splat` (0x6311d–0x6314e).
- `sizeYVaries = lifeExp>0 && sizeStart_y != sizeEnd_y && !splat` (0x63143–0x63184). It also
  requires "not both zero", which is redundant.

Per particle:
```
w = sizeVaries  ? lerp(hs0, hs1, t) : hs0                 // half width
h = sizeYVaries ? lerp(hy0, hy1, t)                        // note: from 0 if sizeStart_y==0 && sizeEnd_y!=0
  : (sizeStart_y == 0 ? w : hy0)                           // 0x635cc..0x63601; lifeExp<0 path 0x635e2
if (splat && particle.stopped) { w = hs1; h = (sizeEnd_y != 0) ? hy1 : hs1; }   // 0x63605-0x63628
```
- A splat particle that has not stopped never changes size: it uses sizeStart, plus sizeStart_y if
  that is non-zero.
- **Size = full quad width/height in metres.** Corners are at ±w and ±h, and w is half of size·S.
  This holds for modes 0, 1, 2, 4 and 5.
- Linked (3) and Motion_Blur (6) **double the width and halve the length extension**
  (0x637eb–0x6380e, 0x639a2–0x639c3):
  ```
  W = 2*w   // = size·S, so the ribbon is 2·size·S wide
  H = h/2   // = size_y·S/4, each end extends by this along the streak
  ```

---------------------------------------------------------------------------------------------------
## 3. COLOUR and ALPHA

### Tint (only if flag 0x8 `m_isTinted`) — 0x63045–0x630a3, 0x6318e, 0x63236
`L = LightManager::GetEmitterLighting(emitterWorldPos)` is evaluated once per emitter per frame
(0x7ee50). It sums:
- each scene light `colour * intensity * (1 - dist/radius)` (skipped when beyond the radius),
- the headlight (diffuse + ambient) if enabled,
- two area lights times their multipliers,

then clamps each channel to ≤ 1. Then:
```
cS = clamp01(cmul(colorStart, L)),  cE = clamp01(cmul(colorEnd, L))
```
Without the tint flag: `cS = clamp01(colorStart)`, `cE = clamp01(colorEnd)` (clamp 0x22fa30 with
0 and 1).

Colour bytes are packed with 255.99 (0x631e8):
```
CS = cS*255.99, CE = cE*255.99
colorVaries = lifeExp>0 && colorStart != colorEnd        // any component, raw (pre-tint) values (0x630a6–0x630f3)
rgbByte = colorVaries ? trunc(t*CE + (1-t)*CS) : trunc(CS)   // per channel, 0x634f6–0x6355f / 0x632b7
```

### Alpha (0x632e1–0x633a3, per particle 0x6356d)
- `gobA = Gob+0x108` (= `GetGobAlpha`, 1 if no gob).
- `tileA = GetTileAlpha` (0x61ce0, inlined at 0x62fec). The tile is `PartEmitter+0x100`, else
  `Gob+0x1a8`. `tileA = tile+0x70` **if emitterWorldZ − tile+0x50 > 5.0**, else 1. This is the
  **roof/tile fade for emitters more than 5 m above the tile base. It is not a camera-distance
  test.**
```
aS = clamp01(alphaStart*tileA*gobA)*255.99,  aE = clamp01(alphaEnd*tileA*gobA)*255.99
alphaVaries = lifeExp>0 && alphaStart != alphaEnd                            // 0x630fa
aF = alphaVaries ? t*aE + (1-t)*aS : aS ;  alphaByte = trunc(aF)
```
- Bench (no gob, no tile): `alpha = lerp(alphaStart, alphaEnd, t)` clamped to 0..1.
- Vertex colour = RGBA8, with the bytes R,G,B,A in memory (0x6362c–0x6366b).
- The fragment shader does `FragmentColor = vColor/255 * texture(texUnit0, uv)`
  (`fsparticle.shd`, ovr version).

---------------------------------------------------------------------------------------------------
## 4. FRAMES (texture sheet) — 0x62ec7–0x62f9a, 0x63c38–0x63d94

Grid setup:
- `xgrid = node+0xac` (0 → 1) and `ygrid = node+0xb0` (0 → 1).
- `hasGrid = xgrid>1 || ygrid>1`.
- Without a grid: UV = full texture (u0=0, u1=1, v0=0, v1=1, initialised at 0x6347e–0x63490), and
  the frame counter is **not** advanced.

With a grid, per particle per *rendered* frame (the advance lives in the draw code):
```
spf = fps > 0 ? 1/fps : 0                                   // 0x62ab4..0x62ac7
if (spf != 0 && frameStart != frameEnd) {
  dt = p.age - p.lastFrameAge                               // p+0x48, initialised 0 (0x66474)
  if (dt >= spf) {
    n = trunc(dt / spf)
    if (frameStart > frameEnd) { p.frame -= n; if (frameEnd > p.frame) p.frame = trunc(frameStart) }
    else                       { p.frame += n; if (p.frame > frameEnd) p.frame = trunc(frameStart) }
    p.lastFrameAge = p.age          // remainder (dt - n*spf) is DROPPED (0x63d09)
  }
}
f   = p.frame                       // int at p+0x44; spawn: trunc(frameStart) (0x664a4), or with flag 0x20 random:
                                    // frameStart + rand() % (frameEnd-frameStart+1) (chkRandom 0x660c0)
row = floor(f / xgrid)              // 0x63d34 (floorf)
col = f - row*xgrid
u0 = col/xgrid,      u1 = u0 + 1/xgrid
v1 = 1 - row/ygrid,  v0 = v1 - 1/ygrid      // v1 = "top" of the cell
```
- Frames advance left→right, then row by row towards decreasing v.
- Frame 0 is the cell at u∈[0,1/x], v∈[1-1/y,1], i.e. the **top-left** cell of the image when
  v=1 is the image top. That is the TGA / GL bottom-up convention; in WebGL use
  `UNPACK_FLIP_Y_WEBGL=true` for upright images (see open points).
- Frames **loop**, wrapping back to frameStart, not modulo. Reverse sequences
  (frameStart > frameEnd) count down.
- `fps 0`, or frameStart == frameEnd → static frame. The random flag only picks the start frame;
  with fps > 0 it then animates from there.
- Frames beyond xgrid·ygrid are not clamped, so V goes negative.

---------------------------------------------------------------------------------------------------
## 5. Quad construction per render mode

### Common emission (0x63c38–0x63fc6)
Each particle produces 6 vertices of 24 bytes (`dword colour, float3 pos, float2 uv`; triangle
list, `RenderVertexStreamData(…,3,6N,0)` at 0x64109):
```
Aw = A*W ,  Bh = B*H
v0 = C  + Aw + Bh  (u1,v1)  colHead      v3 = P2 + Aw - Bh (u1,v0) colTail
v1 = C  - Aw + Bh  (u0,v1)  colHead      v4 = C  - Aw + Bh (u0,v1) colHead
v2 = P2 + Aw - Bh  (u1,v0)  colTail      v5 = P2 - Aw - Bh (u0,v0) colTail
```
- Default: C = P2 = particle position (p+0x10), `W = w`, `H = h`, `colHead = colTail`.
- The front face is CCW with normal **A×B**. +A → u1 (right), +B → v1 (top of the cell).

### Modes 0, 1, 2, 4 (spin path, 0x63aa6–0x63c33)
`θ = p.spin` (p+0x1c, radians; `+= dt*particleRot`, wrapped to ±2π, starts at 0).

For a particle that has not stopped:
- `Qs = quat(axis N, θ)` and `Q = Qs ⊗ Qbb`.
- `A = axisX(Q)`, i.e. `A = cosθ·X + sinθ·Y` in the billboard frame.

Billboard frame per mode (0x62afa–0x62e76):
- **mode 0 `Normal`**: Qbb = camera quaternion, N = camZ. View-plane aligned: every quad is parallel
  to the screen, not pointed at the camera position.
- **mode 1 `Billboard_to_Local_Z`**: Qbb = *current* emitter world quaternion (+0x1d0), N = emitter Z.
  The quad lies flat in the emitter's local XY plane.
- **mode 2 `Billboard_to_World_Z`**: Qbb = identity, N = world Z. A horizontal quad facing up.
- **mode 4 `Aligned_to_World_Z`**: Qbb = camera quaternion, N = camZ.

B per mode:
- Modes 0, 1, 2: `B = N × A` (cross 0x22fba0 = rdx×r8). In closed form `B = -sinθ·X + cosθ·Y`.
  If the emitter has the **splat** flag: `B = p.dir × A` (p+0x4c).
- Mode 4: `B = (0, 0, s)` with `s = camY.z < 0 ? -1 : +1` (0x62d10–0x62d98, 0x63bc3). Width follows
  the camera right axis, rotated by spin; height is world-vertical.

Stopped particle (p+0x70, from bounce/splat):
- `Qs = quat(p.dir, θ)` and `Q = Qs ⊗ p.orient` (p+0x34), then `A = axisX(Q)`.
- B is chosen as above.
- At stop time, `moveParticleStandard` sets `p.orient = RotationArc(z, n)` and `p.dir = n`
  (surface normal; 0x68634–0x68699). For splat this gives a quad lying flat on the surface,
  `A×B = n`, spun by θ about n.

### Mode 5 `Aligned_to_Particle_Dir` (0x639d6–0x63aa1)
- `A = axisX(p.orient)` and `B = axisZ(p.orient)`. B is the **spawn** direction, not the current
  velocity: the orientation is fixed after spawn except for inherit or stop.
- No spin, no camera involvement. W = w, H = h.
- The front normal is −axisY(p.orient), so without `twosidedtex` roughly half the particles of a
  spread emitter are back-face culled.

### Mode 6 `Motion_Blur` (0x6368b–0x6383a)
```
T = p.trail (p+0x64);  C = p.pos (head);  P2 = T (tail);  d = pos - T
u = dot(camX,d), v = dot(camY,d);  r = 1/sqrt(u²+v²)        // fast_rsqrt 0x22fd20 (Quake, 1 Newton step)
u' = u*r, v' = v*r
B = u'·camX + v'·camY                    // streak direction projected to the view plane
A = v'·camX - u'·camY                    // perpendicular in the view plane
W = 2w,  H = h/2
colHead = particle colour; colTail = same RGB, alpha = trunc(aF * min(1, W*r))   // tail fades when streak longer than size
```
- If d is zero or parallel to the view direction, then u = v = 0, so A = B = 0 → a zero-area quad
  (invisible). Example: the first frame after spawn, when the trail point equals the position.
- Trail update, from the earlier notes (FountainEmitter::Update):
  `trail = lerp(trail, pos, min(dt,blurlength)/blurlength)`.

### Mode 3 `Linked` (0x63848–0x639c3)
- Quads drawn = count − 1 (0x629f8). Segment i uses particle i and particle i+1, in array order
  (spawn order, oldest first).
- `P2 = pos_i`, `C = pos_{i+1}`, `d = pos_{i+1} − pos_i`.
- A, B, W and H are computed the same way as Motion_Blur.
- All 4 corners take **particle i's** colour and alpha (no fade), as well as its size and frame.
- Consecutive segments overlap by 2H at the joints. There is no shared-vertex strip.

### `twosidedtex`
`node+0x168 != 0` → `Disable(3)` = glDisable(GL_CULL_FACE). Otherwise culling stays enabled
(0x62efc–0x62f11). The cull state is GL_BACK with CCW front faces: `RenderDrawBucket` ends with
`Enable(3); SetCullFace(1)=GL_BACK` (0xcde11), and `ResetRenderStates` sets `glFrontFace(GL_CCW)`.

Visible sides per mode:
- Modes 0, 3, 6: always facing the viewer.
- Mode 1: from the emitter's +Z side only.
- Mode 2: from above only.
- Mode 4: always facing the viewer.
- Mode 5: from −Y_particle only.
- Splat (stopped): from the normal side only.

---------------------------------------------------------------------------------------------------
## 6. Blending, depth, alpha test, soft particles, order

`SetBlendFunc` table at 0xd8aca0:
- 0 = SRC_ALPHA, 1 = ONE_MINUS_SRC_ALPHA, 2 = ONE, 3 = ZERO
- 4 = DST_COLOR, 5 = SRC_COLOR, 6 = ONE_MINUS_DST_ALPHA
- Enable/Disable cap 0 = GL_BLEND; cap 1 = emulated alpha test; cap 3 = GL_CULL_FACE.

| blend (+0x68) | GL state (0x62f11–0x62f56) | depth write | discard |
|---|---|---|---|
| 0 Normal | blend ON, `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`; alpha-test OFF | **off** | none (shader compiled `NO_DISCARD 1`) |
| 2 Lighten / other | blend ON, `SRC_ALPHA, ONE` (additive weighted by alpha); alpha-test OFF | **off** | none |
| 1 Punch-Through | blend **OFF**, alpha-test ON | **on** | `discard if (vColor.a·tex.a) <= 0.2` |

Evidence for the discard threshold:
- `SetUniforms` 0xb9de2–0xb9e07: `fAlphaDiscardValue = (state bit1) ? 0.2 : -1`.
- `AlphaDiscard` in inc_common.
- The program variant for this case is built with `NO_DISCARD 0` (the bool passed to
  `GetCustomShaderProgram` at 0x6404b/0x6407d → `#define NO_DISCARD %d`, preamble at 0xb4428).

Other state:
- The depth test is not touched here (scene default: on).
- The particle layer enables fog (state 2, `StartEmitterLayer` 0x64720).

Shader `blendMode` uniform (0xb9e1b–0xb9e52):
- 1 (additive) if blend is on and the func is (0, 2); 0 if blend is on with any other func; 2 if
  blend is off.
- Additive: `a *= 1 - fogAlpha`. Otherwise RGB is mixed toward the fog colour.

**Soft particles.** Only at `SHADER_QUALITY_MODE>0` and not on mobile. The value is set with
`SetShaderParamf(0, k)` at 0x640a2–0x640db → `particleSoftness` (0xba27c–0xba28f).
```
maxHalf = max(hs0, hs1 (if sizeVaries||splat), hy0, hy1 (if sizeYVaries||splat))
k = (!splat && mode != 1 && mode != 2 && maxHalf > 0) ? 1 / min(0.2, 0.2*maxHalf) : 0
frag.a *= (clamp((sceneLinearDepth - fragLinearDepth) * k, 0, 1))^2
```
This fades the particle within `min(0.2 m, 0.2·maxHalfSize)` of the geometry behind it. The 0.2
here is `SOFT_PARTICLES_RANGE`; it is a different 0.2 from the punch-through cutoff.

**Order.**
- Within an emitter: array order, oldest first; no per-particle sort.
- Between emitters (`Scene::DoEmitterBucket` 0xc7ae0): all emitters draw after the scene and are
  `std::sort`ed with comparator 0xc91f0.
  - Key: `PartEmitter+0x240` = squared distance from the camera position (cam+0xb0) to the Gob
    position (or the emitter position), ascending, so **nearest first**.
  - Ties with the same Gob are ordered by `renderorder` ascending.
  - Remaining ties are unspecified (introsort is not stable).

---------------------------------------------------------------------------------------------------
## 7. Other things that change what is on screen

- **Early-outs.** The emitter is not drawn at all when any of these hold:
  - the tile object (`Gob+0x1a8` or `PartEmitter+0x100`) has float `+0xc8 < 1.0`
    (0x62930–0x6295e);
  - particle count ≤ 0, or ≤ 1 for Linked (0x62a3e);
  - lifeExp == 0 (0x62a47).
- `g_nRenderEmitterSpheres` → debug bounding sphere only (0x62a5b).
- The **5.0** constant is the tile/roof fade height (section 3), not a camera distance.
- The **0.01** constant is the invLife fallback, effectively unused (section 2).
- **threshold** (PartEmitter+0xd8) is never read by rendering; only `moveParticleP2PGrav` reads it
  (0x678e1).
- **deadspace** affects spawn direction only (`randomDirection`).
- The gob scale multiplies sizes only. Spawn area was already scaled at spawn.
- Frame animation advances only while the emitter is being drawn, because the advance code is in
  the draw path.
- Colours and alpha are 8-bit-quantised (truncated) per vertex. Within a quad only Motion_Blur
  varies (head→tail alpha).
- Particle colours do not use scene lighting unless `m_isTinted` is set. The fragment shader has
  `LIGHTING 0`.
- `Material::CheckAndBindCustomShader` (0x640f3): a texture material (.mtr) with a custom shader
  could override `vsparticle`/`fsparticle` (rare).

---------------------------------------------------------------------------------------------------
## Open points (not settled by this function)

1. **Meaning of `tile+0xc8 < 1` (skip emitter).** Settle by finding the writer of `Gob+0x1a8` and
   `PartEmitter+0x100` and the class layout (tile / area-tile object). It is irrelevant for a
   bench with no tile.
2. **Texture V origin.** The UV math puts frame 0 at v≈1. Whether v=1 is the top of the image as
   authored depends on the texture upload path. TGA is stored bottom-up; Bioware DDS is believed
   to be stored flipped to match. Settle by disassembling `Material::LoadTextures` 0x84410 → the
   texture loader / glTexImage2D path, or empirically with a numbered sprite sheet.
3. **Texture wrap/filter** for emitter textures is not set in RenderEmitter.
4. PartEmitter+0x1a8 is uninitialised when the `render` string matches nothing.
