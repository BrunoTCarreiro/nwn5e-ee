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
// Part 2 (0.50.0):
// - R8 Evasion: nwn5e grants NWN's Improved Evasion (212) at Rogue 7
//   (rules_features), and a HasFeat filter answers it for characters by the
//   2024 rule: Rogue 7+, not Incapacitated, and - when a save was just rolled -
//   only if it rolled Dexterity (rules_save_roll's RecentSaveAbility: C1 moves
//   some Reflex saves to Constitution - Cone of Cold, Sunbeam, Sunburst). The
//   engine's Reflex-damage command (ExecuteCommandGetReflexAdjustedDamage
//   0x52f790: the script's GetReflexAdjustedDamage) asks for 212 right after
//   its save: none on a success, half on a failure. Faithful's served feat row
//   names it "Evasion" (data/rows/feat.if_classfeat2024.txt).
// - R10 Slippery Mind (Rogue 15): Wisdom and Charisma save proficiency
//   (rules_saves.cpp, rules::ClassFeatureSaveProficiency).
// - R11 Elusive (Rogue 18): no attack roll against it has Advantage unless it
//   is Incapacitated (rules_advantage.cpp: weapon and spell attack rolls).
// Parts 3-4 (0.50.0):
// - R7 Uncanny Dodge (Rogue 5): after CNWSCreature::ResolveDamage put a hit's
//   damage in the attacker's attack data (resolve_damage.cpp; int16 per type at
//   +0x16), the Rogue it hits - seeing the attacker, not Incapacitated - halves
//   it before ResolvePost*Damage makes the damage effect (before resistances,
//   as 2024 orders it), spending its round's reaction: the round's AoO count
//   (+0x2f40, decremented - no opportunity attack or Retaliation after it).
// - R9 Reliable Talent (Rogue 7): rules_skills' ReliableTalentRoll at the d20s
//   nwn5e rolls - the scripts' GetIsSkillSuccessful, trap searching, a
//   searching observer's Perception (and since 0.35.0 hiding's Stealth).
// - R12 Stroke of Luck (Rogue 20): rules_save_roll rolls the first failed save
//   since a Short or Long Rest again with its d20 a 20 (record nwn5e_stroke_used).
// NWN mode: NWN's sneak attack, Evasion and Improved Evasion.

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
#include "chat.h"
#include "dice.h"
#include "effects.h"
#include "records.h"
#include "resolve_damage.h"
#include "rest.h"
#include "feats.h"
#include "saves.h"
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

// A call from the game's own code (nwmain.exe), not nwn5e's: nwn5e's feature
// sync must see the real feat list to grant Improved Evasion.
bool FromGame(uintptr_t caller) {
  static uintptr_t begin = 0, end = 0;
  if (!begin) {
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    begin = base;
    end = base + nt->OptionalHeader.SizeOfImage;
  }
  return caller >= begin && caller < end;
}

// R8: Improved Evasion (212) as the 2024 Evasion, for characters - the game's asks.
bool EvasionFilter(void* stats, int feat, uintptr_t caller, int* answer) {
  if (feat != rules::kFeatImprovedEvasion || !Faithful() || !stats || !FromGame(caller)) return false;
  if (!ReadCreatureFacts(stats).character) return false;  // monsters: NWN's
  void* creature = CreatureOf(stats);
  bool has = rules::HasRogueEvasion(RogueLevel(stats));
  if (has && creature && IncapacitatedByCondition(creature)) has = false;
  if (has && creature) {
    const std::optional<rules::Ability> ability = RecentSaveAbility(creature);
    if (ability && *ability != rules::Ability::kDexterity) has = false;
  }
  *answer = has ? 1 : 0;
  return true;
}

