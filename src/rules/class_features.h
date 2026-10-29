#pragma once

// Phase 3 CM9 / CM10 (PLAN.md): the 2024 class features, level by level, and
// each class's SRD 5.2 subclass (granted at class level 3). The status of
// each feature in nwn5e is part of the table, so the game can show what
// works. Filled one class at a time, each with its own brief.

#include <stdint.h>

#include <string>

#include <vector>

#include "rules/combat.h"
#include "rules/d20.h"
#include "rules/grapple.h"
#include "rules/saves.h"

namespace rules {

constexpr int kSubclassLevel = 3;

// The SRD 5.2 subclass of each class (one per class).
const char* Subclass2024(CharacterClass character_class);

struct ClassFeature {
  int level;
  const char* name;
  bool subclass;       // a feature of the SRD subclass
  const char* status;  // "built", "built: <how>", or the decision / phase it waits for
};

// The class's features up to `level` (all of them for level 20); empty for a
// class whose brief hasn't been written yet.
std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level);

// Has the class's brief been written (its features listed)?
bool HasFeatureList(CharacterClass character_class);

// Fighter (PLAN.md "Phase 3 — Fighter"), by Fighter level:
// C1 Champion's Improved / Superior Critical: the lowest d20 that crits with
// weapons and unarmed strikes (20; 19 from 3; 18 from 15).
int ChampionCriticalFloor(int fighter_level);
// C2 Remarkable Athlete (Champion 3): advantage on initiative.
bool HasRemarkableAthlete(int fighter_level);
// F6 Studied Attacks (13): a miss gives advantage on the next attack at that
// creature before the end of your next turn.
bool HasStudiedAttacks(int fighter_level);

// F5 Indomitable: rerolls of a failed save per long rest (1 at 9, 2 at 13, 3 at 17).
int IndomitableUses(int fighter_level);
// C5 Survivor's Heroic Rally (Champion 18): Bloodied = at most half the
// maximum; at the start of each turn, Bloodied and above 0, regain 5 + Con.
bool IsBloodied(int hit_points, int max_hit_points);
int HeroicRallyHitPoints(int con_modifier);
bool HasHeroicRally(int fighter_level);

// F1: the Fighting Style feats (data/rows/feat.styles.txt), feat.2da rows.
constexpr int kFeatFightingStyleArchery = 1210;
constexpr int kFeatFightingStyleDefense = 1211;
constexpr int kFeatFightingStyleGreatWeapon = 1212;
constexpr int kFeatFightingStyleTwoWeapon = 1213;
// The class levels with a Fighting Style feat slot: Fighter 1, and 7 (the
// Champion's Additional Fighting Style). Paladin / Ranger 2 with their briefs.
bool IsFightingStyleLevel2024(CharacterClass character_class, int class_level);
// The Fighting Style feature (SRD 5.2): Fighter 1, Paladin 2, Ranger 2 - the
// prerequisite of a Fighting Style feat.
bool HasFightingStyleFeature2024(CharacterClass character_class, int class_level);
constexpr int kArcheryAttackBonus = 2;
constexpr int kDefenseArmorClassBonus = 1;
// Great Weapon Fighting: a damage die of 1 or 2 counts as 3.
int GreatWeaponFightingDie(int roll);
// The NWN base items with 2024's Two-Handed or Versatile property (the
// double weapons are two weapons in NWN, so not here); NWN-only weapons as
// their nearest 2024 weapon (bastard sword, katana, dwarven waraxe, magic
// staff: versatile; heavy flail, scythe: two-handed).
bool IsTwoHandedWeapon2024(int base_item);
bool IsVersatileWeapon2024(int base_item);

// F2: Second Wind, one feat row per number of uses (data/rows/feat.fighter.txt).
constexpr int kFeatSecondWind2 = 1220;  // Fighter 1
constexpr int kFeatSecondWind3 = 1221;  // Fighter 4
constexpr int kFeatSecondWind4 = 1222;  // Fighter 10

