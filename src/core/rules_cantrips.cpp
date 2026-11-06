// #39 Part 1, build 1: SRD 5.2's damage cantrips (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "go for 39 focusing
// on the cantrips", then "1. A, but do not reuse icons yet ... 2. A, but poison and acid are different things and
// force and psychic aren't magical ... 4. A ... 6. ok"), `spellcasting` faithful (#36's key).
// - Served rows (spells.if_cantrips2024.txt): Fire Bolt 1050, Chill Touch 1051, Shocking Grasp 1052, Poison Spray
//   1053, Sacred Flame 1054, Starry Wisp 1055, Vicious Mockery 1056 - new, with nwn5e's placeholder icon (no game
//   icon reused yet); Light 100, Ray of Frost 144 and Acid Splash 424 run nwn5e's scripts; NWN's own cantrips (Daze,
//   Flare, Electric Jolt, Virtue, Cure / Inflict Minor Wounds) off the class lists (4A). Poison, Force and Psychic:
//   damagetypes.2da rows 13-15 (NWN:EE's custom types 1-3) with their groups (names, the combat log's colours).
// - Each cantrip's script runs in C++ (script_hooks.h), as the caster: a first script chunk makes the roll - the
//   engine's touch attack (SK10 makes it a 2024 spell attack: PB + the casting ability) or the target's save against
//   GetSpellSaveDC() (C5: 2024's DC) - and names who is hit (a 2 for a Critical Hit); the damage is rolled here
//   (rules::CantripDiceRolled: Cantrip Upgrade by character level, doubled on a Critical Hit) and a second chunk
//   applies it with the spell's visual and its rider:
//   Ray of Frost -10 ft Speed until your next turn (a third of a 30-ft Speed, a round); Chill Touch no Hit Points
//   regained until the end of your next turn (CNWSEffectListHandler::OnApplyHeal: a heal on it heals 0); Shocking
//   Grasp no Opportunity Attacks until its next turn (rules_opportunity.cpp asks); Starry Wisp Dim Light and no
//   Invisible until the end of your next turn (its invisibility effects removed); Vicious Mockery Disadvantage on
//   its next attack roll before the end of its next turn (rules_advantage.cpp asks); Poison Spray nothing to a
//   creature immune to poison. A turn here is a round (6 s): "your next turn" ends within two.
// - Acid Splash: a 5-foot (1.5 m) sphere at the point (or the creature) - each creature in it a Dexterity save, one
//   damage roll. Light: the object or creature touched lights 1 hour (Bright 20 ft and Dim 20 ft: NWN's 15 m light);
//   cast again, the earlier one ends.
// NWN mode: NWN's cantrips.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "script_hooks.h"
#include "rules/cantrips.h"

namespace {

constexpr uint64_t kRoundMs = 6000;
constexpr uint64_t kUntilYourNextTurnMs = kRoundMs;           // "until the start of your next turn"
constexpr uint64_t kUntilEndOfNextTurnMs = 2 * kRoundMs;      // "until the end of your next turn"
constexpr const char* kVarLevel = "n5c_level";
constexpr const char* kVarHits = "n5c_hits";

using OnApplyHealFn = int (*)(void* handler, void* object, void* effect, int load);
OnApplyHealFn g_on_apply_heal = nullptr;

std::unordered_map<uint32_t, uint64_t> g_no_healing;     // Chill Touch: until (game ms)
std::unordered_map<uint32_t, uint64_t> g_no_reactions;   // Shocking Grasp: no Opportunity Attacks until
std::unordered_map<uint32_t, uint64_t> g_mocked;         // Vicious Mockery: Disadvantage on the next attack until
int g_logged = 0;

bool Faithful() { return g_config.spellcasting == rules::RuleVariant::kFaithful; }

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

bool Live(std::unordered_map<uint32_t, uint64_t>* marks, uint32_t id) {
  if (marks->empty()) return false;
  const auto at = marks->find(id);
  if (at == marks->end()) return false;
  if (GameMs() < at->second) return true;
  marks->erase(at);
  return false;
}

// The roll - an attack's or each target's save - and who it hits ("id:2;" for a Critical Hit).
const char* const kAttack = R"nss(
object t = GetSpellTargetObject(); string r = "";
if (GetIsObjectValid(t)) {
  SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  $BEAM
  int hit = $TOUCH(t);
  if (hit > 0) r = ObjectToString(t) + ":" + IntToString(hit) + ";";
}
SetLocalInt(OBJECT_SELF, "n5c_level", GetHitDice(OBJECT_SELF)); SetLocalString(OBJECT_SELF, "n5c_hits", r);
)nss";

