#pragma once

// #140, radial casting retargets with the area shown (docs/PLAN.md "Phase 7"; owner, 2026-11-06: "If I select an
// enemy, sure, we can assume we want to cast a spell that targets them IF it's a single target spell. If it's an
// area spell you can give the player back agency to retarget. if the radial menu is summoned on an unspecified piece
// of scenery, like the ground, no matter if it is a helpful or harmful, single target or area effect, allow the
// player to retarget and show areas of effect. ... If it's helpful I think you can assume the target is the
// character, if harmful, allow the player to retarget"; decided "Q1. right Q2. b Q3. a Q4. before").
//
// A spell, a spell-like ability or a feat's spell picked from the radial menu is either cast at once at what the
// radial was opened on (NWN's way) or hands the player the targeting cursor the quickbar uses, which shows the
// spell's area. "Area": spells.2da TargetShape set; "harmful": HostileSetting 1. Cast at once whatever the radial was
// opened on: a self-only spell (TargetType only the self bit), one of personal range (Range P: centred on the
// caster) or a feat that only targets its user (feat.2da TARGETSELF).

#include <stdint.h>

#include <string>

namespace rules {

// What the radial menu was opened on.
enum class RadialOn {
  kSelf,    // the character casting
  kAlly,    // a creature friendly to it (a henchman, a companion)
  kOther,   // any other creature (hostile or neutral), a door, a placeable, an item
  kGround,  // the ground or scenery (no object)
};

struct RadialSpell {
  bool area = false;     // a TargetShape (sphere, cone, line ...)
  bool harmful = false;  // HostileSetting 1
  bool at_once = false;  // self-only, personal range or a self-only feat: nothing to aim
  bool self_target = true;  // TargetType has the self bit (or gives none): it can be cast on its caster
};

// The spell's facts from its spells.2da values ("" for ****).
RadialSpell RadialSpellFromRow(const std::string& range, const std::string& target_type,
                               const std::string& hostile_setting, const std::string& target_shape);

// True: the targeting cursor, the area shown. False: cast at once at the radial's target (NWN's way).
//   on an enemy, a neutral creature or an object: an area spell retargets, a single-target one is cast;
//   on the ground: every spell retargets;
//   on yourself or an ally (Q1, Q2 b): a helpful single-target spell is cast on them, a harmful or area one
//   retargets - on yourself, also one that can't be cast on its caster (Help: another creature only).
bool RadialRetargets(RadialOn on, const RadialSpell& spell);

// The creature menu's feat param (feat | extra << 16) as the game's RadialMenuUseFeat passes it to the feat cursor:
// the extra only for feat 5 and feats past 0x430.
int64_t CreatureMenuFeatCursorParam(int64_t param);
// ... and as it passes it to a use at once: always feat | extra << 16 (32 bits).
uint32_t CreatureMenuFeatUseParam(int64_t param);

const char* RadialOnName(RadialOn on);

// The server's id of an object the client knows: the client's is the server's with bit 31 set
// (CNWSMessage::WriteOBJECTIDServer; the player character 0x7FFFFFFF is 0xFFFFFFFF there, OBJECT_INVALID
// 0x7F000000 stays). 0.89.2: 0.89.1 looked the radial's target up as the client knows it - no object, so every
// creature counted as the ground.
uint32_t ServerObjectId(uint32_t client_id);

}  // namespace rules
