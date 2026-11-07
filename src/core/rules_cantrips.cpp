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
// - Build 2 (owner, 2026-11-07: "go for it"; decision 5 "recommendations": a menu, past its slots a "More"):
//   Resistance (NWN's 151 rewritten), Guidance 1057 and Sorcerous Burst 1059 are cast through a choice - each
//   option a variant row (rules::ChoiceCantrips; the radial's menu: rules_spell_slots.cpp). Resistance and
//   Guidance put a marker on the creature touched (a tagged VFX_DUR_CESSATE_POSITIVE, its spell id the row cast;
//   Concentration, up to 1 minute, ends it): Resistance takes 1d4 off its type's damage once per turn
//   (resistance.h's reducer, before Resistance halves it), Guidance adds 1d4 to checks with its skill
//   (cantrip_effects.h: rules_skill_dcs.cpp's checks, !skill). Spare the Dying 1058 makes a player character at 0
//   Hit Points Stable (rules_dying.cpp; its range by level through CNWSCreature::ComputeSpellRange). Sorcerous
//   Burst: a ranged spell attack, d8s of its type, each 8 another d8 (at most the Charisma modifier more); the
//   Sorcerer's only (spells_page.cpp keeps it off the Wizard, NWN's Wiz_Sorc column being shared).
// - Build 3a (owner, 2026-11-07: "1 A, 2 A, 3 A, 4 A and while Produce Flame is active the character is also a
//   light source, 5 A"): Produce Flame 1096 - its Flame (1097) a light in the hand for 10 minutes, a carried
//   light for the stealth rules; its Hurl (1098) an action while it lasts, a ranged spell attack, d8 Fire. A
//   Bonus Action spell's cast (rules::BonusActionFeatOfSpell) becomes its feat's use - instant, one a round
//   (bonus_actions.cpp), granted at the first cast.
// - Build 3b (owner, 2026-11-07: "build 3b"): Shillelagh 1099 (a Bonus Action, its feats 1549 / 1550) and True
//   Strike (NWN's 415 rewritten: its cast an attack) - rules_weapon_cantrips.cpp.
// - Build 4a: Druidcraft, Elementalism, Prestidigitation, Thaumaturgy and Mage Hand - rules_utility_cantrips.cpp.
// NWN mode: NWN's cantrips.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <optional>
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
#include "action_guard.h"
#include "cantrip_effects.h"
#include "chat.h"
#include "resistance.h"
#include "script_hooks.h"
#include "spell_impact.h"
#include "weapon_cantrips.h"
#include "rules/cantrips.h"
#include "rules/skills.h"

int StabilizeDownPlayer(uint32_t id);        // rules_dying.cpp: 1 made Stable, 2 already, 0 not a down player
bool IsDownHenchmanUnderRules(uint32_t id);  // rules_dying.cpp

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

// --- #39 Part 1, build 2: Resistance, Guidance, Spare the Dying, Sorcerous Burst ---------------------------------

const rules::ChoiceCantrip* g_resistance = nullptr;
const rules::ChoiceCantrip* g_guidance = nullptr;
std::unordered_map<uint32_t, uint64_t> g_resisted;         // Resistance: once per turn - until (game ms)
std::unordered_map<uint32_t, uint32_t> g_resist_locked;    // a master cast's type: the first taken
std::unordered_map<uint32_t, uint32_t> g_guidance_locked;  // a master cast's skill: the first checked
constexpr int kAbilityCharisma = 5;
constexpr float kMetresPerFoot = 0.3048f;

// The row of the effect on `object` that one of `cantrip`'s rows put there (-1: none): its marker, a tagged
// VFX_DUR_CESSATE_POSITIVE carrying the row cast as its spell id. The engine drops it at its end, or Concentration
// (rules_concentration.cpp) at the caster's.
int ActiveRow(const void* object, const rules::ChoiceCantrip* cantrip) {
  if (!object || !cantrip) return -1;
  void* const* list = Field<void* const*>(object, ee::kObjectEffects);
  const int count = Field<int32_t>(object, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return -1;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (!effect) continue;
    const int spell = Field<int32_t>(effect, ee::kEffectSpellId);
    if (spell == cantrip->master || rules::ChoiceCantripOf(spell) == cantrip) return spell;
  }
  return -1;
}

