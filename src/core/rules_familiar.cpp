// #39 Part 2a, build 4c (owner, 2026-11-16: "go with your recommendations for find familiar, excep allow the player
// to pick alignment too"): Find Familiar (SRD 5.2.1; rules/find_familiar.h).
// - The familiar is NWN's own kind (its master's familiar slot - so NWN's possession is its eyes): the engine's
//   SummonFamiliar with the master's familiar type set to the bat's for the call (its own put back), then remade -
//   the form's model and size (appearance.2da, scaled), name, abilities (the engine's base setters), its kind's racial
//   type and alignment (Celestial, Fey or Fiend), hit points (CNWSCreature::GetMaxHitPoints answered for it) and AC
//   (rules_attack.cpp asks FamiliarArmorClass); its combat scripts cleared and its blows skipped - it can't attack.
// - One familiar: a cast with one out (or away) gives it the new form; NWN's own familiar (the Summon Familiar feat,
//   kept - Q3 A) goes.
// - Seeing through its eyes is its master's Bonus Action: CNWSCreature::PossessFamiliar, the Bonus Action spent first.
// - Touch spells: one cast at a creature out of its master's reach and in the familiar's (within 100 feet of the
//   master), its Reaction ready - the order goes ahead with the familiar's reach (rules_cantrips.cpp's
//   ComputeSpellRange asks FamiliarTouchRangeMetres), and the cast takes its Reaction.
// - Dismiss (to a pocket dimension) and Recall (within 30 feet): feats casting rows 1225 / 1226 (a Magic action each),
//   in Find Familiar's menu while held; its hit points kept while it's away. At 0 HP it disappears.
// - Celestial, Fey or Fiend: the player's pick in !options (Spellcasting; a record), else by alignment.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "action_guard.h"
#include "attack_roll.h"
#include "bonus_actions.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "reactions.h"
#include "records.h"
#include "script_hooks.h"
#include "spell_cast_listeners.h"
#include "spell_impact.h"
#include "rules/combat.h"
#include "rules/find_familiar.h"
#include "rules/spells2024.h"

int FamiliarArmorClass(void* stats);                              // rules_attack.cpp
float FamiliarTouchRangeMetres(void* creature, uint32_t spell);   // rules_cantrips.cpp
void FindFamiliarSpell(uint32_t self, void* caster, int row);     // rules_spells2024.cpp (UtilitySpell)
void FinishSummon(uint32_t self);                                 // below (n5_f_summon)
bool FamiliarCaster(void* creature);                              // combat_options.cpp
int FamiliarKindPick(void* creature);
void SetFamiliarKindPick(void* creature, int pick);
bool InstallFindFamiliar();                                       // rules_spells2024.cpp's install

namespace {

constexpr size_t kStatsFamiliarType = 0xe34;  // int32, hen_familiar.2da's row (GetFamiliarCreatureType; SummonFamiliar)
constexpr int32_t kBatFamiliarType = 0;       // hen_familiar 0: NW_FM_BAT - the familiar Find Familiar remakes
constexpr const char* kVarFamiliarKind = "nwn5e_familiar_kind";  // a record: 0 by alignment, 1 Celestial, 2 Fey, 3 Fiend
constexpr float kMetresPerFoot = 0.3048f;
constexpr ULONGLONG kTouchWaitMs = 15000;     // a delivered touch's order, until it's cast

struct Familiar {
  uint32_t creature = 0;  // its object; 0 while it's away
  rules::FamiliarForm form = rules::FamiliarForm::kNone;
  rules::FamiliarKind kind = rules::FamiliarKind::kFey;
  int hit_points = 0;     // kept while it's away (0: full)
  bool dismissed = false;
  ULONGLONG next = 0;
};
std::unordered_map<uint32_t, Familiar> g_familiars;  // by master

struct FormNumbers {
  int hit_points = 1;
  int armor_class = 10;
};
std::unordered_map<uint32_t, FormNumbers> g_forms;  // by familiar creature

struct Touch {
  uint32_t spell = 0;
  uint32_t familiar = 0;
  ULONGLONG until = 0;
};
std::unordered_map<uint32_t, Touch> g_touch;  // by master: the touch spell its familiar delivers

int g_logged = 0;
bool LogSlot() { return g_logged < 400 && ++g_logged; }

bool Faithful() { return g_config.spellcasting == rules::RuleVariant::kFaithful; }

uint32_t ObjId(void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : 0; }

uint32_t ParseHex(const std::string& text) { return static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 16)); }

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0) Log("familiar: %s - its script returned %d", what, result);
}

