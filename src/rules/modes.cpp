#include "rules/modes.h"

#include <ctype.h>

#include <string>

namespace rules {

RuleVariant ParseRuleVariant(const char* text, RuleVariant fallback) {
  if (!text) return fallback;
  std::string value;
  for (const char* c = text; *c; ++c) {
    if (!isspace(static_cast<unsigned char>(*c))) value += static_cast<char>(tolower(static_cast<unsigned char>(*c)));
  }
  if (value == "faithful" || value == "1") return RuleVariant::kFaithful;
  if (value == "nwn") return RuleVariant::kNwn;
  if (value == "off" || value == "0") return RuleVariant::kOff;
  return fallback;
}

}  // namespace rules
