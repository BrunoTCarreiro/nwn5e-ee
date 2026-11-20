#include "rules/breaking.h"

#include <ctype.h>
#include <string.h>

#include <string>

namespace rules {

namespace {

std::string Lower(const char* text) {
  std::string out;
  for (const char* p = text; p && *p; ++p) out += static_cast<char>(tolower(static_cast<unsigned char>(*p)));
  return out;
}

}  // namespace

int DebrisMaterialFor(const char* armor_type, bool glass) {
  const std::string armor = Lower(armor_type);
  if (armor == "ethereal") return -1;
  if (glass) return 4;
  if (armor == "wood") return 0;
  if (armor == "stone") return 1;
  if (armor == "plate") return 2;
  if (armor == "leather") return 3;
  return -1;
}

bool IsGlassLabel(const char* label) {
  const std::string text = Lower(label);
  for (const char* word : {"glass", "window", "mirror", "crystal", "bottle", "jar"}) {
    if (text.find(word) != std::string::npos) return true;
  }
  return false;
}

int DebrisSizeFor(const char* destroyed_sound) {
  const std::string sound = Lower(destroyed_sound);
  return sound.size() >= 3 && sound.compare(sound.size() - 3, 3, "lrg") == 0 ? 1 : 0;
}

}  // namespace rules
