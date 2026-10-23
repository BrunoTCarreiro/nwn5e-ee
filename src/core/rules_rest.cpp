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
//   every Hit Die comes back; with rest_clock the clock moves 8 hours.
// - The Short Rest (a HUD button next to Rest; `!shortrest` too): instant,
//   out of combat - no enemy that can see the character within 30 m, no
//   damage taken or spell cast in the last 10 s (the engine's "exerted" timer,
//   +0x574, which also stops NWN's Rest) - spending at least one Hit Die (as
//   many as reach the maximum, the biggest first), and Second Wind one use
//   back. The party's henchmen rest with the player. With rest_clock the
//   clock moves an hour.
// NWN mode: NWN's rest, no Hit Dice, no Short Rest.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
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
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"
#include "rules/hit_dice.h"

namespace {

constexpr const char* kVarHitDiceSpent = "nwn5e_hit_dice_spent";
constexpr float kShortRestEnemyRange = 30.0f;  // the Rest button's REST_ENEMY_CHECK_DISTANCE
constexpr uint32_t kNoGradualRestore = 0xffffffffu;
constexpr int kAbilityConstitution = 2;

using RestFn = void (*)(void* creature, int associate, int enemy_line_of_sight);
using FeatUsesFn = void (*)(void* stats, uint16_t feat);
using AssociateIdFn = uint32_t (*)(void* creature, uint16_t type, int nth);
using MaxHitPointsFn = int16_t (*)(void* creature, int include_temporary);

RestFn g_rest = nullptr;
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

void Rest(void* creature, int associate, int enemy_line_of_sight) {
  g_rest(creature, associate, enemy_line_of_sight);
  if (!creature || !(Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting)) return;
  // Begun: no HP or spell levels along the way - all at the end, or nothing.
  SetField<uint32_t>(creature, ee::kCreatureRestHpRate, kNoGradualRestore);
  SetField<uint32_t>(creature, ee::kCreatureRestSpellRate, kNoGradualRestore);
}

void MoveClock(uint32_t creature_id, int hours) {
  char code[256];
  sprintf_s(code,
            "DelayCommand(0.0, SetTime(GetTimeHour() + %d, GetTimeMinute(), GetTimeSecond(), GetTimeMillisecond()));",
            hours);
  RunScriptChunk(code, creature_id);
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
  if (engine::IsDeadOrDying(creature)) return "not while down";
  if (Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting) return "already resting";
  if (Field<int32_t>(creature, ee::kCreatureExcitedTimer) > 0) {
    return "not so soon after fighting (damage taken or a spell cast in the last 10 seconds)";
  }
  EnemySearch search{creature, IdOf(creature), 0};
  engine::ForEachOtherCreatureInArea(creature, &LookForEnemy, &search);
  if (search.found) return "an enemy can see you";
  return "";
}

// One creature's Short Rest: its Hit Dice, Second Wind. The line for the player.
std::string ShortRestOf(void* creature, const std::string& who) {
  void* stats = StatsOf(creature);
  if (!stats) return "";
  const rules::HitDice all = rules::HitDiceOf(ClassLevelsOf(stats));
  const rules::HitDice spent = SpentHitDice(creature);
  const rules::HitDice left = rules::Remaining(all, spent);
  if (left.Total() == 0) return who + ": no Hit Dice left - a Long Rest gives them back.";
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
      features = " Second Wind: a use back.";
      break;
    }
  }
  Log("rests: 0x%08X Short Rest - %s (Con %+d): +%d HP; Hit Dice left %s", IdOf(creature), rolls.c_str(),
      engine::AbilityModifier(stats, kAbilityConstitution), rest.healed,
      DiceText(rules::Remaining(all, now_spent)).c_str());
  return who + ": " + rolls + " - +" + std::to_string(rest.healed) + " HP." + features +
         " Hit Dice left: " + DiceText(rules::Remaining(all, now_spent)) + ".";
}

}  // namespace

// The Short Rest of `player` and its henchmen (the HUD button, !shortrest).
std::vector<std::string> TakeShortRest(void* player) {
  if (!Faithful() || !player) return {"nwn5e: the Short Rest comes with rests faithful."};
  const std::string why = CantRest(player);
  if (!why.empty()) return {"nwn5e: no Short Rest - " + why + "."};
  std::vector<std::string> lines = {"nwn5e: Short Rest."};
  lines.push_back(ShortRestOf(player, "You"));
  for (int nth = 1; g_associate_id && nth <= 8; ++nth) {
    const uint32_t id = g_associate_id(player, ee::kAssociateTypeHenchman, nth);
    if (!id || id == ee::kObjectInvalid) break;
    void* henchman = CreatureById(id);
    if (!henchman || !CantRest(henchman).empty()) continue;
    lines.push_back(ShortRestOf(henchman, "Henchman"));
  }
  if (g_config.rest_clock) MoveClock(IdOf(player), 1);
  return lines;
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

// !rest: the Hit Dice left.
std::vector<std::string> RestCommand(void* creature, const std::string&) {
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const rules::HitDice all = rules::HitDiceOf(ClassLevelsOf(stats));
  return {"nwn5e: Hit Dice " + DiceText(rules::Remaining(all, SpentHitDice(creature))) + " left of " + DiceText(all) +
          ". A Short Rest (the button next to Rest, or !shortrest) spends at least one, out of combat; a Long Rest "
          "(Rest) gives them all back."};
}

}  // namespace

void RequestShortRest() { g_short_rest_requested = true; }

void InstallRestRules() {
  if (!Faithful()) return;
  RegisterRecords({kVarHitDiceSpent});
  g_feat_use_back = reinterpret_cast<FeatUsesFn>(EngineExport(ee::kIncrementFeatRemainingUses));
  g_associate_id = reinterpret_cast<AssociateIdFn>(EngineExport(ee::kGetAssociateId));
  g_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  if (!HookExport("CNWSCreature::Rest (rests: all or nothing)", ee::kCreatureRest, reinterpret_cast<void*>(&Rest),
                  reinterpret_cast<void**>(&g_rest))) {
    Log("SKIP rests: CNWSCreature::Rest not hooked - NWN's rest stays as it is");
  }
  if (!AddRestListener(&AfterLongRest)) Log("SKIP rests: the end of a rest is not hooked - no Hit Dice back");
  AddChatCommand("!shortrest", &ShortRestCommand);
  AddCreatureUpdateListener(&ShortRestRequested);
  AddChatCommand("!rest", &RestCommand);
  Log("rests: Long Rest = NWN's Rest (all or nothing, Hit Dice back), Short Rest = Hit Dice (rests faithful%s)",
      g_config.rest_clock ? ", the clock moves" : "");
}
