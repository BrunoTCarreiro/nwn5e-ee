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
// Monsters keep NWN's Monk.

#include <windows.h>

#include <stdint.h>

#include <algorithm>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "movement_rate.h"
#include "ruleset.h"
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

// (c) After InitializeNumberOfAttacks (rules_two_weapon.cpp): the bonus strike.
void MartialArtsBonusStrike(void* round) {
  void* creature = round ? Field<void*>(round, ee::kRoundOwner) : nullptr;
  const int level = MartialArtsLevel(creature);
  if (level <= 0) return;
  const int off_hand = Field<int32_t>(round, ee::kRoundOffHandAttacks);
  const bool flurry = Field<uint8_t>(creature, ee::kCreatureCombatMode) == ee::kCombatModeFlurry;
  const int extra = rules::MartialArtsBonusAttacks(true, off_hand, flurry);
  const uint32_t id = IdOf(creature);
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(id, extra * 4 + (off_hand > 0 ? 1 : 0) + (flurry ? 2 : 0), 7))) {
    Log("monk: 0x%08X Martial Arts bonus strike: %s", id,
        extra ? "one more attack this round" : off_hand > 0 ? "none (an off-hand attack this round)"
                                                            : "none (NWN's Flurry of Blows adds its own)");
  }
  if (extra) SetField<int32_t>(round, ee::kRoundExtraAttacks, Field<int32_t>(round, ee::kRoundExtraAttacks) + extra);
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
  Log("monk: part 2 - Martial Arts: %s; Dex for attacks, damage and the Shove / Grapple DC; Monk weapons roll the "
      "die when higher; a bonus strike each round",
      die ? "the Martial Arts die unarmed" : "unarmed dice NOT converted");
  Log("monk: part 1 - X1 (NWN-only Monk feats removed); %s; %s; %s; Evasion at 7; Body and Mind at 20",
      defense ? "Unarmored Defense 10 + Dex + Wis (no Monk level / 5)" : "Unarmored Defense NOT converted",
      movement ? "Unarmored Movement +10 to +30 ft" : "Unarmored Movement NOT converted",
      empowered ? "Empowered Strikes from 6 (through DR X/+N)" : "Empowered Strikes NOT converted");
}