float FeetBetween(void* a, void* b) {
  const float dx = Field<float>(a, ee::kObjectPositionX) - Field<float>(b, ee::kObjectPositionX);
  const float dy = Field<float>(a, ee::kObjectPositionY) - Field<float>(b, ee::kObjectPositionY);
  return std::sqrt(dx * dx + dy * dy) / kMetresPerFoot;
}

// The master's familiar now (NWN's GetAssociate: the feat's or Find Familiar's), 0 for none.
uint32_t AssociateFamiliar(uint32_t master, void* creature) {
  Run("object f = GetAssociate(ASSOCIATE_TYPE_FAMILIAR, OBJECT_SELF); SetLocalString(OBJECT_SELF, \"n5f_fam\", "
      "GetIsObjectValid(f) ? ObjectToString(f) : \"\");",
      master, "its familiar");
  return ParseHex(engine::GetLocalString(creature, "n5f_fam"));
}

using SetBaseFn = void (*)(void* stats, uint8_t value);
using SetConBaseFn = void (*)(void* stats, uint8_t value, int keep);
void SetAbilityBase(void* stats, int i, int value) {
  const uintptr_t setter = EngineExport(ee::kSetAbilityBase[i]);
  if (!setter) return;
  if (i == 2) {
    reinterpret_cast<SetConBaseFn>(setter)(stats, static_cast<uint8_t>(value), 1);
  } else {
    reinterpret_cast<SetBaseFn>(setter)(stats, static_cast<uint8_t>(value));
  }
}

const char* AlignmentConstant(rules::FamiliarKind kind) {
  switch (rules::FamiliarGoodEvil(kind)) {
    case 4: return "ALIGNMENT_GOOD";
    case 5: return "ALIGNMENT_EVIL";
    default: return "ALIGNMENT_NEUTRAL";
  }
}

std::string FamiliarName(const Familiar& f) {
  return std::string(rules::FamiliarStatsOf(f.form).name) + " (" + rules::FamiliarKindName(f.kind) + ")";
}

// The familiar remade to its form and kind; its hit points the kept ones (0: the form's).
void Remake(uint32_t familiar, const Familiar& f) {
  void* creature = CreatureById(familiar);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return;
  const rules::FamiliarStats& s = rules::FamiliarStatsOf(f.form);
  g_forms[familiar] = FormNumbers{s.hit_points, s.armor_class};
  char code[1400];
  sprintf_s(code,
            "object f = OBJECT_SELF; SetCreatureAppearanceType(f, %d);"
            " SetObjectVisualTransform(f, OBJECT_VISUAL_TRANSFORM_SCALE, %.2f); SetName(f, \"%s\");"
            " AdjustAlignment(f, %s, 100, FALSE);"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_NOTICE, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_END_COMBATROUND, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_MELEE_ATTACKED, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_DAMAGED, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_SPELLCASTAT, \"\");"
            " SetLocalInt(f, \"n5f_familiar\", 1); SetLocalInt(f, \"n5f_form\", %d); SetLocalInt(f, \"n5f_kind\", %d);"
            " ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_POLYMORPH), f);",
            s.appearance, s.scale, FamiliarName(f).c_str(), AlignmentConstant(f.kind), static_cast<int>(f.form),
            static_cast<int>(f.kind));
  Run(code, familiar, "the familiar's form");
  for (int i = 0; i < 6; ++i) SetAbilityBase(stats, i, s.abilities[i]);
  SetField<uint16_t>(stats, ee::kStatsRace, static_cast<uint16_t>(rules::FamiliarRacialType(f.kind)));
  SetField<int32_t>(creature, ee::kCreatureSize, 1);  // Tiny
  const int hit_points = f.hit_points > 0 ? std::min(f.hit_points, s.hit_points) : s.hit_points;
  Run("SetCurrentHitPoints(OBJECT_SELF, " + std::to_string(hit_points) + ");", familiar, "the familiar's hit points");
}

