// Phase 3 Barbarian (docs/PLAN.md "Phase 3 — Barbarian (CM9)", decided by the
// owner 2026-10-24), with class_features faithful - part 1, the passives:
// - X1: NWN's Uncanny Dodge (195, 251-255) and Damage Reduction (196, 332-334,
//   epic 1067) have no 2024 counterpart: rules_features takes them off
//   characters, and the Barbarian's feat table served in faithful no longer
//   grants them (data/rows/cls_feat_barb.if_classfeat2024.txt; it also moves
//   Fast Movement to level 5).
// - B4 Unarmored Defense: with no armour (a chest item of base AC 0) the
//   Constitution modifier is added to AC (nothing when negative), a shield
//   allowed - GetArmorClassVersus (rules_attack.cpp) and the sheet's
//   GetArmorClass (rules_fighter.cpp), as the Defense style. A Monk's Wisdom
//   bonus, which the engine gives, isn't stacked with it (X5: the higher).
// - B5 Danger Sense (2): advantage on Dexterity saves unless Incapacitated
//   (the save d20, rules_save_roll.cpp).
// - B8 Fast Movement (5): +10 ft out of Heavy armour (base AC 6+). The engine
//   adds ruleset BARBARIAN_ENDURANCE_BONUS when HasFeat(194) holds
//   (GetMovementRateFactor +0x4A): for characters the feat is answered by
//   Barbarian level and armour, and the bonus is a third (10 ft of 30) -
//   monsters keep NWN's.
// - B9 Feral Instinct (7): advantage on initiative (rules_initiative.cpp).
// - B13 Primal Champion (20): Strength and Constitution +4, at most 25
//   (rules_abilities.cpp's 2024 scores).
// - B14 Weapon Mastery: melee weapons only (rules_mastery.cpp, the served
//   mastery rows).
// Rage and the rest come in parts 2-4. NWN mode: NWN's Barbarian.

#include <stdint.h>

#include <algorithm>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "ruleset.h"
#include "rules/class_features.h"
#include "rules/saves.h"

namespace {

constexpr int kFeatBarbarianFastMovement = 194;
constexpr int kAbilityConstitution = 2;
constexpr int kAbilityWisdom = 4;

using ComputeArmorClassFn = int (*)(void* item);
using MovementRateFn = float (*)(void* creature);

ComputeArmorClassFn g_compute_armor_class = nullptr;
MovementRateFn g_movement_rate = nullptr;
thread_local void* t_moving = nullptr;  // the creature whose movement rate the engine is computing
LogOnce g_logged(500);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// A character's (player character or henchman) class levels in faithful; 0 otherwise.
int LevelsOf(void* stats, rules::CharacterClass character_class) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, character_class);
}

int BarbarianLevelOf(void* stats) { return LevelsOf(stats, rules::CharacterClass::kBarbarian); }

// The chest item's base AC (0 with none, or clothing).
int ChestArmorClass(void* creature) {
  void* chest = creature ? engine::ItemInSlot(creature, ee::kSlotChest) : nullptr;
  return chest && g_compute_armor_class ? g_compute_armor_class(chest) : 0;
}

bool HoldsShield(void* creature) {
  switch (engine::BaseItemRowInSlot(creature, ee::kSlotLeftHand)) {
    case 14: case 56: case 57: return true;  // small, large, tower shield
    default: return false;
  }
}

// B8: feat 194 for characters - Barbarian 5+, out of Heavy armour.
bool FastMovementFilter(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if (feat != kFeatBarbarianFastMovement || !stats || !Faithful()) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return false;  // monsters: NWN's
  void* creature = CreatureOf(stats);
  *answer = creature && rules::HasBarbarianFastMovement(ClassLevel2024(stats, rules::CharacterClass::kBarbarian)) &&
            ChestArmorClass(creature) < rules::kHeavyArmorBaseAc;
  return true;
}

float GetMovementRateFactor(void* creature) {
  void* outer = t_moving;
  t_moving = creature;
  const float factor = g_movement_rate(creature);
  t_moving = outer;
  return factor;
}

float EnduranceBonus(float engine_value) {
  if (!t_moving || !Faithful()) return engine_value;
  void* stats = StatsOf(t_moving);
  if (!stats || !ReadCreatureFacts(stats).character) return engine_value;
  return rules::kBarbarianFastMovementFactor;
}

}  // namespace

// B4: the AC an unarmoured Barbarian adds (GetArmorClassVersus and the sheet).
int UnarmoredDefenseBonus(void* stats) {
  if (BarbarianLevelOf(stats) < 1) return 0;
  void* creature = CreatureOf(stats);
  if (!creature || ChestArmorClass(creature) > 0) return 0;
  // X5: a Monk without a shield already has its Wisdom (GetACNaturalBase).
  const int monk_wis = ClassLevel2024(stats, rules::CharacterClass::kMonk) > 0 && !HoldsShield(creature)
                           ? engine::AbilityModifier(stats, kAbilityWisdom)
                           : 0;
  const int bonus = rules::UnarmoredDefenseConBonus(engine::AbilityModifier(stats, kAbilityConstitution), monk_wis);
  if (bonus > 0 && g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), bonus, 1))) {
    Log("barbarian: 0x%08X Unarmored Defense: +%d AC (Constitution, no armour)", ObjectIdOf(stats), bonus);
  }
  return bonus;
}

// Advantage a class gives a creature's save on `ability` (the save d20): B5
// Danger Sense on Dexterity saves, unless Incapacitated.
int ClassSaveAdvantage(void* creature, rules::Ability ability) {
  void* stats = StatsOf(creature);
  if (ability == rules::Ability::kDexterity && rules::HasDangerSense(BarbarianLevelOf(stats)) &&
      !IncapacitatedByCondition(creature)) {
    return 1;
  }
  return 0;
}

// B9 Feral Instinct: advantage on initiative (rules_initiative.cpp).
bool HasFeralInstinctAdvantage(void* creature) {
  return creature && rules::HasFeralInstinct(BarbarianLevelOf(StatsOf(creature)));
}

void InstallBarbarianRules() {
  if (!Faithful()) return;
  ServeConditionalRows("classfeat2024");  // X1 + B8: cls_feat_barb.if_classfeat2024.txt
  g_compute_armor_class = reinterpret_cast<ComputeArmorClassFn>(EngineExport(ee::kItemComputeArmorClass));
  if (!g_compute_armor_class) Log("barbarian: no %s - Unarmored Defense and Fast Movement see no armour",
                                  ee::kItemComputeArmorClass);
  if (!AddFeatFilter(&FastMovementFilter)) Log("SKIP barbarian: Fast Movement - HasFeat not hooked");
  if (!HookExport("CNWSCreature::GetMovementRateFactor (Fast Movement: whose rate)", ee::kGetMovementRateFactor,
                  reinterpret_cast<void*>(&GetMovementRateFactor), reinterpret_cast<void**>(&g_movement_rate)) ||
      !AnswerRulesetFloat("BARBARIAN_ENDURANCE_BONUS", "Fast Movement: +10 ft (a third) for characters",
                          &EnduranceBonus)) {
    Log("barbarian: Fast Movement keeps NWN's +10%%");
  }
  Log("barbarian: X1 (no NWN Uncanny Dodge / Damage Reduction), Unarmored Defense, Danger Sense, Fast Movement, "
      "Feral Instinct, Primal Champion, melee Weapon Mastery (class_features faithful)");
}
