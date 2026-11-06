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
// - Part 2, upcasting by choice (owner, 2026-11-06: "Upcasting should not be automatic, the player should have/get to
//   choose at what level they cast a spell. I picture it as selecting the spell from their original level but there
//   being an extra level (in the radial menu) where the user can go into and pick at which level they want to cast
//   the spell at. For the quickbar, they can just set an upcast spell in a slot"; then "1 ok, 2 go with L3·4").
//   0.79.0's automatic upcast is gone. The chosen slot level rides in the cast's domain level (rules::ChosenSlotLevel;
//   0: the spell's own) - a byte in the cast message, the action's +0x48, the caster's +0x21C (GetLastSpellLevel()),
//   the quickbar's QBDomainLevel; a spontaneous class never used it.
//   - Server: GetSpellUsesLeft answers a domain level N with level N's slots, when NWN would cast the spell with a
//     slot of its own level (asked with domain 0 - its ConfirmDomainSpell refuses a spontaneous class - and that
//     level's byte 1 for the question). UnReadySpell spends level N the same way: NWN spends the own level's byte
//     (set to 1 for it, domain 0), and level N's is decremented instead. SL-2's queue counts exact levels.
//   - Client radial (CRadialMenu::SetEntryx hooked): a 2024 caster's spell leaf (the cast callback, no metamagic, no
//     domain) with a higher level of slots becomes a submenu (SetSubMenuCreatorx) - "Level N" for its level and each
//     higher one with slots (rules::UpcastMenuLevels), each casting with domain N (0 for its own), its count the
//     level's slots left; built as the game builds one (its operator new, the CRadialMenu constructor, +0x7F1).
//   - The client's uses getter: domain N - level N's slots; domain 0 - its own level's, the radial's spell list (the
//     parent entry) the slots at its level and above. An upcast quickbar button's number reads "L3·4"
//     (rules::UpcastCountText: the getter's caller's next ToString, as a cantrip's is blanked).
//   - 0.84.1: a spell the class doesn't know (not among its cantrips and prepared spells) reads 0 on the client - a
//     stale quickbar button greys out (the owner's Negative Energy Ray, left from a Sorcerer, counted a Cleric's
//     three 2nd-level slots while the server refused it: "No uses or preparations left for that spell").
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

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
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
// Upcasting by choice: the client radial's pieces (CRadialMenu, CGuiIcon exports; the cast callback by pattern; the
// game's operator new and its controlled creature from the game's own calls).
using SetEntryFn = void (*)(void* menu, int index, void* icon, uint32_t strref, void* handler, void* callback,
                            int64_t param);
using SetEntryTextFn = void (*)(void* menu, int index, const char* icon, const char* text, void* handler,
                                void* callback, int64_t param);
using SetCreatorFn = void (*)(void* menu, int index, void* icon, uint32_t strref, void* handler, void* creator,
                              int64_t param);
using SetCreatorTextFn = void (*)(void* menu, int index, const char* icon, const char* text, void* handler,
                                  void* creator, int64_t param);
using DisableEntryFn = void (*)(void* menu, int index);
using SetCountTextFn = void (*)(void* icon, void* text);  // CExoString by value: the callee owns it
using RadialCtorFn = void* (*)(void* menu);
using GameNewFn = void* (*)(size_t size);
using ControlledFn = void* (*)(void* app);
using ExoStringFromCharsFn = void* (*)(void* out, const char* text);
using ExoStringDestroyFn = void (*)(void* text);
SetEntryFn g_set_entry = nullptr;  // the original (hooked)
SetEntryTextFn g_set_entry_text = nullptr;
SetCreatorFn g_set_creator = nullptr;
SetCreatorTextFn g_set_creator_text = nullptr;
DisableEntryFn g_disable_entry = nullptr;
SetCountTextFn g_set_count_text = nullptr;
RadialCtorFn g_radial_ctor = nullptr;
GameNewFn g_game_new = nullptr;
ControlledFn g_controlled = nullptr;
void** g_controlled_app = nullptr;
ExoStringFromCharsFn g_exo_from_chars = nullptr;
ExoStringDestroyFn g_exo_destroy = nullptr;
void* g_cast_callback = nullptr;                 // the radial's leaf callback that casts (0x9A4540)
std::vector<uintptr_t> g_radial_count_sites;     // the radial spell lists' calls of the client getter (returns)
thread_local uintptr_t t_upcast_text_at = 0;     // the next ToString that writes an upcast button's number
thread_local int t_upcast_text_level = 0;
bool g_radial_ready = false;                     // the radial's level menu installed
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

