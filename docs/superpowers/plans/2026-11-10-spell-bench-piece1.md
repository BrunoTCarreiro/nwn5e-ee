# The Spell & Attack Bench, piece 1 - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Chromatic Orb end to end. It's picked and timed in a new local bench, previewed there, and replayed in the running game against
spawned enemies. Save writes a "spell look" that the game plays exactly as timed.

**Architecture:**
- A look is a text file (`*.look`) per spell, parsed and turned into script text by pure code in `src/data/`, unit-tested.
- A player in `src/core/` loads the looks and swaps the cast clip from its own spell-cast listener. At the impact it plays
  the missile, hit, leap and sounds through script chunks with `DelayCommand`, and returns when the hit lands. The game
  rule calls it in one line.
- The bench (`nwn5e-tools/spell_bench/`, Node, no dependencies) shares the format through a JS twin. Both writers
  reproduce one fixture byte for byte.
- A dev-layer link replays the bench's unsaved look in game: it spawns the formation and fakes the cast.

**Tech Stack:** C++ (MSVC, `build.bat` / `test.bat`, `tests/check.h`), Node (plain `*_check.js` scripts), canvas 2D, NWScript chunks
through `RunScriptChunk`.

**Spec:** `docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md` (approved 2026-11-10), mockup beside it. The game session's
agreements of 2026-11-10 are recorded in the spec and in "The game session's part" below.

## Global Constraints

