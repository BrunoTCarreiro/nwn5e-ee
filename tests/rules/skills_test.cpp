#include <string>
#include <vector>

#include "check.h"
#include "rules/backgrounds.h"
#include "rules/skills.h"

using namespace rules;

// SK1. Breaks caught: Search mapped to Investigation (2024 finds hidden things
// with Perception); a merged row left out of its 2024 skill; a 3e-only row
// (Discipline, Concentration, Use Magic Device) given a 2024 skill; a key
// ability not switched to the 2024 one (Animal Handling is Wisdom).
TEST(NwnSkillsMapToThe2024Skills) {
  CHECK_EQ(Skill2024ForNwnSkill(kNwnHide) == Skill2024::kStealth, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnMoveSilently) == Skill2024::kStealth, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnSpot) == Skill2024::kPerception, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnListen) == Skill2024::kPerception, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnSearch) == Skill2024::kPerception, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnOpenLock) == Skill2024::kThievesTools, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnCraftWeapon) == Skill2024::kSmithsTools, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnDiscipline).has_value(), false);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnConcentration).has_value(), false);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnUseMagicDevice).has_value(), false);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnCraftTrap).has_value(), false);
  CHECK_EQ(Skill2024ForNwnSkill(99).has_value(), false);
  CHECK_EQ(AbilityFor(Skill2024::kAnimalHandling) == Ability::kWisdom, true);
  CHECK_EQ(AbilityFor(Skill2024::kPerception) == Ability::kWisdom, true);
  CHECK_EQ(AbilityFor(Skill2024::kThievesTools) == Ability::kDexterity, true);
  CHECK_EQ(std::string(Faithful2024SkillLabelForStrref(274)) == "Stealth (Hide)", true);
  CHECK_EQ(std::string(Faithful2024SkillLabelForStrref(284)) == "Perception (Search)", true);
  CHECK_EQ(Faithful2024SkillLabelForStrref(343) == nullptr, true);  // Discipline keeps its name
}

// Breaks caught: multi-word names not found; aliases missing; unknown words
// accepted.
TEST(SkillNamesParse) {
  std::vector<Skill2024> skills;
  std::string unknown;
  CHECK_EQ(ParseSkill2024List("Stealth sleight of hand, thieves' tools ANIMAL HANDLING", &skills, &unknown), true);
  CHECK_EQ(skills.size(), static_cast<size_t>(4));
  CHECK_EQ(skills[1] == Skill2024::kSleightOfHand, true);
  CHECK_EQ(skills[2] == Skill2024::kThievesTools, true);
  CHECK_EQ(skills[3] == Skill2024::kAnimalHandling, true);
  skills.clear();
  CHECK_EQ(ParseSkill2024List("stealth spot", &skills, &unknown), false);
  CHECK_EQ(unknown == "spot", true);
}

// SK2 / SK3. Breaks caught: Expertise without proficiency counted; effects
// dropped; script scaling applied to penalties.
TEST(SkillBonusesAre2024) {
  CHECK_EQ(SkillBonus2024(3, 2, false, false, 0), 3);
  CHECK_EQ(SkillBonus2024(3, 2, true, false, 1), 6);
  CHECK_EQ(SkillBonus2024(3, 4, true, true, 0), 11);
  CHECK_EQ(SkillBonus2024(3, 4, false, true, 0), 3);
  CHECK_EQ(ScriptSkillValue2024(6), 12);
  CHECK_EQ(ScriptSkillValue2024(-1), -1);
}

