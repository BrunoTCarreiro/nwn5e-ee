#include "rules/spell_time.h"

namespace rules {

int SpellSeconds(int minutes) { return minutes > 0 ? minutes * 60 : 0; }

bool RestEnds(int rest_seconds, int remaining_seconds) { return remaining_seconds <= rest_seconds; }

bool RestEndsSpellArea(const std::string& tag, bool long_rest) {
  if (tag == "NWN5E_ENTANGLE" || tag == "NWN5E_GREASE") return true;
  if (tag.rfind("NWN5E_FOG_CLOUD_", 0) == 0) return true;
  if (tag == "NWN5E_ALARM") return long_rest;
  return false;
}

bool RestClearsSpellsAtStart(bool faithful) { return !faithful; }

}  // namespace rules
