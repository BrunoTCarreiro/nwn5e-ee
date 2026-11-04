// #36 Part 1 (owner, 2026-11-04: "go with your recommendations and note down the upcasting thing so you don't forget when doing 39" - 1A
// 2A 3A 4A; docs/PLAN.md "Phase 4"), key `spellcasting` faithful: the 2024 spell slots.
// - Slots: classes.2da's SpellGainTable the 2024 tables (data/files/cls_spgn_5e*.2da; data/rows/classes.if_spells2024.txt,
//   served for "spells2024"), and CNWSCreatureStats::GetSpellGainWithBonus answers rules::SpellGain2024 for a caster
//   class's slot - none of the engine's 3e extras: the bonus spells of a high casting score ((modifier - spell level +
//   4) / 4), the domain slot, prestige classes' caster levels.
// - 1A: Cleric, Druid, Paladin and Ranger cast spontaneously (MemorizesSpells 0, every class spell known): any slot of a
//   spell's level casts any of the class's spells - their prepared count comes with Part 3's window. Paladin and Ranger
//   from level 1, the Paladin on Charisma. Bard and Sorcerer keep their known spells; the Wizard memorises (its
//   spellbook and prepared list: Part 3).
// - Cantrips at will (0.74.1: owner, "SL-3 cantrips run out"). A cast spends in CNWSCreatureStats::UnReadySpell (entered
//   from CNWSCreature::DecrementSpellReadyCount): a spontaneous caster's use is a byte per spell level that it decrements
//   inline, so the hook puts a cantrip's back; a memorised spell is unreadied through the ClassInfo's
//   SetMemorizedSpellInSlotReady, which leaves a cantrip ready. DecrementSpellsPerDayLeft (a script's
//   DecrementRemainingSpellUses) spends nothing at level 0 either. GetSpellMinAbilityMet: no 3e minimum score (10 + the
//   spell's level).
// Part 2: upcasting and the combined multiclass pool (2A, 4A); Part 3: the Prepare spells window (3A).

#include <stdint.h>

#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"
#include "rules/spell_slots.h"

