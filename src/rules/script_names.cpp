#include "rules/script_names.h"

#include <ctype.h>

namespace rules {

int FindScriptName(const std::vector<std::string>& names, const char* chars, size_t length) {
  if (!chars || length == 0) return -1;
  for (size_t i = 0; i < names.size(); ++i) {
    const std::string& name = names[i];
    if (name.size() != length) continue;
    size_t k = 0;
    while (k < length && tolower(static_cast<unsigned char>(name[k])) == tolower(static_cast<unsigned char>(chars[k]))) {
      ++k;
    }
    if (k == length) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace rules
