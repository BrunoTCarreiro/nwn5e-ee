// Phase 3 Monk (docs/PLAN.md "Phase 3 — Monk (CM9)", decided by the owner
// 2026-10-24: M1-M15 and OH1-OH4 A; X1 "remove them completely"), with
// class_features faithful. Part 1 (0.52.0), the mask and the passives:
// - M0 / M16: rules_features takes the NWN-only Monk feats off characters
//   (Still Mind, Purity of Body, Diamond Body, Diamond Soul, Perfect Self, Empty
//   Body, Ki Strike 2-3 and the epic Ki Strikes; from Monks also Cleave,
//   Improved Unarmed Strike and the epic Improved Stunning Fist), and the Monk's
//   feat table served in faithful (data/rows/cls_feat_monk.if_classfeat2024.txt)
//   no longer grants them; it gives Fast Movement at 2 and Ki Strike at 6.
//   Their counterparts carry the 2024 names (feat.if_classfeat2024.txt).
// - M2 Unarmored Defense: CNWSCreatureStats::GetACNaturalBase (0x3f3e90) adds,
//   while CNWSCreature::GetUseMonkAbilities holds (Monk levels, no armour - the
//   chest item's base AC 0 - and no shield), the Wisdom modifier (when positive,
//   with Monk AC Bonus 260) and Monk level / 5. For characters the level / 5 is
//   taken off: 10 + Dex + Wis. (Its argument 1 asks for that Monk part alone - the
//   same.) The sheet and GetArmorClassVersus both read it.
// - M6 Unarmored Movement: GetMovementRateFactor adds NWN's Monk part ((Monk
//   level / 3) x 0.1, a literal) when HasFeat(207) and the Monk abilities hold -
//   replacing a Barbarian's Fast Movement. Asked there for a character, 207 is
//   answered no (a Barbarian / Monk keeps both, as in 2024), and the 2024 bonus is
//   added before the other adjusters (the grapple's drag halves it too): +10 ft (a
//   third of 30 ft) at Monk 2, +15 at 6, +20 at 10, +25 at 14, +30 (double) at
//   18; at most NWN's Monk cap (3.0); not while the engine's fixed rate is set
//   (+0x788 = 1: it returns +0x78c, no class bonus); not while overloaded with
//   2024 carrying (0.52.1: Speed at most 5 ft - the owner's MK3 log showed an
//   overloaded Monk's 0.12 lifted to 0.62).
// - M10 Empowered Strikes: CNWSCreature::CalculateDamagePower (0x385610) raises
//   an unarmed Monk attack's power (GetUseMonkAttackTables) to the ruleset's
//   KI_STRIKE_LEVEL_5..1 by feats 698, 697, 344, 343, 213. For characters
//   HasFeat(213) is Monk 6+ (the served table gives it at 6; a character made
//   before still has it from 10) and KI_STRIKE_LEVEL_1 is DAMAGE_POWER_PLUS_TWENTY
//   (21): unarmed strikes get through every damage reduction X/+N - 2024's Force
//   damage, which resistance to nonmagical attacks doesn't stop. Per-type
//   resistances (X/-) stay.
// - M11 Evasion (7): rules_rogue's Evasion filter takes Monk 7 as Rogue 7;
//   rules_features grants Improved Evasion (2024's Evasion) at Monk 7.
// - M15 Body and Mind (20): rules::ClassFeatureAbilityIncreases (rules_abilities).
// Part 2 (0.53.0), M1 Martial Arts - while a character Monk is unarmed or holds
// only Monk weapons (rules::IsMonkWeapon2024; a torch may be in the off hand)
// and GetUseMonkAbilities holds (no armour, no shield):
// - (a) Dexterous Attacks: GetWeaponFinesse answers 1 for its unarmed strikes
//   and Monk weapons (rules_finesse.cpp: the better of Dex and Str to hit), its
//   damage bonus takes the better too (FinesseDamageBonus), and its Shove and
//   Grapple DC (rules_shove.cpp's RollUnarmedSave; a grapple's escape DC is the
//   same number).
// - (b) the Martial Arts die: GetUnarmedDamageDie / Dice answer d6 / d8 / d10 /
//   d12 at 1 / 5 / 11 / 17, one die, whatever the size (NWN's Monk tiers and
//   their Small rows aside); a Monk weapon whose dice roll lower on average rolls
//   the die instead (rules_fighter.cpp's weapon-dice filter - Great Weapon
//   Fighting and Savage Attacker apply to it). The served baseitems.2da marks the
//   2024 Monk weapons IsMonkWeapon (data/rows/baseitems.if_classfeat2024.txt), so
//   NWN's Flurry of Blows (part 3 makes it 2024's) and Monk attack tables take
//   them.
// - (c) the bonus unarmed strike (the owner's (c)(i), until Phase 6): one more
//   attack each round on the round's extra attacks (+0x2f60, after
//   InitializeNumberOfAttacks - rules_two_weapon.cpp), unless the round has an
//   off-hand attack or NWN's Flurry of Blows is on; it uses what is in hand.
//   Once a round (0.53.1; the owner's MA4 log: a weapon equipped mid-fight
//   gave "extra 2" - 4 attacks): the engine counts a round's attacks again on
//   an equip, and InitializeNumberOfAttacks only sets the extra attacks in
//   Flurry / Rapid Shot (else it adds Haste's to what is there); EndCombatRound
//   clears them (0x375c43) as rules_mastery's round counter ticks. So the strike
//   is added once per counted round, and taken back if a mid-round recount no
//   longer allows it (a second weapon: an off-hand attack).
// Part 3 (0.56.0), Monk's Focus (decided: M3-M5, M7, M9, M13, M14 A):
// - M3: the Focus points (Monk level, from 2) are X6's pool on the character
//   (feat_uses.cpp, record nwn5e_focus_spent; the Long Rest empties it, a Short
//   Rest too): the Focus feats' uses are the points / their cost (Stunning
//   Strike, Patient Defense, Step of the Wind 1; Superior Defense 3), on the
//   radial and quickbar as the engine counts them. The Focus DC 8 + Wis + PB.
//   `!focus`.
// - M4 Flurry of Blows: NWN's Flurry mode (feat 204, from Monk 2) is the 2024
//   Flurry - each round in it with Martial Arts and a point left spends one:
//   two strikes (three from 10, Heightened Focus) in place of the bonus strike
//   (the engine's Flurry gives one; the rest added on the round's extra
//   attacks), no off-hand attack; out of points, the bonus strike alone. Its -2
//   (FLURRY_OF_BLOWS_TO_HIT_MODIFIER) answered 0.
// - M9 Stunning Strike: NWN's Stunning Fist (39) from Monk 5, unarmed or a
//   Monk weapon (a UseFeat guard), once a turn; its point spent by the
//   engine's own decrement in ResolveMeleeSpecialAttack (after the attack roll)
//   only on a hit (feat_uses' spend filter); the save that follows is the Focus
//   DC's (rules_save_roll asks StunningStrikeSaveDc - the 2024 scale, a Con
//   save); NWN's 6 s stun on a failure; on a success half speed and the next
//   attack roll on it with Advantage (rules_advantage). The -4
//   (STUNNING_FIST_TO_HIT_MODIFIER) answered 0.
// - M7 Uncanny Metabolism (the owner's (a)): at a fight's initiative (one a
//   30 s), half the points spent or Bloodied, once per Long Rest - every point
//   back, Martial Arts die + Monk level healed. Perfect Focus (15): else 3 or
//   fewer points rise to 4.
// - M13 Disciplined Survivor (14): every save proficient (rules::
//   ClassFeatureSaveProficiency); a failed save rolled again for a point after
//   Indomitable (rules_save_roll, automatic).
// - M5 Patient Defense, Step of the Wind (2; nwn5e's feats 1317 / 1318 on the
//   radial, spells 1029 / 1030 -> n5_patient / n5_stepwind): Disengage (no
//   opportunity attacks this round - rules_opportunity) and Dodge (attacks on
//   it Disadvantage, Dexterity saves Advantage, not while Incapacitated) or a
//   Dash (twice as fast for the round); Heightened Focus: Patient Defense's two
//   Martial Arts dice of temporary hit points. The free versions: Phase 6.
// - M14 Superior Defense (18; feat 1319, spell 1031 -> n5_superior, 3 points):
//   Resistance (X4, resistance.cpp) to every damage but Force for a minute,
//   Incapacitated ends it.
// Monsters keep NWN's Monk.

