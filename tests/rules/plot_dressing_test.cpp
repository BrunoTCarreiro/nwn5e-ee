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
  p.scripts_besides_trap = 1;  // a lamp's nw_02_onoff, a quest object's OnDeath
  CHECK_EQ(PlotDressingBreaks(true, true, p), false);
}

// 0.99.93 (owner, 2026-11-19: "1B"): a Plot lamp breaks like other dressing when its only script is the stock on/off
// switch; one running a module's own script stays Plot.
TEST(PlotDressingLampsBreakWithOnlyTheStockSwitch) {
  PlaceableFacts lamp;  // m0q1d's candelabra: Plot, not clickable in the module, a LightColor, OnUsed nw_02_onoff
  lamp.plot = true;
  lamp.is_light = true;
  lamp.scripts_besides_trap = 0;  // the stock switch isn't counted on a light (DressingScriptCounts)
  CHECK_EQ(PlotDressingBreaks(true, true, lamp), true);
  lamp.scripts_besides_trap = 1;  // a module's own lamp script
  CHECK_EQ(PlotDressingBreaks(true, true, lamp), false);

  CHECK_EQ(DressingScriptCounts("OnUsed", "nw_02_onoff", true), false);
  CHECK_EQ(DressingScriptCounts("OnUsed", "NW_02_ONOFF", true), false);
  CHECK_EQ(DressingScriptCounts("OnUsed", "x2_am_lampuse", true), false);
  CHECK_EQ(DressingScriptCounts("OnUsed", "nw_02_onoff", false), true);    // not a light: a script like any other
  CHECK_EQ(DressingScriptCounts("OnDeath", "nw_02_onoff", true), true);    // the switch belongs in OnUsed only
  CHECK_EQ(DressingScriptCounts("OnUsed", "q_lamp_quest", true), true);    // a module's own
  CHECK_EQ(DressingScriptCounts("OnUsed", "nw_02_onoff2", true), true);    // a module's, named after the stock one
  CHECK_EQ(DressingScriptCounts("OnUsed", "", true), false);               // none set
  CHECK_EQ(DressingScriptCounts("OnDeath", "", false), false);
  CHECK_EQ(DressingScriptCounts("OnDeath", "nw_o2_generalhig", false), true);
}

// 0.99.89: the engine's light flag starts at 1 on every placeable; only an appearance with a LightColor is lit by it.
TEST(PlaceableLitOnlyWithALightColour) {
  CHECK_EQ(PlaceableLit(false, 1), false);  // a bench or a flag: the flag's starting 1 means nothing
  CHECK_EQ(PlaceableLit(false, 0), false);
  CHECK_EQ(PlaceableLit(true, 1), true);    // a lit brazier
  CHECK_EQ(PlaceableLit(true, 0), false);   // a brazier put out
}
