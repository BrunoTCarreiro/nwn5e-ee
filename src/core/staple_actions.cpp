// Phase 6 (#56; docs/PLAN.md "Phase 6 — Action economy"; the owner, 2026-11-02: "Let's go with B and have it off
// by default, but we'll have the possibility there anyway in case the player wants to add that house rule. And
// dodge, dash, no help for attacks and grenades in their own issue"), `actions` faithful - every player character's
// (rules::FeatsEveryoneHas2024, granted by rules_features.cpp; data/rows/feat.actions.txt, spells.actions.txt),
// each the round's Action (feat.2da ReqAction 1: NWN's cast path - mid-round it waits behind the round's attacks,
// action_order.cpp; then its cast round, no attacks):
// - Dodge (feat 1322, n5_dodge): until the next round attacks against it have Disadvantage (an attacker it sees) and
//   its Dexterity saves Advantage - the Monk's Patient Defense Dodge (rules_monk.cpp's MonkDodging asks Dodging),
//   not while Incapacitated. The radial's Special Attacks page, slot 0 (Parry's), and the quickbar.
// - Dash (feat 1323, n5_dash): its speed doubled for the round (a movement rate bonus: rules::DashMovementFactor -
//   not an effect, so the engine's speed cap doesn't hold it back; the grapple's drag still halves it). Slot 2
//   (Improved Power Attack's), and the quickbar.
// - Help (feat 1324, n5_help) - the house rule, off by default ([house_rules] help; not SRD 5.2: the 2024 Help
//   stabilizes a creature at 0 Hit Points, and nwn5e's downed henchmen make no death saves): on a downed henchman, a
//   DC 10 Wisdom (Medicine) check (NWN's Heal, the 2024 skill value) - success, it gets up at 1 HP (rules_dying.cpp);
//   failure, nothing. The quickbar (a target: the radial's page has no slot left).

#include "staple_actions.h"

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "movement_rate.h"
#include "script_hooks.h"
#include "rules/action_economy.h"
#include "rules/skills.h"

bool IsDownHenchmanUnderRules(uint32_t id);  // rules_dying.cpp
bool HelpDownHenchmanUp(uint32_t id);        // rules_dying.cpp

namespace {

constexpr uint64_t kRoundMs = 6000;
using SkillRankFn = int8_t (*)(void* stats, uint8_t skill, void* vs, int base_only);

std::unordered_map<uint32_t, uint64_t> g_dodging;  // by creature id: until (game ms); the engine's thread
std::unordered_map<uint32_t, uint64_t> g_dashing;

bool Until(std::unordered_map<uint32_t, uint64_t>& map, uint32_t id) {
  const auto it = map.find(id);
  if (it == map.end()) return false;
  if (GameMs() < it->second) return true;
  map.erase(it);
  return false;
}

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, uint32_t id, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(id, "nwn5e: " + line);
}

void DodgeScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  g_dodging[self] = GameMs() + kRoundMs;
  Log("actions: 0x%08X Dodge - attacks against it with Disadvantage, Dexterity saves with Advantage, until its next "
      "round", self);
  Tell(creature, self, "Dodge - until your next round, attacks against you have Disadvantage and your Dexterity "
                       "saves Advantage.");
}

void DashScript(uint32_t self) { StartDash(self); }

float DashBonus(void* creature, float factor) {
  if (g_dashing.empty() || !creature) return factor;
  return rules::DashMovementFactor(factor, Until(g_dashing, Field<uint32_t>(creature, ee::kObjectId)));
}

// Help's target: the spell's (GetSpellTargetObject, read by script - no field of it is known here).
uint32_t SpellTarget(uint32_t self) {
  RunScriptChunk("SetLocalString(OBJECT_SELF, \"nwn5e_help_target\", ObjectToString(GetSpellTargetObject()));", self);
  void* creature = CreatureById(self);
  const std::string text = creature ? engine::GetLocalString(creature, "nwn5e_help_target") : std::string();
  return text.empty() ? ee::kObjectInvalid : static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 16));
}

void HelpScript(uint32_t self) {
  void* creature = CreatureById(self);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return;
  const uint32_t target = SpellTarget(self);
  if (target == ee::kObjectInvalid || !IsDownHenchmanUnderRules(target)) {
    Log("actions: 0x%08X Help on 0x%08X - not a downed henchman: nothing", self, target);
    Tell(creature, self, "Help - only a downed henchman can be helped up (the house rule).");
    return;
  }
  static const auto skill_rank = reinterpret_cast<SkillRankFn>(EngineExport(ee::kGetSkillRank));
  const int medicine = skill_rank ? skill_rank(stats, static_cast<uint8_t>(rules::kNwnHeal), nullptr, 0) : 0;
  const int d20 = RollEngineDice(1, 20);
  const bool up = rules::HelpMedicineSucceeds(d20, medicine);
  char check[96];
  sprintf_s(check, "Medicine %+d: %d vs DC %d", medicine, d20 + medicine, rules::kHelpMedicineDc);
  if (up) HelpDownHenchmanUp(target);
  Log("actions: 0x%08X Help on the downed henchman 0x%08X (%s) - %s", self, target, check,
      up ? "it gets up at 1 HP" : "not this time");
  Tell(creature, self, std::string("Help (") + check + ") - " +
                           (up ? "your henchman gets up at 1 HP." : "not this time; try again next round."));
}

}  // namespace

bool Dodging(uint32_t id) { return !g_dodging.empty() && Until(g_dodging, id); }

void StartDash(uint32_t id) {
  void* creature = CreatureById(id);
  if (!creature) return;
  g_dashing[id] = GameMs() + kRoundMs;
  Log("actions: 0x%08X Dash - twice as fast this round", id);
  Tell(creature, id, "Dash - twice as fast this round.");
}

void InstallStapleActions() {
  if (g_config.actions != rules::RuleVariant::kFaithful) return;
  const bool scripts = AddScriptHandler("n5_dodge", &DodgeScript) && AddScriptHandler("n5_dash", &DashScript) &&
                       AddScriptHandler("n5_help", &HelpScript);
  const bool dash = AddMovementRateBonus(&DashBonus);
  Log("actions: staple actions - Dodge and Dash%s for every player character%s%s",
      g_config.help_house_rule ? ", and Help (the house rule: a downed henchman up)" : " (Help: the house rule is off)",
      scripts ? "" : " - NOT available (no script hook)", dash ? "" : "; Dash NOT faster (no movement rate hook)");
}
