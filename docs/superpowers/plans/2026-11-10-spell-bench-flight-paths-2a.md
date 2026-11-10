# Spell & Attack Bench piece 2a: flight paths - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A look's missile flies a picked, tuned flight in game exactly as the bench previews it:
- the kinds: rise-and-dash, arc, straight bolt, and a rebuilt homing curve;
- darts that fan out and stagger;
- flights to a point on the ground.

**Architecture:**
- A flight is a short list of segments. Each segment has a duration, an end offset from the launch point, and one of the
  game's seven easings for the ground plane and another for height.
- One C++ function (`src/data/anim_flight`) and its JS twin (`spell_bench/public/flight.js`) build and evaluate the
  segments, held to the same hand-computed numbers.
- In game, a whole NWNScript (run without the `main` wrapper) does the flying:
  - it creates an invisible carrier (`plc_invisobj`);
  - it shows the missile on it, through a lasting-effect twin row;
  - it plays each segment with `SetObjectVisualTransform`, timed by `DelayCommand`.
- The bench flies the same segments in its preview.

**Tech Stack:**
- C++17 DLL: nwn5e-ee-anim, branch `anim`; `test.bat`; `build.bat` with `NWN5E_PRIVATE`.
- NWScript run through `CVirtualMachine::RunScriptChunk`.
- Node bench, no dependencies: nwn5e-tools `spell_bench`, its `checks/*.js` and `browser_check.mjs`.
- The dev layer: nwn5e-tools `dev/`, compiled into the DLL.

**Spec:** `docs/superpowers/specs/2026-11-10-spell-bench-flight-paths-design.md` (piece 2a; piece 2b is out of scope).

## Global Constraints

- The game must match the bench. A flight is defined once, and the C++ function and its JS twin give the same numbers;
  no flight the game can't reproduce is offered.
- Single player is the main concern, and multiplayer must work too: the route is server scripts plus the client's
  visual-transform smoothing.
- The look format stays line-based. The C++ reader / writer and `look.js` stay byte-for-byte identical, and
  `tests/data/fixtures/chromatic_orb.look` is unchanged. A new key is written only when not at its default.
- A look never changes a rule. The rules' hooks (Magic Missile's darts, Fireball's point) are the game session's work;
  this plan delivers the calls.
- `src/core` outside `anim_*` is the game session's. Announce a change there and wait for its OK before committing.
- Process:
  - TDD;
  - commit with `git commit --only <paths> -F -` (a new file `git add`ed first), one git command per call, never
    `git add -A`, never push;
  - end each commit message with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`;
  - announce the anim and tools commits together;
  - every test-board row stands on its own.
- Commands:
  - game tests: `cmd //c 'C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\anim_test.bat'` (expect `N tests, 0 failed`);
  - DLL build: `cmd //c 'C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\anim_build.bat'` (expect `built ...`; it compiles
    nwn5e-tools `dev/` too);
  - bench: `node spell_bench/checks/run.js` and `node spell_bench/checks/browser_check.mjs` from `nwn5e-tools`.

## Review Focus

1. **A target at the launch point, or straight above it.** The horizontal distance is 0: `dir` is undefined and so is
   the fan rotation. Expect a valid flight (dir = +x, durations at least 0.05 s) and no NaN.
   *(Test: Task 1, `AnimFlightZeroDistance`.)*
