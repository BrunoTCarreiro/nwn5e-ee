#include "check.h"
#include "data/anim_look_armed.h"

#include <string>

using namespace data;

namespace {
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
}  // namespace

// #162 (owner, 2026-11-13: "go with G, B and D for #162"): an armed smite's look - B, a flash and its sound as it arms
// (the game's impact effect in the smite's colour, with its own sound); D, the game's weapon glow while it's armed.
TEST(ArmedLookRows) {
  CHECK_EQ(ArmedLookOf(1183).flash, 71);  // Divine Smite: VFX_IMP_HOLY_AID ...
  CHECK_EQ(ArmedLookOf(1183).glow, 5);    // ... ITEM_VISUAL_HOLY
  CHECK_EQ(ArmedLookOf(1184).flash, 61);  // Searing Smite: VFX_IMP_FLAME_S ...
  CHECK_EQ(ArmedLookOf(1184).glow, 3);    // ... ITEM_VISUAL_FIRE
  CHECK_EQ(ArmedLookOf(1185).flash, 44);  // Ensnaring Strike: VFX_IMP_ACID_S ...
  CHECK_EQ(ArmedLookOf(1185).glow, 0);    // ... ITEM_VISUAL_ACID
  CHECK_EQ(ArmedLookOf(1182).flash, -1);  // anything else: no look
  CHECK_EQ(ArmedLookOf(1182).glow, -1);
}

// Arming: the flash on the creature, the glow (tagged, temporary) on the weapon in its right hand, that weapon kept on the
// creature - the game may unequip it before the smite ends (the game session's note on #162).
TEST(ArmedLookOnScript) {
  const std::string s = ArmedOnScript(ArmedLookOf(1183));
  CHECK_EQ(Has(s, "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(71), OBJECT_SELF)"), true);
  CHECK_EQ(Has(s, "GetItemInSlot(INVENTORY_SLOT_RIGHTHAND, OBJECT_SELF)"), true);
  CHECK_EQ(Has(s, "TagItemProperty(ItemPropertyVisualEffect(5), \"nwn5e_armed_glow\")"), true);
  CHECK_EQ(Has(s, "DURATION_TYPE_TEMPORARY"), true);
  CHECK_EQ(Has(s, "SetLocalObject(OBJECT_SELF, \"nwn5e_armed_glow\""), true);
  // No glow for a look without one; no flash on a refresh.
  ArmedLook flash_only;
  flash_only.flash = 61;
  CHECK_EQ(Has(ArmedOnScript(flash_only), "ItemPropertyVisualEffect"), false);
  CHECK_EQ(Has(ArmedRefreshScript(ArmedLookOf(1184)), "EffectVisualEffect"), false);
  CHECK_EQ(Has(ArmedRefreshScript(ArmedLookOf(1184)), "ItemPropertyVisualEffect(3)"), true);
}

// The glow comes off the weapon kept - its tagged properties only, never the weapon's own - and the record goes.
TEST(ArmedLookOffScript) {
  const std::string s = ArmedOffScript();
  CHECK_EQ(Has(s, "GetLocalObject(OBJECT_SELF, \"nwn5e_armed_glow\")"), true);
  CHECK_EQ(Has(s, "GetItemPropertyTag(ip) == \"nwn5e_armed_glow\""), true);
  CHECK_EQ(Has(s, "RemoveItemProperty("), true);
  CHECK_EQ(Has(s, "DeleteLocalObject(OBJECT_SELF, \"nwn5e_armed_glow\")"), true);
  // A refresh takes it off the weapon kept first (a weapon swapped in the meantime: the glow moves to the new one).
  CHECK_EQ(ArmedRefreshScript(ArmedLookOf(1183)).find("RemoveItemProperty(") < ArmedRefreshScript(ArmedLookOf(1183)).find("AddItemProperty("), true);
}