#include <windows.h>

#include <intrin.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "feat_uses.h"
#include "feats.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "movement_rate.h"
#include "records.h"
#include "resistance.h"
#include "resource_panel.h"
#include "rest.h"
#include "ruleset.h"
#include "script_hooks.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"
#include "rules/weapons.h"

namespace {

constexpr int kNwnClassMonk = 5;
constexpr float kMonkSpeedCap = 3.0f;  // ruleset MOVEMENT_SPEED_BONUS_MONK_CAP's default

using AcNaturalBaseFn = char (*)(void* stats, int monk_part_only);
using UseMonkAbilitiesFn = int (*)(void* creature);
using DamagePowerFn = uint8_t (*)(void* creature, void* target, int offhand);
using UnarmedDiceFn = uint8_t (*)(void* stats);

AcNaturalBaseFn g_ac_natural_base = nullptr;
UseMonkAbilitiesFn g_use_monk_abilities = nullptr;
DamagePowerFn g_damage_power = nullptr;
UnarmedDiceFn g_unarmed_die = nullptr;
UnarmedDiceFn g_unarmed_dice = nullptr;
CodeRange g_movement_code;           // GetMovementRateFactor: its HasFeat(207)
thread_local void* t_powering = nullptr;  // the creature CalculateDamagePower is working for
LogOnce g_logged(500);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

uint32_t IdOf(const void* creature) { return creature ? Field<uint32_t>(creature, ee::kObjectId) : 0; }

// A character's (player character or henchman) Monk levels in faithful; 0 otherwise.
int MonkLevelOf(void* stats) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, rules::CharacterClass::kMonk);
}

// The Monk class slot's own levels (what GetACNaturalBase divides by 5).
int NwnMonkSlotLevels(void* stats) {
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (slot.nwn_class_id == kNwnClassMonk) return slot.levels;
  }
  return 0;
}

bool UsesMonkAbilities(void* creature) { return creature && g_use_monk_abilities && g_use_monk_abilities(creature); }

// M2: NWN's Monk level / 5 off a character's natural AC.
char AcNaturalBase(void* stats, int monk_part_only) {
  const char ac = g_ac_natural_base(stats, monk_part_only);
  if (MonkLevelOf(stats) <= 0) return ac;
  const int drop = rules::NwnMonkLevelArmorClass(NwnMonkSlotLevels(stats));
  if (drop <= 0 || !UsesMonkAbilities(CreatureOf(stats))) return ac;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), drop, 2))) {
    Log("monk: 0x%08X Unarmored Defense - NWN's +%d (Monk level / 5) left out: 10 + Dex + Wis", ObjectIdOf(stats),
        drop);
  }
  return static_cast<char>(ac - drop);
}

// M6: NWN's Monk speed is not added for characters (the 2024 one is, below).
bool MonkSpeedFeat(void* stats, int feat, uintptr_t caller, int* answer) {
  if (feat != rules::kFeatMonkEndurance || !Faithful() || !g_movement_code.Contains(caller)) return false;
  if (!stats || !ReadCreatureFacts(stats).character) return false;  // monsters: NWN's
  *answer = 0;
  return true;
}