namespace {

using GainFn = uint8_t (*)(void* stats, uint8_t slot, uint8_t spell_level);
using MinAbilityFn = int (*)(void* stats, uint8_t slot, uint8_t spell_level);
using DecrementFn = void (*)(void* stats, uint8_t slot, uint8_t spell_level);
using UnReadyFn = int (*)(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic);
using SetReadyFn = void (*)(void* class_info, uint8_t spell_level, uint8_t index, int ready);
constexpr const char* kGain = "?GetSpellGainWithBonus@CNWSCreatureStats@@QEAAEEE@Z";
constexpr const char* kMinAbility = "?GetSpellMinAbilityMet@CNWSCreatureStats@@QEAAHEE@Z";
constexpr const char* kDecrement = "?DecrementSpellsPerDayLeft@CNWSCreatureStats@@QEAAXEE@Z";
constexpr const char* kUnReady = "?UnReadySpell@CNWSCreatureStats@@QEAAHIEEE@Z";
constexpr const char* kSetReady = "?SetMemorizedSpellInSlotReady@CNWSCreatureStats_ClassInfo@@QEAAXEEH@Z";
GainFn g_gain = nullptr;
MinAbilityFn g_min_ability = nullptr;
DecrementFn g_decrement = nullptr;
UnReadyFn g_unready = nullptr;
SetReadyFn g_set_ready = nullptr;
LogOnce g_gain_logged(400);  // a player character's slots, once per class, level and spell level
int g_cast_lines = 0;

bool LogCast() { return g_cast_lines < 100 && ++g_cast_lines; }

bool IsPlayer(void* stats) {
  const void* creature = CreatureOf(stats);
  return creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0;
}

// The class row and level in a class slot - true for a 2024 caster class (rules::CasterKindOfClass).
bool CasterSlot(void* stats, uint8_t slot, int* row, int* level) {
  if (!stats) return false;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  if (slot >= slots.size()) return false;
  *row = slots[slot].nwn_class_id;
  *level = slots[slot].levels;
  return rules::CasterKindOfClass(*row) != rules::CasterKind::kNone;
}

uint8_t Gain(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_gain(stats, slot, spell_level);
  const int gain = rules::SpellGain2024(row, level, spell_level);
  if (IsPlayer(stats) && g_gain_logged.First(LogOnce::Key(ObjectIdOf(stats), row * 100 + level, spell_level))) {
    Log("spells: object 0x%08X class %d level %d, spell level %d: %d (2024; NWN's %d)", ObjectIdOf(stats), row, level,
        spell_level, gain, g_gain(stats, slot, spell_level));
  }
  return static_cast<uint8_t>(gain);
}

int MinAbility(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  return CasterSlot(stats, slot, &row, &level) ? 1 : g_min_ability(stats, slot, spell_level);
}

void Decrement(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  if (CasterSlot(stats, slot, &row, &level) && !rules::CastSpendsUse(true, spell_level)) {
    if (LogCast()) Log("spells: object 0x%08X lost a cantrip use (class %d) - at will, none spent", ObjectIdOf(stats), row);
    return;
  }
  g_decrement(stats, slot, spell_level);
}

// A spontaneous caster's uses left at this spell level (ClassInfo's per-level bytes).
uint8_t* UsesLeft(void* stats, uint8_t slot, int spell_level) {
  return static_cast<uint8_t*>(stats) + ee::kStatsClassInfo + slot * ee::kClassInfoSize + ee::kClassInfoSpellsLeft +
         spell_level;
}

int UnReady(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_unready(stats, spell_id, slot, domain_level, metamagic);
  uint8_t* cantrips = UsesLeft(stats, slot, 0);
  const uint8_t before = *cantrips;
  const int result = g_unready(stats, spell_id, slot, domain_level, metamagic);
  if (*cantrips != before && !rules::CastSpendsUse(true, 0)) {
    *cantrips = before;
    if (LogCast()) {
      Log("spells: object 0x%08X cast cantrip %u (class %d) - at will, no use spent", ObjectIdOf(stats), spell_id, row);
    }
  }
  return result;
}

void SetReady(void* class_info, uint8_t spell_level, uint8_t index, int ready) {
  if (!ready && class_info && !rules::CastSpendsUse(true, spell_level) &&
      rules::CasterKindOfClass(Field<uint8_t>(class_info, ee::kClassInfoClass)) != rules::CasterKind::kNone) {
    if (LogCast()) Log("spells: a memorised cantrip (slot %d) stays ready - at will", index);
    return;
  }
  g_set_ready(class_info, spell_level, index, ready);
}

}  // namespace

void InstallSpellSlots() {
  if (g_config.spellcasting != rules::RuleVariant::kFaithful) {
    Log("spells: NWN's slots and memorising (key spellcasting)");
    return;
  }
  ServeConditionalRows("spells2024");  // classes.2da: the 2024 slot tables, spontaneous casters, Paladin / Ranger 1
  const bool gain = HookExport("CNWSCreatureStats::GetSpellGainWithBonus (2024 slots, #36)", kGain,
                               reinterpret_cast<void*>(&Gain), reinterpret_cast<void**>(&g_gain));
  const bool min = HookExport("CNWSCreatureStats::GetSpellMinAbilityMet (no 3e minimum score, #36)", kMinAbility,
                              reinterpret_cast<void*>(&MinAbility), reinterpret_cast<void**>(&g_min_ability));
  const bool spend = HookExport("CNWSCreatureStats::DecrementSpellsPerDayLeft (a script's cantrip use, #36)", kDecrement,
                                reinterpret_cast<void*>(&Decrement), reinterpret_cast<void**>(&g_decrement));
  const bool cast = HookExport("CNWSCreatureStats::UnReadySpell (a cast cantrip spends no use, #36)", kUnReady,
                               reinterpret_cast<void*>(&UnReady), reinterpret_cast<void**>(&g_unready));
  const bool ready =
      HookExport("CNWSCreatureStats_ClassInfo::SetMemorizedSpellInSlotReady (a memorised cantrip stays ready, #36)",
                 kSetReady, reinterpret_cast<void*>(&SetReady), reinterpret_cast<void**>(&g_set_ready));
  Log("spells: 2024 slots (SRD 5.2's tables; no bonus spells, no 3e minimum score); Cleric, Druid, Paladin and Ranger "
      "cast any class spell with any slot of its level; Paladin and Ranger from level 1; cantrips at will (#36 Part 1)%s",
      gain && min && spend && cast && ready ? "" : " - SOME hooks missing");
}
