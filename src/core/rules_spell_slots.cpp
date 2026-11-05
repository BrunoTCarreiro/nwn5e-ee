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
// - 0.74.2 (owner, 2026-11-04: "SL-2 should not be allowed to queue spells a larger number of times than the slots
//   available" / "SL-3 cantrips still show a number os uses even though they never go down"):
//   - A cast order beyond the slots left is refused with the engine's own "no uses" feedback
//     (action_guard's spell order guard): AddCastSpellActions checks only the uses left now, and a use is spent
//     when casting starts, so the casts already queued from the same slots count (the running one too, until it
//     spends its use).
//   - A cantrip's button shows no number: the client's uses getter (not exported, a byte pattern) marks a
//     cantrip, and its caller's next CExoString::ToString - the number it writes - comes back empty.
// Part 2: upcasting and the combined multiclass pool (2A, 4A); Part 3: the Prepare spells window (3A).

#include <intrin.h>
#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "action_guard.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "prepare_spells.h"
#include "rules/nwn_conversion.h"
#include "rules/spell_lists.h"
#include "rules/spell_slots.h"

namespace {

using GainFn = uint8_t (*)(void* stats, uint8_t slot, uint8_t spell_level);
using MinAbilityFn = int (*)(void* stats, uint8_t slot, uint8_t spell_level);
using DecrementFn = void (*)(void* stats, uint8_t slot, uint8_t spell_level);
using UnReadyFn = int (*)(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic);
using SetReadyFn = void (*)(void* class_info, uint8_t spell_level, uint8_t index, int ready);
using UsesLeftFn = uint8_t (*)(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic);
using GetSpellFn = void* (*)(const void* spells, int id);
using SpellLevelFn = uint8_t (*)(void* spell, uint8_t nwn_class);
using MetaMagicCostFn = uint8_t (*)(void* rules_object, uint8_t metamagic);
using FeedbackFn = void (*)(void* creature, uint16_t id, void* data, void* player);
using ClientUsesFn = uint8_t (*)(void* client_stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level,
                                 uint8_t metamagic);
using ToStringFn = void* (*)(void* out, int value);  // static CExoString CExoString::ToString(int)
using KnownPerLevelFn = uint8_t (*)(void* klass, uint8_t class_level, uint8_t spell_level, uint8_t nwn_class,
                                    uint16_t race, uint8_t ability);
using ReadSpellsFn = void (*)(void* stats, void* gff, void* gff_struct, int flag);
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
UsesLeftFn g_uses_left = nullptr;
GetSpellFn g_get_spell = nullptr;
SpellLevelFn g_spell_level = nullptr;
MetaMagicCostFn g_metamagic_cost = nullptr;
FeedbackFn g_feedback = nullptr;
ClientUsesFn g_client_uses = nullptr;
ToStringFn g_to_string = nullptr;
// The client's uses getter called from (its return address) -> that caller's next ToString (its return address).
std::vector<std::pair<uintptr_t, uintptr_t>> g_count_sites;
thread_local uintptr_t t_blank_at = 0;  // the ToString whose number is a cantrip's uses
KnownPerLevelFn g_known_per_level = nullptr;
ReadSpellsFn g_read_spells = nullptr;
thread_local bool t_loading_spells = false;  // a saved known list loads whole
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

// 0.75.1: only a real spell level - CNWSCreatureStats::LevelUpAutomatic asks with a package spell's level for the
// class, 0xFF when it's off the list, and on a yes adds it to its per-level lists at that index (out of bounds:
// the crash of !test spellcleric, once the Cleric's list became a restricted known list).
int MinAbility(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_min_ability(stats, slot, spell_level);
  return rules::SpellLevelMet2024(spell_level) ? 1 : 0;
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
  if (result) PrepareSpellsNoteCast(CreatureOf(stats));  // #36 Part 3: a Long Rest's change closes
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

void* RulesObject() {
  static void** const global = reinterpret_cast<void**>(EngineExport(ee::kRules));
  return global ? *global : nullptr;
}

// The level of the slots a cast draws on for a class (classes.2da row): the spell's level for it plus its
// metamagic's cost; -1 off the class's list (or unknown).
int SlotLevel(int nwn_class, uint32_t spell_id, int metamagic) {
  void* rules_object = RulesObject();
  if (!rules_object || !g_get_spell || !g_spell_level || !g_metamagic_cost) return -1;
  void* spells = Field<void*>(rules_object, ee::kRulesSpells);
  void* spell = spells ? g_get_spell(spells, static_cast<int>(spell_id)) : nullptr;
  if (!spell) return -1;
  const int level = g_spell_level(spell, static_cast<uint8_t>(nwn_class));
  if (level == 0xFF) return -1;
  return level + (metamagic ? g_metamagic_cost(rules_object, static_cast<uint8_t>(metamagic)) : 0);
}

bool Memorizes(int nwn_class) {
  void* rules_object = RulesObject();
  const uint8_t* classes = rules_object ? Field<const uint8_t*>(rules_object, ee::kRulesClasses) : nullptr;
  return classes && Field<int32_t>(classes + nwn_class * ee::kClassStride, ee::kClassMemorizesSpells) != 0;
}

// SL-2: an order to cast beyond the slots left is refused - the casts already queued from the same slots count, the
// running one (the queue's first node between ticks) until it has spent its use.
bool SlotsSpokenFor(void* creature, const SpellOrder& order) {
  if (!creature || order.fake || order.spontaneous || order.domain_level != 0 || order.class_slot < 0 ||
      order.class_slot >= ee::kMaxClassSlots || !g_uses_left) {
    return false;
  }
  void* stats = StatsOf(creature);
  const uint8_t slot = static_cast<uint8_t>(order.class_slot);
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return false;
  const rules::QueuedCast wanted{order.spell, order.metamagic, SlotLevel(row, order.spell, order.metamagic)};
  if (wanted.slot_level < 0 || !rules::CastSpendsUse(true, wanted.slot_level)) return false;  // a cantrip: at will
  const bool memorizes = Memorizes(row);
  const int left = g_uses_left(stats, order.spell, slot, 0, static_cast<uint8_t>(order.metamagic));
  const bool running_spent = Field<int32_t>(creature, ee::kCreatureCastUseSpent) != 0;
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  int queued = 0;
  bool first = true;
  for (const void* node = list ? Field<void*>(list, ee::kListHead) : nullptr; node;
       node = Field<void*>(node, ee::kListNodeNext)) {
    const void* action = Field<void*>(node, ee::kListNodeData);
    const bool running = first;
    first = false;
    if (!action || Field<int32_t>(action, ee::kActionId) != ee::kActionCastSpell) continue;
    if (running && running_spent) continue;  // under way, its use already spent
    if ((Field<uint32_t>(action, ee::kCastFlags) & ee::kCastFlagFake) != 0) continue;
    if (Field<uint8_t>(action, ee::kCastClassSlot) != slot || Field<int32_t>(action, ee::kCastDomainLevel) != 0) continue;
    const uint32_t spell = Field<uint32_t>(action, ee::kCastSpell);
    const int metamagic = Field<int32_t>(action, ee::kCastMetamagic);
    const rules::QueuedCast queued_cast{spell, metamagic, memorizes ? 0 : SlotLevel(row, spell, metamagic)};
    if (rules::DrawsOnSameSlots(memorizes, wanted, queued_cast)) ++queued;
  }
  if (!rules::CastQueueFull(true, wanted.slot_level, queued, left)) return false;
  if (IsPlayer(stats) && g_feedback) g_feedback(creature, ee::kFeedbackNoSpellUses, nullptr, nullptr);
  if (LogCast()) {
    Log("spells: object 0x%08X spell %u (level %d) not queued - %d already queued from its slots, %d left",
        ObjectIdOf(stats), order.spell, wanted.slot_level, queued, left);
  }
  return true;
}

// SL-3: the client's uses getter - a cantrip of a 2024 caster class marks its caller's next number to come back empty.
uint8_t ClientUsesLeft(void* client_stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  const uint8_t uses = g_client_uses(client_stats, spell_id, slot, domain_level, metamagic);
  t_blank_at = 0;
  if (!client_stats || domain_level != 0 || metamagic != 0 || slot >= ee::kClientStatsClassSlots ||
      slot >= Field<uint8_t>(client_stats, ee::kClientStatsClassCount)) {
    return uses;
  }
  const int row = Field<uint8_t>(client_stats, ee::kClientStatsClassSlotClass + slot * ee::kClientStatsClassSlotSize);
  if (rules::CasterKindOfClass(row) == rules::CasterKind::kNone) return uses;
  const int spell_level = SlotLevel(row, spell_id, 0);
  if (spell_level < 0 || rules::CastSpendsUse(true, spell_level)) return uses;
  const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (const auto& site : g_count_sites) {
    if (site.first == from) t_blank_at = site.second;
  }
  return uses;
}

void* ToString(void* out, int value) {
  void* result = g_to_string(out, value);
  if (t_blank_at && reinterpret_cast<uintptr_t>(_ReturnAddress()) == t_blank_at) {
    t_blank_at = 0;
    char* text = result ? Field<char*>(result, 0) : nullptr;
    if (text) text[0] = '\0';
  }
  return result;
}

// Part 3: the classes.2da row of a CNWClass (rules classes +0x110, 0x530 each), -1 if it isn't one.
int ClassRowOf(const void* klass) {
  void* rules_object = RulesObject();
  const uint8_t* classes = rules_object ? Field<const uint8_t*>(rules_object, ee::kRulesClasses) : nullptr;
  if (!classes || !klass) return -1;
  const ptrdiff_t offset = static_cast<const uint8_t*>(klass) - classes;
  const int count = Field<uint8_t>(rules_object, ee::kRulesClassCount);
  if (offset < 0 || offset % static_cast<ptrdiff_t>(ee::kClassStride) != 0) return -1;
  const int row = static_cast<int>(offset / static_cast<ptrdiff_t>(ee::kClassStride));
  return row < count ? row : -1;
}

// Part 3: a 2024 caster's known list is its cantrips and prepared spells - the cap 2024's counts (the Prepare
// spells window holds the prepared total); none while a saved list loads.
uint8_t KnownPerLevel(void* klass, uint8_t class_level, uint8_t spell_level, uint8_t nwn_class, uint16_t race,
                      uint8_t ability) {
  const int cap = rules::KnownSpellsCap2024(ClassRowOf(klass), class_level, spell_level);
  if (cap < 0) return g_known_per_level(klass, class_level, spell_level, nwn_class, race, ability);
  if (t_loading_spells) return 0xFF;
  return static_cast<uint8_t>(cap);
}

void ReadSpells(void* stats, void* gff, void* gff_struct, int flag) {
  t_loading_spells = true;
  g_read_spells(stats, gff, gff_struct, flag);
  t_loading_spells = false;
}

bool HideCantripCounts() {
  const uintptr_t inside = FindPattern("the client's spell uses getter (#36 SL-3)", ee::kClientSpellUsesLeftPattern);
  const uintptr_t getter = inside ? FunctionEntry(inside) : 0;
  const uintptr_t to_string = EngineExport(ee::kExoStringToString);
  if (!getter || !to_string) return false;
  for (uintptr_t site : FindCallsAnywhere(getter)) {
    for (uintptr_t call : FindCallsTo(FunctionAt(site), to_string)) {
      if (call <= site) continue;
      g_count_sites.push_back({site + 5, call + 5});  // E8 rel32: the return address is 5 bytes on
      break;
    }
  }
  if (g_count_sites.empty()) return false;
  return HookAddress("client CNWCCreatureStats::GetSpellUsesLeft (a cantrip's count hidden, #36)", getter,
                     reinterpret_cast<void*>(&ClientUsesLeft), reinterpret_cast<void**>(&g_client_uses)) &&
         HookExport("CExoString::ToString (a cantrip's count hidden, #36)", ee::kExoStringToString,
                    reinterpret_cast<void*>(&ToString), reinterpret_cast<void**>(&g_to_string));
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
  g_uses_left = reinterpret_cast<UsesLeftFn>(EngineExport(ee::kGetSpellUsesLeft));
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  g_spell_level = reinterpret_cast<SpellLevelFn>(EngineExport(ee::kSpellGetLevel));
  g_metamagic_cost = reinterpret_cast<MetaMagicCostFn>(EngineExport(ee::kGetMetaMagicLevelCost));
  g_feedback = reinterpret_cast<FeedbackFn>(EngineExport(ee::kCreatureSendFeedbackMessage));
  const bool queue = g_uses_left && g_get_spell && g_spell_level && g_metamagic_cost && g_feedback &&
                     AddSpellOrderGuard(&SlotsSpokenFor);
  Log(queue ? "spells: a cast order beyond the slots left is refused (the queued casts count; #36 SL-2)"
            : "SKIP spells #36 SL-2: cast orders aren't checked against the slots left");
  const bool lists = HookExport("CNWClass::GetSpellsKnownPerLevel (2024 cantrips and prepared spells, #36)",
                                ee::kGetSpellsKnownPerLevel, reinterpret_cast<void*>(&KnownPerLevel),
                                reinterpret_cast<void**>(&g_known_per_level)) &&
                     HookExport("CNWSCreatureStats::ReadSpellsFromGff (a saved list loads whole, #36)",
                                ee::kReadSpellsFromGff, reinterpret_cast<void*>(&ReadSpells),
                                reinterpret_cast<void**>(&g_read_spells));
  Log(lists ? "spells: a caster's known list is its cantrips and prepared spells (2024 counts; #36 Part 3)"
            : "SKIP spells #36 Part 3: the known lists keep NWN's counts");
  const bool counts = HideCantripCounts();
  Log(counts ? "spells: a cantrip's button shows no uses number (%zu places; #36 SL-3)"
             : "SKIP spells #36 SL-3: a cantrip's button keeps its uses number (%zu places found)",
      g_count_sites.size());
  Log("spells: 2024 slots (SRD 5.2's tables; no bonus spells, no 3e minimum score); Cleric, Druid, Paladin and Ranger "
      "cast any class spell with any slot of its level; Paladin and Ranger from level 1; cantrips at will (#36 Part 1)%s",
      gain && min && spend && cast && ready ? "" : " - SOME hooks missing");
}