2. **A missile row with no carrier twin** (a P row the bench lists but `FlightTwinRow` doesn't know). Expect the
   engine's homing missile as today, logged once, never no missile.
   *(Test: Task 5, `AnimLookHitScriptKeepsMirvWithoutTwin`.)*
3. **More darts than creatures** with "one per creature". Expect the darts to wrap around the formation, never to index
   past it. *(Test: Task 7, bench check "darts wrap the formation".)*
4. **A flight slower than the rule's patience.** A slow arc can land later than the old MIRV estimate, so the rule must
   use the returned arrival, not its own. Expect the damage when the missile lands.
   *(Test: Task 5, `AnimLookHitTimingUsesTheFlight`.)*
5. **An old saved look** with no flight keys. Expect it to read as homing with the default settings and write back
   byte-for-byte. *(Test: Task 4, fixture round trip plus the `AnimLookFlightDefaults` check.)*

---

### Task 1: The flight - segments and their evaluation (C++)

**Files:**
- Create: `src/data/anim_flight.h`, `src/data/anim_flight.cpp`
- Test: `tests/data/anim_flight_test.cpp`

**Interfaces:**
- Produces (namespace `data`):
  - `struct FlightPoint { float x = 0, y = 0, z = 0; };`
  - `enum class FlightKind { kHoming, kArc, kStraight, kRiseDash };`
  - `bool FlightKindFromName(const std::string&, FlightKind*)` and `const char* FlightKindName(FlightKind)`; the names are
    `homing`, `arc`, `straight`, `rise_dash`.
  - `struct FlightSettings`. Fields, defaults and ranges:

    | Field | Default | Range | Meaning |
    |---|---|---|---|
    | `kind` | `kHoming` | | |
    | `speed` | 12 | 1-60 m/s | arc and straight: along the ground; rise-and-dash: the dash |
    | `height` | 1.5 | 0-10 m | arc: apex over the line; rise-and-dash: the rise |
    | `rise` | 0.4 | 0.05-3 s | rise-and-dash: the slow part |
    | `fall` | 0.4 | 0.1-0.9 | arc: the share of the time spent falling |
    | `speedup` | false | | straight |
    | `fan` | 20 | 0-90 degrees | between neighbouring darts |
    | `stagger` | 0.04 | 0-0.5 s | between neighbouring darts' starts |
  - `struct FlightSegment { float seconds = 0; FlightPoint to; int lerp_xy = 1, lerp_z = 1; };`. `to` is the offset from
    the launch point at the segment's end; the lerps are `OBJECT_VISUAL_TRANSFORM_LERP_*` (0-7).
  - `float FlightLerp(int type, float x)` gives nwscript.nss's formulas for x in 0..1:
    - 0 none: 1 for any x > 0, else 0;
    - 1 linear: `x`;
    - 2 smoothstep: `x*x*(3-2x)`;
    - 3 inverse smoothstep: `0.5 - sin(asin(1-2x)/3)`;
    - 4 ease in: `1-cos(x*pi/2)`;
    - 5 ease out: `sin(x*pi/2)`;
    - 6 quadratic: `x*x`;
    - 7 smootherstep: `x*x*x*(x*(6x-15)+10)`.
  - `std::vector<FlightSegment> FlightSegments(const FlightSettings&, FlightPoint from, FlightPoint to, int dart, int darts)`
  - `float FlightSeconds(const std::vector<FlightSegment>&)` (the sum)
  - `FlightPoint FlightOffset(const std::vector<FlightSegment>&, float t)`. Within segment k it lerps from the previous
    `to` (zero for the first segment): x and y with `lerp_xy`, z with `lerp_z`. It clamps outside 0..total.
  - `float FlightStart(const FlightSettings&, int dart)` = `dart * stagger`.

**The shapes:**
- Definitions:
  - `D` = to - from;
  - `len` = |D|;
  - `d` = |D.xy|;
  - `dir` = D.xy / d, or (1, 0) when d < 0.001;
  - `up` = (0, 0, 1).
- `fanned(p)`: rotates p's xy part about the vertical by `(dart - (darts-1)/2) * fan` degrees. It applies only when
  `darts > 1`.
- **homing:**
  - T = `MirvFlightSeconds(len)` (from `anim_look.h`), at least 0.05.
  - seg1: T/2, to `fanned(D*0.5) + up*0.05*len`, lerps (1, 5).
  - seg2: T/2, to `D`, lerps (1, 4).
- **arc:**
  - T = max(0.05, d / speed); t_down = T * fall; t_up = T - t_down.
  - seg1: t_up, to `fanned(D*(t_up/T)) + up*height`, lerps (1, 5).
  - seg2: t_down, to `D`, lerps (1, 4).
- **straight:**
  - The final segment: max(0.05, len / speed), to `D`, lerps (6, 6) when `speedup`, else (1, 1).
  - With `darts > 1`, it is preceded by seg0: 0.15 s, to `fanned(dir*0.8) + up*0.3`, lerps (5, 5). The final
    segment's duration is then measured from seg0's end.
- **rise_dash:**
  - seg1: `rise`, to `fanned(dir*min(0.15*d, 1.0)) + up*height`, lerps (5, 5).
  - seg2: max(0.05, |D - seg1.to| / speed), to `D`, lerps (6, 6).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(AnimFlightLerps) {  // nwscript.nss's formulas
  CHECK_EQ(Near(data::FlightLerp(1, 0.5f), 0.5f) && Near(data::FlightLerp(2, 0.5f), 0.5f) && Near(data::FlightLerp(6, 0.5f), 0.25f), true);
  CHECK_EQ(Near(data::FlightLerp(4, 0.5f), 0.29289f) && Near(data::FlightLerp(5, 0.5f), 0.70711f) && Near(data::FlightLerp(7, 0.5f), 0.5f), true);
  CHECK_EQ(Near(data::FlightLerp(3, 0.25f), 0.32635f) && Near(data::FlightLerp(0, 0.01f), 1.0f) && Near(data::FlightLerp(0, 0.0f), 0.0f), true);
}
TEST(AnimFlightStraight) {
  data::FlightSettings s; s.kind = data::FlightKind::kStraight; s.speed = 12;
  auto seg = data::FlightSegments(s, {0, 0, 1}, {12, 0, 1}, 0, 1);
  CHECK_EQ(seg.size() == 1 && Near(seg[0].seconds, 1.0f) && Near(seg[0].to.x, 12) && seg[0].lerp_xy == 1, true);
  CHECK_EQ(Near(data::FlightOffset(seg, 0.5f).x, 6), true);
  s.speedup = true;
  CHECK_EQ(Near(data::FlightOffset(data::FlightSegments(s, {0, 0, 1}, {12, 0, 1}, 0, 1), 0.5f).x, 3), true);  // quadratic
}
TEST(AnimFlightArc) {
  data::FlightSettings s; s.kind = data::FlightKind::kArc; s.speed = 10; s.height = 2; s.fall = 0.4f;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(seg.size() == 2 && Near(seg[0].seconds, 0.6f) && Near(seg[0].to.x, 6) && Near(seg[0].to.z, 2) && seg[0].lerp_z == 5, true);
  CHECK_EQ(Near(seg[1].seconds, 0.4f) && Near(seg[1].to.x, 10) && Near(seg[1].to.z, 0) && seg[1].lerp_z == 4, true);
  const data::FlightPoint p = data::FlightOffset(seg, 0.3f);
  CHECK_EQ(Near(p.x, 3) && Near(p.z, 1.41421f), true);
}
TEST(AnimFlightRiseDash) {
  data::FlightSettings s; s.kind = data::FlightKind::kRiseDash; s.height = 1.5f; s.rise = 0.4f; s.speed = 20;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(Near(seg[0].seconds, 0.4f) && Near(seg[0].to.x, 1.0f) && Near(seg[0].to.z, 1.5f) && seg[0].lerp_xy == 5, true);
  CHECK_EQ(Near(seg[1].seconds, 0.45620f) && Near(seg[1].to.x, 10) && seg[1].lerp_xy == 6 && seg[1].lerp_z == 6, true);
  CHECK_EQ(Near(data::FlightSeconds(seg), 0.85620f), true);
}
TEST(AnimFlightHoming) {  // the rebuilt homing: today's MIRV time, a lift of 5% of the way
  data::FlightSettings s;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(Near(seg[0].seconds, 0.5613f) && Near(seg[0].to.x, 5) && Near(seg[0].to.z, 0.5f) && seg[1].lerp_z == 4, true);
  CHECK_EQ(Near(data::FlightSeconds(seg), data::MirvFlightSeconds(10)), true);
}
TEST(AnimFlightDartsFan) {
  data::FlightSettings s; s.kind = data::FlightKind::kRiseDash; s.fan = 20;
  auto a = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 3), b = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 1, 3),
       c = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 2, 3);
  CHECK_EQ(Near(a[0].to.x, 0.93969f) && Near(a[0].to.y, -0.34202f) && Near(b[0].to.y, 0) && Near(c[0].to.y, 0.34202f), true);
  CHECK_EQ(Near(a[1].to.x, 10) && Near(c[1].to.y, 0), true);  // each still lands on its target
  CHECK_EQ(Near(data::FlightStart(s, 2), 0.08f), true);
}
TEST(AnimFlightZeroDistance) {  // Review Focus 1: no NaN, a valid flight
  for (auto k : {data::FlightKind::kHoming, data::FlightKind::kArc, data::FlightKind::kStraight, data::FlightKind::kRiseDash}) {
    data::FlightSettings s; s.kind = k;
    auto seg = data::FlightSegments(s, {1, 1, 0}, {1, 1, 3}, 1, 3);
    const data::FlightPoint p = data::FlightOffset(seg, data::FlightSeconds(seg));
    CHECK_EQ(!seg.empty() && data::FlightSeconds(seg) >= 0.05f && Near(p.z, 3) && p.x == p.x, true);
  }
}
```
(`Near(a, b)` is `fabsf(a - b) < 0.001f`.)

- [ ] **Step 2: Run the tests.** Expected: the compile fails, because `data/anim_flight.h` doesn't exist.
- [ ] **Step 3: Implement** `anim_flight.h/.cpp` as specified above.
- [ ] **Step 4: Run the tests.** Expected: `... tests, 0 failed`, with the 7 new ones `ok`.
- [ ] **Step 5: Commit** `src/data/anim_flight.*`, `tests/data/anim_flight_test.cpp`:
  `feat: spell looks - flights as segments: rise-and-dash, arc, straight, homing, darts (#155)`.

### Task 2: The flight in game - the script, the carrier rows, the whole-script runner

**Files:**
- Modify: `src/data/anim_flight.h/.cpp`, `tests/data/anim_flight_test.cpp`
- Create: `data/looks/visualeffects.flight.look.txt`
- Modify (the game session's; announce first): `src/core/effects.h`, `src/core/effects.cpp`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  - `int FlightTwinRow(int missile_row)`. It maps:

    | Missile row | Twin row | Model |
    |---|---|---|
    | 2100-2105 | 2190-2195 | the orbs |
    | 181 | 2196 | `vpr_magmisl` |
    | 233 | 2197 | `vpr_arofire` |
    | 245 | 2198 | `vpr_aroacid` |
    | 503 | 2199 | `vpr_ectoelectric` |

    Any other row gives 0.
  - `std::string FlightScript(const std::vector<FlightSegment>& seg, FlightPoint from, int twin_row, float start)`. It
    returns a whole NWScript (its own `void main()`, run as the caster) that:
    1. defines `void Seg(object o, float x, float y, float z, int a, int b, float s)`, which calls
       `SetObjectVisualTransform` for TRANSLATE_X (lerp `a`), TRANSLATE_Y (lerp `a`) and TRANSLATE_Z (lerp `b`), each
       over `s` seconds;
    2. `object o = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", Location(GetArea(OBJECT_SELF), Vector(fx, fy, fz), 0.0), FALSE, "n5_flight");`
    3. `DelayCommand(start, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(twin), o, total + 0.1));`
    4. for segment k: `DelayCommand(start + t_k, Seg(o, to.x, to.y, to.z, lerp_xy, lerp_z, seconds));`;
    5. `DestroyObject(o, start + total + 0.2);`.

    Times and metres are printed `%.3f`.
  - `int RunWholeScript(const char* code, uint32_t self)` in `effects.h`. It is `RunScriptChunk` with
    `bWrapIntoMain = 0`.

- [ ] **Step 1: Announce.** Message `mechanics dev [7888d5]`:
  "anim wants a two-line addition to src/core/effects.h/.cpp: `int RunWholeScript(const char* code, uint32_t self)`, the
  same bridge call as RunScriptChunk with bWrapIntoMain 0, so a flight script can define the void helper that
  DelayCommand needs (SetObjectVisualTransform returns a float). OK?"
  Wait for the OK. If it's refused, ask the game session to add it itself and wait for the commit.
- [ ] **Step 2: Write the failing tests.**
  - `AnimFlightTwinRows`: 2100 → 2190, 2105 → 2195, 181 → 2196, 503 → 2199, 61 → 0.
  - `AnimFlightScript`, for `FlightScript(segs, {1, 2, 3}, 2190, 0.5f)`, where `segs` are rise_dash segments with
    `speed = 20` and the other settings at their defaults, from (0,0,0) to (10,0,0) (Task 1's `AnimFlightRiseDash`). The
    text contains:
    - `void Seg(object o, float x, float y, float z, int a, int b, float s)`;
    - `void main()`;
    - `CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", Location(GetArea(OBJECT_SELF), Vector(1.000, 2.000, 3.000), 0.0), FALSE, "n5_flight")`;
    - `DelayCommand(0.500, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(2190), o, 0.956))`;
    - `DelayCommand(0.500, Seg(o, 1.000, 0.000, 1.500, 5, 5, 0.400));`;
    - `DelayCommand(0.900, Seg(o, 10.000, 0.000, 0.000, 6, 6, 0.456));`;
    - `DestroyObject(o, 1.556);`.
- [ ] **Step 3: Run.** Expected: compile fails on `FlightTwinRow` / `FlightScript`.
- [ ] **Step 4: Implement** both, then `RunWholeScript` (after the OK).
- [ ] **Step 5: Add the twin rows.** Create `data/looks/visualeffects.flight.look.txt` with the header
  `Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject`. The
  rows 2190-2199 are labelled `NWN5E_FLY_ORB_FIRE` ... `NWN5E_FLY_ELECTRIC`, `D 0`, with the model in all four
  `Imp_Root_*` columns, then `0`. Start the file with a `#` comment saying what the rows are.
- [ ] **Step 6: Run the tests and the build.** Expected: `0 failed`, `built ...`.
- [ ] **Step 7: Commit** the data, tests, rows and effects files:
  `feat: spell looks - a flight in game: an invisible carrier flown by visual transforms (#155)`.

### Task 3: The proof in game - GATE

**Files:**
- Create: `nwn5e-tools/dev/flight_probe.cpp`
- Modify: `nwn5e-tools/dev/dev_layer.cpp` (declare `InstallFlightProbe()`, call it in `InstallDev()`)

**Interfaces:**
- Consumes: Task 2 (`FlightSegments`, `FlightScript`, `FlightTwinRow`, `RunWholeScript`).
- Produces: the chat command `!flight [homing|arc|straight|rise_dash] [darts 1-8]`.
  - It flies the fire orb's twin (2190) from the player's position + (0, 0, 1.3) to the point 10 m ahead along the
    player's facing (the same height), with the default settings, `darts` darts, each from `FlightStart`.
  - It answers in chat with the kind and each dart's seconds, and logs `flight: <kind> x<darts> - <seconds> s`.

- [ ] **Step 1:** Write `flight_probe.cpp` (`AddChatCommand("!flight", ...)`, as `sound_probe.cpp` does) and register it.
- [ ] **Step 2: Build.** Expected: `built ...` and `dev layer: on`.
- [ ] **Step 3:** Commit the tools change:
  `feat: !flight - the flight paths proof in game (nwn5e-ee #155, dev layer)`.
- [ ] **Step 4: Hand off.** Send the game session "anim ready at <commit> + tools ready at <commit>" (the install), and
  add the test row SB-21 on the board:
  - **Setup:** the install, a development build; your character in an open area.
  - **Do:** `!flight rise_dash`, `!flight arc`, `!flight straight`, `!flight homing`, `!flight rise_dash 3`.
  - **Expect:**
    - the orange orb rises slowly, then dashes; the arc lobs; the straight bolt flies level; the homing bows a little;
    - three darts fan out, then dash;
    - smooth, with the orb carried along, and no jump where two parts join.
- [ ] **Step 5: GATE.** The owner judges by eye.
  - **A pass** (or only a small seam where two parts join) continues with Task 4.
  - **A fail** (the orb doesn't travel, stutters or jumps) stops here. Report it with the log, and re-plan with the
    per-frame mover after agreeing it with the game session (`src/core/client_smoothing.cpp` is theirs).

### Task 4: The JS twin and the look format's flight keys

**Files:**
- Create: `nwn5e-tools/spell_bench/public/flight.js` (global `SpellFlight` / `module.exports`),
  `nwn5e-tools/spell_bench/checks/flight_check.js`
- Modify:
  - `src/data/anim_look.h/.cpp`, `tests/data/anim_look_test.cpp`;
  - `nwn5e-tools/spell_bench/public/look.js`, `checks/look_check.js`;
  - the server's `FILES` and `index.html`, which load `flight.js` before `look.js`.

**Interfaces:**
- Consumes: Task 1, Task 2's twin table.
- Produces:
  - **JS:** `SpellFlight` with the same names in camelCase and the same values:
    - `KINDS`, `DEFAULTS`;
    - `lerp(type, x)`, `segments(settings, from, to, dart, darts)`, `seconds(segs)`, `offset(segs, t)`, `start(settings, dart)`;
    - `twinRow(row)`.

    Points are `{x, y, z}`; segments are `{seconds, to, lerpXY, lerpZ}`.
  - **C++:** `LookParts` gains `FlightSettings flight;` and `std::string launch = "hand";` (hand, head or ground).
  - **The keys** are parts keys, so a `[row]` section may set them. They are written after `leap_beam`, in this order,
    only when not at the default:

    | Key | Value |
    |---|---|
    | `flight` | `homing`, `arc`, `straight` or `rise_dash` |
    | `flight_speed` | number |
    | `flight_height` | number |
    | `flight_rise` | number |
    | `flight_fall` | number |
    | `flight_speedup` | `1` |
    | `darts_fan` | number |
    | `darts_stagger` | number |
    | `launch` | `head` or `ground` |

    The ranges are Task 1's; outside them the reader refuses with the key's name.

- [ ] **Step 1: Write the failing checks.**
  - `flight_check.js` repeats Task 1's seven tests with the same numbers.
  - `look_check.js` gets the same as the new C++ test `AnimLookFlightKeys`:
    - `flight = rise_dash\nflight_speed = 20\nflight_height = 1.5\nflight_rise = 0.4\ndarts_fan = 30\nlaunch = head\n`
      reads, and writes back in that order. `flight_height = 1.5` and `flight_rise = 0.4` are the defaults, so they
      are not written back.
    - A `[1155]` section's `flight = arc` holds for that row only.
    - `flight_fall = 0.95` is refused, naming `flight_fall`.
  - `AnimLookFlightDefaults` (Review Focus 5): the fixture reads with `kind == kHoming`, default settings and
    `launch == "hand"`, and writes back byte-for-byte.
- [ ] **Step 2: Run.** Expected: the C++ compile fails on `parts.flight`; `flight_check.js` fails on the missing
  `flight.js`; the look checks FAIL.
- [ ] **Step 3: Implement** `flight.js` and the keys in both readers and writers.
- [ ] **Step 4: Run** the game tests and `node spell_bench/checks/run.js`. Expected: both green.
- [ ] **Step 5: Commit** the anim and tools sides separately:
  `feat: spell looks - a look's flight: kind, settings, darts, launch (#155)`.

### Task 5: The look player flies the look's flight

**Files:**
- Modify: `src/data/anim_look.h/.cpp`, `tests/data/anim_look_test.cpp`, `src/core/anim_look_player.cpp`

**Interfaces:**
- Consumes: Tasks 1, 2 and 4.
- Produces:
  - `FlightPoint LaunchPoint(FlightPoint caster, FlightPoint target, const std::string& launch)`. It returns the caster
    plus `dir * 0.3 + up * 1.3` for `hand`, `up * 1.8` for `head`, and `dir * 0.3 + up * 0.1` for `ground`. `dir` is
    the unit ground-plane direction to the target, or (1, 0) when the target is directly above.
  - `LookTiming LookFlightTiming(const SpellLook&, float since_cast, float flight_seconds)`: the release as
    `LookHitTiming` computes it, with `hit_in = release_in + flight_seconds`.
  - `LookHitScript` and `LookMissScript` no longer fire the MIRV `Instant` for the missile when
    `FlightTwinRow(parts.missile) != 0`, because the flight carries it.

- [ ] **Step 1: Write the failing tests.**
  - `AnimLookLaunchPoint`: from (0,0,0) to (10,0,0), `hand` → (0.3, 0, 1.3), `head` → (0, 0, 1.8),
    `ground` → (0.3, 0, 0.1).
  - `AnimLookHitTimingUsesTheFlight` (Review Focus 4): with rule_at 1.5, release 1.6 and since 1.5,
    `LookFlightTiming(.., 1.5, 2.4)` gives release_in 0.1 and hit_in 2.5.
  - `AnimLookHitScriptLeavesTheMissileToTheFlight`: missile 2100 → no `EffectVisualEffect(2100)` in the hit script.
  - `AnimLookHitScriptKeepsMirvWithoutTwin` (Review Focus 2): missile 9000 → the `Instant` is still there.
- [ ] **Step 2: Run.** Expected: FAIL / compile errors on the new names.
- [ ] **Step 3: Implement.** In `PlayLookArrival`:
  - read the caster and target positions (`ee::kObjectPositionX/Y/Z` on `CreatureById`);
  - `from = LaunchPoint(..)`, and `seg = FlightSegments(parts.flight, from, target + up*1.0, 0, 1)`;
  - when `FlightTwinRow(parts.missile)`: `timing = LookFlightTiming(look, since, FlightSeconds(seg))` and
    `RunWholeScript(FlightScript(seg, from, twin, timing.release_in), caster)` before the hit / miss script; else the
    current MIRV path and timing;
  - log `look: ... flies <kind> (<seconds> s)`.
- [ ] **Step 4: Run the tests and the build.** Expected: `0 failed`, `built`.
- [ ] **Step 5: Commit:** `feat: spell looks - the look's missile flies its flight (#155)`.

### Task 6: Darts and points - the rules' calls

**Files:**
- Modify:
  - `src/data/anim_flight.h/.cpp`, `tests/data/anim_flight_test.cpp`;
  - `src/data/anim_look_sound.h/.cpp` (a cue at a point);
  - `src/core/anim_sound_player.cpp`, `src/core/anim_look_player.h/.cpp`;
  - `src/data/anim_bench_scene.h/.cpp`, `tests/data/anim_bench_scene_test.cpp`.

**Interfaces:**
- Produces (rules API, `anim_look_player.h`):
  - `std::vector<float> PlaySpellLookDarts(uint32_t caster, const std::vector<uint32_t>& targets, int row)`. Each
    dart's arrival, in seconds from now. Dart i flies `FlightSegments(.., i, n)` from `release_in + FlightStart(i)`, and
    its hit / sounds land at its target at that arrival. Empty when there's no look.
  - `float PlaySpellLookAt(uint32_t caster, float x, float y, float z, int row)`. It flies to the point, and on arrival:
    - the hit effect at the location (`ApplyEffectAtLocation`);
    - the plain `hit` sounds from a short-lived `plc_invisobj` created there;
    - the effect sounds as cues at the point.

    Returns the arrival; 0 when there's no look.
  - The look-in-hand variants, for the bench's replay, beside `PlayLookHit`:
    - `std::vector<float> PlayLookDarts(uint32_t caster, const std::vector<uint32_t>& targets, const data::SpellLook&, int row)`;
    - `float PlayLookAt(uint32_t caster, data::FlightPoint at, const data::SpellLook&, int row)`.
  - data:
    - `std::string FlightLandingScript(FlightPoint at, int hit_row, const std::vector<std::string>& plain_sounds, float at_time)`:
      a whole-script body section, the helper `void Land(location l, int hit, string s)`, then the `DelayCommand(at_time, Land(..))`
      lines.
    - `SoundCue` gains `bool at_point = false; FlightPoint point;`.
  - BenchRequest gains `int darts = 1` (1-8), `std::string spread = "target"` (`target` or `each`) and `bool point = false`.
- [ ] **Step 1: Write the failing tests.**
  - `AnimFlightLanding`: the text has `void Land(location l, int hit, string s)` and
    `DelayCommand(1.250, Land(Location(GetArea(OBJECT_SELF), Vector(5.000, 0.000, 0.000), 0.0), 61, "sim_explflame"));`.
  - `AnimBenchRequest`, extended: `darts=3 spread=each point=1` reads; `darts=9` and `spread=all` are refused.
- [ ] **Step 2: Run.** Expected: compile errors.
- [ ] **Step 3: Implement.** In the sound player, a cue with `at_point` plays at `point`, not at an object's position.
- [ ] **Step 4: Run the tests and the build.** Then update the dev link (`spell_bench_link.cpp`):
  - for `darts > 1`, the targets are `spawned[order[0]]` for every dart (`target`), or `spawned[(order[0] + i) % count]`
    (`each`, wrapping);
  - for `point`, the target spot's ground point goes through `PlayLookAt` (the link's own look-in-hand variant, beside
    `PlayLookHit`).
- [ ] **Step 5: Commit** the anim and tools sides:
  `feat: spell looks - darts and flights to a point: the rules' calls (#155)`.

### Task 7: The bench - pick the flight, darts and points in the preview

**Files:**
- Modify: `nwn5e-tools/spell_bench/public/look.js` (timeline), `app.js`, `editor.js`, `index.html`, `style.css`,
  `server.js` (`requestLine`), `checks/look_check.js`, `checks/server_check.js`, `checks/browser_steps.mjs`.

**Interfaces:**
- Consumes: Task 4 (`SpellFlight`, the look keys), Task 6 (request keys).
- Produces:
  - `timeline(look, row, spell, metres, leaps, rows, female, miss, arrivals)`. `arrivals` gives each dart's arrival
    (seconds from the cast's start). When given, the hit markers are one per dart (`kind: "hit"`, label `dart k` when
    there's more than one). With darts > 1 there are no leaps.
  - The scene gains `darts`, `spread` and `point`.
  - The Attack card gains:
    - `card-flight` (kind);
    - `card-flight-speed`, `-height`, `-rise`, `-fall`, `-speedup`, each shown only for the kinds that use it;
    - `card-launch`, `card-darts-fan`, `card-darts-stagger`.

- [ ] **Step 1: Write the failing checks.**
  - `look_check`: a timeline given arrivals [2.0, 2.04, 2.08] has three hit markers at those times and no leap.
  - `server_check`: `/api/play?...&darts=3&spread=each&point=0` writes `... darts=3 spread=each`, and `darts=9` → 400.
  - **Browser steps:**
    - picking `rise_dash` on the Attack card: the frame's orb at release + 0.2 s is higher than at the release, and at
      arrival it's at the target's chest;
    - darts 3: three orb positions mid-flight, spread sideways;
    - darts 5 with "one per creature" and count 3: every dart's target index < 3 (Review Focus 3);
    - a point target: the hit effect sits at ground height at the target's spot;
    - Play in game with darts 3 → the request line has `darts=3`.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
  - `computeFrame` flies each dart from `SpellFlight.segments(parts.flight, launch, target, i, n)`, with the launch
    point as `LaunchPoint` computes it, mirrored in JS in `flight.js` as `launchPoint`.
  - The missile effect gets an event per dart (`missile-i`).
  - The hits are per dart.
  - A point target puts the hit on the ground.
  - The cards and the scene fields are as listed.
  - The Cast VFX list leaves out the `NWN5E_FLY_*` rows.
  - A missile without a twin (`twinRow` 0) shows the flight select disabled, with the note "the game's own homing only".
- [ ] **Step 4: Run** `node spell_bench/checks/run.js` and `browser_check.mjs`. Expected: all pass.
- [ ] **Step 5: Commit:**
  `feat: the Spell & Attack Bench - flight paths: kinds, settings, darts and points in the preview (nwn5e-ee #155)`.

### Task 8: Hand-off

- [ ] **Step 1:** Run the game tests, the build, `run.js` and `browser_check.mjs`. Expected: all green.
- [ ] **Step 2: Add the test rows** (each standing on its own; Setup names the install and the bench steps):
  - **SB-22** (bench only): each kind's preview, and its settings change the flight.
  - **SB-23** (game, Play in game): rise-and-dash with darts 3, "one per creature".
  - **SB-24** (game, a real Chromatic Orb cast): the orb flies the rebuilt homing and lands as the bench shows.
  - **SB-25** (game, Play in game): a point target lands on the ground.
- [ ] **Step 3: Message the game session** with "anim ready at <commit> + tools ready at <commit>", the install, and
  the hooks to add:
  - Magic Missile → `PlaySpellLookDarts(caster, targets, row)`, with the damage per dart at its arrival;
  - Fireball, when its rule is written → `PlaySpellLookAt`.
