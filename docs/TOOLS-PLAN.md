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
cm, shoulders 155.7, the head's base 175.3 (about 1.9 m, long legs, a short torso and arms); the overlay scaled to their hips
(x1.25-1.31) makes the owner's legs match (thigh 49.9 / 46.2 cm) and the rest bigger (hips to shoulders 66.6 / 53.2, upper arm
38.4 / 30.5); the retarget copies angles, so only contacts move; the takes read the owner at 156-172 cm (calibration scale).
*Bench redesign (2026-10-30; owner: "it's time for a big development ... make a plan to redesign the bench knowing what we know today ... trimming out all the fat"):* a review of the bench (an inventory and its history) and a research pass (owner: "do the research pass first"; docs/research/reports/Mocap cleanup and editing workflow.md: the three edits they asked for - speed, IK placement, key poses - are what animators do on top of mocap, on layers over an untouched capture). The picture approved ("sounds right, write the design doc"); asked whether mixes with NWN's animations are for their game or everyone: "they should ship with nwn5e" - so a recipe names NWN's animations and the game assembles it at startup. Design: docs/BENCH-REDESIGN.md. *Accepted for building (owner, 2026-10-30: "approved, write the phase 1 plan")*; the epic #86 holds the seven phases #87-#93.
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
proposed (shift under ~5 cm, step above, never slide), waiting on their pick.

### Gear on the bench's doll (2026-11-02, #111; plan docs/superpowers/plans/2026-11-02-mocap-bench-gear.md)

*Built (nwn5e-tools ccd1852, 9a5e679, 77a7119, e4fd9f3, 434c663; bench only):*
- **Measured from the game's models.** gear_measure.js reads nwn_base.key's baseitems.2da, blueprints and part models,
  and keeps per item, in its attach node's frame (weapons on `rhand` along +Y, shields on `lforearm`), each part's
  mesh box and its cells: the triangles clipped to 8 slices along a weapon, a 5 x 5 grid across a shield. Two hands
  for a Large weapon. Only numbers are kept (public/core/gear_table.js).
- **A gear picker** (weapon / shield, beside ghost / points), never saved; a project or start-pose change picks the
  start pose's gear (1hreadyr a longsword and a large shield, 2hreadyr a greataxe); a two-hander clears the shield.
- **The warning:** each part's cells against the body's capsules (torso, head, shoulders, thighs, shins, the free
  arms, by the hip height), deeper than 1 cm. The timeline's take row is marked; Save names each item's deepest pass
  per body part, ranked by each line's own threshold so it isn't buried.
