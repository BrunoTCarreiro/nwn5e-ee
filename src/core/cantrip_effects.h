#pragma once

// #39 Part 1, build 2: what the cantrips' effects change in other rules (rules_cantrips.cpp).

// Guidance on a skill check by `creature` (a CNWSCreature) with skills.2da row `nwn_skill`: 1d4 (rolled, logged)
// when an active Guidance covers the skill, else 0. A master Guidance (cast with no skill) takes the first skill
// checked.
int GuidanceCheckBonus(void* creature, int nwn_skill);
