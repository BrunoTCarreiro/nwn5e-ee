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
  // 0.75.1 (owner, 2026-11-05: "attempting to do !test spellcleric crashes the game"): 0xFF is no spell level.
  CHECK_EQ(SpellLevelMet2024(0), true);
  CHECK_EQ(SpellLevelMet2024(9), true);
  CHECK_EQ(SpellLevelMet2024(0xFF), false);
  CHECK_EQ(SpellLevelMet2024(10), false);
}

// 0.74.2 (owner, 2026-11-04: "SL-2 should not be allowed to queue spells a larger number of times than the slots
// available"). Breaks caught: a full queue let through or a short one refused, a cantrip's queue refused, NWN mode
// refusing, a memorised spell matched by level or ignoring metamagic, a spontaneous cast matched by spell.
TEST(CastQueue) {
  CHECK_EQ(CastQueueFull(true, 1, 4, 4), true);    // four 1st-level casts queued, four slots left: a fifth refused
  CHECK_EQ(CastQueueFull(true, 1, 3, 4), false);
  CHECK_EQ(CastQueueFull(true, 2, 1, 0), true);    // none left
  CHECK_EQ(CastQueueFull(true, 0, 9, 3), false);   // a cantrip: at will
  CHECK_EQ(CastQueueFull(false, 1, 4, 4), false);  // NWN: the engine's own check at cast time
  const QueuedCast magic_missile{107, 0, 1}, sleep{166, 0, 1}, empowered{107, 1, 3}, web{213, 0, 2};
  CHECK_EQ(DrawsOnSameSlots(true, magic_missile, magic_missile), true);   // a Wizard: its ready copies
  CHECK_EQ(DrawsOnSameSlots(true, magic_missile, sleep), false);
  CHECK_EQ(DrawsOnSameSlots(true, magic_missile, empowered), false);
  CHECK_EQ(DrawsOnSameSlots(false, magic_missile, sleep), true);          // a Cleric: the slots of the level
  CHECK_EQ(DrawsOnSameSlots(false, magic_missile, web), false);
}