- **The general rule** (the owner's standard): NWN's own stance loops (pause1, 1hreadyr, 2hreadyr, every frame) are
  clear on all four bodies. Against the real item meshes on NWN's own 1h/2h/pl/shield clips (140 clip-body runs), 48
  warnings, 41 confirmed by the mesh.
*Measured on the owner's n5_rage* (battleaxe and large shield): the axe ~13 cm into the torso and ~10 cm into the
head at ~0.8 s on every body, ~6 cm into the left shoulder; the shield into the head or torso on a_ba and a_fa - 19
of 20 warnings confirmed by the real meshes.
*Final review* (a fresh reviewer): six Important findings, all fixed with tests that failed first - header boxes
that included the node origin (the halberd's head ran down the haft), the "quarterstaff" that was the two-bladed
sword, solid boxes over curved shapes (69 of 91 warnings false on NWN's own clips), the head and the shoulders
uncovered, and gear lines buried at Save. Checks: checks/run.js 15/15, browser_check 46/46, 23 deliberate breaks
caught. Rulings and deferred minors: the ledger, listed on #111.

### Planted feet never slide at a clip's ends (2026-11-02, #114; plan docs/superpowers/plans/2026-11-02-mocap-bench-foot-step.md)

*Asked (owner):* "the right foot is weakly placed" (n5_rage on a_dfa); then "any "fix" ... needs to be based on rules
that are extensible to other animations ... feet that are planted should not move (right?) ... The foot should drag,
but I also need to learn how to plant feet correctly so it doesn't happen"; "Go with your recommendations" (the step rule).
*Diagnosed:* the bench's solve held the foot still to the end (0.0 cm); the game's assembly dragged it 25.5 cm in the
last 0.3 s - the loop crossfade (n5_rage loops, 0.4 s) pulling a last plant that #100's 25 cm move-or-leave rule had
left off the stance's spot toward the first frame (on the spot). My first explanation (#100's lock letting go) was
wrong; the owner's own reading (the loop) was right.
*Built (nwn5e-tools 41280e2, ec62aed, dce65b7, a66c9d8; bench only):*
- **The step rule:** a clip's first / last plant covers its end when its held frames reach into the seam's window.
  The first 5 cm of its gap to the start pose's foot spot shift the plant (its turn by the same share); any rest is a
  step - the foot lifts (a lift growing from nothing), moves only while up and sets down on the spot, at least 4
  frames. Two feet stepping at one end take turns; a plant with no room to step lands on the spot. No point where the
  behaviour flips.
- **No loop crossfade while a returning clip's ends match the stance** (both ends are the stance; it only pulled a
  stepping foot down). In-place clips keep it.
- **Save** names each step as a notice ("right foot steps 25.4 cm onto the stance's spot at the clip's end"), still
  names a leg a key leaves off the stance, and measures slides over the held spans only.
*Measured* (n5_rage through the game's assembly, 4 bodies x pause1 / 2hreadyr): every foot starts and ends 0.0 cm off
the stance's spot; never both feet up; planted frames move only 0.5-0.8 cm at isolated frames (the recipe's key
reduction, pre-existing). *Final review* (a fresh reviewer): 7 Important + 3 re-graded Minors fixed with tests that
failed first; checks/run.js 15/15, browser_check 46/46, 15 deliberate breaks caught. Rulings and deferred minors: the
ledger, listed on #114.

### The two-handed version: a grip in the recipe (2026-11-02, #112; plan docs/superpowers/plans/2026-11-02-two-handed-variant.md)

*Asked (owner):* "just because they're raging, doesn't mean they can suddenly 1hand a great axe"; "Go with your
recommendations" - a grip rule in the recipe and one two-handed version per clip (polearms use it too for now).
*Built:*
- **The grip (core 0.60.1, 157c8f1):** a recipe's `grip = 1` holds the left hand where the start pose holds it
  against the right hand - its place and turn in rhand_g's frame, from the start pose's first frame on each body -
  every frame, after the other IK. A body without the right hand or the left arm has none. Where the left arm can't
  reach, it stretches straight at the grip and the shortfall is named like an IK target's.
- **The variant pick (0.60.1):** `AnimationVariant(name, two_handed)` gives `<name>_2h` when the character wields a
  two-hander and that recipe is built (16 characters at most). The game session's 0.60.2 (bac312d) uses it for Rage.
- **The bench (nwn5e-tools 6f5ecee, 3c9e3e3):** the clip's "two-handed grip" box, and "make the two-handed version":
  a new project `<name>_2h` - the take, edits and settings the same, the grip on, the start pose 2hreadyr (keys
  rebased as a start pose change does) - opened unsaved; refused with the reason over 16 characters or when it exists.
  An edit made while it (or a start pose change) solves wins.
*Final review* (a fresh reviewer) - "with fixes":
- **The plan's rule isn't enough (C1, open):** on n5_rage made into the version, the left arm stretches straight at the
  haft and falls short on 87-94 of 105 frames on a_ba, a_da and a_dfa (20-47 cm; a_fa 73 frames, 20 cm): the
  one-handed take carries the weapon hand where the off hand can't follow. The remedy changes how the weapon hand
  moves - waiting on the owner's pick.
- **The blends undo the grip (I1, with C1):** the grip is solved before the clip's blends, so unseamed clips and
  in-place loops drift 2-9 cm inside their blend windows (seamed clips like n5_rage don't). Fixed with C1's core change.
- **An edit during the solve was lost (I2, fixed 3c9e3e3):** the guard compared the project with itself.
*Checks:* checks/run.js 15/15, browser_check 48/48, 12 deliberate breaks caught (3 of them core rules through the
helper). Rulings and deferred minors: the ledger, listed on #112.
*Addenda (owner, 2026-11-02):* "go with a, and c for the important clips"; "yes, go with the polearm version too. For
the crossbow let's leave it at 1 handed rage for now"; "p2"; "let's go with your recommendations" (t3).
- **0.60.4 (d8883c1):** the grip runs on the finished frames (after the blends); where the off arm can't reach, the weapon
  hand comes in; `<name>_pl` on plreadyr for polearms, spears and staves (`data::WeaponGrip`); the game session's
  0.60.5 (41aae35) passes NWN's drawn hold (double weapons and crossbows: one-handed). Bench 52d3967: two buttons.
- **The second review:** on n5_rage the arms flipped at the strike (the polearm's upper arm 92-179 deg a frame, the hands
  13-38 cm apart between frames) and the weapon elbow popped 12-25 cm; the ends' left elbow sat 2-4 cm off.
- **0.60.6 (27d4286):** the weapon turns first - along the shortest turn toward the off hand, its forearm roll up to
  80 deg and the wrist up to 60 deg - and the hand moves only for the rest; the twist goes to the forearm's roll. Both
  arms solve from the arm as solved on the frame before (the start pose's on the first), bending toward the start
  pose's bend. Where folding the arm would pass the wrist's caps, the weapon follows the forearm back while that
  converges; a long polearm grip that would chase (1.2 m on a_dfa) keeps the weapon and bends the wrist instead.
*Measured on n5_rage (4 bodies, through 0.60.6):* every frame on the grip in both versions; no flips or pops; the ends
exactly the start pose; the hands within 3.4 cm between frames. Two-handed: the weapon hand moves up to 12-38 cm, the
wrist within 63 deg. Polearm: the hand moves 5-70 cm; the wrist within 57 deg on a_fa, up to 84-89 deg on 12-13 frames
(a_ba, a_da) and up to 117 deg on 50 frames (a_dfa, whose polearm grip spreads the hands 89 cm). A recorded two-handed
take (the owner's (c)) is the quality path for clips that matter.
*Checks:* test.bat 331/0; bench checks/run.js 15/15, browser_check 48/48; breaks caught across the core, the helper and
the browser (ledger). Rulings and deferred minors: the ledger, listed on #112.
*The owner's test of 0.60.6 (2026-11-02):* "the two handed sword becomes awkward but the two handed polearm is straigh
up nonsense. the pole arm flips in the middle of the animation and then flips again" - the weapon followed the one-handed
take's fist (the polearm turned 136 deg against the torso on a_dfa; the fist 167). Owner: "go with f1".
- **0.60.9 (2315ebf) and 0.60.10 (dff8f46):** a recipe's `grip_follow` (0..1; absent 1). The weapon hand's place and
  turn against the torso are the start pose's, mixed toward the take's by the share; the forearm takes the hand's twist
  against the start pose's wrist (0.60.9 let the roll pile up: the wrist 180 deg off). Bench d5f5f33: the slider "weapon
  follows the arms (%)"; the polearm version opens at 25%, the two-handed at 50%.
*Measured on n5_rage (4 bodies, at those shares):* the polearm within 28-32 deg of its start direction (was 102-136),
the two-handed 45-57 (was 79-82); the wrist within 16-32 / 38-58 deg of its holding turn; every frame on the grip; no
flips or pops; the hands within 0.7 cm between frames; the ends exactly the start pose.

### A clip's versions saved with it (2026-11-02, #117; plan docs/superpowers/plans/2026-11-02-mocap-bench-clip-versions.md)

*Asked (owner, testing #112):* "We need to find a way to make saving the variants easier." "go with your
recommendations, 117 first".
*Built (nwn5e-tools 9cec76e, ea44e28, 97f0e68, 6deb365, fecb556; bench only):*
- **The clip keeps its versions:** a box per version (two-handed on 2hreadyr, polearm on plreadyr), each with its own
  "weapon follows the arms" share; none while the clip holds its own two-handed grip.
- **One Save writes the clip and every version it has,** from the same take and edits (each rebased onto its start pose
  on the shown body). The server checks every recipe on every body with the game's code and writes all or none. The
  status names the versions, any separate project whose recipe a version replaces, and a version's recipe left from an
  earlier Save (the game still plays it).
- **A "show" switch** puts a version on the doll (its gear too; the header says so). While it's shown, keying from the
  doll waits with a word - the pose is the version's, the keys the clip's.
- **The separate-project button (#112) stays,** for a version with its own edits: it saves the very recipe the clip's
  version does.
*Final review* (a fresh reviewer): one Critical - older projects (n5_rage's) couldn't tick a version - and six
Important, fixed with checks that failed first; checks/run.js 15/15, browser_check 52/52, 32 deliberate breaks caught.
Rulings and deferred minors: the ledger, listed on #117.

### Whole-pose keys and simplify to main poses (2026-11-02, #116; plan docs/superpowers/plans/2026-11-02-mocap-bench-whole-pose-keys.md)

*Asked (owner):* "is it possible to key the main poses of an animation to simplify these animations and make them more
readable?" (#116: n5_rage keys every frame, 1% of its arm frames at rest; NWN's own clips 2.5-8.7 keys a bone a second,
21-58% at rest), then "but this way we're not keying the whole pose, we're keying specific parts" and "yes to both, go
with your recommendations".
*Built (nwn5e-tools 6e689e7, 32022f2, 57dfaab, 42267c1, 8f0ddaa; bench only - the core, the recipe format and the
install unchanged):*
- **Whole poses:** Shift+K keys every part at the marker as the doll stands, as one key on a "whole pose" row (replace
  by default). It moves, deletes and takes its mode and ease as one; turning a part at its time adjusts it inside it; a
  part's own row takes its key out. Stored as the parts' pose keys sharing an id, so rebasing (#101) and versions (#117)
  carry them unchanged.
- **The body flows between whole poses:** a part keeps moving through one unless it turns back or holds there (a
  monotone cubic, each run measured from its first key); a part's own keys still stop at each key. The recipe carries
  the flow as a pose line at every frame the core samples, so the game plays exactly what the doll shows.
- **Legs:** keyed by a whole pose only while the foot is off the ground; release keys keep a planted leg on the capture.
- **Simplify to main poses** (the clip tab): a detail slider (degrees, 8 by default) and a button. It keys the clip as
  whole poses where its motion turns or holds, at least two frames apart, never for a one-frame tracking glitch, with
  runs of near-equal keys held still. It keys what the doll shows now; Ctrl+Z undoes it, and right after, the slider
  simplifies again in its place.
*Measured (n5_rage at 8 deg, through the helper):* 26-27 whole poses (7.5-7.8 a second), the arms at rest 36-44% of the
frames (was 2%); its fist shaking (0.6-1.4 s) stays dense - real motion, faster than the detail. 8 deg sits at the top
of NWN's key rate (as a fast attack); the slider goes coarser for idles and taunts.
*Final review* (a fresh reviewer): one Critical - the flow swung a part past 180 deg the long way (149 deg in one frame
on n5_rage's hand) - and two Important (a leg's own key lost its ease beside a plant; a planted leg's whole-pose key
ignored a turn), fixed with checks that failed first; checks/run.js 16/16, browser_check 63/63, 34 deliberate breaks
caught. Rulings and deferred minors: the ledger, listed on #116.
*The owner's test of simplify (2026-11-02):* "The simplify option ends up creating A LOT more keyframes and not
simplifying things at all" - a whole pose keyed every part whenever one strayed (n5_rage: 284 keys), and the fast second
(0.5-1.6 s) kept a pose every frame or two. "go with A + C, the shaking is noise"; measured, C (a global cutoff) was the
wrong tool - the fast second is a 1-2.6 Hz flail plus the hands' roll jitter, so a cutoff low enough bent the whole clip -
then "go with 1 + 2, commit A first".
- **A (nwn5e-tools ba5a08d):** each part keyed only where it turns or holds; keys within a frame share a whole pose.
- **1 (b08e237, a71eaa8):** "smooth here" spans on their own timeline row - double-click to add, drag, Delete; inside
  one, simplify keeps only the motion slower than its cutoff (1.5 a second by default, 0.5-6), easing in over 0.1 s.
- **2:** the hands' and forearms' roll (twist about their own bone) counts a third against the detail.
*Measured (n5_rage at 8 deg, a span over 0.5-1.6 s at 1.5 a second):* 284 -> 99 keys (1.8 a part a second); the flail's
second 75 -> 36 keys. *A second fresh review:* simplifying again after a span edit refitted its own result (fixed: it
refits the performance), a new span's cutoff field was empty (fixed), and five rules had no check (added); checks/run.js
16/16, browser_check 67/67. Rulings and deferred minors: the ledger, listed on #116.

### A slot's time budget (2026-11-02, #122; with #121 on the game side)

*Reported (owner):* "I've had the rage animation cancel an attack animation because it was too long (the attack still
went through, just didn't show the animation)". Found: since 0.61.0 Rage is an instant Bonus Action, so its 3.47 s
flourish (a fire-and-forget at speed 1.0) plays mid-round; swings are 1.0 s at about 1, 3 and 5 s of the 6 s round.
*Decision (owner):* "B and C, do not trim it, I'll tailor the animations to fit the budget" - C (#121, game side): wait for
the swing, then play the flourish while pausing the round for its length, as an equip does; B (#122, nwn5e-tools
f9a2d4e): each slot's time budget - n5_rage 1.5 s, n5_shove 1.0 s - in the clip tab, as a red line on the take row, and
named at Save. No clip changed.

### Flourishes inside the combat round (2026-11-02, #121, 0.61.3)

*Found:* a script PlayAnimation runs ClearAllActions - the swing in progress cut, the target dropped, the round
emptied (also the game session's BA-12). *Built* (plan docs/superpowers/plans/2026-11-02-flourish-in-the-round.md):
src/data/anim_flourish (the pure decision) and src/core/anim_flourishes (a creature-update listener). A flourish is
set on the creature directly (CNWSCreature::SetAnimation), never through a script:
- in a fight it waits for the creature's swing (its own round pause, or an enemy's at it), then pauses its round for
  the clip plus 75 ms as the engine pauses for an equip (CheckActionLength first; the round's length shrinks, no
  attack lost); after the round's last swing, or between two rounds, it waits for the next round;
- out of combat it plays at once, set back to the idle at its end so the next one is sent again;
- it waits while the creature moves, and drops if it dies, the Rage ends first, or after 12 s.
Rage: victorymg, swapped for the built n5_rage (_2h, _pl) on part-based bodies. Intimidating Presence (owner: "C with
A meanwhile"): victoryth, or n5_presence once the owner records and builds it - no code change then. One log line per
stage (`flourish:` asks / waits / plays / over / done / drops).
*A fresh review* (0.62.1): "attacking" read the current action, which reads none outside the engine's RunActions
(now the action queue's head), and an enemy's swing pausing the round before its own round ran was played over
(now it waits; a pause counts while the round runs or in a fight). Four minors deferred (the ledger, listed on #121).
*Accepted (owner, 2026-11-02: "it looked right, accept #121")* on 0.62.2: out of combat at once; mid-fight it
waited for the swing three times and the next attack followed the clip on the same target; after the round's swings
it played at the next round's start; Presence's victoryth the same. The enemy's-swing case wasn't exercised.

### Flourishes stretch the round (2026-11-03, #128)

*Reported (owner, 0.66.1):* "the animation only kicked in at the start of the next turn even though the vfx and the
voice line played immediately" - with less than the clip left in the round, #121's flourish waited for the next round.
*Direction (owner):* "why can't the round be paused to play the animation? the sanctity of the 6 second round is
already gone out the window from the moment it is paused for attacks." *Built:* the flourish pauses its running round
right after the swing in progress, however little of the round is left: no CheckActionLength, no
DecrementRoundLength - the round's timer stands still for the clip, then runs on, so the round ends later and its
later attacks keep their timing. Still waits for a swing in progress (pausing over it cut the swing - #121's bug).
*Accepted (owner, 2026-11-03: "it's good")* on 0.66.2 (with #53's one swing per attack): six flourishes played, the
only waits were for a swing in progress (90 and 478 ms), none dropped.

### Lights in the VFX Bench's effects (2026-11-08, #145; plan docs/superpowers/plans/2026-11-08-vfx-bench-effect-light.md)

*Asked (owner):* "can the vfx bench edit the light source of an effect?" -> "plan it" -> "go with native, start the light
plan". *Found (Task 0, read-only):* 91 of the 489 effect models carry a light node (flag 0x002; its header at +0x60,
shadow at +0xB0); the game keys only its colour (76: the fireball's flicker, Premonition's blue loop) and its radius
(88: fades from and to 0), never the multiplier. *Built:*
- vfx_model (src/data): a VfxEffect's lights (VfxLight: parent, position, colour, radius, multiplier, shadow, colour and
  radius keys) through the draft (`[light <name>]` after the emitters, `<anim>_color` / `<anim>_radius`), the ASCII model
  writer (`node light`, Aurora spelling - `isdynamic`, `affectdynamic`, `colorkey` / `radiuskey`; the animations last
  their keys) and both readers (ASCII and compiled: a light's ancestors kept, its keys read). The library export is
  format 12, so the game's effects come into the bench with their lights.
- The VFX Bench (nwn5e-tools): the Lights card - add / remove, hangs under the point, a node or an emitter, position,
  colour, radius, brightness, shadow; Flicker (colour keys in the loop, whole flickers, a pattern) and Fade in / out
  (radius keys: a lasting effect's start and end, a one-shot's impact, lengthened to fit); its keys follow a changed
  colour or radius; warnings past 4 lights or 20 m. The preview: a ground glow as wide as the light reaches there and a
  halo at the light, following its keys.
*A fresh review* (0 critical, 4 important, 5 minor) - fixed: a radius through 0 (typing "0.8" passes "0") or a
colour through black wiped a light's keys (now they wait and come back from their own scale); the flicker started
and ended up to 30% off the light's colour (a jump into and out of the loop; now each flicker dips from the full
colour and back, its own depth the pattern); a game light's own flags (ambient only, dynamic, priority, fading) were
dropped on Save (now carried: draft lines only off a new light's values; the library export is format 13). Left for
the owner: an effect with only a light (no emitter) can't be played and the game's light-only effects aren't in the
library; five minors (on #145).
*Tests:* test.bat 425/0 (7 light tests in vfx_model_test: the draft both ways, the writer, the ASCII and compiled
readers, the flags); check_effect.js 45 light checks; 55 breaks caught across the tasks and the review. *In game:* L-0 passed on 0.95.3 (owner:
"ok, it light's up" - the ASCII light loads and lights); L-1 (the fireball's own light read into the bench) on 0.95.4.

### Formula recipes take their legs from another animation (2026-11-09, #149)

*Asked (owner):* "most casting animations are very elaborate, especially for cantrips so I was thinking about making a
shorter one, but I'd like the legs to stay mostly the same"; "castout with the combat ready legs"; "go with B" (B: a
`legs` line in formula recipes - not the new mocap bench's sources, #92, which wait with the mocap work). *Built:*
- src/data/anim_recipe: `AnimRecipe::legs`, written only when set (recipes without it unchanged); DeriveAnimClip splices
  the legs half from that animation - NWN's own split (#84): rootdummy's place, `pelvis_g` and everything under it -
  from its start at its own speed, looping over the recipe; the torso half (torso_g down) and the root's turn stay the
  base's (trimmed, sped, mixed, leant); a bone it doesn't key keeps the base's; the crouch bends the legs it gets.
  anim_packs loads it like the mix (a body without it logs "its legs stay the base's").
- The Animation Recipe Bench: **Legs from** in the formula editor, previewed live (public/core.js's port; legs_check.js
  holds the C++ test's numbers).
- data/anims/n5_cantrip.recipe: castout on 1hreadyr's legs, speed 1 (castout is 0.5 s: the long part of a cast is the
  conjure before it - which spells play n5_cantrip, and for how long, is the game session's).
*Tests:* test.bat 429/0 (AnimRecipeLegsRoundTrip, AnimRecipeLegsFromAnother), legs_check.js; 11 breaks caught; on the
real libraries castout + 1hreadyr: the thigh and shin are 1hreadyr's, the torso and right arm castout's at 1.5x.

## Spell & Attack Bench - piece 1 (2026-11-10, #155)

  Owner, 2026-11-10: "I think we will need a spell bench and eventually an 'attack' bench where mocap, VFX,
  spell and perhaps sound will be combined for a final preview of the whole thing". Design (approved):
  docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md; plan: docs/superpowers/plans/2026-11-10-spell-
  bench-piece1.md. Piece 1 is Chromatic Orb end to end:
  - `nwn5e-tools/spell_bench/` (127.0.0.1:8768): the scene (formations), five part cards with a sound each, a
    timeline snapped to the clip's frames, a preview, Save into build\spellbench\looks\ (never build\data\, which
    installs overwrite), Play in game (dev/spell_bench_link.cpp: a staged replay against a spawned formation), and
    `ship.js` (an accepted look into the repo's data/looks/, the draft deleted - the game session's ask).
  - The look (`.look`, src/data/anim_look; the bench's public/look.js writes it byte for byte the same) and its
    player (src/core/anim_look_player): the clip swap and the caster's parts at the cast (its own spell-cast
    listener), the missile / hit / leap / sounds at the impact through `PlaySpellLook`, the rule's one line, which
    returns when the hit lands so the damage waits for it.
  - Six orbs, the game's vpr_ecto*01 flown as its MIRVs: visualeffects 2100-2105, progfx 910-915
    (data/looks/*.look.txt; the game session's row server serves data\looks then spellbench\looks, empty rows only).
  - Per-sound volume (owner, 2026-11-10, via the game session) comes with piece 4: the game's PlaySound has none.

## Spell & Attack Bench - piece 2a: flight paths (2026-11-11, #155)

  Owner, 2026-11-10: "yes, start piece 2 with flight paths"; design docs/superpowers/specs/2026-11-10-spell-bench-
  flight-paths-design.md, plan docs/superpowers/plans/2026-11-10-spell-bench-flight-paths-2a.md (native execution, one
  final review). The proof gated it (SB-21b, passed 2026-11-11).
  - A look's flight: the kind (homing, an arc, straight, rise then dash), its settings, the darts' fan and stagger,
    where it leaves (hand, head, ground) - parts keys, written after leap_beam only when not the default.
  - In game a missile with a flight twin (visualeffects 2190-2199: the orbs, 181, 503; a bench copy by its projectile
    row) flies an invisible carrier along the game's visual-transform easings, with the missile row's own flight sound;
    the hit waits for it. The arrows (233, 245) keep the MIRV until the carrier turns the model.
  - Rule calls: PlaySpellLookDarts (Magic Missile, wired 0.99.17 behind SpellLookFlies), PlaySpellLookAt (Fireball,
    when its rule is written).
  - The bench: the Attack card's flight controls; the scene's darts, spread and "aim at the ground"; the preview flies
    each dart as the game does (public/flight.js, held to anim_flight's numbers).
  - Open: SB-22..25, MMD-1/2; deferred minors on the ledger (darts capped at 8, fan per neighbour - the owner's call).
  - Next: piece 2b, companions and trails.

## Mocap Bench: RGBMocap's BVH takes (2026-11-11, #157)

  Owner, 2026-11-11: "Let's go back to meshing RGB mocap with the bench", then "go with A" - a BVH take reader in the
  bench (B, a pass through Blender for every take, set aside). RGBMocap (toyxyz, installed at E:\toyxyz_RGBMocap)
  turns a video into SMPL-X motion and exports BVH (root Hips, Mixamo-style names, Y up, 30 fps).
  - A recording folder holding a .bvh is a take (a FreeMoCap folder's own files still win). The reader runs the
    skeleton forward per frame and maps the joints onto the body points (LeftArm is the shoulder); the nose and ears,
    knuckles and thumb and the heels are placed from their joint's own turn, so the head's look and the wrist's roll
    carry (fingers ignored: "nwn hands are just fists").
  - A model's take skips the webcam clean-up (stray hands, outliers, swaps, lost limbs); its smoothing starts sharp.
  - Fixtures: a hand-written BVH only - ExMR / GVHMR and SMPL-X's derived data are research and nonprofit use; no
    export goes into git. Whether clips made from these takes may go into the public repo is the owner's call.
  - nwn5e-tools a230df7; test rows RGB-1 (video to BVH in RGBMocap) and RGB-2 (the bench reads it).

## Mocap Bench: legs from the stance, with a keyframed twist (2026-11-12, #159)

  Owner, 2026-11-12: "it's a bit hard to create a seamless attack loop because the legs (and therefore the starting and
  ending position) don't match the selected stance. what can we do to simplify the process?", then "mostly planted, go
  with legs from the stance, but just like we have a "lean" slider in the recipe bench allow for a twist here as well
  that can be keyframed". Plan docs/superpowers/plans/2026-11-12-mocap-stance-legs-twist.md (native, one final review).
  - Assembled recipes (src/data/anim_recipe, anim_assemble): `stance_legs = 1` holds rootdummy's place and pelvis_g with
    everything under it at the stance's first frame on every frame (the stance's root turn folded into the pelvis, as
    for an unkeyed child of the root); the take's leg tracks, foot and hips targets and leg / pelvis pose keys don't
    apply. `twist <t> <degrees> <ease>` (step 8b, after the legs; under the stance's legs only, as the bench writes it):
    the torso turned about the world's up by it, the pelvis by 35%, both legs solved again to keep each foot's place and
    turn; 0 at the clip's first and last frames (0.2 s fades). Recipes without the lines are unchanged.
  - The bench: a new clip starts on the stance's legs (an idle loop on the take's; an older project reads as the
    take's). The Clip tab's legs switch, and under the stance's legs the twist slider keying the marker and the twist
    row; no contact lanes, no foot or hips handles or rows, no foot lines in Save's report, and E / K / Shift+K /
    simplify leave the pelvis and the legs out. Under the take's legs the twist keys stay, hidden.
  - anim_helper: NWN5E_HELPER_OUT builds a branch's helper elsewhere, NWN5E_ANIM_HELPER runs the checks and the bench on it.
  - The core changed: an install, then the bench's helper rebuilt from main (until then the bench can't preview a
    stance's-legs clip). Test rows LT-1 (the bench) and LT-2 (`!anim` in game). Deferred minors on the plan's ledger
    (the held legs 2-4 mm off on ready stances, an unreported foot slide at a large twist on straight-legged stances).

## VFX Bench: area mode (planned 2026-11-12, #161)

  Owner, 2026-11-12: "how can I create new effects for grease or fog or entangle for example?", then, on the options
  (an area mode in the VFX Bench, recommended, or building it straight into the Spell & Attack Bench): "go with the area
  mode, plan it as an issue". An area's look is a vfx_persistent.2da row: its shape and size, up to three effect models
  copied across it (NUMACT, DURATION, EDGEWGHT), a low-detail model, a whole-duration effect and its sounds.
  - Planned on #161, in order: a proof first (Play in game rewriting a reserved vfx_persistent row live, as row 1600
    does for visualeffects, and measuring how the game scatters the copies); the game's area effects in the library;
    the area on the stage; Play in game; Save; checks, the VFX Bench's first browser check among them.
  - Recon: the fogs and Grease are particles; Entangle adds 6 flat textured planes; Web and Darkness are a
    whole-duration effect only.
  - Waiting on the owner: Q1, whether the bench draws Entangle's planes (static, shown only); Q2, whether a saved area
    ships through the looks overlay (vfx_persistent rows in a bench range) or through Claude adding rows.
  - *Built (2026-11-13; plan docs/superpowers/plans/2026-11-12-vfx-bench-area-mode.md, native, one final review):*
    - The proof (VA-1 passed, "the one copy showed too"): vfx_persistent lives at the rules' 2DA arrays +0x120, read
      for each new area, so the dev layer rewrites row 199 live; the client's scatter (delays 0.3 / 0.4 / 0.35 s per
      slot, re-placed after the lifetime, the rim's share on it, inside ~0.5 m from it) is what the stage copies.
    - src/data/vfx_area: an area's row columns (sizes 0.6-100 m: the game divides by the size x 100 when placing a
      copy, so a tiny area crashes it), the .area draft (twin: vfx_bench/public/area.js), the bench's request, unique
      library names (NWN5E_FOG_CLOUD_1..9). src/data/vfx_model: plain meshes kept as planes (Entangle's vines) through
      the compiled and ASCII models and the draft. src/data/look_rows: vfx_persistent 200-299 (not the scripts).
    - The VFX Bench: the Areas tab (the game's, yours), the area on the stage at its size, Play in game, Save (each
      slot's effect its own saved draft, the row served at the next start), ship_area.js; `!vfxarea row <n>`.
    - The final review's six Important findings fixed with tests; deferred minors on the plan's ledger (a Stop within
      0.3 s of a Play missing the area, float-vs-double rounding at the fourth decimal, planes without their fade).
    - Test rows VA-2 (the areas on the stage and in game), VA-3 (a saved area after a restart).

## The benches' frame and timeline (2026-11-15; spec docs/superpowers/specs/2026-11-15-bench-frame-timeline-design.md)

  Owner, 2026-11-15: "we need to make the benches in general more user friendly. the previews shoudl always be visible,
  organizing timing for the effects/attacks or major motions for the mocap should be first and foremost", then "the beats
  in time first, then the look of each piece", then, on the layout mock-up: "looks right, go with that layout for all
  benches". Three benches on one page frame: a bar on top, an always-visible stage, a side panel that scrolls on its own
  and a timeline along the bottom, with splits the page remembers.
  - *Built (2026-11-15; plan docs/superpowers/plans/2026-11-15-bench-frame-timeline.md, spec as above; nwn5e-tools, 13 tasks, each reviewed):*
    - common/ (nwn5e-tools): frame.js + frame.css (the layout, the drag splits, double-click to reset, sizes kept per
      bench in the browser, the page never scrolls), the theme tokens (light and dark), history.js (undo: last 100
      steps), timeline.js (the core: zoom about the pointer, Shift+wheel pan, F fit, snapping) and its canvas widget,
      and /common/ served by all three servers and in their stale-code check.
    - VFX Bench (vfx_bench/public/beats.js, app.js): the frame; a beats lane (a row per beat, bars dragged to move or
      stretch Starts after / On for); copies added together form a group (`glow`, `glow2`...) with one bar and shared
      settings (a differing value shows "mixed"); the panel is what is selected on the timeline; Undo / Redo (Ctrl+Z,
      Ctrl+Y); area mode on the frame (a row per slot with a model).
    - Spell & Attack Bench: the frame (the scene panel is the drawer) and the shared timeline in place of its own
      (five lanes, dragging the release, a sound or the Cast VFX bar), Undo / Redo in the bar.
    - Mocap Bench: the frame (no drawer); #89's fixed-height timeline box kept - rows scroll inside it, so the stage
      never moves under a drag.
    - Test rows BF-1 (VFX Bench, #164), BF-2 (Spell & Attack Bench, #155), BF-3 (Mocap Bench, #89).
  - *The final whole-branch review (2026-11-15: "ready with fixes"), fixed in one wave (nwn5e-tools 9596dc8..050fd3e),
    each with a check step watched failing first:*
    - F1: the timeline's ruler and header stay pinned while its rows scroll (common/timeline.js, VFX and Spell); the
      VFX Bench's rows scroll in a box of their own, so the transport and "+ new beat" never scroll away.
    - F2: Ctrl+Z / Ctrl+Y undo with a dropdown, slider or checkbox focused (`BenchHistory.typingIn`: only typing in a
      text field keeps the browser's own undo); Space and R keep their rule.
    - F3: renaming an emitter through another's name (`x` -> `glow3` past `glow`) leaves that emitter its target node
      and lights.
    - F4: the VFX status line shows a long message whole, on a row of its own in the bar.
    - F5: every bench's check measures at the 1100 x 700 minimum (no page scroll, the stage at least 320 x 240, the
      transport and ruler in view).
    - F6: the Spell & Attack Bench's timeline starts at 235 px, so all five lanes show.
    - F7: the Spell & Attack Bench snaps a drag once (the widget's 6 px), no second 0.05 s pull onto a key.
    - F8: the Mocap Bench's check gains the spec's stage-stays-in-view step.
  - Rulings (the plan's ledger, R1-R17):
    - R1: the timeline's onEdit is `onEdit(label, edit = {row, item, start, end}, phase)`; the bench applies the edit.
    - R2: spell_bench/public/timeline.js is no longer loaded but not deleted - the owner's OK first.
    - R3: work committed on nwn5e-tools main, no branches. R4: the .superpowers workspace is excluded locally.
    - R5: the panel's size variable is `--panel-size` (`--panel` is the colour token).
    - R6: rowsOf(fx, duration, hidden) takes the page's set of hidden emitters for the row's muted flag.
    - R7: a body move passes only `start` (retime sets an end only on an end-handle edit), so an always-on beat
      stays always-on when moved.
    - R8: the old Emitters list card is left hidden, not deleted (owner's OK first).
    - R9: a beat's remembered look was per open effect, cleared when another opened. Changed by the owner, 2026-11-15
      ("let's try your suggestion, if it feels off we'll make changes"; nwn5e-tools 1411581): remembered for the
      session, keyed by the effect (its library name, each New effect its own, an area's slot) and the beat - another
      effect's same-named beat has its own; Save carries them to the saved name; dropping unsaved changes or Apply the
      text forgets them.
    - R10: the browser fixture area VFX_PER_TRIPLE (three model slots) tests "a row per slot".
    - R11: the spell timeline snaps to key times within 6 px, else the 0.05 s grid (today's rule), not strictly to frames.
    - R12: the five spell lanes scroll inside the timeline box when they don't fit (all five fit at the default since F6).
    - R13: seeking only from the ruler; a click on an empty track selects nothing (as in every bench).
    - R14: the Mocap timeline's first height is round(clamp(0.30 x window height, 180, 280)), the old box's look, within [160, 420].
    - R15: no issue exists for the frame, so BF-1 files under #164, BF-2 under #155, BF-3 under #89; opening one is the owner's call.
    - R16: BF-3 makes a row appear (an IK key) and checks the stage, box and timeline keep their sizes; a contact adds no row.
    - R17: the final review ran while Task 13's test rows were being fixed (the rows were outside the code it read).
  - Deferred minors (the plan's ledger had the full list):
    - Timeline core and widget: pointercancel commits the partial edit; double-click toggles a row label twice; wheel
      ignores deltaMode; wheel is swallowed over the tracks (rows scroll from the gutter only); every draw (now every
      scroll too) reallocates the canvas and reads the theme 8 times; hover can be a frame stale after a scroll; a
      bar under ~10 px can only be trimmed, not moved; opening another effect / area / look keeps the old zoom.
    - Frame and servers: serveCommon is copy-pasted in three servers (a shared helper would avoid drift); %00 in a
      /common path may 500; /common/checks/* is served too; the frame check drags only the panel split;
      common/checks/run.js doesn't run timeline_browser.mjs / frame_browser.mjs; window resize redraws twice; a long
      status row plus a tall saved timeline at 700 px high can shrink the stage under 240 px.
    - VFX: a one-shot's bars and ends can be dragged past Seconds, and On for clears at the timeline's end, not at
      Seconds; a group's start-handle drag gives every copy the group's latest end; zero-padded names group oddly
      (`spark02` joins `spark`); backwards scrubbing restarts the sim on every pointer move; "+ new beat" reports the
      uncapped playhead time; the 10 s cap squeezes copy offsets; looks are not dropped on Remove or kept on rename;
      Copy is refused for names ending in a digit; Undo does not clear sim.hidden or the beat looks; the area playhead
      wraps at the play length; areaRowsLocked can't fail on the lock; the default timeline (200 px) shows ~3 rows -
      drag the split taller (remembered); VFX and Spell keep their own status code rather than frame.setStatus.
    - Spell: a fast Undo during a clip load can leave two loads in flight; muted sound lines are not in the undo
      snapshots; clicking a lane's label or empty track doesn't open its card (its bars do).
    - Mocap: the default timeline now holds the transport too, so its rows get ~49 px less than the old box at first
      look (the owner to judge); New's name error says "letters" though names are lowercase only.
  - Follow-up: #165, condition looks (a saved VFX Bench effect shown while a condition lasts; Poisoned's green first),
    to be planned next.

## A saved VFX Bench effect shipped as nwn5e's own: Detect Magic's glow (2026-11-16, #39)

  Owner, 2026-11-16: "Can we use a vfx effect for the detect magic glow?", then "option 1, I'll make it in the bench";
  on large creatures, "wouldn't a smaller area be obscured by the creature's body? but let's test it". The first saved
  bench effect shipped as a game effect - the way #165's condition looks will go.
  - The owner's n5_detmagicef (a lasting effect on the root: star motes rising from a patch, a soft flickering light) in
    the four model columns of one visualeffects row - S / M / L / H, the game picking by the target's size: M as made,
    S smaller, L and H wider, taller and denser (area x0.55 / 1.7 / 2.5, the light's reach the same, rate x0.6 / 1.8 /
    2.8) so a large body doesn't hide it. data/rows/visualeffects.detect.txt row 2092 NWN5E_DETECT_MAGIC; models
    data/files/n5_detmag_<s|m|l|h>.mdl written by the game's own writer (WriteVfxModel, via mdl2draft --model) from the
    drafts tools/data/vfx/n5_detmag_<s|m|l|h>.vfx. The mechanics session points Detect Magic's $GLOW at it (0.99.46).
  - Rows: nwn5e's visualeffects rows live in data/rows and data/looks - the looks own 2100-2199; 2100 at first collided
    and the launch check dropped the looks' whole file, so 2092 (2093-2099 kept for the other Detects).
  - L4-29 passed (owner: "looks good, both the detect magic and the silent image"). Open: which column the game uses for
    an item on the ground or a placeable (all four are filled, so it shows either way).

## The VFX Bench's Composer and mesh beats, built (2026-11-17, #39)

  Spec docs/superpowers/specs/2026-11-16-vfx-bench-composer-design.md (its "Addendum 2026-11-16: a general Composer"
  wins); plan docs/superpowers/plans/2026-11-16-vfx-bench-composer.md (17 tasks, each reviewed; the ledger under
  .superpowers/sdd/2026-11-16-vfx-bench-composer/).
  Owner, 2026-11-16, on the Floating Disk's piles: "If I don't like the compositions, is there a way to compose them
  myself?"; then, on the design: "let's do something here, instead of buiding something specific for the disk, let's
  build something more versatile that will allow other things to be built. The disk doesn't need to be rendered."; "I
  think placeables and meshes for spells"; "go with A and those outputs" (a mode of the VFX Bench, not a separate Model
  Bench); "go for all 17".
  - *Built (anim 88a2ea6..d329e3a, nwn5e-tools 8a975ad..3cd3e86):*
    - The generators (tools/data/, anim):
      - floatdisk.js loads in a page (window.FloatDisk);
      - a part's model may be a weapon's three pieces;
      - compositionText writes composition.json in its own layout;
      - reader.js holds the part and texture reading build.js and the bench share;
      - build.js --json prints the summary as one line;
      - files.js holds one ownership rule for both build.js files;
      - composer/ (project.js, build.js, check.js, check_build.js): a placeable's model, its .pwk (the use point;
        Blocks as a ground mesh of the guide's outline, material 7) and its rows, from 14100-14199; a spell mesh's draft,
        into the bench's vfxedit\saved\.
      - The disk's 13 files are unchanged (check_build.js).
    - src/data/vfx_model.* (A4, anim d329e3a):
      - a plane keeps alpha keys per animation (`<anim>_alpha`), and, for the bench, its mesh, scale, Starts after and On
        for;
      - WriteVfxModel writes the keys as `alphakey` on the plane's node, and LastKeyTime counts them;
      - test.bat 564 -> 568.
    - The VFX Bench (nwn5e-tools):
      - composer_server.js: the checkout (--ee) and its projects; the parts list (the game's 1268 placeables and 903
        items, your own files); a part's meshes through reader.js; textures, DDS first; thumbnails, kept in the build
        folder; Save per kind, all or nothing.
      - The Composer mode (public/composer.js, composer_page.js): projects and variants, the guide, a placeable's card
        (use point, walk, copied row), the parts list with thumbnails, the handles and resting, undo across variants,
        Save and build, Reload, Play in game.
      - DDS textures: core.decodeDds, DXT1 and DXT5, under Bioware's header and the standard one.
      - Mesh beats in the effect editor: + mesh, a row and a card as a light's (parts.meshKeys), and sim.planesAt, the
        one planes path.
  - *Rulings worth keeping (the ledger has them all):*
    - F1: composition.json has two numbers JSON writes otherwise (the rim's `1.0`, the large stage's `0.0`). The first
      Save rewrites both: a one-time diff to commit.
    - F17: one sim.planesAt, through the chunks' mesh path, for effects and areas. Every effect's own planes draw in the
      effect mode, the game's too, not only mesh beats.
    - F23: the Composer opens on the first variant that takes parts (the disk's Small).
    - A5: a change to the checkout's generator parts refuses only the Composer's routes; the effect mode goes on.
    - A8: check.js compares a draft as the game reads it - CRLF as LF, a generator's draft by its body under its mark
      line.
    - A10: the Composer button asks nothing and keeps the effect; entering it clears the history; one Backdrop for
      both modes.
    - A11: a weapon is laid flat by turning its thinnest side vertical (the game models them lying on an edge).
    - A12: resting over overlapping parts lifts them (the design's call 11).
    - A13: a save is never undone: the rows and models it gives are no undo step.
    - A14: mesh textures ask the game first (its DDS, else its TGA), the library only when the game has none; the
      particles keep the library's. Assumed, not checked in game: the game draws a DDS over a TGA of the same name
      wherever each is.
    - A16: a plane with no keys at its own alpha 0 draws (Entangle's vines); only a plane its keys fade is hidden by
      its alpha.
  - *Visible changes in the effect mode, for the owner:*
    - every effect's planes draw, textured and shaded as chunks, under their parents - a game effect's too (Entangle's
      vines; Mage Armor's shields, solid, since the library keeps none of their keys);
    - in area mode the planes are shaded, not flat white, follow their parents' turns (Wall of Blades' swords now sit
      where the model puts them), and a render 0 one isn't drawn;
    - mesh and plane textures come from the game first; a see-through chunk mesh draws after every opaque one;
    - a lasting effect's always-on mesh beat with a Starts after and a Fade out shows in the end if the spell ends
      before its start (a light does the same).
  - *For the owner in the Composer:*
    - a thumbnail drawn before DDS was read stays as it was: delete `<build>\vfxedit\composer\thumbs\` once;
    - with Rests on, a nudge lifts one of the Floating Disk's Medium parts onto its neighbour (they overlap): untick
      Rests to nudge in place;
    - the first Save of the Floating Disk rewrites the two numbers (F1).
  - *For the game:*
    - the next "anim ready" names A4's src/data/vfx_model.* change. A mesh beat fades in the game only after the game
      session's install of a DLL built from it; before, it shows for the whole effect;
    - the generators' changes build nothing new for the disk.
  - *Open:*
    - walking into a Blocks placeable and using it in game (A3's ruling) waits for a way to put one in an area: no
      blueprint (UTP) is ours, and no chat command places a placeable;
    - for the final review: the thumbnails keep every decoded texture and about 2,200 object URLs for the page's life;
      Ctrl+Z during a drag undoes the previous edit and drops the drag;
    - one spell mesh's save reads singular ("Saved your mesh ..."): the owner to confirm;
    - deferred minors on the ledger.
  - Test rows CO-1 (open it: the Floating Disk's Small), CO-2 (build: the parts list and its thumbnails, a drag from
    it, the handles, stacking), CO-3 (the onion skin, undo across variants), CO-4 (a placeable saved and built), CO-5 (a
    spell mesh as a chunk and as a mesh beat), CO-6 (Play in game, and what differs from the installed result).

## The Composer's dyed parts: PLT textures baked (planned 2026-11-17, #39)

  Owner, 2026-11-17: "helms don't have textures? Not in the bench and not in-game". After the explanation - a PLT is
  NWN's dyeable texture: each pixel is a shade and a layer, painted from an item's or a creature's colours; placeables
  and effects have none, so the game binds no texture; helm_001/002/003/011/012 have their texture only as a PLT - "go
  with A, per-part colours" (also a #39 comment).
  - *Decision A:*
    - a part whose texture exists only as a PLT is baked: painted with the game's palettes into an ordinary texture of
      its own, shipped beside the model (placeables, spell meshes, the Floating Disk);
    - the VFX Bench draws the same baked picture;
    - each such part gets colour fields in the Composer's part card (Metal 1, Metal 2, Leather 1, Leather 2, Cloth 1,
      Cloth 2, as palette numbers) with neutral defaults;
    - test-first.
  - *Spec and plan:* the "Addendum 2026-11-17: PLT parts baked" in docs/superpowers/specs/2026-11-16-vfx-bench-composer-design.md
    and in docs/superpowers/plans/2026-11-16-vfx-bench-composer.md. Four tasks:
    - P1, the bake in floatdisk.js;
    - P2, the generators bake (reader.js, files.js, both build.js);
    - P3, the server;
    - P4, the page.

    About one session.
  - *What the game's files say:*
    - a PLT is `PLT V1  `, then its width and height, then a shade and a layer a pixel, rows bottom-up;
    - the palettes are seven 256 x 176 TGAs: a colour is a row from the top, a shade a column, and the alpha is shine;
    - 34 of the parts list's 2,171 entries are dyed: 33 helmets and gi_cloak01.
  - *Calls:*
    - one bake (floatdisk.js), used by the generators, the server and the page;
    - a baked texture is named `n5p_` and a 12-digit hash of its PLT and colours (helm_001 at the defaults:
      `n5p_2cd3d725dedb`);
    - the defaults: Metal 1 0, Metal 2 3, Leather 1 3, Leather 2 2, Cloth 1 22, Cloth 2 23;
    - only colours off the defaults are written (`"colours": { "metal1": 5 }`);
    - the page bakes from the PLT and palettes the bench serves; Play in game bakes on the server first;
    - no dev-layer or game change.
  - *Where the baked textures live:* the bench build's `vfxedit\saved\`, made from the owner's own game, which the dev
    layer serves at every start; never a repo. A baked texture is art made from the game's files, which CLAUDE.md never
    commits.
  - *Open, the owner's:* Q3. Keep them in `vfxedit\saved\` (a build without the dev layer draws dyed parts untextured),
    or commit them in `data\files` against that rule? The disk's committed models already carry the game's meshes.
  - *Limits:* matte metal (no shine); metal 2's palette is the documented one, to be confirmed in game.
  - *Built (2026-11-17; anim 7b3adaf, 65d560d; nwn5e-tools 7f979ad, 73f5448, ad07fc8):*
    - P1 (anim 7b3adaf): floatdisk.js has a part's `colours` (readColours, cleanColours, fullColours), the PLT reader,
      the bake (bakePlt), its name (bakedName), the TGA writer (tgaBytes) and bakeBitmaps. It loads in a page.
    - P2 (anim 65d560d):
      - reader.js's pltOnly and bake;
      - both build.js bake a dyed part's texture beside the models they write (files.js bakeParts), and the models name
        it;
      - build_derived.js carries the bakes into every build;
      - the Floating Disk's Large is rebuilt: its helmet names `n5p_2cd3d725dedb`.
    - P3 (nwn5e-tools 7f979ad, 73f5448):
      - the texture route serves a PLT as itself (X-Texture: plt), and the palettes;
      - POST /api/composer/bake bakes Play in game's textures;
      - Save passes --saved only for a spell mesh;
      - the thumbnails are kept in `thumbs\v2\`;
      - the Composer's checks build a fixed test recipe in their copy.
    - P4 (nwn5e-tools ad07fc8), the page:
      - it renames a dyed part's texture as the generators do and bakes it itself, for the stage, the ghosts and the
        thumbnails (the page's bake of helm_001 equals reader.js's);
      - the part card's Colours: six palette numbers with swatches, live as you type, one undo step a change, the
        defaults never saved;
      - Play in game posts its bakes before the request, and its draft names them as the built model does;
      - Save's status names the bakes;
      - in the effect mode, a mesh that names a PLT itself is drawn baked at the defaults;
      - checks: check_composer.js +3, composer_check.mjs +8, among them that floatdisk.js runs before project.js in the
        page.
    - *Where the bakes live:* the owner's option 1 (2026-11-17), which answers Q3 above. A bake is never committed.
      - A Save bakes it beside the models in the checkout's `data\files` (git-ignored: `/data/files/n5p_*`).
      - build.bat's build_derived.js bakes it again for every build, so an install has it.
      - A spell mesh's bake goes in the bench's `vfxedit\saved\`; Play in game's goes in `<build>\vfxedit\`.
      - The "Where the baked textures live" paragraph above (`vfxedit\saved\` only) is superseded.
    - *The defaults:* Metal 1 0, Metal 2 3, Leather 1 3, Leather 2 2, Cloth 1 22, Cloth 2 23 (helm_001:
      `n5p_2cd3d725dedb`). Only colours off the defaults are written (`"colours": { "metal1": 8 }`).
    - *Limits:*
      - metal is matte: a palette's alpha is shine, and the bake draws none;
      - metal 2's palette is pal_armor02, as documented, still to be confirmed in game;
      - Play in game's bakes and a spell mesh's reach the game through the dev layer only. The Floating Disk's and a
        placeable's reach it through the build.
    - Test rows: CO-8 (the bench: the helmet textured, its Colours, the thumbnails), CO-9 (Metal 1 gold, Undo, Save),
      CO-10 (Play in game), CO-11 (installed).

## The Composer's shapes: the dome (2026-11-17, #39)

  Spec: the design's "Addendum 2026-11-17: basic shapes" and "Addendum 2026-11-17 (b)" in
  docs/superpowers/specs/2026-11-16-vfx-bench-composer-design.md; plan docs/superpowers/plans/2026-11-16-vfx-bench-composer.md
  (S1-S5; addendum (b) amends them).
  Owner, 2026-11-17: "is it possible to use basic 3d geometry for VFX? ... I was trying to create something for the Zone
  of Truth watch is a half sphere area and I wanted to have a translucent sphere so the player could see the affected
  area", then "go with the shapes, start with the dome". On Q4-Q6: "1. hide them, we want the bench to reflect the game as
  much as possible 2. no, only exploring visuals for now 3. wait". On "is it possible to project texture on the surface of
  the 3d mesh?": "go with your recommendation".
  - *Calls (short; the design has them all):*
    - a shape is a part of a fourth source kind, `"shape": { "type": "dome", "radius": 4.57, ... }`: no scale, no dyes,
      only what isn't a default written;
    - one implementation, floatdisk.js: its mesh, its material and its soft texture, for the page, reader.js, both
      generators and build_derived;
    - the dome: radius (a new one the guide's), height (default its radius), smoothness 32 (8-96), both sides by default
      (one mesh, each side its own vertices, the inside first), its bottom open;
    - its look: Mapping Wrapped (Repeat around 1-32, Repeat up 1-16) or From above; Colour `#99ccff`, Opacity 0.35, Glow
      1; Texture Soft, None or a texture from the bench's library (the game's, by name: nothing copied);
    - the soft texture: `n5s_` and 12 hex digits of its colour (the default `n5s_8c757fb28f99`), 128 x 128 with alpha,
      generated from numbers (original). Build output beside what names it, never committed: data\files is git-ignored
      and built by build.bat (build_derived); a spell mesh's goes in vfxedit\saved\, Play in game's in vfxedit\;
    - the bench draws only front faces, everywhere, and tiles mesh textures past 1, as the game does;
    - no game-side change.
  - *Built:*
    - docs: anim 5d390a6 (the design and plan addenda), 02882fe (addendum (b): the owner's answers, Mapping, the repeats,
      a library texture);
    - S1 (anim fa2ca10, 0ebd84b): floatdisk.js's shape tables, readShape and cleanShape, the dome's mesh and material,
      its soft texture and name; a part may be a shape;
    - S2 (anim afd705a, d9983b7): both generators build a shape's mesh and write its soft texture beside what names it;
      build_derived carries it; a PLT-only library texture refused;
    - S3 (nwn5e-tools 733a12d, 4b31661): the texture route serves `n5s_`, POST /api/composer/bake makes Play's;
      composer.js places a shape with no part request, cleans it, adds it sized to the guide;
    - S4 (nwn5e-tools 15d7f34), the page:
      - the parts list's Shapes tab (its thumbnail drawn on the spot, never kept);
      - a shape's card: the fields from floatdisk.js's SHAPE_INFO, each one undo step, refused out of bounds; no Scale and
        no scale square; Texture From the library... through the emitter's texture picker;
      - its soft texture made in the page for the stage, the ghosts and the thumbnails; Save's status names it; Play in
        game makes it first;
      - view.js: front faces only (cullBack) and repeating mesh textures (a picture not a power of two stretched first);
      - the view buttons fit a big variant (frameBox, up to 40 m);
      - composer_check.mjs +10 (88 steps); browser_check.mjs unchanged (95: no step relied on a mesh's back).
    - S5 (anim d9ee874; nwn5e-tools 8272de9), the other shapes:
      - floatdisk.js: Sphere, Cylinder (open), Cone (point up), Disc (flat, on the ground), Ring (flat band) - one turned
        profile for every type (the dome's mesh as before), each both sides, the inside first, its base at z 0, each with
        Mapping and the repeats: Wrapped from its base up, a disc's and a ring's from the rim in (the ruling below); a
        sphere's, a cylinder's and a cone's Height twice the radius, a flat one none; a ring's Inner radius 0.85 of the
        radius to the millimetre, always below it;
      - composer.js liftOf: a disc or a ring rests 2 cm up wherever the page rests a part (an add, a move, a nudge, Drop
        it, Duplicate), as the vision cones were lifted (PLAN 0.31.1); the page's card refuses an Inner radius at or past
        the Radius;
      - floatdisk check +16 (68: a check a type, and one a type for each mapping), check_composer +1 (38),
        composer_check.mjs +1 (89 steps: otherShapes); browser_check.mjs unchanged (95).
  - *The owner's answers:*
    - Q4: front faces only, everywhere the bench draws a mesh (the stage, area mode, the Composer, the thumbnails). A
      game mesh's back no longer shows in the effect mode either.
    - Q5 (Zone of Truth's rules): no, for now - the recipe stays a recipe; open for later.
    - Q6 (a texture of the owner's own, a glyph): waits for the dome's in-game test.
  - *Limits:*
    - the bench shades a textured shape by its faces' turn; the game lights it by its glow;
    - a 4.57 m dome reaches through low ceilings and walls, and uneven ground cuts its ring;
    - a big shape round other parts takes the click: pick those by their chips;
    - untested in game: the draw order against other see-through things (transparencyhint on planes would be a small,
      announced game-side change), and whether the game tiles a texture past 1 as the bench does (SHP-3 step 4: Repeat
      up 2 on Soft - tiled, a second bright band starts halfway up; clamped, the upper half stays evenly faint);
    - if the game's loader welded vertices by position, a two-sided mesh's normals could cancel (dark at a low glow);
    - Play in game's effect follows the character: `!vfxarea <effect> 0.6 1 0` puts it down for 60 s;
    - a library texture's own alpha isn't known to the generator: a textured shape is always drawn see-through;
    - no scrolling texture (a later task, after the in-game test: our writers write no UV animation);
    - the 2 cm is the Composer's: a flat shape typed or lifted back to 0 sits at the ground's own height;
    - a sphere, a cylinder or a cone of radius over 50 m has a default Height past the field's 100 m (it builds).
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing for the DLL - the "anim ready" for S1, S2 and these
    docs carries tools only.
  - *Next for Zone of Truth (the design's recipe):* the owner tries the look (SHP-1 to SHP-4). If they like it and Q5 turns
    to yes, the animation session ships `n5_zotruth` as Detect Magic's glow was (its draft, its model by WriteVfxModel,
    its texture, a visualeffects row of type D at a free row), and the game session builds the spell: its spells.2da row,
    a vfx_persistent area of 4.57 m whose DurationVFX is that row, the Charisma save, 10 minutes. S5 added the sphere,
    cylinder, cone, disc and ring.
  - Test rows SHP-1 (the bench: the dome, its card, inside and out, Repeat up, From above, a library texture, then saved),
    SHP-2 (a mesh beat fading in), SHP-3 (Play in game, and Repeat up 2 in game), SHP-4 (`!vfxarea` at your feet), SHP-5 (the
    bench: the other five shapes, the flat ones just above the ground, a cone tilted onto its side).
  - *The ruling on S5 (anim, the commit carrying this line; nwn5e-tools b1ccbfd):* a disc's and a ring's Soft glows at
    its rim (a ring's outer edge), fading in - the area's edge, as a dome's bright ground ring: their Wrapped v runs
    from the rim (0) in, as S5's table had it (addendum (b)'s "from the middle out" was a wording slip). The card's
    note says "brighter towards its rim"; Smoothness's tooltip no longer speaks of a dome only.
  - *The review of S4 (nwn5e-tools 1ef7da0):* a shape field past its bounds is refused as typed, never rounded
    into them first (Opacity 0.015 was saved as 0.02); the rows made self-contained.

## The Composer's textures tinted (2026-11-18, #39; M1)

  Spec: docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md ("Tint", and "Addendum 2026-11-18
  (a)": the owner's answers); plan docs/superpowers/plans/2026-11-18-mesh-texture-animation-and-tint.md (M1 of five: then
  the moving textures, M2 - TMO-1 - M3, then a mesh beat's keys, M4 - M5).
  Owner, 2026-11-18: "for the existing textures I'd like to add tinting options to them as right now when using a texture
  there's no way to change the color"; on Q1-Q4 as put with recommendations, Q3 (Glow on a library texture) among them:
  "accept your recommendations".
  - *Calls (short; the design has them):*
    - a part's `"tint": [r, g, b]` (0 to 1) multiplies every one of its meshes' ambient, diffuse and selfillum. The game
      lights a textured mesh as texture x (selfillum + ambient x the area's light + diffuse x the lights), clamped to 1,
      so the material tints the texture - vim_cntglobe's blue glyph globe is a grey texture (fxpa_symbl_003) so tinted.
      White is none and never written; a dyed part is tinted over its dyes; a shape takes none (its Colour is its
      colour);
    - one place: floatdisk.js's placePart tints (tintMeshes), so both generators, build_derived and the page get it;
    - a shape's library texture (Q3): selfillum its Colour x Glow, ambient and diffuse its Colour x (1 - Glow) - Glow 1 is
      the texture x the Colour in any light (ambient = diffuse = Colour on top of the glow washed a pale colour to white
      by day). Soft and None as they were;
    - the stage draws a textured mesh as texture x min(1, selfillum + diffuse x its shade): the game's formula, the
      stage's one soft light standing for the scene's. An untextured mesh as before;
    - no game-side change: the writers already write a mesh's material (meshLines, the draft's planes). `git diff --stat
      -- src` on anim: empty before and after.
  - *Built:*
    - anim (the commit carrying this paragraph): floatdisk.js readTint, isWhite, tintMeshes; readPart's tint (a shape's
      refused, each error naming the part); placePart tints; compositionText writes tint after colours, never white;
      shapeModel's library texture lit by its Glow. project.js projectText likewise (colours and tint in its part keys).
      floatdisk check +6 (78), composer check +1 (19), composer check_build +1 (a crate tinted [0.5, 1, 0.5], its model
      and its draft: r and b halved, g kept; the library dome's material now its Glow's); the Floating Disk's 13 files
      unchanged (no part has a tint), derived check unchanged;
    - nwn5e-tools (3bb1831): composer.js clean (the tint to 0.001, white and a shape's dropped); view.js meshData's
      textured formula (a ghost still its shade alone); the part card's Tint and None (any part but a shape, one undo
      step); the shape card's note on a library texture; the hook's drawn.colours (each stage mesh's vertex colours)
      and parts' tint. check_composer +2 (41), check_chunks +1 (31: the textured formula, and a game mesh as before - the
      shades worked out from the faces), composer_check.mjs +2 (93 steps: partTint, libraryTintShows), browser_check.mjs
      unchanged (97 steps, all passed: no step relied on a textured mesh's colour being its shade alone).
  - *What changes on the bench:* a game mesh whose diffuse is under 1 draws a little darker than before, as its material
    lights it (each game mesh carries its own: the rock chunks 0.698, the tiles 0.776, the bones 0.66; 0.8 is the draft's
    default, not the game's) - the Composer's parts, the thumbnails (drawn again: `thumbs\v3\`), and the effect mode's
    chunks alike (one mesh path); a draft's plane at that default is drawn at full, as the game draws it (the review,
    below); a Soft shape at Glow from about 0.45 up draws flat - unshaded, glow plus light at full every way it faces - on
    the stage and in new thumbnails, as the game's clamp at 1 draws it; a library texture's dome shows its Colour (S4's
    limit "the bench shades a textured shape by its faces' turn" is now the game's formula under the stage's light).
  - *What changes in game:* a shape's library texture saved earlier at Glow 1 looks a little darker and more coloured
    (Q3 says so) once its spell mesh is saved again - an effect's mesh beat keeps the old material until then and until
    the beat is refreshed from its mesh (n5_zot's n5m_test2, fxpa_web, still has ambient and diffuse 0.6 0.8 1 everywhere:
    Save and build n5m_test2 in the Composer, then Refresh from the mesh on the beat); a part with no tint builds byte for
    byte as before.
  - *Limits:* the stage's one light stands for the scene's - exact at Glow 1 (selfillum alone), close below it (the
    game's lights, its night and its gamma aren't the stage's); a part's thumbnail is its model's, untinted. The stage
    takes an area's ambient as 1 for an untextured mesh and 0 for a textured one (a follow-up from the review, not
    changed here).
  - *M1's review (anim, the commit carrying this line; nwn5e-tools 7db86e0):*
    - a draft's plane at the draft's default material (ambient 0.2, diffuse 0.8, compared as %g writes them) is written
      by the game's model writer at full, 1 1 1 (WriteVfxModel, src/data/vfx_model.cpp; pinned by
      tests/data/vfx_model_test.cpp) - the game never draws such a plane at 0.8, but M1's formula did on the stage
      (the owner's Entangle Plane01: 0.681-0.970 before M1, 0.545-0.776 after; browser_check's Plane01, one flat plane,
      0.566 under M1 and its shade, 0.708, now). One rule, twice:
      floatdisk.js drawnMaterial (the generators) and core.js drawnMaterial (the bench; composer check compares them);
    - the stage draws a draft's planes by it: the effect mode's and the areas' (sim.js planesAt's, app.js showPlanes)
      and a spell mesh's in the Composer (view.js worldMeshes' `plane`); a placeable's and the Floating Disk's meshes as
      their models are written;
    - on a spell mesh, a tinted mesh at that default is lifted to full before its tint (placePart's `planes`: composer
      build.js, the page's placeOne, so Play in game's draft too) - a tint of 1 1 0.999 gave 0.2 0.2 0.1998, which the
      game draws as it is; now 1 1 0.999. Untinted it's left as it is: every draft keeps its bytes (the game lifts it);
    - the thumbnails' cache moves to `thumbs\v3\` (every entry drawn again once, lit by its material);
    - floatdisk check +2 (80), composer check +2 (21), composer check_build +1 (a tinted default-material file part: the
      spell mesh's draft 1 1 0.999, the placeable's model 0.2 0.2 0.1998 / 0.8 0.8 0.7992); check_composer +1 (42),
      check_chunks +2 (33: the C++ test's plain plane), check_composer_server's thumbCache at v3 (47), composer_check.mjs
      +1 (94 steps: spellMeshPlanesFull - n5_testcrate, no material lines, in a spell mesh at the placeable's colours over
      0.8, tinted the stage's g kept and Play's plane 0.502 1 0.502) and partTint's Play draft carrying the tint,
      browser_check.mjs +1 (98 steps: entangleVinesFullMaterial - each vertex's colour its triangle's shade, worked out
      from its corners through vfxBench.planeMeshes()). The new steps were seen failing on 3bb1831's page first.
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing for the DLL.
  - Test rows (for the board: #39, stream animation):

    | ID | Setup | Do | Expect |
    |---|---|---|---|
    | TIN-1 | VFX Bench (restarted), the Composer: New project `tintdome`, a spell mesh; Parts, Shapes, Dome; on its card Texture From the library..., type `fxpa_symbl_003`, Use; Colour orange (#ff8000); Glow 1 (its default) | Play in game by day, then at night (or in a dark area); then Glow 0.3 and Play in game again, day and night | Glow 1: the glyph orange - the texture times the Colour - the same by day and night, never washed to white by day; the stage's dome orange too. Glow 0.3: orange, darker at night |
    | TIN-2 | VFX Bench (restarted), the Composer: New project `tintbox`, a placeable; Parts, Placeables, `plc_a03` (a crate) added | its card's Tint pale green (#80ff80): the stage's crate turns green; Play in game; Save and build; after the next install (build.bat), in the toolset a placeable of its appearance (N5_Tintbox, its row 14100-14199) put in an area, and the module played | a pale green crate beside the character (Play) and in the area (installed), its wood's grain kept; Ctrl+Z on the bench takes the tint off |

## A shape's Blend: additive soft textures (2026-11-18, #39; M1b)

  Spec: the design's "Addendum 2026-11-18 (b)" (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md);
  plan Task M1b. The owner's test on 0.99.72: a soft texture with `transparencyhint 1` was still cut at alpha 0.2 and hid
  the sparkles; "fxpa_globe works. it's faint, you can see the sparkles AND the texture is animated?"; "go with your rec on
  the blend". Every faint game effect mesh is additive (fxpa_Globe, fxpa_Globe2, fxpa_spots, fxpa_shield ...).
  - *Calls:*
    - a shape's `"blend"`: `"additive"` or `"normal"`; `shapeDefaults` keeps Normal, so a shape saved without it keeps its
      look; the Composer writes Additive on every shape it adds (the coordinator's ruling). Written only when Additive
      (whatever its texture: under a library texture or None it means nothing, but Soft again keeps it);
    - Additive writes `n5s_<hash>.txi` - `blending additive` (floatdisk.js SOFT_TXI) - beside the soft texture wherever
      it's written: files.js shapeWrites (both generators: data\files, --drafts, a spell mesh's --saved), build_derived
      (owned by its name, as the TGA), the bench's bake for Play (vfxedit\). Git-ignored by `/data/files/n5s_*`;
    - the blend is in the soft texture's name (Additive hashes ":additive" after its colour; Normal's names unchanged, so
      every name made before stays) - changing it never leaves a stale TXI beside a texture;
    - the stage draws an additive mesh adding its light (SRC_ALPHA, ONE), no depth written: an Additive shape's, and any
      mesh whose texture's TXI says `blending additive` - the texture route says so (`X-Blending: additive`), so the game's
      fxpa_globe and a saved Additive soft texture draw so in the Composer and the effect mode alike;
    - M1's follow-up: a spell mesh as an emitter's chunk model is drawn by drawnMaterial's lift, as the game's model
      writer writes it (app.js marks its draft, view.js chunkModel lifts it);
    - no game-side change: `git diff --stat -- src` on anim empty before and after.
  - *Built:*
    - anim (the commit carrying this section): floatdisk.js SHAPE_KEYS / LOOK_FIELDS / SHAPE_INFO.blend, shapeDefaults'
      Normal, readShape's error (`<where>.blend: additive or normal`), shapeTextureName by blend, shapeModel's `additive`
      and textures' `blend`, SOFT_TXI, txiAdditive; files.js shapeTextures / shapeWrites / shapeLines / shapeJson and the
      ownership of a `.txi`; build_derived's shape kind for a `.txi`. floatdisk check +6 (86; three of its earlier
      expectations moved: the defaults, SHAPE_KEYS and a soft texture's entry now carry the blend), composer check +1 (22),
      build check +1 (11), composer build check +1 (20), derived check +1 (16);
    - nwn5e-tools (d72b739): composer.js partFrom Additive; the shape card's Blend (Soft only) and notes; the bake route's
      TXI; the texture route's X-Blending; view.js meshPass and the additive pass; the chunk lift. check_composer +2 (44),
      check_chunks +3 (36), check_composer_server +2 (49), composer_check.mjs +1 (95 steps), browser_check.mjs +2 (100
      steps). Each new check seen failing first.
  - *Limits:* the stage's additive is the bench's guess at the game's (SRC_ALPHA, ONE: its alpha scales the light it
    adds); the draw order in game against other see-through things, and whether the game's additive path also ignores
    the mesh's own alpha, wait for BL-1. A library texture's TXI is read through the game's own files (the texture
    route): one the route can't reach (no game) draws as before.
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing for the DLL; a spell mesh saved before keeps Normal
    until its shapes are set to Additive and it's saved again (then Refresh from the mesh on an effect's mesh beat).
  - Test row (for the board: #39, stream animation): BL-1 - a dome at Opacity 0.1, Soft, Additive, with the new effect's
    sparks inside, Play in game: faint and glowing, the sparks seen through it; then Normal: cut at alpha 0.2 as before
    (at Opacity 0.1 all or nearly all of it), whatever's drawn hiding the sparks.

## Moving textures in the generators (2026-11-18, #39; M2)

  Spec: the design's "Texture motion", "Calls I made" and addenda (a) and (b)
  (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md); plan Task M2. The owner, 2026-11-18:
  "for the animated textures: 1. scrolling, scaling (breathing), rotating", "for the breathing and rotating I was
  thinking about the textures on the meshes though"; Q1 route A (an EE material and a shader of ours; a flipbook, B, the
  fallback if TMO-1 fails). NWN:EE has no UV-motion setting (`animateuv` & co. are KotOR's).
  - *Calls:*
    - a part's `"motion"`: `{ "scroll": [u, v] (repeats a second, -10..10), "turn": <degrees a second, -720..720>,
      "breathe": 0..0.9, "rate": <breaths a second, 0.05..5> }`, any part, a shape too: it moves every textured mesh of the
      part. Absent or still: nothing written changes. Written after `tint`, its non-zero keys only (the rate while it
      breathes, and not at its default, 0.5);
    - one maths, floatdisk.js `uvMotion`, round the pivot p - the middle of the mesh's UV box (`uvPivot`): s = 1 + breathe
      sin(2 pi rate t), uv' = R(-turn t) (uv - p) / s + p - scroll t. + scroll moves the picture along +u / +v, + turn
      turns it counter-clockwise (seen from above, From above), breathing grows and shrinks the picture, not the mesh. It
      takes the material's numbers (the motion and the pivot each to 0.001), so the stage and the game can't drift;
    - in game: each textured mesh's bitmap becomes `n5u_` + 12 hex digits of its texture, the motion and its pivot (each
      number to 0.001: one rule with the material's text, so the name fixes the bytes); `n5u_<...>.mtr` says
      `customshadervs vsn5uvmove`, `customshaderfs fslit` (the game's lit fragment shader), `texture0 <the real
      texture>`, and `parameter float` n5ScrollU, n5ScrollV, n5Turn, n5Breathe, n5Rate, n5PivotU, n5PivotV. The game
      attaches a material by its mesh's texture name (its ice tiles, oldf_head, gui_cg_color);
    - `data\files\vsn5uvmove.shd`, ours, committed and listed in floatdisk\made.txt: original text (`motionShaderText`)
      - its first line its mark - with the defines the game's lit vertex shader takes, `#include "inc_standard"` (and
      `inc_uniforms`, guarded), the seven uniforms, `ApplyStandardShader()`, then `vVertexTexCoords` moved by the maths
      with t = `float(worldtimerTimeOfDay) * 0.001` - the game's world clock, as its water's shader takes it. t itself is
      a float32: late in a game day (near 2880 s) it holds to about a quarter of a millisecond, the motion's limit. The
      scroll, the turn and the breath are taken within one repeat, one turn and one breath (`fract`, `mod`), so the
      coordinates stay small and lose nothing more: the same picture, as the texture repeats (floatdisk check runs its
      main as JS against uvMotion);
    - the materials and the shader go beside what names them: a placeable's or the disk's models in data\files (the
      materials git-ignored, `/data/files/n5u_*`), `--drafts`, a spell mesh's `--saved`. A material is ours by its name,
      the shader by its first line or made.txt, and compared as text (line ends aside) as a model is (files.js);
      build_derived carries both. The Floating Disk's own build never calls the listed shader unused;
    - the probe (TMO-1) decides the route before the bench's side (M3): `n5m_uvprobe`, seven faint domes (radius 0.4: 0.8
      m wide; alpha 0.3), 1 m apart along x and clear of the character. The texture0 route, 0.4 m high, x -3, -2, -1:
      1 Soft Additive Wrapped scrolling up (0, 0.25), 2 fxpa_symbl_003 (a glyph, additive by its TXI) From above turning 30
      a second, 3 Soft Additive From above breathing 0.3 at 0.5 a second; the copy route, 0.7 m high, x 3, 2, 1 (no
      texture0: the texture copied under the material's name with its TXI - the soft one made again, fxpa_symbl_003's from
      the game), the same the other way: 4 scrolling down, 5 turning -30, 6 breathing at 1 a second; and 7, x -4, 0.4 m
      high, the texture0 route with the texture's TXI beside the material under its n5u_ name (no texture there):
      fxpa_symbl_003 turning 90 a second. Each side reads scroll, turn, breathe from its outer end in, so a side that
      moves says which it is, and the heights tell the routes apart when nothing does. Every texture is additive (the Soft
      one's n5s_ TXI, fxpa_symbl_003's own), so it also shows whether, and where, a TXI's blending survives the material
      (the owner's 0.99.72 finding: only an additive TXI escapes the 0.2 cut and the depth writing). fxpa_symbl_003, not
      fxpa_globe: the globe's own arturo ripple would muddy the turn;
    - no game-side change: `git diff --stat -- src` on anim empty before and after.
  - *Built (anim 7d092f2; its review's fixes c7c3de1):* floatdisk.js MOTION_KEYS / MOTION_INFO / MOTION_PARAMS /
    MOTION_TEXTURE / MOTION_SHADER, readMotion, hasMotion, cleanMotion, uvPivot, uvMotion, motionName,
    motionMaterialText, motionShaderText, isMotionShader, moveTextures; readPart's and compositionText's `motion`.
    files.js motionMaterials, motionWrites, motionLines, motionJson and the "material" / "shader" ownership; both
    build.js (after the bakes and the shipped textures, so texture0 is the real one; `--json`'s `materials`; the
    Composer's spell mesh counts a material's texture0 as named by its draft); project.js projectText; build_derived's
    kinds; `data/files/vsn5uvmove.shd`; `.gitignore`; `tools/data/composer/uvprobe.js`. floatdisk check +11 (97), composer
    check +2 (24), build check +1 (12), composer build check +6 (26), derived check +1 (17); each new check seen failing
    first. The Floating Disk's 13 files unchanged.
  - *What changes:* nothing for a part without a motion (every model, draft and project byte for byte). The bench's
    Composer doesn't know `motion` yet (M3: the card, the stage, Play in game): a part with one shows and plays unmoved
    there, and only build.js writes its material.
  - *Limits:* untested in game until TMO-1 - our shader compiling and running on an effect's mesh and a placeable's,
    `texture0` binding, the texture's TXI (additive, a library texture's own procedure) applying under the material. A
    moving texture's material forces the vslit / fslit pair, so it drops a game texture's own `.mtr` and any
    `envmaptexture` its TXI asks for. The motion runs on the world clock: it doesn't start with the effect, and may jump
    once a game day when the clock wraps (inferred). Each mesh turns and breathes round its own UV box's middle: a game
    model's meshes that share a texture get a material each (plc_a03's three meshes, two textures: three materials). A
    clamped texture (the GUI's) would wrap where the stage's maths slides it off. A changed shader may need the game
    restarted.
  - *Native option, not built:* fxpa_globe already animates in game by its TXI's `proceduretype arturo` (the game's own
    ripple; `cycle` is its flipbook) - a texture with such a TXI moves with no material at all.
  - *For the bench:* nothing until M3. *For the game:* nothing for the DLL; the probe's files go into the bench build only.
  - *TMO-1's setup:* from the anim worktree, `node tools\data\composer\uvprobe.js --saved ..\nwn5e-ee\build\vfxedit\saved`
    (the bench build's; it refuses any other folder, writes only its own files there - each "new" - and adds
    `n5m_uvprobe` to vfxedit\meshes.txt - "rebuilt": it lists n5m_test already), then restart vfx_bench.bat.
  - Test rows (for the board: #39, stream animation; TMO-2 and TMO-3 after M3):

    | ID | Setup | Do | Expect |
    |---|---|---|---|
    | TMO-1 | The probe written (above); VFX Bench restarted: New, Type Stays (D), + mesh `n5m_uvprobe`, Permanent (until Stop), Name `n5_uvprobe`, Save; the game started, `!test level1ranger` | Play in game; watch each dome; look along the row from each end (the new effect's sparks behind the domes); Stop in game, quit; before the next start, search `Documents\Neverwinter Nights\logs\nwclientLog1.txt` and `nwengineLog.txt` for `Failed to`, `shader`, `vsn5uvmove`, `n5u_` | Per dome: textured, moving as named, faint with the sparks seen through it; no "Failed to compile shader:", "Failed to link shader program", "Failed to load the shader:" or "Failed to link material custom parameter" line. Then: a log error, or solid magenta domes (the engine's fs_invalid) - the shader failed: fix vsn5uvmove and probe again. No error and nothing moving - route B (a flipbook). Domes 1-3 moving, see-through - route A as built (texture0). 1-3 hiding the sparks, 7 see-through - texture0 plus the TXI beside the n5u_ material (dome 7's). 1-3 and 7 hiding the sparks, 4-6 see-through - the copy route (the texture and its TXI copied under the n5u_ name). All moving, every dome hiding the sparks - no TXI placement survives a material: route B, a flipbook TXI (`proceduretype cycle`, `blending additive`, as fxpa_flame02's). Low domes flat white (no texture), tall ones textured and see-through - texture0 doesn't bind: the copy route |
    | TMO-2 | (M3, nwn5e-tools 91b56b7) the Composer: New project `tmoglyph`, Spell mesh; the guide's Diameter (m) 2; Shapes, Dome (half sphere); its card Mapping From above, Texture From the library... `fxpa_symbl_003`, Use; Colour orange (#ff8000), Glow 1; Texture motion: Turn (°/s) 30, Breathe 0.2 (Breaths (/s) 0.5) | Play in game; then Save and build, Back to effects, New, Type Stays (D), + mesh `n5m_tmoglyph`, Permanent ticked, Play in game | an orange glyph turning counter-clockwise seen from above (a turn in 12 s) and pulsing (a breath in 2 s) on the stage and in game, additive as the game's own globe; the status names its `n5u_` material; the mesh beat's card: "Its texture turns 30° a second and breathes 0.2 at 0.5 a second - from the spell mesh n5m_tmoglyph; change it there." A white glyph in game, or none, means our material drops the mesh's colour (never seen through an n5u_ material: TMO-1's glyphs were white) - say which |
    | TMO-3 | (M3, nwn5e-tools 91b56b7; its review 22a3d62) the Composer: New project `tmocrate`, Placeable; Placeables, `plc_a03`; its card's Tint pale green (#80ff80), then its Texture motion: Scroll round 0.5, Tab | the Undo button at the top (or click outside the boxes, then Ctrl+Z), then Redo (Ctrl+Y); Play in game; Save and build; after the next install (the animation session commits the project, its .pwk and data\rows\placeables.tmocrate.txt; build.bat makes its .mdl and the n5u_ materials), the toolset: a placeable of appearance N5_Tmocrate in an area, the module played | the pale green crate's textures slide (a repeat every 2 s) on the stage, in Play in game and installed (the data\files route) - green there too (the tint through the placeable's material); Undo stops it, Redo starts it. plc_creats is a sheet of several pictures, so other pictures slide across the crate's faces - expected |

## A mesh beat's keys in the effect model (2026-11-18, #39; M4 - game-side)

  Spec: the design's "Keys on a mesh beat" (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md);
  plan Task M4. The owner, 2026-11-18: "is it possible to add keyframes to animate the shapes? Or make them follow
  emitters, just like other elements". The one game-side change of the plan, announced to the game session before its
  first edit: src/data/vfx_model.h, src/data/vfx_model.cpp, tests/data/vfx_model_test.cpp and a new fixture,
  tests/data/fixtures/plane_moves.vfx; no model written today changes.
  - *Calls:*
    - `VfxKeys.scale`: rows t s, one value (the same in x, y and z) - the model's `scalekey`, controller 36 with one
      column, as the game's own vdr_bigby_int keys its hand (a plain mesh: 1 to 10 over its 0.333 s impact, held at 10,
      10 to 1 over its 0.3 s cessation). Of the game's 25,305 compiled models two key a scale, both on a mesh, both one
      column (vdr_bigby_int and the door tn_sdoor_24); its ASCII models key none, but the exe's base-node keyword table
      reads `scale` / `scalekey` / `scalebezierkey` beside `position` / `positionkey` and `orientation` /
      `orientationkey`, and nwn5e's own shipped ASCII models already key a trimesh with exactly `scalekey N` and N rows
      of `t s` in an animation (n5_lb* / n5_ld* / n5_rs* / n5_rh* / n5_rc*, stealth_rings.py, since 0.40.4, tested in
      game; n5v_mark* since 0.96.9). Only a plane's reaches the model: a node's scale keys are kept in the draft and
      not written (controller 36 appears only on meshes in the game's models);
    - `VfxPlane.tint`: r g b, 1 1 1 none - the bench's (a mesh beat's material is already multiplied by it), in the
      draft only, never in the model;
    - the draft: `<anim>_scale = t s ...` after `<anim>_alpha` (in any section, as every key list); a plane's
      `tint = r g b` after its `emit_seconds`, before its keys, only when it isn't 1 1 1 (read as its material's colours
      are). A draft without them is written back byte for byte;
    - WriteVfxModel's animations: a plane with any keys there - position (a whole row: 4 numbers), orientation (5),
      scale (2) or alpha (2) - gets `node trimesh <its name>`, `parent <its parent>`, then `positionkey`,
      `orientationkey`, `scalekey` and `alphakey`, each only when it has a row, and `endnode` (of each kind the first list
      in that animation: a draft has one an animation). A plane with only move keys gets its node too. A plane without
      keys there is in no animation, as before; its geometry unchanged. Every key list of the model - a node's, an
      emitter's, a light's, a plane's - is written by one helper, the bytes as before;
    - `LastKeyTime` counts a plane's position, orientation and scale keys beside its alpha: an animation lasts as long as
      its planes' keys.
  - *Built (anim, the commit carrying this section):* the two fields, the draft's lines, LastKeyTime, the plane's
    animation node; plane_moves.vfx (plane_beat's beat, tinted 1 0.5 0.5, dropping in from 3 m while it turns half round
    and grows from 0.2 to 1 in the impact, with plane_beat's fade; a whole turn in thirds over 3 s in the duration).
    test.bat 583 -> 588: VfxModelPlaneMoveKeys, VfxModelPlaneMoveKeysLength, VfxDraftScaleAndTint,
    VfxModelUnchangedWithoutPlaneKeys and the game anchor VfxGameEffectsScaleKeys (vdr_bigby_int's hand read from the
    game: controller 36, one column; its rows as a plane's keys written as `scalekey 2` with `t s` rows; it skips
    cleanly without the game). The model readers are unchanged.
  - *What changes:* nothing for an effect without a plane's move, turn or scale keys or a tint. HEAD's code and this
    write the same drafts and models (hashes) for every draft kept and every game model the bench reads.
  - *Not read back (as before):* neither model reader keeps a plane's keys - its alpha, move, turn or scale. A game
    effect re-read in the bench (vdr_bigby_int's growing hand) shows the plane unmoved, and a re-save writes none.
  - *For the bench:* nothing until M5 (core.js reads and writes the same bytes; plane_moves.vfx is its round trip).
  - *What a plane's keys mean (for M5):* its position and orientation keys are a node's - absolute in its parent's
    frame, replacing the plane's own `position` and `orientation` while the animation plays (so M5's sim, Drop in and
    Make it spin build them around the plane's own pose, or it snaps to them); its scale keys multiply the mesh as
    placed (its verts are already scaled), 1 = as placed - M5 bounds scale above 0 (nwn5e's own models shrink to 0.05
    or 0.001, never 0). A turn of 180 degrees or more is split into steps of 120 or less, as the game's quaternions
    take the short way.
  - *For the game:* the next install carries it: a mesh beat's move, turn and scale keys play in game. Nothing changes
    for existing effects. Until the install, the game's DLL writes only a beat's alpha keys, so the beat shows unmoved
    in game; an older draft reader skips `_scale` and `tint` without an error. MBK-1 to MBK-3 (the design's rows) wait
    for M5 and the install.
  - *The impact's blend (TT, 2026-11-18):* the owner, MBK-1 on 0.99.79, after Drop in's first key moved to 0: "the disk
    in-game still starts at ground level, rises to 3m and then drops down again".
    - *The blend:* the game blends an animation in from the pose before it over its `transtime` (the compiled
      animation header's f32 at +0x74, after its length at +0x70) - an effect's impact from the rest pose, its
      geometry's. WriteVfxModel wrote `transtime 0.25` for every animation, so a disc at rest on the ground, keyed from
      3 m, rose for 0.25 s before it fell.
    - *The rule* (vfx_model.cpp's StartsOffRest, one helper): the impact is written `transtime 0` when an item's first
      impact key is off its rest by more than 1e-4 - a node's, emitter's, light's or plane's position (against its
      position) or orientation (against its orientation, compared as turns: the angle between them), a plane's scale
      (against 1) or alpha (against its material's), a light's colour or radius (against its own). Otherwise 0.25, as
      before. 1e-4 is far below what a blend could show (0.1 mm, 0.006 degrees) and far above a value's noise on its way
      from the bench; it is the binary reader's own tolerance for a key at the geometry's value. The duration and the
      cessation keep 0.25: each blends from the animation before it.
    - *The game's own practice:* its effects' impacts blend 0.25 (282), start at once with 0 (81) or blend 0.15 (2).
      Its keyed effect nodes keep their rest at their first key: vim_protevil's OmenEmitter01, vdr_bigby_int's hands,
      and fx_meteor and fx_rainice (both at transtime 0).
    - *What changes:* only the impact's transtime line, and only for an effect with an off-rest first key. Of the 564
      drafts kept (the fixtures, Detect Magic's four, the main build's vfxedit and the anim helper's cache), every draft
      writes the same bytes and 534 write the same model. The other 30 differ by that line alone: plane_beat and
      plane_moves, the bench's request (Drop in), and 27 library drafts of 25 effects as the bench reads them (24 of
      the game's and nwn5e's n5_srest). Of the 24, the game already has 0 on 3, 0.15 on 1 and 0.25 on 20: a re-save
      of one of those 20 now starts on its keys instead of blending.
    - *For the game:* the next install carries it, because the game's DLL writes the model (the bench writes none).
      Until then the installed DLL writes 0.25, and MBK-1's disc still rises first.

## The Mesh card's keys, Breathe and Tint; the sim (2026-11-18, #39; M5)

  Spec: the design's "Keys on a mesh beat" and "Tint" (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md),
  addenda (a) and (b); plan Task M5. The owner, 2026-11-18: "is it possible to add keyframes to animate the shapes? Or make
  them follow emitters, just like other elements", and "tinting options" for the textures; Q2 (no): a beat doesn't
  override its texture's motion - nothing of that here. Bench only (nwn5e-tools); the game side is M4 (anim 7dd4657,
  installed by 0.99.76). `git diff --stat -- src` on anim: empty before and after.
  - *Calls:*
    - core.js matches M4's draft byte for byte: `<anim>_scale` right after `<anim>_alpha` (any section, as every key
      list), a plane's `tint = r g b` after its emit_seconds, before its keys, only off 1 1 1 (core.plane()'s tint
      1 1 1), read as its material's colours are;
    - the sim (sim.js planesAt) plays a plane's own keys as the game's model writer keys them (this section's M4
      paragraph, "What a plane's keys mean"): its position and turn keys absolute under its parent, in place of its own
      pose; its scale keys times the mesh as placed - scaled, then turned, then placed under its parent's place.
      `lastKey` counts its move, turn and scale keys beside its alpha (the C++'s LastKeyTime);
    - the Mesh card's **Moves** reuse K1's key table (app.js keyTable gains `onChange`; the Motion card's own by
      default, unchanged): the first plane's keys shown and edited, then copied onto every plane of the group
      (beats.js shareKeys - each plane keeps its own alpha keys); **Drop in** (parts.dropIn, round its place),
      **Make it spin** (parts.spinKeys: a node's turn keys - quarter turns, whole turns in the loop - built round its own
      turn, so it doesn't snap), **Breathe** (parts.breatheKeys: 1 + how much x sin, 12 rows a breath, whole breaths in the
      loop, 1 at both ends; how much 0.01-0.9 and 0.05-5 breaths a second, refused past them - never 0 or below),
      **Clear its keys** (move, turn and scale; its fades kept). Each one undo step. Its Position moved takes its move
      keys with it (core.movePosition, as a node's); its Turn changed leaves its turn keys (as a node's): Make it spin
      again;
    - **Tint** (#mesh-tint, None): every plane's material the spell mesh's (readMesh) times the tint, lifted from the
      draft's defaults first as the game draws such a plane, through core.num's rounding (beats.js tintPlanes; white gives
      the mesh's own back), `tint` kept on each; a mesh changed since it was copied (as Scale tells it): the copy's own
      material over its old tint, times the new (retintPlanes). One undo step. The game sees only the material: a tinted
      beat shows with any install;
    - Refresh from the mesh keeps the keys (the first plane's move, turn and scale keys on every new plane) and the tint
      (applied to the mesh's material as it is now); rekeyMesh rebuilds only alpha; + mesh gives tint 1;
    - M5's follow-up found on the way: parts.applyLoop (a motion's pace setting the loop) retimes a beat's moves in the
      loop with it, as a node's - else a spin or a breath would stop short of the new loop and jump; its alpha keys (its
      timing, seconds) stay.
  - *Built (nwn5e-tools 7b3d104):* core.js, sim.js, parts.js, beats.js, app.js, index.html, README. check.js +8 (the
    anim worktree's fixture plane_moves.vfx - NWN5E_EE, else ..\nwn5e-ee-anim, skipped with a note when it isn't there -
    and the rule's lines; one a guard), check_beats.js +5, check_parts.js +3, browser_check.mjs +5 (meshKeys, meshDropIn,
    meshSpinBreathe, meshFollows, meshTint: 105 steps, all passed; the five seen failing on 14c02ab's page). check.js:
    "523 drafts (1885 emitters) read and written back, 233 textures decoded, 0 failed" (before: 522 / 1884, 1 failed - the
    main checkout's plane_moves.vfx, 0.99.76 carrying M4) and "the C++ fixture plane_moves.vfx ... read and written back
    byte for byte by core.js"; `--helper` with a helper built from anim add2cca into a temporary folder: 416 of the game's
    effects, 0 failed. Beats, parts, effect, chunk, composer and area checks pass; composer_check.mjs all passed.
  - *Limits:* the model readers keep no plane keys: a game effect's keyed mesh (vdr_bigby_int's growing hand) opens
    static on the bench, and a re-save writes none (out of scope). A mesh under an emitter is untested in game until
    MBK-3. Only the first plane's keys are edited; a draft whose beat planes carry different keys is shown by its first.
  - *For the bench:* restart vfx_bench.bat. *For the game:* the install carrying anim 7dd4657 (0.99.76 or later) plays a
    beat's moves; before it the beat shows unmoved in game. Nothing for the DLL here.
  - *M5's review (nwn5e-tools 7a6f653):*
    - the tint of a beat whose spell mesh changed since it was copied: retintPlanes lifts an untinted copy at the draft's
      defaults first, as tintPlanes lifts the mesh's (a first red was 0.8 0 0 / 0.2 0 0, now 1 0 0 / 1 0 0); a tint
      needing back a colour the old tint took to 0 (red to green, red to none) is refused - "Refresh from the mesh
      first...", nothing changed, no undo step (it gave a black material, or red under a card showing none). The limit:
      on a changed mesh, Refresh from the mesh before such a tint;
    - Make it spin / Breathe on a timed beat in a lasting effect (hidden in the loop): the keys are made and the status
      says they aren't seen (On for 0 shows them) - said, not keyed into the span it shows;
    - one Drop in row (app.js dropInRow) for the Motion card and the Mesh card; the loop note counts the beats a motion's
      pace retimed;
    - another limit, named: turn keys' angles are kept as worked out, an undo gives them as the draft writes them (6
      digits) - as a node's;
    - check_beats.js +2, browser_check.mjs +3 (meshTintChangedMesh, meshTimedSpinSaysItsHidden, meshLoopRetimed: 108
      steps, all passed), each seen failing on 7b3d104.
  - Test rows (for the board: #39, stream animation; each needs the game from the install carrying anim 7dd4657 -
    0.99.76 or later - and the VFX Bench restarted; the spell mesh: Composer, New project `mbkglyph`, Spell mesh; Parts,
    Shapes, Disc (flat, on the ground); Radius (m) 1, Mapping From above, Texture From the library... `fxpa_symbl_003`,
    Use, Colour white (#ffffff); Save and build - `n5m_mbkglyph`; Back to effects, New, Type Stays (D), Permanent (until
    Stop) ticked, + mesh `n5m_mbkglyph`):

    | ID | Setup | Do | Expect |
    |---|---|---|---|
    | MBK-1 | The spell mesh and the effect above (after the install carrying anim 7dd4657) | On the mesh's card, Moves: Drop in: start this high (m) 3, Land after (s) 0.5, In Start (impact), Drop in; Play in game | the glyph falls from 3 m to the ground in half a second at the start, on the stage and in game, and lies there - no snap at the landing |
    | MBK-2 | As MBK-1 | Moves: Turns a second (round its z) 0.5, Make it spin; Breathe: how much 0.1, Breaths a second 0.5, Breathe; Play in game, at least 10 s | one turn (counter-clockwise from above) and one breath (1.1 to 0.9 of its size) every 2 s, the same in game as on the stage; no jump at the loop |
    | MBK-3 | As MBK-1 | the sparks beat's Its own motion: Orbit radius (m) 2, Turns a second 0.5, Height (m) 0, Make it orbit; the mesh's card Hangs under sparks; Play in game | the glyph circles the character 2 m out with the sparks, once every 2 s, on the stage and in game |
    | TIN-3 | As MBK-1 | the mesh's card Tint red (#ff0000); Play in game; then the Composer, mbkglyph's disc Radius (m) 1.5, Save and build, Back to effects, the mesh's card None (before Refresh), then Refresh from the mesh; Play in game | the glyph red on the stage and in game; None on the changed mesh refused ("Refresh from the mesh first", still red); after Refresh the larger glyph, still red (the status: its tint kept) |

## Moving textures on the bench (2026-11-18, #39; M3)

  Spec: the design's "Texture motion" and addenda (a) and (b)
  (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md); plan Task M3. The owner's TMO-1,
  2026-11-18: "it does show through and all the domes animate" - route A as built: the material names its texture
  (texture0), our vsn5uvmove moves it, and the texture's additive TXI still applies under the material (faint,
  see-through, no 0.2 cut); the copy route and the TXI beside the material aren't needed. Q2 (no): a mesh beat shows its
  texture's motion and never changes it - the spell mesh's. Bench only (nwn5e-tools); `git diff --stat -- src` on anim
  empty before and after, and nothing on anim but this section and M2's two rows.
  - *Calls:*
    - the part card's **Texture motion**, after the part's own fields (a shape's after its Texture): Scroll round, Scroll
      up, Turn (°/s), Breathe and Breaths (/s) - floatdisk.js MOTION_INFO's labels, bounds and titles; Breaths greyed
      while Breathe is 0; each change one undo step; a value past its bounds as typed refused, the field put back ("Breathe:
      0 to 0.9.": the shapes' rule); shown while the part has a textured mesh (a shape's Soft or library texture, a
      model's). A file keeps `motion` as cleanMotion writes it, each number to 0.001 first (composer.js clean); a save's
      status names the materials ("Moving textures: n5u_... (its texture), with vsn5uvmove.shd (in vfxedit\saved).");
    - what you see is what's built: composer.js placeOne marks a moving part's meshes by floatdisk.js's own moveTextures
      (each keeps its texture on the stage and carries uvMotion { motion, pivot, name }); view.js's mesh program gains
      `u_uv` (a mat3: v_uv = (u_uv * vec3(a_uv, 1.0)).xy), set per mesh by uvMatrix - floatdisk.js uvMotion with v
      flipped as the stage holds its UVs (u, 1 - v): [a, -b, b + c, -d, e, 1 - e - f] - else a flipbook's frame, else
      identity (the particles' always);
    - the stage's clock: the Composer's runs while a shown mesh moves and the page is visible (app.js draws a frame each
      animation frame; otherwise the clock stands); the effect mode's is the preview's time, an area's its own. The game
      moves a texture by its world clock (worldtimerTimeOfDay) from wherever that is: the stage shows its speed and
      direction, not its moment;
    - the server: a texture's info gains `cycle` ({ numx, numy, fps } from its TXI's `proceduretype cycle`, else null) and
      `additive` (its TXI's `blending additive`); an `n5u_` name resolves through its .mtr (the checkout's data\files, the
      bench build's saved\ and vfxedit\) to its texture0's picture, the texture route saying `X-Motion` { motion, pivot,
      of }; a flipbook `X-Cycle: numx numy fps`;
    - Play in game: the planes name the materials; `POST /api/composer/bake` takes `materials: [{ texture, motion, pivot
      }]` beside its textures and shapes and writes each `n5u_<...>.mtr` (motionMaterialText) and `vsn5uvmove.shd`
      (motionShaderText, CRLF) into vfxedit\ - every entry checked first, one refused writes nothing;
    - the effect mode: a plane or a chunk's mesh naming an `n5u_` material moves by the material's numbers (the loader
      loads the checkout's floatdisk.js first); a library flipbook (Elemental Shield's fxpa_flame02, 4 x 4 at 32) draws
      one frame at the preview's time - floor(t x fps) mod 16, row by row from the top - not the whole sheet: since M3's
      review (below) each frame is its own texture, bound, the UVs as they are; additive TXIs as M1b drew them; the Mesh
      card says its planes' motion: "Its texture scrolls up 0.25 a second - from the spell mesh n5m_x; change it there."
  - *Built (nwn5e-tools 91b56b7):* view.js, composer.js, composer_page.js, composer_server.js, app.js, index.html, README.
    check_chunks +4 (40), check_composer +2 (46), check_composer_server +4 (53; bakeShapesForPlay's expected error text
    names the materials now, realWorktreeUntouched counts `n5u_` files and the shader too), composer_check.mjs +3
    (textureMotionCard, motionPlays, motionBounds; 99 ok lines, all passed), browser_check.mjs +2 (beatTextureMoves,
    cycleRing; 110, all passed); each new check seen failing first. Beats, effect, parts and area checks pass; check.js:
    523 drafts (1885 emitters), 233 textures, 0 failed.
  - *What changes:* nothing for a part without a motion - its file, its stage meshes, Play's draft and its bake request
    as before.
  - *Limits:* the stage's moment isn't the game's (above). An `arturo` ripple stays still on the stage. A flipbook's
    frames bound one at a time (M3's review: repeating past UV 1, a moving flipbook moving within its frame) and counted
    from the top left, across then down, are inferred from the game's shaders, not yet seen in game (TMO-4). A material
    is read as every Composer texture is - the route needs the game: without it the effect mode draws an `n5u_` plane
    untextured and still, and its Mesh card says nothing of the motion.
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing for Play in game (it writes the material and the
    shader itself; 0.99.76 carries vsn5uvmove.shd in data\files since M2). A placeable's moving texture (TMO-3) reaches
    the game with the install carrying its project (build_derived makes its materials).
  - Test rows: TMO-2 and TMO-3 (M2's table, above, now with the card's steps), and TMO-4 (M3's review, below).
  - *M3's review (nwn5e-tools 22a3d62):*
    - a flipbook binds a frame, never slides its UVs: the game's stock shaders can't translate UVs (`vVertexTexCoords =
      mat2(m_texture) * vTcIn.xy`, fslit samples them as they are), so the engine must bind each frame as its own texture.
      view.js cuts a cycle picture into its numx x numy frames at upload (cycleFrames, row-major from the top), and
      drawMeshes binds `frames[floor(t x fps) mod n]` (frameAt), the UVs as they are: Elemental Shield's ring (u -0.083 to
      2.917) shows the same flame three times round, where M3 showed three different ones; a moving flipbook moves within
      its frame. uvMatrix is motion or identity;
    - uvMatrix takes the slide (c, f) within one repeat, 0 to below 1, as the shader's `fract` does;
    - Texture None takes a part's motion with it in the same undo step - nothing left to move, no motion kept that moves
      nothing - the status saying so, Undo bringing both back (the coordinator left the call to me; greying the group would
      keep a motion in the file that does nothing);
    - TMO-2 searches both logs; TMO-3's step undoes with the Undo button (Ctrl+Z in a number box is the browser's own text
      undo), and says what scrolling a sheet (plc_creats) shows and what the commit and the install each bring; README and
      these limits name the frame order and the repeat past 1 as unseen in game; TMO-4 (below) looks at both;
    - checks: check_chunks 41 (the frame check re-aimed at frameAt and the bound frame, the motion check's slide within a
      repeat, the additive check's frame, and a new one: a flipbook's sheet sliced into its frames, row-major from the top,
      each its own texture); browser_check.mjs cycleRing on a ring spanning u 0 to 3 (one frame texture of 16, 32 x 32,
      identity u_uv); composer_check.mjs motionBounds (None takes the motion with it); each changed check seen failing
      first (the two browser checks on 91b56b7's page, from a temporary copy).

    | ID | Setup | Do | Expect |
    |---|---|---|---|
    | TMO-4 | VFX Bench restarted; the game started (`!test level1ranger`); the Effects drawer's Game tab, row 147 `VFX_DUR_ELEMENTAL_SHIELD` (vdr_eleshield) opened | look at the stage's ring for 30 s; in game chat `!vfx`, search `elemental` (or 147), click `VFX_DUR_ELEMENTAL_SHIELD` (8 s a time; again as it ends, for a minute), close up; Clear effects. Then the Composer: New project `tmofall`, Spell mesh; Shapes, Cylinder (open); its card's Opacity 1, Texture From the library... `tcm02_wtfall01` (a waterfall, 1 x 8 frames at 12 a second), Use; watch the stage; Play in game | the ring's three flames the same at every moment, on the stage and in game; any difference in how they move between the two (backwards, or a jump every eighth of a second): say so. The cylinder's water runs the same way on the stage and in game (the frames counted from the top left) |

  - *M3's whole-plan review (nwn5e-tools 78f977d; anim, the commit carrying this line):*
    - TMO-2 sets the glyph orange (Colour #ff8000, Glow 1): a coloured texture through an n5u_ material has never been
      seen in game (TMO-1's glyphs were white) - a white glyph or none says the material drops the mesh's colour. TMO-3
      tints its crate pale green, for the placeable route. TMO-4 compares a waterfall cylinder (tcm02_wtfall01) on the
      stage and in game, and uses the dev layer's `!vfx` window for Elemental Shield (as the csv);
    - Drop in on a mesh beat with a Starts after (M5's) landed before the beat appeared: it now starts the fall at the
      beat's start in the impact - held high from the first key to its start, landing its Land after from then (three
      keys; parts.dropKeys' `from`), the status saying "from its start"; check_parts and browser_check.mjs
      meshDropInAfterItsStart;
    - the Mesh card's note said "Cut at alpha 0.2" of a plane whose texture is additive (M1b's): it now says "Drawn adding
      its light in game (its texture's TXI says blending additive)"; browser_check.mjs meshHintSaysAdditive;
    - stale lines after TMO-1 and M3 (floatdisk\README.md, composer\README.md, uvprobe.js, floatdisk.js): "TMO-1 passed
      2026-11-18: route A (texture0), the texture's TXI still applies"; an envmap under our material is unknown, not
      dropped. The VFX Bench README's limits gain a moving texture's envmap, an `n5u_` material outside the bench build (the
      design's addendum (c), the shipping route still open) and the stage's area ambient (1 untextured, 0 textured).

## A breathing texture's edges clamped (2026-11-18, #39; W1)

  Spec: the design's addendum (c) (docs/superpowers/specs/2026-11-18-mesh-texture-animation-and-tint-design.md). TMO-2
  (passed otherwise), the owner: "when breathing the texture wrapped around the mesh, so instead of the glyph circle only
  growing and shrinking on the top, when it shrank the texture being tiled and wrapping would show a bit at the bottom
  edge of the dome" - then "go with 1 for the wrap". The game repeats a texture past UV 1 (a scroll needs it); a breath
  below 1 scales the UVs past the picture's edge, so the next repeat shows at the mesh's edge. No src or tests change
  (`git diff --stat -- src tests` empty); no DLL change, no install.
  - *Calls:*
    - **the rule** (floatdisk.js `motionClamps`, one place; `moveTextures` marks the material `clamp: true`): clamped when
      the motion breathes and doesn't scroll - each as the material takes it, to 0.001 - and every UV of the mesh is
      within 0..1, 0.001 either side (a shape From above, or Wrapped at Repeat around / up 1; a game part whose UVs don't
      tile). A turn alone isn't clamped (no breath, nothing shrinks); a scroll with a breath repeats (the scroll needs the
      next repeat); no UVs, it repeats. The Composer's page runs the same rule (its own copy of the checkout's
      floatdisk.js), so the stage, Save and Play agree;
    - **the name** hashes `:clamp` after the texture, motion and pivot: a clamped and an unclamped motion never share a
      name (the copy and its TXI go with the clamped one only), and every unclamped name is as before (M2's pin
      `n5u_31e9f07948e1` holds). TMO-2's dome is `n5u_bfb2c9b5a9bf` now;
    - **the game's clamp line, `clamp 3`.** The game's 15 clamp TXIs (read 2026-11-18, values only): `clamp 1` in 4
      (gui_chrsht_2122, gui_chrsht_b16, gui_chrsht_b16lt, gui_slider), `clamp 2` in 1 (tdt01_testc, a test tile), `clamp 3`
      in 10 (the GUI's tabs, icons and loadscreens). Read as two bits - 1 u, 2 v (inferred from the values, not seen) - 3
      clamps both ways; it's also the game's commonest;
    - **the TXI** (`clampedTxi`): the source texture's own lines as they are, in its own line ends (the game's CRLF, our
      LF) - its blending, its procedure (`proceduretype`, `numx`, `numy`, `fps`), `decal` ... - any clamp line of its own
      dropped, its blank lines at the end dropped, then `clamp 3`. fxpa_symbl_003's: `decal 1`, `blending additive`,
      `downsamplemax 0`, `downsamplemin 0`, `clamp 3`;
    - **the copy route TMO-1 proved** (its copy domes 4-6): the material has no `texture0`; the texture's picture is
      copied under the material's name, `n5u_<...>.tga` or `.dds` as the source is (a soft one as made, a dyed part's
      bake, the game's - its DDS when it has one, what the game draws - or the one beside your own file), with the TXI
      beside it. **One copy path** (files.js `motionPicture`, uvprobe's code moved there): the generators
      (`motionCopies`), the probe and the bench's bake route all use it; `motionWrites` writes a material's copy and TXI
      beside its `.mtr` (kind "material": owned by the `n5u_` name, as the material);
    - **where:** wherever materials go - data\files for a placeable (git-ignored by `/data/files/n5u_*`; build_derived
      already carries `n5u_*.tga / .dds / .txi` as materials: only its comment changed), a spell mesh's saved\ (Save),
      Play's vfxedit\ (the bench's bake route), the Floating Disk's data\files and drafts. Never committed: a library
      texture's copy is the game's art;
    - **the probe** writes TMO-1's domes as they were: `moveTextures`' `{ clamp: false }` (the rule off) - its breathing
      From-above domes 3 and 6 would be clamped now; through the shared copy path, its output byte for byte as before
      (composer check_build's probe check unchanged);
    - **a spell mesh's own-file texture** shipped beside its draft stays its own when its material is clamped: a draft's
      `n5u_` material with no `texture0` gives its texture by its name, as this build makes it (composer build.js);
    - **the stage:** view.js `uvMatrix` takes a clamped motion's slide as it is (no `fract`: a clamped picture doesn't
      repeat, and the game's shader has no scroll to wrap); a clamped material's picture (the route's X-Motion `clamp`)
      uploads CLAMP_TO_EDGE (a flipbook's frames too); a Composer part's clamped mesh (its uvMotion's `clamp`) on a
      picture uploaded repeating - another mesh may repeat it - is drawn CLAMP_TO_EDGE and its texture put back after.
      composer.js placeOne carries the clamp from the material; Play's bake request carries `clamp: true`;
    - **the server:** the bake route takes a material's `clamp` (true or false; anything else refused, nothing written):
      a clamped one named with its clamp, its `.mtr` naming the copy, the copy and TXI written into vfxedit\ (the
      checkout's files.js loaded for `motionPicture`: a soft texture or bake made in the same request, else the game's,
      else beside the parts or where they're built), listed `{ material, name, texture, state, copy, txi }`; the texture
      route's `X-Motion` (and a part's texture info) say `"clamp": true` when the material's picture's TXI clamps
      (`txiClamps`); files.js joins the checkout code server.js watches (the restart message names it).
  - *Built:* nwn5e-tools 0eb8d7b on branch `w1-clamp` (view.js, composer.js, composer_page.js - a `setClock` hook for
    the checks -, app.js, composer_server.js, server.js, README; check_chunks, check_composer, check_composer_server,
    composer_check.mjs); anim: the commit carrying this section, on branch `w1-clamp-anim` (floatdisk.js, files.js,
    floatdisk\build.js, composer\build.js, uvprobe.js, build_derived.js's comment, the READMEs; floatdisk check, floatdisk
    and composer check_build, check_derived). Both built in git worktrees of their own, not the live checkouts the
    owner's bench runs from (the coordinator, 2026-11-18: each change there tripped the bench's stale-server guard); the
    coordinator fast-forwards main / anim after review, so the bench restarts once.
  - *Checks* (step 1 -> now; each new check seen failing first - a generator's by its `motionCopies` call taken out, the
    page's on HEAD's view.js and composer.js):
    - anim: floatdisk check 97 -> 104 (+7: the rule, the name, the TXI, the material, moveTextures, files.js's copy and
      writes, the page's floatdisk.js); composer check 24 (unchanged); floatdisk check_build 12 -> 13; composer check_build
      26 -> 30 (+4: a spell mesh's TMO-2 dome, a placeable's Soft dome, nothing copied for a scroll / a tiled dome / a
      turn, an own file's breath; the own-file M2 check re-aimed at a turn, as a breath is clamped now); check_derived 17
      -> 18 (a placeable's clamped library dome into --out). A mutation check of the rule's code (19 breaks, in a
      temporary copy): every one fails a W1 check;
    - nwn5e-tools: check_chunks 41 -> 44 (a clamped slide as it is; drawMeshes' wrap per mesh; a clamped picture's
      upload), check_composer 46 -> 47 (placeOne's clamp), check_composer_server 53 -> 55 (Play's clamped materials and
      their copies, served clamped; a saved spell mesh's clamped dome served as its copy), composer_check.mjs 99 -> 100
      (clampedRimClear: TMO-2's dome from above at the breath's smallest - the rim exactly as with the dome deleted; on
      HEAD's view.js up to 243 brighter, the next glyph), browser_check.mjs 112, all passed; beats, effect, parts and
      area as before; check.js 0 failed.
  - *Limits:* **unproven until TMO-5** that the game honours a TXI's clamp on a mesh (14 of its 15 clamp TXIs are the
    GUI's); if it doesn't, the fallback is option 2, a breath that only grows (1 to 1 + 2 x Breathe and back). Clamped,
    a picture whose edges aren't plain (a Soft texture's rim glow) shows its edge pixels stretched where it shrinks -
    expected, the owner's option 1. `clamp 3`'s meaning (both ways) is inferred from the game's values.
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing installed - Play in game writes the copy and its TXI
    into vfxedit\ itself; a placeable's clamped texture reaches the game with the install carrying its project.

    | ID | Setup | Do | Expect |
    |---|---|---|---|
    | TMO-5 | VFX Bench restarted; the game started (`!test level1ranger`); the Composer: `tmoglyph` (or New project `tmoglyph`, Spell mesh, Diameter (m) 2, Shapes, Dome (half sphere), Mapping From above, Texture From the library... `fxpa_symbl_003`, Colour orange, Turn (°/s) 30, Breathe 0.2); a second dome 2.5 m along x, Mapping From above, `fxpa_symbl_003`, Scroll up 0.25 | View: Top; watch both domes while the glyph shrinks; Play in game, watch from above and the side, at the first dome's bottom edge; Save and build; read the status; quit, search the logs | the turning glyph's dome edge stays clear while it shrinks (no part of the next glyph at the bottom edge), on the stage and in game; still orange, turning, faint, see-through; the scrolling dome still repeats (the next glyph slides in). The edge showing the next glyph in game only: the game ignores the TXI's clamp on a mesh - say so (the fallback: a breath that only grows) |

## The Cylinder's Top radius: a truncated cone either way up (2026-11-18, #39; TR)

  The owner, 2026-11-18, on Elemental Shield's ring of flames: "how is that angled ring of flames made? it looks like a
  flame texture on a truncated upside down cone" - offered a Top radius on the Cylinder, which makes it a truncated cone
  either way up: "yes". The game's `vdr_eleshield` Cylinder02: two rings of 40 points, 1.00 m at z 0.012 and 1.17 m at
  z 0.605, open both ends, `fxpa_flame02` (additive, a 4 x 4 flipbook at 32 by its TXI) three times round. No src or tests
  change; no DLL change, no install.
  - *Calls:*
    - floatdisk.js: `top` joins SHAPE_KEYS after `inner` (the order it's written in) and the Cylinder's card after Radius
      (SHAPE_FIELDS.cylinder: radius, top, height, segments, sides); SHAPE_INFO.top "Top radius (m)", 0.01-100, step
      0.05, titled "The radius at its top: the same as Radius is a cylinder; wider flares out (a lampshade upside down),
      narrower leans in". No other type takes one (refused, naming its own keys);
    - its default is the Radius (shapeDefaults), never written: **a Top left at its default follows the Radius**, as a
      Height does (a Top typed equal to the Radius isn't kept, and follows it from then on);
    - the mesh: the cylinder's profile runs from (radius, 0) to (top, height) - both sides, the inside first, the
      groups, the seams, Wrapped round it and up it, the repeats, motion, tint and blend as before. From above spans
      the wider of its two rings (every UV within 0..1), and so does its box (shapeModel's lo / hi);
    - **at Top = Radius the cylinder before it, byte for byte**: its model, a placeable's .mdl text, a spell mesh's
      draft, its placed meshes, its written shape and its soft texture name - seven cylinders compared with anim c7c650b's
      (Top given or not; the probe in the coordinator's tmp), and five of their models pinned in the floatdisk check;
    - the page needed no change for the card (S5's promise holds: the Cylinder's box shows Top radius from SHAPE_FIELDS
      and SHAPE_INFO, refuses it out of bounds, one undo step). Two small page changes: composer.js keeps every metres
      field to the millimetre - those SHAPE_INFO labels "(m)", where it had a list of three - and the Composer hook's
      `drawn.rings` (each stage mesh's heights and its reach from its middle at each) for the checks.
  - *Built:* nwn5e-tools d0d70ef on branch `tr-top-tools` (composer.js, composer_page.js's hook, README;
    check_composer, composer_check.mjs); anim: the commit carrying this section, on branch `tr-top` (floatdisk.js, its
    check, the floatdisk and composer READMEs). Both in worktrees of their own; the coordinator lands them.
  - *Checks* (before -> after; each new check seen failing first):
    - anim: floatdisk check 104 -> 110 (+6: the field and its default, byte for byte at Top = Radius, the flared mesh -
      two rings of 37 at 1 and 1.17, faces out outside and in inside by face and by signed volume, its leaning-in twin -
      From above from the wider ring, the repeats, the page's floatdisk.js; S5's "other shapes" check amended for the new
      key, field and default); a mutation check (13 breaks of the code, in a temporary copy): every one fails a TR check. composer check
      24, floatdisk check_build 13, composer check_build 30, check_derived 18 - unchanged, all pass;
    - nwn5e-tools: check_composer 46 -> 47 (TR: clean keeps Top to the millimetre, a Top at the Radius not written, a
      flared cylinder placed 2.34 m across; its round trip's one failure is the CRLF checkout's, before and after - 48,
      all pass, with an LF composition.json), composer_check.mjs 100 -> 101, all passed (cylinderTopRadius: the
      Cylinder's card shows Top radius after Radius at the Radius; Radius 1, Height 0.6, then Top 1.17 typed - the stage's
      mesh two rings, 1 m on the ground and 1.17 m at 0.6 m; one undo step; 0.005 refused; otherShapes' Cylinder card
      with its Top radius) - seen failing on the page without the hook, and on anim c7c650b's floatdisk.js;
      browser_check.mjs 112, all passed; check_composer_server 54 (its checkoutDefault wants an nwn5e-ee-anim beside the
      worktree: none there).
  - *Limits:* the game's ring has 40 segments and starts its u at -0.083, so its flames sit at other angles than a
    32-segment Cylinder's (Smoothness 40 matches the count). A library texture is tinted by the shape's Colour - white
    shows `fxpa_flame02` as it is (the default `#99ccff` would make it pale blue).
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing installed - Play in game writes the mesh into the
    request's draft; `fxpa_flame02` is the game's.
  - Test row SHP-8 (the coordinator's tmp `shp8_row.csv`, for docs/tests.csv): the flame ring in the Composer and in
    game.

## A game effect's own keys and transtimes (2026-11-18, #39; KF - game-side)

  The owner, 2026-11-18, TMO-4: "in the bench the blue flame in the middle doesn't stop and pressing Play in game doesn't
  show the angled flame ring at the bottom in-game. it shows via !vfx" - the bench's copy of `vdr_eleshield` had its ring
  Cylinder02 at alpha 0 (its fade-in keys dropped) and Flame01 emitting for ever (its birthrate keys unread) - offered
  "keep a game effect's own keys": **"yeah"**. And TT's review: a re-saved game effect wrote transtime 0 where the game's
  own is 0.25 (vff_piranha, vff_revivedrag...). Shared files, announced: src/data/vfx_model.h, src/data/vfx_model.cpp,
  tests/data/vfx_model_test.cpp; no draft or model of nwn5e's changes.
  - *Measured* (values only, over the game's 3,714 effect models - 2,887 compiled, 827 ASCII - their impact, duration and
    cessation; a list counts when it has 2+ rows or a row off the geometry's value):
    - meshes: 660 keyed - alpha 543, turn 302, move 203, scale 1: all four what M4's writer writes;
    - emitters: 1,090 keyed - birthrate 861, move 239, alphaStart 239, alphaEnd 227, sizeEnd 83, sizeStart 75, velocity
      48, turn 33, xsize / ysize / randvel 32, lifeExp 31, mass 29, particleRot 19, colorStart / grav 12, colorEnd /
      spread 11, drag 9, fps 8, frameStart / frameEnd / bounce_co 7, the lightning, point-to-point and combinetime ones
      5-7. **Kept: birthrate, alphaStart, alphaEnd** - with the move and turn kept since 0.42.3 they cover 878 of the
      1,090 wholly (80.6%; the library's 390: 690 of 852, 81.0%) and touch 1,040; birthrate alone 60.5%; sizeStart /
      sizeEnd would add 5.3%. The rest stay dropped;
    - transtimes: impacts 0.25 (296), 0 (85), 0.15 (2); durations 0.25 (96), 0 (20); cessations 0.25 (93), 0 (24);
    - `vdr_eleshield`: Flame01 keys its birthrate 100 -> 0 in the impact (100 to 0.567 s, 0 at 0.6 s, none after) and its
      alphaStart (0.6 -> 0 by 0.967 s; 0 in the duration), and its velocity and randvel (2 -> -0.2, 0.4 -> 0 at 0.533 s:
      dropped); Ring01 / Ring02 their birthrate (50; 50 -> 0 in the cessation); Cylinder02 (alpha 0) its alpha (0 -> 1
      by 0.3 s, 1, 1 -> 0 by 0.333 s); every transtime 0.25.
  - *Calls:*
    - `VfxKeys.birthrate`, `alpha_start`, `alpha_end`: rows t v, an emitter's (controllers 88, 84, 80; the ASCII model's
      `birthratekey`, `alphaStartkey`, `alphaEndkey`) - each list as read, a single row at the geometry's value too (a
      value the animation sets: the game's controller holds it into the next animation);
    - `VfxEffect.transtime[3]`: impact, duration, cessation, -1 not known; every reader's rule (TranstimeValue): 0 to
      `kVfxTranstimeMax` (60), else -1; -0 is 0;
    - the compiled reader: an animation's transtime at its header's +0x74; each animation's copy of an emitter keeps its
      88 / 84 / 80 lists, of a plain mesh (a plane) its 128 (alpha) and 36 (scale) lists and its move and turn as a node's
      (a single row at its geometry's place is no motion); a mesh that is a node too keeps its move and turn for both;
    - the ASCII reader: `transtime` in an animation; a trimesh's `alphakey` / `scalekey`, an emitter's three lists (a
      count or an endlist, any case) into the same keys; a plane takes its node's keys (a mesh that's also a node keeps
      its fade and scale on the plane alone);
    - the draft: `<anim>_transtime = T` right after `lengths = ` and `<anim>_birthrate` / `_alpha_start` / `_alpha_end`
      after the other key lines (any section), each only when there - nwn5e's own drafts keep their bytes;
    - WriteVfxModel: an emitter with its own birthrate list in the impact, the duration or the cessation (OwnEmission)
      gets, in each animation, its own list there and only it - none where it has none (the game holds the value:
      Flame01 stays off) - in place of nwn5e's emission keys (VA-2's restore, an emit time's, a start's, a one-shot's, the
      cessation's off), and its geometry keeps its rate; its `alphaStartkey` / `alphaEndkey` after its move and turn keys.
      LastKeyTime counts the three. **The transtime rule: a kept transtime is written as it is; none kept, TT's rule** -
      with one exception: an impact with a **mesh beat** (a plane the bench placed: its `mesh` named) whose first key there
      is off its rest is written 0, so a Drop in on a game effect's mesh beat falls from its height instead of rising from
      its rest for 0.25 s (MBK-1 again). Argued: the game's own planes never have a mesh, so no game effect as read
      changes (an override for any plane would have changed 7 of the library's effects - a plane of theirs off its rest
      at the impact's first key, the game blending it 0.25 s); limit: a Drop in on a game effect's node or emitter keeps
      the game's transtime (0.25: it rises from its rest first).
  - *What changes* (hashes only, HEAD 5109810's vfx_model.* against KF's):
    - every draft nwn5e keeps writes the same draft and model - 564: the 4 fixtures, Detect Magic's 4, the main
      checkout's build\vfxedit (library game 437, nwn5e 18, chunks 40, saved 30, the bench's request) and the anim
      helper's cache (30); Detect Magic's committed models (data/files) are the same bytes (VfxKeptDraftsUnchanged);
    - every game model the bench reads, with KF's keys and transtimes taken out, writes what HEAD wrote (23,766 compiled,
      25,108 with any_mesh, 6,576 ASCII; the same ones fail to read on both sides). With them, 459 compiled and 51 ASCII
      models write another draft (412 and 48 another model). Of the library's 390 effects 255 drafts and 225 models
      change: 215 gain emitter keys (205 their own emission), 22 plane keys, 255 transtimes; 66 write another transtime
      line than TT's rule did (83 lines);
    - 2 of the library's effects have an emitter with its own emission and no cessation keys still on at the cessation:
      it emits there, as in the game.
  - *Checks:* test.bat 621 -> 628 (7 new, 6 failing first with the fields alone; the guard passing); 48 deliberate breaks
    on scratch copies against the VFX tests alone - 47 fail a test, the 48th (TranstimeValue without its isfinite) is
    equivalent (the range test refuses nan and inf already). nwn5e-tools: check.js --helper with a helper built from
    this worktree, 416 effects byte for byte (290 failures with the old core.js); check_effect.js's 12 KF checks and
    browser_check.mjs's eleshieldPlaysItsOwnKeys, both failing on the old sim / page; the trait's stand-ins.
  - *Library format 20* (nwn5e-tools dev/vfx_library_format.h): 20 when the core's `data::VfxEffect` has `transtime`
    and its `data::VfxKeys` (an emitter's keys) `birthrate` and the 19 rule holds; else the 19 / 18 / 17 / 16 rule. The
    install with KF exports the library once more at the game's next start.
  - *Limits:* a game effect's velocity, randvel, size, colour, life, mass and the other emitter keys are still dropped
    (Flame01's pull back after 0.53 s isn't kept); Rate / Opacity typed scale an emitter's own keys, but Starts after and
    On for don't change an emitter that has them (the bench's note says so); a transtime only from a model read - a
    draft of nwn5e's has none.
  - Test row KF-1 (the coordinator's tmp `kf_row.csv`, for docs/tests.csv): VFX_DUR_ELEMENTAL_SHIELD from the library,
    on the stage and in game.
  - *Fix round 1* (the review, 2026-11-19: "Needs fixes"):
    - **nwn5e's own models read as before KF.** At format 20 the library's nwn5e drafts (ExportNwn5eModels, and the
      in-game editor's LoadEntry, read served `n5_*.mdl` with ReadVfxAsciiModel) would have kept their own generated
      emission keys (VA-2's restore, an emit time's, the cessation's off) and transtimes as "the game's": an On for on
      Detect Magic dropped, a Drop in on n5_ds_00's emitter blending 0.25 s from its rest. The rule: a model with
      nwn5e's mark - `kVfxModelMark`, "# nwn5e", at the start of a comment line before its newmodel; the writer's line is
      built from it ("# nwn5e VFX editor (!vfxedit): <name>") - keeps no transtime, no emitter's own list and no plane
      key. The mark and not the writer's line alone: 14 of the 18 library nwn5e drafts are the generators' (death-save
      and stealth markers, bleed: "# nwn5e (tools/data/<script>.py)"; the Short Rest's copy "# nwn5e: ..."), n5_ds_00
      among them. Proved on the real served models: all 18 write HEAD's drafts and models again.
    - **The animations are the model header's list** (+0x78, count +0x7C; anim_recipe.cpp's ReadAnimClip's), not the
      first "\0impact\0" with a length in (0, 60): 81 compiled models (creatures and placeables - c_air, a_gnome, the
      golems...; none in the library) had a node named impact taken for one, with a garbage transtime; vim_ballfire's and
      vps_bugs' 0 s animations were missed - now read (their transtimes, their keys; a 0 s length isn't taken as one). The
      one-shot's seconds the listed impact's. Stripped of KF's keys, every game model writes HEAD's bytes but those 81
      (their lengths no longer garbage); 479 compiled models now write another draft (420 another model), 398 carry a
      transtime (no garbage ones).
    - The library's numbers again: 255 drafts and 225 models of its 390 effects change; **204** gain their own emission
      (205 counted an ASCII emitter keyed outside the three animations); 84 transtime lines differ from TT's rule, in 66.
      **31 of its 40 chunk drafts change** at format 20 (their transtimes; 27 their planes' keys) - nothing visible: the
      stage draws chunks at rest.
    - The bench (nwn5e-tools): Rate / Opacity typed 0 take an emitter's own keys to 0 (silent, in game too); the keys
      follow from what they were when the edit began (a value typed through 0 comes back); from keys all at 0 a value
      above 0 is refused, the status saying how to get them back (Ctrl+Z, or the drop below) - M5's pattern for a tint
      back from 0. A keyed emitter's timeline bar is its keys' span (sim.js emissionSpans), locked, titled;
      **Use this card's timing** on its card drops its own keys (Starts after / On for taking their first span) so the card
      times it - one undo step.
    - Checks: test.bat 628 -> 630 (2 new, both failing first; the three older synthetic compiled models given the
      model header's list, as the game lays it out); 11 deliberate breaks of the fix - 10 fail a test, the 11th (the mesh
      fade gate) is equivalent (a plane takes no keys from such a model, a node's fade is cleared).
  - *Fix round 2* (the scoped re-review, 2026-11-19; nwn5e-tools only, no src change): the timeline's rows had become
    seconds a redraw on big game effects (vff_hellball 4.4 s) - the spans sampled every 120th of a second, each sample
    scanning every item's keys. sim.js emissionSpans now works them out from the key rows as the stage plays them (rows
    out of time order too: vdr_irongreen's OmenEmitter05 has two of its model's lists under one name, read as one - what the
    game does with two isn't traced), each emitter once a redraw: vff_hellball 10 ms (2 ms as the page asks), none of the
    library's 437 over 16 ms (88 before); the spans of the helper's 732 own-emission emitters match the stage frame by
    frame. Use this card's timing keeps first start to last end and serves opacity-only keyed emitters too (140 in 38
    library effects); after Rate 0 the refusal names only Ctrl+Z and the Game tab, and the drop is off on keys all at 0.

## Decal textures copied, flipbooks and soft textures clamped in v (2026-11-19, #39; DC)

  SHP-8 on 0.99.81 (the owner): SHP-8's flame ring - a Cylinder (open) with a Top radius, `fxpa_flame02`, a green Colour -
  showed "a top very thin color at the edge on the rim of the cylinder" on the stage and in game, and "when played in
  game the tinting doesn't show". The SHP-8 diagnosis (the coordinator's tmp `SHP8-diag-report.md`) read the engine
  (nwmain, the same build as the win32 exe) and the game's files, numbers only; the owner: "Go" to its recommendation.
  No src or tests change; no DLL change, no install.
  - *The engine evidence:*
    - **decal -> unlit.** `Material::ParseField` keeps a TXI's `decal`; for a decal mesh `PartTriMesh::Mat()` sets the
      colour to (1, 1, 1, alpha) and returns without its `SetMaterialv` calls (ambient, diffuse, emission), and
      `RenderFlat` turns lighting off for the draw, so the shader is vsvt / fst: `globalColor x texture`. A shape's Colour
      (selfillum, ambient, diffuse) never shows. 21 of the game's 734 TXIs say `decal 1` (read 2026-11-19, values only):
      `fxpa_flame02`, `fxpa_symbl_003`, `fxpa_web`, `fxpa_fern`, all 7 of its flipbooks (`fxpa_flame02`, the 5
      waterfalls, `tnp_bird02`, `tsw01_panel01`), `c_air_skin`, `c_water_skin`, `vdu_tex_shade`, glass and GUI ones;
    - **frames as their own textures.** `GLRender::CreateTexture` cuts a flipbook's frame out (`ImageGetSubImage`) as a
      texture of its own; each gets `SetTextureParameters(.., GetClamp)` - repeating both ways unless the TXI clamps,
      mipmapped, linear. So a mesh whose v runs to 1 samples the frame's own wrapped bottom row at its top rim: the line
      (the frame's top row is black, its bottom row bright). Our soft textures (alpha 77 at the top row, 253 at the
      bottom) show it fainter;
    - **clamp bits 1 / 2 / 4.** `SetTextureParameters`: bit 1 sets WRAP_S, bit 2 WRAP_T, bit 4 WRAP_R to CLAMP_TO_EDGE,
      else REPEAT - W1's `clamp 3` is both ways (seen, no longer inferred); `clamp 2` is v alone.
  - *Calls:*
    - **one rule** (floatdisk.js `stillTextures`; the generators through files.js, the VFX Bench's Play in game through
      composer.js `played`): only a **still** shape (its part has no motion - a moving one's texture goes through its
      material, M2 / W1) and only its own texture - a game part's and your own file's are drawn as their models have them,
      and the game's own effects (opened from the library) are never copied;
    - **a library texture whose TXI says decal** (any value but 0, as atoi reads it) - or **a flipbook on a mesh whose v
      stays within one repeat**, decal or not (the call the brief left open: the line shows on any flipbook and the owner
      wants none; the game's 7 are all decal anyway, so only a flipbook beside your own parts or in ovr\ is concerned) -
      names a **copy**, `n5l_<12 hex digits>` (`copyName`: cyrb53 of "copy:<texture>:<clamp>:<key>" - one copy per
      distinct result, never per colour; fix round 1: the key, `copyKey`, is 14 hex digits of the source TXI's text as
      read and 14 of its picture's bytes - the content that decides the copy, so a game update to either names a new copy,
      and the name fixes its bytes. Both are at hand where a copy is made - the generators' copy path and the bench's
      server - and cheap: one cyrb53 pass over a TXI and a 49 KB picture; the page is told the key by the texture route,
      `X-Copy-Key`, and the bake route refuses a key that isn't the picture's now, 409). Its picture is the game's (its DDS when it has one: the one copy path, `motionPicture`); its TXI (`copiedTxi`)
      the source's lines as they are but every `decal` line - `proceduretype cycle`, `numx`, `numy`, `fps`, `blending
      additive` kept - in its own line ends, then one clamp line, the union of its own and `clamp 2` when every v of the
      mesh is within [0, 1] (0.001 either side: `vClamps`; u may tile - Repeat around, Elemental Shield's 3). Repeat up
      above 1: a decal texture's copy without the clamp, a plain flipbook as it is (it repeats, line included, as the
      game's does);
    - **the soft texture** of a still shape within one repeat: its own name (`shapeTextureName` hashes `:clamp2` after
      the colour and blend - M1b's rule: a TXI change never shares a name) and a TXI saying `clamp 2` (`softTxi`; an
      Additive one's `blending additive` first). The brief's alternative - `clamp 2` when every mesh using the soft
      texture is within one repeat - would make one name's TXI depend on the other meshes of a build (and of a Play: one
      vfxedit\ for all), so the clamp went into the name instead; Repeat up above 1 (the only mapping that tiles v) and a
      moving shape keep the unclamped name (a scroll needs its texture0 to repeat);
    - **W1's `clampedTxi` drops `decal` too** (`copiedTxi` with clamp 3; the union with the game's own clamp lines, 1-3,
      is always 3): the diagnosis's risk - a clamped breathing glyph on a decal picture skipping our material, reading
      whatever colour and alpha the mesh before it left. Every W1 copy of a non-decal texture is byte for byte as before;
    - **files and ownership:** files.js `shapeTextures` (the clamped soft texture and its TXI, kind "shape"),
      `copyTextures` / `copyWrites` (kind "copy", owned by the `n5l_` name; `/data/files/n5l_*` git-ignored - the game's
      art); both build.js write them beside what names them, build_derived carries them into --out; the summary's line
      and --json's `copies: [{ copy, texture, clamp }]` (a clamped soft texture's `shapes` entry `"clamp": true`);
    - **the bench** (nwn5e-tools, branch `dc-decal-tools`): the texture route says `X-Decal: 1` (its TXI says decal, never
      a material's) and `X-Clamp: <bits>`; it finds an `n5l_` copy where it's built (data\files, saved\); the bake route
      takes `copies: [{ texture, clamp }]` - checked again against the rule on the texture's own TXI, else refused - and
      a shape's `clamp`, writing the copy, its TXI and the clamped soft texture into vfxedit\ (all checked before any is
      written). Play in game names them (composer.js `played`, the stage's mesh loader's TXI: one not read yet waits -
      "still loading - try again in a moment"). app.js - KF's file, kept small - three edits: gameImage keeps the route's
      `X-Decal`, `X-Clamp` and (fix round 1) `X-Copy-Key` as the picture's marks; request()'s bake body carries `copies`;
      the `meshTexture` check hook says `decal`;
    - **the stage** (view.js): `upload` clamps each way by the bits; a picture's `clampBits` (X-Clamp: a saved spell
      mesh's copy, a clamped soft texture) uploads it so, a flipbook's frames too; a Composer still shape's mesh carries
      composer.js placeOne's marks - `clampV` (its v within one repeat) and `copies` (a library texture) - and is drawn
      with t CLAMP_TO_EDGE (u repeating) when its soft texture, or a library texture the game copies (`copied` on the
      loader's X-Decal / X-Cycle), wants it; **decal drawn unlit**: a mesh whose texture says decal (X-Decal) and that the
      game is given as it is - a game effect's own plane (Elemental Shield's ring), a chunk, a Composer game part - is drawn
      texture x alpha (the particle program's `u_unlit`), as fst draws it; a Composer still shape's copy and any moving
      mesh drawn lit. The stage knows decal from the texture route (the game's TXI), not the effect library - no library
      format change (KF's 19 -> 20 untouched); a texture the route can't give (no checkout: the library's copy) draws lit,
      as before.
  - *What changes byte-wise* (the probe `tmp\animtex\dcprobe\bytes.js`, each project built --check before and after):
    - **models / drafts:** only those with a still shape that names a decal or flipbook library texture (its bitmap now
      `n5l_...`) or a still Soft shape within one repeat (its bitmap the clamped `n5s_...`). The Floating Disk's committed
      composition (no shapes): its 7 models byte for byte; no committed Composer project in the repo. Every other model,
      draft and TXI byte for byte (a game part, your own file, a moving shape, a Repeat-up-2 shape, a plain library
      texture);
    - **TXIs:** new `n5l_<...>.txi` (the copies) and the clamped soft textures' `n5s_<...>.txi` (`clamp 2`, or
      `blending additive` + `clamp 2`); W1's `n5u_<...>.txi` of a decal texture (fxpa_symbl_003's: its `decal 1` line
      gone); none other.
  - *Built:* anim branch `dc-decal` (this section's commit: floatdisk.js, files.js, both build.js, build_derived.js,
    .gitignore, the READMEs; floatdisk check, floatdisk and composer check_build, check_derived) and nwn5e-tools 819c998
    on branch `dc-decal-tools` (composer_server.js, view.js, composer.js, composer_page.js - Save's status names the
    copies -, app.js, README; check_chunks, check_composer, check_composer_server, composer_check.mjs) - in worktrees of
    their own; the coordinator lands them.
  - *Checks* (before -> after; each new check seen failing first - the anim ones on the code before the change, the
    bench's on HEAD's view.js, composer.js, composer_page.js, app.js and composer_server.js):
    - anim: floatdisk check 110 -> 117 (+7: the TXI's words, copiedTxi, vClamps, the names, the rule, the page, files.js;
      W1's TXI and files.js checks amended: decal dropped), floatdisk check_build 13 -> 14, composer check_build 30 ->
      33 (SHP-8's ring in --saved, TIN-1's glyph and a tiled ring in data\files, the clamped soft textures), check_derived
      18 -> 19, composer check 24 - all pass. The soft-texture checks of before (M1b, S3) give their domes Repeat up 2
      (as before DC); the still clamped ones are DC's. A mutation check (19 breaks of the DC code, in a temporary copy):
      every one fails a DC or W1 check;
    - nwn5e-tools: check_chunks 44 -> 48, check_composer 48 -> 49 (with an LF composition.json; the CRLF checkout's round
      trip fails before and after), check_composer_server 55 -> 58, composer_check.mjs 101 -> 104 (dcFlameRingNoLine: 4
      mm under SHP-8's rim at most 1 brighter than with the ring deleted - 26 on HEAD's code: the line; dcFlameRingPlays;
      dcMeshBeatCopyClamped), browser_check.mjs 112, check_effect 157, check_area 21, beats and parts - all pass. Old
      Composer steps amended where a still Soft dome within one repeat now names its clamped soft texture.
  - *Fix round 1* (the review, 2026-11-19: Needs fixes): at nwn5e-tools 819c998 check_composer_server was **57 of 58** -
    its `shapeSummaryMatchesGenerator` compared the page's still shape meshes, now carrying composer.js placeOne's
    `clampV` / `copies` marks, with the generator's whole (its last run had come before that composer.js edit, so the 58
    above was wrong). The step now compares the meshes without the marks and asserts each mark is what the generator's
    own rule gives (`copyKind`, `vClamps`): zotest's still Soft domes and flat shapes `clampV`, the wrapped one none. The
    copy's name now hashes its content (`copyKey`, above) - the texture route's `X-Copy-Key`, the bake route's key check
    (409). check_composer_server takes `--port` (run on 18793, beside another run on 18776). After the last edit, every
    check again: anim floatdisk check 117, floatdisk check_build 14, composer check_build 33, check_derived 19, composer
    check 24; nwn5e-tools check_chunks 48, check_composer 49 (LF), check_composer_server 58 (port 18793; on 819c998's
    composer_server.js the two key steps fail), composer_check.mjs 104, browser_check.mjs 112, check_effect, check_area,
    beats and parts - all pass. The mutation check: 24 breaks (5 of the key), every one caught. Byte-wise as above but for
    the copy's name: the owner's mbkglyph disc names `n5l_1cef2bd88098` (its picture and TXI the same bytes).
  - *Limits:* unseen in game until DC-1 / DC-3: that the copy keeps cycling under its own name (very likely: the cycle
    hangs on the texture's own TXI) and that the game honours `clamp 2` on a mesh (TMO-5 passed for `clamp 3`). A
    tinted **game part** on a decal texture still shows no tint in game (its model's meshes are given as they are) - a
    follow-up if wanted. On the stage, game planes on decal textures are now drawn unlit - brighter than before where
    their material was dark (Meteor Swarm's red flames now orange, as in game).
  - *For the bench:* restart vfx_bench.bat. *For the game:* nothing installed - Play in game writes the copies into
    vfxedit\; a saved spell mesh's into saved\.
  - Test rows DC-1 to DC-4 (the coordinator's tmp `dc_rows.csv`, for docs/tests.csv).

## Moving textures keep their blend, a flipbook moves from DC's copy, Scroll round clamped in v, and the play folder read again (2026-11-19, #39; MV)

  DC-1 on 0.99.82 with Texture motion **Scroll round** (the owner): the rim line came back on the stage; in game some
  plays were an opaque block of the tint, others the flames tinted but on black. The MV diagnosis (the coordinator's tmp
  `MV-diag-report.md`) read the engine (nwmain, the same build as the win32 exe), the played files and the logs, numbers
  only; the owner, 2026-11-19: "go" to its recommendation - "Fix the race in the game's dev layer: re-read the folder
  before each Play. Moving textures keep their blend: each material gets a tiny TXI carrying the source's `blending
  additive`. A flipbook plays from the copy the still ring already uses, which keeps its frames. Scroll round keeps the
  vertical clamp on the stage and in game, so the rim line stays gone. Clamping only applies when the motion is purely
  sideways. TMO-1 and TMO-2 get retest notes on the board." No src or tests change here; the dev layer (nwn5e-tools)
  changes, so it needs an install.
  - *The engine evidence:*
    - **the race.** `CExoResMan::GetKeyEntry` finds a file only in a key table's in-memory index; a directory added with
      detect_changes is listed again only by `CExoResMan::Update` - at most once a second (0xF4240 us), and only when the
      directory's mtime, in whole seconds, changed (`CheckForModificationsInStorage`, then `RebuildTable`). The bench writes
      a play's `n5u_` MTR (and any new `n5l_` / `n5s_` picture or TXI) 0.4-0.6 s before the model loads; the dev layer read
      the folder again only when the **model** wasn't seen, and the models' names (n5vx0001...) are always listed from the
      last session. So a new material was found only when the poll fell between its write and the load - about half of
      all first plays. A missed MTR: no SharedMaterial, the Material's own slot 0 the missing `n5u_` picture (white), the
      engine's lit program, ambient = diffuse = 0: selfillum x white, alpha 1 - **the tint, flat and opaque**;
    - **the blend.** A mesh's blend comes only from `<mesh bitmap>.txi` (`Material::Init` -> `ParseField`: `blending`,
      `decal`); texture0's TXI isn't read for it and an MTR has no such field (`SharedMaterial::ParseField`: texture0-10,
      the shaders, parameter, transparency, twosided, renderhint ...). An `n5u_` bitmap with no TXI keeps the constructor's
      (0, 1): SRC_ALPHA / ONE_MINUS_SRC_ALPHA, depth written. So every moving texture lost `blending additive`: a 24-bit
      one (fxpa_flame02, fxpa_symbl_003, fxpa_globe ...) at alpha 1 drew **on black**. TMO-1's domes (alpha 0.3) and
      TMO-2's (Opacity 0.35) couldn't tell alpha-blending from additive: their additive verdicts are void;
    - **texture0's own TXI is its texture's** (`CAurTextureBasic::Init`): its flipbook (`proceduretype cycle` makes a
      `CycleTIDTextureController`, stepped by every `BindInUnit`: a flipbook under texture0 **cycles**) and its clamp
      apply; its `decal` and `blending` don't;
    - **sideways only leaves v as it is.** vsn5uvmove: v' = (v - pV) / s + pV - fract(n5ScrollV t), s = 1 + breathe
      sin(...): with scroll v, turn and breath 0 (as the material carries them), s = 1 exactly and v' = v (1 ulp at most,
      harmless with CLAMP_TO_EDGE); the Rate enters only s.
  - *Calls:*
    - **the play folder read again before every play** (nwn5e-tools dev `vfx_play_folder.h`, `vfx_editor.cpp`
      PlayEffect - the bench's Play in game and Preview on me - and WriteAreaModels, once an area request): the models
      written, then `RefreshFolder` (vfxedit\ and its saved\, each rebuilt by the engine's own UpdateResourceDirectory -
      fix round 1; removed and added back only when it can't), then the models looked for. Rather than
      "whenever a file the request names isn't listed": the model's name can't tell (listed from the last session, its
      file rewritten), and the files a play names - the model's planes' `n5u_` materials, each MTR's texture0 (a copy, a
      soft texture), optional TXIs - would be a second copy of the generators' naming rules in C++, for no gain: the read
      is one listing of a few hundred files, at a click, never a frame (the engine lists the folder after every write
      there anyway). And no clock: two writes in the same mtime second are listed too, which a wait can't promise. A
      model still not listed after the read fails the play as before. Checked with stand-ins (a standalone translation
      unit over the header, as `vfx_library_format.h`'s); the DLL builds with it;
    - **a moving texture's material gets a TXI of its own** (floatdisk.js `motionTxi`, `moveTextures`' `txiOf`):
      `n5u_<...>.txi` - its texture's lines the engine reads for a mesh from its bitmap's TXI but `decal`, as they are,
      in their order, LF: `blending`, and (fix round 1 - the review read `Material::ParseField`, 0x4c5850, which also
      takes `envmaptexture`, `bumpmaptexture`, `bumpyshinytexture` and `bumpreplacementtexture` there; "only blending" was
      wrong) the environment and bump map lines; `decal` never carried - it would drop the material. The game's 734 TXIs:
      `blending` 42 (13 additive, 23 default, 4 normal, 2 punchthrough), `envmaptexture` 50, `bumpmaptexture` 22,
      `bumpyshinytexture` 33 - no map line on an fx*, n5* or vfx* texture. None of these lines: no TXI, the name as
      before. Hashed into the name (`:txi:` and its text: M1b's rule). W1's clamped material as it was (its copy's TXI
      has the lines already);
    - **a moving shape's texture0 is a still one's** (floatdisk.js `gameTextures`, DC's rule with "v within one repeat"
      read as `sidewaysClamps`: scroll up, turn and breath 0, v within [0, 1]): its copy (`n5l_`) for a decal texture or
      a flipbook moving sideways - the still ring's own copy (one per texture, never per speed: a 49 KB picture once, ~18
      B of TXI per motion), its frames kept, `clamp 2`; a decal texture scrolling up or turning gets its copy unclamped; its
      clamped soft texture sideways only. Option (d) of the diagnosis, not (a2) - W1's per-motion copy route with clamp 2,
      exact too but a picture per speed tried. A mesh W1 clamps (it breathes) is W1's; a game part's and your own file's
      textures as before (DC's scope) - their material still gets its TXI. Never `clamp 3` (u runs round the ring and must
      repeat), never a clamp with a scroll up, a turn or a breath. The sideways clamp's own mark in the name, `:clampv`
      (never W1's `:clamp`), only where the rule clamped it (texture0 a copy or the clamped soft texture);
    - **files:** files.js `shapeTextures` / `copyTextures` call `gameTextures` (a moving shape's clamped soft texture and
      copy made, written and owned as DC's); `motionMaterials` reads each texture's TXI (given `sources` and `copies`:
      both build.js; uvprobe gives none - TMO-1's domes as they were); `motionWrites` writes `<name>.txi` (kind
      "material", owned by the `n5u_` name; git-ignored with the material); the summary's line and --json's `side`,
      `txi`. `stillTextures` keeps DC's answer for a moving part (a bench from before MV);
    - **the bench** (nwn5e-tools, branch `mv-move-tools`): the stage marks a moving shape's meshes as a still one's
      (composer.js `stillMarks` by `sidewaysClamps`: clampV, copies; W1's breathing ones as they are), so view.js's
      `wrapBits` draws the ring scrolling round clamped in v, u repeating - no rim line - and scrolling up repeating;
      Play names each material as the generators do (composer.js `played`: `gameTextures` then `moveTextures`, the TXI from
      the texture route's new `X-Motion-Txi`, a copy's from its source, a soft texture's from its shape's blend) and sends
      `side` and `name`; the bake route reads the TXI itself (this request's soft texture or copy, else the copy path,
      vfxedit\ included), writes `<name>.txi`, refuses a name it wouldn't make (409) and a `side` with a scroll up, turn
      or breath (400). `X-Motion`'s clamp only on W1's copy route (a route-A material's texture0 says its own clamp in
      `X-Clamp`). The stage's blend for a moving texture was already the game's (its texture's additive), so only the game
      changed there. app.js (KF's file): gameImage keeps `X-Motion-Txi` as the picture's `motionTxi` (DC's edit site).
  - *What changes byte-wise* (the probe `tmp\animtex\mvprobe\bytes.js`: HEAD f97f398's generators against these, on the
    Floating Disk, the owner's three projects and eleven moving test meshes): the Floating Disk's files, the owner's
    `mbkglyph`, `test` and `tmoglyph` (none moves) and every still part byte for byte; of the moving ones, unchanged: a
    Normal soft dome, W1's breathing glyph, a plain texture with no TXI (`tcn01_rock01`) scrolling round, a turning game
    crate (plc_a03: no blending or map lines); changed: the flame ring scrolling round (0.5 or 0.01) - its draft names a new
    material whose MTR names the still ring's copy `n5l_bf0dee9aee86` (already written for the still ring) + its TXI;
    scrolling up and the glyph turning - new unclamped copies (`n5l_5cba05886e23`, `n5l_805918c2c51b`) + TXIs; an
    Additive soft dome scrolling up - the same MTR bytes under a new name + its TXI; one scrolling round - the clamped
    soft texture `n5s_bfc2ebac4509` + TXI. (Fix round 1, the review's 25 shape cases and the gems', `mvprobe\bytes2.js`,
    completing the list:) a **Normal** soft dome scrolling round - texture0 its clamped soft texture `n5s_a40318b044a1`
    (TXI `clamp 2`), `:clampv` in the name, no `n5u_` TXI (a Normal soft texture has no blending line); the glyph
    scrolling round - the still glyph's own copy `n5l_1cef2bd88098` (clamp 2); a game flipbook with `blending default`
    (the waterfalls, tnp_bird02, tsw01_panel01) - its copy (`n5l_618bbb57d245` for tcm02_wtfall01, clamp 2 sideways) and
    an `n5u_` TXI `blending default\n`; a texture with map lines (the gems' c_diamond, a chest's plc_gold - a moving game part on one too) - an `n5u_` TXI of them
    (`envmaptexture evmap_irrid\nbumpyshinytexture evmap_irrid\n`): the only change fix round 1 makes. So each new `n5u_`
    TXI is its texture's own lines: `blending additive\n` for every fx* texture here, `blending default\n` for the six
    other flipbooks, the map lines for the gems, crystals and metals that have them. No committed file.
  - *Built:* anim branch `mv-move` (floatdisk.js, files.js, both build.js, uvprobe.js's comments, build_derived.js's
    comment, the READMEs; floatdisk check, floatdisk and composer check_build, check_derived) and nwn5e-tools branch
    `mv-move-tools` (dev/vfx_play_folder.h, dev/vfx_editor.cpp, dev/README.md; composer_server.js, composer.js,
    composer_page.js, view.js, app.js, README; check_chunks, check_composer, check_composer_server, composer_check.mjs) -
    in worktrees of their own; the coordinator lands them, and the dev layer needs an install.
  - *Limits:* unseen in game until MV-1..3: that the n5u_ TXI makes route A additive (from the code: `Material::Init`
    runs before the SharedMaterial and nothing else sets the blend), that the copy's flipbook cycles under texture0 (from
    the code: `BindInUnit` steps it), and that the refresh holds on Windows (the 1-second poll and whole-second mtime were
    read in the Linux build). A missed picture before this install may stay white for the session (unchecked whether the
    engine keeps an invalid texture by name): restart the game after installing. A moving texture's `envmaptexture` and
    bump map lines are carried (fix round 1); whether the plain lit fslit our material pairs with vsn5uvmove draws them
    is unknown.
  - *Fix round 1* (the review, 2026-11-19: Needs fixes):
    - **the refresh, non-destructive** (Important): the remove-and-add was safe for effects on screen (the review traced
      DestroyTable, CRes::Dump and the name-keyed renderer caches) but heavy - the idle cache dumped, "Resource %s still
      in demand during table deletion" logged per resource in use, and the tables' priority climbing 2 a play
      (`CExoResMan::AddKeyTable` lifts a new table above every table of its million whose priority is at or above its
      own - read in nwmain: priority / 1,000,000 by multiply-shift, then +1 past each; the array kept highest first,
      GetKeyEntry's search order). Now each folder is rebuilt by the engine's exported `UpdateResourceDirectory` ->
      `UpdateKeyTable(name, 2)` -> `CExoKeyTable::RebuildTable()` (what the 1-second poll runs; 1, or 0 and an engine log
      line when no such directory table - read in nwmain), the old remove-and-add only when the export is missing or
      returns 0 (said once in nwn5e.log). An area request reads them once, after all its slot models are written
      (`ReadFolderForPlayAll`), not once a model. The policy (`FolderReader`, `ReadFolderForPlayAll`) is in
      `vfx_play_folder.h` and its stand-in check is committed now, `dev\checks\check_play_folder.cpp` (`.bat` to run it:
      14 checks; 15 mutants of the header, the pre-MV policy among them, all caught). The same nwmain read shows saved\
      (asked for at 15000009) coming in at 15000011, above vfxedit\'s 15000010: since #161 a name in both is saved\'s;
      vfx_editor.cpp's comment said "under the editor's" and now says so;
    - **the map lines** (above) carried in the material's TXI;
    - **the byte lists completed** (above), the MV-3 row's arithmetic (Repeat around 3: one repeat - a third of the ring -
      in 100 s at 0.01, a full turn in 300 s) and the TMO-2 note (its Opacity 0.35 the diagnosis's inference).

## nwn5e's area copies drawn as on a creature; a lone copy fades in once (2026-11-19, #39; AH - game-side)

  SHP-4: `!vfxarea n5_zotruth 0.6 1 0` showed the red-tinted dome **untinted** and **blinking** (fading in over and over),
  where the bench's Play in game of the byte-identical model was right. The AR diagnosis (the coordinator's tmp
  `AR-diag-report.md`) found both in the client's area code; the owner's checks confirmed them (`graphics.hilite.enabled
  = false`: the dome red; two copies: no blink). The owner, 2026-11-19: "go on the area fix", to the recommendation "one
  change in nwn5e's own area hook, for our models only (the game's own areas untouched): switch the white highlight off
  on each area copy, so tints and Glow draw as on a creature, in both lighting modes; give a lone copy its own
  start-and-loop sequence in place, so it fades in once and stays". In core/anim_area_scatter (VA-5's LoadEffect hook);
  game-side, so it needs an install.
  - *The engine evidence* (win64 nwmain.exe read at the hook's anchors, named from the Linux build's symbols):
    - **the highlight.** `CNWCAreaOfEffectObject::LoadEffect` (rva 0x97f770) calls `CNWCAnimBase::Hilite(Vector(1, 1,
      1), slot 3, glow 0)` (rva 0x965cf0) on every copy it loads - three call sites, one per group, and the area code's
      only Hilite calls. Hilite sets the slot's on flag (+0x9c + 4 x slot) to the global bound to
      `graphics.hilite.enabled`; `RefreshHilite` then sets the model's illumination to the lowest slot on: emission =
      selfillum + (1, 1, 1) - clamped to white without enhanced lighting (the tint lost), x (2, 1.067, 1.067) with it (the
      BR "brighter"). Creature plays are never hilited. `CNWCAnimBase::UnHilite(slot)` (rva 0x968590 - the Linux
      0x9cb550's bytes: a model, slot < 5, the flag cleared, the same RefreshHilite) leaves no slot on: illumination (0,
      0, 0), colour shifting (1, 1, 1) - the creature play's state;
    - **the lone copy.** The loader gives group 1's copy k a start of k x 300 ms, so copy 0's is 0, which the update
      (`UpdateInstancePosition`) skips as nothing pending: with group 1 of one, that copy is never placed and never gets
      `SetAnimation` (VA-5 places it when group 1 has several). A placed copy gets: `SetInstanceRandomPosition`,
      `AttachToScene` the first time, `SetAnimation(1, i)` (rva 0x981420: state 1, its timer the impact's length - at
      least 1 ms - and the impact played once, flags 1), `ResetTimer` (alive = the row's lifetime), start 0, and the
      row's one-shot sound at its spot by chance (SoundOneShot, SoundOneShotPercentage); `AttachToScene` also sets the
      copy's attached flag (+0x1c0), which `SetScene` uses to attach it again. `AIUpdate` starts the duration, looped,
      when the timer runs out (inlined on win64: state 2, the copy's own SetAnimation(2), flags 0); a lifetime above 0
      ends in the cessation and `SetMoveTimer`'s start of 1 - re-placed at random in the radius.
  - *Calls:*
    - **ours: a copy whose model's name starts "n5"** (any case; the copy's own CResRef at +0x30, read whatever the row):
      nwn5e names every model it ships or plays so (n5_, n5v_, n5t_, the bench's n5vx / n5va plays), and none of the
      game's 113,538 resources (both keys and ovr\) starts so. Not "our rows": rows 100-116 show the game's Entangle,
      fogs and flask models, made for the highlight (Blade Barrier's swords are full-bright only through it), and the
      bench's rows may name the game's models too;
    - **the highlight off**: after the original LoadEffect (it returned 1), for each loaded copy of ours whose slot 3 is
      on, `UnHilite(3)`. With `graphics.hilite.enabled` off the loader set nothing and the hook calls nothing;
    - **a lone copy started in place**: group 1 of one, copy 0 at start 0, not alive, state 0, its model ours -
      `SetAnimation(1, 0)`, the call the update makes for a placed copy; its lifetime left 0 (what `ResetTimer` gives a
      lifetime-0 row, and the only one that stays where it stands), so it fades in once and holds its duration until
      the area ends (its cessation then, from `StopEffect`, as before). It gets neither the one-shot sound nor the
      attached flag, as before: it is the object's own animation base, attached by the object's `AttachToScene`, and
      never was placed. Done before the client's `SetScene` attaches the
      copy: `Gob::PlayAnimation` only adds a run to the model's list, which attaching (`Gob::SetScene`: the scene's lists,
      an "UpdateScenes" event) leaves alone, and the first frame both animates it and counts the area's timer down;
    - **nothing re-highlights them**: the hover's highlight (`UpdateMouseCursorAppearance`) is slot 1 on copy 0, drawn
      over it while the pointer is there and cleared alone; `RefreshHilite`'s other callers only read the slots; the
      `graphics.hilite.*` callbacks only set globals; a re-placed copy gets no Hilite. Only a self-illumination or alpha
      visual effect put on the area object itself would set copy 0's slot 3 again - nwn5e's areas never get one;
    - **guards**: every function is found by its bytes before the hook installs - Hilite and UnHilite (entries, the same
      RefreshHilite), LoadEffect's direct calls to Hilite (exactly three, each slot 3 and glow 0), the copies' array
      (0x118 each, at +0x78) and total (+0x1a0) in LoadEffect, a copy's model (+0xc8) and name (+0x30) in its LoadModel
      (`CNWCAnimBaseVisualEffect`'s vtable by RTTI, slot 3), SetAnimation (state +0x1d0, timer +0x180) and the update's
      impact call landing on it; any missing: "SKIP area copies", VA-5 alone. Every offset and value the hook reads
      memory or calls with (13, VA-5's three included) is one of those patterns' bytes at a fixed place
      (`kOffsetAnchors`, fix round 1): the tests check each, the hook again at install (else nothing hooked), so a
      pattern found in the game proves its offset. At each load: counts as the loader lays them out (1-255 in all, group
      1's 0..all), every copy of that class - else logged once and the area left alone. The offsets and patterns live in
      `data/anim_area_engine.h` (VA-5's moved there too, unchanged).
  - *Tests* (test.bat 630 -> 636; fix round 1 -> 640): `AreaOwnModels`, `AreaGameModelsNotOurs`, `AreaLoneCopyStarted`,
    `AreaLoneCopyOtherwiseLeft`, `AreaCopiesLayoutGuard`, and `AreaEnginePatternsInTheGame`: every pattern the hook
    checks, in the game's own nwmain.exe, as the hook checks it (unique, in the right function, the call targets lining
    up; skipped without the game - NWN_GAME_DIR or Steam's folder). Mutation check: 27 deliberate breaks - 13 of the
    rules, 14 of the anchors (an offset, a slot, a call position, the class, VA-5's step) - each failed a test.
  - *Built:* anim branch `ah-area` (src/core/anim_area_scatter.{h,cpp}, src/data/anim_area_copies.{h,cpp},
    src/data/anim_area_engine.h, tests/data/anim_area_copies_test.cpp); the DLL builds. Rows AH-1 (the dome red, fading in
    once, steady - one and two copies, day and night, enhanced lighting on and off) and AH-2 (the game's own areas as
    before).
  - *Limits:* unseen in game until AH-1. A lone copy of ours with a lifetime keeps its duration for good (never
    re-placed or replayed), as the game keeps a lone copy at the centre. At Glow below 1 our area copies are now lit by
    the area, as on a creature, not full-bright. Emitters weren't checked (the AR diagnosis: not drawn by
    `PartTriMesh::Mat`). The lone copy's impact timer counts down from the first AIUpdate after LoadEffect, so its
    first countdown includes the area's load time (the review's M3; a placed copy's starts at its placement) - on a slow
    load the duration may start before the impact has played out, the fade-in then ending in a step to its end value.
  - *Fix round 1* (the review, 2026-11-19: Needs fixes):
    - **the offsets tied to their patterns** (Important): the named constants were kept equal to the patterns' bytes
      only by hand - the reviewer changed each of 9 and the suite stayed green, so after a game patch a pattern updated
      without its constant would have the hook read a wrong pointer and crash the client. Now `kOffsetAnchors` names, for
      each of the 13 constants the hook reads memory or calls with (`kCopies`, `kCopyStride`, `kGroup1Count`,
      `kTotalCount`, `kCopyAlive`, `kCopyStart`, `kCopyState`, `kCopyTimer`, `kCopyModelName`, `kCopyModel`,
      `kCopyHiliteOn`, `kAreaHiliteSlot`, `kAreaImpact`), the pattern carrying it and the byte where its displacement
      or immediate starts; `data::OffsetAnchored` checks the little-endian bytes there (`AreaOffsetsTiedToTheirPatterns`,
      `AreaOffsetAnchoredRule`), and `InstallAreaScatter` checks them all again before anything else. Each of the 13
      constant mutants now fails;
    - **the Hilite calls checked as the hook checks them** (M1): one shared function, `data::AreaHiliteCallsAsLoaded`
      (every direct call to Hilite in LoadEffect: exactly three, each ending kHiliteSlot3CallPattern - glow 0, slot 3),
      used by the hook (FindCallsTo's sites) and by the game test (its own E8 scan of LoadEffect); the pattern parser the
      tests used is `data::BytePattern` / `BytesMatch` now (`AreaBytePatterns`, `AreaHiliteCallsChecked`);
    - **the placed copy's sound and flag** (M6): above. test.bat 636 -> 640; 49 mutants (19 of the rules, 30 of the
      anchors and constants), all caught.

## The target-count marker (2026-11-19, #171; TC)

  Retired 2026-11-20 (the owner): replaced by TF; files removed in the commit "retire #171's first disc markers and the TP probe".

  #171's target window, the game session's (B): the owner, 2026-11-19: "it should be a permanently visible indicator on
  top of each enemy saying how many darts/bolts are assigned to them, not how many are left"; look, glow and size "A, A,
  as you said" - a bold white number with a dark outline on a small dark-red disc, about 0.4 m, 0.35 m clear above the
  head; the red glow (VFX_DUR_GLOW_LIGHT_RED, n5tw_mark) stays the "picked" mark. Then (the coordinator, same day) a hard
  requirement: it must show **while paused** - the owner picks targets paused and the whole window runs paused.
  - **The contract:** `int TargetCountMarkerVfx(int count)` (src/core/anim_target_counts.h, one inline line over
    data::TargetCountRow): the visualeffects row for 1..11, -1 otherwise. The game session applies it as a temporary
    duration effect tagged n5tw_count, swaps it as the count changes and removes it on OK / Cancel.
  - **Rows** 2200-2210 (data/rows/visualeffects.tcount.txt; data::kTargetCountFirstRow, one source of truth - a test
    reads the file): type D, the model on the head (Imp_HeadCon_Node), OrientWithObject 0. Past the looks' 2100-2199 and
    the Detects' 2092-2099. New clash check, tests/data/anim_vfx_rows_test.cpp: no visualeffects row in two files (rows
    and shipped looks), no rows file in the looks' range.
  - **Paused (the TC diagnosis, the engine read):** pausing disables the scene's animation, and an emitter is ticked -
    so its particles born - only while the scene animates (or its Gob is force-animated: the walk-to ring alone); an
    emitter with no particle isn't drawn. A camera-facing sprite applied while paused would appear only on unpause. A
    mesh is drawn as soon as the client attaches the effect, which happens the moment the message arrives, paused or
    not; removing the effect detaches it at once. (The stealth and death-save markers, Single sprites, have the same
    paused gap - not changed here.)
  - **So the marker is geometry:** four one-sided discs facing out round the head (0.4 m, 0.2 m from its axis, centre
    0.55 m over the head node). The game culls a back face, so a disc is only seen from the side it faces - never
    mirrored; the head attach is position only, turned to the world's axes - upright, never upside down; one disc faces
    the camera within 45 degrees, two at 45 degrees show the number twice at 71% width. Drawn unlit (`decal 1`). No keys
    (a key waits for the clock). One inert emitter lists each model in the VFX Bench (its library takes models with an
    emitter); it never makes a particle.
  - **No cessation (TC-1 fix, #171):** the models have only impact and duration. A cessation (it was 0.2 s, empty) makes
    the client re-attach the model each frame while its timer runs - on the object's dt, 0 while paused - and then keep it
    3 s, so a removed marker stayed (forever paused) and overlapped the new. With none, PlayAnimation returns 0, the
    effect is detached and deleted the next frame. Never a cessation of length 0: its timer is 0 and it never finishes.
  - **Art:** tools/data/tcount/build.js draws the 11 textures from its own digit shapes (lines and arcs, stroked; no font)
    and writes the models; outputs committed in data/files (own, deterministic art, as the death-save marker's;
    build_derived.js is for art made from the game's files). check.js: 11 distinct textures, clear outside the disc, an
    outline round every white pixel, a white shape per digit, the same bytes twice, data/files in step (README there).
  - Tests: 6 in test.bat (the mapping and bounds, the names, the rows file, the four discs as the bench reads them and
    their draft round trip, the clash check), 6 mutations caught; check.js's 8 mutations caught. Rows TC-1 (in game,
    paused) and TC-2 (the VFX Bench) with the game session's wiring.

## The marker's probe: glowing tally marks on a quad our vertex shader turns to the camera (2026-11-19, #171; TP)

  Retired 2026-11-20 (the owner): replaced by TF; files removed in the commit "retire #171's first disc markers and the TP probe".

  The owner, 2026-11-19, on the four-disc marker: "glowing tally marks that always face the camera, is that possible?" -
  then "go with the probe, white-gold on red". A probe beside the marker (2200-2210 and n5_tcnt_* unchanged); if picked,
  all 11 follow. tools/data/tprobe (build.js, check.js, README.md - the full reading there).
  - **The route:** a particle faces the camera but isn't born while paused (TC); a mesh is drawn at once, and a vertex
    shader runs on every draw with no clock. So one quad whose EE material (`.mtr`, found by the mesh's bitmap name)
    names our vertex shader with the game's lit fragment shader, as the moving textures (vsn5uvmove, TMO-1 / MV), and
    the blend from a TXI under the same name (MV: the only place the engine reads a mesh's blend).
  - **The shader's inputs, read (values only):** the game's `vslit` is its defines, `inc_standard`,
    `ApplyStandardShader()`; with lighting on, `inc_common` declares `m_mv` (model-view), `m_proj`, `m_view`, `vPos`,
    and the varyings `vPosView`, `vVertexNormal`; `inc_transform` writes `vPosView = m_mv vPos`, `gl_Position = m_proj
    vPosView`; `inc_fog` / `inc_keyhole` write `fFogFragCoord` / `fFragCamOffset` (the latter only with
    `KEYHOLING_ENABLED`). The engine (nwmain-linux): `GLRender::ConnectShaderData` looks up `m_mv`, `m_m`, `m_view`,
    `m_proj` ... for every program; `GLRender::SetUniforms` uploads `m_view` = the stack's view, `m_mv` = view x model;
    the model matrix at a mesh's draw is the Gob's (its attachment moves it to the head node every frame, paused too)
    times each part's position, turn and scale (`Gob::PartDraw` pushes them before the part draws). So `m_mv (0,0,0,1)`
    is the mesh node's origin in view space, following the head. The engine's shader header: `#version 330 core`, the
    quality defines, `varying` as out / in.
  - **The billboard** (vsn5bill_s / _c, after `ApplyStandardShader()`): centre = the node's origin (0.55 m over the
    head node); the world's up in view space = `m_view`'s third column; right = up x view +z, normalised (straight
    above: the screen's x); up = view +z x right (**spherical**: in the view plane, facing the camera fully, from above
    too) or the world's up (**cylindrical**: upright, turning about it only); `vPosView = centre + scale (vPos.x right +
    vPos.z up)` - the quad made round its node's origin in the x-z plane - `gl_Position` again, the normal right x up;
    `fFogFragCoord = 0`, `fFragCamOffset = 0` (no fog box round an additive glow, no keyhole: as the game's decal glows
    and the four discs). Counter-clockwise, facing the camera: the front face from any side.
  - **Fragment shader `fslit`, not `fst`:** `fst` multiplies by `globalColor`, set to white only for a decal mesh;
    `fslit` with ambient, diffuse, specular 0 and selfillum 1 gives the texture exactly (specular colour from albedo x
    diffuse = 0), the same by day and night. **No `decal`** in the TXI: a decal mesh is drawn by vsvt / fst, its
    material dropped (SHP-8). `blending additive` also makes the mesh not opaque (`Material::GetIsOpaque`), so it's drawn
    with the see-through ones.
  - **Art:** n5_tally_03 / _07, 256 x 128, 32-bit, alpha 255: white-gold #fff2c0 bars, 10 px, in a soft glow of the
    picked glow's red (VFX_DUR_GLOW_LIGHT_RED = progfx 408 AlphaLightRed, 1.0 0.4 0.4), black border; a five = four bars
    and a rising strike; every count 1-11 fits (checked). Own strokes, no font.
  - **Files and rows:** data/files n5_tprobe_s3 / c3 / s7 / c7 (.mdl one 0.5 x 0.25 m quad, .mtr, .txi), n5_tally_03 /
    _07, vsn5bill_s / _c; rows 2211 (spherical 3), 2212 (cylindrical 3), 2213 / 2214 (the same, 7) in
    data/rows/visualeffects.tprobe.txt (a file of its own: the marker's test wants exactly 11 rows in tcount's), type D,
    head node, OrientWithObject 0; no keys, no cessation.
  - **Checks:** check.js (the files, determinism, the art and layout, the TXI / MTR / model / shader lines, the shader's
    maths in JS over 1080 cameras, the rows, data/files in step): 38 of 38 mutations caught. The clash test sees
    2211-2214 in tprobe's file (it failed first). Both shaders compiled and linked with fslit by this machine's GL
    driver (a 3.3 core context) under the engine's header with every include expanded from the game's shaders, in all
    48 combinations of the quality defines; the game's own vslit + fslit pass the same rig, a broken copy of ours fails.
  - **Unproven until TP-1 in game:** that the quad faces the camera and turns while paused; that the probe looks as
    the art does (additive on the scene); which way reads better.

## The final marker: black tallies on each spell's glow, facing the camera (2026-11-20, #171; TF)

  The owner, 2026-11-20, after TP-1: the face-on (spherical) style - "camera level for max readability for every angle" -
  and "for now let's keep your generated style, but can we do it black on blue/orange/violet glow for magic missile,
  scorching ray and eldritch blast respectively?"; then "can you make them 99.9% black so they don't get seen as
  transparent?" (the bars #010101). tools/data/tally (build.js, check.js, README.md - the full reading there).
  - **The call (the game session wires it):** `enum class TargetCountStyle { MagicMissile, ScorchingRay, EldritchBlast }`
    and `int TargetCountMarkerVfx(TargetCountStyle style, int count)` (src/core/anim_target_counts.h, over
    data::TargetTallyRow): -1 outside 1..11 or for a style the enum doesn't name. The spells are the game side's to map
    (rules::kMagicMissile, rules::kScorchingRay; Eldritch Blast later) - agreed with the game session. The one-argument
    `TargetCountMarkerVfx(count)` stays until they switch, now Magic Missile's look; the four discs' rows keep
    data::TargetCountRow, unused.
  - **Rows** 2215-2247 (data/rows/visualeffects.tally.txt, a file of its own; data::kTargetTallyFirstRow - a test reads
    the file): style s, count n at 2215 + 11 s + n - 1 - Magic Missile 2215-2225, Scorching Ray 2226-2236, Eldritch
    Blast 2237-2247; type D, head node, OrientWithObject 0. The clash test sees them (it failed first).
  - **Two quads a model** (an additive glow can't draw black): the bars - #010101 with the strokes' alpha, alpha-blended
    (TXI `blending default`; material `transparency 1`: never opaque, drawn with the blended meshes, cut at alpha 0.2) -
    and the glow - the spell's colour (#3a7bff, #ff8a1e, #a040ff), additive, **black where the bars are opaque**, made 1
    cm in front (y -0.01). Bars bitmaps n5_tfb_NN (one per count, all three spells), glows n5_tfg_<ss>NN; pictures
    n5_tfbp_NN / n5_tfgp_<ss>NN (256 x 128, 32-bit; the probe's layout and glow kept); 33 models n5_tf_<ss>NN. Each
    TXI is its blend line alone; the pictures have none (fix round 1, below).
  - **The order, read (nwmain-linux):** no sort by distance. `Scene::AddPartsTreeToDrawBuckets` files a mesh by its
    material: additive (blend dst ONE) -> bucket 4; blended and not opaque (`Material::GetIsOpaque`: false for
    `transparency`, SharedMaterial +0x1f0 as `SharedMaterial::ParseField` sets it) -> bucket 3; a bucket in tree order.
    `Scene::RenderSinglePass` draws buckets 2 and 3 in `RenderDynamicGeometry` (3 with blend and alpha discard on:
    `fAlphaDiscardValue` 0.2, else -1 - `GLRender::SetUniforms`; the standard `AlphaDiscard` cuts alpha <= it) and bucket 4
    ("Additive") last. So the bars come first, the glow after, every frame. Black under the bars, the glow adds nothing
    there; 1 cm nearer the camera, the bars' depth never cuts it. Either order gives the same core and clear pixels
    (check.js composites both); only the 1-pixel rim differs.
  - **The shader `vsn5bill_sl`:** vsn5bill_s (TP) with `- vPos.y * toEye` in `vPosView` (`toEye = -centre / |centre|`):
    a vertex's y moves it along the eye's line through the centre, so the glow lies 1 cm (x scale) nearer, its picture
    the bars' scaled about the same screen point by |centre| / (|centre| - 1 cm) - under a pixel at the edge from 2 m.
    Compiled and linked with fslit in all 48 quality variants by TP's rig (a broken copy: 0 of 48); since fix round 1
    paired with fsn5unlit (below).
  - **The bench:** its library export (nwn5e-tools dev/vfx_editor.cpp, the dev layer) copied each plane's bitmap's
    picture and logged TP's and TF's material bitmaps "not found" on every export; now a name with no picture of its
    own is read as a material and its texture0's picture copied under texture0's name (dev/vfx_library_texture.h; the
    stand-in check dev/checks/check_library_texture.bat). The bench still doesn't show these models: its library lists
    effects with an emitter, and these have none (adding one, as the discs had, would be an inert node beside a
    camera-facing pair - not done); a plane named after a material would draw white there.
  - **Checks:** check.js (the files, determinism, the layout as the probe's, the pictures - #010101, never 0 0 0 seen -
    the glow black under the bars, the composite both ways on four grounds, the TXI / MTR / model / shader lines, the
    shader's maths over 1800 cameras, the rows, data/files in step): 45 of 45 mutations caught. test.bat: 4 new tests
    (the style call and its bounds, the names, the rows file against the constants, the models as the dev layer reads
    them with their materials and TXIs), the clash check widened.
  - **Fix round 1 (the review; the owner: "1A, 2A"):**
    - **1A, an unlit fragment shader `fsn5unlit`** for both quads. The review: the picked target's red glow is a
      creature illumination - `ApplySelfIllumination` -> `CNWCCreature::Hilite` slot 3 (1, 0.4, 0.4) -> `RefreshHilite`
      -> `Gob::SetIllumination(..., propagate)` -> "propagateillumination" to the attached Gobs, the marker's too - and
      fslit adds it to the emission: under enhanced lighting (gamma correction, the game's default) the glow went pale /
      amber / magenta. fsn5unlit is the game's standard fragment setup (`inc_standard`) with LIGHTING, FOG and
      KEYHOLING 0: texture0 into the colour space and back, opaque white (the alpha first read from the material -
      see TA below), cut by the standard `AlphaDiscard` (none in NO_DISCARD
      variants). No light, emission, fog, keyhole or area-targeting highlight (TP's faint box in a spell's shape gone).
      The bars use it too (black can't be tinted, but the highlight would grey them; one program). 48 of 48 variants
      with vsn5bill_sl (`materialFrontEmissive` and the targeting uniforms not active), a broken copy 0 of 48.
    - **2A, the downsample lines gone:** this engine reads no `downsamplemax` / `downsamplemin` (no "downsample" string in
      nwmain-linux or nwmain.exe; not among the texture TXI's keywords). Each TXI is its blend line alone; the 44 picture
      TXIs are deleted. check.js fails a downsample line, a picture's TXI and a marker file left in data/files the
      generator doesn't make; 51 of 51 mutations caught (the fragment shader's among them).
    - Rows TF-1 / TF-2 with Enhanced Lighting on (Options, Lighting), the mouse off the targets after picking (the
      illumination arrives on the next hilite refresh): the glow its own colour, untinted. The dev layer's
      MaterialTexture0 takes an MTR's last `texture0` line, as the engine (each line overwrites).
    - Far off - a bar under about a pixel wide, at the pictures' small mip levels - the bars' alpha falls to 0.2 or
      under and is cut while the glow keeps its black slots: on snow or sky those show the background.
  - **TA (the owner: "go ahead with the shader fix for the tally"; TF-1 / TF-2 failed on 0.99.100):** a picked enemy is
    red-glowed (VFX_DUR_GLOW_LIGHT_RED) and translucent, and what is attached inherits the translucency - most likely the
    mesh's material alpha, which fsn5unlit multiplied in - so the tally marks were hard to read (fine over a PC, not
    translucent). fsn5unlit's output alpha is now texture0's alone, no materialFrontDiffuse (no longer declared; inactive in
    all 48 variants with vsn5bill_sl; a broken copy 0 of 48). Colour, alpha discard and everything else unchanged; only
    data/files/fsn5unlit.shd changes. Cost: a material alpha can't fade the marker (it has no keys or cessation, so none
    ever did). Row note for TF-1 / TF-2's retest: the tallies stay solid over a picked (red, translucent) enemy.
  - **Unproven until TF-1..3 in game:** the bars drawn black over the glow in game (the order and depth as read); the
    glow untinted over a picked target with enhanced lighting (fsn5unlit); readability from the usual camera distance
    (the probe's size kept: bars 2 cm wide).
  - **Candidates to retire (the owner's call):** the four discs (rows 2200-2210, data/files n5_tcnt_01..11 .mdl / .tga
    / .txi, tools/data/tcount, data::TargetCountRow / TargetCountModel and their tests) and the probe (rows 2211-2214,
    n5_tprobe_s3 / c3 / s7 / c7 .mdl / .mtr / .txi, n5_tally_03 / _07, vsn5bill_s / _c, tools/data/tprobe).

## One burst: the game's fountaining placeables served without the "dead" loop's detonate (2026-11-20, #172; LD)

  The owner, 2026-11-20, on the Academy's lectern (PLC_K02), which "fountained a huge amount of debris" when broken (the
  DB recon: a placeable's debris is its model's "die"; the client then loops "dead" for the 2 s until the server deletes
  it, and 16 of the game's placeable models carry a detonate in that "dead", so every loop re-fires the chunks and
  smoke): "For the lectern A" - copies of those 16 without the "dead" loop's detonate, built from the owner's game at
  build time and never committed. tools/data/README.md, "One burst", has the full reading.
  - **The 16** (tools/data/oneburst.js `MODELS`): plc_c03, plc_c04, plc_c10, plc_h10, plc_i02, plc_k02, plc_q03,
    plc_x0_fgs, plc_x0_grw, plc_x0_tds, plc_x0_wds, pwc_mage_001, px2_b04, px2_c10, px2_e01, px2_g02 - every
    placeables.2da model whose "dead" has a detonate and which has an Explosion emitter for it (plc_x0_grs has the
    detonate and no emitter: left as the game's). check_derived.js recomputes it from the game and fails on drift.
  - **Made and served:** build_derived.js (build.bat unchanged) reads each from the game (its keys, ovr\ over them) and
    takes the "dead" animation's detonate events out through the repo's own model code - src/data/vfx_model.cpp's new
    `ReadModelAnimations` / `DropAnimationEvents`, run by mdlmesh's new `--anims` / `--drop-events` - then writes it
    under its own name into the build's data\files (git-ignored by name, owned there by name). nwn5e serves data\files
    over the game's keys, where the held torch's flame (`GenerateTorchFlames`) goes. **Compiled for 14, as the torch's:**
    the game's bytes changed in place, the "dead" header's events array made the one the game keeps for an animation
    with none (offset, count, allocated count 0), its freed entries zeroed; **ASCII for 2** (plc_c10, pwc_mage_001 are
    text in the game): the "dead" block's detonate lines taken out. Every other byte or line is the game's: the "die",
    its detonates, emitters and keys, the other animations. 34 events in all (every "dead" event of the 16 is one).
  - **Checks:** test.bat 4 new tests (the compiled "dead" compacted and emptied, nothing else touched; a cut file or
    text refused; the ASCII block's lines) - 650 tests. check_derived.js: the list against the game; each copy against
    the game's model (animations, events, emitters, every other byte / line); without them every other derived file
    byte for byte the same; skips said; a one-burst name in --out rewritten.
  - **The three with no "die" burst get DE's debris (the owner, 2026-11-20: B):** plc_h10 (Cot), plc_i02 (Birdbath) and
    plc_q03 (Flag3) have no event in "die" - their debris was only in "dead" (the Cot's and the Flag's "dead" also sinks
    the mesh: "die" and "dead" look swapped). Their "die" stays the game's - their detonates aren't moved into it - so
    they have no debris of their own (a "die" detonate that sets off a chunk emitter), and the game session's death
    trigger gives them DE's material debris: the Cot and the Flag cloth small (row 2254), the Birdbath stone large (row
    2251). Until it lands they break with none (the mesh still sinks, the Destroyed sound plays).
  - **Fix round 1 (the review approved the code):** LD-1 breaks with Shatter only (the Academy's lectern and benches are
    Useable 0 / Static: not clickable); the three's docs say B; `readModels` keeps an animation mdlmesh prints with no
    name (`anim (\S*)`: the events after it landed on the wrong one); `derive` reads each copy back beside the game's
    (`verifyCopy`: the same form, emitters and animations, the "dead" the game's minus its detonates, none left) - a bad
    copy stops the build; `fountains` says a "dead" inherited from a supermodel isn't looked at (none today: 192 with a
    supermodel, no chain with a detonating "dead"); mdlmesh's modes with the wrong number of arguments (or an unknown
    option) give the usage line, exit 2. New game-free check `tools/data/check_oneburst.js` (4 checks, each failing
    first). Mutations re-run: 14 of 14 C++ breaks caught.
  - **Row** LD-1 (#172): break the Academy's lectern - one burst of debris and smoke, no fountain over the next 2 s; a
    bench unchanged.
