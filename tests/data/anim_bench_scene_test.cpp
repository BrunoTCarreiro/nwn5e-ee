#include <cmath>
#include <string>
#include <vector>

#include "check.h"
#include "data/anim_bench_scene.h"

// The Spell & Attack Bench's scene (data/anim_bench_scene.h): where the staged enemies stand, the order a leap visits
// them, the bench's request line and the scripts that spawn and clear them. Breaks caught: a formation that puts
// creatures elsewhere than the bench draws them (its JS twin pins the same numbers); a leap that revisits a creature
// or jumps past 30 feet; a request that passes a bad target or resref through; spawned enemies that would fight back,
// die, or pile up over replays.

TEST(AnimBenchFormations) {
  auto line = data::FormationSpots(data::Formation::kLine, 3, 9);
  CHECK_EQ(line.size(), 3);
  CHECK_EQ(line[0].right == -1.5f && line[0].ahead == 9.0f, true);
  CHECK_EQ(line[2].right == 1.5f, true);
  auto ring = data::FormationSpots(data::Formation::kRing, 4, 6);
  CHECK_EQ(std::fabs(ring[0].ahead - 8) < 1e-4f && std::fabs(ring[1].right - 2) < 1e-4f &&
               std::fabs(ring[2].ahead - 4) < 1e-4f,
           true);
  auto around = data::FormationSpots(data::Formation::kAround, 4, 3);
  CHECK_EQ(std::fabs(around[1].right - 3) < 1e-4f && std::fabs(around[2].ahead + 3) < 1e-4f, true);
  CHECK_EQ(data::FormationSpots(data::Formation::kLine, 20, 50).size(), 8);  // clamped
  auto cluster = data::FormationSpots(data::Formation::kCluster, 2, 9);
  CHECK_EQ(std::fabs(cluster[1].ahead - 9.9f) < 1e-4f && std::fabs(cluster[1].right - 0.8f) < 1e-4f, true);
  CHECK_EQ(data::FormationSpots(data::Formation::kOne, 5, 9).size(), 1);
}

TEST(AnimBenchLeapOrder) {
  auto line = data::FormationSpots(data::Formation::kLine, 3, 9);
  CHECK_EQ((data::LeapOrder(line, 1, 2) == std::vector<int>{1, 0, 2}), true);
  CHECK_EQ((data::LeapOrder(line, 1, 5) == std::vector<int>{1, 0, 2}), true);  // runs out of creatures
  CHECK_EQ((data::LeapOrder(line, 0, 0) == std::vector<int>{0}), true);
  auto far = data::FormationSpots(data::Formation::kAround, 2, 10);  // 20 m apart: past the leap's 30 feet
  CHECK_EQ((data::LeapOrder(far, 0, 1) == std::vector<int>{0}), true);
}