// NWN's SummonFamiliar, the master's familiar type the bat's for the call; the new familiar, 0 if none came.
uint32_t Summon(uint32_t master, void* caster) {
  void* stats = StatsOf(caster);
  if (!stats) return 0;
  const int32_t own = Field<int32_t>(stats, kStatsFamiliarType);
  SetField<int32_t>(stats, kStatsFamiliarType, kBatFamiliarType);
  Run("SummonFamiliar(OBJECT_SELF);", master, "the familiar's summoning");
  SetField<int32_t>(stats, kStatsFamiliarType, own);
  return AssociateFamiliar(master, caster);
}

void GrantFeat(void* caster, int feat, bool on) {
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats) return;
  if (on && !engine::HasFeat(stats, feat)) engine::AddFeat(stats, feat);
  if (!on && engine::HasFeat(stats, feat)) engine::RemoveFeat(stats, feat);
}

int GoodEvilOf(uint32_t master, void* caster) {
  Run("SetLocalInt(OBJECT_SELF, \"n5f_ge\", GetAlignmentGoodEvil(OBJECT_SELF));", master, "the caster's alignment");
  return engine::GetLocalInt(caster, "n5f_ge");
}

void Dismiss(uint32_t self, void* caster) {
  const auto it = g_familiars.find(self);
  if (it == g_familiars.end() || it->second.dismissed || !CreatureById(it->second.creature)) {
    TellPlayer(self, "nwn5e: Find Familiar - no familiar of yours is here to dismiss.");
    return;
  }
  Familiar& f = it->second;
  Run("SetLocalInt(OBJECT_SELF, \"n5f_hp\", GetCurrentHitPoints(OBJECT_SELF)); ApplyEffectAtLocation(DURATION_TYPE_INSTANT,"
      " EffectVisualEffect(VFX_IMP_UNSUMMON), GetLocation(OBJECT_SELF)); DestroyObject(OBJECT_SELF);",
      f.creature, "the familiar's dismissal");
  if (void* creature = CreatureById(f.creature)) f.hit_points = engine::GetLocalInt(creature, "n5f_hp");
  g_forms.erase(f.creature);
  f.creature = 0;
  f.dismissed = true;
  engine::SetLocalInt(caster, "n5f_away", 1);  // a save keeps it away
  engine::SetLocalInt(caster, "n5f_form", static_cast<int>(f.form));
  engine::SetLocalInt(caster, "n5f_kind", static_cast<int>(f.kind));
  engine::SetLocalInt(caster, "n5f_hp", f.hit_points);
  GrantFeat(caster, rules::kFeatFamiliarDismiss, false);
  GrantFeat(caster, rules::kFeatFamiliarRecall, true);
  TellPlayer(self, "nwn5e: Find Familiar - your " + FamiliarName(f) + " goes to a pocket dimension (its Recall brings it "
                   "back within 30 feet).");
  if (LogSlot()) Log("familiar: 0x%08X dismissed its familiar (%d hit points kept)", self, f.hit_points);
}

