#include "data/anim_area_copies.h"

namespace data {

int FirstCopyStart(int group1_count, int start0, int alive0) {
  return group1_count > 1 && start0 == 0 && alive0 == 0 ? kFirstCopyStartMs : start0;
}

}  // namespace data
