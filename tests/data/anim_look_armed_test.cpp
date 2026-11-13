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

// #162 1A (owner, 2026-11-13: "1. A"): a smite whose row has a look made in the Spell & Attack Bench plays that look as
// it arms - its cast (the clip swap, the caster's parts, the gesture) and its hit on the caster - instead of the fixed
// flash; only its hit when that look's cast already began for the caster (a cast that ran: the cast listener started it,
// so it never plays twice). No look: the fixed flash, as before.
TEST(ArmedLookPlays) {
  const ArmedPlay none = ArmedPlayOf(false, false, false);
  CHECK_EQ(none.flash && !none.cast && !none.gesture && !none.hit, true);
  const ArmedPlay fresh = ArmedPlayOf(true, false, false);
  CHECK_EQ(!fresh.flash && fresh.cast && fresh.gesture && fresh.hit, true);
  const ArmedPlay begun = ArmedPlayOf(true, true, false);
  CHECK_EQ(!begun.flash && !begun.cast && !begun.gesture && begun.hit, true);
  CHECK_EQ(ArmedPlayOf(false, true, false).flash, true);  // no look: a cast seen changes nothing
  // A still look (the Animation card's None; owner, AS-11: "there should be an option for no animation"): its cast's
  // effects and sounds and its hit, no gesture.
  const ArmedPlay still = ArmedPlayOf(true, false, true);
  CHECK_EQ(!still.flash && still.cast && !still.gesture && still.hit, true);
}

// The arming's script: the fixed flash and the glow, or - with a look - the glow and the gesture (none for a still
// look), no flash.
TEST(ArmedLookArmScript) {
  const std::string flash = ArmedArmScript(ArmedLookOf(1183), ArmedPlayOf(false, false, false), 1183);
  CHECK_EQ(Has(flash, "EffectVisualEffect(71)"), true);
  CHECK_EQ(Has(flash, "ItemPropertyVisualEffect(5)"), true);
  CHECK_EQ(Has(flash, "ActionCastFakeSpellAtObject"), false);
  const std::string look = ArmedArmScript(ArmedLookOf(1183), ArmedPlayOf(true, false, false), 1183);
  CHECK_EQ(Has(look, "EffectVisualEffect(71)"), false);
  CHECK_EQ(Has(look, "ItemPropertyVisualEffect(5)"), true);
  CHECK_EQ(Has(look, "ActionCastFakeSpellAtObject(1183, OBJECT_SELF)"), true);
  const std::string begun = ArmedArmScript(ArmedLookOf(1184), ArmedPlayOf(true, true, false), 1184);
  CHECK_EQ(Has(begun, "ItemPropertyVisualEffect(3)") && !Has(begun, "ActionCastFakeSpellAtObject"), true);
  const std::string still = ArmedArmScript(ArmedLookOf(1183), ArmedPlayOf(true, false, true), 1183);
  CHECK_EQ(Has(still, "ItemPropertyVisualEffect(5)") && !Has(still, "ActionCastFakeSpellAtObject") &&
               !Has(still, "EffectVisualEffect(71)"),
           true);
}

// The gesture: the row's own conjure and cast, faked at the caster (the look's clips swapped in), only when the caster
// isn't doing anything else - never queued behind an attack, never clearing one.
TEST(ArmedLookGesture) {
  const std::string s = ArmedGestureScript(1185);
  CHECK_EQ(Has(s, "GetCurrentAction(OBJECT_SELF) == ACTION_INVALID"), true);
  CHECK_EQ(Has(s, "ActionCastFakeSpellAtObject(1185, OBJECT_SELF)"), true);
  CHECK_EQ(Has(s, "ClearAllActions"), false);
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