// Upcasting by choice: a spontaneous 2024 caster's spell of level 1-9 (`own`, with its metamagic's cost) - the slot
// level a cast with this domain level spends (rules::ChosenSlotLevel), -1 for a lower or unknown one; -2 when the
// cast isn't one of these (a memorizer, a cantrip, off the class's list): NWN's.
int ChosenSlot(int row, uint32_t spell_id, uint8_t domain_level, uint8_t metamagic, int* own) {
  if (Memorizes(row)) return -2;
  *own = SlotLevel(row, spell_id, metamagic);
  if (*own < 1 || *own > 9) return -2;
  return rules::ChosenSlotLevel(*own, domain_level);
}

// Upcasting by choice: a domain level N names the slot - level N's slots, when NWN would cast the spell with one of
// its own level (asked with domain 0 and that level's byte set to 1 for the question).
uint8_t UsesLeftHook(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  int row = 0, level = 0, own = -1;
  if (domain_level == 0 || !CasterSlot(stats, slot, &row, &level)) {
    return g_uses_left(stats, spell_id, slot, domain_level, metamagic);
  }
  const int chosen = ChosenSlot(row, spell_id, domain_level, metamagic, &own);
  if (chosen == -2) return g_uses_left(stats, spell_id, slot, domain_level, metamagic);
  if (chosen < 0) return 0;
  uint8_t* own_byte = UsesLeft(stats, slot, own);
  const uint8_t before = *own_byte;
  if (before == 0) *own_byte = 1;
  const uint8_t castable = g_uses_left(stats, spell_id, slot, 0, metamagic);
  *own_byte = before;
  return castable ? *UsesLeft(stats, slot, chosen) : 0;
}

