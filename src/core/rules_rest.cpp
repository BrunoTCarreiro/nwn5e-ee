// Phase 5a (docs/PLAN.md "Phase 5 — rests, healing, death", decided by the
// owner 2026-10-23), `rests` faithful:
// - Hit Dice (rules/rest.h): one per level, the class's die; the dice a
//   character has spent are the record nwn5e_hit_dice_spent (packed by size);
//   a Long Rest gives them all back.
// - The Long Rest is NWN's Rest as it is (no enemies within 30 m, no limit).
//   CNWSCreature::Rest is hooked so that, once a rest has started, its gradual
//   restore is off: the ms per HP and per spell level AIActionRest restores
//   at (+0x854 / +0x85C, which it computes only when 0) are set past any rest's
//   length - an interrupted rest gives nothing, the end still restores
//   everything. At its end (the rest listener, after ResetFeatRemainingUses)
//   every Hit Die comes back; with rest_clock (on by default since 0.59.18,
//   #110) the clock moves 8 hours.
//   A Long Rest also removes every level of Exhaustion (rules_dying.cpp).
// - #135 (owner, 2026-11-06: "Also impose the 16 hour limit but also make it so the user can remove this limit as
//   part of the house rules we talked about earlier"): SRD 5.2 - "After you finish a Long Rest, you must wait at
//   least 16 hours before starting another one." Each Long Rest's end is recorded (nwn5e_long_rest_end, the game
//   hour: rules::GameHourOf, + the 8 hours rest_clock moves), and a rest's start is refused until 16 have passed
//   (rest.h's gate; the player told how many are left; !rest says it too). [house_rules] long_rest_any_time lifts it.
//   #135 b (owner, 2026-11-06: "1 yes, 2 keep, 3 a+b, 4 B"): "To start a Long Rest, you must have at least 1 Hit
//   Point" - a creature that is down (dying, or a henchman lying down until healed) doesn't start one; a downed
//   henchman's master is told to heal it first, and the player's own rest goes ahead.
// - The Short Rest (a HUD button next to Rest; `!shortrest` too): instant,
//   out of combat - no enemy that can see the character within 30 m, no
//   damage taken or spell cast in the last 10 s (the engine's "exerted" timer,
//   +0x574, which also stops NWN's Rest) - spending at least one Hit Die (as
//   many as reach the maximum, the biggest first), and Second Wind one use
//   back. The party's henchmen rest with the player. With rest_clock the
//   clock moves an hour.
// NWN mode: NWN's rest, no Hit Dice, no Short Rest.

#include <windows.h>

#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature_update.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rest.h"
#include "player_text.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"
#include "rules/hit_dice.h"
#include "rules/spell_time.h"

int ExhaustionLevel(void* creature);  // rules_dying.cpp (Phase 5b)
bool IsDownForRest(void* creature);   // rules_dying.cpp (#135 b)
void EndConcentrationOnRest(void* creature);  // rules_concentration.cpp (#39: a rest outlasts it)

