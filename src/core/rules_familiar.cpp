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
// - Celestial, Fey or Fiend: picked in its menu, between the form and the slot level (0.99.49; a form's own row: by
//   alignment).
// - 0.99.49 (owner, 2026-11-16, L4-31: the raven used its fear gaze and swung at an orc): NWN's associate heartbeat
//   fights for its master - the familiar's own follows it (n5_f_hb: not when told to stand its ground, nor while
//   possessed), and its casts (the blueprint's gaze) are refused.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <set>
#include <unordered_map>

#include "action_guard.h"
#include "attack_roll.h"
#include "bonus_actions.h"
#include "chat.h"
#include "damage_listeners.h"
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
#include "player_text.h"
#include "reactions.h"
#include "records.h"
#include "script_hooks.h"
#include "spell_cast_listeners.h"
#include "spell_impact.h"
#include "rules/combat.h"
#include "rules/find_familiar.h"
#include "rules/spells2024.h"

int FamiliarArmorClass(void* stats);                              // rules_attack.cpp
int FamiliarMaxHitPoints(void* creature);                         // rules_hit_points.cpp
float FamiliarTouchRangeMetres(void* creature, uint32_t spell);   // rules_cantrips.cpp
void FindFamiliarSpell(uint32_t self, void* caster, int row);     // rules_spells2024.cpp (UtilitySpell)
void FinishSummon(uint32_t self);                                 // below (n5_f_summon)
bool InstallFindFamiliar();                                       // rules_spells2024.cpp's install

namespace {

constexpr size_t kStatsFamiliarType = 0xe34;  // int32, hen_familiar.2da's row (GetFamiliarCreatureType; SummonFamiliar)
constexpr int32_t kBatFamiliarType = 0;       // hen_familiar 0: NW_FM_BAT - the familiar Find Familiar remakes
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
// 0.99.62: its look's lines (a few each), its first heartbeat, and others' shouts it ignores (a few)
std::unordered_map<uint32_t, int> g_tend_logged;
std::set<uint32_t> g_heartbeats;
int g_others_heard = 0;
void Tend(uint32_t familiar, void* creature, const char* by);  // its look (below)

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
  return PlayerText("familiar.name",
                    {{"form", rules::FamiliarStatsOf(f.form).name}, {"kind", rules::FamiliarKindName(f.kind)}});
}