// 2024 choices. Breaks caught: a class's count or list wrong; the Rogue's
// Thieves' Tools not free; Expertise levels wrong (Rogue 1 / 6, Bard 2 / 9,
// Ranger 2 / 9, Wizard 2); Human's extra skill missing; multiclass extra skill.
TEST(SkillAllowancesAre2024) {
  const SkillAllowance fighter = SkillAllowanceFor({{CharacterClass::kFighter, 5}}, Species2024::kOther);
  CHECK_EQ(fighter.class_skills, 2);
  CHECK_EQ(fighter.TotalSkills(), 4);
  CHECK_EQ(fighter.expertise, 0);
  const SkillAllowance human_rogue = SkillAllowanceFor({{CharacterClass::kRogue, 6}}, Species2024::kHuman);
  CHECK_EQ(human_rogue.TotalSkills(), 7);
  CHECK_EQ(Has(human_rogue.granted, Skill2024::kThievesTools), true);
  CHECK_EQ(human_rogue.expertise, 4);
  CHECK_EQ(SkillAllowanceFor({{CharacterClass::kRogue, 5}}, Species2024::kOther).expertise, 2);
  CHECK_EQ(SkillAllowanceFor({{CharacterClass::kBard, 1}}, Species2024::kOther).expertise, 0);
  CHECK_EQ(SkillAllowanceFor({{CharacterClass::kBard, 9}}, Species2024::kOther).expertise, 4);
  CHECK_EQ(SkillAllowanceFor({{CharacterClass::kRanger, 9}}, Species2024::kOther).expertise, 3);
  const SkillAllowance wizard = SkillAllowanceFor({{CharacterClass::kWizard, 2}}, Species2024::kOther);
  CHECK_EQ(wizard.expertise, 1);
  CHECK_EQ(Has(wizard.expertise_list, Skill2024::kArcana), true);
  CHECK_EQ(Has(wizard.expertise_list, Skill2024::kStealth), false);
  const SkillAllowance multi =
      SkillAllowanceFor({{CharacterClass::kFighter, 4}, {CharacterClass::kRogue, 1}}, Species2024::kOther);
  CHECK_EQ(multi.multiclass_skills, 1);
  CHECK_EQ(Has(multi.granted, Skill2024::kThievesTools), true);
}

// Breaks caught: too many picks accepted; picks outside the class list not
// placed in the background slots; a tool counted as a skill.
TEST(SkillChoicesAreChecked) {
  const SkillAllowance fighter = SkillAllowanceFor({{CharacterClass::kFighter, 1}}, Species2024::kOther);
  SkillSet ok = SkillBit(Skill2024::kAthletics) | SkillBit(Skill2024::kPerception) |
                SkillBit(Skill2024::kStealth) | SkillBit(Skill2024::kArcana) | SkillBit(Skill2024::kSmithsTools);
  CHECK_EQ(CheckSkillChoice(fighter, ok).empty(), true);  // Stealth, Arcana from the background
  SkillSet off_list = SkillBit(Skill2024::kStealth) | SkillBit(Skill2024::kArcana) |
                      SkillBit(Skill2024::kDeception);  // three non-Fighter skills, two background slots
  CHECK_EQ(CheckSkillChoice(fighter, off_list).empty(), false);
  SkillSet too_many = ok | SkillBit(Skill2024::kHistory);
  CHECK_EQ(CheckSkillChoice(fighter, too_many).empty(), false);
  SkillSet two_tools = SkillBit(Skill2024::kSmithsTools) | SkillBit(Skill2024::kThievesTools);
  CHECK_EQ(CheckSkillChoice(fighter, two_tools).empty(), false);
  const SkillAllowance rogue = SkillAllowanceFor({{CharacterClass::kRogue, 1}}, Species2024::kOther);
  CHECK_EQ(CheckSkillChoice(rogue, two_tools).empty(), true);  // Thieves' Tools is granted
  CHECK_EQ(CheckExpertiseChoice(rogue, SkillBit(Skill2024::kStealth), SkillBit(Skill2024::kStealth)).empty(), true);
  CHECK_EQ(CheckExpertiseChoice(rogue, SkillBit(Skill2024::kStealth), SkillBit(Skill2024::kArcana)).empty(), false);
}

// SK5. Breaks caught: a passive observer rolling (NWN's d10, or a d20) instead
// of passive Perception 10; a searching one losing its d20.
TEST(PassivePerceptionUnlessSearching) {
  CHECK_EQ(PerceptionDie(false, 0), 10);
  CHECK_EQ(PerceptionDie(false, 17), 10);
  CHECK_EQ(PerceptionDie(true, 3), 3);
  CHECK_EQ(PerceptionDie(true, 20), 20);
}

// SK3. Breaks caught: the save conversion's 10..30 clamp reused (a DC 1
// "always" check would need 10; a DC 100 lock would become pickable at 30);
// rounding that disagrees with C6; the immediate DCs patched in the engine
// (Pick Pocket 30 / 20, Tumble 15) drifting from the rule.
TEST(SkillDcsAre2024) {
  CHECK_EQ(ConvertNwnSkillDc(0), 0);
  CHECK_EQ(ConvertNwnSkillDc(1), 1);
  CHECK_EQ(ConvertNwnSkillDc(5), 8);
  CHECK_EQ(ConvertNwnSkillDc(10), 10);
  CHECK_EQ(ConvertNwnSkillDc(15), 13);  // Tumble (patched as 13)
  CHECK_EQ(ConvertNwnSkillDc(20), 15);  // Pick Pocket, hostile (15)
  CHECK_EQ(ConvertNwnSkillDc(25), 18);
  CHECK_EQ(ConvertNwnSkillDc(30), 20);  // Pick Pocket (20)
  CHECK_EQ(ConvertNwnSkillDc(40), 25);
  CHECK_EQ(ConvertNwnSkillDc(50), 30);
  CHECK_EQ(ConvertNwnSkillDc(100), 55);
}