// Barbarian (PLAN.md "Phase 3 — Barbarian", decided 2026-10-24), by
// Barbarian level: B5 Danger Sense (2: advantage on Dexterity saves unless
// Incapacitated), B8 Fast Movement (5: +10 ft out of Heavy armour), B9 Feral
// Instinct (7: advantage on initiative), B13 Primal Champion (20: Strength
// and Constitution +4, at most 25).
bool HasDangerSense(int barbarian_level);
bool HasBarbarianFastMovement(int barbarian_level);
bool HasFeralInstinct(int barbarian_level);
bool HasPrimalChampion(int barbarian_level);
// B8: +10 ft of a 30 ft speed (NWN's movement-rate factor).
constexpr float kBarbarianFastMovementFactor = 1.0f / 3.0f;
// B4 Unarmored Defense: the AC an unarmoured Barbarian adds - its Constitution
// modifier (nothing when negative) - less a Monk's Wisdom bonus the engine
// already gives it (X5: the higher of the two Unarmored Defenses, not both).
int UnarmoredDefenseConBonus(int con_modifier, int monk_wis_bonus);
// NWN heavy armour: a chest item of base AC 6 or more (NWN's own heavy range).
// Heavy armour: rules/armor.h (IsHeavyArmor2024 - chain mail, splint, plate).

// B1-B3 Rage: NWN's Rage feats (293 at 1, its successors 326-331, which the
// engine maps to the highest one a character has) stay the Rage - one pool of
// uses; spell 307 is its spell ability (served in faithful with nwn5e_rage).
constexpr int kFeatRage = 293;
constexpr int kSpellRage = 307;
bool IsRageFeat(int feat);
// Uses per Long Rest: 2; 3 at 3; 4 at 6; 5 at 12; 6 at 17 (0 below level 1).
int RageUses2024(int barbarian_level);
// Rage Damage on Strength attacks: +2; +3 at 9; +4 at 16.
int RageDamage2024(int barbarian_level);
// A Rage lasts up to 10 minutes; it lapses after a round (6 s) with no attack
// roll at an enemy, no save forced on one and no chase (B3).
constexpr int kRageMaxSeconds = 600;
constexpr int kRageLapseSeconds = 6;
// B3's chase (0.50.1; owner: "I think my henchman is raging twice in a row" -
// its AI walks to the next enemy with a plain move, and the Rage lapsed): a
// creature moving in a fight keeps its Rage while an enemy it sees is ahead -
// within 45 degrees of its facing, and within 40 m.
constexpr float kRageChaseMetres = 40.0f;
bool HeadingToward(float facing_x, float facing_y, float to_x, float to_y);
// By Barbarian level: B7 Primal Knowledge (3: Strength for some skills while
// raging), BZ2 Mindless Rage (Berserker 6), B9 Instinctive Pounce (7), B11
// Relentless Rage (11), B12 Persistent Rage (15), B13 Indomitable Might (18).
bool HasPrimalKnowledge(int barbarian_level);
bool HasMindlessRage(int barbarian_level);
bool HasInstinctivePounce(int barbarian_level);
bool HasRelentlessRage(int barbarian_level);
bool HasPersistentRage(int barbarian_level);
// BZ3 Retaliation (Berserker 10): a creature within reach that damages you takes your reaction attack.
bool HasRetaliation(int barbarian_level);
// BZ4 Intimidating Presence (Berserker 14): nwn5e's feat 1314 on the radial.
constexpr int kFeatIntimidatingPresence = 1314;
bool HasIntimidatingPresence(int barbarian_level);
// Its save: Wisdom against 8 + Strength modifier + PB; 30 ft; 1 minute, a new save each round.
int IntimidatingPresenceDc(int strength_modifier, int proficiency_bonus);
bool HasIndomitableMight(int barbarian_level);
// B9: entering Rage gives half the speed again for one round (a charge).
constexpr int kInstinctivePounceSpeedPercent = 50;
// B11: the Constitution save's DC (10, +5 per use since the last Short or
// Long Rest) and the hit points a success leaves (twice the Barbarian level).
int RelentlessRageDc(int uses_since_rest);
int RelentlessRageHitPoints(int barbarian_level);
// B13 Indomitable Might: a Strength check or save totalling less than the
// Strength score uses the score instead.
int IndomitableMightTotal(int total, int strength_score);