- **Game repo:** work only in `C:\Users\Bruno\Desktop\Projects\nwn5e-ee-anim`, branch `anim`.
  - Never edit `dllmain.cpp`, any `src/core/rules_*`, `src/core/data_serving.*` or `data/rows/` (the game session's).
  - Never edit the shared files (`build.bat`, `test.bat`, `src/core/config.*`, `nwn5e.ini`, `README.md`, `CLAUDE.md`).
- **Tools repo:** `C:\Users\Bruno\Desktop\Projects\nwn5e-tools`, branch `main`. `mocap_bench/` is on hold: read it, never edit it.
- **Names** are unique across `src/core`, `src/data` and `src/rules` (one obj folder). New: `anim_look.*` and `anim_bench_scene.*`
  (data), `anim_look_player.*` (core).
- **Git:**
  - A new file gets `git add <path>` first. Commit with `git commit --only <paths> -F -` (message on stdin), ending with
    `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
  - One git command per Bash call. Never `git add -A`. Never push.
- **Game tests:** from the game repo, in cmd: `set NWN5E_PRIVATE=C:\Users\Bruno\Desktop\Projects\nwn5e-ee\private&& test.bat`.
- **Build check:** `set NWN5E_PRIVATE=C:\Users\Bruno\Desktop\Projects\nwn5e-ee\private&& set NWN5E_OUT=C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\build_out&& build.bat`.
  Never build into `build\` (the game may be running).
- **Bench checks:** from the tools repo, `node spell_bench/checks/run.js` (a copy of `mocap_bench/checks/run.js`).
- **Bench conventions** (as in `vfx_bench`):
  - Listens on 127.0.0.1 only, port **8768**.
  - Host / Origin checks; files are written whole through `.tmp` and rename.
  - Front-end files are IIFEs that set a global and `module.exports`.
  - The build folder: `--build` / `NWN5E_BUILD` / `..\..\nwn5e-ee\build`. The game folder: `anim_helper/helper.js` `findGameDir`.
- **Look text:** LF line ends.
  - Resrefs match `^[a-z0-9_]{1,16}$`.
  - Numbers are written with at most 3 decimals and trailing zeros trimmed (`1.5`, `1`, `0.05`).
  - Times are 0-10 s.
- **The table columns a look may set** (the game session's list): ConjAnim, ConjHeadVisual, ConjHandVisual, ConjGrndVisual,
  ConjSoundVFX, ConjSoundMale, ConjSoundFemale, CastAnim, CastHeadVisual, CastHandVisual, CastGrndVisual, CastSound, Proj,
  ProjModel, ProjType, ProjSpwnPoint, ProjSound, ProjOrientation.
- **Folders, read in this order (later wins):**
  - `<dll>\data\looks\`: shipped from the repo's `data/looks/`.
  - `<dll>\spellbench\looks\`: the bench's Save.
  - Never write the bench's Save under `build\data\` - every install's xcopy overwrites it.
- **New effect rows:** visualeffects **2100-2199**, progfx **910-999**, only into empty rows (the game session's guard).
- **Logs:** start with `look: `. Object ids print as `0x%08X`.
- **Test rows** (`scripts/tests_board.py`) each stand alone (owner, 2026-11-09). Setup names the version, every `!` command,
  the bench and what to pick.

## Review Focus

1. **A look value that would break the script text** (a quote, a backslash, a resref over 16 characters) must be refused
   when the look is read, with the line. It must never reach `RunScriptChunk`. Tests: Task 1 `AnimLookRefuses`, Task 7
   `readLook refuses`.
2. **The rule runs after the release mark** (a slow cast, a server hitch). The missile leaves at once, the hit stays one
   flight later, and the log says "late". It must never be a negative delay. Tests: Task 2 `AnimLookLate`, Task 7
   `hitTiming late`.
3. **A creature closer than 1 m.** The flight curve's divisor crosses zero near 0.51 m, and the time must stay positive
   and continuous. Tests: Task 2 `AnimLookFlightCurve`, Task 7 `mirvFlightSeconds`.
4. **An install between a bench Save and shipping** must not lose the Save. The bench writes only under
   `build\spellbench\`, and shipping deletes the draft. Test: Task 8 `server_check` "never writes build\data".
5. **The staged enemies must not fight back or die, and a second Play or Stop must not stack them.** Tests: Task 3
   `AnimBenchSpawnScript` (commoner faction, plot, the tag) and `AnimBenchClearScript`.

## Calls this plan makes (beyond the spec)

- **Area targets** wait for the first area spell. Chromatic Orb is single target plus leaps. The **beam, touch and melee**
  attack kinds come with piece 2 (beams) and piece 5 (weapons). Piece 1 has a missile, or "on the target" (no missile).
- **The cast VFX's place** (hand, chest, head) is the effect row's own node. The VFX Bench decides it, and the card shows it.
- **The preview draws stand-ins:** coloured glows and an orb. It doesn't run the effects' particles; Play in game shows
  the real ones.
- **The staged replay's table-driven parts** (the conjure animation, the conjure and cast sounds) come from the fake cast
  and the tables as last saved and restarted. Everything else plays the unsaved settings.
- **One switch for both bench links:** nwn5e.ini `[rules] vfx_bench`, so no new ini key in a shared file.
- **When the rule runs:** the bench assumes ConjTime (`rule_at`). The game measures it from the cast's start and logs it,
  and the player times from the measurement.

---

## The game session's part (theirs, not tasks here)

Agreed 2026-11-10. It lands in the same install as the hook, after Chromatic Orb's L1-19 / L1-20 pass.

- **The row server** reads `<dll>\data\looks\` and then `<dll>\spellbench\looks\`, the later folder winning row by row.
  It logs every file it merges.
  - `spells.look.txt`: presentation columns only.
  - `visualeffects.look.txt` / `progfx.look.txt`: rows 2100-2199 / 910-999, every column, empty rows only.
  - Everything else is refused and logged.
- **The hook in `ChromaticOrb`:**
  - `PlaySpellLook(self_or_caster_id, target_id, row)` returns the seconds until the hit. Damage then goes through a
    delayed command on the target.
  - Each leap uses `PlaySpellLookLeap(from, to, row, after)`.
  - A return of 0 keeps its current visuals.
  - Task 12 sends the exact lines.

---

### Task 1: The look file - read, write, variants

**Files:**
- Create: `src/data/anim_look.h`, `src/data/anim_look.cpp`
- Create: `tests/data/anim_look_test.cpp`, `tests/data/fixtures/chromatic_orb.look`

**Interfaces:**
- Produces (namespace `data`):
```cpp
enum class LookAnchor { kCast, kRelease, kHit };                // a sound's time counts from the cast's start, the release, or each hit
struct LookSound { LookAnchor anchor = LookAnchor::kCast; float at = 0; std::string resref; };
struct LookParts {                                               // what a [row] section may change
  int cast_vfx = 0; float cast_vfx_at = 0, cast_vfx_for = 0;     // a visualeffects row on the caster (0 = none)
  int missile = 0, hit = 0, leap_beam = 0;                       // visualeffects rows (0 = none; missile 0 = "on the target")
  std::vector<LookSound> sounds;
  std::map<std::string, std::string> table;                     // spells.2da presentation columns
};
struct SpellLook {
  std::string name, clip, clip_base = "castout";
  std::vector<int> rows;
  float rule_at = 0, clip_seconds = 0, release = 0, leap_for = 1, leap_after = 0.3f;
  LookParts parts;                                               // the defaults
  std::map<int, LookParts> variants;                             // a [row] section: the defaults with its own keys over them
};
bool ReadSpellLook(const std::string& text, SpellLook* look, std::string* error);
std::string WriteSpellLook(const SpellLook& look);
const LookParts& LookPartsFor(const SpellLook& look, int row);
```

**Format:**
- The header line is `# nwn5e spell look - key = value`. `#` starts a comment, and lines are trimmed.
- `key = value` lines: `name`, `rows` (space-separated), `rule_at`, `clip`, `clip_base`, `clip_seconds`, `release`,
  `leap_for`, `leap_after`, then the parts keys: `cast_vfx`, `cast_vfx_at`, `cast_vfx_for`, `missile`, `hit`,
  `leap_beam`, `sound = <cast|release|hit> <at> <resref>` (repeatable), and `table.<Column> = <value>`.
- `[<row>]` starts a variant that copies the defaults. Every later line belongs to it, and only parts keys are allowed
  there. A variant's first `sound` line clears its copied sounds.
- The writer's canonical order is the order above.
  - `cast_vfx*` is written only when `cast_vfx != 0`; `table.*` keys are sorted.
  - Each section is preceded by one blank line and holds only its differing keys. A section's sounds are written in
    full when they differ.

- [ ] **Step 1: Write the fixture** `tests/data/fixtures/chromatic_orb.look`, exactly:
```
# nwn5e spell look - key = value
name = chromatic_orb
rows = 1153 1154 1155 1156 1157 1158 1159
rule_at = 1.5
clip = n5_cantrip
clip_base = castout
clip_seconds = 2.5
release = 1.6
leap_for = 1
leap_after = 0.3
missile = 2100
hit = 61
leap_beam = 210
sound = cast 0.2 vs_chant_evoc_lm
sound = hit 0.05 sim_explflame
table.ConjAnim = hand

[1155]
missile = 2101
hit = 63
leap_beam = 211
sound = hit 0.05 sim_explfrost
```

- [ ] **Step 2: Write the failing tests** in `tests/data/anim_look_test.cpp`. A file-local
  `std::string Fixture(const char* name)` reads `tests/data/fixtures/<name>` with `std::ifstream` in binary, as
  `anim_assemble_test.cpp:263` does. Tasks 2's tests use it too.
```cpp
TEST(AnimLookReadsTheFixture) {
  data::SpellLook look; std::string error;
  CHECK_EQ(data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error), true);
  CHECK_EQ(look.name == "chromatic_orb", true);
  CHECK_EQ(look.rows.size(), 7);
  CHECK_EQ(data::LookPartsFor(look, 1154).missile, 2100);            // no section: the defaults
  CHECK_EQ(data::LookPartsFor(look, 1155).missile, 2101);
  CHECK_EQ(data::LookPartsFor(look, 1155).hit, 63);
  CHECK_EQ(data::LookPartsFor(look, 1155).sounds.size(), 1);         // a section's sounds replace the defaults'
  CHECK_EQ(data::LookPartsFor(look, 1155).table.at("ConjAnim") == "hand", true);  // copied from the defaults
}
TEST(AnimLookWritesTheFixtureBack) {                                  // the canonical form; the JS twin must match it too
  data::SpellLook look; std::string error;
  const std::string text = Fixture("chromatic_orb.look");
  CHECK_EQ(data::ReadSpellLook(text, &look, &error), true);
  CHECK_EQ(data::WriteSpellLook(look) == text, true);
}
TEST(AnimLookRefuses) {
  const std::string head = "name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.6\n";
  data::SpellLook look; std::string error;
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim\"x\n", &look, &error), false);            // a quote
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 abcdefghijklmnopq\n", &look, &error), false); // 17 characters
  CHECK_EQ(data::ReadSpellLook(head + "table.CastTime = 500\n", &look, &error), false);
  CHECK_EQ(error.find("CastTime") != std::string::npos, true);
  CHECK_EQ(data::ReadSpellLook("name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.2\n", &look, &error), false);  // before the rule
  CHECK_EQ(data::ReadSpellLook("name = x\nrule_at = 1\nrelease = 1\n", &look, &error), false);    // no rows
  CHECK_EQ(data::ReadSpellLook(head + "[1155]\nrelease = 2\n", &look, &error), false);             // can't vary
  CHECK_EQ(error.find("line 6") != std::string::npos, true);
}
```

- [ ] **Step 3: Run the tests.** Command: the game tests (Global Constraints).
  Expected: the build stops on `ReadSpellLook` not declared.

- [ ] **Step 4: Implement** `ReadSpellLook`, `WriteSpellLook` and `LookPartsFor` in `src/data/anim_look.cpp`.
  - Follow `ReadAnimRecipe`'s style: errors are `"line N: <why>"`, `*look` is written only on success, and unknown keys
    are an error, not ignored, so that typos surface.
  - `LookPartsFor` returns the variant when there is one, else the defaults.

- [ ] **Step 5: Run the tests.** Expected: no FAIL lines, exit 0.

- [ ] **Step 6: Commit** `src/data/anim_look.h src/data/anim_look.cpp tests/data/anim_look_test.cpp tests/data/fixtures/chromatic_orb.look`:
  `feat: spell looks - the .look file the Spell & Attack Bench saves and the game plays (piece 1)`.

---

### Task 2: Timing and the script text

**Files:**
- Modify: `src/data/anim_look.h`, `src/data/anim_look.cpp`
- Test: `tests/data/anim_look_test.cpp`

**Interfaces:**
- Consumes: Task 1's types.
- Produces (namespace `data`):
```cpp
float MirvFlightSeconds(float metres);          // the game's Magic Missile curve: d / max(1, 3 ln d + 2); 0 when d <= 0
struct LookTiming { float release_in = 0, hit_in = 0; bool late = false; };
LookTiming LookHitTiming(const SpellLook& look, const LookParts& parts, float since_cast, float metres);
// release_in = max(0, release - since_cast); late = since_cast > release; hit_in = release_in + flight (missile != 0) or release_in
std::string LookCastScript(const LookParts& parts);                       // run as the caster at the cast's start
std::string LookHitScript(const LookParts& parts, uint32_t target, const LookTiming& timing);  // run as the caster
std::string LookLeapScript(const LookParts& parts, float leap_for, uint32_t from, uint32_t to, float beam_in, float hit_in);  // run as `from`
std::string LookDistanceScript(uint32_t target);  // run as the caster: SetLocalString(OBJECT_SELF, "n5look_d", <metres, 3 decimals>)
```

**Script text:**
- Ids are written as `StringToObject("<lowercase hex>")`, times as `%.2f`.
- Every effect is created in the chunk and applied through `DelayCommand`:
  - The cast script holds the caster's VFX (`DURATION_TYPE_TEMPORARY`, `cast_vfx_for`) and the `cast`-anchored sounds.
  - The hit script holds:
    - the missile, `INSTANT` on `t` at `release_in`;
    - the hit, `INSTANT` on `t` at `hit_in`;
    - the `release`-anchored sounds on the caster;
    - the `hit`-anchored sounds through `AssignCommand(t, DelayCommand(...))`.
  - The leap script holds:
    - `EffectBeam(leap_beam, from, BODY_NODE_CHEST)`, `TEMPORARY` on `to` for `leap_for`, at `beam_in`;
    - the hit on `to` at `hit_in`;
    - the `hit`-anchored sounds on `to`.

- [ ] **Step 1: Write the failing tests.**
```cpp
TEST(AnimLookFlightCurve) {
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(10) - 1.1226f) < 0.001f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(5) - 0.7323f) < 0.001f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(0.5f) - 0.5f) < 0.001f, true);      // under the curve's knee: d / 1
  CHECK_EQ(data::MirvFlightSeconds(0) == 0.0f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(0.7165f) - 0.7165f) < 0.002f, true);  // continuous at the knee
}
TEST(AnimLookTiming) {
  data::SpellLook look; std::string error;
  data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error);
  const data::LookTiming t = data::LookHitTiming(look, look.parts, 1.5f, 10);
  CHECK_EQ(std::fabs(t.release_in - 0.1f) < 0.001f, true);
  CHECK_EQ(std::fabs(t.hit_in - 1.2226f) < 0.001f, true);
  CHECK_EQ(t.late, false);
}
TEST(AnimLookLate) {
  data::SpellLook look; std::string error;
  data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error);
  const data::LookTiming t = data::LookHitTiming(look, look.parts, 2.0f, 10);
  CHECK_EQ(t.release_in == 0.0f, true);
  CHECK_EQ(t.late, true);
  CHECK_EQ(std::fabs(t.hit_in - 1.1226f) < 0.001f, true);
}
TEST(AnimLookHitScript) {
  data::LookParts p; p.missile = 2100; p.hit = 61;
  p.sounds = {{data::LookAnchor::kRelease, 0, "sco_x"}, {data::LookAnchor::kHit, 0.05f, "sim_explflame"}};
  const std::string s = data::LookHitScript(p, 0x1a2b, {0.10f, 1.22f, false});
  CHECK_EQ(s.find("object t = StringToObject(\"1a2b\");") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(0.10, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(2100), t));") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(1.22, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(61), t));") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(0.10, PlaySound(\"sco_x\"));") != std::string::npos, true);
  CHECK_EQ(s.find("AssignCommand(t, DelayCommand(1.27, PlaySound(\"sim_explflame\")));") != std::string::npos, true);
  p.missile = 0;
  CHECK_EQ(data::LookHitScript(p, 0x1a2b, {0.10f, 0.10f, false}).find("EffectVisualEffect(2100)") == std::string::npos, true);
}
TEST(AnimLookLeapScript) {
  data::LookParts p; p.hit = 61; p.leap_beam = 210;
  const std::string s = data::LookLeapScript(p, 1.0f, 0x10, 0x11, 1.22f, 1.52f);
  CHECK_EQ(s.find("EffectBeam(210, StringToObject(\"10\"), BODY_NODE_CHEST)") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(1.22, ApplyEffectToObject(DURATION_TYPE_TEMPORARY,") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(1.52, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(61)") != std::string::npos, true);
}
```

- [ ] **Step 2: Run the tests.** Expected: the build stops on the new names.
- [ ] **Step 3: Implement** the six functions. Use `std::log` (natural log, as NWScript's `log` and the mod's Magic
  Missile at `rules_spells2024.cpp:322`).
- [ ] **Step 4: Run the tests.** Expected: no FAIL lines, exit 0.
- [ ] **Step 5: Commit** `src/data/anim_look.h src/data/anim_look.cpp tests/data/anim_look_test.cpp`:
  `feat: spell looks - the flight time and the script text that plays a look`.

---

### Task 3: The bench scene - formations, leaps, the request, spawning

**Files:**
- Create: `src/data/anim_bench_scene.h`, `src/data/anim_bench_scene.cpp`, `tests/data/anim_bench_scene_test.cpp`

**Interfaces:**
- Produces (namespace `data`):
```cpp
enum class Formation { kOne, kLine, kCluster, kRing, kAround };       // names: one line cluster ring around
struct Spot { float ahead = 0, right = 0; };                          // metres in the caster's frame
std::vector<Spot> FormationSpots(Formation kind, int count, float distance);  // count 1-8, distance 1-30 (clamped)
std::vector<int> LeapOrder(const std::vector<Spot>& spots, int target, int leaps);  // nearest unhit within 9.144 m (30 ft); ties: lower index
struct BenchRequest { bool play = false; int serial = 0, row = 0, count = 1, target = 0, leaps = 0;
                      Formation formation = Formation::kOne; float distance = 9; std::string enemy = "nw_goblina"; };
bool ReadBenchRequest(const std::string& first_line, BenchRequest* request, std::string* error);
std::string BenchSpawnScript(const std::vector<Spot>& spots, const std::string& enemy);  // run as the PC; sets "n5sb_spawned" = "id;id;"
std::string BenchClearScript();                                        // destroys every "n5sb_target"
std::string BenchCastScript(uint32_t target, int row);                 // run as the PC: face the target, ActionCastFakeSpellAtObject
```

**Spots:**
- **one:** `(d,0)`.
- **line:** `right = (i-(n-1)/2)*1.5`, `ahead = d`.
- **cluster:** `(d,0)` plus the first n of `(0,0) (0.9,0.8) (-0.8,0.9) (0.7,-0.9) (-0.9,-0.7) (1.6,0) (-1.6,0) (0,1.6)`.
- **ring:** radius 2 around `(d,0)`, angle `2*pi*i/n` from straight ahead.
- **around:** radius d around the caster, the same angles.

**Request line:** `request = play <serial> row=<r> formation=<name> count=<n> distance=<m> target=<i> enemy=<resref> leaps=<k>`,
or `request = stop <serial>`. Unknown keys and `target >= count` are errors.

- [ ] **Step 1: Write the failing tests.**
```cpp
TEST(AnimBenchFormations) {
  auto line = data::FormationSpots(data::Formation::kLine, 3, 9);
  CHECK_EQ(line.size(), 3); CHECK_EQ(line[0].right == -1.5f && line[0].ahead == 9.0f, true); CHECK_EQ(line[2].right == 1.5f, true);
  auto ring = data::FormationSpots(data::Formation::kRing, 4, 6);
  CHECK_EQ(std::fabs(ring[0].ahead - 8) < 1e-4f && std::fabs(ring[1].right - 2) < 1e-4f && std::fabs(ring[2].ahead - 4) < 1e-4f, true);
  auto around = data::FormationSpots(data::Formation::kAround, 4, 3);
  CHECK_EQ(std::fabs(around[1].right - 3) < 1e-4f && std::fabs(around[2].ahead + 3) < 1e-4f, true);
  CHECK_EQ(data::FormationSpots(data::Formation::kLine, 20, 50).size(), 8);  // clamped
}
TEST(AnimBenchLeapOrder) {
  auto line = data::FormationSpots(data::Formation::kLine, 3, 9);
  CHECK_EQ((data::LeapOrder(line, 1, 2) == std::vector<int>{1, 0, 2}), true);
  CHECK_EQ((data::LeapOrder(line, 1, 5) == std::vector<int>{1, 0, 2}), true);   // runs out of creatures
  CHECK_EQ((data::LeapOrder(line, 0, 0) == std::vector<int>{0}), true);
}
TEST(AnimBenchRequest) {
  data::BenchRequest r; std::string error;
  CHECK_EQ(data::ReadBenchRequest("request = play 7 row=1156 formation=line count=3 distance=9 target=1 enemy=nw_goblina leaps=1", &r, &error), true);
  CHECK_EQ(r.play && r.serial == 7 && r.row == 1156 && r.count == 3 && r.target == 1 && r.leaps == 1, true);
  CHECK_EQ(data::ReadBenchRequest("request = stop 8", &r, &error) && !r.play && r.serial == 8, true);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 count=2 target=2", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 enemy=nw\"x", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 colour=red", &r, &error), false);
}
TEST(AnimBenchSpawnScript) {
  const std::string s = data::BenchSpawnScript(data::FormationSpots(data::Formation::kLine, 3, 9), "nw_goblina");
  CHECK_EQ(s.find("AngleToVector(f) * 9.00") != std::string::npos, true);
  CHECK_EQ(s.find("AngleToVector(f - 90.0) * -1.50") != std::string::npos, true);
  CHECK_EQ(s.find("CreateObject(OBJECT_TYPE_CREATURE, \"nw_goblina\", l, FALSE, \"n5sb_target\")") != std::string::npos, true);
  CHECK_EQ(s.find("ChangeToStandardFaction(o, STANDARD_FACTION_COMMONER);") != std::string::npos, true);
  CHECK_EQ(s.find("SetPlotFlag(o, TRUE);") != std::string::npos, true);
  CHECK_EQ(s.find("SetLocalString(OBJECT_SELF, \"n5sb_spawned\", ids);") != std::string::npos, true);
}
TEST(AnimBenchClearScript) {
  const std::string s = data::BenchClearScript();
  CHECK_EQ(s.find("GetObjectByTag(\"n5sb_target\", i)") != std::string::npos, true);
  CHECK_EQ(s.find("DestroyObject(o);") != std::string::npos, true);
}
```

- [ ] **Step 2: Run the tests.** Expected: the build stops on the new names.
- [ ] **Step 3: Implement** the functions.
  - The spawn script builds each spot as
    `Location(GetArea(OBJECT_SELF), p + AngleToVector(f) * <ahead> + AngleToVector(f - 90.0) * <right>, f + 180.0)`.
  - `p`/`f` are `GetPosition`/`GetFacing` of `OBJECT_SELF`; numbers are `%.2f`.
  - The cast script: `ClearAllActions(); SetFacingPoint(GetPosition(t)); ActionCastFakeSpellAtObject(<row>, t);`.
- [ ] **Step 4: Run the tests.** Expected: no FAIL lines, exit 0.
- [ ] **Step 5: Commit** the three files:
  `feat: the Spell & Attack Bench's scene - formations, the leap order, its request and the spawn script (piece 1)`.

---

### Task 4: The look player in the game

**Files:**
- Create: `src/core/anim_look_player.h`, `src/core/anim_look_player.cpp`
- Modify: `src/core/anim_layers.cpp` (`InstallAnimLayers()`: add `InstallLookPlayer();` beside `InstallCastClip();`)

**Interfaces:**
- Consumes:
  - Tasks 1-2;
  - `RunScriptChunk` (`effects.h`), `engine::GetLocalString` (`engine.h`);
  - `CreatureById`, `Field` / `ee::kObjectId` (`creature.h`, `ee_layout.h`);
  - `GameMs` (`game_clock.h`);
  - `AddSpellCastListener` (`spell_cast_listeners.h`, the listener shape of `CantripCastBroadcast`);
  - `RequestCastClip` (`anim_cast_clip.h`); `Log` (`log.h`).
- Produces:
```cpp
// The game rule's one line, at the spell's impact: the look for `row`, from `caster` to `target`. Returns the seconds
// until the hit lands (the damage waits that long); 0, and nothing played, when no look has that row. Safe inside a
// spell's impact handler; never blocks.
float PlaySpellLook(uint32_t caster, uint32_t target, int row);
// A leap from `from` to `to` that starts `after` seconds from now (the previous hit). Returns the seconds until it lands,
// from now; 0 and nothing played without a look.
float PlaySpellLookLeap(uint32_t from, uint32_t to, int row, float after);
// The same with a look in hand - the bench's staged replay.
void StartLookCast(uint32_t caster, const data::SpellLook& look, int row);
float PlayLookHit(uint32_t caster, uint32_t target, const data::SpellLook& look, int row);
float PlayLookLeap(uint32_t from, uint32_t to, const data::SpellLook& look, int row, float after);
void InstallLookPlayer();
```

**Behaviour:**
- **Loading:** looks load at install from `DllDirectory()+L"\\data\\looks"`, then `L"\\spellbench\\looks"`, `*.look`.
  - The later folder wins by name; within a folder the newest file wins.
  - Copy `anim_packs.cpp`'s file-local helpers. Log `look: SKIP <file>: <error>` for a bad file and
    `look: <n> spell looks loaded (<names>)` at the end.
  - A map from row to look. A row in two looks: the later one wins, logged.
- **The listener:** a cast of a row with a look calls `StartLookCast`, unless the caster started one in the last 500 ms
  (the staged replay's fake cast).
- **`StartLookCast`:**
  - Records `GameMs()` for the caster.
  - Calls `RequestCastClip(creature, clip_base, clip, clip_seconds)` when `clip` isn't empty.
  - Runs `LookCastScript` as the caster, and logs `look: 0x%08X <name> row <r>: the cast begins (clip <clip>)`.
- **`PlayLookHit`:**
  - `since_cast` is the recorded start if it's younger than 15 s, else `rule_at`.
  - It gets the metres through `LookDistanceScript`, then the local string, then `atof`.
  - It runs `LookHitScript` as the caster and returns `hit_in`. The log line: `look: 0x%08X <name> row <r>: the rule ran
    <since> s after the cast began (the look expects <rule_at>); the missile leaves in <release_in> s and lands in
    <hit_in> s (<m> m)` plus ` - late` when late.
- **`PlayLookLeap`** runs `LookLeapScript(parts, leap_for, from, to, after, after + leap_after)` as `from`, returns
  `after + leap_after`, and logs `look: ... leaps to 0x%08X, landing in <s> s`.

- [ ] **Step 1: Write** the header and the player as above. No unit test reaches `src/core`: the logic it calls is
  Tasks 1-3's, already tested. This task's checks are the build and the in-game rows (Task 12).
- [ ] **Step 2: Build check** (Global Constraints). Expected: `dev layer: on`, no errors, and `nwn5e.dll` in the scratch
  folder.
- [ ] **Step 3: Run the game tests.** Expected: no FAIL lines (nothing in `src/data` regressed).
- [ ] **Step 4: Commit** `src/core/anim_look_player.h src/core/anim_look_player.cpp src/core/anim_layers.cpp`:
  `feat: the look player - plays a spell look in game, timed from the cast, and tells the rule when the hit lands (piece 1)`.

---

### Task 5: The orb rows

**Files:**
- Create: `data/looks/visualeffects.look.txt`, `data/looks/progfx.look.txt` (game repo)

These are the game's own `vpr_ecto*01` orbs, flown like its MIRVs. They change nothing until a look uses them.

- [ ] **Step 1: Write `data/looks/progfx.look.txt`** in the row format (`data/README.md`): `#` comment lines, then the header
  `Label Type Param1 Param2 Param3 Param4 Param5`, then:
  - `910 N5OrbFire 10 vpr_ectofire01 340 0 4 log`;
  - 911 `N5OrbCold` / `vpr_ectocold01`, 912 `N5OrbAcid` / `vpr_ectoacid01`, 913 `N5OrbElec` / `vpr_ectoelec01`,
    914 `N5OrbPoison` / `vpr_ectonatr01`, 915 `N5OrbThunder` / `vpr_ectosonc01`, each with the same `10 ... 340 0 4 log`.
  - The values are copied from progfx 905 MirvElectro.
- [ ] **Step 2: Write `data/looks/visualeffects.look.txt`:** the header `Label Type_FD OrientWithGround ProgFX_Impact
  SoundDuration OrientWithObject`, then `2100 NWN5E_ORB_FIRE P 0 910 **** 0` through `2105 NWN5E_ORB_THUNDER P 0 915 **** 0`.
  The order: FIRE, COLD, ACID, ELEC, POISON, THUNDER.
- [ ] **Step 3:** No commit yet. Task 6's check proves these rows against the game's own tables, and they're committed
  with it.

---

### Task 6: The bench's game data

**Files:**
- Create: `spell_bench/game_data.js`, `spell_bench/checks/run.js` (a copy of mocap's), `spell_bench/checks/game_data_check.js`

**Interfaces:**
- Produces (`module.exports`):
```js
resources(gameDir) -> (name, type) => Buffer|null  // nwn_base.key, nwn_retail.key over it, then loose ovr/<name>.<ext> (2da 2017, nss 2009, mdl 2002, wav 4)
resourceNames(gameDir, type) -> string[]
twoDa(text) -> { columns: string[], rows: Map<number, Object> }   // "2DA V2.0" text, quotes, ****
applyRows(table, text) -> number                                  // the repo's row format; returns rows changed
catalog({ gameDir, buildDir }) -> { spells, missiles, beams, hits, effects, sounds }
wavOf(buffer) -> Buffer                                           // a game WAV as PCM16 WAV: IMA ADPCM (format 17) decoded, PCM unchanged
```

**Details:**
- The KEY/BIF reader with overrides is `C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\recon\dump2.js`; copy its logic.
- The IMA decoder: the tables and loop of `C:\Users\Bruno\.claude\jobs\deffbb5f\tmp\sfx_preview.js`, mono and stereo by
  block.
- `catalog` sources: the base table, then `buildDir\data\rows\<table>.*.txt` (sorted), then `buildDir\data\looks\<table>.look.txt`,
  then `buildDir\spellbench\looks\<table>.look.txt`.
- The lists:
  - `missiles`: Type_FD P whose `ProgFX_Impact` progfx row has Type 10. Fields: `{row, label, model: Param1, curve: Param5, sound: SoundDuration}`.
  - `beams`: Type_FD B, `{row, label, model, sound}`.
  - `hits`: Type_FD F, `{row, label, model: first of Imp_Impact_Node / Imp_HeadCon_Node / Imp_Root_M_Node, sound: SoundImpact}`.
  - `effects`: Type_FD D or F.
  - `spells`: rows with `{row, label, conjTime, conjAnim, castTime, castAnim}`.
  - `sounds`: wav resrefs, sorted.

- [ ] **Step 1: Write the failing check** `spell_bench/checks/game_data_check.js`, in mocap's `check(what, ok, detail)` style:
  - `twoDa` of a 3-row fixture string with a quoted value and `****` returns the columns and values.
  - `applyRows`: a partial row changes only its named columns, `+` appends, and a row past the end fills `****`.
  - `wavOf` of a one-block synthetic IMA WAV (block align 36, predictor 0, index 0, data bytes `0x77`) returns a RIFF PCM16
    WAV with 65 samples; a PCM WAV comes back byte-equal.
  - The real game (print `skip: no game folder` and pass if `findGameDir` finds none) with `buildDir` = a temp folder
    whose `data\looks` holds Task 5's two files (copied from `--ee`, default `..\nwn5e-ee-anim`):
    - missile 181 flies `vpr_magmisl` and missile 233 flies `vpr_arofire`;
    - missiles 2100-2105 fly the six `vpr_ecto*01`, and each model resolves (`resources(...)(model, 2002) !== null`);
    - the base table's visualeffects 2100-2105 and progfx 910-915 are empty or past the end (the game session's guard);
    - hit 61 is `vim_explflame` with `sim_explflame`.
- [ ] **Step 2: Run** `node spell_bench/checks/run.js`. Expected: `FAIL game_data_check.js` (`game_data.js` missing).
- [ ] **Step 3: Implement** `spell_bench/game_data.js`.
- [ ] **Step 4: Run** `node spell_bench/checks/run.js`. Expected: `all 1 checks passed`. If the guard line fails, a row is
  taken: move the orbs to the next empty block in the range and update Tasks 5/1's numbers (a Ruling).
- [ ] **Step 5: Commit both repos:**
  - tools: `spell_bench/game_data.js spell_bench/checks/run.js spell_bench/checks/game_data_check.js`, message
    `feat: the Spell & Attack Bench's game data - tables with nwn5e's rows, effect lists, sounds as WAV (nwn5e-ee piece 1)`;
  - game repo: `data/looks/visualeffects.look.txt data/looks/progfx.look.txt`, message
    `feat: the orb rows - the game's vpr_ecto*01 orbs as missiles a look can fly (visualeffects 2100-2105, progfx 910-915)`.

---

### Task 7: The JS twin of the look

**Files:**
- Create: `spell_bench/public/look.js` (IIFE: global `SpellLook`, `module.exports`), `spell_bench/checks/look_check.js`

**Interfaces:**
- Consumes: Task 1's format, Task 2's timing, Task 3's spots and leap order. Same rules, same numbers.
- Produces:
```js
readLook(text) -> { look, error }          // look: { name, rows, rule_at, clip, clip_base, clip_seconds, release, leap_for, leap_after, parts, variants: {row: parts} }
writeLook(look) -> string
partsFor(look, row), mirvFlightSeconds(m), hitTiming(look, parts, sinceCast, m) -> { releaseIn, hitIn, late }
formationSpots(kind, count, distance) -> [{ ahead, right }], leapOrder(spots, target, leaps) -> [index]
timeline(look, row, spell, metres, leaps) -> { length, bars: [{ lane, label, start, end }], markers: [{ lane, kind, t, label, locked, sound }] }
snap(t, times, within) -> number
overlays(looks) -> { spells: string }      // spells.look.txt (row format) from the looks' table.* keys, per row
```

**Timeline rules:**
- Lanes: `Animation`, `Cast VFX`, `Attack`, `Hit VFX`, `Sounds`.
- Animation bars: the conjure from `0` to `rule_at`, labelled `conjure (<ConjAnim>)`; then the clip (or the CastAnim)
  from `rule_at` to `clip_seconds`.
- Markers:
  - `rule`: locked, at `rule_at`;
  - `release`: draggable;
  - `hit`: locked, at `release + flight`;
  - leap hits: locked, each one `leap_after` later;
  - each sound at its anchor's time plus `at`.

- [ ] **Step 1: Write the failing check** `look_check.js`:
  - `writeLook(readLook(fixture).look)` equals the fixture's text byte for byte. The fixture is
    `<ee>\tests\data\fixtures\chromatic_orb.look`, `--ee` default `..\nwn5e-ee-anim`.
  - `readLook refuses`: the same six texts as `AnimLookRefuses`, each with an error naming its line.
  - `mirvFlightSeconds`: 10 → 1.1226, 5 → 0.7323, 0.5 → 0.5, 0 → 0 (±0.001).
  - `hitTiming late`: since 2.0 → `releaseIn` 0, `late` true, `hitIn` 1.1226.
  - Formations and leap order: Task 3's values.
  - `timeline(fixture, 1156, {conjAnim:"hand"}, 10, 1)`:
    - release at 1.6, hit at 2.7226, the leap hit at 3.0226;
    - the `hit` sound at 2.7726 and again at 3.0726;
    - the rule locked at 1.5.
  - `snap(1.62, [1.5, 1.6, 1.7], 0.05)` is 1.6; `snap(1.66, [1.5, 1.6, 1.7], 0.03)` is 1.66.
  - `overlays([fixture]).spells` has the header `ConjAnim` and a row `1153 hand` through `1159 hand`.
- [ ] **Step 2: Run** `node spell_bench/checks/run.js`. Expected: `FAIL look_check.js`.
- [ ] **Step 3: Implement** `look.js`.
- [ ] **Step 4: Run.** Expected: `all 2 checks passed`.
- [ ] **Step 5: Commit** `spell_bench/public/look.js spell_bench/checks/look_check.js`:
  `feat: the Spell & Attack Bench's look in JS - the same file, timing and formations as the game's (piece 1)`.

---

### Task 8: The bench's server, Save and shipping

**Files:**
- Create: `spell_bench/server.js`, `spell_bench/spell_bench.bat`, `spell_bench/ship.js`, `spell_bench/README.md`,
  `spell_bench/checks/server_check.js`
- Modify: `README.md` (tools repo: the Tools table row)

**Interfaces:**
- Consumes: `game_data.js`, `look.js`, `anim_helper/helper.js` (`findGameDir`, `extractLibraries`).
- Produces these endpoints:
  - `GET /`, the public files, and `GET /lib/recipe_core.js` (serves `../recipe_bench/public/core.js`).
  - `GET /api/state` returns `{ looks, spells, link }`.
  - `GET /api/catalog`.
  - `GET /api/look?name=`: the bench's, else the shipped.
  - `POST /api/look?name=`: checked with `readLook` (400 and its error otherwise).
    - Writes `<build>\spellbench\looks\<name>.look`.
    - Regenerates `<build>\spellbench\looks\spells.look.txt` from every look in that folder.
  - `GET /api/sound/<resref>` (audio/wav through `wavOf`).
  - `GET /api/library/<name>` (from `anim_helper\cache\libraries`, extracted at start when missing, as the Recipe Bench).
  - `GET /api/recipe?name=`: `<build>\animedit\recipes` over `<build>\data\anims`, newest wins.
  - `POST /api/play?row&formation&count&distance&target&enemy&leaps`: writes
    `<build>\spellbench\bench\request.look`, i.e. the Task 3 request line, then the look.
  - `POST /api/stop`.
  - `GET /api/game`: `alive.txt` younger than 6 s, plus `status.txt`'s `<serial>\t<text>`.
- **`ship.js <name> [--build] [--ee]`** is the accept step:
  - copies the bench's `<name>.look` into `<ee>\data\looks\`;
  - regenerates `<ee>\data\looks\spells.look.txt` from the repo's looks;
  - deletes the bench's draft and regenerates the bench's `spells.look.txt` (the game session's request);
  - prints what moved.

- [ ] **Step 1: Write the failing check** `server_check.js`. It starts the server on port 18768 with `--build <temp>`
  and `--no-open`.
  - POST the fixture: `<temp>\spellbench\looks\chromatic_orb.look` is the fixture's text, and `spells.look.txt` holds
    `1156 hand`.
  - `never writes build\data`: `<temp>\data` doesn't exist after every call.
  - A bad look returns 400 with `line`.
  - `POST /api/play?row=1156&formation=line&count=3&distance=9&target=1&enemy=nw_goblina&leaps=1`: the request file's
    first line is `request = play 1 row=1156 formation=line count=3 distance=9 target=1 enemy=nw_goblina leaps=1`.
  - A request without the right Origin returns 403.
  - `ship.js chromatic_orb --build <temp> --ee <temp2>`: `<temp2>\data\looks\chromatic_orb.look` exists, and the draft
    is gone.
- [ ] **Step 2: Run.** Expected: `FAIL server_check.js`.
- [ ] **Step 3: Implement** the server (`vfx_bench/server.js`'s helpers copied), the `.bat` (as `vfx_bench.bat`),
  `ship.js` and the README (what it is, start, Save vs ship, the link files).
- [ ] **Step 4: Run.** Expected: `all 3 checks passed`.
- [ ] **Step 5: Commit** the six files: `feat: the Spell & Attack Bench's server - Save into spellbench\looks, the game link, ship.js (piece 1)`.

---

### Task 9: The page - scene and preview

**Files:**
- Create: `spell_bench/public/index.html`, `style.css`, `stage.js` (IIFE `SpellStage`), `app.js`
- Create: `spell_bench/checks/browser_check.mjs` (headless Edge through `mocap_bench/checks/cdp.mjs`, used as
  `mocap_bench/checks/browser_check.mjs` uses it)

**Interfaces:**
- Consumes: `/lib/recipe_core.js` (`AnimRecipes.readLibrary`, `readClip`, `deriveClip`, `worldPositions`, `readRecipe`,
  derived as `recipe_bench/public/app.js` does), `SpellLook`, and the server's API.
- Produces:
  - `SpellStage(canvas)` with `draw(scene)`.
  - `scene = { camera: "side"|"above"|"shoulder"|"follow", caster: { joints }, enemies: [{ x, y, joints, hit }], orb: { x, y, z, colour }|null, beams: [{ from, to, colour }], glows: [{ x, y, z, colour, size }] }`.
  - `window.spellBench = { ready, time, frame }` for the browser check.

**The layout** is the mockup's three columns (`docs/superpowers/specs/2026-11-10-spell-attack-bench-mockup.png`). Scene
settings on the left: formation, enemies, distance, target, the caster's body (`a_ba` default), camera.

**Preview:**
- The caster plays the `conjure1` loop to `rule_at`, then the clip.
- Enemies stand in their rest pose at `formationSpots`.
- The orb is a stand-in (Spec: piece 1 preview) on a homing arc from `rhand` to the target's chest, over
  `mirvFlightSeconds`.
- Glows mark the cast VFX and the hits, and the leaps draw a straight beam.
- Colours come from the row's label or model keyword (fire/flame orange, cold/frost pale blue, acid green,
  elec/lightning yellow, natr/poison olive, sonc/sonic/thunder lilac, else white).
- Restart, Pause and 0.25/0.5/1x speed.

- [ ] **Step 1: Write the failing browser check.** It starts the server, opens the page, and selects Chromatic Orb · Fire
  (the bench's draft from the server check's fixture, written into its temp build).
  - Expected: no console errors; `spellBench.ready`.
  - At `time = 2.0` the orb is between the caster and the target.
  - At `time = 3.0` the target's glow is present.
- [ ] **Step 2: Run** `node spell_bench/checks/browser_check.mjs`. Expected: FAIL (no page).
- [ ] **Step 3: Implement** the page, stage and preview.
- [ ] **Step 4: Run** the browser check and `node spell_bench/checks/run.js`. Expected: both pass. Look at the screenshot
  it saves against the mockup.
- [ ] **Step 5: Commit** the five files: `feat: the Spell & Attack Bench's page - the scene and the preview (piece 1)`.

---

### Task 10: The page - parts, timeline, sounds, Save and Play in game

**Files:**
- Create: `spell_bench/public/timeline.js` (IIFE `SpellTimeline`)
- Modify: `spell_bench/public/app.js`, `index.html`, `style.css`, `spell_bench/checks/browser_check.mjs`

**Interfaces:**
- Consumes: `SpellLook.timeline` / `snap`, `/api/catalog`, `/api/sound`, `/api/look`, `/api/play`, `/api/stop`, `/api/game`.
- Produces: `SpellTimeline(canvas, onChange)` with `draw(timeline, time)`.

**The five cards** (the mockup's right column), for the current variant (the spell's row picker):
1. **Animation:** the clip (the recipes plus the base game's animations), and the table's conjure animation `hand`/`head`.
2. **Cast VFX:** an `effects` row, with when and how long.
3. **Attack:** a `missiles` row, or "on the target" (0).
4. **Target:** single, plus leaps (0-3) for Chromatic Orb.
5. **Hit VFX:** a `hits` row and the leap's `beams` row.

**Sounds:** each card has a sound picker (the `sounds` list, filtered by typing) with a play button (`new Audio('/api/sound/<r>')`).

**Timeline:**
- Dragging the release, a sound or a Cast VFX bar edits the look.
- The release clamps to at least `rule_at`.
- Everything snaps (`snap`, 0.05 s) to the clip's key times; locked markers don't move.

**Buttons:**
- **Save** posts the look (the page shows the server's error line on a 400).
- **Play in game** posts it with the scene; **Stop** stops it.
- A link dot shows the game's alive state, and the status line shows the game's answer to the last serial.

- [ ] **Step 1: Extend the browser check.**
  - Drag the release marker by +0.2 s: the look text from `SpellLook.writeLook` has `release = 1.8`, and the hit marker
    follows.
  - Save: the server's file has `release = 1.8`.
  - Play in game: the request file exists and its first line starts with `request = play`.
- [ ] **Step 2: Run.** Expected: FAIL on the new steps.
- [ ] **Step 3: Implement** them.
- [ ] **Step 4: Run** the browser check and `node spell_bench/checks/run.js`. Expected: both pass.
- [ ] **Step 5: Commit:** `feat: the Spell & Attack Bench's parts, timeline, sounds, Save and Play in game (piece 1)`.

---

### Task 11: The staged replay in game (dev layer)

**Files:**
- Create: `dev/spell_bench_link.cpp` (tools repo)
- Modify: `dev/dev_layer.cpp` (declare and call `InstallSpellBenchLink()`, and add its line to the header comment)
- Modify: `dev/vfx_editor.cpp`: expose its `[rules] vfx_bench` reading as `bool BenchLinksOn()`, so one switch covers
  both links.

**Interfaces:**
- Consumes:
  - Task 3: `ReadBenchRequest`, `FormationSpots`, `LeapOrder`, `BenchSpawnScript`, `BenchClearScript`, `BenchCastScript`;
  - Task 1: `ReadSpellLook`;
  - Task 4: `StartLookCast`, `PlayLookHit`, `PlayLookLeap`;
  - `EndCastClip`; `RunScriptChunk`; `engine::GetLocalString`.
- Produces: `void InstallSpellBenchLink();`.

**Behaviour:**
- **The link:** a creature-update listener for the player character, at most every 250 ms (as `vfx_editor.cpp`'s
  `BenchUpdate`).
  - It watches `<dll>\spellbench\bench\request.look`'s write time, and ignores a request from before this start.
  - It writes `alive.txt` every 2 s and `status.txt` as `<serial>\t<line>`.
- **play:**
  - Clear the previous replay (`BenchClearScript`, `EndCastClip` on the PC).
  - Spawn the formation and read `n5sb_spawned`.
  - After 0.5 s, `BenchCastScript(target, row)`, then `StartLookCast(pc, look, row)`.
  - At `rule_at` from then, `PlayLookHit(pc, target, look, row)`, then `PlayLookLeap` along `LeapOrder`. Each leap's
    `after` is the previous call's return.
  - Status: `Playing <name> at <n> <enemy> (<formation>, <m> m)`.
- **stop:** clear, `EndCastClip`, then `Stopped`.
- **A bad look:** `Couldn't read the look: <error>`, with nothing spawned.

- [ ] **Step 1: Write** the link as above. Its logic is Task 3's, already tested; the checks are the build and SB-4.
- [ ] **Step 2: Build check** (Global Constraints), run from the anim worktree. build.bat takes the dev layer from the
  sibling `..\nwn5e-tools`. Expected: `dev layer: on`, no errors.
- [ ] **Step 3: Commit** `dev/spell_bench_link.cpp dev/dev_layer.cpp dev/vfx_editor.cpp` in the tools repo:
  `feat: the Spell & Attack Bench's Play in game - the unsaved look replayed on the character against a spawned formation (nwn5e-ee piece 1)`.

---

### Task 12: Hand-off - test rows, the game session, the plan notes

**Files:**
- Create: `<scratch>\tests_bench.csv`
- Modify: `docs/tests.csv` (by `tests_board.py export`), `docs/TOOLS-PLAN.md` (one line under its benches paragraph: the
  Spell & Attack Bench, port 8768, its spec)

- [ ] **Step 1: Run everything once more.**
  - Game tests: exit 0. Build check: OK. `node spell_bench/checks/run.js`: all pass. The browser check: passes.
- [ ] **Step 2: Write the test rows** (the `tests_154.csv` columns), each self-contained.
  - **SB-1, the preview:** Chromatic Orb · Fire, Line of 3, 9 m, the middle target, 1 leap. The orb leaves at the release
    mark and flashes on arrival, then the beam goes to a neighbour.
  - **SB-2, the sounds:** pick and play a sound on each card.
  - **SB-3, the timeline:** drag the release; it snaps and the hit follows.
  - **SB-4, Play in game:**
    - Setup: the install with nwn5e-tools `<commit>`, a dev build, and the owner's character in an open area with the
      bench open.
    - Expected: three goblins appear 9 m ahead and don't fight. The character conjures, then casts with the bench's clip.
      The orb leaves at the mark, the flash and sound play on arrival, and the beam goes to the next goblin. Stop
      removes them.
  - **SB-5, Save then a restart, then a real cast** (needs the game session's hook):
    - Setup: name the current `!spawn` command (look it up in `data/tests/` or the game session's rows).
    - Expected: the saved orb, and the damage numbers appear when the orb lands, not before.
- [ ] **Step 3: Board.** After merging main into anim: `python -I scripts/tests_board.py add <scratch>\tests_bench.csv`,
  then `python -I scripts/tests_board.py export`.
- [ ] **Step 4: Commit** `docs/tests.csv docs/TOOLS-PLAN.md`: `docs: the Spell & Attack Bench's test rows and its line in the tools plan`.
- [ ] **Step 5: Tell the game session:**
  - "anim ready at `<commit>`" and "tools ready at `<commit>`";
  - the test rows;
  - `src/core/anim_look_player.h`'s two declarations, with the hook lines for `ChromaticOrb`:
    `const float lands = PlaySpellLook(caster_id, target_id, row);` with damage through
    `AssignCommand(t, DelayCommand(lands, ...))` when `lands > 0`, and `PlaySpellLookLeap(from, to, row, after)` per leap;
  - the row-server work list ("The game session's part");
  - SB-5 waits for the install carrying both.
