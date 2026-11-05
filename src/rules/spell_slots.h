#pragma once

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

}  // namespace rules
