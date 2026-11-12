#pragma once

#include <string>

// #131 (owner, 2026-11-04: "most living enemies would avoid hazards, while undead and enemies that are charmed or
// somehow dominated would not"; 2 B: the spells' harmful areas too; 3 B: step out and keep out) - core/hazard_watch.cpp.

namespace rules {

// What a hazard area hurts: the grenade hazards anyone (Holy Water's patch only undead and evil outsiders), the
// harmful spell areas whom NWN's area-spell rule picks (x0_i0_spells' spellsIsTarget, SPELL_TARGET_STANDARDHOSTILE).
// #39 build 2c, #160: Entangle's Difficult Terrain, Grease, a Fog Cloud (anyone in them; who leaves one, and how:
// TerrainAreaStep).
enum class HazardKind { kNone, kAnyone, kHolyWater, kSpell, kDifficultTerrain, kGrease, kFog };

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

// #39 build 2c (owner, 2026-11-12: "Hazard avoidance, sentient creatures with ranged attacks should still try to avoid
// these, but not as much melee only sentient creatures."), revised by #160 (owner, 2026-11-12: "We need to work on enemies avoiding hazard areas, if it's just difficult terrain or low visibility an enemy probably shouldn't be running from it like the plague ...", then "go with A on all"):
// - Sentient: not an animal, a beast, vermin or an ooze. Q1 A, careful: the Intelligence and Wisdom modifiers
//   together 0 or more - a dumb one plows through.
// - A Fog Cloud is left to no one (Q2 A: a lost target in it is searched). Entangle's Difficult Terrain: a careful
//   sentient avoider walks out once, not running - holding a ranged weapon, or melee-only with its attack target not
//   inside. Grease (Q3 A, a fall): the same creatures keep out of it as of a hazard. Never one that can't move
//   (held); no attack is held for these areas.
bool IsSentientRacialType(int nwn_racial_type);
bool IsCarefulMind(int intelligence_modifier, int wisdom_modifier);
enum class TerrainStep { kStay, kWalkOutOnce, kKeepOut };
TerrainStep TerrainAreaStep(HazardKind kind, bool avoids, int racial_type, bool careful, bool ranged, bool target_inside,
                            bool held);
// Q2 A: a creature the game moves (in control of itself) that loses its enemy in a Fog Cloud searches - it walks to
// where it last saw it, then to a spot within 3 m of that each round, three rounds, then gives up.
constexpr int kFogSearchRounds = 3;
constexpr float kFogSearchMetres = 3.0f;
bool SearchesFog(bool ai_controlled, bool in_control, bool target_enemy, bool target_in_fog, bool has_attack_target);

}  // namespace rules
