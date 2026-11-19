#include "check.h"
#include "rules/plot_dressing.h"

using namespace rules;

// #172 (owner, 2026-11-19: "D′, with the switch on by default"): Plot dressing with nothing attached breaks.
TEST(PlotDressingBreaksWhenNothingAttached) {
  PlaceableFacts dressing;  // the Prelude's flags, pots and benches: Plot, not clickable, no script but the trap one
  dressing.plot = true;
  CHECK_EQ(PlotDressingBreaks(true, true, dressing), true);
  CHECK_EQ(PlotDressingBreaks(false, true, dressing), false);  // the house rule off
  CHECK_EQ(PlotDressingBreaks(true, false, dressing), false);  // NWN mode keeps NWN's

  PlaceableFacts not_plot = dressing;  // already breakable as it is: nothing to change
  not_plot.plot = false;
  CHECK_EQ(PlotDressingBreaks(true, true, not_plot), false);
}

TEST(PlotDressingKeepsWhatHasAPurpose) {
  PlaceableFacts base;
  base.plot = true;
  PlaceableFacts p = base;
  p.is_static = true;  // scenery: not an object the client updates
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
  p = base;
  p.useable = true;  // a sign, a lever, a Plot chest
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
  p = base;
  p.has_inventory = true;
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
  p = base;
  p.has_conversation = true;
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
  p = base;
  p.is_light = true;  // the light rules put it out instead
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
  p = base;
  p.scripts_besides_trap = 1;  // a lamp's nw_02_onoff, a quest object's OnDeath
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
}
