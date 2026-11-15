#pragma once

// #166 (owner, 2026-11-15: "what about half-cover?" - "go with your recommendations"): 2024 Cover (SRD 5.2) -
// Half Cover +2 to AC and Dexterity saves, Three-Quarters Cover +5, Total Cover: no attack or spell targets it
// directly; only the most protective degree applies. Part 1: a creature in the way gives Half Cover against an
// attack (faithful). Walls and objects, Three-Quarters Cover and Dexterity saves: part 2, with #9's sight lines.

namespace rules {

enum class Cover { kNone, kHalf, kThreeQuarters, kTotal };

// A degree of cover's AC bonus: 0, +2, +5; Total Cover 0 (no attack reaches it to roll against).
int CoverArmorClassBonus(Cover cover);
// The more protective of two degrees.
Cover MoreCover(Cover a, Cover b);

// How far a creature's body reaches from its centre, in metres, by NWN size (1 Tiny .. 5 Huge): Tiny 0.2, Small
// and Medium 0.4, Large 0.8, Huge 1.2 - nwn5e's estimate (NWN has no grid), to tune in play.
float CreatureBodyRadius(int nwn_size);
// A Tiny creature gives no cover: it blocks less than half of anything bigger.
bool SizeGivesCover(int nwn_size);

// A creature at (bx, by) whose body reaches `radius` stands in the way of an attack from (ax, ay) at (tx, ty): its
// body touches the line between them, and it stands between them - past the attacker's own body
// (`attacker_radius`) and short of the target's (`target_radius`).
bool CreatureInTheWay(float ax, float ay, float tx, float ty, float bx, float by, float radius, float attacker_radius,
                      float target_radius);

}  // namespace rules
