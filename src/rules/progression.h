#pragma once

// Phase 3 CM1 / CM2 (PLAN.md): levels, XP and multiclassing, SRD 5.2.

#include <stdint.h>

#include <vector>

#include "rules/combat.h"
#include "rules/saves.h"

namespace rules {

constexpr int kMaxLevel2024 = 20;
// The engine's XP table: entry i is the XP needed for level i + 1 (41 entries).
constexpr int kXpTableSize = 41;

// XP needed for `level`: 2024 (levels 1–20) and NWN's exptable.2da, 500·L·(L−1)
// (levels 1–41) — the formula the campaign scripts also hard-code.
uint32_t Xp2024ForLevel(int level);
uint32_t NwnXpForLevel(int level);

// Faithful's engine table for `level` (1–41): 2024's thresholds up to 20, then
// NWN's steps continued from 355,000 (shown only; the cap stops at 20).
uint32_t FaithfulXpForLevel(int level);

// The level `xp` reaches on faithful's table (1–41).
int FaithfulLevelForXp(uint32_t xp);

// What a script's SetXP becomes in faithful, for a character of
// `current_level` with `current_xp`. The scripts speak NWN's table:
// - a value that is NWN's threshold for another level sets that level
//   (HotU's 105,000 = level 15 -> 165,000; Prelude's 1,000 / 3,000);
// - NWN's threshold for the current level is the respawn scripts' "never lose
//   a level" clamp: XP is never raised by it, nor drops below the level;
// - anything else is kept, but never below the current level's threshold
//   (2024 has no XP loss that takes levels).
uint32_t ScriptSetXp2024(uint32_t requested, uint32_t current_xp, int current_level);

// CM7: nwn5e's Ability Score Improvement feats (data/rows/feat.txt): feat.2da
// rows 1200..1206, each needing the one before.
constexpr int kAsiFeatFirst = 1200;
constexpr int kAsiFeatCount = 7;

// CM7: the class levels that give a feat in 2024 (Ability Score Improvement
// or another feat): 4, 8, 12, 16 and 19 (Epic Boon), the Fighter also 6 and
// 14, the Rogue also 10. Fighting Style levels are not counted here.
bool IsFeatLevel2024(CharacterClass character_class, int class_level);
// How many of those `class_level` levels of the class have reached.
int FeatLevelsReached2024(CharacterClass character_class, int class_level);

// 2024 multiclassing: a score of 13 in the primary ability of the new class
// and of every current class. Scores indexed by Ability.
bool MeetsPrimaryAbility(CharacterClass character_class, const int scores[6]);

// "Strength or Dexterity 13", for messages.
const char* PrimaryAbilityText(CharacterClass character_class);

// Taking a level in `next`: allowed when the character already has `next`,
// or when `next` and every current class meet their primary ability. On a
// refusal `failing` (when given) is the first class that does not.
bool CanMulticlassInto(CharacterClass next, const std::vector<CharacterClass>& current, const int scores[6],
                       CharacterClass* failing);

}  // namespace rules
