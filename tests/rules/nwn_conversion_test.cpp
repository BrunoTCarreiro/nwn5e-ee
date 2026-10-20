#include <limits>

#include "check.h"
#include "rules/nwn_conversion.h"

using namespace rules;

// Decision 11. Breaks caught: henchmen treated as monsters (their gear AC
// replaced by the CR table), or summons/companions/familiars treated as
// characters (keeping 3e AC and gaining Extra Attack).
TEST(PlayersAndHenchmenUseCharacterRules) {
  CHECK_EQ(UsesCharacterRules(true, 0), true);   // player character
  CHECK_EQ(UsesCharacterRules(false, 1), true);  // henchman
  CHECK_EQ(UsesCharacterRules(false, 0), false); // monster / NPC
  CHECK_EQ(UsesCharacterRules(false, 2), false); // animal companion
  CHECK_EQ(UsesCharacterRules(false, 3), false); // familiar
  CHECK_EQ(UsesCharacterRules(false, 4), false); // summoned
  CHECK_EQ(UsesCharacterRules(false, 5), false); // dominated
}

// Breaks caught: player characters using their CR (NWN gives PCs a CR too), or
// creatures using their level.
TEST(PlayerCharactersUseLevelCreaturesUseChallengeRating) {
  CHECK_EQ(ProficiencyBonusForCreature(true, 9, 1), 4);
  CHECK_EQ(ProficiencyBonusForCreature(false, 9, 1), 2);
  CHECK_EQ(ProficiencyBonusForCreature(false, 1, 21), 7);
}

// Breaks caught: NWN epic player levels (21-40) raising the bonus past +6.
TEST(EpicPlayerLevelsStayAtPlusSix) {
  CHECK_EQ(ProficiencyBonusForCreature(true, 30, 30), 6);
}

// Breaks caught: adding the BAB instead of removing it, or dropping the rest
// of the engine's modifier.
TEST(AttackModifierSwapsBaseAttackBonusForProficiency) {
  // Level 10 fighter: BAB +10, Str +3, +1 weapon, Weapon Focus +1 -> engine +15.
  CHECK_EQ(ConvertAttackModifier(15, 10, 4), 9);
  // Level 1 wizard: BAB +0, Dex +2 -> engine +2.
  CHECK_EQ(ConvertAttackModifier(2, 0, 2), 4);
}

// Breaks caught: the classes.2da row order shifted by one (Fighter 4 / Monk 5 /
// Wizard 10 mislabelled), or racial classes mapped to a base class.
TEST(NwnClassIdsMapToBaseClasses) {
  CHECK_EQ(ClassFromNwnId(0).value_or(CharacterClass::kWarlock), CharacterClass::kBarbarian);
  CHECK_EQ(ClassFromNwnId(4).value_or(CharacterClass::kWarlock), CharacterClass::kFighter);
  CHECK_EQ(ClassFromNwnId(5).value_or(CharacterClass::kWarlock), CharacterClass::kMonk);
  CHECK_EQ(ClassFromNwnId(7).value_or(CharacterClass::kWarlock), CharacterClass::kRanger);
  CHECK_EQ(ClassFromNwnId(8).value_or(CharacterClass::kWarlock), CharacterClass::kRogue);
  CHECK_EQ(ClassFromNwnId(10).value_or(CharacterClass::kWarlock), CharacterClass::kWizard);
  CHECK_EQ(ClassFromNwnId(11).has_value(), false);   // Aberration
  CHECK_EQ(ClassFromNwnId(24).has_value(), false);   // Outsider
  CHECK_EQ(ClassFromNwnId(38).has_value(), false);   // Ooze
  CHECK_EQ(ClassFromNwnId(39).has_value(), false);   // empty classes.2da row
  CHECK_EQ(ClassFromNwnId(255).has_value(), false);  // empty slot
  CHECK_EQ(ClassFromNwnId(-1).has_value(), false);
}

// Decision 10: a prestige class counts as levels of the class it would be a
// 2024 subclass of. Breaks caught: a prestige class mapped to the wrong
// parent, or left unmapped.
TEST(PrestigeClassesCountAsTheirParentClass) {
  const auto parent = [](int id) { return ClassFromNwnId(id).value_or(CharacterClass::kWarlock); };
  CHECK_EQ(parent(27), CharacterClass::kRogue);    // Shadowdancer
  CHECK_EQ(parent(28), CharacterClass::kBard);     // Harper Scout
  CHECK_EQ(parent(29), CharacterClass::kFighter);  // Arcane Archer
  CHECK_EQ(parent(30), CharacterClass::kRogue);    // Assassin
  CHECK_EQ(parent(31), CharacterClass::kPaladin);  // Blackguard
  CHECK_EQ(parent(32), CharacterClass::kPaladin);  // Champion of Torm
  CHECK_EQ(parent(33), CharacterClass::kFighter);  // Weapon Master
  CHECK_EQ(parent(34), CharacterClass::kWizard);   // Pale Master
  CHECK_EQ(parent(35), CharacterClass::kDruid);    // Shifter
  CHECK_EQ(parent(36), CharacterClass::kFighter);  // Dwarven Defender
  CHECK_EQ(parent(37), CharacterClass::kSorcerer); // Red Dragon Disciple
  CHECK_EQ(parent(41), CharacterClass::kFighter);  // Purple Dragon Knight
}

