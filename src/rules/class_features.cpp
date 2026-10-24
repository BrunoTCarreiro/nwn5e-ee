#include "rules/class_features.h"

namespace rules {

namespace {

// SRD 5.2 Fighter and Champion (PLAN.md "Phase 3 — Fighter"); statuses as
// of the brief, updated as its decisions are built.
const std::vector<ClassFeature>& Fighter() {
  static const std::vector<ClassFeature> kFeatures = {
      {1, "Fighting Style", false, "built: Archery, Defense, Great Weapon Fighting, Two-Weapon Fighting"},
      {1, "Second Wind", false, "built: radial menu (an action until Phase 6), 2 / 3 / 4 uses per rest"},
      {1, "Weapon Mastery", false, "built (!mastery)"},
      {2, "Action Surge", false, "Phase 6"},
      {2, "Tactical Mind", false, "Phase 7 (a choice)"},
      {3, "Champion", false, "built: granted at 3"},
      {3, "Improved Critical", true, "built: crits on 19-20"},
      {3, "Remarkable Athlete", true, "built: advantage on initiative (the move after a crit: Phase 6)"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Extra Attack", false, "built"},
      {5, "Tactical Shift", false, "built: no opportunity attacks the round you use Second Wind"},
      {6, "Ability Score Improvement", false, "built (!asi)"},
      {7, "Additional Fighting Style", true, "built: a second style"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Indomitable", false, "built: a failed save is rerolled with + Fighter level (automatic), 1 use per long rest"},
      {9, "Tactical Master", false, "later (a per-attack choice)"},
      {10, "Heroic Warrior", true, "with Heroic Inspiration (CM6)"},
      {11, "Two Extra Attacks", false, "built"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Indomitable (2 uses)", false, "built"},
      {13, "Studied Attacks", false, "built"},
      {14, "Ability Score Improvement", false, "built (!asi)"},
      {15, "Superior Critical", true, "built: crits on 18-20"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Action Surge (2 uses)", false, "Phase 6"},
      {17, "Indomitable (3 uses)", false, "built"},
      {18, "Survivor", true, "built: Heroic Rally (Defy Death: Phase 5)"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Three Extra Attacks", false, "built"},
  };
  return kFeatures;
}

// SRD 5.2 Barbarian and Path of the Berserker (PLAN.md "Phase 3 — Barbarian").
const std::vector<ClassFeature>& Barbarian() {
  static const std::vector<ClassFeature> kFeatures = {
      {1, "Rage", false, "next (Rage): NWN's Rage until then"},
      {1, "Unarmored Defense", false, "built: 10 + Dex + Con with no armour (a shield allowed)"},
      {1, "Weapon Mastery", false, "built (!mastery): melee weapons"},
      {2, "Danger Sense", false, "built: advantage on Dexterity saves unless incapacitated"},
      {2, "Reckless Attack", false, "later: a radial toggle"},
      {3, "Path of the Berserker", false, "built: granted at 3"},
      {3, "Primal Knowledge", false, "built: a skill; Strength for some checks while raging with Rage"},
      {3, "Frenzy", true, "later: with Reckless Attack"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Extra Attack", false, "built"},
      {5, "Fast Movement", false, "built: +10 ft (a third faster) out of heavy armour"},
      {6, "Mindless Rage", true, "next: with Rage"},
      {7, "Feral Instinct", false, "built: advantage on initiative"},
      {7, "Instinctive Pounce", false, "next: with Rage (a charge: half again as fast for a round)"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Brutal Strike", false, "later: effects picked on the radial"},
      {10, "Retaliation", true, "later"},
      {11, "Relentless Rage", false, "next: with Rage"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Improved Brutal Strike", false, "later"},
      {14, "Intimidating Presence", true, "later"},
      {15, "Persistent Rage", false, "next: with Rage"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Improved Brutal Strike (2d10, two effects)", false, "later"},
      {18, "Indomitable Might", false, "next: with Rage"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Primal Champion", false, "built: Strength and Constitution +4 (at most 25)"},
  };
  return kFeatures;
}

}  // namespace

const char* Subclass2024(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return "Path of the Berserker";
    case CharacterClass::kBard: return "College of Lore";
    case CharacterClass::kCleric: return "Life Domain";
    case CharacterClass::kDruid: return "Circle of the Land";
    case CharacterClass::kFighter: return "Champion";
    case CharacterClass::kMonk: return "Warrior of the Open Hand";
    case CharacterClass::kPaladin: return "Oath of Devotion";
    case CharacterClass::kRanger: return "Hunter";
    case CharacterClass::kRogue: return "Thief";
    case CharacterClass::kSorcerer: return "Draconic Sorcery";
    case CharacterClass::kWarlock: return "Fiend Patron";
    case CharacterClass::kWizard: return "Evoker";
  }
  return "";
}

bool HasFeatureList(CharacterClass character_class) {
  return character_class == CharacterClass::kFighter || character_class == CharacterClass::kBarbarian;
}

bool HasDangerSense(int barbarian_level) { return barbarian_level >= 2; }

bool HasBarbarianFastMovement(int barbarian_level) { return barbarian_level >= 5; }

bool HasFeralInstinct(int barbarian_level) { return barbarian_level >= 7; }

bool HasPrimalChampion(int barbarian_level) { return barbarian_level >= 20; }

int UnarmoredDefenseConBonus(int con_modifier, int monk_wis_bonus) {
  const int bonus = con_modifier - (monk_wis_bonus > 0 ? monk_wis_bonus : 0);
  return bonus > 0 ? bonus : 0;
}

const std::vector<int>& NwnOnlyClassFeats() {
  static const std::vector<int> kFeats = {
      195, 251, 252, 253, 254, 255,  // Uncanny Dodge 1-6 (Barbarian, Rogue)
      196, 332, 333, 334, 1067,      // Barbarian Damage Reduction 1-4, epic
  };
  return kFeats;
}

int ChampionCriticalFloor(int fighter_level) { return fighter_level >= 15 ? 18 : fighter_level >= 3 ? 19 : 20; }

bool HasRemarkableAthlete(int fighter_level) { return fighter_level >= 3; }

bool HasStudiedAttacks(int fighter_level) { return fighter_level >= 13; }

std::vector<int> GrantedFeats2024(const std::vector<ClassLevels>& classes) {
  std::vector<int> feats;
  for (const ClassLevels& c : classes) {
    if (c.character_class != CharacterClass::kFighter || c.levels < 1) continue;
    feats.push_back(c.levels >= 10 ? kFeatSecondWind4 : c.levels >= 4 ? kFeatSecondWind3 : kFeatSecondWind2);
  }
  return feats;
}

const std::vector<int>& GrantableFeats2024() {
  static const std::vector<int> kFeats = {kFeatSecondWind2, kFeatSecondWind3, kFeatSecondWind4};
  return kFeats;
}

int IndomitableUses(int fighter_level) { return fighter_level >= 17 ? 3 : fighter_level >= 13 ? 2 : fighter_level >= 9 ? 1 : 0; }

bool IsBloodied(int hit_points, int max_hit_points) { return max_hit_points > 0 && hit_points * 2 <= max_hit_points; }

int HeroicRallyHitPoints(int con_modifier) { return 5 + con_modifier; }

bool HasHeroicRally(int fighter_level) { return fighter_level >= 18; }

int GreatWeaponFightingDie(int roll) { return roll < 3 ? 3 : roll; }

bool IsTwoHandedWeapon2024(int base_item) {
  switch (base_item) {
    case 10: case 13: case 18: case 35: case 55: return true;  // halberd, greatsword, greataxe, heavy flail, scythe
    default: return false;
  }
}

bool IsVersatileWeapon2024(int base_item) {
  switch (base_item) {
    case 1: case 2: case 5: case 50: case 58: case 95:  // longsword, battleaxe, warhammer, quarterstaff, spear, trident
    case 3: case 41: case 45: case 108:                 // bastard sword, katana, magic staff, dwarven waraxe
      return true;
    default: return false;
  }
}

bool IsFightingStyleLevel2024(CharacterClass character_class, int class_level) {
  return character_class == CharacterClass::kFighter && (class_level == 1 || class_level == 7);
}

std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level) {
  std::vector<ClassFeature> features;
  const std::vector<ClassFeature>* table = character_class == CharacterClass::kFighter     ? &Fighter()
                                           : character_class == CharacterClass::kBarbarian ? &Barbarian()
                                                                                           : nullptr;
  if (!table) return features;
  for (const ClassFeature& feature : *table) {
    if (feature.level <= level) features.push_back(feature);
  }
  return features;
}

}  // namespace rules
