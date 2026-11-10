# Spell & Attack Bench, piece 2a: flight paths

Date: 2026-11-10. Part of #155's piece 2 ("Projectiles and beams") in
`docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md`. The owner: "yes, start piece 2 with flight paths".
Everything quoted is the owner's; everything marked *(my call)* is the designer's and open to the owner's veto.

## Intent and why

Today every look's missile flies one way: the game's MIRV homing curve (visualeffects `P` rows on progfx Type 10, Magic
Missile's `log` curve; the orbs are rows 2100-2105 / 910-915). The owner wants the missile's flight to be part of the
look, as the clip, the effects and the sounds already are. The owner asked earlier: "can the trajectory of the
projectile be configured as well?". The picture that started it:

> "a magic missile, it starts on a slow arc upwards and then speeds up very quickly and goes almost in a straight line
> into the target"

The owner **picks a kind of flight and tunes it** ("picking a kind of flight and tuning it"). They don't draw the path.

## Goals

1. **Flight kinds**, picked per look on the bench's Attack card, each with a few settings:
   - **Rise-and-dash** (Magic Missile): leaves the hand on a slow upward arc, then speeds up sharply and goes almost
     straight into the target. Settings *(my call; the owner approved the playback)*: how high the rise goes, how long
     the slow part lasts, how fast the dash is.
   - **Arc** (Fireball): a lob. Settings (the owner's): "how high it goes, how fast it falls and general speed". "How
     fast it falls" is read as the drop being quicker than the climb, so the arc can be lopsided *(my reading, approved)*.
   - **Straight bolt**: a straight line from the hand to the target. Settings *(my call)*: its speed, and an optional
     speed-up toward the end.
   - **Homing curve**: today's MIRV flight, **rebuilt as one of our own kinds** (owner, after the builder check: "go
     with your recommendations"). It's a homing-like curve made of segments like the others, so it matches the preview
     exactly. Existing looks move to it, Chromatic Orb's included, looking very close to today, and its darts can fan
     out and stagger too.
2. **Several missiles per cast** (Magic Missile's darts):
   - The darts leave together and "fan out a little, then each dashes at its target". You tune the fan's width.
   - The dashes start "almost the same moment, with a tiny stagger", and you tune the stagger.
   - Each dart can go to its own target, or several to the same one.
   - The spell's rule says how many darts and at whom (2024 Magic Missile: three darts, plus one per slot level above
     1st).
   - Each dart's hit lands when it arrives, and the rule waits for each one's damage *(my call, approved in the playback)*.
3. **Flights to a point on the ground** (Fireball; owner, after the builder check: "go with your recommendations"). Every
   entry point today aims at a creature; a fireball is aimed at a spot and bursts there. This piece adds:
   - a call that flies a look's missile to a location;
   - a hit that plays on the ground there (the look's hit effect and `hit` sounds at the location);
   - a "point" target in the bench's scene.

   No 2024 Fireball rule exists yet; the game session hooks it when it writes one.
4. **The timeline follows the flight.** The hit lands when the missile arrives, the timeline shows it, and the rule
   waits for it, as today.
5. **The bench shows the flight exactly** in its 3D preview: the same path, the same speeds, the same arrival time.
6. **Single player is the main concern, multiplayer should work too** (owner: "single player is the main concern, but
   maybe let's try to mae it compatible with multiplayer too?").

## Anti-goal

**The game's flight not matching the bench preview** (owner: "if the game flight doesn't match the bench preview"). Every
flight must be defined once and evaluated by the same math in the game and in the bench. A flight the game can't
reproduce must not be offered in the bench.

## Non-goals

- Drawing or dragging a path. Kinds and settings only.
- A target's reaction (a dodge on a miss, a flinch on a hit). The owner: "no, don't add them".
- Your own missile models (colours, sizes, trails), beams and the ray spells: later parts of piece 2.
- Chasing a target that moves during the flight. A flight aims where the target stood when it left *(my call: the route
  below can't follow, and spell flights are short)*.
- Hooking more spells' rules. Magic Missile's darts and Fireball's arc show in game only once their rules hand them to the
  look, which is the game session's work (as for Chromatic Orb, `rules_spells2024.cpp`). This piece delivers the API and
  the bench; the hooks are announced to the game session.

## How it flies in game (decided with the owner)

Recon, 2026-11-10:
- **The engine's own paths can't do it.** Scripts pick from default, homing, ballistic, high ballistic and accelerating
  (`PROJECTILE_PATH_TYPE_*` on `ActionCast(Fake)Spell*`). spells.2da adds linked, bounce and spiral. All are fixed
  shapes; none is the rise-and-dash.
- **NWN:EE's object visual transform** (`SetObjectVisualTransform`, 1.80.8193.14+) can do it. It slides an object's
  rendering along X, Y and Z, each with its own easing and duration, and the client interpolates it smoothly.
  - Its easings are published in `nwscript.nss`:
    - linear: `x`
    - smoothstep: `x*x*(3-2x)`
    - inverse smoothstep: `0.5 - sin(asin(1-2x)/3)`
    - ease in: `1-cos(x*pi/2)`
    - ease out: `sin(x*pi/2)`
    - quadratic: `x*x`
    - smootherstep: `x*x*x*(x*(6x-15)+10)`
  - It is server-driven, so it works in multiplayer.
- **The DLL's client smoothing** (`src/core/client_smoothing.cpp`) moves a creature on screen every frame, exactly.
  Limits: single player only, and ground-level x/y only today.

**Route** *(my call, told to the owner)*:
- An invisible carrier object, with the missile's model on it as a lasting effect, is flown by visual-transform
  segments. Each flight kind is a short list of segments; each segment is a duration and, per axis, a target offset and
  an easing.
- A **proof in game comes first**: does an effect on the carrier travel with its visual transform, smoothly, and does
  a segment chain play back to back without hitches?
- **The owner judges the proof by eye in a test row** (owner, after the builder check: "go with your recommendations"):
  - A small hitch where two segments join, which can't be polished away, is a pass: the route stays multiplayer-safe.
  - Anything that visibly breaks the flight is a fail: the effect doesn't travel with the carrier, it stutters, or it
    jumps. The fallback is then the DLL's per-frame mover (client smoothing widened to height and curves), single
    player only. That code is the game session's (`src/core/client_smoothing.cpp`), so the change is agreed with it first.

Because the bench uses the same segments and the same easing formulas, the preview matches the game by construction.

## The parts

- **The look format** (`src/data/anim_look`, and its JS twin `spell_bench/public/look.js`, byte for byte as today):
  - the flight kind and its settings, per look, overridable per `[row]` section like the other parts;
  - the dart fan and stagger;
  - the launch point *(my call: hand by default, as today; head and ground also offered, since spells.2da names them)*.
- **One flight definition, two evaluators.** A pure function turns a kind, its settings, the start and the target into
  the segment list (durations, offsets, easings). A twin evaluates any point of it at time `t`. Both live in C++
  (`src/data`, tested) and JS (`look.js`, checked), held to the same fixtures as the look format is.
- **The game side** (`src/core/anim_look_player`):
  - plays a flight by creating the carrier at the launch point and applying the missile's lasting effect;
  - sends the segments at their times, then removes the carrier on arrival;
  - lands the hit at the summed duration, which is exact, unlike today's MIRV estimate.
  - New served rows may be needed: a lasting-effect (`Type_FD D`) twin of each missile model, so it can sit on the
    carrier.
- **A darts call for the rules** (beside `PlaySpellLook`): it plays N darts at the given targets and returns each dart's
  arrival time. The game session wires Magic Missile to it.
- **A location call for the rules** (Goal 3): it flies the look's missile to a point and plays its hit there, returning
  the arrival time. The game session wires Fireball to it when it writes the rule.
- **The bench:**
  - the Attack card: the flight kind and its settings, the fan and the stagger;
  - the scene: a dart count, and where the darts go (all at the target, or one per creature), and a "point" target - a
    spot on the ground at the scene's distance (Goal 3);
  - the preview flies the segments;
  - the timeline shows each dart's flight and hit.

## Constraints

- The look format stays line-based and readable; the C++ reader and the JS twin stay identical (the fixture rule).
- A look never changes a rule. The rule asks the look when its hit lands.
- Existing looks keep working. Their `missile` rows fly the rebuilt homing-like curve (Goal 1), so nothing is left on
  the engine's MIRV path that the preview can't match. The rules' own visuals without a look are untouched.
- Process as for piece 1:
  - TDD;
  - the game session owns installs and the rules' hooks;
  - tools and anim commits are announced together;
  - every test row on the board stands on its own.

## Left to the builder

- The settings' names, ranges and defaults, and the exact segment shapes per kind, within the owner's descriptions.
- Which easings each kind uses (from the seven above).
- The carrier: which invisible object, how it's tagged and cleaned up, and how it faces along its path (*my call*: an
  arrow-like missile should point where it's going; an orb doesn't care).
- The darts call's exact signature, agreed with the game session.
- The Attack card's layout.
- The proof's form (a dev-layer command or a bench Play in game) and how its result is logged.

## Next: piece 2b, companions and trails (agreed, designed after 2a)

Owner, 2026-11-10, after this design: "can we have multiple particles? think one main projectile and then 1, 2 or 3
auxiliary particles that would accompany it. They could spiral around the main projectile for example" - "go with 2a
first, then 2b". What the owner agreed in the playback (to be designed in its own short document once 2a is in game):

- **Companions:** one to three around the main projectile, each its own element ("could be different elements").
- **How they move**, picked per companion with a few settings:
  - a **spiral** (radius, how fast it turns);
  - a **jitter / zig-zag** ("jitter and zig-zag") - how far, how jumpy.
- **Crackling lightning arcs** between the main projectile and its companions: an extra, switched on per look ("we can add
  both").
- **Trails** behind the main projectile and each companion, three styles, each tuned for length and colour / element:
  - "a streak of fading sparks";
  - "a line that gets thinner or fades";
  - "particles affected by gravity".
- *(my calls)*:
  - The companions and trails end with the main projectile's hit (or fizzle) and don't hit on their own.
  - They are parts of the missile's own model, built by the bench, so they ride any flight kind and the darts.
  - The preview runs the same particle rules as the game.
