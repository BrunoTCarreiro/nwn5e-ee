#include "rules/object_attack.h"

#include <ctype.h>

namespace rules {

namespace {

bool Is(const char* text, const char* word) {
  if (!text) return false;
  for (; *text && *word; ++text, ++word) {
    if (tolower(static_cast<unsigned char>(*text)) != *word) return false;
  }
  return *text == '\0' && *word == '\0';
}

}  // namespace

int ObjectArmorClass(const char* armor_type) {
  if (Is(armor_type, "leather")) return 11;
  if (Is(armor_type, "stone")) return 17;
  if (Is(armor_type, "plate")) return 19;
  return 15;
}

int AttackAgainstObject(int d20, int bonus, int armor_class) {
  if (d20 >= 20) return 2;
  if (d20 <= 1) return 0;
  return d20 + bonus >= armor_class ? 1 : 0;
}

}  // namespace rules