const char* const kSave = R"nss(
object t = GetSpellTargetObject(); string r = "";
if (GetIsObjectValid(t)) {
  SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  if ($SAVE(t, GetSpellSaveDC(), SAVING_THROW_TYPE_NONE, OBJECT_SELF) == 0) r = ObjectToString(t) + ":1;";
}
SetLocalInt(OBJECT_SELF, "n5c_level", GetHitDice(OBJECT_SELF)); SetLocalString(OBJECT_SELF, "n5c_hits", r);
)nss";

// Acid Splash: every creature in a 5-foot sphere at the point.
const char* const kSplash = R"nss(
location l = GetSpellTargetLocation(); string r = "";
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_ACID_L), l);
object t = GetFirstObjectInShape(SHAPE_SPHERE, 1.52, l, TRUE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  if (ReflexSave(t, GetSpellSaveDC(), SAVING_THROW_TYPE_NONE, OBJECT_SELF) == 0) r += ObjectToString(t) + ":1;";
  t = GetNextObjectInShape(SHAPE_SPHERE, 1.52, l, TRUE, OBJECT_TYPE_CREATURE);
}
SetLocalInt(OBJECT_SELF, "n5c_level", GetHitDice(OBJECT_SELF)); SetLocalString(OBJECT_SELF, "n5c_hits", r);
)nss";

// The damage on one target, its visual and the cantrip's rider.
const char* const kDamage = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
$IMMUNE
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage($AMOUNT, $TYPE), EffectVisualEffect($VIS)), t);
$RIDER
)nss";

struct Look {
  std::string beam;    // before the roll (a beam, a projectile)
  std::string vis;     // on a hit
  std::string rider;   // after the damage
  std::string immune;  // before it
};

Look LookOf(int spell) {
  Look look;
  switch (spell) {
    case 144:  // Ray of Frost
      look.beam = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(VFX_BEAM_COLD, OBJECT_SELF, BODY_NODE_HAND), "
                  "t, 1.7);";
      look.vis = "VFX_IMP_FROST_S";
      look.rider = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, SetEffectSpellId(EffectMovementSpeedDecrease(33), 144), "
                   "t, RoundsToSeconds(1));";
      break;
    case 424:  // Acid Splash
      look.vis = "VFX_IMP_ACID_S";
      break;
    case 1050:  // Fire Bolt
      look.beam = "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV_FLAME), t);";
      look.vis = "VFX_IMP_FLAME_S";
      break;
    case 1051:  // Chill Touch
      look.vis = "VFX_IMP_NEGATIVE_ENERGY";
      break;
    case 1052:  // Shocking Grasp
      look.vis = "VFX_IMP_LIGHTNING_S";
      break;
    case 1053:  // Poison Spray
      look.beam = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(VFX_BEAM_DISINTEGRATE, OBJECT_SELF, "
                  "BODY_NODE_HAND), t, 1.0);";
      look.vis = "VFX_IMP_POISON_S";
      look.immune = "if (GetIsImmune(t, IMMUNITY_TYPE_POISON, OBJECT_SELF)) { ApplyEffectToObject(DURATION_TYPE_INSTANT, "
                    "EffectVisualEffect(VFX_IMP_MAGIC_PROTECTION), t); SendMessageToPC(OBJECT_SELF, GetName(t) + "
                    "\" is immune to Poison.\"); return; }";
      break;
    case 1054:  // Sacred Flame
      look.vis = "VFX_IMP_DIVINE_STRIKE_FIRE";
      break;
    case 1055:  // Starry Wisp
      look.beam = "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV), t);";
      look.vis = "VFX_IMP_MAGBLUE";
      look.rider =
          "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, SetEffectSpellId(EffectVisualEffect(VFX_DUR_LIGHT_WHITE_5), "
          "1055), t, RoundsToSeconds(2)); effect e = GetFirstEffect(t); while (GetIsEffectValid(e)) { int k = "
          "GetEffectType(e); if (k == EFFECT_TYPE_INVISIBILITY || k == EFFECT_TYPE_IMPROVEDINVISIBILITY) "
          "RemoveEffect(t, e); e = GetNextEffect(t); }";
      break;
    case 1056:  // Vicious Mockery
      look.vis = "VFX_IMP_HEAD_MIND";
      break;
    default:
      look.vis = "VFX_IMP_MAGBLUE";
  }
  return look;
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0 && g_logged < 40) {
    ++g_logged;
    Log("cantrips: 0x%08X %s - its script returned %d", self, what, result);
  }
}