// Part 3 (B6, BZ1, B10, X8): Reckless Attack (2) - a radial toggle, nwn5e's
// feat 1310; Frenzy (Berserker 3 - the subclass comes at 3); Brutal Strike (9;
// Staggering and Sundering at 13; 2d10 and two effects at 17) - nwn5e's feat
// 1311, its effects picked on its sub-radial.
constexpr int kFeatRecklessAttack = 1310;
constexpr int kFeatBrutalStrike = 1311;
bool HasRecklessAttack(int barbarian_level);
bool HasFrenzy(int barbarian_level);
// Frenzy's d6s: as many as the Rage Damage bonus (2 / 3 / 4).
int FrenzyDice(int barbarian_level);
bool HasBrutalStrike(int barbarian_level);
// Brutal Strike's d10s: 1; 2 at 17. Its effects at once: 1; 2 at 17.
int BrutalStrikeDice(int barbarian_level);
int BrutalStrikeEffectCount(int barbarian_level);
enum class BrutalEffect { kNone = 0, kForceful = 1, kHamstring = 2, kStaggering = 3, kSundering = 4 };
// Forceful and Hamstring from 9, Staggering and Sundering from 13.
bool BrutalEffectAvailable(BrutalEffect effect, int barbarian_level);
const char* BrutalEffectName(BrutalEffect effect);  // "Forceful Blow" ...
// Forceful Blow: pushed 15 ft; Hamstring Blow: -15 ft speed (half of 30);
// Sundering Blow: +5 on the next attack roll another creature makes at it.
constexpr float kForcefulBlowMetres = 4.572f;
constexpr int kHamstringSpeedPercent = 50;
constexpr int kSunderingAttackBonus = 5;
// X8: the standing choice - up to two effects, the older first. Kept on the
// character as one number (0: never chosen - Hamstring Blow, the default).
struct BrutalChoice {
  BrutalEffect first = BrutalEffect::kNone;
  BrutalEffect second = BrutalEffect::kNone;
};
int EncodeBrutalChoice(const BrutalChoice& choice);
BrutalChoice DecodeBrutalChoice(int stored);
enum class BrutalPick { kChosen, kRemoved, kLocked, kOff };
// A sub-radial pick: kNone turns Brutal Strike off; an effect not yet
// available is refused; a chosen one is removed; else it's chosen - alone
// below 17, from 17 with the newer of the two kept.
BrutalPick PickBrutalEffect(BrutalChoice* choice, BrutalEffect picked, int barbarian_level);
// The effects a Brutal Strike applies now (available ones, at most the count).
std::vector<BrutalEffect> ActiveBrutalEffects(const BrutalChoice& choice, int barbarian_level);

// X1 (owner, 2026-10-24: "remove them completely"): NWN class feats with no
// 2024 counterpart, taken off characters in faithful and no longer granted
// (the classes' feat tables, data/rows/cls_feat_*.if_classfeat2024.txt).
// The Barbarian's: Uncanny Dodge 1-6, Damage Reduction 1-4 and epic; the
// Rogue's (R0, 0.50.0): Evasion, Crippling Strike, Defensive Roll,
// Opportunist, Skill Mastery, Slippery Mind and the epic sneak dice (Improved
// Evasion stays as the 2024 Evasion - R8; the Sneak Attack feats stay, R1);
// the Monk's (M0 / M16, 0.52.0): Still Mind, Purity of Body, Diamond Body,
// Diamond Soul, Perfect Self, Empty Body, Ki Strike 2-3 and the epic Ki
// Strikes (Ki Strike itself stays as Empowered Strikes - M10).
const std::vector<int>& NwnOnlyClassFeats();
// M16: feats the Monk's NWN table grants that anyone may also have picked in
// NWN (Cleave, Improved Unarmed Strike; the epic Improved Stunning Fist):
// taken off characters with Monk levels only.
const std::vector<int>& NwnOnlyMonkFeats();
// The name a chat line gives a removed feat's kind ("Uncanny Dodge" for all six).
const char* NwnOnlyClassFeatName(int feat);

