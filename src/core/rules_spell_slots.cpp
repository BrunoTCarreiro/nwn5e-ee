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
// - Part 2a (2A, upcasting; owner, 2026-11-06: "Go with your recommendations for 1 and 4."): SRD 5.2 - a spell is
//   cast with a slot of its level or higher. With none left at its level, the lowest higher slot is spent
//   (rules::SlotToSpend):
//   - GetSpellUsesLeft (the cast's own check, the AI's, GetHasSpell) answers the casts left - the slots at the
//     spell's level and above (rules::CastsLeft) - when NWN would cast it with a slot of its level (known, its
//     metamagic feat...): asked with that level's byte set to 1 for the question if it's empty.
//   - UnReadySpell checks GetSpellUsesLeft and then decrements the slot level's byte inline, so for an upcast that
//     byte is 1 while NWN spends it and the higher slot is decremented instead.
//   - The client's uses getter (the quickbar's number, its greyed button) answers the same total from its copy.
//   - The slot level a cast spent is kept on the caster (local int N5_SLOT_LEVEL) for #39's spells - "the spell
//     assumes the higher level for that casting"; the player is told when a higher slot is spent.
//   - SL-2's queue check spends the queued casts' slots in order, as they will be (rules::CastFitsQueue).
// - Part 2b (4A, the combined multiclass pool): with two or more 2024 caster classes, the slots are SRD 5.2's
//   Multiclass Spellcaster table's at the full casters' levels plus half the Paladin and Ranger levels (rounded up).
//   NWN keeps uses per class slot, so GetSpellGainWithBonus gives every caster class the pool's table
//   (rules::SlotsForClassSlot2024; cantrips stay each class's) - a rest fills them alike - and a spend from one is
//   made in the others: the cast's (UnReadySpell) and a script's (DecrementSpellsPerDayLeft). Each class's counts
//   stay the pool's, so the casts left and the client's numbers read them as they are; SL-2's queue counts the
//   casts of every class sharing it. No class shows 0 slots in NWN's own panels.

#include <intrin.h>
#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "action_guard.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "engine.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "prepare_spells.h"
#include "spells_page.h"
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
UsesLeftFn g_uses_left = nullptr;  // the original (hooked: Part 2a's casts left)
constexpr const char* kSlotLevelVar = "N5_SLOT_LEVEL";  // the slot level the caster's last cast spent
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

// Part 2b: a creature's class slots, in order.
std::vector<rules::ClassLevel> ClassLevelsOf(void* stats) {
  std::vector<rules::ClassLevel> classes;
  if (!stats) return classes;
  for (const rules::NwnClassSlot& entry : ReadClassSlots(stats)) classes.push_back({entry.nwn_class_id, entry.levels});
  return classes;
}