// A cantrip's script, run for `self` (rules::Cantrips()).
void Cast(uint32_t self, int spell) {
  const rules::Cantrip* cantrip = rules::CantripOf(spell);
  if (!cantrip || cantrip->die <= 0) return;
  std::string roll;
  if (spell == 424) {
    roll = kSplash;
  } else if (cantrip->roll == rules::CantripRoll::kRangedSpellAttack ||
             cantrip->roll == rules::CantripRoll::kMeleeSpellAttack) {
    roll = kAttack;
    Replace(&roll, "$TOUCH",
            cantrip->roll == rules::CantripRoll::kMeleeSpellAttack ? "TouchAttackMelee" : "TouchAttackRanged");
  } else {
    roll = kSave;
    Replace(&roll, "$SAVE", cantrip->roll == rules::CantripRoll::kWisdomSave ? "WillSave" : "ReflexSave");
  }
  const Look look = LookOf(spell);
  Replace(&roll, "$BEAM", look.beam);
  Run(roll, self, cantrip->script);
  void* object = CreatureById(self);
  const int level = object ? engine::GetLocalInt(object, kVarLevel) : 1;
  const std::string hits = object ? engine::GetLocalString(object, kVarHits) : "";
  int splash = -1;  // Acid Splash: one damage roll for every creature it hits
  size_t at = 0;
  while (at < hits.size()) {
    const size_t colon = hits.find(':', at);
    const size_t end = hits.find(';', at);
    if (colon == std::string::npos || end == std::string::npos || colon > end) break;
    const std::string target = hits.substr(at, colon - at);
    const bool critical = atoi(hits.substr(colon + 1, end - colon - 1).c_str()) == 2;
    at = end + 1;
    int amount = 0;
    if (spell == 424) {
      if (splash < 0) splash = RollEngineDice(rules::CantripDiceRolled(level, false), cantrip->die);
      amount = splash;
    } else {
      amount = RollEngineDice(rules::CantripDiceRolled(level, critical), cantrip->die);
    }
    std::string apply = kDamage;
    Replace(&apply, "$TARGET", target);
    Replace(&apply, "$AMOUNT", std::to_string(amount));
    Replace(&apply, "$TYPE", std::to_string(cantrip->damage_type));
    Replace(&apply, "$VIS", look.vis);
    Replace(&apply, "$IMMUNE", look.immune);
    Replace(&apply, "$RIDER", look.rider);
    Run(apply, self, cantrip->script);
    const uint32_t id = static_cast<uint32_t>(strtoul(target.c_str(), nullptr, 16));
    const uint64_t now = GameMs();
    if (spell == 1051) g_no_healing[id] = now + kUntilEndOfNextTurnMs;
    if (spell == 1052) g_no_reactions[id] = now + kUntilYourNextTurnMs;
    if (spell == 1056) g_mocked[id] = now + kUntilEndOfNextTurnMs;
    if (g_logged < 60) {
      ++g_logged;
      Log("cantrips: 0x%08X %s hits 0x%08X - %d damage (%d dice of d%d at level %d%s)", self, cantrip->script, id, amount,
          spell == 424 ? rules::CantripDice(level) : rules::CantripDiceRolled(level, critical), cantrip->die, level,
          critical ? ", a Critical Hit" : "");
    }
  }
}

