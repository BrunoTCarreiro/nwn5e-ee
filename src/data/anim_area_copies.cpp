#include "data/anim_area_copies.h"

#include <stdlib.h>

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

std::vector<int> BytePattern(const char* text) {
  std::vector<int> pattern;
  for (const char* p = text; p && *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (*p == '?') {
      pattern.push_back(-1);
      p += p[1] == '?' ? 2 : 1;
      continue;
    }
    const char digits[3] = {p[0], p[1] ? p[1] : '\0', '\0'};
    pattern.push_back(static_cast<int>(strtoul(digits, nullptr, 16)));
    p += p[1] ? 2 : 1;
  }
  return pattern;
}

bool BytesMatch(const uint8_t* code, const std::vector<int>& pattern) {
  if (!code) return false;
  for (size_t i = 0; i < pattern.size(); ++i) {
    if (pattern[i] >= 0 && code[i] != pattern[i]) return false;
  }
  return true;
}

bool OffsetAnchored(const area_engine::OffsetAnchor& anchor) {
  if (!anchor.pattern || anchor.size < 1 || anchor.size > 4 || anchor.at < 0) return false;
  if (anchor.size < 4 && anchor.value >> (8 * anchor.size) != 0) return false;  // doesn't fit its bytes
  const std::vector<int> pattern = BytePattern(anchor.pattern);
  if (static_cast<size_t>(anchor.at) + static_cast<size_t>(anchor.size) > pattern.size()) return false;
  for (int i = 0; i < anchor.size; ++i) {
    if (pattern[anchor.at + i] != static_cast<int>((anchor.value >> (8 * i)) & 0xff)) return false;  // ?? is -1
  }
  return true;
}

bool AreaHiliteCallsAsLoaded(const std::vector<const uint8_t*>& calls) {
  if (calls.size() != static_cast<size_t>(area_engine::kHiliteSlot3Calls)) return false;
  const std::vector<int> prefix = BytePattern(area_engine::kHiliteSlot3CallPattern);
  for (const uint8_t* call : calls) {
    if (!call || !BytesMatch(call - area_engine::kHiliteSlot3CallAt, prefix)) return false;
  }
  return true;
}

}  // namespace data
