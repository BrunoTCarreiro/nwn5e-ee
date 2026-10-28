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
      {1, "Rage", false, "built: resistance, Rage Damage, Strength advantage, no spells; uses by level"},
      {1, "Unarmored Defense", false, "built: 10 + Dex + Con with no armour (a shield allowed)"},
      {1, "Weapon Mastery", false, "built (!mastery): melee weapons"},
      {2, "Danger Sense", false, "built: advantage on Dexterity saves unless incapacitated"},
      {2, "Reckless Attack", false, "built: a radial toggle (henchmen: while raging)"},
      {3, "Path of the Berserker", false, "built: granted at 3"},
      {3, "Primal Knowledge", false, "built: a skill; Strength for some checks while raging"},
      {3, "Frenzy", true, "built: raging and reckless, the first hit each round +d6s"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Extra Attack", false, "built"},
      {5, "Fast Movement", false, "built: +10 ft (a third faster) out of heavy armour"},
      {6, "Mindless Rage", true, "built: immune to charm and fear while raging"},
      {7, "Feral Instinct", false, "built: advantage on initiative"},
      {7, "Instinctive Pounce", false, "built: a charge - half again as fast for a round on raging"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Brutal Strike", false, "built: once a round, advantage given up for +1d10 and an effect (radial)"},
      {10, "Retaliation", true, "later"},
      {11, "Relentless Rage", false, "built: at 0 HP while raging, a Con save (DC 10, +5 a use)"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Improved Brutal Strike", false, "built: Staggering and Sundering Blow"},
      {14, "Intimidating Presence", true, "later"},
      {15, "Persistent Rage", false, "built: uses back on initiative once; lasts 10 minutes"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Improved Brutal Strike (2d10, two effects)", false, "built"},
      {18, "Indomitable Might", false, "built: a failed Strength save at or under the score succeeds"},
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

// SRD 5.2 Rogue and Thief (PLAN.md "Phase 3 — Rogue", decided 2026-10-28).
const std::vector<ClassFeature>& Rogue() {
  static const std::vector<ClassFeature> kFeatures = {
      {1, "Expertise", false, "built (!skills)"},
      {1, "Sneak Attack", false, "built: Advantage, or an ally next to the target; Finesse or ranged; once a round; "
                                 "crits double the dice; no creature is immune"},
      {1, "Thieves' Cant", false, "listed (NWN has no languages)"},
      {1, "Weapon Mastery", false, "built (!mastery)"},
      {2, "Cunning Action", false, "Phase 6 (bonus actions)"},
      {3, "Thief", false, "built: granted at 3"},
      {3, "Steady Aim", false, "Phase 6 (bonus actions)"},
      {3, "Fast Hands", true, "later (Rogue part 6): locks and traps by the better of Thieves' Tools and Sleight of Hand"},
      {3, "Second-Story Work", true, "listed (NWN has no climbing or jumping)"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Cunning Strike", false, "later (Rogue part 5)"},
      {5, "Uncanny Dodge", false, "later (Rogue part 3)"},
      {6, "Expertise", false, "built (!skills)"},
      {7, "Evasion", false, "later (Rogue part 2)"},
      {7, "Reliable Talent", false, "later (Rogue part 4)"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Supreme Sneak", true, "later (an attacking-ends-hiding rule)"},
      {10, "Ability Score Improvement", false, "built (!asi)"},
      {11, "Improved Cunning Strike", false, "later (Rogue part 5)"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Use Magic Device", true, "later (Rogue part 6)"},
      {14, "Devious Strikes", false, "later (Rogue part 5)"},
      {15, "Slippery Mind", false, "later (Rogue part 2)"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Thief's Reflexes", true, "Phase 6"},
      {18, "Elusive", false, "later (Rogue part 2)"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Stroke of Luck", false, "later (Rogue part 4)"},
  };
  return kFeatures;
}

bool HasFeatureList(CharacterClass character_class) {
  return character_class == CharacterClass::kFighter || character_class == CharacterClass::kBarbarian ||
         character_class == CharacterClass::kRogue;
}

bool HasDangerSense(int barbarian_level) { return barbarian_level >= 2; }

bool HasBarbarianFastMovement(int barbarian_level) { return barbarian_level >= 5; }

bool HasFeralInstinct(int barbarian_level) { return barbarian_level >= 7; }

bool HasPrimalChampion(int barbarian_level) { return barbarian_level >= 20; }

bool IsRageFeat(int feat) { return feat == kFeatRage || (feat >= 326 && feat <= 331); }

int RageUses2024(int barbarian_level) {
  if (barbarian_level < 1) return 0;
  if (barbarian_level >= 17) return 6;
  if (barbarian_level >= 12) return 5;
  if (barbarian_level >= 6) return 4;
  if (barbarian_level >= 3) return 3;
  return 2;
}

int RageDamage2024(int barbarian_level) {
  if (barbarian_level < 1) return 0;
  if (barbarian_level >= 16) return 4;
  if (barbarian_level >= 9) return 3;
  return 2;
}

bool HasPrimalKnowledge(int barbarian_level) { return barbarian_level >= 3; }
bool HasMindlessRage(int barbarian_level) { return barbarian_level >= 6; }
bool HasInstinctivePounce(int barbarian_level) { return barbarian_level >= 7; }
bool HasRelentlessRage(int barbarian_level) { return barbarian_level >= 11; }
bool HasPersistentRage(int barbarian_level) { return barbarian_level >= 15; }

bool HasRetaliation(int barbarian_level) { return barbarian_level >= 10; }

bool HasIntimidatingPresence(int barbarian_level) { return barbarian_level >= 14; }

int IntimidatingPresenceDc(int strength_modifier, int proficiency_bonus) {
  return 8 + strength_modifier + proficiency_bonus;
}
bool HasIndomitableMight(int barbarian_level) { return barbarian_level >= 18; }

int RelentlessRageDc(int uses_since_rest) { return 10 + 5 * (uses_since_rest > 0 ? uses_since_rest : 0); }

int RelentlessRageHitPoints(int barbarian_level) { return 2 * barbarian_level; }

int IndomitableMightTotal(int total, int strength_score) { return total < strength_score ? strength_score : total; }

bool HasRecklessAttack(int barbarian_level) { return barbarian_level >= 2; }
bool HasFrenzy(int barbarian_level) { return barbarian_level >= 3; }
int FrenzyDice(int barbarian_level) { return HasFrenzy(barbarian_level) ? RageDamage2024(barbarian_level) : 0; }
bool HasBrutalStrike(int barbarian_level) { return barbarian_level >= 9; }
int BrutalStrikeDice(int barbarian_level) { return barbarian_level >= 17 ? 2 : HasBrutalStrike(barbarian_level) ? 1 : 0; }
int BrutalStrikeEffectCount(int barbarian_level) {
  return barbarian_level >= 17 ? 2 : HasBrutalStrike(barbarian_level) ? 1 : 0;
}

bool BrutalEffectAvailable(BrutalEffect effect, int barbarian_level) {
  switch (effect) {
    case BrutalEffect::kForceful:
    case BrutalEffect::kHamstring:
      return barbarian_level >= 9;
    case BrutalEffect::kStaggering:
    case BrutalEffect::kSundering:
      return barbarian_level >= 13;
    default:
      return false;
  }
}

const char* BrutalEffectName(BrutalEffect effect) {
  switch (effect) {
    case BrutalEffect::kForceful: return "Forceful Blow";
    case BrutalEffect::kHamstring: return "Hamstring Blow";
    case BrutalEffect::kStaggering: return "Staggering Blow";
    case BrutalEffect::kSundering: return "Sundering Blow";
    default: return "none";
  }
}

int EncodeBrutalChoice(const BrutalChoice& choice) {
  return 1 + static_cast<int>(choice.first) + 8 * static_cast<int>(choice.second);
}

BrutalChoice DecodeBrutalChoice(int stored) {
  BrutalChoice choice;
  if (stored <= 0) {  // never chosen: Hamstring Blow
    choice.first = BrutalEffect::kHamstring;
    return choice;
  }
  const int value = stored - 1;
  const int first = value % 8, second = value / 8;
  choice.first = first >= 1 && first <= 4 ? static_cast<BrutalEffect>(first) : BrutalEffect::kNone;
  choice.second = second >= 1 && second <= 4 ? static_cast<BrutalEffect>(second) : BrutalEffect::kNone;
  if (choice.first == BrutalEffect::kNone) std::swap(choice.first, choice.second);
  return choice;
}

BrutalPick PickBrutalEffect(BrutalChoice* choice, BrutalEffect picked, int barbarian_level) {
  if (picked == BrutalEffect::kNone) {
    *choice = BrutalChoice();
    return BrutalPick::kOff;
  }
  if (!BrutalEffectAvailable(picked, barbarian_level)) return BrutalPick::kLocked;
  if (choice->first == picked || choice->second == picked) {
    if (choice->first == picked) choice->first = choice->second;
    choice->second = BrutalEffect::kNone;
    return BrutalPick::kRemoved;
  }
  if (BrutalStrikeEffectCount(barbarian_level) < 2) {
    *choice = BrutalChoice();
    choice->first = picked;
  } else if (choice->first == BrutalEffect::kNone) {
    choice->first = picked;
  } else if (choice->second == BrutalEffect::kNone) {
    choice->second = picked;
  } else {
    choice->first = choice->second;  // the older one goes
    choice->second = picked;
  }
  return BrutalPick::kChosen;
}

std::vector<BrutalEffect> ActiveBrutalEffects(const BrutalChoice& choice, int barbarian_level) {
  std::vector<BrutalEffect> active;
  for (BrutalEffect effect : {choice.second, choice.first}) {  // the newer first: kept when only one fits
    if (effect != BrutalEffect::kNone && BrutalEffectAvailable(effect, barbarian_level) &&
        static_cast<int>(active.size()) < BrutalStrikeEffectCount(barbarian_level)) {
      active.push_back(effect);
    }
  }
  return active;
}

int UnarmoredDefenseConBonus(int con_modifier, int monk_wis_bonus) {
  const int bonus = con_modifier - (monk_wis_bonus > 0 ? monk_wis_bonus : 0);
  return bonus > 0 ? bonus : 0;
}

const std::vector<int>& NwnOnlyClassFeats() {
  static const std::vector<int> kFeats = {
      195, 251, 252, 253, 254, 255,  // Uncanny Dodge 1-6 (Barbarian, Rogue)
      196, 332, 333, 334, 1067,      // Barbarian Damage Reduction 1-4, epic
      206,                           // Evasion (the Rogue's at 2; 2024's is Improved Evasion at 7)
      222, 223, 224, 225,            // Crippling Strike, Defensive Roll, Opportunist, Skill Mastery
      259,                           // Slippery Mind (NWN's reroll; 2024's is R10)
      834, 835, 836, 837, 838, 839, 840, 841, 842, 843,  // Epic Improved Sneak Attack 1-10
  };
  return kFeats;
}

const char* NwnOnlyClassFeatName(int feat) {
  switch (feat) {
    case 195: case 251: case 252: case 253: case 254: case 255: return "Uncanny Dodge";
    case 196: case 332: case 333: case 334: case 1067: return "Damage Reduction";
    case 206: return "Evasion";
    case 222: return "Crippling Strike";
    case 223: return "Defensive Roll";
    case 224: return "Opportunist";
    case 225: return "Skill Mastery";
    case 259: return "Slippery Mind";
    default: return feat >= 834 && feat <= 843 ? "Epic Improved Sneak Attack" : "a class feat";
  }
}

int SneakAttackDice2024(int rogue_level) {
  if (rogue_level <= 0) return 0;
  const int dice = (rogue_level + 1) / 2;
  return dice > 10 ? 10 : dice;
}

bool SneakAttackApplies(bool finesse_or_ranged, RollMode mode, bool ally_next_to_target) {
  if (!finesse_or_ranged) return false;
  return mode == RollMode::kAdvantage || (mode == RollMode::kNormal && ally_next_to_target);
}

int ChampionCriticalFloor(int fighter_level) { return fighter_level >= 15 ? 18 : fighter_level >= 3 ? 19 : 20; }

bool HasRemarkableAthlete(int fighter_level) { return fighter_level >= 3; }

bool HasStudiedAttacks(int fighter_level) { return fighter_level >= 13; }

std::vector<int> GrantedFeats2024(const std::vector<ClassLevels>& classes) {
  std::vector<int> feats;
  for (const ClassLevels& c : classes) {
    if (c.character_class == CharacterClass::kFighter && c.levels >= 1) {
      feats.push_back(c.levels >= 10 ? kFeatSecondWind4 : c.levels >= 4 ? kFeatSecondWind3 : kFeatSecondWind2);
    }
    if (c.character_class == CharacterClass::kBarbarian) {
      if (HasRecklessAttack(c.levels)) feats.push_back(kFeatRecklessAttack);
      if (HasBrutalStrike(c.levels)) feats.push_back(kFeatBrutalStrike);
      if (HasIntimidatingPresence(c.levels)) feats.push_back(kFeatIntimidatingPresence);
    }
  }
  return feats;
}

// Appended only: a character's record is a bit per entry, in this order.
const std::vector<int>& GrantableFeats2024() {
  static const std::vector<int> kFeats = {kFeatSecondWind2,    kFeatSecondWind3,  kFeatSecondWind4,
                                          kFeatRecklessAttack, kFeatBrutalStrike, kFeatKnockdownShove,
                                          kFeatShovePush,      kFeatIntimidatingPresence, kFeatGrapple};
  return kFeats;
}

std::vector<int> FeatsEveryoneHas2024(bool shove_faithful) {
  std::vector<int> feats;
  if (shove_faithful) {
    feats.push_back(kFeatKnockdownShove);
    feats.push_back(kFeatShovePush);
    feats.push_back(kFeatGrapple);  // G2: Grapple, the Unarmed Strike's third option
  }
  return feats;
}

bool GrantedOnlyWhenMissing(int feat) { return feat == kFeatKnockdownShove; }

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

bool HasFightingStyleFeature2024(CharacterClass character_class, int class_level) {
  switch (character_class) {
    case CharacterClass::kFighter: return class_level >= 1;
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger: return class_level >= 2;
    default: return false;
  }
}

std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level) {
  std::vector<ClassFeature> features;
  const std::vector<ClassFeature>* table = character_class == CharacterClass::kFighter     ? &Fighter()
                                           : character_class == CharacterClass::kBarbarian ? &Barbarian()
                                           : character_class == CharacterClass::kRogue     ? &Rogue()
                                                                                           : nullptr;
  if (!table) return features;
  for (const ClassFeature& feature : *table) {
    if (feature.level <= level) features.push_back(feature);
  }
  return features;
}

}  // namespace rules
