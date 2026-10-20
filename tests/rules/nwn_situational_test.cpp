#include "check.h"
#include "rules/nwn_situational.h"

using namespace rules;

// Breaks caught: a term's 3e value or sign wrong (the hook would leave part of
// the flat modifier in, or remove one the engine never added); prone using the
// melee value for ranged attacks.
TEST(NwnSituationalTermsAddUpLikeTheEngine) {
  CHECK_EQ(NwnSituationalTotal({}), 0);
  NwnSituationalTerms blind;
  blind.target_blind = true;
  CHECK_EQ(NwnSituationalTotal(blind), 2);
  NwnSituationalTerms invisible;
  invisible.attacker_invisible = true;
  CHECK_EQ(NwnSituationalTotal(invisible), 2);
  NwnSituationalTerms unseen;
  unseen.cant_see_target = true;
  CHECK_EQ(NwnSituationalTotal(unseen), -4);
  NwnSituationalTerms prone_melee;
  prone_melee.target_prone = true;
  CHECK_EQ(NwnSituationalTotal(prone_melee), 4);
  NwnSituationalTerms prone_ranged = prone_melee;
  prone_ranged.ranged = true;
  CHECK_EQ(NwnSituationalTotal(prone_ranged), -4);
  NwnSituationalTerms stunned;
  stunned.target_stunned = true;
  CHECK_EQ(NwnSituationalTotal(stunned), 2);
  NwnSituationalTerms flanked;
  flanked.target_flanked = true;
  CHECK_EQ(NwnSituationalTotal(flanked), 2);
  // The 0.6.1 bow test, shot 2: a goblin still running up and already within
  // its reach took the shot from +4 to -2.
  NwnSituationalTerms shot2;
  shot2.ranged = true;
  shot2.target_moving = true;
  shot2.ranged_in_melee = true;
  CHECK_EQ(NwnSituationalTotal(shot2), -6);
}

// Phase 1b (decided 2026-10-18). Breaks caught: a 2024 source missing
// (unseen target, prone at range -> Disadvantage; blinded, invisible attacker,
// stunned, prone in melee -> Advantage); a 3e term with no 2024 rule turned
// into one (moving target; flanking, B2 off); NWN's target-only "ranged in
// melee" term deciding the mode instead of the 2024 close-combat check (B5);
// sources not cancelling.
TEST(NwnSituationsBecomeAdvantageOrDisadvantage) {
  CHECK_EQ(SituationalRollMode({}, false), RollMode::kNormal);
  NwnSituationalTerms nwn_close_shot;
  nwn_close_shot.ranged = true;
  nwn_close_shot.ranged_in_melee = true;
  CHECK_EQ(SituationalRollMode(nwn_close_shot, false), RollMode::kNormal);
  CHECK_EQ(SituationalRollMode(nwn_close_shot, true), RollMode::kDisadvantage);
  NwnSituationalTerms shot_at_range;
  shot_at_range.ranged = true;
  CHECK_EQ(SituationalRollMode(shot_at_range, true), RollMode::kDisadvantage);
  NwnSituationalTerms prone_melee;
  prone_melee.target_prone = true;
  CHECK_EQ(SituationalRollMode(prone_melee, false), RollMode::kAdvantage);
  NwnSituationalTerms prone_ranged = prone_melee;
  prone_ranged.ranged = true;
  CHECK_EQ(SituationalRollMode(prone_ranged, false), RollMode::kDisadvantage);
  NwnSituationalTerms stunned;
  stunned.target_stunned = true;
  CHECK_EQ(SituationalRollMode(stunned, false), RollMode::kAdvantage);
  NwnSituationalTerms blind;
  blind.target_blind = true;
  CHECK_EQ(SituationalRollMode(blind, false), RollMode::kAdvantage);
  NwnSituationalTerms invisible;
  invisible.attacker_invisible = true;
  CHECK_EQ(SituationalRollMode(invisible, false), RollMode::kAdvantage);
  NwnSituationalTerms unseen;
  unseen.cant_see_target = true;
  CHECK_EQ(SituationalRollMode(unseen, false), RollMode::kDisadvantage);
  NwnSituationalTerms moving;
  moving.ranged = true;
  moving.target_moving = true;
  CHECK_EQ(SituationalRollMode(moving, false), RollMode::kNormal);
  NwnSituationalTerms flanked;
  flanked.target_flanked = true;
  CHECK_EQ(SituationalRollMode(flanked, false), RollMode::kNormal);
  NwnSituationalTerms invisible_archer = shot_at_range;
  invisible_archer.attacker_invisible = true;
  CHECK_EQ(SituationalRollMode(invisible_archer, true), RollMode::kNormal);
}

// B5 + B3. Breaks caught: a melee attack given the ranged close-combat
// Disadvantage; Point Blank Shot not cancelling it; no enemy nearby still
// counted.
TEST(RangedAttacksInCloseCombat) {
  CHECK_EQ(RangedCloseCombat(true, false, 1), true);
  CHECK_EQ(RangedCloseCombat(true, false, 3), true);
  CHECK_EQ(RangedCloseCombat(true, true, 1), false);
  CHECK_EQ(RangedCloseCombat(false, false, 2), false);
  CHECK_EQ(RangedCloseCombat(true, false, 0), false);
}

// D4: Vex / Sap count as ordinary advantage / disadvantage sources and cancel
// with the situational ones. Breaks caught: mastery overriding the situation;
// an advantage + disadvantage pair not cancelling.
TEST(MasteryRollSourcesCancel) {
  NwnSituationalTerms none;
  CHECK_EQ(static_cast<int>(SituationalRollMode(none, false, 1, 0)), static_cast<int>(RollMode::kAdvantage));
  CHECK_EQ(static_cast<int>(SituationalRollMode(none, false, 0, 1)), static_cast<int>(RollMode::kDisadvantage));
  CHECK_EQ(static_cast<int>(SituationalRollMode(none, false, 1, 1)), static_cast<int>(RollMode::kNormal));
  CHECK_EQ(static_cast<int>(SituationalRollMode(none, true, 1, 0)), static_cast<int>(RollMode::kNormal));
}