float UnarmoredMovement(void* creature, float factor) {
  if (!creature || !Faithful() || Field<int32_t>(creature, ee::kCreatureFixedMovementRate) == 1) return factor;
  if (OverloadedSpeedCap(creature)) return factor;
  void* stats = StatsOf(creature);
  const int level = MonkLevelOf(stats);
  const float bonus = rules::UnarmoredMovementFactor(level);
  if (bonus <= 0.0f || !UsesMonkAbilities(creature)) return factor;
  const float faster = std::min(factor + bonus, std::max(factor, kMonkSpeedCap));
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), rules::UnarmoredMovementFeet(level), 3))) {
    Log("monk: 0x%08X Unarmored Movement +%d ft (Monk %d): movement rate %.2f -> %.2f", ObjectIdOf(stats),
        rules::UnarmoredMovementFeet(level), level, factor, faster);
  }
  return faster;
}

// M10: Ki Strike (213) is Empowered Strikes, from Monk 6, for characters.
bool EmpoweredStrikesFeat(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if (feat != rules::kFeatKiStrike || !Faithful() || !stats || !ReadCreatureFacts(stats).character) return false;
  *answer = rules::HasEmpoweredStrikes(MonkLevelOf(stats)) ? 1 : 0;
  return true;
}

uint8_t CalculateDamagePower(void* creature, void* target, int offhand) {
  void* outer = t_powering;
  t_powering = creature;
  const uint8_t power = g_damage_power(creature, target, offhand);
  t_powering = outer;
  return power;
}

int KiStrikeLevel1(int engine_value) {
  void* stats = t_powering ? StatsOf(t_powering) : nullptr;
  const int level = MonkLevelOf(stats);
  if (!rules::HasEmpoweredStrikes(level)) return engine_value;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), level, 4))) {
    Log("monk: 0x%08X Empowered Strikes (Monk %d): unarmed strikes at damage power +20 (through any DR X/+N)",
        ObjectIdOf(stats), level);
  }
  return rules::kDamagePowerPlusTwenty;
}

}  // namespace

// --- Part 2: M1 Martial Arts --------------------------------------------------------

// The Monk level when Martial Arts holds for a creature now (a character in
// faithful; unarmed or only Monk weapons; no armour, no shield), else 0.
int MartialArtsLevel(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  const int level = MonkLevelOf(stats);
  if (level <= 0) return 0;
  if (!rules::MartialArtsHands(engine::BaseItemRowInSlot(creature, ee::kSlotRightHand),
                               engine::BaseItemRowInSlot(creature, ee::kSlotLeftHand)) ||
      !UsesMonkAbilities(creature)) {
    return 0;
  }
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), level, 5))) {
    Log("monk: 0x%08X Martial Arts (Monk %d): d%d unarmed, the better of Dex and Str, Monk weapons roll the d%d "
        "when higher, a bonus strike each round", ObjectIdOf(stats), level, rules::MartialArtsDie(level),
        rules::MartialArtsDie(level));
  }
  return level;
}

// (a) Dexterous Attacks for an attack with this hand's item (-1: unarmed).
bool MartialArtsAttack(void* creature, int base_item) {
  return (base_item < 0 || rules::IsMonkWeapon2024(base_item)) && MartialArtsLevel(creature) > 0;
}

// (b) The die a Monk weapon's damage rolls instead of its own dice (0: its own).
int MartialArtsWeaponDie(void* creature, int base_item, int dice, int sides) {
  if (base_item < 0 || !rules::IsMonkWeapon2024(base_item)) return 0;
  const int level = MartialArtsLevel(creature);
  const int die = rules::MartialArtsDie(level);
  if (!rules::MartialArtsDieBetter(dice, sides, die)) return 0;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(IdOf(creature), base_item, 6 + die * 16))) {
    Log("monk: 0x%08X Martial Arts: base item %d rolls d%d in place of its %dd%d", IdOf(creature), base_item, die,
        dice, sides);
  }
  return die;
}

// (c) After InitializeNumberOfAttacks (rules_two_weapon.cpp): the bonus strike, once a round.
namespace {
struct BonusStrike {
  uint32_t round = 0;  // CombatRoundsEnded when it was last decided
  bool added = false;
  bool flurry_paid = false;  // M4: this round's Flurry point spent
};
std::unordered_map<uint32_t, BonusStrike> g_bonus_strikes;  // by creature id (the engine's thread)
bool g_rounds_counted = false;
}  // namespace

bool SpendFlurryPoint(void* creature);  // below (part 3)