namespace {

constexpr const char* kVarHitDiceSpent = "nwn5e_hit_dice_spent";
constexpr const char* kVarLongRestEnd = "nwn5e_long_rest_end";  // #135: the game hour the last Long Rest ended
constexpr float kShortRestEnemyRange = 30.0f;  // the Rest button's REST_ENEMY_CHECK_DISTANCE
constexpr uint32_t kNoGradualRestore = 0xffffffffu;
constexpr int kAbilityConstitution = 2;

using FeatUsesFn = void (*)(void* stats, uint16_t feat);
using AssociateIdFn = uint32_t (*)(void* creature, uint16_t type, int nth);
using MaxHitPointsFn = int16_t (*)(void* creature, int include_temporary);

FeatUsesFn g_feat_use_back = nullptr;
AssociateIdFn g_associate_id = nullptr;
MaxHitPointsFn g_max_hit_points = nullptr;

bool Faithful() { return g_config.rests == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

std::vector<rules::ClassLevels> ClassLevelsOf(void* stats) {
  std::vector<rules::ClassLevels> classes;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (const auto c = rules::ClassFromNwnId(slot.nwn_class_id)) classes.push_back({*c, slot.levels});
  }
  return classes;
}

rules::HitDice SpentHitDice(void* creature) {
  EnsureRecords(creature);  // C6: an exported character's, from its database
  return rules::UnpackHitDice(engine::GetLocalInt(creature, kVarHitDiceSpent));
}

std::string DiceText(const rules::HitDice& dice) {
  std::string text;
  for (int i = rules::kHitDieSizes - 1; i >= 0; --i) {
    if (!dice.count[i]) continue;
    text += (text.empty() ? "" : " + ") + std::to_string(dice.count[i]) + "d" + std::to_string(rules::HitDieSides(i));
  }
  return text.empty() ? "none" : text;
}

int CurrentHitPoints(void* creature) { return Field<int16_t>(creature, ee::kObjectCurrentHitPoints); }

// --- The Long Rest ----------------------------------------------------------------

// A rest has begun (rest.h): no HP or spell levels along the way - all at the end, or nothing.
std::unordered_map<uint32_t, ULONGLONG> g_rest_begun;  // a rest's start (CNWSCreature::Rest), by creature
constexpr ULONGLONG kRestEndWithinMs = 5 * 60 * 1000;  // the end hook's call a rest's end within this - not a load

void RestBegun(void* creature) {
  g_rest_begun[IdOf(creature)] = GetTickCount64();
  SetField<uint32_t>(creature, ee::kCreatureRestHpRate, kNoGradualRestore);
  SetField<uint32_t>(creature, ee::kCreatureRestSpellRate, kNoGradualRestore);
}

// #39 (0.99.79; owner, 2026-11-18, on DU-1 and L4-53: "the spell should only end after the full long rest is
// completed. right now the spell ends a little after the long rest starts"): AIActionRest, as a rest begins, calls
// CNWSObject::ClearSpellEffectsOnOthers - every spell effect the resting creature cast goes, on itself and on others -
// so 0.99.67's "a rest ends what it outlasts" found nothing left at the rest's end ("outlasts 0 spell effect(s)").
// Faithful (rules::RestClearsSpellsAtStart), that one call is skipped: the rest's end (AfterLongRest ->
// RestOutlastsSpells) ends what the rest outlasts, and a rest cancelled ends nothing. Other callers are NWN's.
using ClearSpellEffectsFn = void (*)(void* object);
constexpr const char* kClearSpellEffectsOnOthers = "?ClearSpellEffectsOnOthers@CNWSObject@@QEAAXXZ";
constexpr const char* kAIActionRest = "?AIActionRest@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
ClearSpellEffectsFn g_clear_spell_effects = nullptr;
uintptr_t g_rest_clear_site = 0;  // AIActionRest's call (its return address)

void ClearSpellEffectsOnOthers(void* object) {
  if (g_rest_clear_site && reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_rest_clear_site) {
    Log("rests: 0x%08X a rest begins - its spells kept (NWN's clearing skipped; the rest's end ends what it outlasts)",
        IdOf(object));
    return;
  }
  g_clear_spell_effects(object);
}

void KeepSpellsAtRestStart() {
  if (rules::RestClearsSpellsAtStart(g_config.spellcasting == rules::RuleVariant::kFaithful)) return;
  const uintptr_t rest_action = EngineExport(kAIActionRest);
  const uintptr_t clear = EngineExport(kClearSpellEffectsOnOthers);
  const std::vector<uintptr_t> calls =
      rest_action && clear ? FindCallsTo(FunctionAt(rest_action), clear) : std::vector<uintptr_t>();
  if (calls.size() != 1) {
    Log("SKIP rests #39: AIActionRest's clearing of spells not found (%zu calls) - a rest clears them as it begins",
        calls.size());
    return;
  }
  g_rest_clear_site = calls[0] + 5;
  HookExport("CNWSObject::ClearSpellEffectsOnOthers (a rest keeps its spells until its end, #39)",
             kClearSpellEffectsOnOthers, reinterpret_cast<void*>(&ClearSpellEffectsOnOthers),
             reinterpret_cast<void**>(&g_clear_spell_effects));
}

// #135: the game hour now (the calendar's, by the script bridge).
int GameHourNow(void* creature) {
  RunScriptChunk("object p = OBJECT_SELF; SetLocalInt(p, \"nwn5e_now_y\", GetCalendarYear());"
                 " SetLocalInt(p, \"nwn5e_now_m\", GetCalendarMonth()); SetLocalInt(p, \"nwn5e_now_d\", GetCalendarDay());"
                 " SetLocalInt(p, \"nwn5e_now_h\", GetTimeHour());",
                 IdOf(creature));
  return rules::GameHourOf(engine::GetLocalInt(creature, "nwn5e_now_y"), engine::GetLocalInt(creature, "nwn5e_now_m"),
                           engine::GetLocalInt(creature, "nwn5e_now_d"), engine::GetLocalInt(creature, "nwn5e_now_h"));
}

// #135: the hours before this creature may start a Long Rest (0: it may).
int LongRestWaitLeft(void* creature) {
  EnsureRecords(creature);
  const int last = engine::GetLocalInt(creature, kVarLongRestEnd);
  if (last <= 0) return 0;
  return rules::LongRestWaitLeft(Faithful(), g_config.long_rest_any_time, last, GameHourNow(creature));
}

std::string HoursText(int hours) {
  return PlayerText(hours == 1 ? "rest.hour" : "rest.hours", {{"hours", std::to_string(hours)}});
}

// #135: the gate on a rest's start (rest.h) - b: not while down; a: the 16-hour wait.
std::string LongRestGate(void* creature) {
  if (rules::DownBlocksLongRest(Faithful(), engine::IsDeadOrDying(creature) || IsDownForRest(creature))) {
    Log("rests: 0x%08X Long Rest refused - down (a Long Rest needs at least 1 hit point; #135)", IdOf(creature));
    if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) {
      RunScriptChunk("object m = GetMaster(OBJECT_SELF); if (GetIsPC(m)) SendMessageToPC(m, \"nwn5e: \" + "
                     "GetName(OBJECT_SELF) + \" is down and doesn't rest - heal them first (a spell, a healer's kit or "
                     "a Short Rest), then rest.\");",
                     IdOf(creature));
    }
    return PlayerText("rest.long_rest_down");
  }
  const int left = LongRestWaitLeft(creature);
  if (left <= 0) return "";
  Log("rests: 0x%08X Long Rest refused - %d of the 16 hours after the last one still to pass (#135)", IdOf(creature),
      left);
  return PlayerText("rest.long_rest_wait", {{"hours", HoursText(left)}});
}

