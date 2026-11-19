#pragma once

namespace rules {

// #172 (owner, 2026-11-19, after S2-16: "decorative flags, plant pots, benches, etc... placeables that are not marked
// "plot" should be breakable", then on plot items with no scripts: "D′, with the switch on by default"; and "items
// with traps, like chests, are usually meant to be breakable as that's a viable way to overcome the traps"). A module
// often marks its set dressing Plot - in the Prelude every non-clickable piece is, with no script but the toolset's
// default trap one. With the house rule (plot_dressing_breakable, on by default) and spellcasting faithful, such a
// piece loads breakable: a Plot placeable that isn't static scenery, isn't clickable, has no inventory, no
// conversation, isn't a light (the light rules have their own), and has no event script besides the trap-triggered
// one (trapped or not doesn't matter). What has a purpose stays as the module made it.
struct PlaceableFacts {
  bool plot = false;
  bool is_static = false;      // scenery: not an object the client updates
  bool useable = false;        // clickable
  bool has_inventory = false;  // a container: never Plot dressing (non-Plot chests break as they are)
  bool has_conversation = false;
  bool is_light = false;
  int scripts_besides_trap = 0;  // event scripts set, OnTrapTriggered not counted
};

bool PlotDressingBreaks(bool house_rule, bool spellcasting_faithful, const PlaceableFacts& placeable);

}  // namespace rules