uint32_t TargetIdOf(void* caster) {
  const std::string target = caster ? engine::GetLocalString(caster, "n5c_target") : std::string();
  return target.empty() ? ee::kObjectInvalid : static_cast<uint32_t>(strtoul(target.c_str(), nullptr, 16));
}

std::string DamageName(uint32_t type) {
  switch (type) {
    case 1: return "Bludgeoning";
    case 2: return "Piercing";
    case 4: return "Slashing";
    case rules::kDamageTypeAcid: return "Acid";
    case rules::kDamageTypeCold: return "Cold";
    case rules::kDamageTypeRadiant: return "Radiant";
    case rules::kDamageTypeLightning: return "Lightning";
    case rules::kDamageTypeFire: return "Fire";
    case rules::kDamageTypeNecrotic: return "Necrotic";
    case rules::kDamageTypeThunder: return "Thunder";
    case rules::kDamageTypePoison: return "Poison";
    case rules::kDamageTypePsychic: return "Psychic";
  }
  return "type " + std::to_string(type);
}

// Resistance and Guidance on the creature touched: the marker (Concentration, up to 1 minute; an earlier one of the
// same cantrip on it ends) - its effect icon on the effect bar, named for the option (0.90.1) - and a line to the
// caster.
const char* const kWard = R"nss(
object t = GetSpellTargetObject();
if (!GetIsObjectValid(t)) t = OBJECT_SELF;
SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), FALSE));
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "$TAG") RemoveEffect(t, e); e = GetNextEffect(t); }
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect($VIS), t);
effect m = EffectLinkEffects(EffectIcon($ICON), EffectVisualEffect(VFX_DUR_CESSATE_POSITIVE));
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(m, GetSpellId()), "$TAG"), t,
                    RoundsToSeconds(10));
SetLocalString(OBJECT_SELF, "n5c_target", ObjectToString(t));
string who = GetName(t);
if (t == OBJECT_SELF) who = "yourself";
SendMessageToPC(OBJECT_SELF, "nwn5e: $SAY " + who + ".");
)nss";

void Ward(uint32_t self, const rules::ChoiceCantrip* cantrip, const char* tag, const char* vis) {
  void* caster = CreatureById(self);
  if (!caster || !cantrip) return;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const rules::CantripOption* option = rules::ChoiceOptionOf(row);
  std::string label = cantrip->kind == rules::ChoiceKind::kSkill ? "Guidance" : "Resistance";
  if (option) {
    label += std::string(" (") + option->label + ")";
  } else {
    label += cantrip->kind == rules::ChoiceKind::kSkill ? " (the first skill checked)"
                                                        : " (the first type of damage taken)";
  }
  const int icon = rules::EffectIconOf(row < 0 ? cantrip->master : row);
  std::string code = kWard;
  Replace(&code, "$TAG", tag);
  Replace(&code, "$VIS", vis);
  Replace(&code, "$SAY", label + " on");
  Replace(&code, "$ICON", std::to_string(icon > 0 ? icon : rules::EffectIconOf(cantrip->master)));
  Run(code, self, tag);
  const uint32_t target = TargetIdOf(caster);
  g_resist_locked.erase(target);
  g_guidance_locked.erase(target);
  if (g_logged < 60) {
    ++g_logged;
    Log("cantrips: 0x%08X %s - row %d on 0x%08X, effect icon %d (Concentration, up to 1 minute)", self,
        label.c_str(), row, target, icon);
  }
}

void ResistanceScript(uint32_t self) { Ward(self, g_resistance, "n5c_resist", "VFX_IMP_HEAD_HOLY"); }
void GuidanceScript(uint32_t self) { Ward(self, g_guidance, "n5c_guidance", "VFX_IMP_HEAD_MIND"); }