// --- Rogue (PLAN.md "Phase 3 — Rogue", decided 2026-10-28) -------------------
constexpr int kFeatSneakAttack = 221;     // NWN's first Sneak Attack feat (345-353 the rest)
constexpr int kFeatEvasion = 206;         // NWN's Evasion: removed (R0)
constexpr int kFeatImprovedEvasion = 212;  // the 2024 Evasion from Rogue 7 (R8)
// R1: 2024 Sneak Attack dice - 1d6 at Rogue 1, one more at every odd level
// (10d6 at 19); twice as many on a critical hit.
int SneakAttackDice2024(int rogue_level);
// R1 (a): with a Finesse or ranged weapon, an attack with Advantage - or one
// without Disadvantage when an ally of the attacker is within 5 ft of the
// target (and not Incapacitated).
bool SneakAttackApplies(bool finesse_or_ranged, RollMode mode, bool ally_next_to_target);
// R8 Evasion (7): NWN's Improved Evasion read as 2024's - Dexterity saves only,
// not while Incapacitated (nwn5e grants it at Rogue 7; Monk 7 with its brief).
bool HasRogueEvasion(int rogue_level);
// R10 Slippery Mind (15): proficiency in Wisdom and Charisma saves.
bool HasSlipperyMind(int rogue_level);
// R11 Elusive (18): no attack roll against you has Advantage unless you're Incapacitated.
bool HasElusive(int rogue_level);
// R7 Uncanny Dodge (5): the first hit each round from an attacker it can see is
// halved (its reaction). Halves an attack's damage entries in place (each
// rounded down; -1 = unused) and returns the new total.
bool HasUncannyDodge(int rogue_level);
int HalveAttackDamage(int16_t* entries, int count);
// R12 Stroke of Luck (20): a failed save's d20 becomes a 20, once per Short or Long Rest.
bool HasStrokeOfLuck(int rogue_level);
// R6 Cunning Strike (5), Improved Cunning Strike (11: two effects at once),
// Devious Strikes (14: Daze, Knock Out, Obscure): a Sneak Attack gives up some
// of its dice (before rolling) for effects after its damage; the target's save
// DC is 8 + PB + Dexterity modifier. nwn5e's feats 1315 / 1316 put the effects
// on the radial (sub-radials, as Brutal Strike's); the choice is standing.
constexpr int kFeatCunningStrike = 1315;
constexpr int kFeatDeviousStrikes = 1316;
enum class CunningEffect { kNone = 0, kPoison = 1, kTrip = 2, kWithdraw = 3, kDaze = 4, kKnockOut = 5, kObscure = 6 };
bool HasCunningStrike(int rogue_level);
bool HasDeviousStrikes(int rogue_level);
int CunningStrikeEffectCount(int rogue_level);  // 0; 1 from 5; 2 from 11
// Poison, Trip and Withdraw from 5; Daze, Knock Out and Obscure from 14.
bool CunningEffectAvailable(CunningEffect effect, int rogue_level);
// The Sneak Attack d6s an effect costs: Poison, Trip, Withdraw 1; Daze 2; Obscure 3; Knock Out 6.
int CunningEffectCost(CunningEffect effect);
const char* CunningEffectName(CunningEffect effect);  // "Poison" ...
int CunningStrikeDc(int dexterity_modifier, int proficiency_bonus);
// The standing choice: up to two effects, the older first; kept on the
// character as one number (0: never chosen - off; henchmen never choose).
struct CunningChoice {
  CunningEffect first = CunningEffect::kNone;
  CunningEffect second = CunningEffect::kNone;
};
int EncodeCunningChoice(const CunningChoice& choice);
CunningChoice DecodeCunningChoice(int stored);
enum class CunningPick { kChosen, kRemoved, kLocked, kOff };
// A pick: kNone turns it off; an effect not yet available is refused; a chosen
// one is removed; else it's chosen - alone below 11, from 11 with the newer of
// the two kept.
CunningPick PickCunningEffect(CunningChoice* choice, CunningEffect picked, int rogue_level);
// The chosen effects that apply at this level (available ones, at most the count).
std::vector<CunningEffect> ActiveCunningEffects(const CunningChoice& choice, int rogue_level);
// One Sneak Attack of `dice` d6: the active effects, in order, while their cost
// fits in the dice left (Trip only when the target is Large or smaller - else
// it is skipped and costs nothing); *dice_left is what is still rolled.
std::vector<CunningEffect> CunningEffectsForSneakAttack(const CunningChoice& choice, int rogue_level, int dice,
                                                        bool target_can_trip, int* dice_left);