TEST(AnimBenchRequest) {
  data::BenchRequest r;
  std::string error;
  CHECK_EQ(data::ReadBenchRequest(
               "request = play 7 row=1156 formation=line count=3 distance=9 target=1 enemy=nw_goblina leaps=1", &r,
               &error),
           true);
  CHECK_EQ(r.play && r.serial == 7 && r.row == 1156 && r.count == 3 && r.target == 1 && r.leaps == 1, true);
  CHECK_EQ(r.formation == data::Formation::kLine && r.distance == 9.0f && r.enemy == "nw_goblina", true);
  CHECK_EQ(data::ReadBenchRequest("request = stop 8", &r, &error) && !r.play && r.serial == 8, true);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 count=2 target=2", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 enemy=nw\"x", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 9 row=1156 colour=red", &r, &error), false);
  // A miss to preview (owner, 2026-11-10: "A"): miss=1; none, a hit.
  CHECK_EQ(data::ReadBenchRequest("request = play 10 row=1156 miss=1", &r, &error) && r.miss, true);
  CHECK_EQ(data::ReadBenchRequest("request = play 11 row=1156", &r, &error) && !r.miss, true);
  CHECK_EQ(data::ReadBenchRequest("request = play 12 row=1156 miss=2", &r, &error), false);
  // Darts and a point (piece 2a): darts 1-8, spread target (all at the target) or each (one per creature), point=1 the
  // target spot's ground. None: one dart at the target creature.
  CHECK_EQ(data::ReadBenchRequest("request = play 13 row=1156 count=3 darts=3 spread=each point=1", &r, &error), true);
  CHECK_EQ(r.darts == 3 && r.spread == "each" && r.point, true);
  CHECK_EQ(data::ReadBenchRequest("request = play 14 row=1156", &r, &error) && r.darts == 1 && r.spread == "target" &&
               !r.point,
           true);
  CHECK_EQ(data::ReadBenchRequest("request = play 15 row=1156 darts=9", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 16 row=1156 spread=all", &r, &error), false);
  CHECK_EQ(data::ReadBenchRequest("request = play 17 row=1156 point=2", &r, &error), false);
}

TEST(AnimBenchSpawnScript) {
  const std::string s = data::BenchSpawnScript(data::FormationSpots(data::Formation::kLine, 3, 9), "nw_goblina");
  CHECK_EQ(s.find("AngleToVector(f) * 9.00") != std::string::npos, true);
  CHECK_EQ(s.find("AngleToVector(f - 90.0) * -1.50") != std::string::npos, true);
  CHECK_EQ(s.find("CreateObject(OBJECT_TYPE_CREATURE, \"nw_goblina\", l, FALSE, \"n5sb_target\")") != std::string::npos,
           true);
  CHECK_EQ(s.find("ChangeToStandardFaction(o, STANDARD_FACTION_COMMONER);") != std::string::npos, true);
  CHECK_EQ(s.find("SetPlotFlag(o, TRUE);") != std::string::npos, true);
  CHECK_EQ(s.find("SetLocalString(OBJECT_SELF, \"n5sb_spawned\", ids);") != std::string::npos, true);
}

TEST(AnimBenchClearScript) {
  const std::string s = data::BenchClearScript();
  CHECK_EQ(s.find("GetObjectByTag(\"n5sb_target\", i)") != std::string::npos, true);
  CHECK_EQ(s.find("DestroyObject(o);") != std::string::npos, true);
}

TEST(AnimBenchCastScript) {
  const std::string s = data::BenchCastScript(0x2c, 1156);
  CHECK_EQ(s.find("object t = StringToObject(\"2c\");") != std::string::npos, true);
  CHECK_EQ(s.find("ActionCastFakeSpellAtObject(1156, t);") != std::string::npos, true);
}

TEST(AnimBenchLeapOrderTies) {  // equal neighbours by float sin/cos: the lower index, as the bench's JS (the review)
  CHECK_EQ((data::LeapOrder(data::FormationSpots(data::Formation::kRing, 6, 9), 0, 3) == std::vector<int>{0, 1, 2, 3}),
           true);
  CHECK_EQ((data::LeapOrder(data::FormationSpots(data::Formation::kAround, 5, 4), 0, 3) == std::vector<int>{0, 1, 2, 3}),
           true);
  CHECK_EQ((data::LeapOrder(data::FormationSpots(data::Formation::kAround, 6, 4), 0, 3) == std::vector<int>{0, 1, 2, 3}),
           true);
}

TEST(AnimBenchMarkScript) {  // the Test Grounds' cast mark (#156): Play in game stands on it; elsewhere, as before
  const std::string m = data::BenchMarkScript();
  for (const char* part : {"GetNearestObjectByTag(\"n5test_cast_field\")", "GetNearestObjectByTag(\"n5test_cast_hall\")",
                           "GetNearestObjectByTag(\"n5test_cast_dark\")", "SetLocalString(OBJECT_SELF, \"n5sb_mark\", \"\");",
                           "SetLocalString(OBJECT_SELF, \"n5sb_mark\", GetTag(w));", "JumpToLocation(GetLocation(w));"}) {
    CHECK_EQ(m.find(part) != std::string::npos, true);
  }
  // The formation is spawned round the mark when there is one (no mark: round the character, Review Focus 5).
  const std::string s = data::BenchSpawnScript(data::FormationSpots(data::Formation::kLine, 3, 9.0f), "nw_goblina");
  CHECK_EQ(s.find("object mark = GetNearestObjectByTag(GetLocalString(pc, \"n5sb_mark\"));") != std::string::npos, true);
  CHECK_EQ(s.find("if (GetLocalString(pc, \"n5sb_mark\") != \"\" && GetIsObjectValid(mark)) { p = GetPosition(mark); f = "
                  "GetFacing(mark); }") != std::string::npos, true);
}
