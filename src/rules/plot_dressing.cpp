#include "rules/plot_dressing.h"

namespace rules {

bool PlotDressingBreaks(bool house_rule, bool spellcasting_faithful, const PlaceableFacts& p) {
  if (!house_rule || !spellcasting_faithful || !p.plot) return false;
  if (p.is_static || p.useable || p.has_inventory || p.has_conversation || p.is_light) return false;
  return p.scripts_besides_trap == 0;
}

bool PlaceableLit(bool appearance_has_light_colour, int light_flag) {
  return appearance_has_light_colour && light_flag != 0;
}

}  // namespace rules
