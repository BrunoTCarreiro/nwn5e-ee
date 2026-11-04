#include "check.h"
#include "rules/spell_slots.h"

using namespace rules;

// #36 Part 1 (owner, 2026-11-04: "go with your recommendations ..."): SRD 5.2's tables. Breaks caught: a caster
// class's kind (Paladin a full caster, Sorcerer none), a slot of the full or the half table, the tables' ends,
// cantrips' steps or a class's count, a class's prepared spells, the gain's level 0 not the cantrips, a cantrip
// spending a use in faithful.
TEST(SpellSlots2024) {
  CHECK_EQ(CasterKindOfClass(2) == CasterKind::kFull, true);   // Cleric
  CHECK_EQ(CasterKindOfClass(9) == CasterKind::kFull, true);   // Sorcerer
  CHECK_EQ(CasterKindOfClass(6) == CasterKind::kHalf, true);   // Paladin
  CHECK_EQ(CasterKindOfClass(7) == CasterKind::kHalf, true);   // Ranger
  CHECK_EQ(CasterKindOfClass(4) == CasterKind::kNone, true);   // Fighter
  // The full casters' table.
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 1, 1), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 1, 2), 0);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 3, 2), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 9, 5), 1);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 17, 9), 1);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 18, 5), 3);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 20, 6), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 20, 7), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 21, 1), 0);
  CHECK_EQ(SpellSlots2024(CasterKind::kFull, 0, 1), 0);
  // The half casters' table: from level 1.
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 1, 1), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 3, 1), 3);
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 5, 2), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 13, 4), 1);
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 20, 5), 2);
  CHECK_EQ(SpellSlots2024(CasterKind::kHalf, 20, 6), 0);
  CHECK_EQ(MaxSpellLevel2024(CasterKind::kFull, 1), 1);
  CHECK_EQ(MaxSpellLevel2024(CasterKind::kFull, 17), 9);
  CHECK_EQ(MaxSpellLevel2024(CasterKind::kHalf, 4), 1);
  CHECK_EQ(MaxSpellLevel2024(CasterKind::kHalf, 17), 5);
  // Cantrips.
  CHECK_EQ(CantripsKnown2024(2, 1), 3);   // Cleric
  CHECK_EQ(CantripsKnown2024(2, 4), 4);
  CHECK_EQ(CantripsKnown2024(2, 10), 5);
  CHECK_EQ(CantripsKnown2024(3, 3), 2);   // Druid
  CHECK_EQ(CantripsKnown2024(1, 9), 3);   // Bard
  CHECK_EQ(CantripsKnown2024(9, 1), 4);   // Sorcerer
  CHECK_EQ(CantripsKnown2024(9, 20), 6);
  CHECK_EQ(CantripsKnown2024(10, 10), 5); // Wizard
  CHECK_EQ(CantripsKnown2024(6, 20), 0);  // Paladin
  // Prepared spells.
  CHECK_EQ(PreparedSpells2024(2, 1), 4);
  CHECK_EQ(PreparedSpells2024(2, 12), 16);
  CHECK_EQ(PreparedSpells2024(9, 1), 2);
  CHECK_EQ(PreparedSpells2024(10, 14), 18);
  CHECK_EQ(PreparedSpells2024(10, 20), 25);
  CHECK_EQ(PreparedSpells2024(6, 20), 15);
  CHECK_EQ(PreparedSpells2024(7, 9), 9);
  CHECK_EQ(PreparedSpells2024(4, 5), 0);
  // The gain (cls_spgn): level 0 the cantrips.
  CHECK_EQ(SpellGain2024(10, 1, 0), 3);
  CHECK_EQ(SpellGain2024(6, 5, 2), 2);
  CHECK_EQ(SpellGain2024(9, 5, 3), 2);
  CHECK_EQ(CastSpendsUse(true, 0), false);
  CHECK_EQ(CastSpendsUse(true, 1), true);
  CHECK_EQ(CastSpendsUse(false, 0), true);
}
