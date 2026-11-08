#include "check.h"
#include "data/twoda.h"
#include "data/effect_nodes.h"

using namespace data;

namespace {

// A slice of visualeffects.2da: a game effect (row 0) and nwn5e's rows naming nodes.
const char* kTable =
    "2DA V2.0\n"
    "\n"
    "Label Type_FD Imp_HeadCon_Node Imp_Impact_Node Imp_Root_M_Node Ces_Impact_Node\n"
    "0 GAME_GLOW D **** vdr_glow **** vce_glow\n"
    "1 NWN5E_HAND D **** n5v_flame **** n5v_flame_end\n"
    "2 NWN5E_HEAD D n5v_halo **** **** ****\n"
    "3 NWN5E_STOCK D **** vdr_glow **** ****\n"
    "4 NWN5E_BAD D **** n5v_bad **** ****\n"
    "5 NWN5E_AGAIN D **** N5V_Flame **** ****\n"
    "6 NWN5E_SAME D **** n5v_flame **** ****\n";
const char* kRows =
    "NWN5E_Node NWN5E_NodeUpright\n"
    "1 rhand 1\n"
    "2 Head ****\n"
    "3 lhand 1\n"
    "4 r-hand 1\n"
    "5 lhand 0\n"
    "6 rhand 1\n";

EffectNode Where(const EffectNodes& nodes, const char* model) {
  const auto found = nodes.models.find(model);
  return found == nodes.models.end() ? EffectNode{} : found->second;
}

}  // namespace

// #144. Breaks caught: the cessation model left out, the node not lower-cased, upright read wrong, a game model
// (one a row without a node uses) moved, a bad node name taken, a model's node changed by a later row, a model sent
// to the same node twice reported, a table without the column moving anything.
TEST(EffectNodesFromRows) {
  TwoDA t;
  std::string error;
  CHECK_EQ(ParseTwoDA(kTable, &t, &error), true);
  int changed = 0;
  CHECK_EQ(ApplyRows(&t, kRows, &error, &changed), true);
  const EffectNodes nodes = EffectNodesFrom(t);
  CHECK_EQ(nodes.models.size(), 3u);  // n5v_flame, n5v_flame_end, n5v_halo
  CHECK_EQ(Where(nodes, "n5v_flame").node == "rhand", true);
  CHECK_EQ(Where(nodes, "n5v_flame").upright, true);
  CHECK_EQ(Where(nodes, "n5v_flame_end").node == "rhand", true);  // the cessation model too
  CHECK_EQ(Where(nodes, "n5v_halo").node == "head", true);
  CHECK_EQ(Where(nodes, "n5v_halo").upright, false);
  CHECK_EQ(nodes.models.count("vdr_glow"), 0u);  // the game's row 0 uses it: it stays
  CHECK_EQ(nodes.models.count("n5v_bad"), 0u);   // not a node name
  CHECK_EQ(nodes.refused.size(), 3u);            // the game's model, the bad name, the flame sent elsewhere (row 6: same)
  TwoDA plain;
  CHECK_EQ(ParseTwoDA(kTable, &plain, &error), true);
  CHECK_EQ(EffectNodesFrom(plain).models.empty(), true);
}

// Breaks caught: a space or a dash taken, an empty name taken, the 31-character limit off by one.
TEST(EffectNodeNames) {
  CHECK_EQ(ValidNodeName("rhand"), true);
  CHECK_EQ(ValidNodeName("monster_9"), true);
  CHECK_EQ(ValidNodeName(""), false);
  CHECK_EQ(ValidNodeName("r hand"), false);
  CHECK_EQ(ValidNodeName("r-hand"), false);
  CHECK_EQ(ValidNodeName(std::string(31, 'a')), true);
  CHECK_EQ(ValidNodeName(std::string(32, 'a')), false);
}