// Breaks caught: player characters keeping NWN's iterative count (a level 6
// fighter: 2 attacks in 3e and 5e, a level 6 rogue: 2 in 3e but 1 in 5e).
TEST(PlayerCharactersAttackPerExtraAttack) {
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 6}}, 2), 2);
  CHECK_EQ(MainHandAttacksPerRound(true, {{8, 6}}, 2), 1);
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 5}}, 1), 2);
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 20}}, 4), 4);
}

// Breaks caught: prestige levels not adding to their parent class (Fighter 6 /
// Weapon Master 6 is Fighter 12: 3 attacks, not 2), racial levels counting, or
// a multiclass adding levels across different classes.
TEST(PrestigeLevelsAddToTheParentClassForExtraAttack) {
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 4}, {33, 6}}, 2), 2);
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 6}, {33, 6}}, 3), 3);
  CHECK_EQ(MainHandAttacksPerRound(true, {{8, 10}, {30, 10}}, 4), 1);
  CHECK_EQ(MainHandAttacksPerRound(true, {{24, 10}}, 3), 1);
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 4}, {0, 4}}, 2), 1);
  CHECK_EQ(MainHandAttacksPerRound(true, {{4, 5}, {6, 5}}, 2), 2);
}

// Breaks caught: monsters losing their multiple attacks (NWN racial classes have
// no Extra Attack), or creatures switched to the player rule.
TEST(CreaturesKeepTheEngineAttackCount) {
  CHECK_EQ(MainHandAttacksPerRound(false, {{24, 12}}, 3), 3);
  CHECK_EQ(MainHandAttacksPerRound(false, {{8, 6}}, 2), 2);
}

// Breaks caught: 3e threat ranges (19-20, 18-20, Keen/Improved Critical 15-20)
// surviving, or "no weapon in this hand" (21) turned into a usable threat.
TEST(CriticalThreatOnlyOnTwentyExceptEmptyHand) {
  CHECK_EQ(ConvertCriticalThreatFloor(20), 20);
  CHECK_EQ(ConvertCriticalThreatFloor(19), 20);
  CHECK_EQ(ConvertCriticalThreatFloor(15), 20);
  CHECK_EQ(ConvertCriticalThreatFloor(21), 21);
}

// Breaks caught: 3e x3/x4 multipliers surviving, or "no weapon" (x0) turned into x2.
TEST(CriticalMultiplierIsTwoExceptEmptyHand) {
  CHECK_EQ(ConvertCriticalMultiplier(2), 2);
  CHECK_EQ(ConvertCriticalMultiplier(3), 2);
  CHECK_EQ(ConvertCriticalMultiplier(4), 2);
  CHECK_EQ(ConvertCriticalMultiplier(0), 0);
}

// Decision 13. Breaks caught: flat bonuses (Strength) repeated on a critical
// (the 3e x2), dice not rolled twice, a 3e x3 weapon rolling its dice three
// times, or "no weapon" (x0) turned into an attack.
TEST(CriticalsRollDiceTwiceAndAddFlatBonusesOnce) {
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kDice, 2), 2);
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kDice, 3), 2);
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kFlatBonus, 2), 1);
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kFlatBonus, 4), 1);
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kDice, 0), 0);
  CHECK_EQ(CriticalMultiplierFor(CritDamagePart::kFlatBonus, 0), 0);
}

// Decision 13, effect damage bonuses (GetTotalEffectBonus: each entry is a
// dice count and a die size or constant). Breaks caught: a constant bonus
// (Damage Bonus +2) repeated on a critical, or a dice bonus (+1d6 fire) rolled
// only once; the engine's own test is "dice count != 0".
TEST(EffectDamageBonusesDiceTwiceConstantsOnce) {
  CHECK_EQ(EffectDamagePart(0), CritDamagePart::kFlatBonus);
  CHECK_EQ(EffectDamagePart(1), CritDamagePart::kDice);
  CHECK_EQ(EffectDamagePart(2), CritDamagePart::kDice);
  CHECK_EQ(CriticalMultiplierFor(EffectDamagePart(0), 2), 1);
  CHECK_EQ(CriticalMultiplierFor(EffectDamagePart(1), 2), 2);
}

