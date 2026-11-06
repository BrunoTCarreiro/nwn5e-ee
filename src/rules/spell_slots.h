#pragma once

#include <array>
#include <string>
#include <vector>

// #36 (owner, 2026-11-04: "go with your recommendations and note down the upcasting thing so you don't forget when
// doing 39" - 1A 2A 3A 4A; docs/PLAN.md "Phase 4"): the 2024 spell slots, cantrips and prepared spells - SRD 5.2's
// class tables (the Bard, Cleric, Druid, Paladin, Ranger, Sorcerer and Wizard Features tables; the Multiclass
// Spellcaster table is the full casters'). core/spell_slots.cpp; data/files/cls_spgn_5e*.2da.

namespace rules {

enum class CasterKind { kNone, kFull, kHalf };

// classes.2da rows: Bard 1, Cleric 2, Druid 3, Sorcerer 9, Wizard 10 full casters; Paladin 6, Ranger 7 half casters
// (spells from level 1 in 2024).
CasterKind CasterKindOfClass(int class_row);

// The slots of `spell_level` (1-9) at caster `level` (1-20): the full casters' table (the Multiclass Spellcaster table
// too) or the half casters'. 0 outside.
int SpellSlots2024(CasterKind kind, int level, int spell_level);

// The highest spell level with a slot at that level (0: none).
int MaxSpellLevel2024(CasterKind kind, int level);

// The cantrips a class knows at its level: Bard and Druid 2 / 3 / 4, Cleric and Wizard 3 / 4 / 5, Sorcerer 4 / 5 / 6
// at levels 1 / 4 / 10; Paladin and Ranger none.
int CantripsKnown2024(int class_row, int class_level);

// The level 1+ spells a class prepares at its level (its Prepared Spells column).
int PreparedSpells2024(int class_row, int class_level);

// classes.2da's SpellGainTable value (cls_spgn) for a class level and spell level: level 0 the cantrips known (for a
// spontaneous caster only a gate - they're at will), else the slots. 0 outside.
int SpellGain2024(int class_row, int class_level, int spell_level);

// No 3e minimum score (10 + the spell's level) in 2024: any real spell level 0-9 is met; any other (the engine's
// 0xFF, a spell off the class's list) never is - LevelUpAutomatic indexes its per-level lists with it (0.75.1).
bool SpellLevelMet2024(int spell_level);

// A cast spends a use: not a cantrip's in faithful (at will).
bool CastSpendsUse(bool faithful, int spell_level);

// A cast order and what it draws on: for a class that memorises, the ready copies of the spell with that metamagic;
// for one that casts spontaneously, the slots of its level (core/rules_spell_slots.cpp counts the queue).
struct QueuedCast {
  unsigned spell = 0;
  int metamagic = 0;
  int slot_level = 0;
};
bool DrawsOnSameSlots(bool memorizes, const QueuedCast& a, const QueuedCast& b);

// A new cast order is refused when the casts already queued from the same slots use up what is left (owner,
// 2026-11-04: "SL-2 should not be allowed to queue spells a larger number of times than the slots available");
// a cantrip never (at will).
bool CastQueueFull(bool faithful, int slot_level, int queued, int uses_left);

// Part 2, upcasting by choice (owner, 2026-11-06: "Upcasting should not be automatic, the player should have/get to
// choose at what level they cast a spell" - 2B; 0.79.0's automatic 2A gone): SRD 5.2 - a spell is cast with a slot
// of its level or higher, and "the spell assumes the higher level for that casting". The chosen slot level rides in
// the cast's domain level (0: the spell's own). `SlotUses` is a caster's uses (or slots) per spell level (0-9); a
// slot level is the spell's level plus its metamagic's cost.
using SlotUses = std::array<int, 10>;
// The slot level a cast spends: the spell's own (`slot_level`) for domain level 0, else the domain level when it is
// that level or a higher one up to 9; -1 for any other (never a lower slot).
int ChosenSlotLevel(int slot_level, int domain_level);
// The casts left of a spell needing `slot_level`: the uses at its level and above - its radial entry's count (the
// levels are chosen under it).
int CastsLeft(const SlotUses& uses, int slot_level);
// SL-2: whether a new cast at `slot_level` still finds a slot once the casts already queued (their slot levels) have
// each spent one of theirs; a cantrip (0) always fits.
bool CastFitsQueue(const SlotUses& uses, const std::vector<int>& queued, int slot_level);
// The radial's levels for a spell needing `slot_level` ("an extra level ... where the user can go into and pick at
// which level they want to cast the spell at"): its own, then each higher one the caster has slots of (`slots`:
// its slots per spell level, the class's table). Empty when there is no higher one - the spell's entry casts at once.
std::vector<int> UpcastMenuLevels(const SlotUses& slots, int slot_level);
// An upcast quickbar button's number (owner: "go with L3·4"): "L", the slot level, Windows-1252's middle dot
// (0xB7), the uses left.
std::string UpcastCountText(int slot_level, int uses);
// The client radial's spell entry param (the game's own packing; its cast callback, 0x9A4540, unpacks it): the spell
// id in bits 0-15, the domain level in 24-27, the class slot in 28-29 and 38, the metamagic in 31-36; bit 30 (a
// no-uses listing) and bit 37 (a spontaneous conversion) mark other listings. Bits 40-43 are nwn5e's own: a level
// submenu's first level (its "more" entry).
struct RadialCast {
  unsigned spell = 0;
  int domain_level = 0;
  int class_slot = 0;
  int metamagic = 0;
  bool other = false;
};
RadialCast DecodeRadialCast(long long param);
// The param with its domain level set (0-15), all else kept.
long long RadialCastWithDomain(long long param, int domain_level);
// nwn5e's level submenu's first level (bits 40-43), read and set.
int RadialCastFromLevel(long long param);
long long RadialCastWithFromLevel(long long param, int level);

// Part 2 (4A, the combined multiclass pool; decided 2026-11-04): SRD 5.2's multiclassing - with the Spellcasting
// feature from more than one class, the spell slots come from the Multiclass Spellcaster table (the full casters')
// at the levels of the Bard, Cleric, Druid, Sorcerer and Wizard plus half the Paladin and Ranger levels, rounded up;
// each class prepares its own spells (by its own level) and casts them with those slots. In NWN each class slot has
// its own uses per spell level, so every caster class holds the pool's table and a spend from one is made in all
// (core/rules_spell_slots.cpp): no class shows 0 slots in NWN's own panels. `classes`: a creature's class slots in
// order (classes.2da row, levels).
struct ClassLevel {
  int class_row = -1;
  int level = 0;
};
// The Multiclass Spellcaster table's level; 0 with fewer than two caster classes (one keeps its own table).
int MulticlassCasterLevel2024(const std::vector<ClassLevel>& classes);
// Whether a class slot's spell slots are the shared pool: a caster class, with two or more caster classes.
bool SharesSlotPool(const std::vector<ClassLevel>& classes, int slot);
// classes.2da's SpellGainTable value for a class slot: SpellGain2024's, or for a class sharing the pool spell levels
// 1-9 from the Multiclass Spellcaster table. Level 0 stays each class's own (its cantrips).
int SlotsForClassSlot2024(const std::vector<ClassLevel>& classes, int slot, int spell_level);

}  // namespace rules
