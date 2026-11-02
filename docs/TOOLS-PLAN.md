# nwn5e tools plan

The tools stream's design and decision record: the VFX Bench, the Animation Recipe Bench's Mocap
mode, the bench redesign (docs/BENCH-REDESIGN.md, its research in docs/research) and the dev layer
(the nwn5e-tools repo beside this one). It was split out of docs/PLAN.md, the game's record, on
2026-10-30 (owner: "go with a, write the rules into CLAUDE.md" - CLAUDE.md, "Two sessions in one
checkout"); the sections below were moved from it verbatim. Decisions are given info-first and
quoted here, as in PLAN.md.

## VFX Bench — 0.42.0 to 0.44.0 (2026-10-26; moved from PLAN.md's Phase 5 log)

  0.42.0 (owner, 2026-10-26: "can you come up with a webapp to edit and
  create new vfx? Kind of like the one for animations"): the VFX Bench, a
  local web app in `tools/vfx_bench/` (`vfx_bench.bat`: Node on
  127.0.0.1:8766, no dependencies). nwn5e (`vfx_bench`) copies the game's
  effects out for it once, 3 s into a start (`!vfxexport` again):
  `build\vfxedit\library\` - index.txt (421 rows with emitters), 394 models
  as drafts (vfx_model.cpp's binary reader, the same as !vfxedit's Load), the
  64 particle textures (TGA, the player's own files; fxpa_cloud02 not found)
  and nwn5e's 14 effects; 2.7 s, once. The page: the effects to open (yours,
  nwn5e's as a copy, the game's as fx<row>), a WebGL stage (an approximation of
  the emitters: Fountain / Single / Explosion, the render and blend modes,
  cone spread, gravity, spawn area, life, size / colour / opacity over life,
  texture sheets, bounce, splat, inherit with a walking figure; Lightning not
  drawn), every emitter field with sliders, colour pickers and a texture
  picker with thumbnails, the draft text. Save writes the draft into
  `vfxedit\saved\` (the game writes its model there - WriteVfxModel stays the
  one writer; !vfxedit's Load lists it). Play in game / Stop in game: the
  page writes `vfxedit\bench\request.vfx`, the game (the player's update, 4 a
  second) plays it on the character as Preview on me does and answers in
  `bench\status.txt`; `bench\alive.txt` (every 2 s) says the game is in play.
  `public/core.js` ports WriteVfxDraft / ReadVfxDraft; `check.js` reads and
  writes back all 408 exported drafts (1702 emitters) byte for byte. Found on
  the way: the binary reader took every lasting game effect's cessation (its
  emitters keyed off as it ends) for "on for 0.03 s" - the elemental shield
  died after 2.6 s, in !vfxedit's Load too. Now an animation's copies of the
  nodes belong to the header before them (vdr_eleshield: impact 0x100,
  duration 0xa6c, cessation 0x1344; Ring01's copies 0x55c / 0xec0 / 0x1798),
  the cessation's and the duration's switch-offs are ignored, and a lasting
  effect's impact switch-off counts only for an emitter otherwise off
  (VfxLastingEffectKeepsEmitting, both guards mutation-checked); the library
  format is 2 so an old export is redone. Not in the effect model at all (the
  in-game editor's too): an emitter node's own motion (the shield's circling
  rings stand still). Checked: page controls driven in headless Edge, the
  server's guards (Origin, Host), screenshots of game effects. Not yet: Play
  in game with a character in play.
  0.42.1 (owner: "the effect in the preview is not at the correct height.
  set to foot the effect appears at the character's belly, chest, appears
  above the head and head appears way above the head"; "can we get a dropdown
  list of available effects?"): the preview's nodes are the game's, measured on
  the human male (pmh0, ASCII): root 0, Impact 1.474, headconjure 2.398 (the
  visualeffects head column - 0.4 m over the head; the game's head effects put
  their emitters at 0 to -0.11, their impact ones at 0); the figure is pmh0's
  joints (2 m). It looked a metre off because a new emitter started at 0 0 1
  (!vfxedit's default too): now 0 0 0, on the point; VfxEmitter's default
  position is 0 (a model's emitter without one sits on its node). And the
  binary reader took the first copy of an emitter node in the file - an
  animation's, which carries the same emitter header but only what that
  animation keys (vcm_hitsonc's cloud came out 1 m up with default values,
  vdr_darkness all defaults; 46 game emitters read 0 0 1): emitters now come
  from the geometry's node tree and each animation's copies from its own tree
  (headers +0x48; children at node +0x38 / +0x3C; the node header is 0x70 with
  the name at +0x20 - the reader's "node" is +0x10 into it), replacing
  0.42.0's "the header before it" guess. Test VfxLastingEffectKeepsEmitting
  rebuilt with real trees and the copies first in the file; three mutations
  (tree, cessation, geometry rate) each fail it. Library format 3. The list:
  the page's Effects card (Yours / nwn5e / Game, with search) was empty
  because the page opened before the game's export; it now reads the lists
  again when the export appears (the 2 s link poll carries index.txt's time).
  0.42.2 (owner: "the interpretation of the effects in the bench and the ones
  that play in-game are different" - "I think reading the game's code would be
  the most straightforward way"): the game's particle code read (its exported
  classes: FountainEmitter / ExplosionEmitter / SingleEmitter::Update,
  PartEmitter::chkParticleLife / moveParticleStandard / RenderEmitter,
  Particle::initialize / randomDirection / randomPosition, the emitter field
  parser) and written up in tools/vfx_bench/PARTICLES.md; the preview now runs
  and draws by it. What was off: births (time adds up to 1 / birthrate, then
  int(time x birthrate) born and the rest DROPPED - rate 100 at 60 fps makes
  60 a second; below 1 none); spread (an even angle up to half of it, 0.01
  steps); speed (velocity +- randvel, not + 0..randvel); motion blur (the tail
  is a trail point lagging ~blurlength SECONDS, not a length - the heal's
  streaks were metres long); render modes (Billboard_to_World_Z lies flat
  facing up, Aligned_to_World_Z stands upright - the preview had them the other
  way; Aligned_to_Particle_Dir lies in the particle's spawn frame; back faces
  are culled; Motion_Blur / Linked are 2 x size wide); sizes / colours / alpha
  start -> end only (alphaMid, colorMid, sizeMid, percent* are never read);
  frames step at fps while drawn and loop - nwn5e wrote fps 0, so in game they
  never moved while the preview played them over the life; Punch-Through cuts
  at 0.2; no sorting; names matched exactly ("Aligned_to_World_z", in
  !vfxedit's list, drew nothing defined - fixed). The format gains an emitter's
  `orientation` (axis, angle - particles leave along its z, the area lies in its
  x / y), `fps` and `particle_rot` (spin): the game's own effects use them (fps
  on ~30% of sheet emitters, spin on ~25%); the binary reader reads them
  (controllers 128 / 160 = field offset - 28) and places each emitter in the
  model with its parents (Premonition's eyes at 1.3 / 2.5 m, not the feet).
  Library format 5. Tests: VfxEmitterUnderAMovedDummy (mutation-checked),
  the round trip with the new keys; check.js: 409 drafts byte for byte; node
  checks of the birth / burst / splat / frame rules. Not here: node motion
  (circling rings), Lightning, soft particles, fog.
  0.42.3 (owner: "how can we fix the gaps?" - the gap scan: of 255 compiled
  game effect models, 119 move or turn nodes, 114 inherit_local, 63 deadspace,
  35 twosidedtex, 27 chunky particles, 26 p2p, 5 Lightning - "Go for A in
  whichever order you think is best": keep the game's motion): the effect gains
  nodes (VfxNode: the dummies - or meshes, lights - its emitters hang under,
  with parent, position, orientation), a parent for each emitter (its position /
  turn now under it, not baked into one place as 0.42.2 did), keys per node and
  emitter per animation (position t x y z, orientation t + axis-angle; the
  binary quaternions converted with angle 2 acos w, which comes back to the same
  quaternion with its sign), the three animations' lengths, and the
  inherit_local / inherit_part flags. Draft: [node <name>] sections, parent =,
  <anim>_position / <anim>_orientation, lengths =. Model writer: the nodes as
  dummies, parents first (a loop or a missing parent: under the root), every
  node renamed per model, counted positionkey / orientationkey lists, the
  lengths (at least the keys' last time). Binary reader: the geometry tree with
  parents (the node header is 0x70 bytes, the name at +0x20 - the reader's
  "node" is +0x10 into it; children at +0x38 / +0x3C), the emitters' ancestors
  kept, each animation's tree for keys (no bezier or compressed keys in the
  game's effects - scanned). ASCII reader (125 of the game's effects are ASCII):
  every node kind, keys counted or to "endlist" (the game's own ASCII), the
  lengths. The game's slerp (0x231cd0) turns the first quaternion round when
  they're more than half apart - the bench does too. Bench: the timeline (impact,
  duration looping until removed, cessation), each node's and emitter's place
  composed down to the character's node and facing, spawn from the emitter's
  place now, the three follow modes as moveParticleStandard has them; the Nodes
  card, Hangs under, Make it orbit / spin, the lengths. Library format 6: 99 of
  the game's effects keep nodes, 127 have keys; check.js 409 drafts byte for
  byte. Tests: VfxEmitterUnderAMovedDummy (keys, local place, length),
  VfxMotionThroughDraftAndModel, VfxReadsTheGamesAsciiKeys; mutations of the
  ancestor keeping, the parent renaming and the binary keys each fail them (the
  explicit "endlist" case is covered by the list's own end - an equivalent
  mutant). Found: Mage Armor's shields are meshes (trimesh) - the effect has no
  meshes; counted next.
  0.42.4 (the next gaps, in the order delegated): twosidedtex and deadspace
  kept - the binary reader (emitter header +0xC8 / +0, node +0x168 / +0xa0 in
  the game), the ASCII reader (deadspace read %f: radians), the draft
  ("deadspace =", the "twosidedtex" flag), the model writer. twosidedtex: the
  bench draws both sides (the game turns GL_CULL_FACE off). deadspace, read in
  Particle::randomDirection (0x68a25-0x68d0f; PARTICLES.md): the particle's y
  within D of square to the camera line -> turned round cross(V, y), by
  cos - pi/2 + D (cos + pi/2 + D below 0), again from the start by |cos| - pi/2
  + D while still within; no cap in the game - simulated: one pass for D up to
  0.39, from 0.40 some never end (a hang), so drafts and written models hold
  it at 0.39 (kVfxDeadspaceMax; the game's own: 0.0037 / 0.1745 / 0.2094). The
  bench feeds the sim its camera position. Library format 7 (37 drafts
  two-sided, 98 emitters with deadspace; check.js 408 byte for byte). Tests:
  the draft (and its 0.39 hold), the writer (and its), the ASCII and binary
  readers; eight mutations each fail one. Meshes counted: 25 spell effects
  (22 compiled, 3 ASCII - Bigby's hands 11 each, Mage Armor 8 / 10, Weird 43,
  Wail of the Banshee 15, Meteor Swarm 12, Wilting 28, Revive Dragon 64, Raise Dead 17, ...)
  and 13 tile / placeable models listed as effects; not drawn (owner's call).
  0.42.5: point to point, read in the game's code (PARTICLES.md): the target is
  the emitter's first reference child (SetEmitterTarget via "_EmitterTarget";
  all 117 compiled p2p emitters have one, refModel fx_ref; none -> a null
  call, a crash); p2p_sel picks moveParticleP2PBezier (the free flight blended
  into the cubic emitter -> emitter + p2p_bezier2 z -> target + p2p_bezier3 z
  -> target by (t^3)^combinetime) or moveParticleP2PGrav (pulled at grav,
  speed x drag^dt, aged out within threshold - with the game's early return
  when one starts the frame inside it; inherit_local / _part there also turn
  with the character). Kept: VfxEmitter p2p / p2p_bezier2 / 3 / combinetime /
  grav / drag / threshold, a node under an emitter is its target (VfxNode
  ref_model) - binary (flags, controllers 120 124 140 152 156 188, the first
  header-0x10 child), ASCII (p2p / p2p_sel, the first `node reference` child),
  draft (p2p =, p2p_bezier = b2 b3, combinetime, grav, drag, threshold;
  ref_model =), writer (the target a `node reference` after its emitter with
  refModel / reattachable 0, `node dummy` in the animations as the game's own;
  p2p written off without a target). Bench: both moves ported, the target's
  place each frame, "Goes to a target" (makes <emitter>_target), the kind's
  own fields, a renamed emitter keeps its target. Library format 8: 119 p2p
  emitters (41 bezier, 78 gravity) with their targets; check.js 408 byte for
  byte. Test VfxPointToPoint; nine mutations each fail it.
  0.42.6: chunk models kept - VfxEmitter chunk (binary emitter +0xB8, ASCII
  chunkName, lower-cased: a resref; the game's own write them in any case,
  which check.js caught), the draft's "chunk =", the writer's chunkName (only
  when set: the game reads any value as a model name - no line otherwise, and
  no blank line in its place). Read in the game: a chunk name makes the
  emitter RenderNothing; each particle a ChunkyParticle with its own model
  (NewCAurObject) playing "impact". Bench: the field, grey squares for the
  chunks, a note. 95 chunk emitters in the library (format 9); check.js 408
  byte for byte; six mutations each fail a test.
  0.42.7: Lightning, read in the game (PARTICLES.md): LightningEmitter keeps
  int(birthrate) points (a free list keeps the spare ones, ages and all; life
  fixed at 1), a new bolt each lightningDelay - the ends at the emitter (or
  its area) and the first reference child, RecursiveFractal's midpoints pushed
  up to lightningRadius along two sideways axes, the inner points jittered
  along by lightningScale x half a step - and between bolts turned with the
  emitter -> target line (RotationArc). Kept: lightning_delay / radius / scale
  (binary controllers 208 212 216, ASCII lightningDelay..., the draft's
  "lightning = d r s" for a Lightning emitter, the writer), and a Lightning
  emitter's target (both readers, as for p2p). Bench: the update ported (the
  game's past-the-end read on an even count clamped), the Lightning fields,
  choosing Lightning makes a target, a note when it has none; Lightning is
  drawn now (Doom's and Death Armor's bolts). Library format 10: 23 Lightning
  emitters; check.js 408 byte for byte; test VfxLightning, six mutations.
  The gap list from "how can we fix the gaps?" is done but for the meshes
  (25 spell effects; not started - the owner's call) and the soft-particle
  fade / fog.
  0.43.0 (owner: "build a system that allows to mix and match different
  presets of the various parts of a vfx to create a new one"): the VFX Bench's
  Build from parts - public/parts.js, three kinds of part: 20 particles (the
  look's fields: texture, render, blend, sheet, sizes, colours, alpha, spin),
  15 emissions (the emission's fields: update, rate, life, speed, spread,
  gravity, area, bounce, follow flags, p2p, Lightning; a target where it needs
  one) and 13 motions (nodes and keys in the loop: still / ring, orbit,
  spirals up down in out, rise, fall, bob, sweep, figure eight, sprinkler,
  wander; copies spaced round). A part's fields are reset to the defaults
  before it's put on, so nothing of the one before stays. The card tries the
  mix in the preview only (a layer appended to what the preview runs, never
  to the effect / draft / Save), adds it as emitters + nodes, or puts the
  ticked parts on the selected emitter; your own particles and emissions are
  kept from an emitter (server: /api/parts, build\vfxedit\parts\, checked by
  cleanPart - only the part's fields, of the emitter's types). Values from
  how the game's effects use each texture; Shadow wisp first used
  fxpa_smoke01a with Normal blend - squares (it has no alpha): now smoke01,
  and a rule (check_parts.js) that a covering look's texture has transparency
  (fx_bubble neither: Lighten). check_parts.js: 7,800 mixes sound and kept by
  the draft, 20 run, the motions measured (orbit radius / turn, spiral
  heights, spiral in, sprinkler jets level, bob, ring), settings, a motion
  on an emitter, your parts. No DLL change (the version marks the bench).
  0.43.1 (owner: "can you come up with a few more movement types?"): 11 more
  motions - Tilted orbit (a still node tilting the ring, each ring turned
  180/n round z, a spinner under it: rings 3 = an atom), Tornado (spinner
  rising, the emitters' radius keyed bottom -> top), Pendulum (a swing node
  turning round y under a pivot), Wheel (a node out along x spinning round
  x), Corkscrew (out along x while spinning round x), Boomerang (out and
  back on a curve, copies thrown all round), Breathe (a ring's radius keyed
  in and out, optionally turning), Bounce (|sin| hops, copies taking
  turns), Jitter (seeded random points), Flower (r = R(1 - depth(1 - cos
  petals th)/2)) and Blink (held at seeded spots, a 0.02 s jump between).
  Whole cycles in the loop; "Turned round" aims the one-way ones. Helpers:
  axisKeys (turn keys round any axis), rng, yaw. check_parts.js: 14,400 mixes,
  and each new motion measured (ring distance and tilt height, tornado
  radius by height, pendulum length and swing, wheel and corkscrew radius,
  boomerang reach and curve, breathe in / out, bounce heights, jitter bound,
  petal radii, blink held then jumped).
  0.43.2 (owner: "is there a way to control 3d meshes' rotation? ... rock
  chunks ... no rotation"): read in the game - the chunk emitters' draw
  (RenderNothing 0x64190) sets each chunk model's position, and its turn only
  when particleRot != 0 and it isn't at rest: p.spin (starts 0, += dt
  particleRot) about the particle's direction (+0x4c, the way it was thrown).
  So Spin is the control; 0 (as the Rock chunks preset had) never turns them.
  Rock chunks now spin 6 rad/s; every particle part has a Spin setting; the
  Spin field says what it does to chunks; the bench draws chunks as small
  cubes turned by the same rule (a rested one keeps its last turn) instead of
  flat squares. check_parts.js: the Spin setting, the rocks spin.
  0.43.3 (owner: "Can you have a look at the latest crash report? I think I
  overdid it with the particle effects"): the crash (23:41, 1 s after a Play
  in game) was not the particles - the minidump: an access violation reading
  0x70 at MdlNode::InternalParseField+0x528, under the ASCII model loader
  (ParseNode <- Model::InternalParseField <- ImportMaxTree). The game keeps 31
  characters of a node's name (MdlNode ctor: strncpy 31 into a 32-byte field)
  and finds a "parent" by the whole name; the model's tilted-orbit nodes were
  named n5vx0022_n2_flare_gather_ringspi (32 - 33 with two digits), so the
  emitters' parent wasn't found and the null was read. The writer now names
  every node and emitter <model>_<tag>_<name> in at most 31 characters (the
  readable part gives way; the tag keeps it unique) - a saved effect with a
  16-character name was 9 characters closer to it. Test VfxModelNodeNamesFit
  (a 16-character model, 27 nodes, 24 emitters with long names: every name
  fits, is unique, every parent line - geometry and animations - names a node
  defined before it); the old naming and a 32 limit each fail it. The effect
  had ~1,300 particles alive at once - the game's own reach ~1,670 (Hellball),
  ~1,600 (blood bursts), ~1,440 (Death Armor): within what the game does.
  0.44.0 (owner: "there are also some effects that create some rumbling
  movement of the camera, I think?" - then "add them both": the shake and a
  guide for how many particles): read in the game - the camera shake is the
  effects table row's ShakeType / ShakeDelay / ShakeDuration (0x872770; 1 a
  bump 0x7cbfa0, 2 a shake 0x7cfd00; the camera update 0x7d0470 lifts it:
  a bump 25 cm and back in 0.1 s, a shake 10 cm up and down every 0.14 s for
  its duration; up and down only). The bench's Effect card has Camera shake /
  After / For; the preview's camera moves so; the draft keeps it (shake =
  T D S, both writers and readers, test VfxDraftRoundTrip); Play in game puts
  it on the preview row; the library (format 11) carries each row's shake, so
  the game's 35 shaking effects are marked and open with theirs. And a
  "Particles alive at once" line: about how many at the fullest (the mix
  being tried included), against the game's own (half under 25, 9 in 10
  under 250, Hellball ~1,700): heavy over 250, over 1,700 more than any. A
  little over the preview's peak - at most. check_effect.js: the bump and
  shake curves, the draft line, the count for every kind of emitter.

## Mocap mode — decided by the owner (2026-10-28)

Owner, 2026-10-27/28: "I'm going to attempt to mocap some animations" - FreeMoCap
("what does that export?") - "what can we add that will help turn captured
performances into usable animations in nwn?" - "First thing to capture would be
either a shove, a rage or a crouch stance" - "Sure" (to the plan below; built
after the Rogue).

*FreeMoCap's output:* a processed recording's output_data folder holds 3D
positions of body points per frame (MediaPipe's 33 body landmarks; hands and
face apart) as .npy and .csv (mediapipe_body_3d_xyz); its Blender add-on can
build a T-posed armature and export BVH / FBX. nwn5e reads the CSV directly -
no Blender: NWN's skeleton is coarse, so each NWN bone is aimed by the points at
its two ends (direction retargeting: no rest-pose matching), twist from the hip
and shoulder lines and the hand points.

*In game:* the clips go into nwn5e's animation packs (anim_packs.cpp: built at
each start from the player's own files, per body library - a_ba, a_fa, a_da,
a_dfa and the large builds), retargeted to each body type (rotations only, so
one capture fits all). Named slots the game uses when the clip exists, else
today's animation: `n5_rage` (played as Rage starts, in place of victorymg),
`n5_cridle` (stealth's crouched idle - saving under its name already wins over
the recipe), `n5_shove` (the Shove's swing, in place of nwstab - anim_layers'
swap). The clips are the owner's own data, not game files: they may go in the
public repo.

*Mocap mode v1 (the Animation Recipe Bench):* drop the CSV - up axis, facing,
scale (mm) and frame rate found; clean-up - gap fill, smoothing (a slider,
before / after), bone lengths held to the performer's median, feet to the floor,
drift removed; retarget to the library's bones, in place (travel stripped, the
hip bob kept); trim, blend in / out to the stance, loop seam (pose-matched
crossfade), the contact ("hit") marker (suggested at the hands' peak speed) and
fit-to-length (the swing it replaces); a 3D preview of the raw points over the
NWN skeleton, per body type; save as a keyframe recipe - a new recipe kind
carrying its own keys, read by the C++ pack builder and core.js alike. Later:
foot pinning and stride matching (a crouch walk), mirror, several takes per
recording, the weapon hand kept from an NWN attack (the recipes' kept part).
Order: Rage first (a one-shot flourish: proves the path end to end), the crouch
stance (a loop), the Shove (timed).

*#74 (2026-10-29; owner, with a first FreeMoCap test take: "can the animation tool do anything with
these?", then "go, it's a test take, I want to see what can be done with it, we also probably need to trim
it and clean it up"):* FreeMoCap had moved on: the take's output is RTMPose's - `rtmpose_body_3d_xyz.csv`,
a row a point a frame (COCO-WholeBody's 27 body points: the big toes stand for MediaPipe's foot_index),
hands in files of their own, and the frame times in `synchronized_videos/timestamps/<take>_timestamps.csv`
(no recording_parameters.json). The bench now reads it (MediaPipe's still works): the folder dropped or
picked, the files sorted by name; the times resampled to an even 30 a second (that take: 336 frames at
12.9 a second, 11-16, unevenly). Clean-up gained a glitch filter - a point whose bone from its parent
suddenly stretches or squashes by 30% (and 6 cm) against the frames around it is dropped and filled (the
take: an ankle a metre off for a frame at 9.1 s, its shin at 1.6x; 40 points in all); a neighbour test only
past a metre, because at 13 a second a stab's peak sits 40 cm off its neighbours and a nearer limit cut it.
Hand files whose points don't sit on the wrist (the take: 71-74 cm off - the hands too small in two 1080p
webcams to place) are left out, and that hand follows its forearm. A new take gets a suggested standing
window (the longest still stretch early on - the take: 0.4-1.9 s, arms relaxed, no T-pose needed) and trim
(from just before the first move to just after the last: 4.1-22.0 s; the take ends with a step away at
~21.5 s and 4 s standing), and a motion graph (hands, feet; the standing window, the part kept, the playing
time). Measured (mocap_check.js): resampled from 13 a second a stab's peak lands 29 cm short at the hand,
from 25 15 cm, from 60 6 cm - slow moves are fine at webcam rates, fast ones want a faster camera. A bench
change only: no DLL version.
*#77 (2026-10-29; owner: "How should I clean this? I have blender installed can you guide me through
this?" - Mocap Pro / Auto-Rig Pro weighed and set aside: capture and rigging tools, no route back into the
bench; then "go with the points round trip"):* the take goes to Blender as points and comes back. The
bench's **Export points for Blender** writes the prepared take - game space, metres, an even 30 a second,
the whole take (the standing window must come back) - as a CSV: `time_s`, then `body_<point>_x/y/z`
(MediaPipe's names). tools/mocap_blender/nwn5e_mocap_points.py (Blender 5.2's slotted-action API:
`action.fcurve_ensure_for_datablock`) imports it as an empty per point keyed every frame plus a stick figure
hooked to them, the performer facing Blender's front (game space turned half a turn about Z), and exports
it back (all frames, or the scene's range). Cleaning is Blender's own Graph Editor tools (Key › Smooth ›
Butterworth, Key › Blend › Blend to Neighbor / Scale Average at 0 for a planted foot); the guide is
tools/mocap_blender/README.md. The bench reads a points file's times, keeps already-even frames as they are,
and starts with its glitch filter off for it - run on an edited take it "fixed" the heel left behind by an
ankle lifted on purpose. Checks: tools/mocap_blender/check.py in Blender without a window (the owner's take:
33 points, every frame keyed, the figure on its empties, an edited key back exactly, the rest unchanged);
mocap_check.js's points round trip (frames back within 0.01 mm, the retarget within 0.04 mm).
*#77 fix (2026-10-29; owner: "the export for blender option on the bench seems like it exports the whole
animation, not just the selected part"; offered (a) as is, (b) the standing part to the kept part's end,
marked, (c) the kept part only with the standing pose as a hidden row - "go for b"):* replaces "the whole
take" above. The export runs from the standing window's first frame to the kept part's last (pointsSpan:
referenceSpan / keepSpan, the frame maths prepare, setReference and the retarget now share), with columns
`standing` / `keep` (1 on their frames). The add-on stores the standing part, plays the kept part (the
scene's range), puts markers "standing" / "standing end" on the timeline, and writes the standing part back
as it came and the kept part as the scene's range (narrowing it trims); its "Only the scene's frame range"
tick is gone. The bench sets its standing and keep sliders from a points file's marks (trimFromMarks) instead
of suggesting them. The owner's take: 648 of 780 frames written (0.40-21.97 s). Checks: check.py (the
marks in, the scene on the kept part, every value and mark back, a narrowed range comes back as the kept
part); mocap_check.js (both ends cut, the same standing and kept frames back, retarget within 0.05 mm); the
page in a headless browser (keep start moved to 5.00 s, exported, loaded back as 4.60 s). 5 deliberate
breaks caught.
*#78 (2026-10-29; owner: "something to help with the mocap bench, make the preview scroll along with the page so its
always visible"):* on the two-column layout the preview column (stage, transport, note) is sticky 16 px from the
top, its stage no taller than the window less 150 px, so the figures and Play stay in view down to Save for every
body, in both tabs (they share the stage). One column (820 px or less) is unchanged: stuck there it would cover
the controls. Checked in a headless browser at 1400x1000, 1400x700 and 1100x600 (in view scrolled to the bottom)
and 800x900 (unchanged).
*#79 (2026-10-29; owner: "wouldn't it be easier to edit the mocap animation if it were attached to an IK rig, so that
I could just make sure a foot was well planted and the rest of the body would follow naturally that change?"):* measured
first - the take's planted heels drift 4 cm at most in the points, yet 8-36 cm in game on 4 of 11 stretches, on every
body: the retarget's doing (the hips hung over the feet's midpoint drag a planted foot half a step; NWN's longer, other-
shaped legs at the performer's angles). Offered foot locks in the bench / an IK rig in Blender / both; the owner: "can I
tell the bench when the animation has planted feet?", then "go ahead with the foot locks and for the animation, sure
there would be two types, animate in place (for walk cycles) and return to original place for the others" (Mixamo /
CMU import: "nah ... this is mostly for my own mocap data" - dropped). Built (tools only): suggestPlants (heel under
0.3 m/s and within 6 cm of its low, 0.4 s or more, gaps under 0.15 s bridged, split where the heel wanders 5 cm,
0.2 s apart) and rows L / R under the motion graph (drag to add, drag an end, click to remove; Suggest feet; Lock
planted feet); in the retarget, legIK - thigh and shin turned in the knee's plane so the ankle stays on the spot where
it landed (in 0.1 s before, out over 0.2 s after), the foot keeping its turn - with a locked leg and the root reduced
at a third of the key tolerance (their slack added up to a centimetre at the foot); placeGround: "return" (default;
one-offs) - the body moves with its steps from the first kept frame's feet and the blend-out brings it back -
and "inplace" (walk cycles, loops; a loop slot's default) - the hips' net travel between the part's ends taken out
at an even speed, through the feet's average, the ground and its locked feet sliding back under the body. Marks
left_planted / right_planted through the points CSV and the Blender add-on; a ring under a locked foot; locks out of
the leg's reach red, listed with the shortfall. Checks (mocap_check.js): getlow with 1.25x / 0.75x legs slides 21 cm
free, exact locked (0.45 cm with the keys reduced); walk + 1.2 m/s in place sways within 1.8 cm of NWN's walk
(returning, it travels 118 cm); taunt returning travels as the animation did and ends on the stance's spot; the
owner's take - 108 of 136 locks within reach on the 8 bodies hold to 0.50 cm (29.8 cm unlocked). 7 deliberate breaks
caught; check.py passes the planted feet through; the page in a headless browser (rows drawn, a drag adds, a click
removes, export and reload keep them, the ring).
*#79 timeline (2026-10-29; owner: "timeline should be placed under the mocap preview, selecting a foot planting section
should allow the extension or shortening of the plant, not purely deletion. the extension or shortening, while dragging
the edges of the bar should also snap to the now marker. And there should be a way to move frame by frame the marker
for more finegrain control"):* the motion graph and its rows moved under the preview (in the sticky preview column,
three times wider; the stage gives it room in Mocap). A click on the curves moves the marker (drag to scrub); ◀ ▶ by
Play and the arrow keys step a frame, paused. On the rows a click selects a plant (outlined, a handle at each end; its
times in a readout beside the marker's take time and frame) instead of removing it; its ends drag, snapping to the
marker within 8 px (the marker thickens) and else to whole frames; Delete / Remove plant takes it out. The retarget
gained clipTime (a take time in the result). Checked in a headless browser (layout, frame steps by button and key,
seek, add, select, an end snapped onto the marker, lengthen, Delete, the Recipe tab hides it).
*Accepted TL1-TL8 (owner, 2026-10-29: "TL1 to TL8 pass").*
*#79 knees (2026-10-29; owner: "how do I prevent knees from bending backwards on the target doll?"):* measured first - the
take's knee points sit behind the hip-ankle line by over 1 cm in 36-43% of frames (up to 10 cm: webcams on a straight
leg), the doll copied it, and the locks made the worst cases worse (bending the way the knee leaned): 16.3 cm at worst on
the 8 bodies. Now: straightenKnees puts a knee behind the line onto it (a straight leg, never a backward one) before the
retarget aims the leg, and legIK bends a locked knee forward. "Forward" is kneeWay: the hips' forward, turned halfway to
the foot's when the foot is flat and faces within 100 degrees of the hips (by the foot alone, nwstab's pointed back foot -
its heel-to-toe facing backwards - straightened a knee bent 25 cm forward). The take: 1.3 cm at worst between the blends,
free and locked, on the 8 bodies (a straight leg - under 2 degrees; the check allows 2 cm, about 2.5 degrees); a blend into
the stance can lean a knee back a centimetre for a frame. Checks: pause1 with its knees moved 10 cm back (the old code:
3.3 cm behind; now straight), pause1 with a foot turned round (the knee keeps its bend), the take; 3 deliberate breaks
caught.
*#80 (2026-10-29; owner: "is there a way for me to adjust the lean of the captured data? ... the latest data makes it look
like I'm leaning forward the whole time", then "shouldn't the "standing still" section work this out? The verticality of
the capture?"):* measured - "up" was right (the floor under the feet, within 1.4-3.5 degrees of the body line, the floor
level); the lean was posture: the standing legs 2.5 degrees forward, the torso 6.5 back, the head 18 ahead (the ear
points). The bench took the hips, torso and head as change from the standing pose onto pause1, but copied the legs by
direction: the doll stood 1.5 degrees further forward than pause1. Offered (a) a lean slider, (b) the standing part
setting the legs' neutral too, (c) both; the owner: "go with b". Then: "Can that pose be configurable? such as I want an
animation that follows from an unnarmed combat stance for example. So I pick the unnarmed neutral combat stance and then
whatever changes modify that and return to it at the end" and "I'll attempt to get the body as close to the selected
neutral as possible so that not a lot of effort has to be expended correcting it". Built (tools only): legNeutral - a turn
per thigh, shin and foot taking the standing part's direction onto the stance's, applied in the pelvis's frame (it turns
with the body); not the arms (a T-pose standing part); the standing pose's knees straightened first. The neutral stance
is picked in the Mocap tab (the stances first - standing idles, the ready stances, crouches, kneeling - then every
animation's first frame; pause1 by default, the slots' too); the root's home is the stance's root (nwreadyr's is 17 cm
lower than pause1's), the blends start and end in it, the recipe's source names it. The latest take: the doll's standing
legs 1.2 degrees (pause1 1.1; 2.5 before), the body 1.3 (2.0). Checks: victorymg leaning 5 degrees all take - the legs
4.3 cm off at worst against 3.9 upright (10.8 before), and turned a quarter 5.6 against 4.9 (11.9 before); nwreadyr
picked - the standing part played as it, 0.3 cm (23.1 before); the recording: knees -0.3 cm, 120 of 136 locks reach
(108 before) and hold to 0.46 cm; 3 deliberate breaks caught; the picker in a headless browser. Then (owner: "after
selecting the starting neutral pose there should be a button that shows the neutral from the front, side and 3/4s so I can
get a good idea of how to position myself"): **Show the stance** - the stage shows the picked stance's first frame three
times, turned 0, 45 and 90 degrees (Front, 3/4, Side), with or without a take; again to go back.
*#81 (2026-10-29; owner: "can you take a look at the latest capture? for some reason it's playing really fast ... even the
size of the captured points don't match the dummy", then "I'll need to recalibrate the cameras, I didn't notice the lower
resolution"):* the first three-camera take (21-20-50): the rig at 13 frames a second (26-27 with two), the new camera at
720x1280 against 1080x1920, reprojection 0.36 px (0.16). Its frame times were read right; the reconstruction lost the feet
- 1.4 m under the floor in one frame in five - and the floor (the feet's lowest 5% over the take) went with them: the
standing hips read 2.27 m (0.92 on the two-camera takes, the limbs the same), the take scaled 0.44 to the doll; and its
crouched standing part turned a later straight leg 24 cm backwards under pause1 (#80). "Playing fast" not reproduced (the
bench plays it at its real 28 s). Fixed (tools only): dropImpossible - a limb point whose bone is 60% longer or half as long
as in the standing part, and 15 cm off it, dropped with the points past it on the limb and two frames either side
(resampling blends a lost frame into its neighbours, and a fill anchored on a half-lost one bends toward it), then a lost
heel or toe filled following its ankle (fillFeetFromAnkles) and the rest straight; the floor is where the feet stand in the
standing part (median; the 5th percentile over the take had also set the two-camera takes' floor 12 cm low: their hips
now 0.79-0.82 m); and the knee rule applied again after the legs' turn, at the thigh, along the doll's own hips (in a kick
the capture's hip points face 20-30 degrees off them). The take: hips 0.78 m, jumps over 25 cm 8 -> 2, knees 1.6 cm at
worst; the recordings' locks hold to 0.55-0.80 cm. Checks: feet lost 1.4 m for 0.35 s, the body sunk 30 cm for 0.35 s, a
heel lagging 15 cm (kept), a heel lost on a walk (stays at its ankle's distance to 0.9 cm); 7 deliberate breaks caught.
*#83 (2026-10-29/30; owner: "this doesn't give me enough control over the poses. for example, I can't see that's the foot
orientation. I recorded a kick for the shove animation but the foot ended up being all limp instead of having a stiff heel
out. I need something that gives me more control over the timing and position of the various body parts to make sure poses
are timed correctly and impactful"):* found - the preview never drew the feet (NWN's foot bone ends at the ankle, no toe
bone), and in 21-20-50's kicks the capture has the kicking foot toes-down by 23-88 degrees, with the heel and toe lost at
about half the kick peaks (13 frames a second can't hold a foot through a kick). Drawn first: a heel and a toe turned with
the foot bone. Options: (a) a rig in the Blender add-on, (b) pose keys and retiming in the bench, (c) Cascadeur. Asked to
read Cascadeur's .casc ("can you make the bench use it (or find a way to convert it to fbx)?") - declined: its free plan
exports .casc only and FBX / glTF start at the paid plan, so reading .casc would get around that paywall. *Decision (owner,
2026-10-30):* "Let's try bench pose keys first". Part 1 built (tools only): pose keys - a part (the body's position, hips,
torso, neck, head, thighs, shins, feet, upper arms, forearms, hands) turned at keyed moments by forward / back, side and
twist in degrees about axes in its parent's frame as the stance has it (the body's position: forward, right, up in cm);
*adjust* turns it on top of the capture, *set* replaces it (0 = the stance's pose of the part against its parent); keys
ease (smoothstep) and fade from and to the capture over a set time; applied on every body after the solve and the roots,
before the foot locks and the blends (MC.applyPoses). The bench: sliders that key the part at the marker, hold, previous /
next key, a timeline row per posed part (click, drag to retime with snapping, Delete), the part drawn blue on the doll, the
panel in the rail so the pinned preview and timeline stay in view (the stage gives up a row's height per part). A take's
edits kept in the browser by file and frame count. The solve is reused while the take, body, stance and part kept are the
same (MC.retarget's cache, compared there, not by a key the page builds): a slider to a redrawn doll 63-97 ms on the 20 s
take (a re-solve ~140). Checks: a foot set 50 degrees from the stance's (to 0.0), a forearm 40 at its key and 20 halfway, the
body moved and half-moved through its fade-in (0.00 mm), a locked foot under a turned thigh (0.00 mm), the cache the same
after other keys, another stance or another span; the browser: 15 expectations on the owner's kick take. Deliberate breaks:
8 in the core and 7 in the page, all caught (one more - the fade's lower limit - turned out not to change anything:
smoothstep clamps it). Part 2 (timing: speed a stretch up or down, hold a pose for frames) waits on the owner trying part 1.
*#84 (2026-10-30; owner: "how feasible would it be to separate legs and torso animations so that they can be mixed and matched? taking advantage of existing foot placement and let sway to increase the coherence between movements"):* options given - baked in the bench, or live in the game (the engine layers torso clips, animroot torso_g, but NWN never attacks while moving, no balance at run time, every client told). *Decision (owner, 2026-10-30):* "I was thinking of mixing in the bench", then "go" to: legs from / torso from (this take, an animation, a saved recipe; offset, speed, loop), the seam (chest keeps its direction / turns with the hips), the hips follow the weight on locked feet. Found: rootdummy carries torso_g and pelvis_g side by side (the torso doesn't hang from the hips); only rootdummy has position keys in NWN's clips. Built (tools only): MC.retarget's
halves - legs from the take or an animation (offset, speed, loop; its own root), torso from an animation, a saved recipe or the
take elsewhere (offset, speed); the take's legs are the clock. The seam: the chest keeps its direction, or turns with the hips by
how much the legs' hips face away from its source's. An animation's rootdummy turn (the ready stances stand side-on that way,
nwreadyr 17-19 degrees, 1hreadyr 21-24, swaying) carried into pelvis_g and torso_g - a mocap result keys no rootdummy turn;
without it nwreadyr's feet sat 8 cm high and swayed 10 cm. An animation's planted feet where a foot stands still and low (a
stance: one plant a foot all along; an in-place walk: none). The hips follow the weight: where a foot is held, the root slides
so the upper body's weight (segment masses) sits where the legs' own source had it, the hips sinking where a held leg would no
longer reach than it already did; a pose key's lean the same. The solve cached per frame. The bench: the rail's Mix legs and
torso, the torso's hands dashed on the timeline, the readout at the marker, an animation's plants on the rows (not editable),
saved with the take. Checks: both halves from the take unchanged (also locked, balanced); taunt's torso 0.000 deg off it, the
take's 0.3 s on x1.5 0.000; nwreadyr's legs 0.033 deg, joints 0.00 mm; seams 0.00; plants 100% / 100% / none; bow's torso over
pause1's legs 11.8 -> 1.4 cm off the feet's weight, feet 0.0 mm; a 30-degree pose-key lean 6.9 -> 0.8 cm; no feet held, no
slide; the browser: 11 expectations. Deliberate breaks: 12 core, 6 page, all caught. *Asked alongside (owner): "why isn't the
human male not very well mapped to my own body markers? the mocap data is larger than the doll"* - measured: a_ba's hips at 102.5
cm, shoulders 155.7, the head's base 175.3 (about 1.9 m, long legs, a short torso and arms); the overlay scaled to his hips
(x1.25-1.31) makes the owner's legs match (thigh 49.9 / 46.2 cm) and the rest bigger (hips to shoulders 66.6 / 53.2, upper arm
38.4 / 30.5); the retarget copies angles, so only contacts move; the takes read the owner at 156-172 cm (calibration scale).
*Bench redesign (2026-10-30; owner: "it's time for a big development ... make a plan to redesign the bench knowing what we know today ... trimming out all the fat"):* a review of the bench (an inventory and its history) and a research pass (owner: "do the research pass first"; docs/research/reports/Mocap cleanup and editing workflow.md: the three edits he asked for - speed, IK placement, key poses - are what animators do on top of mocap, on layers over an untouched capture). The picture approved ("sounds right, write the design doc"); asked whether mixes with NWN's animations are for his game or everyone: "they should ship with nwn5e" - so a recipe names NWN's animations and the game assembles it at startup. Design: docs/BENCH-REDESIGN.md. *Accepted for building (owner, 2026-10-30: "approved, write the phase 1 plan")*; the epic #86 holds the seven phases #87-#93.
*#85 (2026-10-30; owner: "I think we can actually remove the benches from the nwn5e code. these are development tools"):* two readings given - the tool folders only, or the DLL's dev-only parts too; *Decision (owner, 2026-10-30):* "b, go with your recommendation" - the folders now (tools/recipe_bench, tools/vfx_bench, tools/mocap_blender moved with their history to the nwn5e-tools repo beside this one; they find ../nwn5e-ee/build; tools/data and tools/gff stay, they generate shipped data), the DLL's dev-only parts (the library export, !animedit, the !anim browser, !animtrace, the VFX editor plumbing) with the redesign's first phase, into a dev add-on loaded on the owner's machine only.
*Built 0.58.0 (#87, the redesign's phase 1; plan docs/superpowers/plans/2026-10-30-bench-redesign-phase1.md, owner: "native, go with your recommendation"):* the dev layer - !animtrace, the !anim browser (now also `!anim <name>`, playing it in place), the VFX editor and the VFX Bench's export live in nwn5e-tools/dev, compiled into the DLL by build.bat only when that checkout sits beside this one and the build isn't `public`; shipped code reaches them through g_dev_seam (dev_seam.h), null otherwise; tests/public_dll_check.py fails a DLL carrying a dev command or the old export, or missing the core stamp (the public build passes, the dev build fails). !animedit and the library export to build\animedit\libraries are gone: nwn5e-tools' helper (nwn5e_anim_helper, built from this repo's src/data) reads the libraries from the install - the user's override, ovr\, nwn_retail.key, nwn_base.key (GameResources reads EE's keys now) - and both carry the recipe core's stamp (core_stamp.bat; the DLL logs it and writes core_stamp.txt). The assembled recipe (`kind = assembled`, data/anim_recipe + data/anim_assemble): the capture per body as turns relative to a named stance's first frame (L = S * delta), the root as the offset from the stance's, the blends into and out of the stance and the loop applied by the game at startup (anim_packs; a body without the stance or without keys logs `recipe <name>: <body>: <reason>` and the rest still build). Checked on a real take (2026-10-29_21-20-50, 7.15-9.75 s on 1hreadyr, nwn5e-tools recipe_bench/convert_assembled.js + compare_assembled.js, the game's own code through the helper): against today's keys recipe of the same solve, worst 0.82 degrees / 1.1 mm on the eight bodies unfitted, 0.84 / 1.1 fitted to 1 s (limits 1.5 / 8: the two key reductions' tolerance). Launch-checked: n5_asmtiny built for a_ba, the other bodies "no keys"; a missing stance skipped per body, no crash. Left in the shipped DLL for a decision: #95 (vfx_model's editor half, visualeffects row 1600, the vfx_bench key, !vfx / !light).
*#95 (2026-10-30), four items given info-first; Decision (owner): "go with your recommendation" - built 0.58.3:* vfx_model's editor half stays in src/data (the linker already leaves it out of the public DLL - its draft header is in the dev build only - and its tests stay in test.bat); visualeffects row 1600 stays (one empty row reserving the number); the vfx_bench key leaves config.h / config.cpp / nwn5e.ini - the dev layer's vfx_editor.cpp reads [rules] vfx_bench from nwn5e.ini itself, on unless 0; !vfx (vfx_browser.cpp) and !light (light_probe.cpp) move to nwn5e-tools/dev, so the owner's builds keep them and players' don't (the game agent told first: !light is the stealth/light rule's calibration tool). public_dll_check.py now also forbids !vfx, !light and vfx_bench.
*Capture habits (told to the owner):* two or more cameras; a T-pose at each
take's start; cycles performed on the spot; a prop of the weapon's length; a
clap or stomp at the contact moment. Rage: 2-3 s, relaxed to a fighting stance.
Crouch stance: 5-10 s held with small natural movement. Shove: ~1 s from a
combat stance, both hands forward, back to stance.

*Built 0.51.0 (owner, 2026-10-28: "go ahead with mocap mode").* tools/recipe_bench:
`public/mocap.js` (the maths), a Mocap tab in the page, `mocap_check.js`; nwn5e reads
the result as a new recipe kind (src/data/anim_recipe.cpp: `kind = keys`, KeyedAnimClip;
anim_packs builds it per library). What it does, and where it differs from the plan:
- *Reading:* FreeMoCap 1.8's `output_data/mediapipe_body_3d_xyz.csv` (header
  `body_<point>_x/_y/_z`, a row a frame, millimetres - read from FreeMoCap's own
  processed test recording, v1.8.0), its saved_data `body_trajectories.csv`
  (`# nose_x ...`) or any file with the 33 body points by name; the frame rate from
  `recording_parameters.json` (FreeMoCap has already filtered at 7 Hz and filled
  gaps up to 10 frames, so clean-up here is light: gaps, a Gaussian smoothing).
- *Axes:* found in the standing window (the T-pose at the take's start): up is the
  floor's - the plane through both heels and toes - when it agrees with the
  body's ankles-to-shoulders line within 25 degrees (the body's line leans a few
  degrees: centimetres at the hands); right the shoulder and hip lines; forward
  up x right, checked against the nose (a mirrored recording turned the right way).
- *Bone lengths aren't held* (the plan's clean-up step): the retarget uses only
  directions, so they change nothing.
- *Retarget, per library* (the libraries' bind poses differ - a_dfa's pelvis binds
  turned 22 degrees - so one set of rotations can't serve all; the bench retargets
  to each and the recipe carries a set per library): arms and legs aimed absolutely
  (shoulder > elbow > wrist > knuckles, the hand's turn from index / pinky; hip >
  knee > ankle; heel > toes); pelvis, torso, neck and head turned by the performer's
  turn since the standing window, from the body's idle stance (pause1 at 0 - the
  bind pose would twist a_dfa); the torso about the waist (the rig's pelvis pivot,
  above the hip joints - aimed from the hips, the pelvis's turn leaked into the
  shoulders); the pelvis tips 35% of the torso's lean (its forward tilt isn't in
  the 33 points); the root follows the hips' height (as a share of the hip joints'
  standing height) and their sway over the feet, travel stripped.
