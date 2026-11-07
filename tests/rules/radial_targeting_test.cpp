#include "check.h"
#include "rules/radial_targeting.h"

using namespace rules;

// #140: a spell's facts from its spells.2da values.
TEST(RadialSpellFromRow) {
  const RadialSpell fireball = RadialSpellFromRow("L", "0x2E", "1", "sphere");
  CHECK_EQ(fireball.area, true);
  CHECK_EQ(fireball.harmful, true);
  CHECK_EQ(fireball.at_once, false);
  const RadialSpell hold = RadialSpellFromRow("M", "0x2A", "1", "");  // single target, harmful
  CHECK_EQ(hold.area, false);
  CHECK_EQ(hold.harmful, true);
  CHECK_EQ(hold.at_once, false);
  const RadialSpell cure = RadialSpellFromRow("T", "0x2B", "0", "");  // single target, helpful
  CHECK_EQ(cure.area, false);
  CHECK_EQ(cure.harmful, false);
  CHECK_EQ(cure.at_once, false);
  CHECK_EQ(RadialSpellFromRow("P", "0x01", "0", "").at_once, true);        // self-only, personal (Shield)
  CHECK_EQ(RadialSpellFromRow("T", "0x01", "0", "").at_once, true);        // only the self bit
  CHECK_EQ(RadialSpellFromRow("P", "0x05", "1", "sphere").at_once, true);  // personal: centred on the caster
  CHECK_EQ(RadialSpellFromRow("p", "0x2F", "0", "sphere").at_once, true);
  CHECK_EQ(RadialSpellFromRow("S", "0x0a", "0", "").at_once, false);       // lower-case hex: not self-only
  CHECK_EQ(RadialSpellFromRow("S", "0x09", "0", "").at_once, false);       // self or an item
  CHECK_EQ(RadialSpellFromRow("S", "", "0", "").at_once, false);           // no TargetType
  CHECK_EQ(RadialSpellFromRow("L", "0x2E", "0", "").harmful, false);
  CHECK_EQ(RadialSpellFromRow("L", "0x2E", "", "").harmful, false);
  CHECK_EQ(cure.self_target, true);                                     // 0x2B: self, creature ...
  CHECK_EQ(RadialSpellFromRow("T", "0x02", "0", "").self_target, false);  // another creature only (Help)
  CHECK_EQ(RadialSpellFromRow("M", "0x2A", "1", "").self_target, false);
  CHECK_EQ(RadialSpellFromRow("T", "0x03", "0", "").self_target, true);   // Lay on Hands
  CHECK_EQ(RadialSpellFromRow("S", "", "0", "").self_target, true);       // none given: as the game
}

// #140: the owner's table (Q1 right, Q2 b).
TEST(RadialRetargets) {
  RadialSpell single_helpful;
  RadialSpell single_harmful;
  single_harmful.harmful = true;
  RadialSpell area_helpful;
  area_helpful.area = true;
  RadialSpell area_harmful;
  area_harmful.area = true;
  area_harmful.harmful = true;
  RadialSpell at_once;
  at_once.area = true;
  at_once.harmful = true;
  at_once.at_once = true;
  // An enemy, a neutral creature or an object: a single target is cast, an area retargets.
  CHECK_EQ(RadialRetargets(RadialOn::kOther, single_helpful), false);
  CHECK_EQ(RadialRetargets(RadialOn::kOther, single_harmful), false);
  CHECK_EQ(RadialRetargets(RadialOn::kOther, area_helpful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kOther, area_harmful), true);
  // The ground: every spell retargets.
  CHECK_EQ(RadialRetargets(RadialOn::kGround, single_helpful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kGround, single_harmful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kGround, area_helpful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kGround, area_harmful), true);
  // Yourself: a helpful single target is yours; harmful or area (Q2 b) retargets.
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, single_helpful), false);
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, single_harmful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, area_helpful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, area_harmful), true);
  // An ally (Q1): the same as yourself.
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, single_helpful), false);
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, single_harmful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, area_helpful), true);
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, area_harmful), true);
  // A helpful spell that can't be cast on its caster (Help): yourself retargets, an ally or an enemy doesn't.
  RadialSpell others_only;
  others_only.self_target = false;
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, others_only), true);
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, others_only), false);
  CHECK_EQ(RadialRetargets(RadialOn::kOther, others_only), false);
  // Self-only and caster-centred: at once, wherever the radial was opened.
  CHECK_EQ(RadialRetargets(RadialOn::kGround, at_once), false);
  CHECK_EQ(RadialRetargets(RadialOn::kOther, at_once), false);
  CHECK_EQ(RadialRetargets(RadialOn::kSelf, at_once), false);
  CHECK_EQ(RadialRetargets(RadialOn::kAlly, at_once), false);
}

// #140 (0.89.2): the radial's target as the server knows it - the client sets bit 31.
TEST(ServerObjectId) {
  CHECK_EQ(ServerObjectId(0x8000031Au), static_cast<uint32_t>(0x31A));         // a spawned orc
  CHECK_EQ(ServerObjectId(0xFFFFFFFFu), static_cast<uint32_t>(0x7FFFFFFF));    // the player character
  CHECK_EQ(ServerObjectId(0x7F000000u), static_cast<uint32_t>(0x7F000000));    // OBJECT_INVALID: the ground
  CHECK_EQ(ServerObjectId(0x0000031Au), static_cast<uint32_t>(0x31A));         // already the server's
}

// #140: the creature menu's feat param, as the game passes it on.
TEST(CreatureMenuFeatParams) {
  CHECK_EQ(CreatureMenuFeatCursorParam(300), static_cast<int64_t>(300));
  CHECK_EQ(CreatureMenuFeatCursorParam((2 << 16) | 300), static_cast<int64_t>(300));  // the extra dropped
  CHECK_EQ(CreatureMenuFeatCursorParam((2 << 16) | 0x430), static_cast<int64_t>(0x430));
  CHECK_EQ(CreatureMenuFeatCursorParam((1 << 16) | 0x431), static_cast<int64_t>(0x10431));  // past 0x430: kept
  CHECK_EQ(CreatureMenuFeatCursorParam((3 << 16) | 5), static_cast<int64_t>(0x30005));      // feat 5: kept
  CHECK_EQ(CreatureMenuFeatCursorParam(0xFDE80005LL), static_cast<int64_t>(0xFFFFFFFFFDE80005ULL));  // sign-extended
  CHECK_EQ(CreatureMenuFeatUseParam((2 << 16) | 300), static_cast<uint32_t>(0x2012C));
  CHECK_EQ(CreatureMenuFeatUseParam(0xFDE80005LL), static_cast<uint32_t>(0xFDE80005u));
  CHECK_EQ(CreatureMenuFeatUseParam(1313), static_cast<uint32_t>(1313));
}