void MoveClock(uint32_t creature_id, int hours) {
  char code[256];
  sprintf_s(code,
            "DelayCommand(0.0, SetTime(GetTimeHour() + %d, GetTimeMinute(), GetTimeSecond(), GetTimeMillisecond()));",
            hours);
  RunScriptChunk(code, creature_id);
}

// #39 (owner, 2026-11-17: "go with A. no real-time clock"): with spellcasting faithful a spell's duration is real time
// (rules::SpellSeconds), so a rest is the time it says and ends what it outlasts - every spell's effect in the area
// with that long or less left; nwn5e's spell areas it outlasts (rules::RestEndsSpellArea, decided below);
// Concentration. The rester's spell clock (Goodberry's 24 hours) moves on; the berries past it go.
const char* const kRestOutlasts = R"nss(
object a = GetArea(OBJECT_SELF);
int n = 0; string areas = "";
object o = GetFirstObjectInArea(a);
while (GetIsObjectValid(o)) {
  int t = GetObjectType(o);
  if (t == OBJECT_TYPE_CREATURE) {
    effect e = GetFirstEffect(o);
    while (GetIsEffectValid(e)) {
      if (GetEffectDurationType(e) == DURATION_TYPE_TEMPORARY && GetEffectSpellId(e) >= 0 &&
          GetEffectDurationRemaining(e) <= $REST) { RemoveEffect(o, e); n++; }
      e = GetNextEffect(o);
    }
  } else if (t == OBJECT_TYPE_AREA_OF_EFFECT) {
    areas += ObjectToString(o) + " " + GetTag(o) + ";";
  }
  o = GetNextObjectInArea(a);
}
int clock = GetLocalInt(OBJECT_SELF, "n5_spell_clock") + $REST;
SetLocalInt(OBJECT_SELF, "n5_spell_clock", clock);
int berries = 0;
object i = GetFirstItemInInventory(OBJECT_SELF);
while (GetIsObjectValid(i)) {
  int until = GetLocalInt(i, "n5_goodberry_until");
  if (until > 0 && clock >= until) { DestroyObject(i); berries++; }
  i = GetNextItemInInventory(OBJECT_SELF);
}
SetLocalInt(OBJECT_SELF, "n5_rest_effects", n); SetLocalInt(OBJECT_SELF, "n5_rest_berries", berries);
SetLocalString(OBJECT_SELF, "n5_rest_areas", areas);
)nss";

