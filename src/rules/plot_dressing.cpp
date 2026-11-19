#include "rules/plot_dressing.h"

#include <ctype.h>
#include <string.h>

namespace rules {

namespace {

bool SameName(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) return false;
  }
  return *a == *b;
}

}  // namespace

bool PlotDressingBreaks(bool house_rule, bool spellcasting_faithful, const PlaceableFacts& p) {
  if (!house_rule || !spellcasting_faithful || !p.plot) return false;
  if (p.is_static || p.useable || p.has_inventory || p.has_conversation) return false;
  return p.scripts_besides_trap == 0;
}

bool DressingScriptCounts(const char* field, const char* script, bool is_light) {
  if (!script || !*script) return false;
  if (is_light && SameName(field, "OnUsed") &&
      (SameName(script, "nw_02_onoff") || SameName(script, "x2_am_lampuse"))) {
    return false;
  }
  return true;
}

bool PlaceableLit(bool appearance_has_light_colour, int light_flag) {
  return appearance_has_light_colour && light_flag != 0;
}

}  // namespace rules
