# The Spell & Attack Bench - design

*Owner's request, 2026-11-10: "I think we will need a spell bench and eventually an 'attack' bench where mocap, VFX,
spell and perhaps sound will be combined for a final preview of the whole thing."*

## Intent and why

A spell or attack in nwn5e is made of pieces that live in different places today:
- the caster's animation (the Animation Recipe Bench, the Mocap Bench);
- the effects (the VFX Bench);
- what flies to the target (the game's projectile models);
- the timing and sounds in the game's tables;
- the rule itself (the game session's scripts).

Nobody sees them together until they're all in the game, and even then only by hand-testing. The owner wants one
bench that shows the whole thing, a caster casting at enemies, lets each piece be picked and timed, and then **writes
what the game needs** so the spell plays that way for real (owner: "It should write what the game needs").

The first spell is **Chromatic Orb** (owner: "Chromatic orb sounds like a good one").

## What it is (as the owner described it)

**The scene.** "I see the player character and one or more enemy characters in different, selectable, formations."
- Formations: one, a line, a cluster, a ring, around the caster.
- Also chosen: the distance, which enemy is the target, the enemy type, a camera view.

**The caster's parts.** "For the PC we pick animation, cast VFX, hit vfx, attack, single target or area and timing and
sound for each of those." In the order they happen:
1. **Animation:** the cast animation (a recipe or an NWN animation; e.g. `n5_cantrip`, #149/#150).
2. **Cast VFX:** what shows on the caster while casting, and where (a hand, the chest, the head, the feet).
3. **Attack:** how it reaches the target - projectile, beam, touch, melee, or "on the target" (nothing travels).
   - For a projectile: its model and its path (homing, arcing/ballistic, ...).
4. **Target:** single, area (with a radius), or a chain/leap to further targets (Chromatic Orb's leap).
5. **Hit VFX:** what plays on each creature hit.

Each part has **its own sound** and **its own timing**.

**The timeline.** One row per part, with the release, the hit and each sound as markers to drag. They snap to the
animation's frames, so the hand's push lines up with the orb leaving it. *(Proposed in the mockup; owner: "That sound
pretty close".)*

**The mockup** the owner saw: `2026-11-10-spell-attack-bench-mockup.png` (and its `.html`) beside this file. Its three
columns are the scene settings, the stage, and the caster's five part cards. The timeline runs under them. The mockup
fixes the arrangement, not the styling.

**Preview, then Play in game.**
- The bench previews the whole thing.
- **Play in game** plays it in the running game: the owner's character casting at enemies spawned in the chosen
  formation, in a test module (below).
- **Play in game is a staged replay** (owner, 2026-11-10: "Go with your recommendations"):
  - The bench's unsaved settings play on the owner's character at once, as the VFX Bench's Play in game does.
  - The orb always hits, and the leap plays when the bench has one.
  - No attack rolls, no spell slot, no damage.
  - The real test is casting the spell normally after Save and a game restart.
  - The replay uses the same player as the real cast (below), so the replay and the real spell can't drift apart.
- Until the test module exists (piece 3), Play in game spawns the formation around the character wherever they stand,
  through the dev layer, and clears it on Stop. *(My call.)*

**The game plays the timeline as dragged** (owner, same answer). The release, the hit, the leap and each sound happen
in game when the bench says, not when the engine happens to:
- The animation side's player releases the missile at the release mark and plays the hit, its sound and the leap when
  the missile arrives.
- The rule's damage waits for that same moment: the rule asks the look how long until the hit, and applies the damage
  then. Today it lands at once, while the orb is still flying.
- This is the one-line change in the game session's code (below), agreed with it before it lands.
- How the game session does its side (mechanics, 2026-11-10, as Magic Missile already does):
  - Every roll stays at the cast: the attack, the dice, the matching-dice check, and the leap's target, picked from
    where the creatures stand at the cast.
  - Each hit's damage is created at the cast and applied by a delayed command on the target. The caster keeps the
    kill credit, and the damage still lands if the caster dies mid-flight.
  - A leap's delay is the previous hit's time plus the leap's own flight.
  - Picking the leap's target when the orb arrives would need a timer on its side. Not for the first version.
    *(My call: at a flight of well under a second, nobody moves far enough to matter.)*

**Save writes what the game needs:**
- the spell's presentation: the clip, the cast and hit effects, the missile, the leap, the sounds and their timing
  (in the look file, and in the tables where the engine reads them - below). The cast times stay the rule's
  (Constraints);
- anything new it uses: recipes, effect models, effect rows, sound definitions.

**How the game plays Chromatic Orb today** (checked in the code, 2026-11-10). Most of the orb's look isn't in the
tables, so a Save that only wrote spells.2da rows wouldn't change it:
- Rows 1153-1159 (`data/rows/spells.if_spells2024.level1b.txt`) carry the same presentation on all seven:
  - ConjTime 1500 (hand), the evocation chant and conjure sounds;
  - CastAnim out, CastTime 1000;
  - Proj 0 and no ProjModel.
- The missile (`VFX_IMP_MIRV_*`), the leap's beam (`VFX_BEAM_*`) and the hit effect (`VFX_IMP_*_S`) are picked per
  damage type in the game session's code (`OrbLookOf`, `src/core/rules_spells2024.cpp`).
- The hit and the damage land as soon as the spell's script runs, while the missile is still flying.

**So Save writes a "spell look", and the rule asks for it** *(answered by CLAUDE.md's pattern)*. CLAUDE.md: "A game rule
that plays an animation calls the animation session's API in one line (e.g. `RequestFlourishClip`) ... Which clip plays,
its timing and swaps live in the animation files." The bench extends that to the whole look:
- Save writes an animation-owned look file per spell, with its variants: the clip, the cast effect, the missile, the
  hit, the leap, the sounds and the timings.
- The rule's code calls the animation side in one line, in place of `OrbLookOf`, and the look plays from that file.
- Table columns the engine itself reads (the cast animation, the sounds of the conjure / cast phases) are written as a
  presentation overlay that the game session's served rows take in.
- The exact signature and the game file's lines go to the game session before any game file changes (its rule: "A new
  hook in a game file is announced, with the lines, before it lands").

**What the game session asks of the function** (mechanics, 2026-11-10):
- Safe to call from inside a spell's impact handler (the rule is inside a script run then), and never blocking.
- With no look for that row, it returns 0 and plays nothing, so the rule's current visuals stay as the fallback.
- Creatures are passed as object ids. The variant is the spells.2da row: 1153 is the master, and 1154-1159 are the
  six types.
- It lands only after Chromatic Orb's own open tests (L1-19, L1-20) pass. The change gets a retest row of its own.

**The overlay** (agreed with mechanics, 2026-11-10):
- One animation-owned file in the repo, `data/looks/spells.look.txt`, versioned and installed with everything else.
  build.bat already copies all of `data/` into `build\`.
- It uses the row format: a header naming its columns, then partial rows. The game session's row server merges it
  last.
- The server takes only presentation columns: CastAnim, ConjAnim, the Conj* / Cast* visuals and sounds, and Proj*.
  Anything else (ImpactScript, class columns, Innate, ConjTime / CastTime, targeting) is refused and logged.
- It's read at start-up, so a change needs a game restart.

**Where Save writes.** While the owner works, Save writes into the game checkout's `build\` (the overlay into
`build\data\looks\`, and the look files beside it). The owner's game then picks them up at the next start, as with the
Recipe Bench (`build\animedit\recipes\`) and the VFX Bench (`build\vfxedit\saved\`). Once the owner accepts a look,
it's copied into the repo's `data/looks/` and ships with every install, as recipes move into `data/anims/`.

**Variants.** A spell with several looks (Chromatic Orb's six damage types) is one set of settings, with the parts
that change per look picked per variant: the orb model, the hit effect, the sounds.

## Goals

- One spell, Chromatic Orb, working end to end:
  - picked and timed in the bench;
  - previewed;
  - played in game against enemies in formation;
  - saved into the game's data, so the game plays it that way.
- Each later piece builds on that: projectiles and beams, sounds with effects, more spells, then weapon attacks.

## Non-goals

- **The rules.** Damage, saving throws, Chromatic Orb's "leap on matching dice" and targeting rules stay with the game
  session (mechanics). The bench decides how a spell looks, sounds and is timed; the rule's script uses what the bench
  made (e.g. the leap plays the bench's leap effect and timing).
- **New external assets.** Owner: "Let's use the game's sound library. The fewer external assets we use, the better."
  The game's own models, effects and sounds come first.
- **Replacing the other benches.** Animations are still made in the Recipe / Mocap benches, and effects in the VFX
  Bench. This bench picks their results, times them and puts them together.
- **The Mocap Bench work** stays on hold (label `on-hold:mocap`) until the owner's capture setup is fixed.
- **A perfect preview.** The game is the truth; the preview is close (the VFX Bench's rule).

## Anti-goals

None named. Owner: "it's a first version, will keep at it until it works".

## Constraints

- **A local app** like the other benches (nwn5e-tools: Node on 127.0.0.1, no dependencies, a `.bat` to start it;
  never hosted).
- **The game's data** (NWN:EE 8193.3x):
  - spells.2da's presentation columns: ConjTime, ConjAnim, ConjHeadVisual/HandVisual/GrndVisual, ConjSoundVFX,
    ConjSoundMale/Female, CastAnim, CastTime, CastHeadVisual/HandVisual/GrndVisual, CastSound, Proj, ProjModel,
    ProjType, ProjSpwnPoint, ProjSound, ProjOrientation, ImpactScript;
  - visualeffects.2da: Type_FD, Imp_* / Dur_* / Cess_* nodes, SoundImpact / SoundDuration / SoundCessastion.
  - CastAnim / ConjAnim name a fixed set (out, self, up, area, point, touch, ... for cast; hand / head for conjure).
    A clip of nwn5e's plays instead through the animation side's `RequestCastClip` swap (#150), which the game session
    calls at the cast.
  - A cast plays `cast<x>` then holds `cast<x>lp` while CastTime runs. Both are swapped (n5_cantrip / n5_cantriplp).
- **Projectiles:**
  - The game flies a projectile model from the caster's spawn point to the target (ProjModel / ProjType / ProjSpwnPoint).
  - It has 38 of its own (`vpr_*`), including orbs in each damage colour: vpr_ectoacid01, vpr_ectocold01,
    vpr_ectofire01, vpr_ectoelec01, vpr_ectosonc01, vpr_ectoevil01, vpr_ectonatr01, ...
  - The VFX Bench can't open, preview or play projectile models today.
- **Sound:**
  - The game's effect sounds are IMA ADPCM WAVs (format 17, mono, 44.1 kHz).
  - Scripts can only play, stop, move and set the volume of sounds.
  - Audio is OpenAL Soft, which has EFX (pitch shifter, low/high/band-pass filters, echo, reverb). The game never uses
    it; nwn5e's DLL can.
  - #154's `!sfx` proof (0.99.2) checks EFX works in the game's own context. **The sound-effects part depends on it.**
- **Ownership** (CLAUDE.md, "Who owns what"):
  - The bench, the dev layer, the look files and their player are the animation session's.
  - The rules, the spell scripts and the served 2DA rows are the game session's.
  - The one-line call and the overlay are agreed in principle with the game session (above). Its exact lines go to it
    before they land.
  - The overlay `data/looks/spells.look.txt` is the animation session's. The row server that merges it is the game
    session's.
  - ConjTime and CastTime decide how long the caster is busy, so they stay the rule's. *(My call.)* The bench shows
    them as the span the cast must fit in, and warns when the animation runs past. A longer or shorter cast is a
    request to the game session.
- **Tests:** every test row stands on its own (owner, 2026-11-09). Its Setup names the version, any !test profile or
  !spawn, and the bench to open.

## The pieces, in order (agreed)

Each piece is useful on its own; each later one builds on the earlier.

1. **The bench itself.**
   - The scene: the caster and the enemies in formation, the camera.
   - The five parts and their timeline.
   - Preview. *(My call:)*
     - The missile is the game's own model, drawn by the VFX Bench's model reader when it opens it. Otherwise it's a
       coloured stand-in on the right path.
     - The leap is a straight beam stand-in.
     - Piece 2 makes both exact and editable.
   - Sounds, plain. *(My call: picking is cheap and the owner asked for a sound per part.)*
     - Each part's sound is picked from the game's library by name and previewed.
     - The bench's server decodes the game's IMA ADPCM to plain WAV for the browser; the decoder already exists in the
       #154 work.
     - Layering and effects stay in piece 4.
   - Save, writing the look and the overlay (above).
   - Play in game.
   - The first spell: Chromatic Orb, using the game's orbs and hit effects.
2. **Projectiles and beams.**
   - Open and preview the game's projectile models: the flight from the spawn point to the target, homing or arcing.
   - Make or change one (as the VFX Bench does effects).
   - Later: beams between two creatures.
3. **A test module of our own** (owner: "a simple map with well lit open areas, some obstacles but also at least a
   section in deep shadow to test stealth and lighting effects", after "Look online to see if there are any testing
   maps available"). Nothing online fits (#154's notes: Sunjammer's VFX Browser, an EE VFX Tester whose page is gone,
   a spawner add-on, an old target-golem module). Ours:
   - a sunlit field;
   - a castle hall with pillars and crates for obstacles;
   - an unlit mine or dungeon wing for deep shadow (nwn5e's light-based stealth);
   - marked spots for the formations, and a way to spawn enemies there and reset;
   - built from base tilesets only, no haks. Possibly generated with nwn5e's own GFF writer (`tools/gff`) rather than
     by hand in the toolset.
4. **Sounds.**
   - Pick, preview and time the game's own sounds per part.
   - Layer several.
   - Apply effects: pitch, filters, echo, reverb. A sound is defined as "a game sound + effects" under its own name,
     and the DLL plays the original with those effects live, so no new audio files. This needs #154's proof to pass.
   - Fallback: bake a modified copy (a new file) when live effects can't do it.
   - Per-sound volume, each sound its own (owner, 2026-11-10: "would it also be possible to add independent volume
     settings per audio file if they are played at the same time?"): the same player, a volume per sound.
5. **Weapon attacks** (the "attack bench"), later: melee and ranged weapon swings with on-hit effects and sounds, in
   the same scene and timeline.

## Decided along the way

- **Live sound effects through the DLL,** a proof first (#154); owner: "Yes, go for it".
- **Our own test module** over a downloaded one (same answer).
- **The game plays the timeline as dragged, and Play in game is a staged replay** (owner, 2026-11-10: "Go with your
  recommendations"; above).
- **Chromatic Orb's first version uses the game's own orbs and hit effects;** custom orbs come with piece 2.
- **Not the old Recipe Bench's "Mix legs and torso".** For the spell's animation, formula recipes' `legs` (#149) and
  the cast swap (#150) are what exist; the new Mocap Bench's sources stay on hold.

## Left to the builder

- The page's layout beyond the mockup (docs: the owner's mockup, 2026-11-10).
- How the bench draws the scene. Reuse what exists:
  - the VFX Bench's WebGL stage and particle simulation (`vfx_bench/public/view.js`, `sim.js`);
  - the Recipe Bench's skeleton playback (`recipe_bench/public/core.js`).
- The bench's own project files (one per spell, variants inside), and how Save maps them onto the game's data.
- How a projectile's flight is previewed (homing / ballistic paths from the game's code where it matters).
- The test module's exact layout and the spawn / reset commands (with the game session where they touch its commands).
- Sound preview through the browser's audio engine (Web Audio: playback rate, biquad filters, delay, convolution).
  Close to the game's OpenAL EFX, not identical.