// T1 Fast Hands (Thief 3): lock and trap checks (Thieves' Tools) use the better
// of that and Sleight of Hand.
bool HasFastHands(int rogue_level);
int FastHandsValue(int thieves_tools, int sleight_of_hand);
// T4 Use Magic Device (Thief 13), scrolls: any Spell Scroll; a spell of level 2
// or more needs an Intelligence (Arcana) check against 10 + its level (a
// failure: the scroll crumbles).
bool HasUseMagicDevice(int rogue_level);
bool ScrollNeedsArcanaCheck(int spell_level);
int ScrollArcanaDc(int spell_level);
// --- Monk (PLAN.md "Phase 3 — Monk", decided 2026-10-24) --------------------
// NWN's Monk feats kept as their 2024 counterparts (M0): Monk AC Bonus is
// Unarmored Defense (M2), the Monk's Fast Movement Unarmored Movement (M6),
// Ki Strike Empowered Strikes (M10), Improved Evasion Evasion (M11).
constexpr int kFeatMonkAcBonus = 260;
constexpr int kFeatMonkEndurance = 207;
constexpr int kFeatKiStrike = 213;
// M2 Unarmored Defense (1): no armour, no shield - AC 10 + Dex + Wis. NWN also
// adds Monk level / 5 (this), which 2024 doesn't.
int NwnMonkLevelArmorClass(int monk_level);
// M6 Unarmored Movement: +10 ft at Monk 2, +15 at 6, +20 at 10, +25 at 14, +30
// at 18 (0 below 2), with no armour and no shield; as a share of a 30 ft speed
// (NWN's movement-rate factor): +1/3 ... +1.
int UnarmoredMovementFeet(int monk_level);
float UnarmoredMovementFactor(int monk_level);
// M10 Empowered Strikes (6): Unarmed Strikes can deal Force damage - in nwn5e
// they get through every damage reduction X/+N (NWN's top damage power, +20);
// per-type resistances stay.
bool HasEmpoweredStrikes(int monk_level);
constexpr int kDamagePowerPlusTwenty = 21;  // DAMAGE_POWER_PLUS_TWENTY (6 is DAMAGE_POWER_ENERGY)
// M11 Evasion (7), as the Rogue's (R8): NWN's Improved Evasion, granted at 7.
bool HasMonkEvasion(int monk_level);
// M15 Body and Mind (20): Dexterity and Wisdom +4, at most 25.
bool HasBodyAndMind(int monk_level);
// M1 Martial Arts (1), while unarmed or wielding only Monk weapons, with no
// armour and no shield: the Martial Arts die - d6, d8 at 5, d10 at 11, d12 at
// 17 (0 below 1) - for Unarmed Strikes and Monk weapons;
int MartialArtsDie(int monk_level);
// the hands it allows (base items, -1 = empty): the main hand empty or a Monk
// weapon, the off hand empty, a Monk weapon or a torch (held, not wielded);
bool MartialArtsHands(int main_hand_base_item, int off_hand_base_item);
// a weapon's damage dice give way to the die when it rolls higher on average;
bool MartialArtsDieBetter(int weapon_dice, int weapon_sides, int martial_arts_die);
// Dexterous Attacks: the better of Strength and Dexterity for the attack and
// damage of Unarmed Strikes and Monk weapons, and for the Grapple / Shove DC;
int DexterousAbilityModifier(int strength_modifier, int dexterity_modifier);
// Bonus Unarmed Strike, nwn5e's stand-in until Phase 6 (bonus actions; the
// owner's M1 (c)(i)): one more attack each round while Martial Arts holds -
// none in a round with an off-hand attack (2024's Light-weapon attack takes the
// same Bonus Action) or in NWN's Flurry of Blows mode (Flurry adds its own).
int MartialArtsBonusAttacks(bool martial_arts, int off_hand_attacks, bool flurry_mode);