int UnReady(void* stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return g_unready(stats, spell_id, slot, domain_level, metamagic);
  // Upcasting by choice: a domain level N spends a level N slot - NWN spends the own level's byte (domain 0, the byte
  // set to 1 for it) and level N's is decremented instead.
  int own = -1;
  const int chosen = domain_level != 0 ? ChosenSlot(row, spell_id, domain_level, metamagic, &own) : -2;
  if (chosen == -1) return 0;  // a lower or unknown level: no cast
  const bool upcast = chosen >= 1;
  if (upcast && *UsesLeft(stats, slot, chosen) == 0) return 0;  // none left at the chosen level
  const uint8_t own_before = upcast ? *UsesLeft(stats, slot, own) : 0;
  if (upcast) *UsesLeft(stats, slot, own) = 1;
  uint8_t* cantrips = UsesLeft(stats, slot, 0);
  const uint8_t before = *cantrips;
  const int result = g_unready(stats, spell_id, slot, upcast ? 0 : domain_level, metamagic);
  if (upcast) {
    *UsesLeft(stats, slot, own) = own_before;
    if (result) --*UsesLeft(stats, slot, chosen);
  }
  if (result) {
    void* const creature = CreatureOf(stats);
    PrepareSpellsNoteCast(creature);  // #36 Part 3: a Long Rest's change closes
    const int spent_level = upcast ? chosen : SlotLevel(row, spell_id, metamagic);
    if (creature && spent_level >= 0) engine::SetLocalInt(creature, kSlotLevelVar, spent_level);
    if (!Memorizes(row)) MirrorSpend(stats, slot, spent_level);  // 4A: the other classes sharing the pool
    if (upcast && LogCast()) {
      Log("spells: object 0x%08X spell %u (level %d) cast with a level %d slot (chosen)", ObjectIdOf(stats), spell_id,
          own, chosen);
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
  if (!creature || order.fake || order.spontaneous || order.class_slot < 0 ||
      order.class_slot >= ee::kMaxClassSlots || !g_uses_left) {
    return false;
  }
  void* stats = StatsOf(creature);
  const uint8_t slot = static_cast<uint8_t>(order.class_slot);
  int row = 0, level = 0;
  if (!CasterSlot(stats, slot, &row, &level)) return false;
  const bool memorizes = Memorizes(row);
  if (memorizes && order.domain_level != 0) return false;  // a domain slot: NWN's
  const int own_level = SlotLevel(row, order.spell, order.metamagic);
  const rules::QueuedCast wanted{order.spell, order.metamagic,
                                 memorizes ? own_level : rules::ChosenSlotLevel(own_level, order.domain_level)};
  if (wanted.slot_level < 0 || !rules::CastSpendsUse(true, wanted.slot_level)) return false;  // a cantrip: at will
  const std::vector<rules::ClassLevel> classes = ClassLevelsOf(stats);
  const bool pooled = !memorizes && rules::SharesSlotPool(classes, slot);  // 4A: every class sharing the pool
  const int left = memorizes ? g_uses_left(stats, order.spell, slot, 0, static_cast<uint8_t>(order.metamagic))
                             : SlotUsesOf(stats, slot)[wanted.slot_level];
  std::vector<int> queued_levels;  // a spontaneous caster's queued casts' slot levels (by choice: their domain's)
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
    const int action_domain = Field<int32_t>(action, ee::kCastDomainLevel);
    if (memorizes && action_domain != 0) continue;
    const uint8_t action_slot = Field<uint8_t>(action, ee::kCastClassSlot);
    if (action_slot != slot && !(pooled && rules::SharesSlotPool(classes, action_slot))) continue;
    const int action_row = action_slot < classes.size() ? classes[action_slot].class_row : row;
    const uint32_t spell = Field<uint32_t>(action, ee::kCastSpell);
    const int metamagic = Field<int32_t>(action, ee::kCastMetamagic);
    const rules::QueuedCast queued_cast{
        spell, metamagic,
        memorizes ? 0
                  : rules::ChosenSlotLevel(SlotLevel(action_row, spell, metamagic),
                                           action_domain < 0 || action_domain > 255 ? 255 : action_domain)};
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
// The client's copy of a class slot's uses per spell level.
rules::SlotUses ClientSlotUses(const void* client_stats, uint8_t slot) {
  rules::SlotUses uses{};
  const uint8_t* bytes = static_cast<const uint8_t*>(client_stats) + ee::kClientStatsClassSlotUses +
                         slot * ee::kClientStatsClassSlotSize;
  for (int level = 0; level <= 9; ++level) uses[level] = bytes[level];
  return uses;
}

uint8_t ClientUsesLeft(void* client_stats, uint32_t spell_id, uint8_t slot, uint8_t domain_level, uint8_t metamagic) {
  const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
  t_blank_at = 0;
  t_upcast_text_at = 0;
  const bool known_slot = client_stats && slot < ee::kClientStatsClassSlots &&
                          slot < Field<uint8_t>(client_stats, ee::kClientStatsClassCount);
  const int row = known_slot ? Field<uint8_t>(client_stats, ee::kClientStatsClassSlotClass +
                                                              slot * ee::kClientStatsClassSlotSize)
                             : -1;
  const bool caster = known_slot && rules::CasterKindOfClass(row) != rules::CasterKind::kNone;
  // 0.84.1: a spell the class doesn't know (a 2024 caster's known list: its cantrips and prepared spells) has no
  // uses - a quickbar button left from another class, or from before a Long Rest's change, reads 0 and greys out
  // (NWN's count for a spontaneous class reads the level's slots only; the server refuses the cast).
  if (caster && !Memorizes(row)) {
    const int class_level = SlotLevel(row, spell_id, 0);
    if (class_level >= 0 && !ClientKnowsSpell(client_stats, slot, class_level, spell_id)) return 0;
  }
  int own = -1;
  if (caster && domain_level != 0) {  // by choice: a level N cast - level N's slots (the radial's, a quickbar button)
    const int chosen = ChosenSlot(row, spell_id, domain_level, metamagic, &own);
    if (chosen != -2) {
      if (chosen < 0) return 0;
      const uint8_t uses = static_cast<uint8_t>(ClientSlotUses(client_stats, slot)[chosen]);
      for (const auto& site : g_count_sites) {
        if (site.first == from) {
          t_upcast_text_at = site.second;  // its number: "L3·4"
          t_upcast_text_level = chosen;
        }
      }
      return uses;
    }
  }
  const uint8_t uses = g_client_uses(client_stats, spell_id, slot, domain_level, metamagic);
  if (!caster || domain_level != 0) return uses;
  if (metamagic == 0 && ChosenSlot(row, spell_id, 0, 0, &own) >= 1) {
    for (uintptr_t site : g_radial_count_sites) {  // the radial's spell entry: the slots at its level and above
      if (site == from) {
        const int casts = rules::CastsLeft(ClientSlotUses(client_stats, slot), own);
        return static_cast<uint8_t>(casts > 255 ? 255 : casts);
      }
    }
  }
  if (metamagic != 0) return uses;
  const int spell_level = SlotLevel(row, spell_id, 0);
  if (spell_level < 0 || rules::CastSpendsUse(true, spell_level)) return uses;
  for (const auto& site : g_count_sites) {
    if (site.first == from) t_blank_at = site.second;
  }
  return uses;
}

void* ToString(void* out, int value) {
  void* result = g_to_string(out, value);
  if (t_upcast_text_at && reinterpret_cast<uintptr_t>(_ReturnAddress()) == t_upcast_text_at) {
    t_upcast_text_at = 0;
    if (result && g_exo_destroy && g_exo_from_chars) {  // an upcast quickbar button: "L3·4"
      const std::string text = rules::UpcastCountText(t_upcast_text_level, value);
      g_exo_destroy(result);
      g_exo_from_chars(result, text.c_str());
    }
    return result;
  }
  if (t_blank_at && reinterpret_cast<uintptr_t>(_ReturnAddress()) == t_blank_at) {
    t_blank_at = 0;
    char* text = result ? Field<char*>(result, 0) : nullptr;
    if (text) text[0] = '\0';
  }
  return result;
}

// --- Upcasting by choice: the radial's level submenu ---

// The client's controlled creature's stats (the radial's cast callback casts for it).
void* ClientControlledStats() {
  void* const holder = g_controlled_app ? *g_controlled_app : nullptr;
  void* const app = holder ? *static_cast<void**>(holder) : nullptr;
  void* const creature = app && g_controlled ? g_controlled(app) : nullptr;
  return creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
}

// The radial's levels for a spell entry's param (rules::UpcastMenuLevels; empty: not one to choose a level for) - a
// 2024 caster's spell, no metamagic, no domain level, no other listing.
std::vector<int> UpcastLevels(int64_t param, void** client_stats_out, rules::RadialCast* cast_out, int* own_out) {
  const rules::RadialCast cast = rules::DecodeRadialCast(param);
  if (cast.other || cast.domain_level != 0 || cast.metamagic != 0) return {};
  void* const client_stats = ClientControlledStats();
  if (!client_stats || cast.class_slot >= ee::kClientStatsClassSlots ||
      cast.class_slot >= Field<uint8_t>(client_stats, ee::kClientStatsClassCount)) {
    return {};
  }
  std::vector<rules::ClassLevel> classes;
  const int count = Field<uint8_t>(client_stats, ee::kClientStatsClassCount);
  for (int index = 0; index < count && index < ee::kClientStatsClassSlots; ++index) {
    classes.push_back({Field<uint8_t>(client_stats, ee::kClientStatsClassSlotClass + index * ee::kClientStatsClassSlotSize),
                       Field<uint8_t>(client_stats, ee::kClientStatsClassSlotLevel + index * ee::kClientStatsClassSlotSize)});
  }
  const int row = classes[cast.class_slot].class_row;
  int own = -1;
  if (rules::CasterKindOfClass(row) == rules::CasterKind::kNone || ChosenSlot(row, cast.spell, 0, 0, &own) < 1) return {};
  rules::SlotUses table{};
  for (int level = 1; level <= 9; ++level) table[level] = rules::SlotsForClassSlot2024(classes, cast.class_slot, level);
  *client_stats_out = client_stats;
  *cast_out = cast;
  *own_out = own;
  return rules::UpcastMenuLevels(table, own);
}

void* BuildLevelMenu(void* handler, int64_t param, int from_level);
void* LevelMenu(void* handler, int64_t param) { return BuildLevelMenu(handler, param, 0); }
void* LevelMenuMore(void* handler, int64_t param) {
  return BuildLevelMenu(handler, rules::RadialCastWithFromLevel(param, 0), rules::RadialCastFromLevel(param));
}

// "Level N" for the spell's level and each higher one with slots (from `from_level` on: a "more" entry's), each
// casting with domain level N (0 for its own), its count the level's slots left - greyed with none, unless the
// quickbar is assigning. Eight entries hold the radial; past them, the last opens the rest.
void* BuildLevelMenu(void* handler, int64_t param, int from_level) {
  void* const menu = g_game_new ? g_game_new(ee::kRadialMenuSize) : nullptr;
  if (!menu) return nullptr;
  g_radial_ctor(menu);
  SetField<uint8_t>(menu, ee::kRadialMenuOwnedFlag, 1);
  void* client_stats = nullptr;
  rules::RadialCast cast;
  int own = -1;
  std::vector<int> levels = UpcastLevels(param, &client_stats, &cast, &own);
  levels.erase(std::remove_if(levels.begin(), levels.end(), [&](int level) { return level < from_level; }),
               levels.end());
  void* rules_object = RulesObject();
  void* spells = rules_object ? Field<void*>(rules_object, ee::kRulesSpells) : nullptr;
  void* spell = spells && g_get_spell ? g_get_spell(spells, static_cast<int>(cast.spell)) : nullptr;
  if (levels.empty() || !spell) return menu;
  char icon[17] = {};
  memcpy(icon, static_cast<const uint8_t*>(spell) + ee::kSpellIconResRef, 16);
  const bool assigning = handler && Field<void*>(handler, ee::kRadialHandlerAssigning) != nullptr;
  const rules::SlotUses uses = ClientSlotUses(client_stats, cast.class_slot);
  for (size_t k = 0; k < levels.size(); ++k) {
    const int index = static_cast<int>(k);
    char text[24];
    if (index == ee::kRadialMenuEntries - 1 && levels.size() > static_cast<size_t>(ee::kRadialMenuEntries)) {
      sprintf_s(text, "Level %d+", levels[k]);
      g_set_creator_text(menu, index, icon, text, handler, reinterpret_cast<void*>(&LevelMenuMore),
                         rules::RadialCastWithFromLevel(param, levels[k]));
      break;
    }
    const int level = levels[k];
    sprintf_s(text, "Level %d", level);
    g_set_entry_text(menu, index, icon, text, handler, g_cast_callback,
                     rules::RadialCastWithDomain(param, level == own ? 0 : level));
    void* const entry_icon = Field<void*>(menu, ee::kRadialMenuIcons + index * sizeof(void*));
    if (entry_icon && g_to_string) {
      alignas(16) uint8_t count[32] = {};
      g_to_string(count, uses[level]);
      g_set_count_text(entry_icon, count);  // a CExoString by value: the callee destroys it
    }
    if (uses[level] == 0 && !assigning) g_disable_entry(menu, index);
  }
  return menu;
}

// CRadialMenu::SetEntryx: a 2024 caster's spell leaf (the cast callback) with a higher level of slots becomes a
// submenu of its levels - its icon, its name, the same param.
void SetEntry(void* menu, int index, void* icon, uint32_t strref, void* handler, void* callback, int64_t param) {
  if (callback == g_cast_callback && g_set_creator) {
    void* client_stats = nullptr;
    rules::RadialCast cast;
    int own = -1;
    if (!UpcastLevels(param, &client_stats, &cast, &own).empty()) {
      g_set_creator(menu, index, icon, strref, handler, reinterpret_cast<void*>(&LevelMenu), param);
      return;
    }
  }
  g_set_entry(menu, index, icon, strref, handler, callback, param);
}

// The radial's pieces, and the SetEntryx hook. False (logged) when any is missing: the spell entries stay NWN's.
bool InstallUpcastRadial(uintptr_t client_getter) {
  const uintptr_t callback = FindPattern("the radial's cast callback (#36 upcasting)", ee::kRadialCastCallbackPattern);
  const uint8_t* lookup = reinterpret_cast<const uint8_t*>(callback + ee::kRadialCastControlled);
  if (callback && lookup[0] == 0x48 && lookup[1] == 0x8B && lookup[2] == 0x05 && lookup[7] == 0x48 && lookup[8] == 0x8B &&
      lookup[9] == 0x08 && lookup[10] == 0xE8) {
    const int32_t rel = *reinterpret_cast<const int32_t*>(lookup + 3);
    g_controlled_app = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(lookup) + 7 + rel);
    g_controlled = reinterpret_cast<ControlledFn>(CallTarget(reinterpret_cast<uintptr_t>(lookup) + 10));
  }
  const uintptr_t set_entry = EngineExport(ee::kRadialSetEntry);
  // The radial's spell lists: the getter's callers that reference the cast callback (the spell list and a spell's
  // subradial; not the spontaneous cure / inflict radial, which casts by its own).
  const std::vector<uintptr_t> cast_refs = callback ? FindRipReferences(callback) : std::vector<uintptr_t>();
  g_radial_count_sites.clear();
  for (uintptr_t site : client_getter && set_entry ? FindCallsAnywhere(client_getter) : std::vector<uintptr_t>()) {
    const CodeRange range = FunctionAt(site);
    bool lists = false;
    for (uintptr_t ref : cast_refs) lists = lists || range.Contains(ref);
    if (!lists) continue;
    g_radial_count_sites.push_back(site + 5);
    if (!g_game_new) {
      const uintptr_t alloc = FindPatternIn("the radial spell list's new menu (#36 upcasting)", FunctionAt(site),
                                            ee::kRadialMenuNewPattern);
      if (alloc) g_game_new = reinterpret_cast<GameNewFn>(CallTarget(alloc + 5));
    }
  }
  g_cast_callback = reinterpret_cast<void*>(callback);
  g_set_entry_text = reinterpret_cast<SetEntryTextFn>(EngineExport(ee::kRadialSetEntryText));
  g_set_creator = reinterpret_cast<SetCreatorFn>(EngineExport(ee::kRadialSetCreator));
  g_set_creator_text = reinterpret_cast<SetCreatorTextFn>(EngineExport(ee::kRadialSetCreatorText));
  g_disable_entry = reinterpret_cast<DisableEntryFn>(EngineExport(ee::kRadialDisableEntry));
  g_set_count_text = reinterpret_cast<SetCountTextFn>(EngineExport(ee::kGuiIconSetCountText));
  g_radial_ctor = reinterpret_cast<RadialCtorFn>(EngineExport(ee::kRadialMenuCtor));
  g_exo_from_chars = reinterpret_cast<ExoStringFromCharsFn>(EngineExport(ee::kExoStringFromChars));
  g_exo_destroy = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  if (!callback || !g_controlled || !set_entry || g_radial_count_sites.size() != 2 || !g_game_new ||
      !g_set_entry_text || !g_set_creator || !g_set_creator_text || !g_disable_entry || !g_set_count_text ||
      !g_radial_ctor || !g_exo_from_chars || !g_exo_destroy) {
    Log("SKIP spells #36 upcasting: the radial's level menu isn't mapped (callback %p, creature %p, %zu spell lists, "
        "new %p)", reinterpret_cast<void*>(callback), reinterpret_cast<void*>(g_controlled),
        g_radial_count_sites.size(), reinterpret_cast<void*>(g_game_new));
    g_cast_callback = nullptr;
    g_radial_count_sites.clear();
    return false;
  }
  return HookExport("CRadialMenu::SetEntryx (a spell's levels to cast it at, #36 upcasting)", ee::kRadialSetEntry,
                    reinterpret_cast<void*>(&SetEntry), reinterpret_cast<void**>(&g_set_entry));
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
  g_radial_ready = InstallUpcastRadial(getter);  // upcasting by choice: the radial's level menu
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
      HookExport("CNWSCreatureStats::GetSpellUsesLeft (upcasting by choice: a domain level names the slot, #36 Part 2)",
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
  Log("spells: upcasting by choice - a spell's radial entry opens its levels (\"Level N\"), the chosen slot level in "
      "the cast's domain level; an upcast quickbar button shows its level before its count (#36 Part 2); two or more "
      "caster classes "
      "share one pool, the Multiclass Spellcaster table's (#36 Part 2b)%s",
      upcasting && cast && g_client_uses && g_radial_ready ? "" : " - SOME hooks missing");
}
