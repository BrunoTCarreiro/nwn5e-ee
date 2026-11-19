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
  if (!is_light) return true;
  // A light's: the stock switch, or nwn5e's own (light_sources.cpp's kUseScript, kHitScript, kAttackScript,
  // kSpellScript), each in its own slot.
  static const char* const kLampScripts[][2] = {
      {"OnUsed", "nw_02_onoff"},    {"OnUsed", "x2_am_lampuse"},          {"OnUsed", "n5_lamp_use"},
      {"OnDamaged", "n5_lamp_hit"}, {"OnMeleeAttacked", "n5_lamp_atk"}, {"OnSpellCastAt", "n5_lamp_spl"},
  };
  for (const auto& one : kLampScripts) {
    if (SameName(field, one[0]) && SameName(script, one[1])) return false;
  }
  return true;
}

bool ClickableByNwn5e(bool is_light, const char* on_used) {
  return is_light && on_used && SameName(on_used, "n5_lamp_use");
}

bool PlaceableLit(bool appearance_has_light_colour, int light_flag) {
  return appearance_has_light_colour && light_flag != 0;
}

}  // namespace rules