void MartialArtsBonusStrike(void* round) {
  void* creature = round ? Field<void*>(round, ee::kRoundOwner) : nullptr;
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  const int level = MartialArtsLevel(creature);
  const int off_hand = Field<int32_t>(round, ee::kRoundOffHandAttacks);
  const bool flurry = Field<uint8_t>(creature, ee::kCreatureCombatMode) == ee::kCombatModeFlurry;
  const int extra = rules::MartialArtsBonusAttacks(level > 0, off_hand, flurry);
  const int extra_now = Field<int32_t>(round, ee::kRoundExtraAttacks);
  const uint32_t now = g_rounds_counted ? CombatRoundsEnded(id) : 0;
  const auto known = g_bonus_strikes.find(id);
  // M4 Flurry of Blows (the mode, from Monk 2): a point a round for its strikes; the engine's Flurry
  // has set the round's extra attacks to one (+ Haste's) - the bonus strike's place.
  if (flurry && level > 0 && rules::HasFlurryOfBlows(level)) {
    const bool this_round = g_rounds_counted && known != g_bonus_strikes.end() && known->second.round == now;
    const bool paid = this_round ? known->second.flurry_paid : SpendFlurryPoint(creature);
    if (paid) SetField<int32_t>(round, ee::kRoundExtraAttacks, extra_now + rules::FlurryStrikes(level) - 1);
    if (off_hand > 0) SetField<int32_t>(round, ee::kRoundOffHandAttacks, 0);  // the same Bonus Action
    BonusStrike state;
    state.round = now;
    state.flurry_paid = paid;
    g_bonus_strikes[id] = state;
    if (g_config.log_attacks && g_logged.First(LogOnce::Key(id, paid ? rules::FlurryStrikes(level) : 1, 9))) {
      Log("monk: 0x%08X Flurry of Blows: %s", id,
          paid ? (std::to_string(rules::FlurryStrikes(level)) + " strikes this round (a Focus point)").c_str()
               : "no Focus point left - the bonus strike alone");
    }
    return;
  }
  // Out of Flurry mode mid-round after its point was paid: the round keeps the Flurry's strikes (a
  // recount outside Flurry adds to what is there), no bonus strike on top.
  if (g_rounds_counted && known != g_bonus_strikes.end() && known->second.round == now && known->second.flurry_paid) {
    return;
  }
  // Already added this round (a recount: an equip mid-round) - and still there (Flurry's set would replace it).
  const bool had = g_rounds_counted && known != g_bonus_strikes.end() && known->second.round == now &&
                   known->second.added && extra_now >= 1 && !flurry;
  if (extra && !had) {
    SetField<int32_t>(round, ee::kRoundExtraAttacks, extra_now + extra);
  } else if (!extra && had) {
    SetField<int32_t>(round, ee::kRoundExtraAttacks, extra_now - 1);  // a second weapon now: an off-hand attack
  }
  if (extra || had || known != g_bonus_strikes.end()) g_bonus_strikes[id] = BonusStrike{now, extra > 0};
  if (level > 0 && g_config.log_attacks &&
      g_logged.First(LogOnce::Key(id, extra * 4 + (off_hand > 0 ? 1 : 0) + (flurry ? 2 : 0), 7))) {
    Log("monk: 0x%08X Martial Arts bonus strike: %s", id,
        extra ? "one more attack this round" : off_hand > 0 ? "none (an off-hand attack this round)"
                                                            : "none (NWN's Flurry of Blows adds its own)");
  }
  if (had && g_config.log_attacks && g_logged.First(LogOnce::Key(id, extra, 8))) {
    Log("monk: 0x%08X attacks counted again mid-round (an equip): the bonus strike %s", id,
        extra ? "kept once" : "taken back");
  }
}

namespace {

uint8_t UnarmedDamageDie(void* stats) {
  const int level = MartialArtsLevel(CreatureOf(stats));
  return level > 0 ? static_cast<uint8_t>(rules::MartialArtsDie(level)) : g_unarmed_die(stats);
}

uint8_t UnarmedDamageDice(void* stats) {
  return MartialArtsLevel(CreatureOf(stats)) > 0 ? 1 : g_unarmed_dice(stats);
}

}  // namespace

// --- Part 3 (0.56.0): Monk's Focus and what it pays for -------------------------------

namespace {

constexpr const char* kVarFocusSpent = "nwn5e_focus_spent";       // X6's pool: the points spent (record)
constexpr const char* kVarMetabolismUsed = "nwn5e_metabolism_used";  // M7: since the last Long Rest (record)
constexpr uint64_t kRoundMs = 6000;

CodeRange g_special_attack_code;  // ResolveMeleeSpecialAttack: Stunning Fist's decrement and save

std::unordered_map<uint32_t, uint64_t> g_dodging;      // M5 Patient Defense: until (game ms)
std::unordered_map<uint32_t, uint64_t> g_disengaged;   // M5: no opportunity attacks until (game ms)
std::unordered_map<uint32_t, uint64_t> g_superior;     // M14 Superior Defense: until (game ms)
std::unordered_map<uint32_t, uint64_t> g_stun_marks;   // M9: a save made - the next attack on it has Advantage
std::unordered_map<uint32_t, uint32_t> g_stun_round;   // M9: the round of the last Stunning Strike (once a turn)
std::unordered_map<uint32_t, uint64_t> g_initiative;   // M7: the last initiative's game time (one a fight)

struct PendingStun {
  uint32_t attacker = 0;
  uint32_t target = 0;
  int dc = 0;
  ULONGLONG at = 0;
};
PendingStun g_pending_stun;  // armed by the point spent, read by the save right after

int FocusTotal(void* creature) { return rules::FocusPoints(MonkLevelOf(creature ? StatsOf(creature) : nullptr)); }

int FocusLeft(void* creature) {
  const int left = FocusTotal(creature) - SpentFeatUses(creature, kVarFocusSpent);
  return left > 0 ? left : 0;
}

bool SpendFocus(void* creature, int points, const char* why) {
  if (FocusLeft(creature) < points) return false;
  SetSpentFeatUses(creature, kVarFocusSpent, SpentFeatUses(creature, kVarFocusSpent) + points);
  Log("monk: 0x%08X %s - %d Focus point%s spent (%d left)", IdOf(creature), why, points, points == 1 ? "" : "s",
      FocusLeft(creature));
  return true;
}

int FocusDcOf(void* creature) {
  void* stats = StatsOf(creature);
  if (!stats) return 10;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  return rules::FocusDc(engine::AbilityModifier(stats, 4),
                        rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating));
}

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

bool Until(const std::unordered_map<uint32_t, uint64_t>& map, uint32_t id) {
  const auto it = map.find(id);
  return it != map.end() && GameMs() < it->second;
}

// M3: the Focus feats' uses are the pool's points / their cost (X6).
bool FocusUsesTotal(void* stats, int feat, int* total) {
  if (!rules::FocusCost(feat) || !Faithful()) return false;
  const int level = MonkLevelOf(stats);
  if (level <= 0) return false;  // a non-Monk's Stunning Fist: NWN's
  *total = rules::FocusPoints(level);
  return true;
}

int FocusUsesCost(void* /*stats*/, int feat) { return rules::FocusCost(feat); }