void RestOutlastsSpells(void* creature, int rest_seconds, bool long_rest) {
  if (!creature || g_config.spellcasting != rules::RuleVariant::kFaithful) return;
  std::string code = kRestOutlasts;
  for (size_t at = code.find("$REST"); at != std::string::npos; at = code.find("$REST", at)) {
    code.replace(at, 5, std::to_string(rest_seconds));
  }
  const uint32_t id = IdOf(creature);
  RunScriptChunk(code.c_str(), id);
  int areas_ended = 0;
  std::istringstream areas(engine::GetLocalString(creature, "n5_rest_areas"));
  std::string one;
  while (std::getline(areas, one, ';')) {
    const size_t space = one.find(' ');
    if (space == std::string::npos || !rules::RestEndsSpellArea(one.substr(space + 1), long_rest)) continue;
    const uint32_t area = static_cast<uint32_t>(strtoul(one.substr(0, space).c_str(), nullptr, 16));
    RunScriptChunk("DestroyObject(OBJECT_SELF);", area);
    ++areas_ended;
  }
  EndConcentrationOnRest(creature);
  Log("rests: 0x%08X the %s Rest outlasts %d spell effect(s), %d spell area(s), %d Goodberry stack(s); spell "
      "clock %d s",
      id, long_rest ? "Long" : "Short", engine::GetLocalInt(creature, "n5_rest_effects"), areas_ended,
      engine::GetLocalInt(creature, "n5_rest_berries"), engine::GetLocalInt(creature, "n5_spell_clock"));
}

// The spell clock (n5_spell_clock, seconds; a player character's, saved with it): real time played - a pause or a
// load not counted - and the rests' hours (RestOutlastsSpells). Goodberry's berries last 24 hours of it.
std::unordered_map<uint32_t, ULONGLONG> g_spell_clock_at;
void SpellClockUpdate(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const ULONGLONG now = GetTickCount64();
  ULONGLONG& at = g_spell_clock_at[IdOf(creature)];
  if (at == 0) at = now;
  ULONGLONG elapsed = now - at;
  if (elapsed < 1000) return;
  if (elapsed > 2000) {  // no update for a while: paused, loading - one second
    at = now;
    elapsed = 1000;
  } else {
    at += (elapsed / 1000) * 1000;
  }
  engine::SetLocalInt(creature, "n5_spell_clock",
                      engine::GetLocalInt(creature, "n5_spell_clock") + static_cast<int>(elapsed / 1000));
}

void AfterLongRest(void* stats) {
  void* creature = CreatureOf(stats);
  if (!creature) return;
  const rules::HitDice spent = SpentHitDice(creature);
  if (spent.Total() > 0) {
    SetRecord(creature, kVarHitDiceSpent, 0);
    Log("rests: 0x%08X Long Rest - %s Hit Dice back", IdOf(creature), DiceText(spent).c_str());
  }
  if (g_config.rest_clock && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0) {
    MoveClock(IdOf(creature), 8);
  }
  // #135: when it ended - after the 8 hours rest_clock moves (just queued) when it's on.
  SetRecord(creature, kVarLongRestEnd, GameHourNow(creature) + (g_config.rest_clock ? 8 : 0));
  // #39: a rest that began a moment ago (not a load, nor !test's ForceRest) outlasts spells - the player
  // character's, for its area
  const auto begun = g_rest_begun.find(IdOf(creature));
  if (begun != g_rest_begun.end()) {
    const bool rested = GetTickCount64() - begun->second < kRestEndWithinMs;
    g_rest_begun.erase(begun);
    if (rested && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0) {
      RestOutlastsSpells(creature, rules::kLongRestSeconds, true);
    }
  }
}

// --- The Short Rest ---------------------------------------------------------------

struct EnemySearch {
  void* self;
  uint32_t self_id;
  uint32_t found;
};

void LookForEnemy(void* context, uint32_t id, void* other) {
  auto* search = static_cast<EnemySearch*>(context);
  if (search->found || !other || engine::IsDeadOrDying(other)) return;
  if (!engine::IsEnemy(search->self, id)) return;
  if (engine::EdgeDistance(search->self, other) > kShortRestEnemyRange) return;
  if (engine::Sees(other, search->self_id)) search->found = id;
}

