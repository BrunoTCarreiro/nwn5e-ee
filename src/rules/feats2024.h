#pragma once

// CM8 (which feats exist), faithful: the feats a character can choose are
// the SRD 5.2 feats nwn5e has built — the Origin feats Alert (NWN's Improved
// Initiative, reused under the 2024 name), Savage Attacker and Skilled,
// Ability Score Improvement (progression.h), the four Fighting Styles
// (class_features.h), Weapon Mastery (C4). NWN's other feats can't be chosen
// any more (characters keep the ones they have). The rest of the SRD's 17
// join as they are built.

#include <vector>

#include "rules/combat.h"

namespace rules {

constexpr int kFeatAlert = 377;  // NWN's Improved Initiative
constexpr int kFeatSavageAttacker = 1300;  // data/rows/feat.origin.txt
constexpr int kFeatSkilled = 1301;

// Whether the creation and level-up screens may offer `feat` (faithful):
// with `masteries`, the Weapon Mastery feats (C4) too.
bool IsChoosableFeat2024(int feat, bool masteries);

// C4: the class bonus picks of one class level, by group — a general feat
// (Ability Score Improvement) at the class's feat levels, a Fighting Style
// at its style levels (`styles`: class_features faithful), the Weapon
// Mastery kinds the level adds (`masteries`). feat_cadence writes their sum
// as the class's bonus-feat slots; the screens' feat filter keeps each
// group to its count.
enum class BonusFeatGroup { kOther, kGeneral, kFightingStyle, kWeaponMastery };
BonusFeatGroup BonusFeatGroupOf(int feat);
struct BonusPicks {
  int general = 0;
  int style = 0;
  int mastery = 0;
  int Total() const { return general + style + mastery; }
};
BonusPicks BonusPicks2024(CharacterClass character_class, int class_level, bool styles, bool masteries);
// Whether this level's picks (`taken`, by group) fit `allowed`: each group
// in its own picks, and a Fighting Style may take the general pick too (the
// SRD's Fighting Style feats are general feats for a class that has them).
bool FitsBonusPicks(const BonusPicks& taken, const BonusPicks& allowed);

// Savage Attacker: the hit's weapon damage dice are rolled twice and one
// whole roll is used. A critical hit's dice are all of its sets (the weapon's
// dice once per multiplier), rolled together - not each set twice with its
// better half kept. `first` / `second` are the two rolls, set by set; true
// when the second is kept (the higher total; the first on a tie).
bool KeepSecondSavageRoll(const std::vector<int>& first, const std::vector<int>& second);

}  // namespace rules