// The familiar remade to its form and kind; its hit points the kept ones (0: the form's).
void Remake(uint32_t familiar, const Familiar& f) {
  void* creature = CreatureById(familiar);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return;
  const rules::FamiliarStats& s = rules::FamiliarStatsOf(f.form);
  g_forms[familiar] = FormNumbers{s.hit_points, s.armor_class};
  char code[1800];
  sprintf_s(code,
            "object f = OBJECT_SELF; SetCreatureAppearanceType(f, %d);"
            " SetObjectVisualTransform(f, OBJECT_VISUAL_TRANSFORM_SCALE, %.2f); SetName(f, \"%s\");"
            " AdjustAlignment(f, %s, 100, FALSE);"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_NOTICE, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_END_COMBATROUND, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_MELEE_ATTACKED, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_DAMAGED, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_SPELLCASTAT, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_HEARTBEAT, \"n5_f_hb\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_DIALOGUE, \"n5_f_talk\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_USER_DEFINED_EVENT, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_BLOCKED_BY_DOOR, \"\");"
            " SetEventScript(f, EVENT_SCRIPT_CREATURE_ON_RESTED, \"\");"
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
    TellPlayer(self, PlayerText("familiar.none_here"));
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
  TellPlayer(self, PlayerText("familiar.dismissed", {{"name", FamiliarName(f)}}));
  if (LogSlot()) Log("familiar: 0x%08X dismissed its familiar (%d hit points kept)", self, f.hit_points);
}

void Recall(uint32_t self, void* caster) {
  const auto it = g_familiars.find(self);
  if (it == g_familiars.end() || !it->second.dismissed) {
    TellPlayer(self, PlayerText("familiar.none_away"));
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
    TellPlayer(self, PlayerText("familiar.recall_far"));
    return;
  }
  Familiar& f = it->second;
  const uint32_t familiar = Summon(self, caster);
  if (!familiar) {
    TellPlayer(self, PlayerText("familiar.recall_failed"));
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
  TellPlayer(self, PlayerText("familiar.back", {{"name", FamiliarName(f)}}));
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
  if (familiar && !engine::IsDeadOrDying(familiar)) {
    Tend(f.creature, familiar, "its master");  // 0.99.62
    return;
  }
  if (familiar) Run("SetIsDestroyable(TRUE, FALSE, FALSE); DestroyObject(OBJECT_SELF);", f.creature, "the familiar's end");
  g_forms.erase(f.creature);
  const std::string name = FamiliarName(f);
  g_familiars.erase(it);
  GrantFeat(creature, rules::kFeatFamiliarDismiss, false);
  GrantFeat(creature, rules::kFeatFamiliarRecall, false);
  TellPlayer(id, PlayerText("familiar.gone", {{"name", name}}));
  if (LogSlot()) Log("familiar: 0x%08X its familiar is gone (%s)", id, familiar ? "0 hit points" : "unsummoned");
}

// Its hit points: the form's (SRD 5.2.1) - CNWSCreature::GetMaxHitPoints answered for a Find Familiar familiar: by
// rules_hit_points.cpp's hook (FamiliarMaxHitPoints) with hit points faithful, else by this one (0.99.48's launch
// check: two hooks on it fail - MH_ERROR_ALREADY_CREATED).
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
      !SpendBonusAction(creature, PlayerText("familiar.eyes").c_str())) {
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
  // 0.99.62 (L4-41): edge to edge, as reach is - a Tiny raven by a Medium orc 8 ft centre to centre is nearer
  const float from_caster = engine::EdgeDistance(creature, target) / kMetresPerFoot;
  const float from_familiar = engine::EdgeDistance(familiar, target) / kMetresPerFoot;
  const float familiar_off = FeetBetween(familiar, creature);
  const bool ready = ReactionReady(familiar);
  if (!rules::FamiliarDeliversTouch(from_caster, from_familiar, familiar_off, ready)) {
    if (LogSlot()) {
      Log("familiar: 0x%08X touch spell %u at 0x%08X not delivered - the target %.0f ft from you, %.0f ft from your "
          "familiar (edge to edge; its reach 5 ft), the familiar %.0f ft from you (100 at most), its Reaction %s",
          id, order.spell, order.target, from_caster, from_familiar, familiar_off, ready ? "ready" : "spent");
    }
    return false;
  }
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
  TellPlayer(id, PlayerText("familiar.touch", {{"name", f != g_familiars.end() ? FamiliarName(f->second)
                                                                         : PlayerText("familiar.your_familiar")}}));
}

// 0.99.49: its heartbeat follows its master. 0.99.62 (L4-39, owner 2026-11-17: "did not follow unless instructed to
// and then it did fight without being asked to"): its master looks after it every second too (FamiliarUpdate), the
// same look - an attack or a cast of its own cleared (a familiar can't attack), and it follows (closer than 3 m it
// waits) unless told to stand its ground (x0_i0_assoc's NW_ASC_MODE_STAND_GROUND in NW_ASSOCIATE_MASTER: the radial's
// word, n5_f_talk, and NWN's familiar dialog set it alike) or possessed.
const char* const kTend = R"nss(
object m = GetMaster(OBJECT_SELF);
int a = GetCurrentAction();
int cleared = 0; int followed = 0;
if (!GetIsPossessedFamiliar(OBJECT_SELF)) {
  if (a == ACTION_ATTACKOBJECT || a == ACTION_CASTSPELL || a == ACTION_ITEMCASTSPELL || a == ACTION_COUNTERSPELL) {
    ClearAllActions(TRUE); cleared = a; a = ACTION_INVALID;
  }
  if (GetIsObjectValid(m) && !(GetLocalInt(OBJECT_SELF, "NW_ASSOCIATE_MASTER") & 0x08000000) &&
      GetArea(m) == GetArea(OBJECT_SELF) && GetDistanceToObject(m) > 3.0 && a != ACTION_FOLLOW) {
    ActionForceFollowObject(m, 2.0); followed = 1;
  }
}
SetLocalInt(OBJECT_SELF, "n5f_cleared", cleared);
SetLocalInt(OBJECT_SELF, "n5f_followed", followed);
)nss";