// Resistance: 1d4 off the chosen type's damage, once per turn (resistance.h: before Resistance halves it).
int ResistanceReducer(void* object, uint32_t flags, int damage, bool simulate) {
  if (simulate || damage <= 0 || !object) return 0;
  const int row = ActiveRow(object, g_resistance);
  if (row < 0) return 0;
  const uint32_t id = Field<uint32_t>(object, ee::kObjectId);
  uint32_t type = 0;
  if (const rules::CantripOption* option = rules::ChoiceOptionOf(row)) {
    type = option->value;
  } else {
    type = g_resist_locked.emplace(id, flags).first->second;  // cast with no choice: the first type taken
  }
  if (!(type & flags) || Live(&g_resisted, id)) return 0;
  g_resisted[id] = GameMs() + kRoundMs;
  const int d4 = RollEngineDice(1, 4);
  const int off = rules::ResistanceReduction(damage, d4);
  if (g_logged < 200) {
    ++g_logged;
    Log("cantrips: 0x%08X Resistance (%s): %d of %d damage stopped (1d4 %d; once this turn)", id,
        DamageName(type).c_str(), off, damage, d4);
  }
  return off;
}

// Spare the Dying: who it was cast at, then the line (and the visual when it worked).
const char* const kSpareTarget = R"nss(
SetLocalString(OBJECT_SELF, "n5c_target", ObjectToString(GetSpellTargetObject()));
)nss";
const char* const kSpareSay = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), FALSE));
if ($WORKED) ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_HEAL), t);
SendMessageToPC(OBJECT_SELF, "nwn5e: Spare the Dying - " + GetName(t) + "$SAY");
)nss";

void SpareTheDyingScript(uint32_t self) {
  Run(kSpareTarget, self, "n5c_sparedying");
  void* caster = CreatureById(self);
  const uint32_t target = TargetIdOf(caster);
  if (target == ee::kObjectInvalid) return;
  const int state = StabilizeDownPlayer(target);
  const char* say = " has Hit Points left, or is dead: nothing to spare.";
  if (state == 1) say = " is Stable: no more death saving throws.";
  if (state == 2) say = " is already Stable.";
  if (state == 0 && IsDownHenchmanUnderRules(target)) {
    say = " lies at 0 Hit Points, in no danger (nwn5e's henchmen make no death saving throws) - heal them to "
          "wake them.";
  }
  std::string code = kSpareSay;
  Replace(&code, "$TARGET", engine::GetLocalString(caster, "n5c_target"));
  Replace(&code, "$WORKED", state == 1 ? "TRUE" : "FALSE");
  Replace(&code, "$SAY", say);
  Run(code, self, "n5c_sparedying");
  const bool henchman = state == 0 && IsDownHenchmanUnderRules(target);
  Log("cantrips: 0x%08X Spare the Dying on 0x%08X - %s", self, target,
      state == 1 ? "Stable" : state == 2 ? "already Stable" : henchman ? "a down henchman, in no danger" : "not dying");
}

// Spare the Dying's range by the caster's character level (15 feet, 30 at 5, 60 at 11, 120 at 17): the engine
// walks the caster into it (CNWSCreature::ComputeSpellRange; the range code by value, freed by the original).
using ComputeSpellRangeFn = float (*)(void* creature, void* range_code, uint32_t spell);
ComputeSpellRangeFn g_compute_spell_range = nullptr;

float ComputeSpellRange(void* creature, void* range_code, uint32_t spell) {
  const float range = g_compute_spell_range(creature, range_code, spell);
  if (const int feet = rules::CantripRangeFeet(static_cast<int>(spell))) {
    return static_cast<float>(feet) * kMetresPerFoot;  // #39 build 4a: the utility cantrips' 30 / 10 feet
  }
  if (spell != static_cast<uint32_t>(rules::kSpareTheDying) || !creature) return range;
  void* stats = StatsOf(creature);
  const int level = stats ? ReadCreatureFacts(stats).level : 1;
  return static_cast<float>(rules::SpareTheDyingRangeFeet(level)) * kMetresPerFoot;
}

// Sorcerous Burst: a ranged spell attack; on a hit d8s of the chosen type, each 8 another d8 (at most the Charisma
// modifier more).
int RollD8(void*) { return RollEngineDice(1, 8); }

Look BurstLookOf(uint32_t type) {
  Look look;
  look.beam = "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV), t);";
  switch (type) {
    case rules::kDamageTypeAcid: look.vis = "VFX_IMP_ACID_S"; break;
    case rules::kDamageTypeCold: look.vis = "VFX_IMP_FROST_S"; break;
    case rules::kDamageTypeLightning: look.vis = "VFX_IMP_LIGHTNING_S"; break;
    case rules::kDamageTypePoison:
      look.vis = "VFX_IMP_POISON_S";
      look.immune = LookOf(1053).immune;  // Poison Spray's: none to a creature immune to poison
      break;
    case rules::kDamageTypePsychic: look.vis = "VFX_IMP_HEAD_MIND"; break;
    case rules::kDamageTypeThunder: look.vis = "VFX_IMP_SONIC"; break;
    default:
      look.beam = "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV_FLAME), t);";
      look.vis = "VFX_IMP_FLAME_S";
  }
  return look;
}

