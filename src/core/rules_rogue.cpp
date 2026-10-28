// Phase 3 Rogue (docs/PLAN.md "Phase 3 — Rogue"; owner, 2026-10-28: "go with the
// recommendations"), class_features faithful. Part 1 (0.50.0):
// - R0: the NWN-only rogue feats go (X1, rules_features.cpp: Evasion,
//   Uncanny Dodge 1-6, Crippling Strike, Defensive Roll, Opportunist, Skill
//   Mastery, Slippery Mind, the epic sneak dice); the served cls_feat_rog no
//   longer grants Evasion or Uncanny Dodge.
// - R1 Sneak Attack (2024): CNWSCreature::ResolveSneakAttack (0x3ebe00, its one
//   caller inside ResolveAttackRoll) decides whether this attack's data gets
//   the sneak flag (+0x60) - in faithful by the 2024 test instead of NWN's
//   (unseen / flat-footed / flanked, 10 m for ranged, sneak and crit
//   immunity): a Finesse or ranged weapon, and the attack's roll mode
//   (rules_advantage) Advantage - or not Disadvantage with an ally of the
//   attacker (friendly, not dead, dying or Incapacitated) within 5 ft of the
//   target; once per the attacker's combat round (marked when the dice roll -
//   a hit); no creature is immune. The dice: CNWSCreatureStats::GetDamageRoll
//   rolls them with its 10th of 12 RollDice calls (+0x12df, count from the
//   feats, 6 sides, outside its critical loop); a dice filter there rolls
//   ceil(Rogue level / 2) d6 - Assassin and Shadowdancer levels counted as the
//   Rogue's (decision 10) - doubled on a critical hit (the roll's context from
//   rules_fighter's GetDamageRoll hook). A creature with sneak feats and no
//   Rogue levels (NWN's blueprints) keeps the engine's count, doubled on a crit.
// - R1 (e): CNWSCreature::GetFlatFooted answers "never" (2024 has no
//   flat-footed: no Dexterity lost from AC; helpless targets give Advantage).
// NWN mode: NWN's sneak attack.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "rules/class_features.h"
#include "rules/weapons.h"

bool CurrentAttackRollMode(uint32_t attacker_id, rules::RollMode* mode);  // rules_advantage.cpp
void* CurrentDamageRoll(int* critical);                                   // rules_fighter.cpp

