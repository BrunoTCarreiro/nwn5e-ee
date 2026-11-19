#include "data/anim_target_count.h"

namespace data {

int TargetCountRow(int count) {
  if (count < 1 || count > kTargetCountMax) return -1;
  return kTargetCountFirstRow + count - 1;
}

std::string TargetCountModel(int count) {
  if (count < 1 || count > kTargetCountMax) return std::string();
  const char digits[3] = {static_cast<char>('0' + count / 10), static_cast<char>('0' + count % 10), 0};
  return std::string("n5_tcnt_") + digits;
}

}  // namespace data