void SorcerousBurstScript(uint32_t self) {
  void* caster = CreatureById(self);
  if (!caster) return;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const rules::CantripOption* option = rules::ChoiceOptionOf(row);
  const uint32_t type = option ? option->value : rules::kDamageTypeFire;  // cast with no choice: Fire
  const Look look = BurstLookOf(type);
  std::string roll = kAttack;
  Replace(&roll, "$TOUCH", "TouchAttackRanged");
  Replace(&roll, "$BEAM", look.beam);
  Run(roll, self, "n5_c_sorcburst");
  const int level = engine::GetLocalInt(caster, kVarLevel);
  const std::string hits = engine::GetLocalString(caster, kVarHits);
  void* stats = StatsOf(caster);
  const int cap = stats ? engine::AbilityModifier(stats, kAbilityCharisma) : 0;
  size_t at = 0;
  while (at < hits.size()) {
    const size_t colon = hits.find(':', at);
    const size_t end = hits.find(';', at);
    if (colon == std::string::npos || end == std::string::npos || colon > end) break;
    const std::string target = hits.substr(at, colon - at);
    const bool critical = atoi(hits.substr(colon + 1, end - colon - 1).c_str()) == 2;
    at = end + 1;
    const int dice = rules::CantripDiceRolled(level, critical);
    int extra = 0;
    const int amount = rules::SorcerousBurstDamage(dice, cap, &RollD8, nullptr, &extra);
    std::string apply = kDamage;
    Replace(&apply, "$TARGET", target);
    Replace(&apply, "$AMOUNT", std::to_string(amount));
    Replace(&apply, "$TYPE", std::to_string(type));
    Replace(&apply, "$VIS", look.vis);
    Replace(&apply, "$IMMUNE", look.immune);
    Replace(&apply, "$RIDER", "");
    Run(apply, self, "n5_c_sorcburst");
    if (g_logged < 60) {
      ++g_logged;
      Log("cantrips: 0x%08X Sorcerous Burst (%s) hits %s - %d damage (%d d8%s, %d more for 8s, at most %d)", self,
          DamageName(type).c_str(), target.c_str(), amount, dice, critical ? ", a Critical Hit" : "", extra, cap);
    }
  }
}

// --- #39 Part 1, build 3a: Produce Flame, and a spell cast as a Bonus Action ------------------------------------

using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          float* location);
UseFeatFn g_use_feat = nullptr;  // CNWSCreature::UseFeat through its hook (bonus_actions.cpp's guard: one a round)
thread_local bool t_bonus_cast = false;

// Whether `object` carries an effect a spell row put there (its spell id).
bool HasEffectOfSpell(const void* object, int spell) {
  if (!object) return false;
  void* const* list = Field<void* const*>(object, ee::kObjectEffects);
  const int count = Field<int32_t>(object, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    if (list[i] && Field<int32_t>(list[i], ee::kEffectSpellId) == spell) return true;
  }
  return false;
}

// The flame in the hand: NWN's 15 m orange light (Bright Light 20 feet and Dim 20 feet more, as Light's) - a carried
// light for the stealth rules (rules_perception.cpp: visual ids 153-180) - for 10 minutes; cast again, the old ends.
// 0.92.1: applied through DelayCommand, after the feat's spell script (Shillelagh's marker, applied inside it on
// 0.92.0, never showed).
const char* const kFlame = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  if (GetEffectTag(e) == "n5c_flame") RemoveEffect(OBJECT_SELF, e);
  e = GetNextEffect(OBJECT_SELF);
}
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_FLAME_S), OBJECT_SELF);
effect l = EffectLinkEffects(EffectVisualEffect(VFX_DUR_LIGHT_ORANGE_15), EffectVisualEffect(VFX_DUR_CESSATE_POSITIVE));
l = TagEffect(SetEffectSpellId(l, $FLAME), "n5c_flame");
DelayCommand(0.0, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, l, OBJECT_SELF, TurnsToSeconds(10)));
SendMessageToPC(OBJECT_SELF, "nwn5e: Produce Flame - a flame in your hand for 10 minutes (Bright Light 20 feet, " +
                "Dim Light 20 feet more): Hurl it with an action.");
)nss";

