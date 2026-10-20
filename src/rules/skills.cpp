#include "rules/skills.h"

#include <ctype.h>

#include <initializer_list>

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
  return class_skills + multiclass_skills + background_skills + (species_any_skill || species_list ? 1 : 0);
}

SkillAllowance SkillAllowanceFor(const std::vector<ClassLevels>& classes, Species2024 species) {
  SkillAllowance allowance;
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

}  // namespace rules