// SK5 B. Breaks caught: a hider behind or beside the observer found (sneaking
// up from behind must work); one 3 m away found; the 2 m edge left out.
TEST(FoundOnlyInFrontUpClose) {
  CHECK_EQ(InFrontWithin(1.0f, 0.0f, 1.0f, 0.0f, 2.0f), true);    // ahead, 1 m
  CHECK_EQ(InFrontWithin(2.0f, 0.0f, 1.0f, 0.0f, 2.0f), true);    // ahead, exactly 2 m
  CHECK_EQ(InFrontWithin(3.0f, 0.0f, 1.0f, 0.0f, 2.0f), false);   // too far
  CHECK_EQ(InFrontWithin(-1.0f, 0.0f, 1.0f, 0.0f, 2.0f), false);  // behind
  CHECK_EQ(InFrontWithin(0.0f, 1.0f, 1.0f, 0.0f, 2.0f), false);   // beside
  CHECK_EQ(InFrontWithin(1.0f, 1.0f, 1.0f, 0.0f, 2.0f), true);    // ahead and to the side, 1.4 m
}

// SK9. Breaks caught: NWN's +4 for Improved Initiative kept instead of PB; PB
// added without the feat.
TEST(InitiativeIs2024) {
  CHECK_EQ(InitiativeModifier2024(2, 2, false), 2);
  CHECK_EQ(InitiativeModifier2024(2, 2, true), 4);
  CHECK_EQ(InitiativeModifier2024(-1, 5, true), 4);
}

// SK11. Breaks caught: a passive check with disadvantage left at 10 (2024: -5);
// advantage and disadvantage not cancelling; the wrong die kept.
TEST(ChecksWithConditions) {
  CHECK_EQ(PassiveCheckBase(false), 10);
  CHECK_EQ(PassiveCheckBase(true), 5);
  CHECK_EQ(KeptCheckD20(7, 15, 0, 1), 7);   // disadvantage: the lower
  CHECK_EQ(KeptCheckD20(7, 15, 1, 0), 15);  // advantage: the higher
  CHECK_EQ(KeptCheckD20(7, 15, 1, 1), 7);   // both: they cancel, the first die
  CHECK_EQ(KeptCheckD20(12, 0, 0, 1), 12);  // one die rolled
}

// Breaks caught: the Skilled feat not adding its three picks, its picks
// limited to a class list (Skilled takes any skill), too many still accepted.
TEST(SkilledAddsThreeSkills) {
  const SkillAllowance fighter = SkillAllowanceFor({{CharacterClass::kFighter, 1}}, Species2024::kOther, 1);
  CHECK_EQ(fighter.feat_skills, 3);
  CHECK_EQ(fighter.TotalSkills(), 2 + 2 + 3);
  SkillSet seven = SkillBit(Skill2024::kAthletics) | SkillBit(Skill2024::kPerception) |  // class
                   SkillBit(Skill2024::kStealth) | SkillBit(Skill2024::kArcana) |       // background
                   SkillBit(Skill2024::kDeception) | SkillBit(Skill2024::kReligion) |   // Skilled
                   SkillBit(Skill2024::kMedicine);
  CHECK_EQ(CheckSkillChoice(fighter, seven).empty(), true);
  CHECK_EQ(CheckSkillChoice(fighter, seven | SkillBit(Skill2024::kHistory)).empty(), false);
  CHECK_EQ(SkillAllowanceFor({{CharacterClass::kFighter, 1}}, Species2024::kOther).feat_skills, 0);
}