// M9: Stunning Fist's own decrement (ResolveMeleeSpecialAttack, after the attack roll) spends a
// point only on a hit - 2024 pays on a hit - and arms the save that follows with the Focus DC.
bool FocusUsesSpend(void* stats, int feat, uintptr_t caller) {
  if (feat != rules::kFeatStunningFist || !g_special_attack_code.Contains(caller)) return true;
  void* creature = CreatureOf(stats);
  uint8_t* data = creature ? CurrentAttackData(creature) : nullptr;
  const uint32_t id = IdOf(creature);
  if (!data || !rules::MasteryAttackHit(data[ee::kAttackResult])) {
    Log("monk: 0x%08X Stunning Strike missed - no Focus point spent", id);
    Tell(creature, "Stunning Strike missed - no Focus point spent.");
    return false;
  }
  g_pending_stun = PendingStun{id, Field<uint32_t>(creature, ee::kCreatureAttackTarget), FocusDcOf(creature),
                               GetTickCount64()};
  if (g_rounds_counted) g_stun_round[id] = CombatRoundsEnded(id);
  Log("monk: 0x%08X Stunning Strike hits - 1 Focus point; Constitution save DC %d", id, g_pending_stun.dc);
  return true;
}

// M3: the HasFeat answers below their 2024 levels (the engine's own above them): Stunning Fist
// (Stunning Strike) from Monk 5, Flurry of Blows from 2 - for a Monk's character.
bool FocusFeatFilter(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if ((feat != rules::kFeatStunningFist && feat != rules::kFeatFlurryOfBlows) || !Faithful() || !stats) return false;
  const int level = MonkLevelOf(stats);
  if (level <= 0) return false;
  const bool has = feat == rules::kFeatStunningFist ? rules::HasStunningStrike(level) : rules::HasFlurryOfBlows(level);
  if (has) return false;
  *answer = 0;
  return true;
}

// M9, 0.57.10 (owner, 2026-10-30, FO6: "cannot use stunning strike while armed"): UseFeat's case for
// Stunning Fist and Quivering Palm (0x3A9590, read 89.8193.37: its jump table sends feats 39 and 296 there)
// refuses any item in the right hand - "You cannot use this feat while armed" (feedback 0xC6), NWN's
// unarmed-only rule. 2024's Stunning Strike takes a Monk weapon too: when the guard below finds Martial Arts
// holding, it marks the creature's inventory, and that one GetItemInSlot call (the one the 0xC6 feedback
// follows) answers an empty right hand for it, once. A non-Monk weapon is refused by the guard first.
using GetItemInSlotFn = void* (*)(void* inventory, uint32_t slot);
GetItemInSlotFn g_get_item_in_slot = nullptr;
uintptr_t g_armed_check_return = 0;    // the return address of UseFeat's "while armed" GetItemInSlot
void* g_armed_ok_inventory = nullptr;  // set by the guard for the use about to run; taken by that call

void* GetItemInSlotHook(void* inventory, uint32_t slot) {
  void* item = g_get_item_in_slot(inventory, slot);
  if (item && inventory && inventory == g_armed_ok_inventory &&
      reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_armed_check_return) {
    g_armed_ok_inventory = nullptr;
    return nullptr;
  }
  return item;
}

// The call in UseFeat that the "while armed" feedback (mov edx, 0C6h) follows; 0 if not found.
uintptr_t FindArmedCheck() {
  static const uint8_t kFeedbackArmed[] = {0xBA, 0xC6, 0x00, 0x00, 0x00};
  const CodeRange use_feat = FunctionAt(EngineExport(ee::kUseFeat));
  uintptr_t found = 0;
  int count = 0;
  for (uintptr_t call : FindCallsTo(use_feat, EngineExport(ee::kGetItemInSlot))) {
    if (!ContainsBytes(call + 5, call + 5 + 0x18, kFeedbackArmed, sizeof(kFeedbackArmed))) continue;
    found = call + 5;
    ++count;
  }
  return count == 1 ? found : 0;
}

// M9: Stunning Strike - Martial Arts' attacks only, once a turn.
bool StunningStrikeGuard(void* creature, int feat) {
  g_armed_ok_inventory = nullptr;  // a mark is for the use right after it, never a later one
  if (feat != rules::kFeatStunningFist || !Faithful()) return true;
  const int level = MonkLevelOf(StatsOf(creature));
  if (level <= 0) return true;
  if (MartialArtsLevel(creature) <= 0) {
    Tell(creature, "Stunning Strike needs an Unarmed Strike or a Monk weapon (no armour, no shield).");
    return false;
  }
  const uint32_t id = IdOf(creature);
  const auto last = g_stun_round.find(id);
  if (g_rounds_counted && last != g_stun_round.end() && last->second == CombatRoundsEnded(id)) {
    Tell(creature, "Stunning Strike - once a turn.");
    return false;
  }
  if (g_armed_check_return) g_armed_ok_inventory = Field<void*>(creature, ee::kCreatureInventory);  // a Monk weapon
  return true;
}

// X6: every point back on a Short Rest.
std::string FocusShortRest(void* creature) {
  const int spent = SpentFeatUses(creature, kVarFocusSpent);
  if (FocusTotal(creature) <= 0 || spent <= 0) return "";
  SetSpentFeatUses(creature, kVarFocusSpent, 0);
  return " Focus: all " + std::to_string(FocusTotal(creature)) + " points back.";
}

// M7: Uncanny Metabolism comes back on a Long Rest (the pool itself: feat_uses.cpp).
void MetabolismLongRest(void* stats) {
  void* creature = CreatureOf(stats);
  if (creature && engine::GetLocalInt(creature, kVarMetabolismUsed)) SetRecord(creature, kVarMetabolismUsed, 0);
}