// R7 Uncanny Dodge: a hit on a Rogue 5+ halved - its reaction.
void UncannyDodge(void* attacker, void* target) {
  if (!Faithful() || !attacker || !target) return;
  void* rogue = CreatureById(IdOf(target));
  void* stats = rogue ? StatsOf(rogue) : nullptr;
  if (!stats || !ReadCreatureFacts(stats).character || !rules::HasUncannyDodge(RogueLevel(stats))) return;
  uint8_t* data = CurrentAttackData(attacker);
  if (!data) return;
  const uint8_t result = data[ee::kAttackResult];
  if (result != 1 && result != 3) return;  // a hit or a critical hit
  auto* entries = reinterpret_cast<int16_t*>(data + ee::kAttackDamageEntries);
  int total = 0;
  for (int i = 0; i < ee::kAttackDamageEntryCount; ++i) total += entries[i] > 0 ? entries[i] : 0;
  if (total <= 0) return;
  const uint32_t rogue_id = IdOf(rogue);
  const uint32_t attacker_id = IdOf(attacker);
  if (engine::IsDeadOrDying(rogue) || IncapacitatedByCondition(rogue) || !engine::Sees(rogue, attacker_id)) return;
  void* round = Field<void*>(rogue, ee::kCreatureCombatRound);
  if (!round || Field<int32_t>(round, ee::kRoundOpportunitiesLeft) <= 0) {
    if (IsPlayer(rogue) && LogSlot()) Log("rogue: 0x%08X hit by 0x%08X (%d) - no Uncanny Dodge: reaction spent", rogue_id,
                                         attacker_id, total);
    return;
  }
  const int halved = rules::HalveAttackDamage(entries, ee::kAttackDamageEntryCount);
  SetField<int32_t>(round, ee::kRoundOpportunitiesLeft, Field<int32_t>(round, ee::kRoundOpportunitiesLeft) - 1);
  if (LogSlot()) Log("rogue: 0x%08X Uncanny Dodge - the hit by 0x%08X halved, %d -> %d (its reaction)", rogue_id,
                     attacker_id, total, halved);
  if (IsPlayer(rogue)) {
    TellPlayer(rogue_id, "nwn5e: Uncanny Dodge - you halve the hit (" + std::to_string(total) + " -> " +
                             std::to_string(halved) + "); your reaction this round.");
  }
}

// R12 Stroke of Luck: ready / used, restored by a Short or Long Rest.
constexpr const char* kVarStrokeUsed = "nwn5e_stroke_used";

void StrokeOfLuckLongRest(void* stats) {
  void* creature = stats ? CreatureOf(stats) : nullptr;
  if (creature && engine::GetLocalInt(creature, kVarStrokeUsed)) SetRecord(creature, kVarStrokeUsed, 0);
}

std::string StrokeOfLuckShortRest(void* creature) {
  if (!creature || !engine::GetLocalInt(creature, kVarStrokeUsed)) return std::string();
  SetRecord(creature, kVarStrokeUsed, 0);
  return "Stroke of Luck is ready again.";
}

// R1 (e): no flat-footed in 2024.
int GetFlatFooted(void* creature) { return Faithful() ? 0 : g_flat_footed(creature); }

}  // namespace

bool StrokeOfLuckReady(void* creature) {
  if (!Faithful() || !creature) return false;
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).character || !rules::HasStrokeOfLuck(RogueLevel(stats))) return false;
  EnsureRecords(creature);
  return engine::GetLocalInt(creature, kVarStrokeUsed) == 0;
}

void StrokeOfLuckUsed(void* creature, bool succeeded) {
  if (!creature) return;
  SetRecord(creature, kVarStrokeUsed, 1);
  const uint32_t id = IdOf(creature);
  Log("rogue: 0x%08X Stroke of Luck - a failed save rolled again as a 20: %s", id, succeeded ? "success" : "still fails");
  if (IsPlayer(creature)) {
    TellPlayer(id, std::string("nwn5e: Stroke of Luck - the failed save's d20 becomes a 20") +
                       (succeeded ? ": it succeeds." : ", and it still fails.") + " Again after a Short or Long Rest.");
  }
}

bool ElusiveTarget(void* creature) {
  if (!Faithful() || !creature) return false;
  void* stats = StatsOf(creature);
  return stats && ReadCreatureFacts(stats).character && rules::HasElusive(RogueLevel(stats)) &&
         !IncapacitatedByCondition(creature);
}

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
  const bool evasion = AddFeatFilter(&EvasionFilter);
  RegisterRecords({kVarStrokeUsed});
  const bool uncanny = AddResolveDamageListener(&UncannyDodge);
  AddRestListener(&StrokeOfLuckLongRest);
  AddShortRestListener(&StrokeOfLuckShortRest);
  Log("rogue: parts 3-4 - %s; Reliable Talent (hiding, searching, the scripts' skill checks); Stroke of Luck (a "
      "failed save becomes a 20, once per rest)",
      uncanny ? "Uncanny Dodge (the first seen hit a round halved, its reaction)" : "Uncanny Dodge NOT on");
  Log("rogue: part 2 - %s; Slippery Mind (Wisdom / Charisma saves at 15); Elusive (no Advantage against you at 18)",
      evasion ? "Evasion as 2024's (Dexterity saves, not Incapacitated; Improved Evasion at 7)"
              : "Evasion NOT converted (no HasFeat filter)");
  Log("rogue: part 1 - %s; %s; NWN-only rogue feats removed (X1)",
      sneak ? "Sneak Attack by the 2024 rules (Advantage or an ally next to the target, once a round, crits double)"
            : "Sneak Attack NOT converted",
      flat ? "no flat-footed" : "flat-footed kept");
}
