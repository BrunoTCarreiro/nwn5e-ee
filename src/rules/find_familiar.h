#pragma once

// #39 Part 2a, build 4c (owner, 2026-11-16: "go with your recommendations for find familiar, excep allow the player
// to pick alignment too"): Find Familiar (SRD 5.2.1). Its familiar takes one of the eight forms NWN can show (Q1 A),
// with the form's numbers (a CR 0 Beast's), as a Celestial, Fey or Fiend (Q2: by the caster's alignment, or the
// player's pick). It can't attack; its master sees through its eyes (NWN's possession, a Bonus Action) and its touch
// delivers its master's touch spells within 100 feet (its Reaction). The rows are in rules/spells2024.h.

namespace rules {

enum class FamiliarForm { kNone, kBat, kCat, kHawk, kLizard, kRat, kRaven, kSpider, kWeasel };
// Find Familiar's menu row (kFamiliarBat ... kFamiliarWeasel, or a form-and-kind row): its form; kNone for any other.
FamiliarForm FamiliarFormOf(int row);
// A form-and-kind row's kind (0.99.49: picked in the menu, between the form and the slot level): 1 Celestial, 2 Fey,
// 3 Fiend; 0 for a form's own row (by alignment) or any other.
int FamiliarKindOfRow(int row);

// A form's numbers (SRD 5.2.1's Bat, Cat, Hawk, Lizard, Rat, Raven, Spider, Weasel) and its NWN look: the game's
// creature model (appearance.2da) and how it's scaled to the form's size - the bat, falcon, rat and raven as they
// are, the cougar (a cat), basilisk (a lizard), giant spider and badger (a weasel) shrunk.
struct FamiliarStats {
  const char* name = "";
  int armor_class = 10;
  int hit_points = 1;
  int abilities[6] = {10, 10, 10, 10, 10, 10};  // Strength .. Charisma
  int appearance = 0;
  float scale = 1.0f;
};
// 0.99.49 (owner, 2026-11-16: "Cat, lizard and spider are very small, especially the lizard and spider familiars. Rat
// is too big"): the cat 0.5, the lizard 0.35, the spider 0.3, the rat 0.6.
const FamiliarStats& FamiliarStatsOf(FamiliarForm form);

// Celestial, Fey or Fiend. The pick (the menu's, FamiliarKindOfRow): 0 by its alignment - good a Celestial, evil a
// Fiend, else a Fey - or the kind itself (1-3).
enum class FamiliarKind { kCelestial = 1, kFey = 2, kFiend = 3 };
constexpr int kFamiliarByAlignment = 0;
FamiliarKind FamiliarKindFor(int pick, int nwn_good_evil);
const char* FamiliarKindName(FamiliarKind kind);
// The familiar's NWN racial type (Outsider 20 for a Celestial or a Fiend, Fey 17) and alignment (ALIGNMENT_GOOD 4,
// ALIGNMENT_EVIL 5, ALIGNMENT_NEUTRAL 1): what Detect Evil and Good and Protection from Evil and Good read.
int FamiliarRacialType(FamiliarKind kind);
int FamiliarGoodEvil(FamiliarKind kind);

// Its telepathy, its touch's delivery and its recall from the pocket dimension, in feet.
constexpr int kFamiliarTelepathyFeet = 100;
constexpr int kFamiliarRecallFeet = 30;
constexpr int kFamiliarReachFeet = 5;
// A touch spell its familiar delivers (its Reaction): the target out of the caster's reach, within the familiar's,
// the familiar within 100 feet of the caster, its Reaction ready.
bool FamiliarDeliversTouch(float target_from_caster_feet, float target_from_familiar_feet,
                           float familiar_from_caster_feet, bool reaction_ready);

}  // namespace rules
