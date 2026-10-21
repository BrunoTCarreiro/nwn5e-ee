#pragma once

// CM8 (which feats exist), faithful: the feats a character can choose are
// the SRD 5.2 feats nwn5e has built — Alert (NWN's Improved Initiative,
// reused under the 2024 name), Ability Score Improvement (progression.h), the
// four Fighting Styles (class_features.h). NWN's other feats can't be chosen
// any more (characters keep the ones they have). The rest of the SRD's 17
// join as they are built.

#include "rules/combat.h"

namespace rules {

constexpr int kFeatAlert = 377;  // NWN's Improved Initiative

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

}  // namespace rules
