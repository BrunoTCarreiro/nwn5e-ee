#pragma once

// The Spell & Attack Bench's scene (docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md): the formations the
// staged enemies stand in, the order a leap visits them, the request line the bench writes for Play in game
// (<dll>\spellbench\bench\request.look) and the scripts the dev layer's link (nwn5e-tools/dev/spell_bench_link.cpp)
// runs as the player character. The bench's JS (spell_bench/public/look.js) pins the same numbers.

#include <stdint.h>

#include <string>
#include <vector>

namespace data {

enum class Formation { kOne, kLine, kCluster, kRing, kAround };  // names: one line cluster ring around
bool FormationFromName(const std::string& name, Formation* kind);
const char* FormationName(Formation kind);

struct Spot {  // metres in the caster's frame
  float ahead = 0.0f, right = 0.0f;
};

// count 1-8 and distance 1-30 m (clamped). one: (d,0). line: 1.5 m apart across, centred, d ahead. cluster: a fixed
// huddle round (d,0). ring: radius 2 round (d,0), from straight ahead. around: radius d round the caster.
std::vector<Spot> FormationSpots(Formation kind, int count, float distance);
// The creatures a hit on `target` leaps through: the target, then up to `leaps` more, each the nearest not yet hit within
// 30 feet (9.144 m) of the last (ties: the lower index). Chromatic Orb's leap, forced for the replay.
std::vector<int> LeapOrder(const std::vector<Spot>& spots, int target, int leaps);

struct BenchRequest {
  bool play = false;
  int serial = 0, row = 0, count = 1, target = 0, leaps = 0;
  Formation formation = Formation::kOne;
  float distance = 9.0f;
  std::string enemy = "nw_goblina";
  bool miss = false;  // the attack misses (owner, 2026-11-10: "A"): the orb flies and fizzles, no leaps
};
// "request = play <serial> row=<r> formation=<name> count=<n> distance=<m> target=<i> enemy=<resref> leaps=<k>
// [miss=1]" or "request = stop <serial>". Unknown keys, out-of-range values and target >= count are errors.
bool ReadBenchRequest(const std::string& first_line, BenchRequest* request, std::string* error);

// Run as the player character: creates `enemy` at each spot (facing back), commoner faction and plot so they neither
// fight nor die, tagged "n5sb_target"; sets local string "n5sb_spawned" = "id;id;" in spot order.
std::string BenchSpawnScript(const std::vector<Spot>& spots, const std::string& enemy);
std::string BenchClearScript();  // destroys every "n5sb_target"
std::string BenchCastScript(uint32_t target, int row);  // run as the PC: face the target, ActionCastFakeSpellAtObject

}  // namespace data
