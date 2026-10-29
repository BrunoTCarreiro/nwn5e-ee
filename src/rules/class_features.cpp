#include "rules/class_features.h"

#include <algorithm>

#include "rules/weapons.h"

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
      {3, "Fast Hands", true, "built: lock and trap checks use the better of Thieves' Tools and Sleight of Hand "
                               "(the speed: Phase 6)"},
      {3, "Second-Story Work", true, "listed (NWN has no climbing or jumping)"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {5, "Cunning Strike", false, "built: a Sneak Attack gives up dice for Poison, Trip or Withdraw (its radial, "
                                   "the quickbar, !options or !cunning; DC 8 + PB + Dex)"},
      {5, "Uncanny Dodge", false, "built: the first hit each round from an attacker you see is halved - your "
                                  "reaction (no Retaliation or opportunity attack after it)"},
      {6, "Expertise", false, "built (!skills)"},
      {7, "Evasion", false, "built: no damage on a successful Dexterity save, half on a failed one; not while "
                            "Incapacitated (NWN's Improved Evasion, granted)"},
      {7, "Reliable Talent", false, "built: a d20 of 9 or lower counts as 10 on proficient checks nwn5e rolls "
                                    "(hiding, searching, the scripts' skill checks; not the engine's locks and traps)"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Supreme Sneak", true, "later (an attacking-ends-hiding rule)"},
      {10, "Ability Score Improvement", false, "built (!asi)"},
      {11, "Improved Cunning Strike", false, "built: two effects per Sneak Attack"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Use Magic Device", true, "built: any spell scroll - from level 2 an Arcana check (DC 10 + its level) "
                                      "or it crumbles; attunement and charges later"},
      {14, "Devious Strikes", false, "built: Daze, Knock Out and Obscure (its own radial)"},
      {15, "Slippery Mind", false, "built: proficient in Wisdom and Charisma saves"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Thief's Reflexes", true, "Phase 6"},
      {18, "Elusive", false, "built: no attack roll against you has Advantage unless you're Incapacitated"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Stroke of Luck", false, "built: your first failed save becomes a 20; again after a Short or Long Rest"},
  };
  return kFeatures;
}

// SRD 5.2.1 Monk and Warrior of the Open Hand (PLAN.md "Phase 3 — Monk", decided 2026-10-24).
const std::vector<ClassFeature>& Monk() {
  static const std::vector<ClassFeature> kFeatures = {
      {1, "Martial Arts", false, "built: unarmed or Monk weapons only, no armour or shield - the better of Dex "
                                 "and Str, the Martial Arts die (d6 / d8 at 5 / d10 at 11 / d12 at 17) when higher, "
                                 "one more attack each round (no off-hand attack, not in Flurry mode)"},
      {1, "Unarmored Defense", false, "built: 10 + Dex + Wis with no armour and no shield (NWN's Monk AC Bonus, "
                                      "without its Monk level / 5)"},
      {2, "Monk's Focus", false, "built: a point per Monk level, all back on a Short or Long Rest (!focus)"},
      {2, "Flurry of Blows", false, "built: its combat mode - a point a round for two strikes (three from 10) in "
                                    "place of the bonus strike, no -2"},
      {2, "Patient Defense", false, "built: radial, 1 point - no opportunity attacks this round and Dodge "
                                    "(free version: Phase 6)"},
      {2, "Step of the Wind", false, "built: radial, 1 point - no opportunity attacks this round and twice as fast "
                                     "(free version: Phase 6)"},
      {2, "Unarmored Movement", false, "built: +10 ft with no armour and no shield; +15 at 6, +20 at 10, +25 at "
                                       "14, +30 at 18"},
      {2, "Uncanny Metabolism", false, "built: automatic at a fight's start with half your points spent or "
                                       "Bloodied - every point back and Martial Arts die + level healed, once per "
                                       "Long Rest"},
      {3, "Deflect Attacks", false, "Monk part 5 (NWN's Deflect Arrows for now)"},
      {3, "Warrior of the Open Hand", false, "built: granted at 3"},
      {3, "Open Hand Technique", true, "Monk part 4"},
      {4, "Ability Score Improvement", false, "built (!asi)"},
      {4, "Slow Fall", false, "listed (NWN has no falling)"},
      {5, "Extra Attack", false, "built"},
      {5, "Stunning Strike", false, "built: NWN's Stunning Fist attack, 1 point (none on a miss), Con save "
                                    "against the Focus DC or Stunned; a success halves its speed and gives the next "
                                    "attack on it Advantage"},
      {6, "Empowered Strikes", false, "built: unarmed strikes get through every damage reduction X/+N (NWN's Ki "
                                      "Strike, from 6)"},
      {6, "Wholeness of Body", true, "Monk part 4 (NWN's for now)"},
      {7, "Evasion", false, "built: no damage on a successful Dexterity save, half on a failed one; not while "
                            "Incapacitated (NWN's Improved Evasion, granted)"},
      {8, "Ability Score Improvement", false, "built (!asi)"},
      {9, "Acrobatic Movement", false, "listed (NWN has no climbing or walking on liquids)"},
      {10, "Heightened Focus", false, "built: three Flurry strikes; Patient Defense's temporary hit points"},
      {10, "Self-Restoration", false, "Monk part 5"},
      {11, "Fleet Step", true, "Phase 6 (bonus actions)"},
      {12, "Ability Score Improvement", false, "built (!asi)"},
      {13, "Deflect Energy", false, "Monk part 5"},
      {14, "Disciplined Survivor", false, "built: proficient in every save; a failed one rolled again for a point "
                                         "(automatic)"},
      {15, "Perfect Focus", false, "built: at a fight's start, 3 or fewer points rise to 4"},
      {16, "Ability Score Improvement", false, "built (!asi)"},
      {17, "Quivering Palm", true, "Monk part 4 (NWN's for now)"},
      {18, "Superior Defense", false, "built: radial, 3 points - Resistance to every damage but Force for a "
                                     "minute (Incapacitated ends it)"},
      {19, "Epic Boon", false, "CM8"},
      {20, "Body and Mind", false, "built: Dexterity and Wisdom +4, at most 25"},
  };
  return kFeatures;
}

bool HasFeatureList(CharacterClass character_class) {
  return character_class == CharacterClass::kFighter || character_class == CharacterClass::kBarbarian ||
         character_class == CharacterClass::kRogue || character_class == CharacterClass::kMonk;
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

bool HeadingToward(float facing_x, float facing_y, float to_x, float to_y) {
  const float distance_squared = to_x * to_x + to_y * to_y;
  if (distance_squared < 0.01f || distance_squared > kRageChaseMetres * kRageChaseMetres) return false;
  const float facing_squared = facing_x * facing_x + facing_y * facing_y;
  if (facing_squared < 0.01f) return false;
  const float dot = facing_x * to_x + facing_y * to_y;
  // cos(45 degrees)^2 = 0.5: dot / (|f| |t|) >= cos 45, squared (dot positive)
  return dot > 0.0f && dot * dot >= 0.5f * facing_squared * distance_squared;
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
      208, 209, 214, 215, 216, 297,  // Still Mind, Purity of Body, Diamond Body, Diamond Soul, Perfect Self, Empty Body
      343, 344, 697, 698,            // Ki Strike 2-3, Epic Improved Ki Strike 4-5 (Ki Strike 213 stays: M10)
  };
  return kFeats;
}

const std::vector<int>& NwnOnlyMonkFeats() {
  static const std::vector<int> kFeats = {
      6, 21,                                             // Cleave, Improved Unarmed Strike
      844, 845, 846, 847, 848, 849, 850, 851, 852, 853,  // Epic Improved Stunning Fist 1-10
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
    case 208: return "Still Mind";
    case 209: return "Purity of Body";
    case 214: return "Diamond Body";
    case 215: return "Diamond Soul";
    case 216: return "Perfect Self";
    case 297: return "Empty Body";
    case 343: case 344: return "Improved Ki Strike";
    case 697: case 698: return "Epic Improved Ki Strike";
    case 6: return "Cleave";
    case 21: return "Improved Unarmed Strike";
    default:
      if (feat >= 834 && feat <= 843) return "Epic Improved Sneak Attack";
      return feat >= 844 && feat <= 853 ? "Epic Improved Stunning Fist" : "a class feat";
  }
}

int SneakAttackDice2024(int rogue_level) {
  if (rogue_level <= 0) return 0;
  const int dice = (rogue_level + 1) / 2;
  return dice > 10 ? 10 : dice;
}

bool HasRogueEvasion(int rogue_level) { return rogue_level >= 7; }

int NwnMonkLevelArmorClass(int monk_level) { return monk_level > 0 ? monk_level / 5 : 0; }

int UnarmoredMovementFeet(int monk_level) {
  if (monk_level < 2) return 0;
  if (monk_level >= 18) return 30;
  if (monk_level >= 14) return 25;
  if (monk_level >= 10) return 20;
  if (monk_level >= 6) return 15;
  return 10;
}

float UnarmoredMovementFactor(int monk_level) { return UnarmoredMovementFeet(monk_level) / 30.0f; }

bool HasEmpoweredStrikes(int monk_level) { return monk_level >= 6; }

bool HasMonkEvasion(int monk_level) { return monk_level >= 7; }

bool HasBodyAndMind(int monk_level) { return monk_level >= 20; }

int MartialArtsDie(int monk_level) {
  if (monk_level < 1) return 0;
  if (monk_level >= 17) return 12;
  if (monk_level >= 11) return 10;
  if (monk_level >= 5) return 8;
  return 6;
}

bool MartialArtsHands(int main_hand_base_item, int off_hand_base_item) {
  constexpr int kTorch = 15;
  const bool main_ok = main_hand_base_item < 0 || IsMonkWeapon2024(main_hand_base_item);
  const bool off_ok =
      off_hand_base_item < 0 || off_hand_base_item == kTorch || IsMonkWeapon2024(off_hand_base_item);
  return main_ok && off_ok;
}

bool MartialArtsDieBetter(int weapon_dice, int weapon_sides, int martial_arts_die) {
  if (martial_arts_die <= 0) return false;
  if (weapon_dice <= 0 || weapon_sides <= 0) return true;
  // Averages: d(die) = (die + 1) / 2 against n x (sides + 1) / 2.
  return martial_arts_die + 1 > weapon_dice * (weapon_sides + 1);
}

int DexterousAbilityModifier(int strength_modifier, int dexterity_modifier) {
  return dexterity_modifier > strength_modifier ? dexterity_modifier : strength_modifier;
}

int MartialArtsBonusAttacks(bool martial_arts, int off_hand_attacks, bool flurry_mode) {
  return martial_arts && off_hand_attacks <= 0 && !flurry_mode ? 1 : 0;
}

int FocusPoints(int monk_level) { return monk_level >= 2 ? monk_level : 0; }

int FocusDc(int wisdom_modifier, int proficiency_bonus) { return 8 + wisdom_modifier + proficiency_bonus; }

int FocusCost(int feat) {
  switch (feat) {
    case kFeatStunningFist:
    case kFeatPatientDefense:
    case kFeatStepOfTheWind:
      return 1;
    case kFeatSuperiorDefense:
      return 3;
    case kFeatQuiveringPalm:
      return 4;
    default:
      return 0;
  }
}

int FocusFeatLevel(int feat) {
  switch (feat) {
    case kFeatPatientDefense:
    case kFeatStepOfTheWind:
      return 2;
    case kFeatStunningFist:
      return 5;
    case kFeatQuiveringPalm:
      return 17;
    case kFeatSuperiorDefense:
      return 18;
    default:
      return 0;
  }
}

bool HasFlurryOfBlows(int monk_level) { return monk_level >= 2; }
bool HasUncannyMetabolism(int monk_level) { return monk_level >= 2; }
bool HasStunningStrike(int monk_level) { return monk_level >= 5; }
bool HasHeightenedFocus(int monk_level) { return monk_level >= 10; }
bool HasDisciplinedSurvivor(int monk_level) { return monk_level >= 14; }
bool HasPerfectFocus(int monk_level) { return monk_level >= 15; }
bool HasSuperiorDefense(int monk_level) { return monk_level >= 18; }

int FlurryStrikes(int monk_level) {
  if (!HasFlurryOfBlows(monk_level)) return 0;
  return HasHeightenedFocus(monk_level) ? 3 : 2;
}

bool UncannyMetabolismWanted(int points, int spent, bool bloodied) {
  if (points <= 0 || spent <= 0) return false;  // nothing to give back
  return bloodied || spent * 2 >= points;
}

int PerfectFocusSpent(int points, int spent) {
  const int left = points - spent;
  if (points < 4 || left >= 4) return spent;
  return points - 4;
}

int PatientDefenseTempDice(int monk_level) { return HasHeightenedFocus(monk_level) ? 2 : 0; }

std::string FocusTooltip(int monk_level, int left, int dc, bool metabolism_ready) {
  const int total = FocusPoints(monk_level);
  std::string tip = "Focus: " + std::to_string(left) + " of " + std::to_string(total) +
                    " points (one per Monk level) - all back on a Short or Long Rest. Focus save DC " +
                    std::to_string(dc) + ".\nCosts: Flurry of Blows 1 a round in its mode; Patient Defense 1; "
                    "Step of the Wind 1";
  if (HasStunningStrike(monk_level)) tip += "; Stunning Strike 1 (none on a miss)";
  if (HasDisciplinedSurvivor(monk_level)) tip += "; a failed save rolled again 1";
  if (HasSuperiorDefense(monk_level)) tip += "; Superior Defense 3";
  tip += ".";
  if (HasUncannyMetabolism(monk_level)) {
    tip += std::string("\nUncanny Metabolism: ") + (metabolism_ready ? "ready" : "used - back on a Long Rest") +
           ". At a fight's start with half your points spent or Bloodied: every point back and d" +
           std::to_string(MartialArtsDie(monk_level)) + " + " + std::to_string(monk_level) + " hit points.";
  }
  if (HasPerfectFocus(monk_level)) tip += "\nPerfect Focus: at a fight's start, 3 or fewer points rise to 4.";
  return tip;
}

bool HasSlipperyMind(int rogue_level) { return rogue_level >= 15; }

bool HasElusive(int rogue_level) { return rogue_level >= 18; }

bool HasUncannyDodge(int rogue_level) { return rogue_level >= 5; }

int HalveAttackDamage(int16_t* entries, int count) {
  int total = 0;
  for (int i = 0; entries && i < count; ++i) {
    if (entries[i] <= 0) continue;
    entries[i] = static_cast<int16_t>(entries[i] / 2);
    total += entries[i];
  }
  return total;
}

bool HasStrokeOfLuck(int rogue_level) { return rogue_level >= 20; }

bool HasFastHands(int rogue_level) { return rogue_level >= 3; }
int FastHandsValue(int thieves_tools, int sleight_of_hand) {
  return sleight_of_hand > thieves_tools ? sleight_of_hand : thieves_tools;
}
bool HasUseMagicDevice(int rogue_level) { return rogue_level >= 13; }
bool ScrollNeedsArcanaCheck(int spell_level) { return spell_level >= 2; }
int ScrollArcanaDc(int spell_level) { return 10 + spell_level; }

bool HasCunningStrike(int rogue_level) { return rogue_level >= 5; }
bool HasDeviousStrikes(int rogue_level) { return rogue_level >= 14; }
int CunningStrikeEffectCount(int rogue_level) { return rogue_level >= 11 ? 2 : HasCunningStrike(rogue_level) ? 1 : 0; }

bool CunningEffectAvailable(CunningEffect effect, int rogue_level) {
  switch (effect) {
    case CunningEffect::kPoison:
    case CunningEffect::kTrip:
    case CunningEffect::kWithdraw:
      return HasCunningStrike(rogue_level);
    case CunningEffect::kDaze:
    case CunningEffect::kKnockOut:
    case CunningEffect::kObscure:
      return HasDeviousStrikes(rogue_level);
    default:
      return false;
  }
}

int CunningEffectCost(CunningEffect effect) {
  switch (effect) {
    case CunningEffect::kPoison:
    case CunningEffect::kTrip:
    case CunningEffect::kWithdraw: return 1;
    case CunningEffect::kDaze: return 2;
    case CunningEffect::kObscure: return 3;
    case CunningEffect::kKnockOut: return 6;
    default: return 0;
  }
}

const char* CunningEffectName(CunningEffect effect) {
  switch (effect) {
    case CunningEffect::kPoison: return "Poison";
    case CunningEffect::kTrip: return "Trip";
    case CunningEffect::kWithdraw: return "Withdraw";
    case CunningEffect::kDaze: return "Daze";
    case CunningEffect::kKnockOut: return "Knock Out";
    case CunningEffect::kObscure: return "Obscure";
    default: return "none";
  }
}

int CunningStrikeDc(int dexterity_modifier, int proficiency_bonus) { return 8 + proficiency_bonus + dexterity_modifier; }

int EncodeCunningChoice(const CunningChoice& choice) {
  return 1 + static_cast<int>(choice.first) + 8 * static_cast<int>(choice.second);
}

CunningChoice DecodeCunningChoice(int stored) {
  CunningChoice choice;
  if (stored <= 0) return choice;  // never chosen: off
  const int value = stored - 1;
  const int first = value % 8, second = (value / 8) % 8;
  choice.first = first >= 1 && first <= 6 ? static_cast<CunningEffect>(first) : CunningEffect::kNone;
  choice.second = second >= 1 && second <= 6 ? static_cast<CunningEffect>(second) : CunningEffect::kNone;
  if (choice.first == CunningEffect::kNone) std::swap(choice.first, choice.second);
  if (choice.first == choice.second) choice.second = CunningEffect::kNone;
  return choice;
}

CunningPick PickCunningEffect(CunningChoice* choice, CunningEffect picked, int rogue_level) {
  if (picked == CunningEffect::kNone) {
    *choice = CunningChoice();
    return CunningPick::kOff;
  }
  if (!CunningEffectAvailable(picked, rogue_level)) return CunningPick::kLocked;
  if (choice->first == picked || choice->second == picked) {
    if (choice->first == picked) choice->first = choice->second;
    choice->second = CunningEffect::kNone;
    return CunningPick::kRemoved;
  }
  if (CunningStrikeEffectCount(rogue_level) < 2) {
    *choice = CunningChoice();
    choice->first = picked;
  } else if (choice->first == CunningEffect::kNone) {
    choice->first = picked;
  } else if (choice->second == CunningEffect::kNone) {
    choice->second = picked;
  } else {
    choice->first = choice->second;  // the older one goes
    choice->second = picked;
  }
  return CunningPick::kChosen;
}

std::vector<CunningEffect> ActiveCunningEffects(const CunningChoice& choice, int rogue_level) {
  std::vector<CunningEffect> effects;
  const size_t count = static_cast<size_t>(CunningStrikeEffectCount(rogue_level));
  for (CunningEffect e : {choice.first, choice.second}) {
    if (effects.size() < count && CunningEffectAvailable(e, rogue_level)) effects.push_back(e);
  }
  return effects;
}

std::vector<CunningEffect> CunningEffectsForSneakAttack(const CunningChoice& choice, int rogue_level, int dice,
                                                        bool target_can_trip, int* dice_left) {
  std::vector<CunningEffect> effects;
  int left = dice > 0 ? dice : 0;
  for (CunningEffect e : ActiveCunningEffects(choice, rogue_level)) {
    if (e == CunningEffect::kTrip && !target_can_trip) continue;
    const int cost = CunningEffectCost(e);
    if (cost > left) continue;
    left -= cost;
    effects.push_back(e);
  }
  if (dice_left) *dice_left = left;
  return effects;
}

bool ClassFeatureSaveProficiency(const std::vector<ClassLevels>& classes, Ability ability) {
  for (const ClassLevels& c : classes) {
    if (c.character_class == CharacterClass::kRogue && HasSlipperyMind(c.levels) &&
        (ability == Ability::kWisdom || ability == Ability::kCharisma)) {
      return true;
    }
    if (c.character_class == CharacterClass::kMonk && HasDisciplinedSurvivor(c.levels)) return true;  // M13: all six
  }
  return false;
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
    if (c.character_class == CharacterClass::kRogue) {
      if (HasRogueEvasion(c.levels)) feats.push_back(kFeatImprovedEvasion);  // R8: 2024's Evasion
      if (HasCunningStrike(c.levels)) feats.push_back(kFeatCunningStrike);   // R6: its radial
      if (HasDeviousStrikes(c.levels)) feats.push_back(kFeatDeviousStrikes);
    }
    if (c.character_class == CharacterClass::kMonk && HasMonkEvasion(c.levels)) {
      feats.push_back(kFeatImprovedEvasion);  // M11: 2024's Evasion
    }
    if (c.character_class == CharacterClass::kMonk) {  // M5, M14: Focus feats on the radial
      if (c.levels >= FocusFeatLevel(kFeatPatientDefense)) feats.push_back(kFeatPatientDefense);
      if (c.levels >= FocusFeatLevel(kFeatStepOfTheWind)) feats.push_back(kFeatStepOfTheWind);
      if (c.levels >= FocusFeatLevel(kFeatSuperiorDefense)) feats.push_back(kFeatSuperiorDefense);
    }
    if (c.character_class == CharacterClass::kBarbarian) {
      if (HasRecklessAttack(c.levels)) feats.push_back(kFeatRecklessAttack);
      if (HasBrutalStrike(c.levels)) feats.push_back(kFeatBrutalStrike);
      if (HasIntimidatingPresence(c.levels)) feats.push_back(kFeatIntimidatingPresence);
    }
  }
  // Once each (a Rogue 7 / Monk 7 asks twice for Evasion), in the order first asked.
  std::vector<int> unique;
  for (int feat : feats) {
    if (std::find(unique.begin(), unique.end(), feat) == unique.end()) unique.push_back(feat);
  }
  return unique;
}

// Appended only: a character's record is a bit per entry, in this order.
const std::vector<int>& GrantableFeats2024() {
  static const std::vector<int> kFeats = {kFeatSecondWind2,    kFeatSecondWind3,  kFeatSecondWind4,
                                          kFeatRecklessAttack, kFeatBrutalStrike, kFeatKnockdownShove,
                                          kFeatShovePush,      kFeatIntimidatingPresence, kFeatGrapple,
                                          kFeatImprovedEvasion, kFeatCunningStrike, kFeatDeviousStrikes,
                                          kFeatPatientDefense, kFeatStepOfTheWind, kFeatSuperiorDefense};
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

// Feats a character may also have taken itself (NWN's picks): only nwn5e's own grant is taken back.
bool GrantedOnlyWhenMissing(int feat) { return feat == kFeatKnockdownShove || feat == kFeatImprovedEvasion; }

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
                                           : character_class == CharacterClass::kMonk      ? &Monk()
                                                                                           : nullptr;
  if (!table) return features;
  for (const ClassFeature& feature : *table) {
    if (feature.level <= level) features.push_back(feature);
  }
  return features;
}

}  // namespace rules