void Recall(uint32_t self, void* caster) {
  const auto it = g_familiars.find(self);
  if (it == g_familiars.end() || !it->second.dismissed) {
    TellPlayer(self, "nwn5e: Find Familiar - no familiar of yours is away to recall.");
    return;
  }
  Run("vector v = GetPositionFromLocation(GetSpellTargetLocation()); SetLocalString(OBJECT_SELF, \"n5f_at\", "
      "FloatToString(v.x, 0, 2) + \" \" + FloatToString(v.y, 0, 2) + \" \" + FloatToString(v.z, 0, 2));",
      self, "the recall's point");
  float x = 0.0f, y = 0.0f, z = 0.0f;
  if (sscanf_s(engine::GetLocalString(caster, "n5f_at").c_str(), "%f %f %f", &x, &y, &z) != 3) return;
  const float dx = x - Field<float>(caster, ee::kObjectPositionX);
  const float dy = y - Field<float>(caster, ee::kObjectPositionY);
  if (std::sqrt(dx * dx + dy * dy) / kMetresPerFoot > static_cast<float>(rules::kFamiliarRecallFeet)) {
    TellPlayer(self, "nwn5e: Find Familiar - it comes back to a spot within 30 feet of you.");
    return;
  }
  Familiar& f = it->second;
  const uint32_t familiar = Summon(self, caster);
  if (!familiar) {
    TellPlayer(self, "nwn5e: Find Familiar - your familiar didn't come back (see nwn5e.log).");
    Log("familiar: 0x%08X Recall - SummonFamiliar gave no familiar", self);
    return;
  }
  f.creature = familiar;
  f.dismissed = false;
  Remake(familiar, f);
  char code[260];
  sprintf_s(code, "JumpToLocation(Location(GetArea(OBJECT_SELF), Vector(%.2f, %.2f, %.2f), GetFacing(OBJECT_SELF)));", x,
            y, z);
  Run(code, familiar, "the familiar's return");
  f.hit_points = 0;
  engine::SetLocalInt(caster, "n5f_away", 0);
  GrantFeat(caster, rules::kFeatFamiliarRecall, false);
  GrantFeat(caster, rules::kFeatFamiliarDismiss, true);
  TellPlayer(self, "nwn5e: Find Familiar - your " + FamiliarName(f) + " is back.");
  if (LogSlot()) Log("familiar: 0x%08X recalled its familiar 0x%08X", self, familiar);
}

// Every creature update: a master's familiar looks every second - gone (0 hit points, or NWN's Unsummon), it's let go.
std::unordered_map<uint32_t, ULONGLONG> g_reload_look;  // a player character -> its next look for a kept familiar

// A player character's Find Familiar familiar kept in a save (its form and kind on it, or on the master while it's
// away): found again, every 5 s until it is.
void FindKeptFamiliar(uint32_t id, void* creature) {
  if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  ULONGLONG& next = g_reload_look[id];
  const ULONGLONG now = GetTickCount64();
  if (now < next) return;
  next = now + 5000;
  Familiar f;
  if (engine::GetLocalInt(creature, "n5f_away") == 1) {
    f.dismissed = true;
    f.form = static_cast<rules::FamiliarForm>(engine::GetLocalInt(creature, "n5f_form"));
    f.kind = static_cast<rules::FamiliarKind>(engine::GetLocalInt(creature, "n5f_kind"));
    f.hit_points = engine::GetLocalInt(creature, "n5f_hp");
  } else {
    const uint32_t familiar = AssociateFamiliar(id, creature);
    void* fam = familiar ? CreatureById(familiar) : nullptr;
    if (!fam || engine::GetLocalInt(fam, "n5f_familiar") != 1) return;
    f.creature = familiar;
    f.form = static_cast<rules::FamiliarForm>(engine::GetLocalInt(fam, "n5f_form"));
    f.kind = static_cast<rules::FamiliarKind>(engine::GetLocalInt(fam, "n5f_kind"));
    const rules::FamiliarStats& s = rules::FamiliarStatsOf(f.form);
    g_forms[familiar] = FormNumbers{s.hit_points, s.armor_class};
  }
  if (f.form == rules::FamiliarForm::kNone) return;
  g_familiars[id] = f;
  if (LogSlot()) Log("familiar: 0x%08X its %s found again (a loaded game)%s", id, FamiliarName(f).c_str(),
                     f.dismissed ? " - away" : "");
}

void FamiliarUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = ObjId(creature);
  auto it = g_familiars.find(id);
  if (it == g_familiars.end()) {
    FindKeptFamiliar(id, creature);
    return;
  }
  Familiar& f = it->second;
  const ULONGLONG now = GetTickCount64();
  if (now < f.next) return;
  f.next = now + 1000;
  if (f.dismissed || !f.creature) return;
  void* familiar = CreatureById(f.creature);
  if (familiar && !engine::IsDeadOrDying(familiar)) return;
  if (familiar) Run("SetIsDestroyable(TRUE, FALSE, FALSE); DestroyObject(OBJECT_SELF);", f.creature, "the familiar's end");
  g_forms.erase(f.creature);
  const std::string name = FamiliarName(f);
  g_familiars.erase(it);
  GrantFeat(creature, rules::kFeatFamiliarDismiss, false);
  GrantFeat(creature, rules::kFeatFamiliarRecall, false);
  TellPlayer(id, "nwn5e: Find Familiar - your " + name + " is gone; cast Find Familiar again for another.");
  if (LogSlot()) Log("familiar: 0x%08X its familiar is gone (%s)", id, familiar ? "0 hit points" : "unsummoned");
}

// Its hit points: the form's (SRD 5.2.1) - CNWSCreature::GetMaxHitPoints answered for a Find Familiar familiar.
using MaxHitPointsFn = int16_t (*)(void* creature, int toughness);
MaxHitPointsFn g_max_hit_points = nullptr;
int16_t MaxHitPoints(void* creature, int toughness) {
  if (!g_forms.empty() && creature) {
    const auto it = g_forms.find(ObjId(creature));
    if (it != g_forms.end()) return static_cast<int16_t>(it->second.hit_points);
  }
  return g_max_hit_points(creature, toughness);
}

// Its master looks through its eyes: NWN's possession, a Bonus Action (SRD 5.2.1) with the actions faithful.
using PossessFn = void (*)(void* creature);
PossessFn g_possess = nullptr;
void Possess(void* creature) {
  if (creature && Faithful() && ActionEconomyOn() && g_familiars.count(ObjId(creature)) &&
      !SpendBonusAction(creature, "Find Familiar: through its eyes")) {
    return;  // told
  }
  g_possess(creature);
}

// "A familiar can't attack": its blows skipped.
bool FamiliarBlow(AttackRoll& roll, uintptr_t) {
  if (g_forms.empty() || !roll.attacker) return false;
  return g_forms.count(ObjId(roll.attacker)) != 0;
}

// A touch spell cast at a creature its familiar can touch: the order goes ahead with the familiar's reach.
bool FamiliarTouchOrder(void* creature, const SpellOrder& order) {
  if (g_familiars.empty() || !creature || order.fake || order.class_slot < 0 || order.class_slot > 7) return false;
  const uint32_t id = ObjId(creature);
  const auto it = g_familiars.find(id);
  if (it == g_familiars.end() || it->second.dismissed) return false;
  void* familiar = CreatureById(it->second.creature);
  void* target = order.target != ee::kObjectInvalid && order.target != id ? GameObjectById(order.target) : nullptr;
  if (!familiar || !target) return false;
  char code[160];
  sprintf_s(code, "SetLocalString(OBJECT_SELF, \"n5f_rng\", Get2DAString(\"spells\", \"Range\", %u));", order.spell);
  Run(code, id, "the spell's range");
  if (engine::GetLocalString(creature, "n5f_rng") != "T") return false;
  const float from_caster = FeetBetween(creature, target);
  const float from_familiar = FeetBetween(familiar, target);
  const float familiar_off = FeetBetween(familiar, creature);
  if (!rules::FamiliarDeliversTouch(from_caster, from_familiar, familiar_off, ReactionReady(familiar))) return false;
  g_touch[id] = Touch{order.spell, it->second.creature, GetTickCount64() + kTouchWaitMs};
  if (LogSlot()) {
    Log("familiar: 0x%08X spell %u at 0x%08X - its familiar delivers the touch (target %.0f ft off, %.0f ft from the "
        "familiar)",
        id, order.spell, order.target, from_caster, from_familiar);
  }
  return false;  // the order goes ahead
}

