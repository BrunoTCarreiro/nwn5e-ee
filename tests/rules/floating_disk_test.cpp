#include "check.h"
#include "rules/floating_disk.h"
#include "rules/spells2024.h"

#include <string>

using namespace rules;

// #39 Part 2a build 4d (owner, 2026-11-16: "go with the henchman disk"): Floating Disk, SRD 5.2.1.
TEST(FloatingDiskEnds) {
  CHECK_EQ(DiskEndReason(true, true, 10.0f, 0, true) == DiskEnd::kNone, true);
  // Up to 500 pounds (GetWeight's tenths); more ends it.
  CHECK_EQ(DiskEndReason(true, true, 10.0f, 5000, true) == DiskEnd::kNone, true);
  CHECK_EQ(DiskEndReason(true, true, 10.0f, 5001, true) == DiskEnd::kOverloaded, true);
  // More than 100 feet from it, or another area.
  CHECK_EQ(DiskEndReason(true, true, 100.0f, 0, true) == DiskEnd::kNone, true);
  CHECK_EQ(DiskEndReason(true, true, 100.5f, 0, true) == DiskEnd::kTooFar, true);
  CHECK_EQ(DiskEndReason(true, false, 0.0f, 0, true) == DiskEnd::kTooFar, true);
  // Its hour, its caster.
  CHECK_EQ(DiskEndReason(true, true, 10.0f, 0, false) == DiskEnd::kHourUp, true);
  CHECK_EQ(DiskEndReason(false, true, 10.0f, 9999, true) == DiskEnd::kMasterGone, true);
  CHECK_EQ(DiskEndReason(true, false, 0.0f, 9999, false) == DiskEnd::kHourUp, true);  // the hour first
  // #168: its words in data/text/en.txt by key (tests/data/text_catalog_test.cpp: every key there).
  CHECK_EQ(DiskEndKey(DiskEnd::kNone)[0] == '\0', true);
  CHECK_EQ(std::string(DiskEndKey(DiskEnd::kOverloaded)) == "disk.why.overloaded", true);
  CHECK_EQ(std::string(DiskEndKey(DiskEnd::kSentAway)) == "disk.why.sent_away", true);
}

TEST(FloatingDiskFollowsAndOrders) {
  CHECK_EQ(DiskFollows(20.0f, false), false);  // immobile within 20 feet
  CHECK_EQ(DiskFollows(20.5f, false), true);
  CHECK_EQ(DiskFollows(60.0f, true), false);   // told to stand its ground
  CHECK_EQ(DiskOrderOf(-1) == DiskOrder::kOpen, true);
  CHECK_EQ(DiskOrderOf(-19) == DiskOrder::kOpen, true);
  CHECK_EQ(DiskOrderOf(-2) == DiskOrder::kStay, true);
  CHECK_EQ(DiskOrderOf(-5) == DiskOrder::kFollow, true);
  CHECK_EQ(DiskOrderOf(-17) == DiskOrder::kEnd, true);
  CHECK_EQ(DiskOrderOf(-3) == DiskOrder::kNone, true);  // attack the nearest: it can't
  CHECK_EQ(DiskOrderOf(0) == DiskOrder::kNone, true);
  CHECK_EQ(kFloatingDiskPounds, 500);
  CHECK_EQ(kFloatingDiskStrength >= 26, true);  // 500 pounds stay a normal load (encumbrance.2da's 26: 613 pounds)
}

// 0.99.55: its stages and its pile.
TEST(FloatingDiskStages) {
  CHECK_EQ(DiskStageOf(0) == DiskStage::kEmpty, true);
  CHECK_EQ(DiskStageOf(1) == DiskStage::kSmall, true);
  CHECK_EQ(DiskStageOf(1666) == DiskStage::kSmall, true);   // a third of 500 pounds: 166.7
  CHECK_EQ(DiskStageOf(1667) == DiskStage::kMedium, true);
  CHECK_EQ(DiskStageOf(3333) == DiskStage::kMedium, true);  // two thirds: 333.3
  CHECK_EQ(DiskStageOf(3334) == DiskStage::kLarge, true);
  CHECK_EQ(DiskStageOf(5000) == DiskStage::kLarge, true);
  CHECK_EQ(DiskStageOf(5400) == DiskStage::kLarge, true);   // an overload: its last pile
  CHECK_EQ(DiskPileBlueprint(DiskStage::kEmpty) == nullptr, true);  // nothing on it, nothing left
  // 0.99.56: the animation session's art - appearance 14000-14003, piles n5_fd_pile1-3 (placeables 14001-14003).
  CHECK_EQ(std::string(DiskPileBlueprint(DiskStage::kSmall)) == "n5_fd_pile1", true);
  CHECK_EQ(std::string(DiskPileBlueprint(DiskStage::kMedium)) == "n5_fd_pile2", true);
  CHECK_EQ(std::string(DiskPileBlueprint(DiskStage::kLarge)) == "n5_fd_pile3", true);
  CHECK_EQ(DiskStageLook(DiskStage::kEmpty).appearance, 14000);
  CHECK_EQ(DiskStageLook(DiskStage::kSmall).appearance, 14001);
  CHECK_EQ(DiskStageLook(DiskStage::kMedium).appearance, 14002);
  CHECK_EQ(DiskStageLook(DiskStage::kLarge).appearance, 14003);
  CHECK_EQ(DiskStageLook(DiskStage::kLarge).scale == 1.0f && DiskStageLook(DiskStage::kEmpty).scale == 1.0f, true);
}

TEST(FloatingDiskSpell) {
  const Spell2024* spell = Spell2024Of(kFloatingDisk);
  CHECK_EQ(spell != nullptr && spell->ritual, true);
  CHECK_EQ(IsRitualSpell(kFloatingDisk), true);
  CHECK_EQ(CastingMinutes(kFloatingDisk), 0);  // an Action - or 10 minutes as a Ritual
  CHECK_EQ(RitualCastingMinutes(CastingMinutes(kFloatingDisk)), 10);
  CHECK_EQ(WizardOnlySpell(kFloatingDisk), true);  // the Wizard's alone of the Wiz_Sorc column
}