// Why the creature can't rest now ("" when it can).
std::string CantRest(void* creature) {
  if (engine::IsDeadOrDying(creature)) return PlayerText("rest.cant_down");
  if (Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting) return PlayerText("rest.cant_resting");
  if (Field<int32_t>(creature, ee::kCreatureExcitedTimer) > 0) {
    return PlayerText("rest.cant_fighting");
  }
  EnemySearch search{creature, IdOf(creature), 0};
  engine::ForEachOtherCreatureInArea(creature, &LookForEnemy, &search);
  if (search.found) return PlayerText("rest.cant_enemy");
  return "";
}

// One creature's Short Rest: its Hit Dice, Second Wind. The line for the player.
std::string ShortRestOf(void* creature, const std::string& who) {
  void* stats = StatsOf(creature);
  if (!stats) return "";
  const rules::HitDice all = rules::HitDiceOf(ClassLevelsOf(stats));
  const rules::HitDice spent = SpentHitDice(creature);
  const rules::HitDice left = rules::Remaining(all, spent);
  if (left.Total() == 0) return PlayerText("rest.no_hit_dice", {{"who", who}});
  const int max_hp = g_max_hit_points ? g_max_hit_points(creature, 1) : CurrentHitPoints(creature);
  const rules::ShortRest rest =
      rules::ShortRestDice(left, CurrentHitPoints(creature), max_hp, engine::AbilityModifier(stats, kAbilityConstitution),
                           [](int sides) { return RollEngineDice(1, sides); });
  rules::HitDice now_spent = spent;
  for (int i = 0; i < rules::kHitDieSizes; ++i) now_spent.count[i] += rest.spent.count[i];
  SetRecord(creature, kVarHitDiceSpent, rules::PackHitDice(now_spent));
  if (rest.healed > 0) {
    ApplyEffectAs(IdOf(creature), IdOf(creature), "DURATION_TYPE_INSTANT",
                  ("EffectHeal(" + std::to_string(rest.healed) + ")").c_str(), 0.0f, 0);
  }
  if (g_config.short_rest_vfx > 0) {  // the potion's look (short_rest_vfx, _scale)
    char visual[96];
    sprintf_s(visual, "EffectVisualEffect(%d, FALSE, %.2f)", g_config.short_rest_vfx,
              g_config.short_rest_vfx_scale / 100.0);
    ApplyEffectAs(IdOf(creature), IdOf(creature), "DURATION_TYPE_INSTANT", visual, 0.0f, 0);
  }
  std::string rolls;
  for (size_t i = 0; i < rest.sides.size(); ++i) {
    rolls += (i ? ", " : "") + std::string("d") + std::to_string(rest.sides[i]) + " " + std::to_string(rest.rolls[i]);
  }
  // Short-rest features: Second Wind one use back.
  std::string features;
  for (int feat = rules::kFeatSecondWind2; feat <= rules::kFeatSecondWind4; ++feat) {
    if (engine::HasFeat(stats, feat) && g_feat_use_back) {
      g_feat_use_back(stats, static_cast<uint16_t>(feat));
      features = " " + PlayerText("rest.second_wind_back");
      break;
    }
  }
  features += AfterShortRest(creature);  // X6: the other short-rest features (Rage)
  Log("rests: 0x%08X Short Rest - %s (Con %+d): +%d HP; Hit Dice left %s", IdOf(creature), rolls.c_str(),
      engine::AbilityModifier(stats, kAbilityConstitution), rest.healed,
      DiceText(rules::Remaining(all, now_spent)).c_str());
  return PlayerText("rest.short_rest_of", {{"who", who}, {"rolls", rolls}, {"healed", std::to_string(rest.healed)},
                                          {"features", features},
                                          {"left", DiceText(rules::Remaining(all, now_spent))}});
}

}  // namespace

// The Short Rest of `player` and its henchmen (the HUD button, !shortrest).
std::vector<std::string> TakeShortRest(void* player) {
  if (!Faithful() || !player) return {PlayerText("rest.the_short_rest_comes_with")};
  const std::string why = CantRest(player);
  if (!why.empty()) return {PlayerText("rest.no_short_rest", {{"why", why}})};
  std::vector<std::string> lines = {PlayerText("rest.short_rest")};
  lines.push_back(ShortRestOf(player, PlayerText("rest.who_you")));
  for (int nth = 1; g_associate_id && nth <= 8; ++nth) {
    const uint32_t id = g_associate_id(player, ee::kAssociateTypeHenchman, nth);
    if (!id || id == ee::kObjectInvalid) break;
    void* henchman = CreatureById(id);
    if (!henchman || !CantRest(henchman).empty()) continue;
    lines.push_back(ShortRestOf(henchman, PlayerText("rest.who_henchman")));
  }
  if (g_config.rest_clock) MoveClock(IdOf(player), 1);
  RestOutlastsSpells(player, rules::kShortRestSeconds, false);  // #39: the hour outlasts spells
  return lines;
}

