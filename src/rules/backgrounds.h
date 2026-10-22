#pragma once

// Phase 3 CM5 (PLAN.md): the four SRD 5.2 backgrounds. Non-SRD backgrounds
// belong in the private layer.

#include <optional>
#include <string>
#include <vector>

#include "rules/abilities.h"
#include "rules/combat.h"
#include "rules/skills.h"

namespace rules {

enum class Background { kNone, kAcolyte, kCriminal, kSage, kSoldier };

struct BackgroundInfo {
  Background background;
  const char* name;
  Ability abilities[3];     // the three the increases come from
  const char* origin_feat;  // its Origin feat
  SkillSet skills;          // its two skills (and Thieves' Tools for the Criminal)
  const char* tool;
};

const std::vector<BackgroundInfo>& Backgrounds2024();
const BackgroundInfo* BackgroundData(Background background);
std::optional<Background> ParseBackground(const std::string& text);
// C6: the feat row of a background's Origin feat, -1 when nwn5e hasn't built
// it (Magic Initiate waits for spells).
int OriginFeatId(Background background);

// The increases from `chosen` (two abilities: +2 / +1; three: +1 each), all
// from the background's three. "" when valid (amounts filled), else why not.
std::string BackgroundIncreases(Background background, const std::vector<Ability>& chosen, int amounts[6]);

// Saved forms (local variables): amounts 0..3 per ability in 2 bits each; an
// ASI as (first + 1) + (second + 1) * 8, 0 = not chosen.
int EncodeAmounts(const int amounts[6]);
void DecodeAmounts(int encoded, int amounts[6]);
int EncodeAsi(const AbilityIncrease& asi);
std::optional<AbilityIncrease> DecodeAsi(int encoded);

// Background feats (docs/CHARGEN.md): the creation screen's level-1 general
// pick offers each background as a master feat whose sub-feats are its seven
// ability splits — feat.2da row kBackgroundFeatFirst + 7 x (background - 1)
// + split, where splits 0-5 are +2 / +1 over the background's three
// abilities in the order (1,2) (1,3) (2,1) (2,3) (3,1) (3,2), and 6 is +1
// each. The feat is the record: saved with the character, exports included.
constexpr int kBackgroundFeatFirst = 1230;
constexpr int kBackgroundSplits = 7;
constexpr int kBackgroundFeatCount = 4 * kBackgroundSplits;

// The feat row for a background and split, or -1.
int BackgroundFeat(Background background, int split);

struct BackgroundFeatChoice {
  Background background;
  std::vector<Ability> chosen;  // as BackgroundIncreases takes them
};
// The background and abilities a feat row stands for (nullopt: not one).
std::optional<BackgroundFeatChoice> BackgroundFromFeat(int feat);

// The creation page's Recommended choice: a background that suits the class
// (its main abilities among the background's three), and the split giving +2
// and +1 to the background's two abilities the class ranks highest
// (RecommendedScores2024's order).
Background RecommendedBackground2024(CharacterClass character_class);
int RecommendedBackgroundSplit2024(Background background, CharacterClass character_class);

// C2 (docs/CHARGEN.md): the background's equipment (SRD 5.2 option A) as NWN
// items, given when a character made on the creation screen enters. Items
// with no NWN counterpart (holy symbol, books, parchment, calligrapher's
// supplies, crowbar, pouches, a gaming set, clothes, the quiver) are left out.
struct KitItem {
  const char* blueprint;  // an NWN item blueprint (resref)
  int count;              // how many (separate items, or one stack's size)
  bool one_stack = false;  // ammunition: one stack of `count`
};
struct BackgroundKit {
  std::vector<KitItem> items;
  int gold;
};
BackgroundKit BackgroundKit2024(Background background);

}  // namespace rules