// The touch's cast: the familiar's Reaction taken.
void FamiliarTouchCast(void* creature, uint32_t spell) {
  if (g_touch.empty() || !creature) return;
  const uint32_t id = ObjId(creature);
  const auto it = g_touch.find(id);
  if (it == g_touch.end() || it->second.spell != spell) return;
  const uint32_t familiar = it->second.familiar;
  g_touch.erase(it);
  void* fam = CreatureById(familiar);
  if (!fam) return;
  TakeReaction(fam);
  Run("ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_ODD), OBJECT_SELF);", familiar,
      "the familiar's touch");
  const auto f = g_familiars.find(id);
  TellPlayer(id, "nwn5e: Find Familiar - your " + (f != g_familiars.end() ? FamiliarName(f->second) : std::string("familiar")) +
                     " delivers the touch (its Reaction).");
}

// Dismiss with a familiar here; Recall with one away.
bool FamiliarFeatGuard(void* creature, int feat) {
  if (feat != rules::kFeatFamiliarDismiss && feat != rules::kFeatFamiliarRecall) return true;
  const uint32_t id = ObjId(creature);
  const auto it = g_familiars.find(id);
  const bool away = it != g_familiars.end() && it->second.dismissed;
  const bool here = it != g_familiars.end() && !it->second.dismissed && CreatureById(it->second.creature);
  if (feat == rules::kFeatFamiliarDismiss && !here) {
    TellPlayer(id, "nwn5e: Find Familiar - no familiar of yours is here to dismiss.");
    GrantFeat(creature, rules::kFeatFamiliarDismiss, false);
    return false;
  }
  if (feat == rules::kFeatFamiliarRecall && !away) {
    TellPlayer(id, "nwn5e: Find Familiar - no familiar of yours is away to recall.");
    GrantFeat(creature, rules::kFeatFamiliarRecall, false);
    return false;
  }
  return true;
}

void FamiliarsGameStarted() {
  g_familiars.clear();
  g_reload_look.clear();
  g_forms.clear();
  g_touch.clear();
}

}  // namespace

int FamiliarArmorClass(void* stats) {
  if (g_forms.empty() || !stats) return 0;
  const auto it = g_forms.find(ObjectIdOf(stats));
  return it == g_forms.end() ? 0 : it->second.armor_class;
}

float FamiliarTouchRangeMetres(void* creature, uint32_t spell) {
  if (g_touch.empty() || !creature) return 0.0f;
  const auto it = g_touch.find(ObjId(creature));
  if (it == g_touch.end() || it->second.spell != spell) return 0.0f;
  if (GetTickCount64() > it->second.until) {
    g_touch.erase(it);
    return 0.0f;
  }
  return static_cast<float>(rules::kFamiliarTelepathyFeet) * kMetresPerFoot;
}

bool FamiliarCaster(void* creature) {
  void* stats = creature && Faithful() ? StatsOf(creature) : nullptr;
  return stats && ClassLevel2024(stats, rules::CharacterClass::kWizard) > 0;
}

int FamiliarKindPick(void* creature) {
  if (!creature) return rules::kFamiliarByAlignment;
  EnsureRecords(creature);
  return engine::GetLocalInt(creature, kVarFamiliarKind);
}

void SetFamiliarKindPick(void* creature, int pick) {
  if (!creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarFamiliarKind, pick);
  Log("familiar: 0x%08X its familiar's kind - %s", ObjId(creature),
      pick == 0 ? "by alignment" : rules::FamiliarKindName(static_cast<rules::FamiliarKind>(pick)));
}

