#pragma once

namespace rules {

// #39 Part 2a, build 4d: Floating Disk (SRD 5.2.1) as a henchman disk (owner, 2026-11-16: "for the floating disk, can
// it work like a henchman? They have their own inventory, right?" - "go with the henchman disk"). Level 1 Conjuration,
// the Wizard's; an Action or a Ritual; an hour. The disk holds up to 500 pounds - more ends the spell and everything
// on it falls to the ground; it stays put while you're within 20 feet and follows you farther; more than 100 feet
// from it, the spell ends.
constexpr int kFloatingDiskPounds = 500;
constexpr int kFloatingDiskFollowFeet = 20;
constexpr int kFloatingDiskBreakFeet = 100;
constexpr int kFloatingDiskHours = 1;
constexpr int kFloatingDiskStrength = 30;      // its own load never slows it (encumbrance.2da: 1,213 pounds at 30)
// 0.99.56: the animation session's disk (anim 3d49e05, data/rows/appearance.floatdisk.txt): appearance 14000 the empty
// disk, 14001-14003 with small, medium and large junk - each the disk and its junk as the creature's own meshes on
// c_invsguy (clickable, it walks), at scale 1.
constexpr int kFloatingDiskAppearance = 14000;
constexpr float kFloatingDiskScale = 1.0f;

enum class DiskEnd { kNone, kMasterGone, kHourUp, kOverloaded, kTooFar, kSentAway };

// Why the spell ends now (kNone: it goes on). master_here: its caster still its master; same_area: in the caster's
// area (another area is past 100 feet); feet: its distance from the caster; weight_tenths: what it carries
// (GetWeight's tenths of pounds); hour_left: its mark still on it.
DiskEnd DiskEndReason(bool master_here, bool same_area, float feet, int weight_tenths, bool hour_left);
// The end's words to the caster ("" for kNone).
const char* DiskEndText(DiskEnd why);

// It moves to its caster once farther than 20 feet - unless told to stand its ground.
bool DiskFollows(float feet, bool standing);

enum class DiskOrder { kNone, kOpen, kStay, kFollow, kEnd };

// Its caster's word, by its conversation event's listen pattern (GetListenPatternNumber): -1 a click (no line
// matched) and -19 the associate radial's Inventory open what it carries; -2 Stand your ground; -5 Follow me; -17
// Leave the party ends the spell (its load falls). Other commands (attack, guard, heal ...) it can't do.
DiskOrder DiskOrderOf(int listen_pattern);

// 0.99.55 (owner, 2026-11-16: "4 stages to it: empty - just the disk; 0-33%, 33%-66%, 66%-99% of the weight they
// would have 3 different meshes of increasing size of random junk on top. once the disk either is unsummoned or
// breaks the mesh that was on top is placed where the disk was and works as a container"; "go with all your
// recommendations"): its look shows its load, and its load falls into that stage's pile - a container, gone once
// emptied.
enum class DiskStage { kEmpty = 0, kSmall = 1, kMedium = 2, kLarge = 3 };
// Nothing on it: empty; up to a third of 500 pounds: small; up to two thirds: medium; more: large.
DiskStage DiskStageOf(int weight_tenths);
struct DiskLook {
  int appearance = kFloatingDiskAppearance;
  float scale = kFloatingDiskScale;
};
// A stage's look: its appearance row (14000 + the stage) - the animation session's four (the disk and its junk as the
// creature's own meshes): the Lantern Archon a little larger each stage.
DiskLook DiskStageLook(DiskStage stage);
// The container a stage's load falls into where the disk floated (null for an empty disk): nwn5e's blueprints
// n5_fd_pile1-3 (data/files) - the animation session's piles (placeables 14001-14003, the stage's junk without the
// disk), usable, an inventory, plot, no treasure scripts.
const char* DiskPileBlueprint(DiskStage stage);

}  // namespace rules