// #39 Part 2, level 2, build 1a: Prayer of Healing - "the benefits of a Short Rest" for one creature (its Hit Dice,
// the short-rest features), no hour passing and no rest's checks (the spell's own 10 minutes: rules_spells2024.cpp).
// `who` names it in the line; "" with rests in NWN mode (no Hit Dice).
std::string PrayerShortRest(void* creature, const std::string& who) {
  if (!Faithful() || !creature) return "";
  return ShortRestOf(creature, who);
}

namespace {

std::vector<std::string> ShortRestCommand(void* creature, const std::string&) { return TakeShortRest(creature); }

// The HUD button's click (hud_rest.cpp, the client's frame) is taken at the
// player character's next update (the server's), as its own line in chat.
bool g_short_rest_requested = false;

void ShortRestRequested(void* creature) {
  if (!g_short_rest_requested || !creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  g_short_rest_requested = false;
  for (const std::string& line : TakeShortRest(creature)) TellPlayer(IdOf(creature), line);
}

// !rest: the Hit Dice left, and Exhaustion (Phase 5b).
std::vector<std::string> RestCommand(void* creature, const std::string&) {
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const rules::HitDice all = rules::HitDiceOf(ClassLevelsOf(stats));
  std::vector<std::string> lines = {
      PlayerText("rest.hit_dice_left_of_a",
                 {{"dice_text", DiceText(rules::Remaining(all, SpentHitDice(creature)))},
                  {"dice_text2", DiceText(all)}})};
  if (const int exhaustion = ExhaustionLevel(creature)) {
    lines.push_back(PlayerText("rest.exhaustion_a_long_rest_removes", {{"exhaustion", std::to_string(exhaustion)}}));
  }
  const int wait = LongRestWaitLeft(creature);  // #135
  lines.push_back(wait > 0 ? PlayerText("rest.the_next_long_rest_in", {{"hours_text", HoursText(wait)}})
                           : PlayerText(g_config.long_rest_any_time ? "rest.long_rest_any_time"
                                                                    : "rest.long_rest_now"));
  return lines;
}

}  // namespace

void RequestShortRest() { g_short_rest_requested = true; }

// #135: a test's rest (a !test profile's ForceRest) doesn't start the 16-hour wait.
void ClearLongRestWait(void* creature) {
  if (creature && Faithful()) SetRecord(creature, kVarLongRestEnd, 0);
}

void InstallRestRules() {
  if (!Faithful()) return;
  RegisterRecords({kVarHitDiceSpent, kVarLongRestEnd});
  g_feat_use_back = reinterpret_cast<FeatUsesFn>(EngineExport(ee::kIncrementFeatRemainingUses));
  g_associate_id = reinterpret_cast<AssociateIdFn>(EngineExport(ee::kGetAssociateId));
  g_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  if (!AddRestStartListener(&RestBegun)) Log("SKIP rests: CNWSCreature::Rest not hooked - NWN's rest stays as it is");
  if (!AddRestListener(&AfterLongRest)) Log("SKIP rests: the end of a rest is not hooked - no Hit Dice back");
  if (!AddRestGate(&LongRestGate)) Log("SKIP rests #135: a rest's start isn't gated - no 16-hour wait");
  KeepSpellsAtRestStart();  // #39 (0.99.79)
  AddChatCommand("!shortrest", &ShortRestCommand);
  AddCreatureUpdateListener(&ShortRestRequested);
  if (g_config.spellcasting == rules::RuleVariant::kFaithful) AddCreatureUpdateListener(&SpellClockUpdate);
  AddChatCommand("!rest", &RestCommand);
  Log("rests: Long Rest = NWN's Rest (all or nothing, Hit Dice back; 16 hours between them%s), Short Rest = Hit Dice "
      "(rests faithful%s)",
      g_config.long_rest_any_time ? " - lifted, house rule long_rest_any_time" : "",
      g_config.rest_clock ? ", the clock moves" : "");
}
