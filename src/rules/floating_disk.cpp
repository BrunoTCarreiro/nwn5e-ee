#include "rules/floating_disk.h"

namespace rules {

DiskEnd DiskEndReason(bool master_here, bool same_area, float feet, int weight_tenths, bool hour_left) {
  if (!master_here) return DiskEnd::kMasterGone;
  if (!hour_left) return DiskEnd::kHourUp;
  if (weight_tenths > kFloatingDiskPounds * 10) return DiskEnd::kOverloaded;
  if (!same_area || feet > static_cast<float>(kFloatingDiskBreakFeet)) return DiskEnd::kTooFar;
  return DiskEnd::kNone;
}

const char* DiskEndKey(DiskEnd why) {
  switch (why) {
    case DiskEnd::kMasterGone: return "disk.why.caster_gone";
    case DiskEnd::kHourUp: return "disk.why.hour";
    case DiskEnd::kOverloaded: return "disk.why.overloaded";
    case DiskEnd::kTooFar: return "disk.why.too_far";
    case DiskEnd::kSentAway: return "disk.why.sent_away";
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

DiskStage DiskStageOf(int weight_tenths) {
  if (weight_tenths <= 0) return DiskStage::kEmpty;
  const int limit = kFloatingDiskPounds * 10;
  if (weight_tenths * 3 <= limit) return DiskStage::kSmall;
  if (weight_tenths * 3 <= limit * 2) return DiskStage::kMedium;
  return DiskStage::kLarge;
}

DiskLook DiskStageLook(DiskStage stage) {
  DiskLook look;
  look.appearance = kFloatingDiskAppearance + static_cast<int>(stage);
  return look;
}

const char* DiskPileBlueprint(DiskStage stage) {
  switch (stage) {
    case DiskStage::kSmall: return "n5_fd_pile1";
    case DiskStage::kMedium: return "n5_fd_pile2";
    case DiskStage::kLarge: return "n5_fd_pile3";
    default: return nullptr;
  }
}

DiskFound DiskFoundAction(bool made_this_game, int seconds_left) {
  if (made_this_game) return DiskFound::kKeep;
  return seconds_left > 0 ? DiskFound::kRemake : DiskFound::kEnd;
}

}  // namespace rules