void Tend(uint32_t familiar, void* creature, const char* by) {
  if (!creature) return;
  Run(kTend, familiar, "the familiar's look");
  const int cleared = engine::GetLocalInt(creature, "n5f_cleared");
  const bool followed = engine::GetLocalInt(creature, "n5f_followed") != 0;
  int& logged = g_tend_logged[familiar];
  if (cleared && logged < 12 && ++logged) {
    Log("familiar: 0x%08X (%s) its own action %d cleared - a familiar can't attack", familiar, by, cleared);
  }
  if (followed && logged < 12 && ++logged) Log("familiar: 0x%08X (%s) follows its master", familiar, by);
}

void FamiliarHeartbeat(uint32_t self) {
  if (g_heartbeats.insert(self).second) Log("familiar: 0x%08X its heartbeat runs (its first this game)", self);
  Tend(self, CreatureById(self), "its heartbeat");
}

// 0.99.62: its listening is nwn5e's (n5_f_talk) - NWN's associate script fought for its master (L4-39). Its
// master's word only (rules::FamiliarOrderOf): a click its dialog, Stand your ground, Follow me; no one else's shout.
void FamiliarTalk(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  Run("SetLocalInt(OBJECT_SELF, \"n5f_pat\", GetListenPatternNumber()); SetLocalInt(OBJECT_SELF, \"n5f_bymaster\", "
      "GetLastSpeaker() == GetMaster(OBJECT_SELF));",
      self, "the familiar's listening");
  const int pattern = engine::GetLocalInt(creature, "n5f_pat");
  const bool by_master = engine::GetLocalInt(creature, "n5f_bymaster") != 0;
  const rules::FamiliarOrder order = by_master ? rules::FamiliarOrderOf(pattern) : rules::FamiliarOrder::kNone;
  if (by_master ? LogSlot() : (g_others_heard < 5 && ++g_others_heard)) {
    Log("familiar: 0x%08X hears %d (%s)%s", self, pattern,
        order == rules::FamiliarOrder::kTalk     ? "its dialog"
        : order == rules::FamiliarOrder::kStay   ? "stay"
        : order == rules::FamiliarOrder::kFollow ? "follow"
                                                 : "nothing it does",
        by_master ? "" : " - not its master's");
  }
  switch (order) {
    case rules::FamiliarOrder::kTalk:
      Run("if (!BeginConversation()) ActionStartConversation(GetLastSpeaker());", self, "the familiar's dialog");
      break;
    case rules::FamiliarOrder::kStay:
      Run("SetLocalInt(OBJECT_SELF, \"NW_ASSOCIATE_MASTER\", GetLocalInt(OBJECT_SELF, \"NW_ASSOCIATE_MASTER\") | "
          "0x08000000); ClearAllActions();",
          self, "the familiar's stay");
      break;
    case rules::FamiliarOrder::kFollow:
      Run("SetLocalInt(OBJECT_SELF, \"NW_ASSOCIATE_MASTER\", GetLocalInt(OBJECT_SELF, \"NW_ASSOCIATE_MASTER\") & "
          "~0x08000000); ClearAllActions(); ActionForceFollowObject(GetMaster(OBJECT_SELF), 2.0);",
          self, "the familiar's follow");
      break;
    default: break;
  }
}

// Its own casts (the blueprint's gaze): refused - it can't attack.
bool FamiliarCastGuard(void* creature, const SpellOrder& order) {
  if (g_forms.empty() || !creature || order.fake || !g_forms.count(ObjId(creature))) return false;
  if (LogSlot()) Log("familiar: 0x%08X a familiar's cast refused (spell %u)", ObjId(creature), order.spell);
  return true;
}

// The damage it takes, logged (0.99.49: L4-36 - an orc's crossbow hits and no damage).
void FamiliarDamaged(uint32_t creator, void* target, int damage) {
  if (g_forms.empty() || !target || !g_forms.count(ObjId(target))) return;
  if (LogSlot()) {
    Log("familiar: 0x%08X takes %d damage from 0x%08X (%d of %d hit points before)", ObjId(target), damage, creator,
        static_cast<int>(Field<int16_t>(target, ee::kObjectCurrentHitPoints)), g_forms[ObjId(target)].hit_points);
  }
}