uint8_t Gain(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_gain(stats, slot, spell_level);
  const int gain = rules::SlotsForClassSlot2024(ClassLevelsOf(stats), slot, spell_level);  // 4A: the pool's
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

uint8_t* UsesLeft(void* stats, uint8_t slot, int spell_level);

// Part 2b (4A): a slot of `spell_level` spent from one class sharing the pool, spent in the others too.
void MirrorSpend(void* stats, uint8_t from_slot, int spell_level) {
  if (spell_level < 1 || spell_level > 9) return;
  const std::vector<rules::ClassLevel> classes = ClassLevelsOf(stats);
  if (!rules::SharesSlotPool(classes, from_slot)) return;
  for (size_t other = 0; other < classes.size(); ++other) {
    if (other == from_slot || !rules::SharesSlotPool(classes, static_cast<int>(other))) continue;
    uint8_t* uses = UsesLeft(stats, static_cast<uint8_t>(other), spell_level);
    if (*uses > 0) --*uses;
  }
}

void Decrement(void* stats, uint8_t slot, uint8_t spell_level) {
  int row = 0, level = 0;
  const bool caster = CasterSlot(stats, slot, &row, &level);
  if (caster && !rules::CastSpendsUse(true, spell_level)) {
    if (LogCast()) Log("spells: object 0x%08X lost a cantrip use (class %d) - at will, none spent", ObjectIdOf(stats), row);
    return;
  }
  const bool real_level = caster && spell_level >= 1 && spell_level <= 9;
  const uint8_t before = real_level ? *UsesLeft(stats, slot, spell_level) : 0;
  g_decrement(stats, slot, spell_level);
  if (real_level && *UsesLeft(stats, slot, spell_level) < before) MirrorSpend(stats, slot, spell_level);  // 4A
}

// A spontaneous caster's uses left at this spell level (ClassInfo's per-level bytes).
uint8_t* UsesLeft(void* stats, uint8_t slot, int spell_level) {
  return static_cast<uint8_t*>(stats) + ee::kStatsClassInfo + slot * ee::kClassInfoSize + ee::kClassInfoSpellsLeft +
         spell_level;
}

rules::SlotUses SlotUsesOf(void* stats, uint8_t slot) {
  rules::SlotUses uses{};
  const uint8_t* bytes = UsesLeft(stats, slot, 0);
  for (int level = 0; level <= 9; ++level) uses[level] = bytes[level];
  return uses;
}

int SlotLevel(int nwn_class, uint32_t spell_id, int metamagic);
bool Memorizes(int nwn_class);

// Part 2a: the slot level a cast needs when it can upcast - a spontaneous 2024 caster's spell levels 1-9, no domain
// slot; -1 otherwise (NWN's).
int UpcastableSlotLevel(int row, uint32_t spell_id, uint8_t domain_level, uint8_t metamagic) {
  if (domain_level != 0 || Memorizes(row)) return -1;
  const int slot_level = SlotLevel(row, spell_id, metamagic);
  return slot_level >= 1 && slot_level <= 9 ? slot_level : -1;
}

// Part 2a: the casts left of a spell - the slots at its level and above - when NWN would cast it with a slot of its
// level (asked with that level's byte set to 1 for the question when it's empty).
uint8_t UsesLeftHook(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  int row = 0, level = 0;
  const int slot_level =
      CasterSlot(stats, slot, &row, &level) ? UpcastableSlotLevel(row, spell_id, domain_level, metamagic) : -1;
  if (slot_level < 1) return g_uses_left(stats, spell_id, slot, domain_level, metamagic);
  const int casts = rules::CastsLeft(SlotUsesOf(stats, slot), slot_level);
  if (casts == 0) return g_uses_left(stats, spell_id, slot, domain_level, metamagic);
  uint8_t* own = UsesLeft(stats, slot, slot_level);
  const uint8_t before = *own;
  if (before == 0) *own = 1;
  const uint8_t castable = g_uses_left(stats, spell_id, slot, domain_level, metamagic);
  *own = before;
  return castable ? static_cast<uint8_t>(casts > 255 ? 255 : casts) : 0;
}

int UnReady(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_unready(stats, spell_id, slot, domain_level, metamagic);
  // Part 2a: with no slot left at the spell's level, NWN spends that level's byte (set to 1 for it) and the lowest
  // higher slot goes instead.
  const int slot_level = UpcastableSlotLevel(row, spell_id, domain_level, metamagic);
  const int spend_at = slot_level >= 1 ? rules::SlotToSpend(SlotUsesOf(stats, slot), slot_level) : -1;
  const bool upcast = slot_level >= 1 && spend_at > slot_level;
  if (upcast) *UsesLeft(stats, slot, slot_level) = 1;
  uint8_t* cantrips = UsesLeft(stats, slot, 0);
  const uint8_t before = *cantrips;
  const int result = g_unready(stats, spell_id, slot, domain_level, metamagic);
  if (upcast) {
    *UsesLeft(stats, slot, slot_level) = 0;
    if (result) --*UsesLeft(stats, slot, spend_at);
  }
  if (result) {
    void* const creature = CreatureOf(stats);
    PrepareSpellsNoteCast(creature);  // #36 Part 3: a Long Rest's change closes
    const int spent_level = upcast ? spend_at : SlotLevel(row, spell_id, metamagic);
    if (creature && spent_level >= 0) engine::SetLocalInt(creature, kSlotLevelVar, spent_level);
    if (slot_level >= 1) MirrorSpend(stats, slot, spent_level);  // 4A: the other classes sharing the pool
    if (upcast) {
      if (LogCast()) {
        Log("spells: object 0x%08X spell %u (level %d) upcast - no level %d slot left, a level %d slot spent",
            ObjectIdOf(stats), spell_id, slot_level, slot_level, spend_at);
      }
      if (IsPlayer(stats)) {
        TellPlayer(ObjectIdOf(stats), "nwn5e: no level " + std::to_string(slot_level) + " slot left - cast with a level " +
                                         std::to_string(spend_at) + " slot.");
      }
    }
  }
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
  const std::vector<rules::ClassLevel> classes = ClassLevelsOf(stats);
  const bool pooled = !memorizes && rules::SharesSlotPool(classes, slot);  // 4A: every class sharing the pool
  const int left = memorizes ? g_uses_left(stats, order.spell, slot, 0, static_cast<uint8_t>(order.metamagic))
                             : rules::CastsLeft(SlotUsesOf(stats, slot), wanted.slot_level);
  std::vector<int> queued_levels;  // Part 2a: a spontaneous caster's queued casts' slot levels, in order
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
    if (Field<int32_t>(action, ee::kCastDomainLevel) != 0) continue;
    const uint8_t action_slot = Field<uint8_t>(action, ee::kCastClassSlot);
    if (action_slot != slot && !(pooled && rules::SharesSlotPool(classes, action_slot))) continue;
    const int action_row = action_slot < classes.size() ? classes[action_slot].class_row : row;
    const uint32_t spell = Field<uint32_t>(action, ee::kCastSpell);
    const int metamagic = Field<int32_t>(action, ee::kCastMetamagic);
    const rules::QueuedCast queued_cast{spell, metamagic, memorizes ? 0 : SlotLevel(action_row, spell, metamagic)};
    if (rules::DrawsOnSameSlots(memorizes, wanted, queued_cast)) ++queued;
    if (!memorizes) queued_levels.push_back(queued_cast.slot_level);
  }
  if (memorizes ? !rules::CastQueueFull(true, wanted.slot_level, queued, left)
                : rules::CastFitsQueue(SlotUsesOf(stats, slot), queued_levels, wanted.slot_level)) {
    return false;
  }
  if (!memorizes) queued = static_cast<int>(queued_levels.size());
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
  if (!client_stats || domain_level != 0 || slot >= ee::kClientStatsClassSlots ||
      slot >= Field<uint8_t>(client_stats, ee::kClientStatsClassCount)) {
    return uses;
  }
  const int row = Field<uint8_t>(client_stats, ee::kClientStatsClassSlotClass + slot * ee::kClientStatsClassSlotSize);
  if (rules::CasterKindOfClass(row) == rules::CasterKind::kNone) return uses;
  const int upcastable = UpcastableSlotLevel(row, spell_id, domain_level, metamagic);
  if (upcastable >= 1) {  // Part 2a: the casts left - its copy of the slots at the spell's level and above
    rules::SlotUses counts{};
    const uint8_t* bytes = static_cast<const uint8_t*>(client_stats) + ee::kClientStatsClassSlotUses +
                           slot * ee::kClientStatsClassSlotSize;
    for (int level = 0; level <= 9; ++level) counts[level] = bytes[level];
    const int casts = rules::CastsLeft(counts, upcastable);
    return static_cast<uint8_t>(casts > 255 ? 255 : casts);
  }
  if (metamagic != 0) return uses;
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
  // Part 3b-1: the client's quick level-up picks against the 2024 total.
  return static_cast<uint8_t>(SpellsPageCap(reinterpret_cast<uintptr_t>(_ReturnAddress()), spell_level, cap));
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
  const bool upcasting =
      HookExport("CNWSCreatureStats::GetSpellUsesLeft (upcasting: the slots at a spell's level and above, #36 Part 2)",
                 ee::kGetSpellUsesLeft, reinterpret_cast<void*>(&UsesLeftHook), reinterpret_cast<void**>(&g_uses_left));
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
  InstallSpellsPage();  // Part 3b-1: NWN's Spells page with 2024's counts
  const bool counts = HideCantripCounts();
  Log(counts ? "spells: a cantrip's button shows no uses number (%zu places; #36 SL-3)"
             : "SKIP spells #36 SL-3: a cantrip's button keeps its uses number (%zu places found)",
      g_count_sites.size());
  Log("spells: 2024 slots (SRD 5.2's tables; no bonus spells, no 3e minimum score); Cleric, Druid, Paladin and Ranger "
      "cast any class spell with any slot of its level; Paladin and Ranger from level 1; cantrips at will (#36 Part 1)%s",
      gain && min && spend && cast && ready ? "" : " - SOME hooks missing");
  Log("spells: upcasting - with no slot left at a spell's level, the lowest higher one is spent; the uses shown are "
      "the slots at its level and above (#36 Part 2a); two or more caster classes share one pool, the Multiclass "
      "Spellcaster table's (#36 Part 2b)%s",
      upcasting && cast && g_client_uses ? "" : " - SOME hooks missing");
}
