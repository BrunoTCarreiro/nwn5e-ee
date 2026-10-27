#pragma once

// Grapple, Shove's two choices and Throw (docs/PLAN.md "Grapple and Throw",
// GT1-GT4; faithful only). SRD 5.2.1: Unarmed Strike (p. 190), Grappled and
// Grappling (p. 182), the Grappler feat (p. 87). Throw is nwn5e's house rule
// on the Kraken's Fling (p. 303) with the Falling damage (p. 182), owner
// 2026-10-27. Sizes are NWN's: 1 Tiny, 2 Small, 3 Medium, 4 Large, 5 Huge.

namespace rules {

// nwn5e's feats for the options (data/rows/feat.unarmed.txt): past the game's
// rows (1116+), so the engine resolves them as an ordinary attack (unarmed.h).
constexpr int kFeatGrapple = 1312;    // G2
constexpr int kFeatShovePush = 1313;  // G1

// A Shove pushes the target 5 ft away or knocks it Prone - the shover's choice:
// NWN's Knockdown (feat 23, "Shove") knocks it Prone, nwn5e's "Shove: push"
// pushes it.
constexpr float kShovePushMetres = 1.524f;  // 5 ft

// The Grapple's save DC and its escape DC: 8 + Strength modifier + PB (as the Shove's).
int GrappleDc(int strength_modifier, int proficiency_bonus);
// At most one size larger than the grappler.
bool GrappleSizeAllowed(int grappler_size, int target_size);

// Dragging: every foot costs 1 extra (half speed) unless the grappled creature
// is Tiny or two or more sizes smaller - or, with the Grappler feat, the
// grappler's size or smaller.
bool DragCostsExtra(int grappler_size, int target_size, bool grappler_feat);
float DragSpeedFactor(int grappler_size, int target_size, bool grappler_feat);

// A grappled creature has Disadvantage on attacks against anyone but its grappler.
bool GrappledAttackHasDisadvantage(bool target_is_grappler);

// GT3: a grappled creature the game moves (not a player) fights its grappler
// if that is whom it was fighting, else spends its round trying to escape.
bool GrappledCreatureTriesToEscape(bool fighting_grappler);
// The escape check: the better of Athletics and Acrobatics against the escape DC.
int EscapeCheckBonus(int athletics_bonus, int acrobatics_bonus);
// A check meets or beats its DC.
bool EscapeSucceeds(int d20, int bonus, int dc);

// G2 (owner, 2026-10-27: "go with the recommendations" - D1-D8):
// D1: grappling takes a free hand - an empty main hand, or a one-handed weapon
// with nothing in the off hand (a two-handed weapon or a full off hand: none).
bool GrappleHandFree(bool main_hand_item, bool main_hand_two_handed, bool off_hand_item);
// D5: the grappled creature is held at the distance it was grabbed at (1 to 2 m
// between them); when the grappler moves farther, it is dragged back to that
// distance. Farther than that plus kGrappleBreakSlackMetres (pushed, stuck
// behind a wall, teleported): out of reach - the grapple ends (D6).
constexpr float kGrappleHoldMinMetres = 1.0f;
constexpr float kGrappleHoldMaxMetres = 2.0f;
constexpr float kGrappleBreakSlackMetres = 1.5f;
float GrappleHoldMetres(float distance_at_grab);
bool GrappleOutOfReach(float distance, float hold_metres);
// GT3: a grappled creature's escape try comes once a round (6 s).
constexpr int kGrappleEscapeEveryMs = 6000;

// Throw (GT2): only a creature you grapple, at least one size smaller than you.
bool ThrowSizeAllowed(int thrower_size, int target_size);
// 5 ft x Strength modifier, at least 5 ft.
int ThrowFeet(int strength_modifier);
float FeetToMetres(int feet);
// 1d6 Bludgeoning per 10 ft thrown, at least 1d6; a success halves it and
// avoids the Prone.
int ThrowDamageDice(int feet);
int ThrowDamageTaken(int rolled, bool saved);

}  // namespace rules
