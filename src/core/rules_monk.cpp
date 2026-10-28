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
//   (+0x788 = 1: it returns +0x78c, no class bonus).
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
// Monsters keep NWN's Monk.

#include <windows.h>

#include <stdint.h>

#include <algorithm>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "movement_rate.h"
#include "ruleset.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"

namespace {

constexpr int kNwnClassMonk = 5;
constexpr float kMonkSpeedCap = 3.0f;  // ruleset MOVEMENT_SPEED_BONUS_MONK_CAP's default

using AcNaturalBaseFn = char (*)(void* stats, int monk_part_only);
using UseMonkAbilitiesFn = int (*)(void* creature);
using DamagePowerFn = uint8_t (*)(void* creature, void* target, int offhand);

AcNaturalBaseFn g_ac_natural_base = nullptr;
UseMonkAbilitiesFn g_use_monk_abilities = nullptr;
DamagePowerFn g_damage_power = nullptr;
CodeRange g_movement_code;           // GetMovementRateFactor: its HasFeat(207)
thread_local void* t_powering = nullptr;  // the creature CalculateDamagePower is working for
LogOnce g_logged(500);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

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

  Log("monk: part 1 - X1 (NWN-only Monk feats removed); %s; %s; %s; Evasion at 7; Body and Mind at 20",
      defense ? "Unarmored Defense 10 + Dex + Wis (no Monk level / 5)" : "Unarmored Defense NOT converted",
      movement ? "Unarmored Movement +10 to +30 ft" : "Unarmored Movement NOT converted",
      empowered ? "Empowered Strikes from 6 (through DR X/+N)" : "Empowered Strikes NOT converted");
}