TEST(MulticlassSlots) {
  constexpr int kFighter = 4;
  const std::vector<ClassLevel> wizard_cleric{{10, 3}, {2, 2}};  // Wizard 3 / Cleric 2: the table's level 5
  CHECK_EQ(MulticlassCasterLevel2024(wizard_cleric), 5);
  CHECK_EQ(SharesSlotPool(wizard_cleric, 0), true);
  CHECK_EQ(SharesSlotPool(wizard_cleric, 1), true);
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 0, 1), 4);  // 4 / 3 / 2, the pool's, in both slots
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 0, 2), 3);
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 0, 3), 2);  // 3rd-level slots neither class has alone
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 1, 1), 4);  // the Cleric's: the same pool
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 1, 3), 2);
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 0, 4), 0);
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 1, 0), SpellGain2024(2, 2, 0));  // its cantrips its own
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 0, 0), SpellGain2024(10, 3, 0));
  const std::vector<ClassLevel> fighter_wizard{{kFighter, 5}, {10, 3}};  // one caster class: its own table
  CHECK_EQ(MulticlassCasterLevel2024(fighter_wizard), 0);
  CHECK_EQ(SharesSlotPool(fighter_wizard, 1), false);
  CHECK_EQ(SlotsForClassSlot2024(fighter_wizard, 1, 1), SpellGain2024(10, 3, 1));
  CHECK_EQ(SlotsForClassSlot2024(fighter_wizard, 1, 2), SpellGain2024(10, 3, 2));
  CHECK_EQ(SlotsForClassSlot2024(fighter_wizard, 0, 1), 0);
  const std::vector<ClassLevel> paladin_ranger{{6, 3}, {7, 3}};  // half each, rounded up: 2 + 2
  CHECK_EQ(MulticlassCasterLevel2024(paladin_ranger), 4);
  CHECK_EQ(SlotsForClassSlot2024(paladin_ranger, 0, 1), 4);
  CHECK_EQ(SlotsForClassSlot2024(paladin_ranger, 0, 2), 3);
  CHECK_EQ(SlotsForClassSlot2024(paladin_ranger, 1, 2), 3);
  const std::vector<ClassLevel> paladin_wizard{{6, 1}, {10, 1}};  // Paladin 1 counts 1 (rounded up)
  CHECK_EQ(MulticlassCasterLevel2024(paladin_wizard), 2);
  CHECK_EQ(SlotsForClassSlot2024(paladin_wizard, 0, 1), 3);
  const std::vector<ClassLevel> three{{kFighter, 2}, {2, 4}, {6, 5}};  // Cleric 4 + Paladin 5 / 2: 4 + 3
  CHECK_EQ(MulticlassCasterLevel2024(three), 7);
  CHECK_EQ(SlotsForClassSlot2024(three, 1, 4), 1);           // 4 / 3 / 3 / 1
  CHECK_EQ(SlotsForClassSlot2024(three, 2, 4), 1);           // the Paladin's: the same pool
  CHECK_EQ(SlotsForClassSlot2024(three, 0, 1), 0);           // the Fighter: none
  CHECK_EQ(SharesSlotPool(three, 0), false);
  CHECK_EQ(SharesSlotPool({{kFighter, 5}}, 0), false);
  CHECK_EQ(SharesSlotPool({{10, 0}, {2, 3}}, 0), false);     // a slot with no levels
  CHECK_EQ(MulticlassCasterLevel2024({{10, 0}, {2, 3}}), 0);  // ... isn't a second caster class
  CHECK_EQ(SharesSlotPool({{10, 3}, {2, 2}, {9, 0}}, 2), false);
  CHECK_EQ(SharesSlotPool(wizard_cleric, 2), false);         // no such slot
  CHECK_EQ(SharesSlotPool(wizard_cleric, -1), false);
  CHECK_EQ(SlotsForClassSlot2024(wizard_cleric, 2, 1), 0);    // no such slot
}