// Decision 14. Breaks caught: off-by-one (the killing attack counted as still
// to come, so a single-attack creature would "cleave"), or a negative count
// when the index runs past the total (extra attacks added mid-round).
TEST(RemainingAttacksAfterTheCurrentOne) {
  CHECK_EQ(RemainingAttacksInRound(1, 0), 0);
  CHECK_EQ(RemainingAttacksInRound(2, 0), 1);
  CHECK_EQ(RemainingAttacksInRound(2, 1), 0);
  CHECK_EQ(RemainingAttacksInRound(3, 0), 2);
  CHECK_EQ(RemainingAttacksInRound(3, 2), 0);
  CHECK_EQ(RemainingAttacksInRound(2, 5), 0);
  CHECK_EQ(RemainingAttacksInRound(0, 0), 0);
}

// Decision 14, ranged. Breaks caught: a nearer enemy that isn't fighting (the
// caged Hook Horror, a sleeping guard) chosen over one that is; the range
// limit dropped or made inclusive (the engine's own search uses "<"); a far
// candidate beating a near one.
TEST(RangedShotsMoveOnlyToEnemiesInTheFight) {
  const RetargetCandidate idle_near_fighting_far[] = {{5.0f, false}, {8.0f, true}};
  CHECK_EQ(NearestEnemyInTheFight(idle_near_fighting_far, 2, 20.0f), 1);
  const RetargetCandidate only_idle[] = {{5.0f, false}};
  CHECK_EQ(NearestEnemyInTheFight(only_idle, 1, 20.0f), -1);
  const RetargetCandidate beyond_range[] = {{25.0f, true}};
  CHECK_EQ(NearestEnemyInTheFight(beyond_range, 1, 20.0f), -1);
  const RetargetCandidate at_range[] = {{20.0f, true}};
  CHECK_EQ(NearestEnemyInTheFight(at_range, 1, 20.0f), -1);
  const RetargetCandidate far_then_near[] = {{12.0f, true}, {6.0f, true}};
  CHECK_EQ(NearestEnemyInTheFight(far_then_near, 2, 20.0f), 1);
  const RetargetCandidate tie[] = {{3.0f, true}, {3.0f, true}};
  CHECK_EQ(NearestEnemyInTheFight(tie, 2, 20.0f), 0);
  CHECK_EQ(NearestEnemyInTheFight(nullptr, 0, 20.0f), -1);
}

// Decision 14, ranged: "in the fight" = its attack order targets the attacker
// or the attacker's side. Breaks caught: an enemy with no attack order counted
// (idle, caged, asleep); an enemy fighting a henchman (same faction) or a
// summon (master = attacker) left out; an enemy fighting a third party
// counted; two unknown factions (-1) taken as the same side.
TEST(AttackOrderAgainstTheAttackersSide) {
  const uint32_t pc = 0x7FFFFFFF, henchman = 0x300, summon = 0x301, other = 0x302, none = 0x7F000000;
  CHECK_EQ(IsAgainstSide({false, pc, none, 5}, pc, 5), false);
  CHECK_EQ(IsAgainstSide({true, pc, none, 5}, pc, 5), true);
  CHECK_EQ(IsAgainstSide({true, henchman, none, 5}, pc, 5), true);
  CHECK_EQ(IsAgainstSide({true, summon, pc, 9}, pc, 5), true);
  CHECK_EQ(IsAgainstSide({true, other, none, 2}, pc, 5), false);
  CHECK_EQ(IsAgainstSide({true, none, none, -1}, pc, -1), false);
}

// Breaks caught: garbage from a wrong memory offset (NaN, huge, negative)
// accepted as a challenge rating; real CRs (0, fractions, 30+) rejected.
TEST(PlausibleChallengeRatingsOnly) {
  CHECK_EQ(IsPlausibleChallengeRating(0.0f), true);
  CHECK_EQ(IsPlausibleChallengeRating(0.25f), true);
  CHECK_EQ(IsPlausibleChallengeRating(13.5f), true);
  CHECK_EQ(IsPlausibleChallengeRating(40.0f), true);
  CHECK_EQ(IsPlausibleChallengeRating(-0.5f), false);
  CHECK_EQ(IsPlausibleChallengeRating(61.0f), false);
  CHECK_EQ(IsPlausibleChallengeRating(3.0e9f), false);
  CHECK_EQ(IsPlausibleChallengeRating(std::numeric_limits<float>::quiet_NaN()), false);
  CHECK_EQ(IsPlausibleChallengeRating(std::numeric_limits<float>::infinity()), false);
}

// Breaks caught: player AC replaced by the CR table, or creatures keeping 3e AC.
TEST(OnlyNonPlayerCreaturesGetTheChallengeRatingArmorClass) {
  CHECK_EQ(ConvertArmorClass(true, 32, 20), 32);
  CHECK_EQ(ConvertArmorClass(false, 32, 20), 19);
  CHECK_EQ(ConvertArmorClass(false, 12, 1), 13);
}
