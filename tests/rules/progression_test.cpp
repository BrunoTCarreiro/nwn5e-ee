#include "check.h"
#include "rules/progression.h"

using namespace rules;

// Breaks caught: an off-by-one between "entry i" and "level i + 1", NWN's
// formula drifting from exptable.2da, the table past 20 dropping below 355,000.
TEST(XpTablesAre2024AndNwn) {
  CHECK_EQ(Xp2024ForLevel(1), 0u);
  CHECK_EQ(Xp2024ForLevel(2), 300u);
  CHECK_EQ(Xp2024ForLevel(5), 6500u);
  CHECK_EQ(Xp2024ForLevel(15), 165000u);
  CHECK_EQ(Xp2024ForLevel(20), 355000u);
  CHECK_EQ(NwnXpForLevel(2), 1000u);
  CHECK_EQ(NwnXpForLevel(15), 105000u);
  CHECK_EQ(NwnXpForLevel(20), 190000u);
  CHECK_EQ(FaithfulXpForLevel(20), 355000u);
  CHECK_EQ(FaithfulXpForLevel(21), 375000u);  // 355,000 + NWN's 20 -> 21 step
  CHECK_EQ(FaithfulLevelForXp(0), 1);
  CHECK_EQ(FaithfulLevelForXp(299), 1);
  CHECK_EQ(FaithfulLevelForXp(300), 2);
  CHECK_EQ(FaithfulLevelForXp(105000), 12);
  CHECK_EQ(FaithfulLevelForXp(165000), 15);
}

// Breaks caught: HotU's level 15 landing at 12, a respawn penalty taking
// levels or giving XP, Prelude's level 2 / 3 not converting, a DM level-down
// not working, an XP cost dropping a level.
TEST(ScriptSetXpKeepsTheScriptsLevels) {
  // HotU: SetXP(105,000) = NWN level 15, on a level-13 character.
  CHECK_EQ(ScriptSetXp2024(105000, 125000, 13), 165000u);
  // Prelude: 1,000 = level 2 and 3,000 = level 3, on a new character.
  CHECK_EQ(ScriptSetXp2024(1000, 0, 1), 300u);
  CHECK_EQ(ScriptSetXp2024(3000, 400, 2), 900u);
  // Respawn at level 3 with 2,000 XP: NWN's clamp asks for 3,000 — no gain.
  CHECK_EQ(ScriptSetXp2024(3000, 2000, 3), 2000u);
  // Respawn at level 15 with 166,000: the clamp asks for 105,000 — keeps 15.
  CHECK_EQ(ScriptSetXp2024(105000, 166000, 15), 165000u);
  // Respawn penalty that doesn't reach the clamp: kept, but not below 15.
  CHECK_EQ(ScriptSetXp2024(165250, 166000, 15), 165250u);
  CHECK_EQ(ScriptSetXp2024(164750, 165500, 15), 165000u);
  // x2_dm_leveldown on a level-6 character: NWN's level-5 threshold.
  CHECK_EQ(ScriptSetXp2024(10000, 15000, 6), 6500u);
  // SetXP(0) resets, as in NWN.
  CHECK_EQ(ScriptSetXp2024(0, 7000, 5), 0u);
}

// Breaks caught: NWN's every-3-levels cadence, the Fighter's 6 / 14 or the
// Rogue's 10 missing or given to others, level 19 (Epic Boon) missing, levels
// past 20 counted.
TEST(FeatLevelsAre2024) {
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kWizard, 3), false);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kWizard, 4), true);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kWizard, 6), false);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kFighter, 6), true);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kFighter, 14), true);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kRogue, 10), true);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kFighter, 10), false);
  CHECK_EQ(IsFeatLevel2024(CharacterClass::kCleric, 19), true);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kFighter, 20), 7);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kRogue, 20), 6);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kBard, 20), 5);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kBard, 3), 0);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kFighter, 8), 3);
  CHECK_EQ(FeatLevelsReached2024(CharacterClass::kFighter, 30), 7);
}

// Breaks caught: the "or" / "and" classes swapped, a class already held
// being checked, the current classes not being checked, a first level
// (no classes yet) being refused.
TEST(MulticlassingNeeds13s) {
  //                Str Dex Con Int Wis Cha
  const int fighter[6] = {15, 12, 14, 10, 12, 8};
  CHECK_EQ(CanMulticlassInto(CharacterClass::kWizard, {CharacterClass::kFighter}, fighter, nullptr), false);
  CHECK_EQ(CanMulticlassInto(CharacterClass::kFighter, {CharacterClass::kFighter}, fighter, nullptr), true);
  CHECK_EQ(CanMulticlassInto(CharacterClass::kWizard, {}, fighter, nullptr), true);
  const int smart_fighter[6] = {15, 12, 14, 13, 12, 8};
  CHECK_EQ(CanMulticlassInto(CharacterClass::kWizard, {CharacterClass::kFighter}, smart_fighter, nullptr), true);
  const int dex_fighter[6] = {8, 15, 14, 13, 12, 10};
  CHECK_EQ(CanMulticlassInto(CharacterClass::kWizard, {CharacterClass::kFighter}, dex_fighter, nullptr), true);
  // The current class must qualify too: a weak wizard can't become a rogue.
  const int weak_wizard[6] = {8, 14, 12, 12, 10, 10};
  CharacterClass failing = CharacterClass::kBard;
  CHECK_EQ(CanMulticlassInto(CharacterClass::kRogue, {CharacterClass::kWizard}, weak_wizard, &failing), false);
  CHECK_EQ(failing == CharacterClass::kWizard, true);
  const int monk[6] = {10, 14, 12, 10, 12, 8};
  CHECK_EQ(MeetsPrimaryAbility(CharacterClass::kMonk, monk), false);  // Dex and Wis
  const int paladin[6] = {13, 10, 12, 8, 10, 13};
  CHECK_EQ(MeetsPrimaryAbility(CharacterClass::kPaladin, paladin), true);
}

TEST(MulticlassRefusalSaysWhy) {
  //                Str Dex Con Int Wis Cha
  const int fighter[6] = {17, 13, 15, 8, 12, 10};  // the owner's Part 3 Fighter
  CHECK_EQ(MulticlassRefusalText(CharacterClass::kMonk, CharacterClass::kMonk, fighter) ==
               "Multiclassing (2024): you can't take a level in Monk now - it needs Dexterity and Wisdom 13, and you "
               "have Dexterity 13, Wisdom 12.",
           true);
  CHECK_EQ(MulticlassRefusalText(CharacterClass::kWizard, CharacterClass::kWizard, fighter) ==
               "Multiclassing (2024): you can't take a level in Wizard now - it needs Intelligence 13, and you have "
               "Intelligence 8.",
           true);
  const int weak_wizard[6] = {8, 14, 12, 12, 10, 10};
  CHECK_EQ(MulticlassRefusalText(CharacterClass::kRogue, CharacterClass::kWizard, weak_wizard) ==
               "Multiclassing (2024): you can't take a level in Rogue now - every class you have needs its score too, "
               "and Wizard needs Intelligence 13: you have Intelligence 12.",
           true);
  CHECK_EQ(PrimaryAbilities(CharacterClass::kFighter).size(), 2u);
}
