#pragma once

#include <stdint.h>

#include <vector>

// #171 (owner, 2026-11-18: "I'd rather have a small window that shows the targets assigned to each ray and an ok and
// cancel button. This should also be the case for magic missile ... faithfully they should be able to be directed";
// the window before the cast, "A"): a player character's cast of Magic Missile or Scorching Ray is held, and a window
// picks each dart's or ray's target (rules/target_rows.h). OK casts it again, as it was ordered (its class, its slot,
// its metamagic); Cancel spends nothing.

// The targets the window sent with `caster`'s cast of `spell` (spells.2da row), one a dart or ray in row order - taken
// (the next cast asks again). Empty when none: an NPC's or a script's cast, or the window wasn't used - every dart or
// ray at the cast's own target.
std::vector<uint32_t> TakeWindowTargets(uint32_t caster, int spell);

// With spellcasting faithful: the cast order's guard, the window, the targeting event.
bool InstallTargetWindow();

// 0.99.107: a player character with a target window open (default_attack.cpp holds its attacks then).
bool PickingTargets(uint32_t player);