// Part 3 (0.56.0): Monk's Focus and what it pays for.
constexpr int kFeatStunningFist = 39;     // NWN's Stunning Fist: Stunning Strike from Monk 5 (M9)
constexpr int kFeatFlurryOfBlows = 204;   // NWN's Flurry of Blows: the 2024 Flurry's mode from Monk 2 (M4)
constexpr int kFeatQuiveringPalm = 296;   // Open Hand 17 (part 4): 4 points
constexpr int kFeatPatientDefense = 1317;
constexpr int kFeatStepOfTheWind = 1318;
constexpr int kFeatSuperiorDefense = 1319;
// M3 Monk's Focus (2): as many points as Monk levels; all back on a Short or Long Rest.
int FocusPoints(int monk_level);  // 0 below 2
// The Focus DC: 8 + Wisdom modifier + PB.
int FocusDc(int wisdom_modifier, int proficiency_bonus);
// What a Focus feat costs: Stunning Strike, Patient Defense, Step of the Wind 1;
// Superior Defense 3; Quivering Palm 4; 0 for any other feat.
int FocusCost(int feat);
// The Monk level a Focus feat needs (Stunning Strike 5, Patient Defense and Step
// of the Wind 2, Superior Defense 18, Quivering Palm 17), 0 for any other feat.
int FocusFeatLevel(int feat);
bool HasFlurryOfBlows(int monk_level);         // 2
bool HasUncannyMetabolism(int monk_level);     // 2
bool HasStunningStrike(int monk_level);        // 5
bool HasHeightenedFocus(int monk_level);       // 10
bool HasDisciplinedSurvivor(int monk_level);   // 14
bool HasPerfectFocus(int monk_level);          // 15
bool HasSuperiorDefense(int monk_level);       // 18
// M4: Flurry of Blows' strikes - 2, 3 from Monk 10 (Heightened Focus); they
// replace the bonus strike (the engine's Flurry mode already gives one).
int FlurryStrikes(int monk_level);
// M7 Uncanny Metabolism (2), once per Long Rest, automatic at a fight's
// initiative (the owner's (a)) when at least half the points are spent or the
// Monk is Bloodied: every point back, and Martial Arts die + Monk level healed.
bool UncannyMetabolismWanted(int points, int spent, bool bloodied);
// M7 Perfect Focus (15): at initiative, Uncanny Metabolism not used then, 3 or
// fewer points left rise to 4 - the points spent after.
int PerfectFocusSpent(int points, int spent);
// M5 Heightened Focus (10): Patient Defense's temporary hit points - two Martial Arts dice.
int PatientDefenseTempDice(int monk_level);  // 0 below 10
// The resource panel's Focus tooltip (0.57.0): the points, the DC, what each
// use costs at this level, Uncanny Metabolism ready or used, Perfect Focus.
std::string FocusTooltip(int monk_level, int left, int dc, bool metabolism_ready);

// Save proficiencies class features give (whatever the first class): Slippery Mind's.
bool ClassFeatureSaveProficiency(const std::vector<ClassLevels>& classes, Ability ability);

// The feats nwn5e grants a character for its class levels (the DLL adds them —
// so characters made earlier get them too — and removes them in NWN mode).
std::vector<int> GrantedFeats2024(const std::vector<ClassLevels>& classes);
// Every feat nwn5e may grant, in a fixed order (the record kept on a character).
const std::vector<int>& GrantableFeats2024();

// G1 (docs/PLAN.md "Grapple and Throw"): 2024 lets anyone Shove, so while the
// Shove is faithful every player character has feat 23 (NWN's Knockdown, the
// Shove then) and nwn5e's Shove: push - and (G2) nwn5e's Grapple. Feat 23 NWN also lets characters take:
// granted only when missing, and only a grant nwn5e made is taken back in NWN mode.
constexpr int kFeatKnockdownShove = 23;  // kFeatShovePush: rules/grapple.h
std::vector<int> FeatsEveryoneHas2024(bool shove_faithful);
bool GrantedOnlyWhenMissing(int feat);

}  // namespace rules
