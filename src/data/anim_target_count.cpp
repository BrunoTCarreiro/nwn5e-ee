#include "data/anim_target_count.h"

namespace data {

namespace {

bool CountIn(int count) { return count >= 1 && count <= kTargetCountMax; }
bool StyleIn(int style) { return style >= 0 && style < kTargetTallyStyles; }

std::string TwoDigits(int count) {
  const char digits[3] = {static_cast<char>('0' + count / 10), static_cast<char>('0' + count % 10), 0};
  return digits;
}

}  // namespace

int TargetCountRow(int count) {
  if (!CountIn(count)) return -1;
  return kTargetCountFirstRow + count - 1;
}

std::string TargetCountModel(int count) {
  if (!CountIn(count)) return std::string();
  return "n5_tcnt_" + TwoDigits(count);
}

int TargetTallyRow(int style, int count) {
  if (!StyleIn(style) || !CountIn(count)) return -1;
  return kTargetTallyFirstRow + style * kTargetCountMax + count - 1;
}

std::string TargetTallyStyleCode(int style) {
  static const char* const kCodes[kTargetTallyStyles] = {"mm", "sr", "eb"};
  return StyleIn(style) ? kCodes[style] : std::string();
}

std::string TargetTallyModel(int style, int count) {
  if (!StyleIn(style) || !CountIn(count)) return std::string();
  return "n5_tf_" + TargetTallyStyleCode(style) + TwoDigits(count);
}

std::string TargetTallyGlowBitmap(int style, int count) {
  if (!StyleIn(style) || !CountIn(count)) return std::string();
  return "n5_tfg_" + TargetTallyStyleCode(style) + TwoDigits(count);
}

std::string TargetTallyBarsBitmap(int count) {
  if (!CountIn(count)) return std::string();
  return "n5_tfb_" + TwoDigits(count);
}

}  // namespace data