// M5 Patient Defense (1 point): no opportunity attacks this round and Dodge until the next one;
// Heightened Focus (10) adds two Martial Arts dice of temporary hit points.
void PatientDefenseScript(uint32_t self) {
  void* creature = CreatureById(self);
  const int level = MonkLevelOf(creature ? StatsOf(creature) : nullptr);
  if (level <= 0) return;
  g_dodging[self] = GameMs() + kRoundMs;
  g_disengaged[self] = GameMs() + kRoundMs;
  std::string line = "Patient Defense - you Disengage and Dodge until your next round (attacks on you have "
                     "Disadvantage, Dexterity saves Advantage).";
  if (const int dice = rules::PatientDefenseTempDice(level)) {
    const int hp = RollEngineDice(dice, rules::MartialArtsDie(level));
    ApplyEffectAs(self, self, "DURATION_TYPE_TEMPORARY", ("EffectTemporaryHitpoints(" + std::to_string(hp) + ")").c_str(),
                  600.0f, 0.0f);
    line += " Heightened Focus: " + std::to_string(hp) + " temporary hit points.";
  }
  Log("monk: 0x%08X Patient Defense", self);
  Tell(creature, line);
}

// M5 Step of the Wind (1 point): no opportunity attacks this round, twice as fast for it (a Dash).
void StepOfTheWindScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature || MonkLevelOf(StatsOf(creature)) <= 0) return;
  g_disengaged[self] = GameMs() + kRoundMs;
  ApplyEffectAs(self, self, "DURATION_TYPE_TEMPORARY", "EffectMovementSpeedIncrease(100)", 6.0f, 0.0f);
  Log("monk: 0x%08X Step of the Wind", self);
  Tell(creature, "Step of the Wind - you Disengage and Dash: twice as fast this round, no opportunity attacks.");
}

// M14 Superior Defense (3 points): Resistance to every damage but Force for a minute.
void SuperiorDefenseScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature || !rules::HasSuperiorDefense(MonkLevelOf(StatsOf(creature)))) return;
  g_superior[self] = GameMs() + 60000;
  Log("monk: 0x%08X Superior Defense for a minute", self);
  Tell(creature, "Superior Defense - Resistance to every damage but Force for a minute (Incapacitated ends it).");
}

uint32_t SuperiorDefenseResistance(void* object) {
  const uint32_t id = IdOf(object);
  if (g_superior.empty() || !Until(g_superior, id)) return 0;
  void* creature = CreatureById(id);
  if (creature && IncapacitatedByCondition(creature)) {
    g_superior.erase(id);
    Log("monk: 0x%08X Superior Defense ends (Incapacitated)", id);
    return 0;
  }
  constexpr uint32_t kAllDamage = 0xFFF, kMagical = 8;  // DAMAGE_TYPE_*: bludgeoning 1 ... sonic 2048; Force = magical
  return kAllDamage & ~kMagical;
}

// !focus
// The resource panel (resource_panel.cpp, 0.57.0): Focus, Uncanny Metabolism its dot.
bool FocusHudPool(void* creature, rules::HudPool* pool) {
  const int level = MonkLevelOf(StatsOf(creature));
  const int total = rules::FocusPoints(level);
  if (total <= 0) return false;
  const bool metabolism = !engine::GetLocalInt(creature, kVarMetabolismUsed);
  pool->name = "Focus";
  pool->left = FocusLeft(creature);
  pool->total = total;
  if (rules::HasUncannyMetabolism(level)) pool->flags.push_back({"Metabolism", metabolism});
  pool->tooltip = rules::FocusTooltip(level, pool->left, FocusDcOf(creature), metabolism);
  return true;
}

// !focus; with [debug] test_commands, "!focus left <n>" sets the points left and "!focus reset" gives them all
// back with Uncanny Metabolism ready (and the next initiative a new fight).
std::vector<std::string> FocusCommand(void* creature, const std::string& args) {
  const int level = MonkLevelOf(StatsOf(creature));
  if (rules::FocusPoints(level) <= 0) return {"nwn5e: !focus - a Monk's Focus points (Monk 2+, class_features faithful)."};
  EnsureRecords(creature);
  if (g_config.debug_test_commands && !args.empty()) {
    char word[16] = {};
    int number = 0;
    const int fields = sscanf_s(args.c_str(), "%15s %d", word, 16u, &number);
    const int points = rules::FocusPoints(level);
    if (fields == 2 && std::string(word) == "left") {
      SetSpentFeatUses(creature, kVarFocusSpent, std::clamp(points - number, 0, points));
    } else if (fields >= 1 && std::string(word) == "reset") {
      SetSpentFeatUses(creature, kVarFocusSpent, 0);
      SetRecord(creature, kVarMetabolismUsed, 0);
      g_initiative.erase(IdOf(creature));
    } else {
      return {"nwn5e: !focus [left <n> | reset]"};
    }
  }
  std::string line = "nwn5e: Focus " + std::to_string(FocusLeft(creature)) + " of " +
                     std::to_string(rules::FocusPoints(level)) + " points, DC " + std::to_string(FocusDcOf(creature)) +
                     ". Flurry of Blows 1 a round in its mode; Patient Defense, Step of the Wind 1";
  if (rules::HasStunningStrike(level)) line += "; Stunning Strike 1 (none on a miss)";
  if (rules::HasDisciplinedSurvivor(level)) line += "; a failed save rolled again 1";
  if (rules::HasSuperiorDefense(level)) line += "; Superior Defense 3";
  line += ". Uncanny Metabolism ";
  line += engine::GetLocalInt(creature, kVarMetabolismUsed) ? "used (back on a Long Rest)." : "ready.";
  return {line, "  All points back on a Short or Long Rest."};
}

}  // namespace

// M9 (rules_save_roll.cpp): the save a Stunning Strike forces - the Focus DC on the 2024 scale. Only
// the save ResolveMeleeSpecialAttack asks for (its feat argument is 0: told by the call site) right
// after the point was spent.
bool StunningStrikeSaveDc(void* target, uint32_t versus, uintptr_t caller, int* dc) {
  if (!g_special_attack_code.Contains(caller)) return false;
  if (!g_pending_stun.attacker || g_pending_stun.attacker != versus || GetTickCount64() - g_pending_stun.at > 2000) {
    return false;
  }
  if (g_pending_stun.target && target && g_pending_stun.target != IdOf(target)) {
    Log("monk: 0x%08X Stunning Strike's save made by 0x%08X (its attack target 0x%08X)", versus, IdOf(target),
        g_pending_stun.target);
  }
  *dc = g_pending_stun.dc;
  return true;
}

