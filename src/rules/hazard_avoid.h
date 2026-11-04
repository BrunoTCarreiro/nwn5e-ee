#pragma once

#include <string>

// #131 (owner, 2026-11-04: "most living enemies would avoid hazards, while undead and enemies that are charmed or
// somehow dominated would not"; 2 B: the spells' harmful areas too; 3 B: step out and keep out) - core/hazard_watch.cpp.

namespace rules {

// What a hazard area hurts: the grenade hazards anyone (Holy Water's patch only undead and evil outsiders), the
// harmful spell areas whom NWN's area-spell rule picks (x0_i0_spells' spellsIsTarget, SPELL_TARGET_STANDARDHOSTILE).
enum class HazardKind { kNone, kAnyone, kHolyWater, kSpell };

// A hazard area by one of its scripts (the enter, exit or round script vfx_persistent.2da or EffectAreaOfEffect names;
// case-blind): #130's flask patches, the Bombs' clouds, Caltrops, Choking Powder; Acid Fog, Incendiary Cloud, Stinking
// Cloud, Cloudkill, Wall of Fire, Blade Barrier, Web, Entangle, Grease, Creeping Doom, Evard's Black Tentacles, Storm
// of Vengeance, Cloud of Bewilderment, Stonehold, Ghoul Touch. None for the rest (Darkness, the auras ...).
HazardKind HazardOfScript(const std::string& name);

// How far from its centre a creature goes to be out of it: the area's radius (a wall: half its length) + 1.5 m; 0 for
// a script that isn't a hazard's.
float HazardStepOutMetres(const std::string& name);

constexpr int kRacialTypeConstruct = 10;
constexpr int kRacialTypeOutsider = 20;
constexpr int kRacialTypeUndead = 24;

// Who keeps out of a hazard: not a player's character, living (not undead, not a construct) and in control of itself
// (not charmed, dominated or confused). With the grenades faithful.
bool AvoidsHazards(bool faithful, bool player_controlled, int racial_type, bool not_in_control);

// 3B: an avoiding creature's melee attack on a target standing in a hazard that would hurt the attacker, while the
// attacker stands outside it, waits (the hazard is short); a ranged attack goes on.
bool HazardHoldsAttack(bool avoids, bool would_be_hurt, bool target_inside, bool attacker_inside, bool ranged);

}  // namespace rules