namespace {

using ResolveSneakFn = void (*)(void* creature, void* target);
using FlatFootedFn = int (*)(void* creature);

constexpr const char* kResolveSneakAttack = "?ResolveSneakAttack@CNWSCreature@@IEAAXPEAV1@@Z";
constexpr const char* kGetFlatFooted = "?GetFlatFooted@CNWSCreature@@QEAAHXZ";
constexpr size_t kAttackSneak = 0x60;      // int32: the sneak attack flag ResolveDamage hands to GetDamageRoll
constexpr int kSpecialNoSneak1 = 0x363;    // special attacks the engine never sneaks with
constexpr int kSpecialNoSneak2 = 0x364;
constexpr size_t kGetDamageRollSneakDice = 9;  // its 10th RollDice call
constexpr size_t kGetDamageRollDiceCalls = 12;
constexpr float kFiveFeet = 1.524f;
constexpr int kLogMax = 600;

ResolveSneakFn g_resolve_sneak = nullptr;
FlatFootedFn g_flat_footed = nullptr;
uintptr_t g_sneak_dice_return = 0;
std::unordered_map<uint32_t, uint32_t> g_sneak_round;  // attacker -> the round (CombatRoundsEnded) its dice rolled
int g_logged = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_logged < kLogMax && ++g_logged; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

int RogueLevel(void* stats) { return stats ? ClassLevel2024(stats, rules::CharacterClass::kRogue) : 0; }

// Sneak Attack at all: Rogue levels, or NWN's sneak feats (blueprints).
bool HasSneakAttack(void* stats) {
  if (RogueLevel(stats) > 0) return true;
  if (engine::HasFeat(stats, rules::kFeatSneakAttack)) return true;
  for (int feat = 345; feat <= 353; ++feat) {
    if (engine::HasFeat(stats, feat)) return true;
  }
  return false;
}

struct AllySearch {
  void* attacker;
  uint32_t attacker_id;
  void* target;
  uint32_t ally;
};

void FindAlly(void* context, uint32_t id, void* creature) {
  auto* search = static_cast<AllySearch*>(context);
  if (search->ally != ee::kObjectInvalid || id == search->attacker_id) return;
  if (!engine::IsFriend(search->attacker, id) || engine::IsDeadOrDying(creature) ||
      IncapacitatedByCondition(creature)) {
    return;
  }
  if (engine::EdgeDistance(search->target, creature) <= kFiveFeet) search->ally = id;
}

// An ally of the attacker within 5 ft of the target, or kObjectInvalid.
uint32_t AllyNextTo(void* attacker, void* target) {
  AllySearch search{attacker, IdOf(attacker), target, ee::kObjectInvalid};
  engine::ForEachOtherCreatureInArea(target, &FindAlly, &search);
  return search.ally;
}

// R1: the 2024 test in place of the engine's.
void ResolveSneakAttack(void* creature, void* target) {
  if (!Faithful()) {
    g_resolve_sneak(creature, target);
    return;
  }
  uint8_t* data = creature ? CurrentAttackData(creature) : nullptr;
  void* stats = creature ? StatsOf(creature) : nullptr;
  void* victim = target ? CreatureById(IdOf(target)) : nullptr;
  if (!data || !stats || !victim) return;
  const int special = Field<uint16_t>(data, ee::kAttackSpecial);
  if (special == kSpecialNoSneak1 || special == kSpecialNoSneak2) return;
  if (!HasSneakAttack(stats)) return;
  const uint32_t id = IdOf(creature);
  const bool player = IsPlayer(creature);
  const auto used = g_sneak_round.find(id);
  if (used != g_sneak_round.end() && used->second == CombatRoundsEnded(id)) {
    if (player && LogSlot()) Log("rogue: 0x%08X no Sneak Attack on 0x%08X - used this round", id, IdOf(victim));
    return;
  }
  const bool ranged = Field<int32_t>(data, ee::kAttackRanged) != 0;
  const int row = AttackWeaponBaseItem(creature);
  const bool weapon_ok = ranged || (row >= 0 && rules::IsFinesseNwnBaseItem(row));
  rules::RollMode mode = rules::RollMode::kNormal;
  CurrentAttackRollMode(id, &mode);
  const uint32_t ally = mode == rules::RollMode::kNormal ? AllyNextTo(creature, victim) : ee::kObjectInvalid;
  if (!rules::SneakAttackApplies(weapon_ok, mode, ally != ee::kObjectInvalid)) {
    if (player && LogSlot()) {
      Log("rogue: 0x%08X no Sneak Attack on 0x%08X - %s", id, IdOf(victim),
          !weapon_ok                                    ? "not a Finesse or ranged weapon"
          : mode == rules::RollMode::kDisadvantage      ? "the roll has Disadvantage"
                                                        : "no Advantage and no ally next to it");
    }
    return;
  }
  SetField<int32_t>(data, kAttackSneak, 1);
  if (player && LogSlot()) {
    char why[64];
    if (mode == rules::RollMode::kAdvantage) {
      sprintf_s(why, "Advantage");
    } else {
      sprintf_s(why, "an ally (0x%08X) next to it", ally);
    }
    Log("rogue: 0x%08X Sneak Attack on 0x%08X - %s (%s); on a hit", id, IdOf(victim), why,
        ranged ? "ranged" : "Finesse weapon");
  }
}

// R1: the sneak dice - ceil(Rogue level / 2) d6, doubled on a critical hit.
bool SneakDice(uintptr_t caller, int count, int sides, int* result) {
  if (caller != g_sneak_dice_return || sides != 6 || !Faithful()) return false;
  int critical = 0;
  void* stats = CurrentDamageRoll(&critical);
  if (!stats) return false;
  const int level = RogueLevel(stats);
  int dice = level > 0 ? rules::SneakAttackDice2024(level) : count;
  if (critical) dice *= 2;
  if (dice <= 0) return false;
  *result = RollEngineDice(dice, 6);
  const uint32_t id = ObjectIdOf(stats);
  g_sneak_round[id] = CombatRoundsEnded(id);
  if (LogSlot()) {
    Log("rogue: 0x%08X sneak dice %dd6%s = %d (Rogue %d%s)", id, dice, critical ? " (critical: doubled)" : "", *result,
        level, level > 0 ? "" : ": the engine's count");
  }
  return true;
}

// R1 (e): no flat-footed in 2024.
int GetFlatFooted(void* creature) { return Faithful() ? 0 : g_flat_footed(creature); }

}  // namespace

void InstallRogueRules() {
  if (!Faithful()) return;
  const CodeRange damage_roll = FunctionAt(EngineExport(ee::kGetDamageRoll));
  const std::vector<uintptr_t> dice = FindCallsTo(damage_roll, EngineExport(ee::kRollDice));
  if (dice.size() != kGetDamageRollDiceCalls) {
    Log("SKIP rogue: Sneak Attack - GetDamageRoll has %zu RollDice calls (expected %zu)", dice.size(),
        kGetDamageRollDiceCalls);
    return;
  }
  g_sneak_dice_return = dice[kGetDamageRollSneakDice] + kCallLength;
  if (!CountCombatRounds()) Log("rogue: no round count - Sneak Attack is not limited to once a round");
  const bool sneak =
      HookExport("CNWSCreature::ResolveSneakAttack (the 2024 Sneak Attack test)", kResolveSneakAttack,
                 reinterpret_cast<void*>(&ResolveSneakAttack), reinterpret_cast<void**>(&g_resolve_sneak)) &&
      AddDiceFilter(&SneakDice);
  const bool flat = HookExport("CNWSCreature::GetFlatFooted (never: 2024 has no flat-footed)", kGetFlatFooted,
                               reinterpret_cast<void*>(&GetFlatFooted), reinterpret_cast<void**>(&g_flat_footed));
  Log("rogue: part 1 - %s; %s; NWN-only rogue feats removed (X1)",
      sneak ? "Sneak Attack by the 2024 rules (Advantage or an ally next to the target, once a round, crits double)"
            : "Sneak Attack NOT converted",
      flat ? "no flat-footed" : "flat-footed kept");
}