// The save's result: 0 failed, 1 saved, 2 immune.
void StunningStrikeSaveResult(void* target, uint32_t versus, int result) {
  if (!g_pending_stun.attacker || g_pending_stun.attacker != versus) return;
  g_pending_stun = PendingStun();
  void* monk = CreatureById(versus);
  const uint32_t id = IdOf(target);
  if (result == 2) {
    Log("monk: 0x%08X is immune to the Stunning Strike", id);
    return;
  }
  if (result == 0) {
    Log("monk: 0x%08X fails the Stunning Strike save - Stunned until 0x%08X's next round", id, versus);
    Tell(monk, "Stunning Strike - it fails its save: Stunned until your next round.");
    return;
  }
  // A success: speed halved and the next attack roll against it has Advantage, until the Monk's next round.
  ApplyEffectAs(versus, id, "DURATION_TYPE_TEMPORARY", "EffectMovementSpeedDecrease(50)", 6.0f, 0.0f);
  g_stun_marks[id] = GameMs() + kRoundMs;
  Log("monk: 0x%08X saves against the Stunning Strike - half speed, the next attack on it has Advantage", id);
  Tell(monk, "Stunning Strike - it saves: half speed, and the next attack on it has Advantage.");
}

// M13 Disciplined Survivor (rules_save_roll.cpp, after Indomitable): a failed save rolled again for a point.
bool DisciplinedSurvivorReroll(void* creature) {
  if (!creature || !rules::HasDisciplinedSurvivor(MonkLevelOf(StatsOf(creature)))) return false;
  if (!SpendFocus(creature, 1, "Disciplined Survivor: a failed save rolled again")) return false;
  Tell(creature, "Disciplined Survivor - the failed save is rolled again (1 Focus point).");
  return true;
}

// M5 (rules_opportunity.cpp): Patient Defense / Step of the Wind - moving provokes nothing this round.
bool MonkDisengaged(uint32_t id) { return !g_disengaged.empty() && Until(g_disengaged, id); }

// M5 Dodge (rules_barbarian.cpp's save advantage, rules_advantage.cpp): not while Incapacitated.
bool MonkDodging(void* creature) {
  if (g_dodging.empty() || !creature) return false;
  return Until(g_dodging, IdOf(creature)) && !IncapacitatedByCondition(creature);
}

// rules_advantage.cpp: Dodge (Disadvantage on attacks against it), Stunning Strike's saved mark
// (the next attack against it has Advantage - used up).
MasteryRollSources MonkRollSources(void* /*attacker*/, void* target) {
  MasteryRollSources sources;
  if (MonkDodging(target)) {
    sources.disadvantage = 1;
    sources.note = "Dodge (Patient Defense)";
  }
  const uint32_t id = IdOf(target);
  if (!g_stun_marks.empty() && Until(g_stun_marks, id)) {
    g_stun_marks.erase(id);
    sources.advantage = 1;
    sources.note += (sources.note.empty() ? "" : ", ") + std::string("Stunning Strike saved: Advantage");
  }
  return sources;
}

// M7 (rules_initiative.cpp): Uncanny Metabolism and Perfect Focus at a fight's initiative.
void MonkOnInitiative(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  const int level = MonkLevelOf(stats);
  if (!rules::HasUncannyMetabolism(level)) return;
  EnsureRecords(creature);
  const uint32_t id = IdOf(creature);
  const uint64_t now = GameMs();
  uint64_t& last = g_initiative[id];
  if (last && now - last < 30000) return;  // the same fight
  last = now;
  const int points = rules::FocusPoints(level);
  const int spent = SpentFeatUses(creature, kVarFocusSpent);
  const int hit_points = Field<int16_t>(creature, ee::kObjectCurrentHitPoints);
  static const auto max_hp = reinterpret_cast<int16_t (*)(void*, int)>(EngineExport(ee::kGetMaxHitPoints));
  const bool bloodied = max_hp && rules::IsBloodied(hit_points, max_hp(creature, 1));
  if (!engine::GetLocalInt(creature, kVarMetabolismUsed) && rules::UncannyMetabolismWanted(points, spent, bloodied)) {
    SetSpentFeatUses(creature, kVarFocusSpent, 0);
    const int healed = RollEngineDice(1, rules::MartialArtsDie(level)) + level;
    ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", ("EffectHeal(" + std::to_string(healed) + ")").c_str(), 0.0f, 0.0f);
    SetRecord(creature, kVarMetabolismUsed, 1);
    Log("monk: 0x%08X Uncanny Metabolism: %d Focus points back, %d healed (%s)", id, spent, healed,
        bloodied ? "Bloodied" : "half the points spent");
    Tell(creature, "Uncanny Metabolism - every Focus point back and " + std::to_string(healed) +
                       " hit points (once per Long Rest).");
    return;
  }
  if (rules::HasPerfectFocus(level)) {
    const int after = rules::PerfectFocusSpent(points, spent);
    if (after != spent) {
      SetSpentFeatUses(creature, kVarFocusSpent, after);
      Log("monk: 0x%08X Perfect Focus: %d -> 4 Focus points", id, points - spent);
      Tell(creature, "Perfect Focus - your Focus rises to 4 points.");
    }
  }
}

bool SpendFlurryPoint(void* creature) { return SpendFocus(creature, 1, "Flurry of Blows"); }

