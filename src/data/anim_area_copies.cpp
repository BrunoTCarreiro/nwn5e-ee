#include "data/anim_area_copies.h"

namespace data {

int FirstCopyStart(int group1_count, int start0, int alive0) {
  return group1_count > 1 && start0 == 0 && alive0 == 0 ? kFirstCopyStartMs : start0;
}

bool IsOwnAreaModel(const char* name, size_t size) {
  return name && size >= 2 && (name[0] == 'n' || name[0] == 'N') && name[1] == '5';
}

bool StartsLoneCopy(int group1_count, int start0, int alive0, int state0, bool own_model) {
  return own_model && group1_count == 1 && start0 == 0 && alive0 == 0 && state0 == 0;
}

bool AreaCopiesLaidOut(int total, int group1_count) {
  return total >= 1 && total <= kMaxAreaCopies && group1_count >= 0 && group1_count <= total;
}

}  // namespace data
