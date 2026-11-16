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
constexpr int kFloatingDiskAppearance = 103;   // a placeholder body - NWN's Lantern Archon - until the animation
constexpr float kFloatingDiskScale = 0.5f;     // session's disk

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

}  // namespace rules
