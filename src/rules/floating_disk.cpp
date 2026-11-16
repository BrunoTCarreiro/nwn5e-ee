#include "rules/floating_disk.h"

namespace rules {

DiskEnd DiskEndReason(bool master_here, bool same_area, float feet, int weight_tenths, bool hour_left) {
  if (!master_here) return DiskEnd::kMasterGone;
  if (!hour_left) return DiskEnd::kHourUp;
  if (weight_tenths > kFloatingDiskPounds * 10) return DiskEnd::kOverloaded;
  if (!same_area || feet > static_cast<float>(kFloatingDiskBreakFeet)) return DiskEnd::kTooFar;
  return DiskEnd::kNone;
}

const char* DiskEndText(DiskEnd why) {
  switch (why) {
    case DiskEnd::kMasterGone: return "its caster is gone";
    case DiskEnd::kHourUp: return "its hour is up";
    case DiskEnd::kOverloaded: return "more than 500 pounds was put on it";
    case DiskEnd::kTooFar: return "you are more than 100 feet from it";
    case DiskEnd::kSentAway: return "you sent it away";
    default: return "";
  }
}

bool DiskFollows(float feet, bool standing) { return !standing && feet > static_cast<float>(kFloatingDiskFollowFeet); }

DiskOrder DiskOrderOf(int listen_pattern) {
  switch (listen_pattern) {
    case -1:   // a click: a conversation with no line matched
    case -19:  // ASSOCIATE_COMMAND_INVENTORY
      return DiskOrder::kOpen;
    case -2: return DiskOrder::kStay;    // ASSOCIATE_COMMAND_STANDGROUND
    case -5: return DiskOrder::kFollow;  // ASSOCIATE_COMMAND_FOLLOWMASTER
    case -17: return DiskOrder::kEnd;    // ASSOCIATE_COMMAND_LEAVEPARTY
    default: return DiskOrder::kNone;
  }
}

}  // namespace rules
