#include "data/anim_look_armed.h"

#include <cstdio>

namespace data {

ArmedLook ArmedLookOf(int spell_row) {
  ArmedLook look;
  switch (spell_row) {
    case 1183: look.flash = 71; look.glow = 5; break;  // Divine Smite: holy
    case 1184: look.flash = 61; look.glow = 3; break;  // Searing Smite: fire
    case 1185: look.flash = 44; look.glow = 0; break;  // Ensnaring Strike: acid (green)
    default: break;
  }
  return look;
}

namespace {

// The tagged glow off the weapon kept on the creature (none kept: nothing).
std::string GlowOff() {
  return std::string("object kept = GetLocalObject(OBJECT_SELF, \"") + kArmedGlowTag + "\");\n"
         "if (GetIsObjectValid(kept)) {\n"
         "  itemproperty ip = GetFirstItemProperty(kept);\n"
         "  while (GetIsItemPropertyValid(ip)) {\n"
         "    if (GetItemPropertyTag(ip) == \"" + kArmedGlowTag + "\") RemoveItemProperty(kept, ip);\n"
         "    ip = GetNextItemProperty(kept);\n"
         "  }\n"
         "}\n";
}

}  // namespace

std::string ArmedRefreshScript(const ArmedLook& look) {
  if (look.glow < 0) return {};
  char seconds[16];
  std::snprintf(seconds, sizeof(seconds), "%.1f", kArmedGlowSeconds);
  return GlowOff() +
         "object weapon = GetItemInSlot(INVENTORY_SLOT_RIGHTHAND, OBJECT_SELF);\n"
         "if (GetIsObjectValid(weapon)) {\n"
         "  AddItemProperty(DURATION_TYPE_TEMPORARY, TagItemProperty(ItemPropertyVisualEffect(" + std::to_string(look.glow) + "), \"" +
         kArmedGlowTag + "\"), weapon, " + seconds + ");\n"
         "  SetLocalObject(OBJECT_SELF, \"" + kArmedGlowTag + "\", weapon);\n"
         "} else {\n"
         "  DeleteLocalObject(OBJECT_SELF, \"" + kArmedGlowTag + "\");\n"
         "}\n";
}

std::string ArmedOnScript(const ArmedLook& look) {
  std::string out;
  if (look.flash >= 0) {
    out += "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(" + std::to_string(look.flash) + "), OBJECT_SELF);\n";
  }
  return out + ArmedRefreshScript(look);
}

std::string ArmedOffScript() { return GlowOff() + "DeleteLocalObject(OBJECT_SELF, \"" + kArmedGlowTag + "\");\n"; }

ArmedPlay ArmedPlayOf(bool has_look, bool cast_begun, bool still) {
  ArmedPlay play;
  if (!has_look) {
    play.flash = true;
    return play;
  }
  play.cast = !cast_begun;
  play.gesture = play.cast && !still;
  play.hit = true;
  return play;
}

std::string ArmedArmScript(const ArmedLook& look, const ArmedPlay& play, int spell_row) {
  std::string out = play.flash ? ArmedOnScript(look) : ArmedRefreshScript(look);
  if (play.gesture) out += ArmedGestureScript(spell_row);
  return out;
}

std::string ArmedGestureScript(int spell_row) {
  return "if (GetCurrentAction(OBJECT_SELF) == ACTION_INVALID) ActionCastFakeSpellAtObject(" + std::to_string(spell_row) +
         ", OBJECT_SELF);\n";
}

}  // namespace data
