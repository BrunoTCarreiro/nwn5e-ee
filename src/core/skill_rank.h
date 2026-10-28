#pragma once

#include <stdint.h>

// CNWSCreatureStats::GetSkillRank(skill, vs, bBaseRanksOnly) — every engine
// and script skill total comes from it (83 call sites in EE: the character
// sheet, skill checks, scripts, Use Magic Device, Discipline, Concentration,
// ...). Rules adjust its answer in the order they were added: each gets the
// value so far (the engine's first) and returns the new one; `caller` is the
// engine call site's return address, to tell the uses apart. The final value
// is clamped to -127..127, as the engine's is.
struct SkillQuery {
  void* stats;
  int skill;  // skills.2da row
  void* vs;   // CNWSObject* the check is against, or null
  bool base_only;
  uintptr_t caller;
};
using SkillAdjuster = int (*)(const SkillQuery& query, int value);

// Hooks GetSkillRank on first use; false (and logged) if it can't.
bool AddSkillAdjuster(SkillAdjuster adjuster);

// A skill total now, with every adjuster (the 2024 value when the skills are
// faithful) - for nwn5e's own checks (an Arcana roll, Fast Hands).
int SkillRankNow(void* stats, int skill, void* vs);

// The return address of the single GetSkillRank call inside the exported
// function `export_name` (0, logged, when there isn't exactly one).
uintptr_t SkillRankCallerIn(const char* export_name);