// Hurl: a ranged spell attack at the target, d8 Fire (Cantrip Upgrade; doubled on a Critical Hit); the flame stays.
void HurlFlame(uint32_t self, void* caster) {
  if (!HasEffectOfSpell(caster, rules::kProduceFlameFlame)) {
    TellPlayer(self, "nwn5e: Produce Flame - no flame in your hand to hurl.");
    return;
  }
  std::string roll = kAttack;
  Replace(&roll, "$TOUCH", "TouchAttackRanged");
  Replace(&roll, "$BEAM", "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV_FLAME), t);");
  Run(roll, self, "n5_c_produceflame");
  const int level = engine::GetLocalInt(caster, kVarLevel);
  const std::string hits = engine::GetLocalString(caster, kVarHits);
  size_t at = 0;
  while (at < hits.size()) {
    const size_t colon = hits.find(':', at);
    const size_t end = hits.find(';', at);
    if (colon == std::string::npos || end == std::string::npos || colon > end) break;
    const std::string target = hits.substr(at, colon - at);
    const bool critical = atoi(hits.substr(colon + 1, end - colon - 1).c_str()) == 2;
    at = end + 1;
    const int dice = rules::CantripDiceRolled(level, critical);
    const int amount = RollEngineDice(dice, rules::kProduceFlameDie);
    std::string apply = kDamage;
    Replace(&apply, "$TARGET", target);
    Replace(&apply, "$AMOUNT", std::to_string(amount));
    Replace(&apply, "$TYPE", std::to_string(rules::kDamageTypeFire));
    Replace(&apply, "$VIS", "VFX_IMP_FLAME_S");
    Replace(&apply, "$IMMUNE", "");
    Replace(&apply, "$RIDER", "");
    Run(apply, self, "n5_c_produceflame");
    if (g_logged < 60) {
      ++g_logged;
      Log("cantrips: 0x%08X Produce Flame hurled at %s - %d Fire (%d d8%s)", self, target.c_str(), amount, dice,
          critical ? ", a Critical Hit" : "");
    }
  }
}

void ProduceFlameScript(uint32_t self) {
  void* caster = CreatureById(self);
  if (!caster) return;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  if (row == rules::kProduceFlameHurl) {
    HurlFlame(self, caster);
    return;
  }
  std::string code = kFlame;  // the Flame, or the cantrip cast with no choice
  Replace(&code, "$FLAME", std::to_string(rules::kProduceFlameFlame));
  Run(code, self, "n5_c_produceflame");
  Log("cantrips: 0x%08X Produce Flame - a flame in the hand, a light for 10 minutes (row %d)", self, row);
}

// Q1 A: a Bonus Action spell's cast becomes its feat's use (rules::BonusActionFeatOfSpell) - instant, the round's
// attacks go on, one a round (a second waits for the next round): the feat granted if missing. Hurl with no flame is
// refused before it costs the round.
bool BonusActionCastGuard(void* creature, uint32_t spell, uint32_t target) {
  if (!creature || t_bonus_cast) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (spell == static_cast<uint32_t>(rules::kProduceFlameHurl) &&
      !HasEffectOfSpell(creature, rules::kProduceFlameFlame)) {
    TellPlayer(id, "nwn5e: Produce Flame - no flame in your hand to hurl: cast its Flame first (a Bonus Action).");
    return true;
  }
  if (rules::IsShillelagh(static_cast<int>(spell)) &&
      !rules::IsShillelaghWeapon(engine::BaseItemRowInSlot(creature, ee::kSlotRightHand))) {
    TellPlayer(id, "nwn5e: Shillelagh - no Club or Quarterstaff in your hand.");  // #39 3b: before the Bonus Action
    return true;
  }
  const int feat = rules::BonusActionFeatOfSpell(static_cast<int>(spell));
  if (feat < 0 || g_config.actions != rules::RuleVariant::kFaithful || !g_use_feat) return false;
  void* stats = StatsOf(creature);
  if (!stats) return false;
  if (!engine::HasFeat(stats, feat)) engine::AddFeat(stats, feat);
  float nowhere[3] = {0.0f, 0.0f, 0.0f};
  t_bonus_cast = true;
  const int used = g_use_feat(creature, static_cast<uint16_t>(feat), 0, id, ee::kObjectInvalid, nowhere);
  t_bonus_cast = false;
  if (g_logged < 200) {
    ++g_logged;
    Log("cantrips: 0x%08X %s cast as a Bonus Action (feat %d): %s", id, SpellNameOf(static_cast<int>(spell)).c_str(),
        feat, used ? "used" : "waits for the next round, or refused");
  }
  (void)target;
  return true;  // the cast refused: the feat's use ran it
}

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