void FindFamiliarSpell(uint32_t self, void* caster, int row) {
  if (row == rules::kFamiliarDismiss) return Dismiss(self, caster);
  if (row == rules::kFamiliarRecall) return Recall(self, caster);
  const rules::FamiliarForm form = rules::FamiliarFormOf(row);
  if (form == rules::FamiliarForm::kNone) {
    TellPlayer(self, "nwn5e: Find Familiar - pick a form from its menu.");
    return;
  }
  Familiar& f = g_familiars[self];
  f.form = form;
  f.kind = rules::FamiliarKindFor(FamiliarKindPick(caster), GoodEvilOf(self, caster));
  // One familiar: one of this spell's out takes the new form (away, it changes there).
  if (f.dismissed) {
    TellPlayer(self, "nwn5e: Find Familiar - your familiar, away in its pocket dimension, becomes a " + FamiliarName(f) + ".");
    return;
  }
  if (f.creature && CreatureById(f.creature) && !engine::IsDeadOrDying(CreatureById(f.creature))) {
    Remake(f.creature, f);
    TellPlayer(self, "nwn5e: Find Familiar - your familiar becomes a " + FamiliarName(f) + ".");
    if (LogSlot()) Log("familiar: 0x%08X its familiar 0x%08X becomes a %s", self, f.creature, FamiliarName(f).c_str());
    return;
  }
  // NWN's own familiar (the Summon Familiar feat) goes - one familiar at a time; it's gone at the frame's end, so
  // the summoning waits a moment (n5_f_summon).
  f.hit_points = 0;
  if (const uint32_t other = AssociateFamiliar(self, caster)) {
    Run("DestroyObject(OBJECT_SELF);", other, "NWN's familiar");
    Run("DelayCommand(0.5, ExecuteScript(\"n5_f_summon\", OBJECT_SELF));", self, "the summoning's wait");
    if (LogSlot()) Log("familiar: 0x%08X NWN's familiar 0x%08X goes", self, other);
    return;
  }
  FinishSummon(self);
}

// The summoning (its form and kind picked by the cast).
void FinishSummon(uint32_t self) {
  void* caster = CreatureById(self);
  const auto at = g_familiars.find(self);
  if (!caster || at == g_familiars.end()) return;
  Familiar& f = at->second;
  const rules::FamiliarForm form = f.form;
  const uint32_t familiar = Summon(self, caster);
  if (!familiar) {
    g_familiars.erase(self);
    TellPlayer(self, "nwn5e: Find Familiar - no familiar came (see nwn5e.log).");
    Log("familiar: 0x%08X SummonFamiliar gave no familiar", self);
    return;
  }
  f.creature = familiar;
  f.dismissed = false;
  Remake(familiar, f);
  GrantFeat(caster, rules::kFeatFamiliarDismiss, true);
  GrantFeat(caster, rules::kFeatFamiliarRecall, false);
  TellPlayer(self, "nwn5e: Find Familiar - a " + FamiliarName(f) + " serves you. It can't attack; Possess Familiar (your "
                   "radial) looks through its eyes - a Bonus Action; it delivers your touch spells within 100 feet (its "
                   "Reaction); Dismiss and Recall are in Find Familiar's menu.");
  if (LogSlot()) {
    const rules::FamiliarStats& s = rules::FamiliarStatsOf(form);
    Log("familiar: 0x%08X Find Familiar - 0x%08X a %s: AC %d, %d HP, appearance %d at %.2f", self, familiar,
        FamiliarName(f).c_str(), s.armor_class, s.hit_points, s.appearance, s.scale);
  }
}

bool InstallFindFamiliar() {
  RegisterRecords({kVarFamiliarKind});
  const bool waits = AddScriptHandler("n5_f_summon", &FinishSummon);
  const bool max_hp = HookExport("CNWSCreature::GetMaxHitPoints (Find Familiar's forms, #39 4c)", ee::kGetMaxHitPoints,
                                 reinterpret_cast<void*>(&MaxHitPoints), reinterpret_cast<void**>(&g_max_hit_points));
  const bool possess = HookExport("CNWSCreature::PossessFamiliar (its eyes a Bonus Action, #39 4c)",
                                  "?PossessFamiliar@CNWSCreature@@QEAAXXZ", reinterpret_cast<void*>(&Possess),
                                  reinterpret_cast<void**>(&g_possess));
  const bool rest = AddAttackRollSkipper(&FamiliarBlow) && AddSpellOrderGuard(&FamiliarTouchOrder) &&
                    AddSpellCastListener(&FamiliarTouchCast) && AddFeatUseGuard(&FamiliarFeatGuard) &&
                    AddCreatureUpdateListener(&FamiliarUpdate) && AddGameStartListener(&FamiliarsGameStarted);
  Log("familiar: Find Familiar (#39 build 4c) - the eight forms, Celestial / Fey / Fiend, its eyes a Bonus Action, its "
      "touch, Dismiss and Recall%s",
      max_hp && possess && rest && waits ? "" : " - SOME hooks missing");
  return max_hp && possess && rest && waits;
}