void InstallMonkRules() {
  if (!Faithful()) return;
  ServeConditionalRows("classfeat2024");  // M0: cls_feat_monk.if_classfeat2024.txt, the 2024 names
  g_use_monk_abilities = reinterpret_cast<UseMonkAbilitiesFn>(EngineExport(ee::kGetUseMonkAbilities));
  if (!g_use_monk_abilities) Log("monk: no %s - Unarmored Defense and Movement left as NWN's", ee::kGetUseMonkAbilities);

  const bool defense = g_use_monk_abilities &&
                       HookExport("CNWSCreatureStats::GetACNaturalBase (Unarmored Defense: no Monk level / 5)",
                                  ee::kGetACNaturalBase, reinterpret_cast<void*>(&AcNaturalBase),
                                  reinterpret_cast<void**>(&g_ac_natural_base));

  bool movement = false;
  if (g_use_monk_abilities && HookMovementRate()) {
    g_movement_code = FunctionAt(EngineExport(ee::kGetMovementRateFactor));
    const size_t asks = FindCallsTo(g_movement_code, EngineExport(ee::kHasFeat)).size();
    if (asks == 2) {
      movement = AddFeatFilter(&MonkSpeedFeat) && AddMovementRateBonus(&UnarmoredMovement);
    } else {
      Log("SKIP monk: Unarmored Movement - %zu HasFeat calls in GetMovementRateFactor (2 expected)", asks);
    }
  }

  bool empowered = false;
  if (HookExport("CNWSCreature::CalculateDamagePower (Empowered Strikes: whose unarmed strike)",
                 ee::kCalculateDamagePower, reinterpret_cast<void*>(&CalculateDamagePower),
                 reinterpret_cast<void**>(&g_damage_power))) {
    empowered = AddFeatFilter(&EmpoweredStrikesFeat) &&
                AnswerRulesetInt("KI_STRIKE_LEVEL_1", "Empowered Strikes: unarmed strikes at +20 for Monk 6+ characters",
                                 &KiStrikeLevel1);
  }

  const bool die = HookExport("CNWSCreatureStats::GetUnarmedDamageDie (Martial Arts die)", ee::kGetUnarmedDamageDie,
                              reinterpret_cast<void*>(&UnarmedDamageDie), reinterpret_cast<void**>(&g_unarmed_die)) &&
                   HookExport("CNWSCreatureStats::GetUnarmedDamageDice (Martial Arts: one die)",
                              ee::kGetUnarmedDamageDice, reinterpret_cast<void*>(&UnarmedDamageDice),
                              reinterpret_cast<void**>(&g_unarmed_dice));
  g_rounds_counted = CountCombatRounds();
  // Part 3: Monk's Focus.
  g_special_attack_code = FunctionAt(EngineExport(ee::kResolveMeleeSpecialAttack));
  if (FindCallsTo(g_special_attack_code, EngineExport(ee::kDecrementFeatRemainingUses)).empty()) {
    Log("monk: no feat-use decrement in ResolveMeleeSpecialAttack - Stunning Strike spends a point on a miss too");
  }
  RegisterRecords({kVarMetabolismUsed});
  const bool focus = AddFeatUsesProvider(kVarFocusSpent, &FocusUsesTotal, &FocusUsesCost, &FocusUsesSpend);
  AddShortRestListener(&FocusShortRest);
  AddRestListener(&MetabolismLongRest);
  AddFeatFilter(&FocusFeatFilter);
  AddFeatUseGuard(&StunningStrikeGuard);
  g_armed_check_return = FindArmedCheck();
  if (!g_armed_check_return) {
    Log("SKIP monk: Stunning Strike with a Monk weapon - UseFeat's \"while armed\" check not found (unarmed only)");
  } else if (!HookExport("CNWSInventory::GetItemInSlot (M9: Stunning Strike with a Monk weapon)", ee::kGetItemInSlot,
                         reinterpret_cast<void*>(&GetItemInSlotHook), reinterpret_cast<void**>(&g_get_item_in_slot))) {
    g_armed_check_return = 0;
  }
  AnswerRulesetInt("STUNNING_FIST_TO_HIT_MODIFIER", "Stunning Strike: no -4", [](int) { return 0; });
  AnswerRulesetInt("STUNNING_FIST_DAMAGE_MODIFIER", "Stunning Strike: no damage penalty", [](int) { return 0; });
  AnswerRulesetInt("FLURRY_OF_BLOWS_TO_HIT_MODIFIER", "Flurry of Blows: no -2", [](int) { return 0; });
  const bool feats = AddScriptHandler("n5_patient", &PatientDefenseScript) &&
                     AddScriptHandler("n5_stepwind", &StepOfTheWindScript) &&
                     AddScriptHandler("n5_superior", &SuperiorDefenseScript);
  const bool resistance = AddResistanceProvider(&SuperiorDefenseResistance);
  AddChatCommand("!focus", &FocusCommand);
  AddHudPool(&FocusHudPool);
  Log("monk: part 3 - %s; Flurry of Blows a point a round; Stunning Strike (a point on a hit, the Focus DC); "
      "Uncanny Metabolism / Perfect Focus at initiative; Disciplined Survivor; %s%s",
      focus ? "Focus points (X6 pool)" : "Focus points NOT kept (no feat-use hooks)",
      feats ? "Patient Defense, Step of the Wind, Superior Defense" : "the Focus feats' scripts NOT hooked",
      resistance ? "" : " (Superior Defense gives NO resistance: no DoDamageImmunity hook)");
  Log("monk: part 2 - Martial Arts: %s; Dex for attacks, damage and the Shove / Grapple DC; Monk weapons roll the "
      "die when higher; a bonus strike each round (%s)",
      die ? "the Martial Arts die unarmed" : "unarmed dice NOT converted",
      g_rounds_counted ? "once a round" : "rounds NOT counted: an equip mid-round may add it twice");
  Log("monk: part 1 - X1 (NWN-only Monk feats removed); %s; %s; %s; Evasion at 7; Body and Mind at 20",
      defense ? "Unarmored Defense 10 + Dex + Wis (no Monk level / 5)" : "Unarmored Defense NOT converted",
      movement ? "Unarmored Movement +10 to +30 ft" : "Unarmored Movement NOT converted",
      empowered ? "Empowered Strikes from 6 (through DR X/+N)" : "Empowered Strikes NOT converted");
}