// cantrip_effects.h: Guidance's 1d4 on a check with its skill.
int GuidanceCheckBonus(void* creature, int nwn_skill) {
  if (!creature || !g_guidance) return 0;
  const int row = ActiveRow(creature, g_guidance);
  if (row < 0) return 0;
  const std::optional<rules::Skill2024> skill = rules::Skill2024ForNwnSkill(nwn_skill);
  if (!skill) return 0;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  uint32_t chosen = 0;
  if (const rules::CantripOption* option = rules::ChoiceOptionOf(row)) {
    chosen = option->value;
  } else {
    chosen = g_guidance_locked.emplace(id, static_cast<uint32_t>(*skill)).first->second;  // the first checked
  }
  if (chosen != static_cast<uint32_t>(*skill)) return 0;
  const int d4 = RollEngineDice(1, 4);
  if (g_logged < 200) {
    ++g_logged;
    Log("cantrips: 0x%08X Guidance (%s): +%d on the check", id, rules::Skill2024Name(*skill), d4);
  }
  return d4;
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
  g_resistance = rules::ChoiceCantripOf(rules::kResistance);
  g_guidance = rules::ChoiceCantripOf(rules::kGuidance);
  const bool choices = AddScriptHandler("n5_c_resist", &ResistanceScript) &&
                       AddScriptHandler("n5_c_guidance", &GuidanceScript) &&
                       AddScriptHandler("n5_c_sparedying", &SpareTheDyingScript) &&
                       AddScriptHandler("n5_c_sorcburst", &SorcerousBurstScript);
  const bool reducer = AddDamageReducer(&ResistanceReducer);
  g_use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through its hook
  const bool flame = AddScriptHandler("n5_c_produceflame", &ProduceFlameScript) &&
                     AddSpellCastGuard(&BonusActionCastGuard) && g_use_feat;
  const bool range = HookExport("CNWSCreature::ComputeSpellRange (Spare the Dying's range by level, #39)",
                                ee::kComputeSpellRange, reinterpret_cast<void*>(&ComputeSpellRange),
                                reinterpret_cast<void**>(&g_compute_spell_range));
  const bool heal = HookExport("CNWSEffectListHandler::OnApplyHeal (Chill Touch: no Hit Points regained, #39)",
                               ee::kOnApplyHeal, reinterpret_cast<void*>(&OnApplyHeal),
                               reinterpret_cast<void**>(&g_on_apply_heal));
  Log("cantrips: SRD 5.2's damage cantrips (%zu; Cantrip Upgrade at 5, 11, 17), Poison / Force / Psychic damage of "
      "their own, NWN's own cantrips off the class lists (#39 Part 1)%s",
      rules::Cantrips().size(), scripts && heal ? "" : " - SOME hooks missing");
  Log("cantrips: build 2 - Resistance, Guidance and Sorcerous Burst cast through a choice (%zu variant rows), "
      "Spare the Dying (#39 Part 1)%s",
      g_resistance && g_guidance ? g_resistance->options.size() + g_guidance->options.size() +
                                       rules::ChoiceCantripOf(rules::kSorcerousBurst)->options.size()
                                 : 0,
      choices && reducer && range ? "" : " - SOME hooks missing");
  Log("cantrips: build 3a - Produce Flame (Flame a Bonus Action, Hurl an action; a carried light) (#39 Part 1)%s",
      flame ? "" : " - SOME hooks missing");
  InstallWeaponCantrips();  // build 3b: Shillelagh, True Strike
  InstallUtilityCantrips();  // build 4a: Fire Play, the doors, Booming Voice, Magic Mark (rules_utility_cantrips.cpp)
}
