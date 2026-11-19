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

// 0.99.94 (owner, 2026-11-19: "can't break candelholders, benches or vases in this tiny room I'm in"): a save made
// with nwn5e running holds its light rules' own lamp scripts, each put where the stock switch or nothing was, and the
// lamp clickable for its Snuff / Light - nwn5e's, not a module's purpose.
TEST(PlotDressingLampsSavedWithNwn5esScripts) {
  CHECK_EQ(DressingScriptCounts("OnUsed", "n5_lamp_use", true), false);
  CHECK_EQ(DressingScriptCounts("OnDamaged", "n5_lamp_hit", true), false);
  CHECK_EQ(DressingScriptCounts("OnMeleeAttacked", "n5_lamp_atk", true), false);
  CHECK_EQ(DressingScriptCounts("OnSpellCastAt", "n5_lamp_spl", true), false);
  CHECK_EQ(DressingScriptCounts("OnDeath", "n5_lamp_hit", true), true);      // not where nwn5e puts it
  CHECK_EQ(DressingScriptCounts("OnDamaged", "n5_lamp_hit", false), true);   // not a light
  CHECK_EQ(DressingScriptCounts("OnUsed", "n5_flame_use", true), true);      // a tile flame's stand-in's: kept

  CHECK_EQ(ClickableByNwn5e(true, "n5_lamp_use"), true);   // m0q1c's candelabras in the owner's save
  CHECK_EQ(ClickableByNwn5e(true, "N5_LAMP_USE"), true);
  CHECK_EQ(ClickableByNwn5e(true, "nw_02_onoff"), false);  // clickable in the module itself
  CHECK_EQ(ClickableByNwn5e(false, "n5_lamp_use"), false);
  CHECK_EQ(ClickableByNwn5e(true, ""), false);

  PlaceableFacts lamp;  // as saved: Plot, clickable (nwn5e's), its scripts nwn5e's
  lamp.plot = true;
  lamp.is_light = true;
  lamp.useable = !ClickableByNwn5e(true, "n5_lamp_use");
  CHECK_EQ(PlotDressingBreaks(true, true, lamp), true);
}

// 0.99.89: the engine's light flag starts at 1 on every placeable; only an appearance with a LightColor is lit by it.
TEST(PlaceableLitOnlyWithALightColour) {
  CHECK_EQ(PlaceableLit(false, 1), false);  // a bench or a flag: the flag's starting 1 means nothing
  CHECK_EQ(PlaceableLit(false, 0), false);
  CHECK_EQ(PlaceableLit(true, 1), true);    // a lit brazier
  CHECK_EQ(PlaceableLit(true, 0), false);   // a brazier put out
}