- *Then:* trim; blends in / out to the stance; the loop cut where the pose comes
  closest to the first (last 40%) and crossfaded 0.25 s; fit to a length with the
  contact moment (suggested: the hands' fastest) put at a chosen time; keys
  reduced where nlerp (as the engine plays them) stays within a tolerance (0.75
  degrees, 4 mm by default).
- *Checked:* NWN animations made into captures (turned, moved, Y-up, mirrored) and
  retargeted back land within 1-3 cm on average on every body (the pelvis tilt the
  points can't give); axes found exactly; FreeMoCap's own test recording retargets
  to all eight bodies (finite, every bone keyed, feet near the floor); mutations
  of the aiming, the neutral and the mirror check fail the check.
- *In game:* `n5_rage` - the Rage's flourish (victorymg swapped for it with EE's
  ReplaceObjectAnimation on part-based bodies, mapped back after); `n5_shove` - the
  Shove's swing (anim_layers' swap, in place of nwstab; the slot fits it to
  nwstab's length); `n5_cridle` - the crouched idle (the owner's recipe already wins
  over nwn5e's). Others play with `!anim <name>`. `!animedit` refuses keys recipes
  (the bench edits them).
- *Size:* keys for eight bodies - a 7.4 s noisy take ~0.9 MB; the bench's save limit
  is 32 MB.
Later (as planned): foot pinning and stride matching, mirror, several takes per
recording, the weapon hand kept from an NWN attack; a woman's own take (a_fa's own
idle and walks differ - most combat animations are a_ba's).
*Parked (owner, 2026-10-28, after trying it: "it's not great, I'll keep trying to get the mocap
working, let's move on for now"):* the owner is working on the captures themselves; back to it
with their takes and what looks wrong.

## Bench redesign, phase 2 — the new Mocap Bench's foundation (2026-10-30, #88)

*Decided (owner, 2026-10-30):* "write the phase 2 plan", then "native, go with your recommendation":
the plan (docs/superpowers/plans/2026-10-30-bench-redesign-phase2.md) was executed inline, with one
fresh review of the whole branch at the end.

*Built* in nwn5e-tools (`mocap_bench/`, port 8767; main from 3939b0e). There is no nwn5e-ee `src`
change and no version bump.
- `anim_helper`: a `serve` mode. One long-running helper assembles recipes with the game's own core
  (phase 1's AssembleAnimClip) and reports its build stamp. `helper.js` is its Node side.
- `public/core/`:
  - one math module, the library reader (the one FK) and the take readers;
  - clean-up v2 and the suggestions (spans, contacts, hit);
  - the solve, carried over verbatim from the old bench's mocap.js;
  - the assembled recipe writer and the project model with undo and redo;
  - a memoized pipeline: clean, suggest, solve, assemble, re-running only the stages an edit touches.
- `server.js`:
  - recordings, projects and recipes under `build\animedit\`, and the helper;
  - 127.0.0.1 only, with Host and Origin checked;
  - Save refuses when the helper's stamp differs from the DLL's `core_stamp.txt`, or that file is
    missing.
- The page:
  - the stage: the result, the ghost (the contact edits undone), the capture's points on the doll's
    proportions, five views;
  - the timeline: the overview, a ruler in take time, the animation's seconds, the moments, a contact
    lane per foot;
  - the inspector, the transport and the keys.

*Parity* (`checks/parity_check.js`, today's code as the reference):
- The carried-over solve matches the old bench's retarget to 6.6e-8 on all eight bodies: on the
  fixture, on a knees-behind fixture and on the owner's take (7.15-9.75 s).
- The assembled clip matches today's baked keys within 0.743° / 0.19 mm on the fixture and 0.818° /
  1.10 mm on the owner's take, on all eight bodies.
- A round trip through the fixture lands within 3.6 cm. Knees were never behind in 114 leg-frames.

*Clean-up v2* (`checks/cleanup_check.js`):
- A one-frame spike is dropped and refilled 0.1 mm off. A strike, a 3-frame hold and an overshoot are
  kept.
- A short gap is filled by the cubic within 0.61 mm; longer ones are held and marked.
- 3 swapped frames are relabelled exactly; a fast crossing is left alone.
- The filter is zero-phase (the peaks don't move) and capped at a quarter of the real rate.
- A strike's peak goes from 3.84 to 3.58 m/s, with the hands and feet cut at 3.75 Hz.

*Checks:* 121 (serve_check 7, the Node checks 94, the browser check 20). Each has deliberate breaks
or a failing run before its fix.

*Rulings that shape behaviour* (the full list is on #88):
- A Hampel run is dropped only when it snaps back (the spec's "isolated spikes that snap back").
- A contact begins on the 3-frame average speed and ends on its frame's own speed: a kick releases as
  soon as the foot speeds up.
- Re-suggest replaces only the `auto` contacts. A suggestion overlapping one the owner set (`always` /
  `never`) is left out.
- New makes the project the history's starting point, as Open does: Ctrl+Z doesn't undo the creation.
- Open and New read the take again, so a moved recording folder is reported, not shown from memory.
- Save writes nothing when any body's solve fails, or when the core refuses the recipe on any body.
- The knee rule is pinned by parity on a fixture whose knees point behind (the plan's visible-knee
  test couldn't catch its removal).

*Final review* (a fresh reviewer on the whole branch): no Critical findings. Eight Important ones, plus
two Minor ones re-graded up, were fixed in one pass, each with a check that failed first:
- the suggested hit on the owner's take was a lost stretch's release (22.07 s; now 8.70 s);
- the page's keys were swallowed by a ticked box or a chosen list;
- the inspector rebuilt under a field being typed in;
- unsaved edits were dropped by Open, New or a reload;
- auto contacts were frozen when the spans moved (they now follow the suggestion, so the ghost
  differs from the result only by the owner's edits), and a moved one stayed auto;
- the stance view didn't show the stance;
- Enter in the New dialog closed it silently;
- the browser check could hang.
The points overlay sitting off the doll with a ready stance is ruled as the assembled model's
nature, for phase 3 to revisit. The deferred minors are listed on #88.

*Finding:* FreeMoCap's `output_data` on this install holds only processed points (`3d_xyz`,
`rigid_3d_xyz`, the centres of mass), so the bench filters points FreeMoCap has already filtered. The
README says to skip FreeMoCap's own filter where its version allows.

*Next:* the owner's test (#88, matrix M1-M8), with phase 3's (#89).

## Bench redesign, phase 3 — IK targets and pins (2026-10-30, #89)

*Decided (owner, 2026-10-30):* "can you go ahead and do the IK phase? I'll look at phases 2 and 3
together". The plan (docs/superpowers/plans/2026-10-30-bench-redesign-phase3.md) was executed inline, with
one fresh review of the whole branch at the end.

*Built:*
- **The core (nwn5e-ee, game-owned files announced to the game session; it installed 0.58.4 and
  installs 0.58.5 / 0.58.6):**
  - `src/data/anim_pose.*`: the pose math in one place, and a two-bone solver for arms and legs (the
    bend side kept, the end's world turn kept, damping over the last 2% of a limb, the shortfall
    returned).
  - Assembled recipes carry `take_start` and `target <limb> <t> <x y z> <reach> [pin]` lines.
  - The IK pass (step 8), per frame:
    - the hips toward their target;
    - the legs on their planted feet, blended toward foot targets;
    - a leg whose goal is fixed in the world (a planted foot, a held key) and falls short sinks the
      hips, at most a quarter of the hip height;
    - the hands toward their targets.
  - Reach eases over 0.2 s outside a limb's keys; a held key stays still in the world until the next.
  - A report of each limb short over 3 cm, and the hips' own gap.
- **The bench (nwn5e-tools):**
  - the helper reports the shortfalls;
  - `core/ik.js` mirrors the frames a target's offset lives in;
  - targets in the project, the recipe and the pipeline (an edit re-assembles, never re-solves);
  - picking and dragging on the doll: a limb moves by the pointer's movement on the camera plane and
    keys at the marker's frame; K keys a limb where it is;
  - a row of keys per limb on the timeline (a diamond, a square when held, the reach curve faint);
  - the key's inspector: time, reach, hold;
  - readouts over the stage: limbs out of reach, foot slide in contacts, a note in a blend or the loop's
    crossfade;
  - Save's per-body report.

*Proved:*
- Through the helper, so the game's own code: a key made from a dragged place lands there (0.000 mm) on
  a_ba, and carries to a_dfa where the mirror says (119 mm from a_ba's place).
- The hips by proportion within 1 mm.
- test.bat 276/0; checks/run.js 12/12; serve_check 8/8; browser_check 29/29 (real drags and keys).
- Every check has breaks it catches.

*Rulings that shape behaviour* (the full list is on #89):
- Foot contacts stay pinned by the bench's solve (phase 2's, verified) until phase 6 moves them into the
  core with the balance. Until then there are two leg solvers.
- A foot target is stored from the hip joint (the spec's proportion rule), so it moves with the hips.
  Past reach it stays short (reported); only world-fixed goals sink the hips.
- Damping is 2%, scaled by the reach: a planted foot lands exactly.
- IK runs before the blends (the spec's order): a key inside a blend or the loop's crossfade is pulled
  back, and the page says so.
- Hand pins are held keys (two keys: the hold and the next), not hand contact lanes.
- The stage never resizes under a drag: the readouts float over it and the timeline scrolls in a fixed
  box. Found by the browser check: the drop landed 7.7 cm off.

*Final review* (a fresh reviewer): one Critical and five Important findings, plus one Minor graded up,
all fixed with checks that failed first:
- a foot past reach crouched the doll and pushed the planted foot into the floor (0.58.5);
- the report named planted feet "short" (0.58.5);
- a held key made while the hips sank landed 9 cm off (0.58.5);
- a nan take_start read past the keys (0.58.5);
- a wobbly click keyed a limb, and drags snapped it to the cursor;
- keys after a pause sat between frames;
- no note in the loop's crossfade.
The deferred minors are listed on #89.

*Next:* the owner's test of #88 and #89 together (matrices M1-M8, then I1-I8), then phase 4, poses (#90).

### The owner's test, first round (2026-10-31, #88)

*Reported (owner):* in Workflow A a foot "set to always" still floated up and down; Save showed "a big chunk
of red text"; the take row on its own scale above rows zoomed to the kept part was counterintuitive ("take s"
should show the motion too); contact bars and the marker drew over the row labels. *Decided (owner):* "go,
all four" (the fixes below, proposed info-first with the evidence).

*Found:* on n5_asm2 the capture's hip points ride up 8-12 cm in each kick, past the standing leg's reach, and
the solve only bent the leg - the planted foot lifted 7-11 cm and dropped back. At the end the owner walks
out: the heel comes off, the tracker loses the left leg and its toe point freezes below the floor, so the
contact ran on and the foot was dragged 36 cm up and 73 cm away (Save's 73 cm). "Always" and "auto" lock
alike; always only keeps re-suggest off a contact.

*Changed (nwn5e-tools, bench only - no core or DLL change):*
- Contacts start only when the ankle is still too, and end when the ankle or heel rises 3 cm above where
  it landed (a push-off) or a foot point is lost; never merged across such an end (e05b8b1).
- The hips come down just enough to hold a planted foot, at most a quarter of the hip height (the core's
  IK rule); the rest is reported short (c9bcee2). On n5_asm2 every planted foot now stays within 1 mm on
  all eight bodies (up to 13.7 cm before).
- Save's report: one line per problem across the bodies, worst first, amber (df00e56).
- The timeline: every row on one time scale (the whole take on opening), the wheel zooms about the
  pointer, Shift+wheel pans, F fits the kept part; the motion behind "take s"; everything clipped to the
  track (53ea19a).

*Rulings:*
- `solveTake` takes `sink` (default on); parity_check runs it off, so it still proves the carried-over
  solve key for key, and solve_check proves the sinking.
- The wheel over the row labels still scrolls the timeline box (it holds the IK rows); over the track it
  zooms.

*Left:* where the take overstretches the leg for a moment (n5_asm2's walk-out, 23.3-23.6 s) the hips dip up
to 19 cm as the foot lets go; trimming the kept part before the walk removes it.

### The owner's test, second round (2026-10-31, #88): a planted foot's turn

*Reported (owner):* "The foot locking is better, although there's some slidy rotation going on. I tried
keyframing it but it only locks the ankle in place, not the foot itself." *Pushed back:* not a later phase -
no phase planned it, and the spec's "no sliding locked feet" covers a swivel. *Decided (owner):* "go for A
and B".

*Found:* the solve aims each foot along the capture's heel-to-toe line every frame; a lock pinned the ankle
only, so the poorly tracked toe point swivelled planted feet up to 26-32 degrees on n5_asm2 (7.03-9.63 s),
the toes swinging 3-8 cm. A held IK foot key also held only the place (the core's two-bone solve keeps the
end's turn from the animation).

*Changed:*
- **A (bench, nwn5e-tools ee550c8):** a lock holds the foot's world turn as it landed (averaged over the
  frames its spot is), eased in and out with the lock. Now 0.0 degrees on n5_asm2. `holdTurn` (default on);
  parity_check runs the old bench's solve with it and `sink` off.
- **B (core, nwn5e-ee 53a4f4e, 0.58.9):** a pinned foot key holds the foot's world turn as it was at the
  key (on the body as it stands there, like its place) until the next key, easing over the same window;
  a free key leaves the turn to the animation; hands and the hips unchanged. SolveTwoBone takes an
  optional end turn. Built in a scratch copy while the game session's mutation run used the tree, put in
  after it: test.bat 281/0, 3 breaks caught. The helper rebuilt (stamp b8991409...).
- The bench's words (98bf4ee): auto and always both lock - place and turn; always only keeps re-suggest
  off a contact. A held key's help: a foot keeps its turn too.

*Scope kept:* a held hand key still turns with the animation (the go covered feet); holding a hand's turn
the same way is a one-line change if wanted.

## Bench redesign, phase 4 - poses (2026-10-31, #90)

*Decided (owner, 2026-10-31):* "looks good, next phase", then "native, go with your recommendation". The plan
(docs/superpowers/plans/2026-10-31-bench-redesign-phase4.md) was executed inline.

*Built:*
- **The core (nwn5e-ee 3830dc0, 0.59.0; game-owned files announced, built in a scratch copy and landed in
  one go while the game session's runs used the tree):**
  - Recipes carry `pose <part> <take t> <x> <y> <z> <push|replace> <ease>` lines (16 parts: torso, neck,
    head, pelvis, the arms and the legs - not rootdummy).
  - Step 4: push turns the capture further (R * delta), replace sets it (R); keys blend by smoothstep and
    ease in and out of the capture over their ease (0-2 s).
  - A posed pelvis keeps the feet: the IK pass re-solves the legs to the capture's foot, place and turn,
    unless the leg itself is posed.
- **The bench (nwn5e-tools):** core/pose.js (the turn convention, the posed turn, the key a drag makes, the
  capture sampled as the core does); poses in the project, the recipe and the pipeline; the stage picks
  any drawn part; pose rows on the timeline; the pose key's inspector; E / W, the turn drag, K, Delete, the
  IK-override note.

*Proved:* through the helper (the game's code), a key made from a drag turns the part exactly as dragged
(0.0000 deg, alone and after another key), every frame of a posed clip matches the mirror within 0.028
deg, and switching a key's mode keeps its pose. test.bat 298/0; checks/run.js 14/14; browser_check passes
with 7 pose steps. Every check has breaks it catches (47 across the phase).

*Rulings:* the plan's up-front five (stance-axis degrees; the pelvis keeps the feet; IK wins on its chains;
picking by segment; the drag's key), plus those in the ledger, listed on #90.

*Next:* the owner's test of #90, then phase 5, timing (#91).

*Final review* (a fresh reviewer): one Critical and four Important findings, all fixed with tests that failed
first (core 0.59.2, 746effd; bench 7bae020):
- a posed pelvis with a foot key, or a leg key inside its window, popped a planted foot ~15 cm - a turned
  pelvis now never moves a foot (the leg's goal from the pose without the pelvis's keys);
- ease-0 keys vanished at the bench's 4-decimal times - now the key's own frame within half a frame;
- a drag or K on a hand or foot under IK landed up to 42 deg off - the key is made from the part's own
  step-4 delta (pose.js keyForTurn);
- a chain's key under its target still turned the hand or foot - now it has no effect there;
- a pose selection went stale after an undo (Delete threw) - kept by its key's time.
test.bat 303/0; checks/run.js 14/14; browser_check 40/40. The deferred minors are listed on #90.

### The owner's questions while testing phase 4 (2026-10-31)

*The foot's spin (owner: "in between the two keyes the foot does a few rotations before landing. Is there a way to
fix this? like a "Simplify motion" feature?"):* measured on saaaaa through the helper - the webcam lost the right
foot in the kick: its own track turns 429 deg between the hit (10.77) and the plant (11.63), up to 140 deg off its
standing turn, and push keys keep that. Answered with replace (already built): the two kick keys switched,
the foot turns 216 deg there in the world (96 against the shin, the straight blend), its largest frame step 14 deg
instead of 88; the planted keys stay push (the contact's held turn). The owner: "Ah, cool it does work!" A repair
of a lost foot in the solve (an ankle limit that leaves the turn-out, or spin detection) was offered, not asked for.

*Planted height (#97; owner: "how can I adjust the height at which one foot is locked to the ground? In saaaaa the
left foot is grounded a few centimeters off the ground but if I key it in the correct position it messes up all the
animation"):* measured - a lock held the ankle where the capture had it at landing: saaaaa's left at 17.1 cm, pause1's
standing ankle 14.0; a free IK foot key follows the hips (free keys at both ends slid it up to 81 cm); two held keys
work but must share one spot. *Decision (owner, 2026-10-31):* "go on your proposal" - on the floor by default, a
height per contact. *Built (nwn5e-tools aef605b, tools only):* a lock's ankle at the stance's standing ankle height
(its first frame) plus the contact's height (m in the project, -0.1 .. 0.5, cm in the inspector; setting it makes an
auto contact always); the hips sink as before; `floor: false` for the parity runs. saaaaa now: the left ankle 13.8-14.0
cm all clip, the right planted 13.9-14.1 (was 12.8-13.4), the body 3.2 cm lower in the kick only (the straight standing
leg), no new pop (the head's largest frame move 3.3 cm, as before). checks/run.js 14/14, browser_check 41/41, 9 breaks
caught. Cost: every project's planted feet move to the floor when next saved; a tiptoe plant needs its height.

*The doll's face (#98, Todo; owner: "is it on the plan to give the bench doll a "face" and obvious hand orientations so
that those can be easier to adjust?"):* not on the plan (BENCH-REDESIGN's "no faces" non-goal is facial animation).
Filed for later; the marks are to be decided when it's picked.

*A kick from a ready stance (#99; owner: "it looks good starting from the "pause1" pose. but if I move to any other it loses
it's power, the leg no longer extends fully"):* measured on saaaaa through the helper - the legs' neutral (#80) is a fixed
turn per thigh, shin and foot, so a ready stance's knee bend (44-55 deg) stayed in every frame: the kick's knee at its
straightest 42-52 deg bent (pause1: 1), the ankle 86-94 cm high (pause1: 102). Offered: a leg in the air moves as
performed (recommended), a pose key per stance today, or pause1's mapping with blends. *Decision (owner, 2026-10-31):*
"sure, let's try it". *Built (nwn5e-tools, tools only):* solveTake's `upright` (the pipeline hands it pause1): a planted
leg keeps the start pose's posture; off the ground a leg's thigh, shin and foot ease into their solve with pause1's legs'
neutral (the rest of the body still the start pose's) over 0.3 s after the lift, and back over 0.3 s before the landing -
by the locks as planted. saaaaa now, under each ready stance: the kick's knee 0-2 deg at its straightest, the ankle 94-102
cm, both knees at the start still the stance's (nwreadyr 53 / 60 deg); the knee no faster at the lift and the landing
than under pause1 (nwreadyr 24.4 / 11.9 deg a frame, pause1 31.3 / 12.4). Checks: airLegAsPerformed, plantedLegsKeepStance,
postureEases (solve), startPoseKeepsAirLegs (pipeline); 5 breaks caught. Caveat: pose keys sit on the start pose's axes,
so keys made under one start pose turn differently under another (saaaaa's thigh keys were made under pause1).

### Seam matching (2026-10-31, #100; plan docs/superpowers/plans/2026-10-31-mocap-seam-matching.md)

*Asked (owner):* "how can we extend what we learned here to connecting other motions to different starting poses?",
then "would this also work on a punch, or a two handed shove", "ok, go ahead", "native, go with your recommendation".
*Measured first (saaaaa, keys off):* the first frame 36-42 deg off pause1 in the forearms, 84 deg off nwreadyr in the
right forearm; the arms never took the stance; the first contact locked where the performer's foot was.
*Built (nwn5e-tools 6584723, c16ec0f, eb66eb3, 9a2aad0; bench only):*
- **The fade at the ends:** at the first and last frames the body's gap to the stance is added back as an offset that
  fades out 0.1 s before the hit (0.3 s at most) and back in after the recovery (or 0.1 s after the hit). It covers the
  whole body - the legs too, since the IK alone left them 39 deg off - and the root. The middle keeps the performance
  and its speed (no crossfade).
- **The target is the stance's pose in the world.** The solve, like the game, leaves rootdummy at its rest turn, and
  the ready stances turn it 17-27 deg about a tilted axis. So the root's children (pelvis, torso) take that turn, and
  the root is placed so the pelvis lands on the stance's.
- **The feet:** a contact covering an end - or whose ease reaches it - locks on the stance's foot spot and turn when it
  landed within 0.25 m; a farther one stays and Save names it. Landings are measured on the body before the fade (the
  fade had moved a whole-clip contact up to 30 cm).
- **The clip setting "start and end on the stance":** on by default, older projects too; while on, the recipe writes no
  blends.
- **Save names** a fade too short to ease, a far foot, and a joint a pose key or IK target leaves more than 2 deg off
  the stance at an end.
*Measured after:* saaaaa (keys off) under pause1, nwreadyr and 1hreadyr: both ends within 0.07 deg of the stance clip's
own first frame, the feet 0.0 cm off its spots. saaaaa as saved: Save says the R foot is 75.1 deg off the stance at
both ends - its foot keys at 9.93 s and 12.57 s, made before the seam, now turn the stance's planted foot.
*Final review* (a fresh reviewer): two Important findings, both fixed with tests that failed first (a contact a few
frames off an end left that end up to 5 cm off; keys at the ends broke a planted foot silently); the game's blend from
the real stance into the clip measured exact. Checks: checks/run.js 14/14, browser_check 42/42, 20 deliberate breaks
caught. Rulings and deferred minors: the ledger, listed on #100.
*Follow-up:* #101 (pose keys keep their look when the start pose changes).

*Pose keys across start poses (#101) and the doll's face (#98); the owner, 2026-10-31: "go with your recommendation,
do #101 and 98".*
- **#101 (nwn5e-tools 161dcd2):** picking another start pose rebases every pose key, so its part keeps the turn it had
  at the key, against its parent - pose.js rebaseKeys, from both start poses' solves - the stance and the keys in one
  undo step. My rulings on the issue's open questions: "keeps its look" is the joint (for the pelvis and torso, their
  turn in the world, since they hang from the root); a key under an IK target keeps its own turn (IK still wins).
  Through the helper, rebased keys turn their parts within 0.0000 deg of before (33.5 deg off not rebased). Caveat:
  IK targets aren't rebased.
- **#98 (nwn5e-tools 43e0eae):** the doll's head has a nose (a line along its forward, a dot at the tip) and each hand
  a tick on its thumb side, both turning with their bones, in a darker shade; the feet unchanged (heel and toe already
  show). Checked: in pause1 the face and thumbs point ahead; a head turned 90 deg turns its nose 90; a hand turned
  over flips its tick.
- Checks: checks/run.js 14/14, browser_check 43/43, 6 deliberate breaks caught.

### The same clip on any start pose and body (2026-11-01, #107; plan docs/superpowers/plans/2026-11-01-mocap-stance-posture.md)

*Asked (owner):* "we need to find a way to make the same animation fit different starting poses and body types ...
n5_rage, in the standard pause 1 and a_ma body it looks fine, move it to a_fa and the character is off balance. change
the starting pose to 2hreadyr and the peak rage pose goes really low and one of the arms clips the leg", then "go for
it with your recommendations".
*Measured first:* the solve mapped the take onto each body's start pose, so the stance's posture stacked on the
performance: a_fa's pause1 leans 10 deg (a cocked hip); 2hreadyr's root sits 12-16 cm lower and turns the torso 22-48
deg. The owner's two hips IK keys, offsets from the start pose's root, took the roar a further 15 cm down.
*Built (nwn5e-tools e0a11fa, 5c23732, 1266e0b, 07d3e18; bench only):*
- **The start pose shapes only the ends.** The middle maps onto each body's upright reference: its pause1 with the
  upper body turned about the hips until pelvis-to-neck stands vertical, its facing and legs kept (not the bind pose:
  a_dfa's turns the pelvis 22 deg). #100's fade keeps the ends on the start pose.
- **A clip setting "stance posture" (0-100%, 0 by default, older projects too)** blends the start pose's lean, crouch
  and twist back in through the middle; 100% is the old solve.
- **Hips IK keys follow a start-pose change** by the share the body doesn't (kept in the world at 0%, their numbers
  unchanged at 100%), in the same undo step as #101's pose keys.
*Measured after (n5_rage as saved, 0%):* the head at the peak, pause1 / 2hreadyr: a_ba 104/104, a_fa 98/97, a_da
108/108, a_dfa 106/106 cm (2hreadyr was ~15 cm lower); the arms clear the legs under 2hreadyr by 8.3 / 0.3 / 13.7 /
8.8 cm (all four clipped before); a_fa's weight 4.7 cm to the side under pause1 (9.5 before); both ends within 0.05
deg of the stance.
*Final review* (a fresh reviewer): one Important fixed with tests that failed first (the hips rebase ignored the
posture: at 100% a key landed 17 cm off the body); one ruled for the owner's test: at 0% a ready stance's end fades
bridge 32-61 deg in 0.3 s (n5_rage on a_ba under 2hreadyr: the torso up to 245 / 294 deg/s, the roar's own peak 122)
- a fade that grows with the gap is the follow-up if it whips. Checks: checks/run.js 14/14, browser_check 45/45, 16
deliberate breaks caught. Caveats: the hips rebase is exact on the body shown (rebased on a_ba, 5.5 cm off on a_dfa);
the 10-15 cm sideways weight is the owner's first hips key's offset. Rulings and deferred minors: the ledger, listed
on #107.

*Rage tilts in combat (#108, 0.59.9, 12cc8db; built in the recipe core at the game session's request):* bench-made
clips keyed no turn on a node without a track, so the engine kept rootdummy's turn from the animation before - the
combat stances turn it 17-27 deg, which tilted Rage's n5_rage with a weapon drawn. The core now keys every node below
the model's top that has no turn track at its rest turn, on both of its return paths. AnimAssembleKeysUnkeyedTurns, 4
breaks caught, test.bat 311/0. The bench's helper is rebuilt for the new core stamp after the install.

*Correction, 0.59.11 (8d59bec; the owner: "fix it"):* 0.59.9 held every untracked node at its bind turn, not just
rootdummy, and the start poses turn many of them away from bind - so a bench clip snapped the weapon point (rhand, 8.5
deg in 1hreadyr), the shield point (lforearm, 13-19 deg), a_fa's shoulder pad (up to 70 deg) and the robe and cloak
bones (up to 72 deg) off the stance for its length. Found when the bench's checks ran on the 0.59.9 helper. Now an
untracked node holds the start pose's first-frame turn (the recipe's S * delta, with no delta); rootdummy its rest;
its untracked child belt_g1 the root's turn folded in, as the solve does for the pelvis; a name the tree holds twice
(a_da's two Impact nodes) stays unkeyed, since the engine matches nodes by name. n5_rage under pause1, 1hreadyr and
2hreadyr: every node but the root within 0.07 deg of the stance at both ends on a_ba, a_fa, a_da and a_dfa. Also the
helper's frames count (nwn5e-tools 05cb3b5) and the bench's checks (2853076). AnimAssembleKeysUnkeyedTurns rewritten,
7 breaks caught, test.bat 311/0, bench checks 14/14, browser_check 45/45.

### The owner's test of #108 with `!test anims` (2026-11-01/02): follow-ups #111-#115

*Reported (owner):* "the shield intersects with the body. We'll need to add accessories to the bench, where a shield
sits, weapons as well so we can preview how they look, even if they're just long cillinders"; "just because they're
raging, doesn't mean they can suddenly 1hand a great axe"; "the cloak doesn't animate correctly, it stays stiff in
both animations"; "the right foot is weakly placed" (a_dfa); "the right arm spasms a bit".
*Measured:* NWN keys its cloak and robe bones by hand in every animation (22-26 bones; up to 109 deg in 1hslashl, 177
in kdbck) - no cloth simulation. n5_rage's right foot: the last contact lands ~25 cm off the start pose's spot on
every body, at #100's 25 cm cutoff, so it drags ~25 cm at the end on some bodies (a_dfa in the game's 12:16 recipe,
a_fa in today's solve); the start poses' footing differs by 17-22 cm at the right foot. The right elbow jolts at
32.30 s in the capture.
*Decisions (owner, 2026-11-02):* "111, 112 and 113. go with recs" - #111 (a) gear as shapes measured from the game's
models, a gear picker, a warning when gear passes into the body; #112 (c) a two-handed variant per clip, generated
(the off hand on the haft) and replaceable by a recorded take, played by the game when a two-hander is wielded; #113
(a) simulated cloth baked into keys; in that order. #115 (span smoothing): "it's 32.3, but the replace keyframes
solved it" - closed, solved with existing tools. #114 (the foot drag): the owner asked for rules that extend to any
animation; the cutoff is general in scope but flips between two results on a tiny difference - a step rule is
proposed (shift under ~5 cm, step above, never slide), waiting on his pick.