// Dismiss with a familiar here; Recall with one away.
bool FamiliarFeatGuard(void* creature, int feat) {
  if (feat != rules::kFeatFamiliarDismiss && feat != rules::kFeatFamiliarRecall) return true;
  const uint32_t id = ObjId(creature);
  const auto it = g_familiars.find(id);
  const bool away = it != g_familiars.end() && it->second.dismissed;
  const bool here = it != g_familiars.end() && !it->second.dismissed && CreatureById(it->second.creature);
  if (feat == rules::kFeatFamiliarDismiss && !here) {
    TellPlayer(id, PlayerText("familiar.none_here"));
    GrantFeat(creature, rules::kFeatFamiliarDismiss, false);
    return false;
  }
  if (feat == rules::kFeatFamiliarRecall && !away) {
    TellPlayer(id, PlayerText("familiar.none_away"));
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
  g_tend_logged.clear();
  g_heartbeats.clear();
  g_others_heard = 0;
}

}  // namespace

int FamiliarMaxHitPoints(void* creature) {
  if (g_forms.empty() || !creature) return 0;
  const auto it = g_forms.find(ObjId(creature));
  return it == g_forms.end() ? 0 : it->second.hit_points;
}

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

void FindFamiliarSpell(uint32_t self, void* caster, int row) {
  if (row == rules::kFamiliarDismiss) return Dismiss(self, caster);
  if (row == rules::kFamiliarRecall) return Recall(self, caster);
  const rules::FamiliarForm form = rules::FamiliarFormOf(row);
  if (form == rules::FamiliarForm::kNone) {
    TellPlayer(self, PlayerText("familiar.pick_form"));
    return;
  }
  Familiar& f = g_familiars[self];
  f.form = form;
  f.kind = rules::FamiliarKindFor(rules::FamiliarKindOfRow(row), GoodEvilOf(self, caster));  // 0.99.49: the menu's
  // One familiar: one of this spell's out takes the new form (away, it changes there).
  if (f.dismissed) {
    TellPlayer(self, PlayerText("familiar.becomes_away", {{"name", FamiliarName(f)}}));
    return;
  }
  if (f.creature && CreatureById(f.creature) && !engine::IsDeadOrDying(CreatureById(f.creature))) {
    Remake(f.creature, f);
    TellPlayer(self, PlayerText("familiar.becomes", {{"name", FamiliarName(f)}}));
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
    TellPlayer(self, PlayerText("familiar.failed"));
    Log("familiar: 0x%08X SummonFamiliar gave no familiar", self);
    return;
  }
  f.creature = familiar;
  f.dismissed = false;
  Remake(familiar, f);
  GrantFeat(caster, rules::kFeatFamiliarDismiss, true);
  GrantFeat(caster, rules::kFeatFamiliarRecall, false);
  TellPlayer(self, PlayerText("familiar.serves", {{"name", FamiliarName(f)}}));
  if (LogSlot()) {
    const rules::FamiliarStats& s = rules::FamiliarStatsOf(form);
    Log("familiar: 0x%08X Find Familiar - 0x%08X a %s: AC %d, %d HP, appearance %d at %.2f", self, familiar,
        FamiliarName(f).c_str(), s.armor_class, s.hit_points, s.appearance, s.scale);
  }
}

bool InstallFindFamiliar() {
  const bool waits = AddScriptHandler("n5_f_summon", &FinishSummon) && AddScriptHandler("n5_f_hb", &FamiliarHeartbeat) &&
                     AddScriptHandler("n5_f_talk", &FamiliarTalk) &&
                     AddSpellOrderGuard(&FamiliarCastGuard) && AddDamageListener(&FamiliarDamaged);
  const bool max_hp = g_config.hit_points == rules::RuleVariant::kFaithful ||  // rules_hit_points.cpp's hook asks
                      HookExport("CNWSCreature::GetMaxHitPoints (Find Familiar's forms, #39 4c)", ee::kGetMaxHitPoints,
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
