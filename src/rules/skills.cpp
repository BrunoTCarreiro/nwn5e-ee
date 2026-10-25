#include "rules/skills.h"

#include <ctype.h>
#include <math.h>

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace rules {

namespace {

struct SkillInfo {
  const char* name;
  Ability ability;
  bool tool;
};

constexpr SkillInfo kSkills[kSkill2024Count] = {
    {"Acrobatics", Ability::kDexterity, false},     {"Animal Handling", Ability::kWisdom, false},
    {"Arcana", Ability::kIntelligence, false},      {"Athletics", Ability::kStrength, false},
    {"Deception", Ability::kCharisma, false},       {"History", Ability::kIntelligence, false},
    {"Insight", Ability::kWisdom, false},           {"Intimidation", Ability::kCharisma, false},
    {"Investigation", Ability::kIntelligence, false}, {"Medicine", Ability::kWisdom, false},
    {"Nature", Ability::kIntelligence, false},      {"Perception", Ability::kWisdom, false},
    {"Performance", Ability::kCharisma, false},     {"Persuasion", Ability::kCharisma, false},
    {"Religion", Ability::kIntelligence, false},    {"Sleight of Hand", Ability::kDexterity, false},
    {"Stealth", Ability::kDexterity, false},        {"Survival", Ability::kWisdom, false},
    {"Thieves' Tools", Ability::kDexterity, true},  {"Smith's Tools", Ability::kStrength, true},
};

struct NwnRow {
  int nwn_skill;
  std::optional<Skill2024> skill;
  const char* label;   // faithful name
  uint32_t strref;     // skills.2da Name
};

const NwnRow kNwnRows[kNwnSkillCount] = {
    {kNwnAnimalEmpathy, Skill2024::kAnimalHandling, "Animal Handling", 269},
    {kNwnConcentration, std::nullopt, nullptr, 270},
    {kNwnDisableTrap, Skill2024::kThievesTools, "Thieves' Tools (Disable Trap)", 271},
    {kNwnDiscipline, std::nullopt, nullptr, 343},
    {kNwnHeal, Skill2024::kMedicine, "Medicine", 273},
    {kNwnHide, Skill2024::kStealth, "Stealth (Hide)", 274},
    {kNwnListen, Skill2024::kPerception, "Perception (Listen)", 275},
    {kNwnLore, Skill2024::kArcana, "Arcana (Lore)", 276},
    {kNwnMoveSilently, Skill2024::kStealth, "Stealth (Move Silently)", 277},
    {kNwnOpenLock, Skill2024::kThievesTools, "Thieves' Tools (Open Lock)", 278},
    {kNwnParry, std::nullopt, nullptr, 279},
    {kNwnPerform, Skill2024::kPerformance, "Performance", 280},
    {kNwnPersuade, Skill2024::kPersuasion, "Persuasion", 281},
    {kNwnPickPocket, Skill2024::kSleightOfHand, "Sleight of Hand", 282},
    {kNwnSearch, Skill2024::kPerception, "Perception (Search)", 284},
    {kNwnSetTrap, Skill2024::kThievesTools, "Thieves' Tools (Set Trap)", 285},
    {kNwnSpellcraft, Skill2024::kArcana, "Arcana (Spellcraft)", 286},
    {kNwnSpot, Skill2024::kPerception, "Perception (Spot)", 287},
    {kNwnTaunt, std::nullopt, nullptr, 342},
    {kNwnUseMagicDevice, std::nullopt, nullptr, 288},
    {kNwnAppraise, std::nullopt, nullptr, 2693},
    {kNwnTumble, Skill2024::kAcrobatics, "Acrobatics", 2695},
    {kNwnCraftTrap, std::nullopt, nullptr, 2697},
    {kNwnBluff, Skill2024::kDeception, "Deception", 8746},
    {kNwnIntimidate, Skill2024::kIntimidation, "Intimidation", 8756},
    {kNwnCraftArmor, Skill2024::kSmithsTools, "Smith's Tools (Armor)", 8787},
    {kNwnCraftWeapon, Skill2024::kSmithsTools, "Smith's Tools (Weapons)", 8789},
    {kNwnRide, Skill2024::kAnimalHandling, "Animal Handling (Ride)", 111889},
    // C5: nwn5e's rows, named by data/strings.txt.
    {kNwnAthletics, Skill2024::kAthletics, "Athletics", 16000396},
    {kNwnHistory, Skill2024::kHistory, "History", 16000398},
    {kNwnInsight, Skill2024::kInsight, "Insight", 16000400},
    {kNwnInvestigation, Skill2024::kInvestigation, "Investigation", 16000402},
    {kNwnNature, Skill2024::kNature, "Nature", 16000404},
    {kNwnReligion, Skill2024::kReligion, "Religion", 16000406},
    {kNwnSurvival, Skill2024::kSurvival, "Survival", 16000408},
};

SkillSet Set(std::initializer_list<Skill2024> skills) {
  SkillSet set = 0;
  for (Skill2024 skill : skills) set |= SkillBit(skill);
  return set;
}

SkillSet AllSkills() {
  SkillSet set = 0;
  for (int i = 0; i < kSkill2024Count; ++i) {
    if (!kSkills[i].tool) set |= 1u << i;
  }
  return set;
}

using S = Skill2024;

// 2024 class skill choices (SRD 5.2): count and list.
struct ClassSkills {
  int count;
  SkillSet list;
};

ClassSkills ClassSkillsFor(CharacterClass c) {
  switch (c) {
    case CharacterClass::kBarbarian:
      return {2, Set({S::kAnimalHandling, S::kAthletics, S::kIntimidation, S::kNature, S::kPerception, S::kSurvival})};
    case CharacterClass::kBard:
      return {3, AllSkills()};
    case CharacterClass::kCleric:
      return {2, Set({S::kHistory, S::kInsight, S::kMedicine, S::kPersuasion, S::kReligion})};
    case CharacterClass::kDruid:
      return {2, Set({S::kArcana, S::kAnimalHandling, S::kInsight, S::kMedicine, S::kNature, S::kPerception,
                      S::kReligion, S::kSurvival})};
    case CharacterClass::kFighter:
      return {2, Set({S::kAcrobatics, S::kAnimalHandling, S::kAthletics, S::kHistory, S::kInsight, S::kIntimidation,
                      S::kPersuasion, S::kPerception, S::kSurvival})};
    case CharacterClass::kMonk:
      return {2, Set({S::kAcrobatics, S::kAthletics, S::kHistory, S::kInsight, S::kReligion, S::kStealth})};
    case CharacterClass::kPaladin:
      return {2, Set({S::kAthletics, S::kInsight, S::kIntimidation, S::kMedicine, S::kPersuasion, S::kReligion})};
    case CharacterClass::kRanger:
      return {3, Set({S::kAnimalHandling, S::kAthletics, S::kInsight, S::kInvestigation, S::kNature, S::kPerception,
                      S::kStealth, S::kSurvival})};
    case CharacterClass::kRogue:
      return {4, Set({S::kAcrobatics, S::kAthletics, S::kDeception, S::kInsight, S::kIntimidation, S::kInvestigation,
                      S::kPerception, S::kPersuasion, S::kSleightOfHand, S::kStealth})};
    case CharacterClass::kSorcerer:
      return {2, Set({S::kArcana, S::kDeception, S::kInsight, S::kIntimidation, S::kPersuasion, S::kReligion})};
    case CharacterClass::kWarlock:
      return {2, Set({S::kArcana, S::kDeception, S::kHistory, S::kIntimidation, S::kInvestigation, S::kNature,
                      S::kReligion})};
    case CharacterClass::kWizard:
      return {2, Set({S::kArcana, S::kHistory, S::kInsight, S::kInvestigation, S::kMedicine, S::kNature,
                      S::kReligion})};
  }
  return {0, 0};
}

std::string Normalize(const std::string& text) {
  std::string out;
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (isalnum(u)) out += static_cast<char>(tolower(u));
  }
  return out;
}

// Kuhn's augmenting path for the skill → slot matching.
bool Augment(int skill, const std::vector<SkillSet>& slots, const std::vector<int>& skills, std::vector<int>* slot_of,
             std::vector<bool>* seen) {
  for (size_t s = 0; s < slots.size(); ++s) {
    if (!(slots[s] & (1u << skills[skill])) || (*seen)[s]) continue;
    (*seen)[s] = true;
    if ((*slot_of)[s] < 0 || Augment((*slot_of)[s], slots, skills, slot_of, seen)) {
      (*slot_of)[s] = skill;
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsTool(Skill2024 skill) { return kSkills[static_cast<int>(skill)].tool; }
const char* Skill2024Name(Skill2024 skill) { return kSkills[static_cast<int>(skill)].name; }
Ability AbilityFor(Skill2024 skill) { return kSkills[static_cast<int>(skill)].ability; }

std::optional<Skill2024> Skill2024ForNwnSkill(int nwn_skill) {
  if (nwn_skill < 0 || nwn_skill >= kNwnSkillCount) return std::nullopt;
  return kNwnRows[nwn_skill].skill;
}

const char* Faithful2024SkillLabel(int nwn_skill) {
  if (nwn_skill < 0 || nwn_skill >= kNwnSkillCount) return nullptr;
  return kNwnRows[nwn_skill].label;
}

const char* Faithful2024SkillLabelForStrref(uint32_t strref) {
  for (const NwnRow& row : kNwnRows) {
    if (row.strref == strref) return row.label;
  }
  return nullptr;
}

std::optional<Skill2024> ParseSkill2024(const std::string& text) {
  const std::string key = Normalize(text);
  if (key.empty()) return std::nullopt;
  for (int i = 0; i < kSkill2024Count; ++i) {
    if (Normalize(kSkills[i].name) == key) return static_cast<Skill2024>(i);
  }
  struct Alias {
    const char* word;
    Skill2024 skill;
  };
  static const Alias kAliases[] = {
      {"sleight", S::kSleightOfHand}, {"animal", S::kAnimalHandling}, {"thieves", S::kThievesTools},
      {"thievestool", S::kThievesTools}, {"smiths", S::kSmithsTools}, {"smith", S::kSmithsTools},
      {"smithstool", S::kSmithsTools},
  };
  for (const Alias& alias : kAliases) {
    if (key == alias.word) return alias.skill;
  }
  return std::nullopt;
}

bool ParseSkill2024List(const std::string& text, std::vector<Skill2024>* skills, std::string* unknown) {
  std::vector<std::string> words;
  std::string word;
  for (char c : text + " ") {
    if (c == ' ' || c == ',' || c == ';') {
      if (!word.empty()) words.push_back(word);
      word.clear();
    } else {
      word += c;
    }
  }
  for (size_t i = 0; i < words.size();) {
    bool matched = false;
    for (size_t n = 3; n >= 1 && !matched; --n) {  // "sleight of hand", "thieves' tools"
      if (i + n > words.size()) continue;
      std::string joined;
      for (size_t k = 0; k < n; ++k) joined += words[i + k];
      if (const auto skill = ParseSkill2024(joined)) {
        skills->push_back(*skill);
        i += n;
        matched = true;
      }
    }
    if (!matched) {
      *unknown = words[i];
      return false;
    }
  }
  return true;
}

int SkillBonus2024(int ability_modifier, int proficiency_bonus, bool proficient, bool expertise, int effect_bonus) {
  int bonus = ability_modifier + effect_bonus;
  if (proficient) bonus += proficiency_bonus;
  if (proficient && expertise) bonus += proficiency_bonus;
  return bonus;
}

int ScriptSkillValue2024(int skill_bonus) { return skill_bonus > 0 ? 2 * skill_bonus : skill_bonus; }

int ConvertNwnSkillDc(int dc) {
  if (dc <= 1) return dc;
  return 10 + (dc >= 10 ? (dc - 10 + 1) / 2 : -((10 - dc) / 2));
}

int CountSkills(SkillSet set) {
  int count = 0;
  for (; set; set &= set - 1) ++count;
  return count;
}

int SkillAllowance::TotalSkills() const {
  return class_skills + multiclass_skills + background_skills + (species_any_skill || species_list ? 1 : 0) +
         feat_skills + lore_skills + primal_skills;
}

SkillAllowance SkillAllowanceFor(const std::vector<ClassLevels>& classes, Species2024 species,
                                 int skilled_feats) {
  SkillAllowance allowance;
  allowance.feat_skills = 3 * skilled_feats;
  if (species == Species2024::kHuman) allowance.species_any_skill = true;
  if (species == Species2024::kElf) allowance.species_list = Set({S::kInsight, S::kPerception, S::kSurvival});
  bool any_expertise = false;
  bool wizard_expertise = false;
  for (size_t i = 0; i < classes.size(); ++i) {
    const CharacterClass c = classes[i].character_class;
    const int level = classes[i].levels;
    const ClassSkills skills = ClassSkillsFor(c);
    if (i == 0) {
      allowance.class_skills = skills.count;
      allowance.class_list = skills.list;
    } else if (c == CharacterClass::kBard || c == CharacterClass::kRanger || c == CharacterClass::kRogue) {
      allowance.multiclass_skills += 1;  // 2024 multiclassing: one skill from the class's list
      allowance.multiclass_list |= skills.list;
    }
    if (c == CharacterClass::kRogue) allowance.granted |= SkillBit(S::kThievesTools);
    if (c == CharacterClass::kBard && level >= 3) allowance.lore_skills += 3;  // C7b: College of Lore
    if (c == CharacterClass::kBarbarian && level >= 3) {                     // C7b: Primal Knowledge
      allowance.primal_skills += 1;
      allowance.primal_list |= skills.list;
    }
    switch (c) {
      case CharacterClass::kRogue:
        allowance.expertise += (level >= 1 ? 2 : 0) + (level >= 6 ? 2 : 0);
        any_expertise = any_expertise || level >= 1;
        break;
      case CharacterClass::kBard:
        allowance.expertise += (level >= 2 ? 2 : 0) + (level >= 9 ? 2 : 0);
        any_expertise = any_expertise || level >= 2;
        break;
      case CharacterClass::kRanger:
        allowance.expertise += (level >= 2 ? 1 : 0) + (level >= 9 ? 2 : 0);
        any_expertise = any_expertise || level >= 2;
        break;
      case CharacterClass::kWizard:
        if (level >= 2) {
          allowance.expertise += 1;  // Scholar
          wizard_expertise = true;
        }
        break;
      default:
        break;
    }
  }
  if (wizard_expertise && !any_expertise) {
    allowance.expertise_list = Set({S::kArcana, S::kHistory, S::kInvestigation, S::kMedicine, S::kNature,
                                    S::kReligion});
  }
  return allowance;
}

std::string CheckSkillChoice(const SkillAllowance& allowance, SkillSet chosen) {
  chosen &= ~allowance.granted;
  int tools = 0;
  std::vector<int> skills;
  for (int i = 0; i < kSkill2024Count; ++i) {
    if (!(chosen & (1u << i))) continue;
    if (kSkills[i].tool) {
      ++tools;
    } else {
      skills.push_back(i);
    }
  }
  if (tools > allowance.background_tools) {
    return "too many tools: " + std::to_string(tools) + " chosen, " + std::to_string(allowance.background_tools) +
           " allowed";
  }
  if (static_cast<int>(skills.size()) > allowance.TotalSkills()) {
    return "too many skills: " + std::to_string(skills.size()) + " chosen, " +
           std::to_string(allowance.TotalSkills()) + " allowed";
  }
  std::vector<SkillSet> slots;
  for (int i = 0; i < allowance.class_skills; ++i) slots.push_back(allowance.class_list);
  for (int i = 0; i < allowance.multiclass_skills; ++i) slots.push_back(allowance.multiclass_list);
  for (int i = 0; i < allowance.background_skills; ++i) slots.push_back(AllSkills());
  if (allowance.species_any_skill) slots.push_back(AllSkills());
  if (allowance.species_list) slots.push_back(allowance.species_list);
  for (int i = 0; i < allowance.feat_skills; ++i) slots.push_back(AllSkills());
  for (int i = 0; i < allowance.lore_skills; ++i) slots.push_back(AllSkills());
  for (int i = 0; i < allowance.primal_skills; ++i) slots.push_back(allowance.primal_list);
  std::vector<int> slot_of(slots.size(), -1);
  std::string unplaced;
  for (size_t k = 0; k < skills.size(); ++k) {
    std::vector<bool> seen(slots.size(), false);
    if (!Augment(static_cast<int>(k), slots, skills, &slot_of, &seen)) {
      if (!unplaced.empty()) unplaced += ", ";
      unplaced += kSkills[skills[k]].name;
    }
  }
  if (!unplaced.empty()) return "not from your class's list (background and species picks are used up): " + unplaced;
  return std::string();
}

int SkillsPageRow(Skill2024 skill) {
  switch (skill) {
    case S::kAcrobatics: return kNwnTumble;
    case S::kAnimalHandling: return kNwnRide;  // Animal Empathy is only a Druid's, Ranger's (AllClassesCanUse 0)
    case S::kArcana: return kNwnLore;
    case S::kAthletics: return kNwnAthletics;
    case S::kDeception: return kNwnBluff;
    case S::kHistory: return kNwnHistory;
    case S::kInsight: return kNwnInsight;
    case S::kIntimidation: return kNwnIntimidate;
    case S::kInvestigation: return kNwnInvestigation;
    case S::kMedicine: return kNwnHeal;
    case S::kNature: return kNwnNature;
    case S::kPerception: return kNwnSpot;
    case S::kPerformance: return kNwnPerform;
    case S::kPersuasion: return kNwnPersuade;
    case S::kReligion: return kNwnReligion;
    case S::kSleightOfHand: return kNwnPickPocket;
    case S::kStealth: return kNwnHide;
    case S::kSurvival: return kNwnSurvival;
    default: return -1;  // the tools
  }
}

bool IsSkillsPageRow(int nwn_skill) {
  const std::optional<Skill2024> skill = Skill2024ForNwnSkill(nwn_skill);
  return skill && SkillsPageRow(*skill) == nwn_skill;
}

SkillAllowance SkillsPageAllowance(CharacterClass first_class, Species2024 species) {
  SkillAllowance page;
  const ClassSkills skills = ClassSkillsFor(first_class);
  page.class_skills = skills.count;
  page.class_list = skills.list;
  page.background_skills = 0;
  page.background_tools = 0;
  if (species == Species2024::kHuman) page.species_any_skill = true;
  if (species == Species2024::kElf) page.species_list = Set({S::kInsight, S::kPerception, S::kSurvival});
  return page;
}

int SkillsPagePoints(const SkillAllowance& page) { return page.TotalSkills(); }

std::vector<Skill2024> FillSkillChoices(const SkillAllowance& allowance, SkillSet proficient,
                                        const std::vector<Skill2024>& preference, int most) {
  std::vector<Skill2024> added;
  auto take = [&](Skill2024 skill) {
    if (static_cast<int>(added.size()) >= most || IsTool(skill) || Has(proficient, skill)) return;
    if (!CheckSkillChoice(allowance, proficient | SkillBit(skill)).empty()) return;
    proficient |= SkillBit(skill);
    added.push_back(skill);
  };
  for (Skill2024 skill : preference) take(skill);
  for (int i = 0; i < kSkill2024Count; ++i) take(static_cast<Skill2024>(i));
  return added;
}

bool CanPickOnSkillsPage(const SkillAllowance& page, SkillSet background, SkillSet picked, Skill2024 skill) {
  if (IsTool(skill)) return false;
  const SkillSet bit = SkillBit(skill);
  if ((background | picked) & bit) return false;
  if (CountSkills(picked) + 1 > SkillsPagePoints(page)) return false;
  return CheckSkillChoice(page, picked | bit).empty();
}

std::vector<Skill2024> SkillPreference2024(CharacterClass first_class) {
  static const std::vector<Skill2024> kCommon = {
      S::kPerception, S::kInsight, S::kStealth, S::kPersuasion, S::kAthletics, S::kAcrobatics, S::kSurvival,
      S::kMedicine, S::kArcana, S::kIntimidation, S::kHistory, S::kInvestigation, S::kNature, S::kReligion,
      S::kDeception, S::kSleightOfHand, S::kAnimalHandling};
  std::vector<Skill2024> order;
  switch (first_class) {
    case CharacterClass::kBarbarian:
      order = {S::kPerception, S::kAthletics, S::kIntimidation, S::kSurvival, S::kAnimalHandling, S::kNature};
      break;
    case CharacterClass::kBard:
      order = {S::kPerformance, S::kPersuasion, S::kDeception, S::kInsight, S::kPerception};
      break;
    case CharacterClass::kCleric:
      order = {S::kInsight, S::kMedicine, S::kReligion, S::kPersuasion, S::kHistory};
      break;
    case CharacterClass::kDruid:
      order = {S::kPerception, S::kNature, S::kAnimalHandling, S::kMedicine, S::kInsight, S::kSurvival,
               S::kArcana, S::kReligion};
      break;
    case CharacterClass::kFighter:
      order = {S::kPerception, S::kAthletics, S::kIntimidation, S::kAcrobatics, S::kSurvival, S::kInsight,
               S::kPersuasion, S::kHistory, S::kAnimalHandling};
      break;
    case CharacterClass::kMonk:
      order = {S::kAcrobatics, S::kStealth, S::kInsight, S::kAthletics, S::kHistory, S::kReligion};
      break;
    case CharacterClass::kPaladin:
      order = {S::kPersuasion, S::kAthletics, S::kInsight, S::kIntimidation, S::kMedicine, S::kReligion};
      break;
    case CharacterClass::kRanger:
      order = {S::kPerception, S::kStealth, S::kSurvival, S::kNature, S::kAnimalHandling, S::kAthletics,
               S::kInsight, S::kInvestigation};
      break;
    case CharacterClass::kRogue:
      order = {S::kStealth, S::kPerception, S::kSleightOfHand, S::kAcrobatics, S::kDeception, S::kInvestigation,
               S::kInsight, S::kPersuasion, S::kIntimidation, S::kAthletics};
      break;
    case CharacterClass::kSorcerer:
      order = {S::kArcana, S::kPersuasion, S::kDeception, S::kInsight, S::kIntimidation, S::kReligion};
      break;
    case CharacterClass::kWarlock:
      order = {S::kArcana, S::kDeception, S::kIntimidation, S::kInvestigation, S::kHistory, S::kNature,
               S::kReligion};
      break;
    case CharacterClass::kWizard:
      order = {S::kArcana, S::kInvestigation, S::kHistory, S::kInsight, S::kMedicine, S::kNature, S::kReligion};
      break;
  }
  std::vector<Skill2024> preference = order;
  for (Skill2024 skill : kCommon) {
    if (std::find(preference.begin(), preference.end(), skill) == preference.end()) preference.push_back(skill);
  }
  return preference;
}

SkillSet RecommendedSkillsPagePicks(CharacterClass first_class, Species2024 species, SkillSet background) {
  const SkillAllowance page = SkillsPageAllowance(first_class, species);
  SkillSet picked = 0;
  for (Skill2024 skill : SkillPreference2024(first_class)) {
    if (CanPickOnSkillsPage(page, background, picked, skill)) picked |= SkillBit(skill);
  }
  return picked;
}

LevelSkillPicks LevelSkillPicks2024(CharacterClass c, int class_level, bool first_class) {
  LevelSkillPicks picks;
  const ClassSkills skills = ClassSkillsFor(c);
  if (class_level == 1 && first_class) {
    picks.skills = skills.count;
    picks.skill_list = skills.list;
  } else if (class_level == 1 && (c == CharacterClass::kBard || c == CharacterClass::kRanger || c == CharacterClass::kRogue)) {
    picks.skills = 1;  // 2024 multiclassing: one skill of the class's list (a Bard's: any)
    picks.skill_list = skills.list;
  }
  if (c == CharacterClass::kBarbarian && class_level == 3) {
    picks.skills += 1;
    picks.skill_list |= skills.list;
  }
  if (c == CharacterClass::kBard && class_level == 3) {
    picks.skills += 3;
    picks.skill_list = AllSkills();
  }
  switch (c) {
    case CharacterClass::kRogue:
      picks.expertise = class_level == 1 || class_level == 6 ? 2 : 0;
      break;
    case CharacterClass::kBard:
      picks.expertise = class_level == 2 || class_level == 9 ? 2 : 0;
      break;
    case CharacterClass::kRanger:
      picks.expertise = class_level == 2 ? 1 : class_level == 9 ? 2 : 0;
      break;
    case CharacterClass::kWizard:
      if (class_level == 2) {
        picks.expertise = 1;
        picks.expertise_list = Set({S::kArcana, S::kHistory, S::kInvestigation, S::kMedicine, S::kNature, S::kReligion});
      }
      break;
    default:
      break;
  }
  return picks;
}

bool CanPickExpertise(const LevelSkillPicks& picks, int taken, Skill2024 skill) {
  if (IsTool(skill) || taken >= picks.expertise) return false;
  return !picks.expertise_list || Has(picks.expertise_list, skill);
}

bool RanksAre2024(const int8_t* ranks, int rows) {
  for (int row = 0; ranks && row < rows; ++row) {
    if (ranks[row] == 0) continue;
    if (ranks[row] < 0 || ranks[row] > 2 || !IsSkillsPageRow(row)) return false;
  }
  return true;
}

std::string CheckExpertiseChoice(const SkillAllowance& allowance, SkillSet proficient, SkillSet expertise) {
  for (int i = 0; i < kSkill2024Count; ++i) {
    if (!(expertise & (1u << i))) continue;
    if (kSkills[i].tool) return std::string("Expertise is for skills, not tools: ") + kSkills[i].name;
    if (!(proficient & (1u << i))) return std::string("not proficient in ") + kSkills[i].name;
    if (allowance.expertise_list && !(allowance.expertise_list & (1u << i))) {
      return std::string("Scholar's Expertise is for Arcana, History, Investigation, Medicine, Nature or Religion: ") +
             kSkills[i].name;
    }
  }
  if (CountSkills(expertise) > allowance.expertise) {
    return "too many: " + std::to_string(CountSkills(expertise)) + " chosen, " + std::to_string(allowance.expertise) +
           " allowed";
  }
  return std::string();
}


int PerceptionDie(bool searching, int d20) { return searching ? d20 : 10; }

int PassiveCheckBase(bool disadvantage) { return disadvantage ? 5 : 10; }

int KeptCheckD20(int first, int second, int advantage, int disadvantage) {
  if (second == 0 || (advantage > 0) == (disadvantage > 0)) return first;
  return advantage > 0 ? (first > second ? first : second) : (first < second ? first : second);
}

int InitiativeModifier2024(int dexterity_modifier, int proficiency_bonus, bool alert) {
  return dexterity_modifier + (alert ? proficiency_bonus : 0);
}

bool InFrontWithin(float dx, float dy, float fx, float fy, float metres) {
  return dx * dx + dy * dy <= metres * metres && fx * dx + fy * dy > 0.0f;
}

int ReliableTalentD20(int d20, int rogue_level, bool proficient) {
  return rogue_level >= 7 && proficient && d20 < 10 ? 10 : d20;
}

float StealthOpacity(int light_level, int extra_bonus) {
  if (light_level < 0) return 0.0f;
  const float step = light_level >= 2 ? 0.9f : light_level == 1 ? 0.7f : 0.5f;
  const float opacity = step - 0.02f * static_cast<float>(extra_bonus > 0 ? extra_bonus : 0);
  return opacity < 0.3f ? 0.3f : opacity;
}

int CarriedLightLevel(int radius_m) {
  if (radius_m <= 0) return -1;
  return radius_m >= 10 ? 2 : 1;
}

int CarriedLightSpotBonus(int radius_m) { return radius_m > 0 ? (radius_m + 4) / 5 * 2 : 0; }

bool WithinVisionCone(float dx, float dy, float fx, float fy, float degrees) {
  if (degrees >= 360.0f) return true;
  if (degrees <= 0.0f) return false;
  const float d = sqrtf(dx * dx + dy * dy), f = sqrtf(fx * fx + fy * fy);
  if (d < 1e-4f || f < 1e-4f) return true;
  return (dx * fx + dy * fy) / (d * f) >= cosf(degrees * 0.5f * 3.14159265f / 180.0f);
}

}  // namespace rules