TEST(Upcasting) {
  // By choice: the domain level carries the slot level (0: the spell's own).
  CHECK_EQ(ChosenSlotLevel(1, 0), 1);
  CHECK_EQ(ChosenSlotLevel(1, 3), 3);
  CHECK_EQ(ChosenSlotLevel(3, 3), 3);
  CHECK_EQ(ChosenSlotLevel(2, 1), -1);   // never a lower slot
  CHECK_EQ(ChosenSlotLevel(1, 10), -1);
  CHECK_EQ(ChosenSlotLevel(0, 0), 0);    // a cantrip
  CHECK_EQ(ChosenSlotLevel(10, 0), -1);
  const SlotUses wizard{0, 4, 2, 1, 0, 0, 0, 0, 0, 0};  // four 1st-level slots, two 2nd, one 3rd
  CHECK_EQ(CastsLeft(wizard, 1), 7);                    // the slots at its level and above
  CHECK_EQ(CastsLeft(wizard, 2), 3);
  CHECK_EQ(CastsLeft(wizard, 3), 1);
  CHECK_EQ(CastsLeft(wizard, 4), 0);
  const SlotUses one_each{0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
  CHECK_EQ(CastFitsQueue(one_each, {}, 1), true);
  CHECK_EQ(CastFitsQueue(one_each, {1}, 1), false);     // its own level only: no upcast by itself
  CHECK_EQ(CastFitsQueue(one_each, {2}, 1), true);
  CHECK_EQ(CastFitsQueue(one_each, {1}, 2), true);      // a chosen 2nd-level cast
  CHECK_EQ(CastFitsQueue(one_each, {2}, 2), false);
  CHECK_EQ(CastFitsQueue(one_each, {0, 0, 0}, 1), true);  // queued cantrips spend nothing
  CHECK_EQ(CastFitsQueue(one_each, {}, 0), true);       // a cantrip, no cantrip uses counted
  const SlotUses empty{3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK_EQ(CastFitsQueue(empty, {}, 0), true);          // a cantrip always fits
  CHECK_EQ(CastFitsQueue(empty, {}, 1), false);
  CHECK_EQ(CastFitsQueue(empty, {}, 10), false);
  // The radial's levels: its own, then each higher one with slots; none when there is no higher one.
  const SlotUses table5{0, 4, 3, 2, 0, 0, 0, 0, 0, 0};  // a full caster 5
  CHECK_EQ(UpcastMenuLevels(table5, 1) == std::vector<int>({1, 2, 3}), true);
  CHECK_EQ(UpcastMenuLevels(table5, 2) == std::vector<int>({2, 3}), true);
  CHECK_EQ(UpcastMenuLevels(table5, 3).empty(), true);   // its highest level: cast at once
  CHECK_EQ(UpcastMenuLevels(table5, 0).empty(), true);   // a cantrip
  const SlotUses gap{0, 4, 0, 2, 0, 0, 0, 0, 0, 0};
  CHECK_EQ(UpcastMenuLevels(gap, 1) == std::vector<int>({1, 3}), true);
  CHECK_EQ(UpcastCountText(3, 4) == "L3\xB7" "4", true);
  CHECK_EQ(UpcastCountText(9, 12) == "L9\xB7" "12", true);
}

TEST(RadialParam) {
  // The game's packing: spell 0-15, domain level 24-27, class slot 28-29 + 38, metamagic 31-36, bits 30 / 37 others.
  const long long plain = 107LL | (1LL << 28);  // Magic Missile, class slot 1
  RadialCast cast = DecodeRadialCast(plain);
  CHECK_EQ(cast.spell, 107u);
  CHECK_EQ(cast.class_slot, 1);
  CHECK_EQ(cast.domain_level, 0);
  CHECK_EQ(cast.metamagic, 0);
  CHECK_EQ(cast.other, false);
  CHECK_EQ(DecodeRadialCast(107LL | (1LL << 28) | (1LL << 38)).class_slot, 5);  // slot 5: bits 28-29 and 38
  CHECK_EQ(DecodeRadialCast(107LL | (2LL << 28)).class_slot, 2);
  CHECK_EQ(DecodeRadialCast(371LL | (2LL << 28)).spell, 371u);                // a spell id past 255
  CHECK_EQ(DecodeRadialCast(107LL | (4LL << 31)).metamagic, 4);                 // Maximize
  CHECK_EQ(DecodeRadialCast(107LL | (32LL << 31)).metamagic, 32);               // Still, the top bit
  CHECK_EQ(DecodeRadialCast(107LL | (1LL << 30)).other, true);
  CHECK_EQ(DecodeRadialCast(107LL | (1LL << 37)).other, true);
  CHECK_EQ(DecodeRadialCast(107LL | (9LL << 24)).domain_level, 9);
  const long long upcast = RadialCastWithDomain(plain, 3);  // "Level 3"
  cast = DecodeRadialCast(upcast);
  CHECK_EQ(cast.domain_level, 3);
  CHECK_EQ(cast.spell, 107u);
  CHECK_EQ(cast.class_slot, 1);
  CHECK_EQ(DecodeRadialCast(RadialCastWithDomain(upcast, 0)).domain_level, 0);
  CHECK_EQ(RadialCastWithDomain(upcast, 0), plain);
  const long long more = RadialCastWithFromLevel(plain, 8);  // a "Level 8+" entry
  CHECK_EQ(RadialCastFromLevel(more), 8);
  CHECK_EQ(RadialCastFromLevel(plain), 0);
  CHECK_EQ(DecodeRadialCast(more).class_slot, 1);
  CHECK_EQ(DecodeRadialCast(more).domain_level, 0);
  CHECK_EQ(RadialCastWithFromLevel(more, 0), plain);
}