// C5. Breaks caught: a 2024 skill with no row on the Skills page, or one row
// standing for two skills; nwn5e's new rows not mapped back to their skills;
// Animal Handling on Animal Empathy (a row a Fighter can't use).
TEST(SkillsPageShowsOneRowPerSkill) {
  int skills = 0;
  for (int i = 0; i < kSkill2024Count; ++i) {
    const Skill2024 skill = static_cast<Skill2024>(i);
    const int row = SkillsPageRow(skill);
    if (IsTool(skill)) {
      CHECK_EQ(row, -1);
      continue;
    }
    CHECK_EQ(Skill2024ForNwnSkill(row) == skill, true);
    CHECK_EQ(IsSkillsPageRow(row), true);
    ++skills;
  }
  CHECK_EQ(skills, 18);
  int page_rows = 0;
  for (int row = 0; row < kNwnSkillCount; ++row) page_rows += IsSkillsPageRow(row) ? 1 : 0;
  CHECK_EQ(page_rows, 18);
  CHECK_EQ(SkillsPageRow(Skill2024::kAnimalHandling), static_cast<int>(kNwnRide));
  CHECK_EQ(Skill2024ForNwnSkill(kNwnSurvival) == Skill2024::kSurvival, true);
  CHECK_EQ(Skill2024ForNwnSkill(kNwnAthletics) == Skill2024::kAthletics, true);
  CHECK_EQ(IsSkillsPageRow(kNwnMoveSilently), false);
}

// C5. Breaks caught: a skill off the class's list taken with a class pick; a
// Human's any-skill pick missing or counted twice; an Elf's Keen Senses
// outside its three; a background skill picked again; picks past the points.
TEST(SkillsPagePicksAre2024) {
  const SkillAllowance fighter = SkillsPageAllowance(CharacterClass::kFighter, Species2024::kOther);
  CHECK_EQ(SkillsPagePoints(fighter), 2);
  CHECK_EQ(CanPickOnSkillsPage(fighter, 0, 0, Skill2024::kStealth), false);
  CHECK_EQ(CanPickOnSkillsPage(fighter, 0, 0, Skill2024::kPerception), true);
  const SkillSet two = SkillBit(Skill2024::kPerception) | SkillBit(Skill2024::kAthletics);
  CHECK_EQ(CanPickOnSkillsPage(fighter, 0, two, Skill2024::kIntimidation), false);  // no points left
  const SkillAllowance human = SkillsPageAllowance(CharacterClass::kFighter, Species2024::kHuman);
  CHECK_EQ(SkillsPagePoints(human), 3);
  CHECK_EQ(CanPickOnSkillsPage(human, 0, two, Skill2024::kStealth), true);  // Skillful: any skill
  CHECK_EQ(CanPickOnSkillsPage(human, 0, SkillBit(Skill2024::kStealth), Skill2024::kArcana), false);
  const SkillSet soldier = SkillBit(Skill2024::kAthletics) | SkillBit(Skill2024::kIntimidation);
  CHECK_EQ(CanPickOnSkillsPage(human, soldier, 0, Skill2024::kAthletics), false);  // the background's already
  const SkillAllowance elf = SkillsPageAllowance(CharacterClass::kWizard, Species2024::kElf);
  CHECK_EQ(SkillsPagePoints(elf), 3);
  CHECK_EQ(CanPickOnSkillsPage(elf, 0, 0, Skill2024::kSurvival), true);  // Keen Senses
  CHECK_EQ(CanPickOnSkillsPage(elf, 0, SkillBit(Skill2024::kSurvival), Skill2024::kStealth), false);
  CHECK_EQ(CanPickOnSkillsPage(elf, 0, 0, Skill2024::kThievesTools), false);
}

// C5, every class x species x background. Breaks caught: Recommended taking
// fewer picks than the page has points, a background's skill, a set the 2024
// choices can't hold, or Performance for a class that can't use Perform.
TEST(SkillsPageRecommendedFitsEveryone) {
  std::vector<SkillSet> backgrounds = {0};
  for (const BackgroundInfo& info : Backgrounds2024()) {
    backgrounds.push_back(info.skills & ~(SkillBit(Skill2024::kThievesTools) | SkillBit(Skill2024::kSmithsTools)));
  }
  int cases = 0;
  for (int c = 0; c <= static_cast<int>(CharacterClass::kWizard); ++c) {
    const CharacterClass character_class = static_cast<CharacterClass>(c);
    for (Species2024 species : {Species2024::kOther, Species2024::kHuman, Species2024::kElf}) {
      for (SkillSet background : backgrounds) {
        const SkillAllowance page = SkillsPageAllowance(character_class, species);
        const SkillSet picks = RecommendedSkillsPagePicks(character_class, species, background);
        CHECK_EQ(CountSkills(picks), SkillsPagePoints(page));
        CHECK_EQ((picks & background) == 0u, true);
        CHECK_EQ(CheckSkillChoice(page, picks).empty(), true);
        CHECK_EQ(!Has(picks, Skill2024::kPerformance) || character_class == CharacterClass::kBard, true);
        ++cases;
      }
    }
  }
  CHECK_EQ(cases, 12 * 3 * 5);
}
