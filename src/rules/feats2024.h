#pragma once

// CM8 (which feats exist), faithful: the feats a character can choose are
// the SRD 5.2 feats nwn5e has built — the Origin feats Alert (NWN's Improved
// Initiative, reused under the 2024 name), Savage Attacker and Skilled,
// Ability Score Improvement (progression.h), the four Fighting Styles
// (class_features.h), Weapon Mastery (C4). NWN's other feats can't be chosen
// any more (characters keep the ones they have). The rest of the SRD's 17
// join as they are built.

#include <vector>

#include <optional>

#include "rules/abilities.h"
#include "rules/combat.h"
#include "rules/progression.h"

namespace rules {

constexpr int kFeatAlert = 377;  // NWN's Improved Initiative
constexpr int kFeatSavageAttacker = 1300;  // data/rows/feat.origin.txt
constexpr int kFeatSkilled = 1301;
// C7: Skilled is Repeatable (SRD 5.2) — the later picks are these rows
// (data/rows/feat.skilled.txt), each needing the one before.
constexpr int kFeatSkilledAgainFirst = 1302;
constexpr int kSkilledRepeats = 6;
bool IsSkilledFeat(int feat);
bool IsOriginFeat2024(int feat);  // Alert, Savage Attacker, Skilled (any of its rows)

// C7: Ability Score Improvement as feats with its choices. The k-th ASI
// (k 1..kAsiFeatCount) is a master feat (masterfeats row kAsiMasterFirst +
// k - 1) whose 21 sub-feats (feat rows kAsiSplitFeatFirst + 21 (k - 1) +
// split) are its choices: splits 0-5 = +2 to Strength .. Charisma, 6-20 = +1
// to two (Str+Dex, Str+Con, Str+Int, Str+Wis, Str+Cha, Dex+Con, ... Wis+Cha).
// Older characters' ASIs (feat rows kAsiFeatFirst.., abilities from !asi)
// still count; new ones are taken as these.
constexpr int kAsiSplits = 21;
constexpr int kAsiSplitFeatFirst = 1400;
constexpr int kAsiMasterFirst = 45;
AbilityIncrease AsiSplit(int split);                 // split 0..20
int AsiSplitIndex(const AbilityIncrease& increase);  // -1 if not a legal increase
int AsiSplitFeat(int slot, int split);               // -1 out of range
struct AsiFeatChoice {
  int slot;  // 1-based
  AbilityIncrease increase;
};
std::optional<AsiFeatChoice> AsiFeatFromId(int feat);
// Whether `feat` is the next ASI's choice for a character who has made
// `owned_asis` of them (older ASI rows and these together).
bool IsNextAsiChoice(int feat, int owned_asis);
// Whether an increase raises any of its abilities (`scores`: Str .. Cha);
// one that only raises abilities already at 20 changes nothing.
bool AsiChangesSomething(const AbilityIncrease& increase, const int scores[6]);

// Whether the creation and level-up screens may offer `feat` (faithful):
// with `masteries`, the Weapon Mastery feats (C4) too.
bool IsChoosableFeat2024(int feat, bool masteries);

// C4: the class bonus picks of one class level, by group — a general feat
// (Ability Score Improvement) at the class's feat levels, a Fighting Style
// at its style levels (`styles`: class_features faithful), the Weapon
// Mastery kinds the level adds (`masteries`). feat_cadence writes their sum
// as the class's bonus-feat slots; the screens' feat filter keeps each
// group to its count.
enum class BonusFeatGroup { kOther, kGeneral, kFightingStyle, kWeaponMastery, kOrigin };
BonusFeatGroup BonusFeatGroupOf(int feat);
struct BonusPicks {
  int general = 0;
  int style = 0;
  int mastery = 0;
  int origin = 0;  // Origin feats only: a Human's Versatile pick at level 1 (set by the caller)
  int Total() const { return general + style + mastery + origin; }
};
BonusPicks BonusPicks2024(CharacterClass character_class, int class_level, bool styles, bool masteries);
// Whether this level's picks (`taken`, by group) fit `allowed`: each group
// in its own picks, and a Fighting Style or an Origin feat may take the
// general pick too (2024: an ASI level's feat is "the Ability Score
// Improvement feat or another feat for which you qualify" — the Fighting
// Style feats for a class that has the feature, the Origin feats for anyone).
bool FitsBonusPicks(const BonusPicks& taken, const BonusPicks& allowed);

// Savage Attacker: the hit's weapon damage dice are rolled twice and one
// whole roll is used. A critical hit's dice are all of its sets (the weapon's
// dice once per multiplier), rolled together - not each set twice with its
// better half kept. `first` / `second` are the two rolls, set by set; true
// when the second is kept (the higher total; the first on a tie).
bool KeepSecondSavageRoll(const std::vector<int>& first, const std::vector<int>& second);

}  // namespace rules