// Light (100): the object or creature touched lights 1 hour; cast again, the earlier one ends.
const char* const kLight = R"nss(
object old = StringToObject(GetLocalString(OBJECT_SELF, "n5c_light"));
if (GetIsObjectValid(old)) {
  effect e = GetFirstEffect(old);
  while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "n5c_light") RemoveEffect(old, e); e = GetNextEffect(old); }
  itemproperty ip = GetFirstItemProperty(old);
  while (GetIsItemPropertyValid(ip)) { if (GetItemPropertyTag(ip) == "n5c_light") RemoveItemProperty(old, ip); ip = GetNextItemProperty(old); }
}
object t = GetSpellTargetObject();
if (!GetIsObjectValid(t)) return;
if (GetObjectType(t) == OBJECT_TYPE_ITEM) {
  AddItemProperty(DURATION_TYPE_TEMPORARY, TagItemProperty(ItemPropertyLight(IP_CONST_LIGHTBRIGHTNESS_NORMAL,
                  IP_CONST_LIGHTCOLOR_WHITE), "n5c_light"), t, HoursToSeconds(1));
} else {
  effect l = EffectLinkEffects(EffectVisualEffect(VFX_DUR_LIGHT_WHITE_15), EffectVisualEffect(VFX_DUR_CESSATE_POSITIVE));
  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(l, 100), "n5c_light"), t, HoursToSeconds(1));
}
SetLocalString(OBJECT_SELF, "n5c_light", ObjectToString(t));
)nss";

void LightScript(uint32_t self) { Run(kLight, self, "n5c_light"); }

template <int Spell>
void CantripScript(uint32_t self) {
  Cast(self, Spell);
}

// Chill Touch: a heal on a creature that can't regain Hit Points heals 0.
int OnApplyHeal(void* handler, void* object, void* effect, int load) {
  if (object && effect && !g_no_healing.empty()) {
    const uint32_t id = Field<uint32_t>(object, ee::kObjectId);
    if (Live(&g_no_healing, id) && Field<int32_t>(effect, ee::kEffectIntegerCount) > 0) {
      int32_t* integers = Field<int32_t*>(effect, ee::kEffectIntegers);
      if (integers && integers[0] > 0) {
        Log("cantrips: 0x%08X regains no Hit Points (Chill Touch) - %d not healed", id, integers[0]);
        integers[0] = 0;
      }
    }
  }
  return g_on_apply_heal(handler, object, effect, load);
}

}  // namespace

// rules_opportunity.cpp: Shocking Grasp - no Opportunity Attacks until its next turn.
bool ShockingGraspNoOpportunityAttack(uint32_t id) { return Live(&g_no_reactions, id); }

// rules_advantage.cpp: Vicious Mockery - Disadvantage on the creature's next attack roll (used up by it).
MasteryRollSources MockeryRollSources(void* attacker) {
  MasteryRollSources sources;
  if (!attacker || g_mocked.empty()) return sources;
  const uint32_t id = Field<uint32_t>(attacker, ee::kObjectId);
  if (!Live(&g_mocked, id)) return sources;
  g_mocked.erase(id);
  sources.disadvantage = 1;
  sources.note = "Vicious Mockery";
  return sources;
}

void InstallCantrips() {
  if (!Faithful()) {
    Log("cantrips: NWN's (key spellcasting)");
    return;
  }
  ServeConditionalRows("cantrips2024");
  bool scripts = AddScriptHandler("n5_c_light", &LightScript) &&
                 AddScriptHandler("n5_c_rayfrost", &CantripScript<144>) &&
                 AddScriptHandler("n5_c_acidsplash", &CantripScript<424>) &&
                 AddScriptHandler("n5_c_firebolt", &CantripScript<1050>) &&
                 AddScriptHandler("n5_c_chilltouch", &CantripScript<1051>) &&
                 AddScriptHandler("n5_c_shockgrasp", &CantripScript<1052>) &&
                 AddScriptHandler("n5_c_poisonspray", &CantripScript<1053>) &&
                 AddScriptHandler("n5_c_sacredflame", &CantripScript<1054>) &&
                 AddScriptHandler("n5_c_starrywisp", &CantripScript<1055>) &&
                 AddScriptHandler("n5_c_mockery", &CantripScript<1056>);
  const bool heal = HookExport("CNWSEffectListHandler::OnApplyHeal (Chill Touch: no Hit Points regained, #39)",
                               ee::kOnApplyHeal, reinterpret_cast<void*>(&OnApplyHeal),
                               reinterpret_cast<void**>(&g_on_apply_heal));
  Log("cantrips: SRD 5.2's damage cantrips (%zu; Cantrip Upgrade at 5, 11, 17), Poison / Force / Psychic damage of "
      "their own, NWN's own cantrips off the class lists (#39 Part 1)%s",
      rules::Cantrips().size(), scripts && heal ? "" : " - SOME hooks missing");
}
