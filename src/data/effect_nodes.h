#pragma once

// #144 (owner, 2026-11-08: "I'd go for engine work as it allows for more flexibility with effects later by allowing
// nodes to follow body parts"): a visualeffects.2da row may name the node of the target's model its models go on
// (NWN5E_Node: rhand, lhand, head, impact, or any other node), and whether they keep upright there
// (NWN5E_NodeUpright 1: they follow the node's position only; otherwise its position and turning).
// core/vfx_nodes.cpp moves them as the client attaches them. No engine code; unit-tested.

#include <map>
#include <string>
#include <vector>

#include "data/twoda.h"

namespace data {

constexpr const char* kEffectNodeColumn = "NWN5E_Node";
constexpr const char* kEffectNodeUprightColumn = "NWN5E_NodeUpright";

struct EffectNode {
  std::string node;      // the node's name, lower case
  bool upright = false;  // follow the node's position only; otherwise its position and turning
};

struct EffectNodes {
  std::map<std::string, EffectNode> models;  // model name (lower case) -> where it goes
  std::vector<std::string> refused;       // why a row's node or model was left out, one line each
};

// The attach modes nwn5e chooses between (Gob::AttachToObject's last argument).
constexpr int kAttachTurning = 0;       // CAurBehaviorAttach: the node's position and turning
constexpr int kAttachPositionOnly = 3;  // CAurBehaviorAttachNoOrientation: its position only

// The mode for a model a row moves: upright, position only; otherwise turning with the node. A mode the game asks
// for other than those two (1 translate, 2 rotate, 4 GUI scale) stays.
int EffectAttachMode(const EffectNode& where, int game_mode);

// After a position-only attach (an effect whose row has no OrientWithObject) the client turns the effect's root part
// to the world's axes - right for an upright effect, wrong for one nwn5e turned to mode 0, which would then start
// on the world's axes and turn from there (the owner, 2026-11-08: "if I face south the flame still appears in
// front of the PC"). Whether that reset must be skipped for an attach the game asked as `game_mode` and nwn5e made
// `new_mode`.
bool SkipsWorldAlignment(int game_mode, int new_mode);

// Whether a Gob::SetPartPosition call is that reset: the part "root" (any case) turned to the identity (w 1, x y z
// 0 - the engine's quaternion order).
bool IsWorldAlignment(const char* part, const float turn[4]);

// A node name a row may give: 1-31 letters, digits or '_' (a model's node names).
bool ValidNodeName(const std::string& name);

// Every model of every row that names a node: its impact or duration models and its cessation models (the Imp_
// and Ces_ columns). The match is by model name, so a model a row moves must be its own: one that a row without
// a node uses too (the game's own effects) stays where the game puts it, and one that two rows send to different
// nodes keeps the first row's. Each such case, and a row with a bad node name, is a line in `refused`.
EffectNodes EffectNodesFrom(const TwoDA& table);

// Nodes set at run time (the VFX bench's preview, core's DevSeam::set_effect_node): model (lower case) -> node.
using EffectNodeOverrides = std::map<std::string, EffectNode>;

// Where `model` (lower case) goes: its override if it has one, else its row's; false when neither names a node.
bool FindEffectNode(const EffectNodes& rows, const EffectNodeOverrides& overrides, const std::string& model,
                    EffectNode* where);

}  // namespace data
