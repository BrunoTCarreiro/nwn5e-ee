// Phase 3 Rogue part 6 (docs/PLAN.md "Phase 3 — Rogue", T1 and T4; owner,
// 2026-10-28: "go with the recommendations"), class_features faithful. The
// Thief - the SRD's one Rogue subclass, granted at Rogue 3:
// - T1 Fast Hands (Thief 3): lock and trap checks - NWN's Open Lock and Disable
//   Trap, 2024's Thieves' Tools - use the better of that and Sleight of Hand
//   (NWN's Pick Pocket), so Expertise in Sleight of Hand counts: a skill
//   adjuster after the 2024 skills' (rules_skills.cpp). The speed (a Bonus
//   Action) comes with Phase 6.
// - T4 Use Magic Device (Thief 13), scrolls first: CNWSCreature::
//   CheckUseMagicDeviceSkill(item, limit) - the engine's "does Use Magic Device
//   overcome this item's class / race / alignment limit", which nwn5e's 3e-only
//   skills answer "never" (SK8) - answers yes for a spell scroll (baseitems
//   75). When the Thief reads a scroll none of its classes could (item_use.cpp:
//   UseItem's guard), a spell of level 2 or more takes an Intelligence (Arcana)
//   check - d20 (Reliable Talent counted) + the 2024 Arcana value (NWN's
//   Spellcraft) - against 10 + the spell's level (spells.2da Innate); a
//   failure crumbles the scroll (one of the stack) and nothing is cast.
//   Cantrips and level 1 need no check. The scroll casts at its own caster
//   level and DC (2024 would use the Thief's Intelligence). Attunement (NWN
//   has none) and charges: later.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_set>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "item_use.h"
#include "log.h"
#include "skill_rank.h"
#include "rules/class_features.h"

int ReliableTalentRoll(void* creature, int nwn_skill, int d20);  // rules_skills.cpp

namespace {

using CheckUmdFn = int (*)(void* creature, void* item, int limit);

constexpr int kBaseItemSpellScroll = 75;
constexpr const char* kVarLift = "nwn5e_umd_lift";
constexpr const char* kVarLevel = "nwn5e_umd_level";

CheckUmdFn g_check_umd = nullptr;
std::unordered_set<uint64_t> g_logged;  // (creature, what) pairs already logged
int g_log_lines = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_log_lines < 300 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

int RogueLevelOf(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats ? ClassLevel2024(stats, rules::CharacterClass::kRogue) : 0;
}

void Tell(void* creature, const std::string& line) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (stats && ReadCreatureFacts(stats).is_player) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

// --- T4 Use Magic Device -------------------------------------------------------------

int CheckUseMagicDeviceSkill(void* creature, void* item, int limit) {
  if (Faithful() && creature && engine::BaseItemRow(item) == kBaseItemSpellScroll &&
      rules::HasUseMagicDevice(RogueLevelOf(creature))) {
    const uint32_t id = IdOf(creature);
    if (g_logged.insert((static_cast<uint64_t>(id) << 8) | 1).second && LogSlot()) {
      Log("thief: 0x%08X Use Magic Device - spell scrolls' limits lifted (Thief 13; limit kind %d)", id, limit);
    }
    return 1;
  }
  return g_check_umd(creature, item, limit);
}

// What the scroll asks of the Thief (read by script): whether only Use Magic
// Device lets it read the scroll (a class limit none of its classes meets),
// and the spell's level (-1 when none is found).
void ScrollFacts(void* creature, uint32_t item_id, bool* lifted, int* level) {
  char script[1100];
  sprintf_s(script,
            "object i = StringToObject(\"%x\"); int limited = 0; int own = 0; int lvl = -1;"
            " itemproperty p = GetFirstItemProperty(i);"
            " while (GetIsItemPropertyValid(p)) { int t = GetItemPropertyType(p);"
            " if (t == ITEM_PROPERTY_USE_LIMITATION_CLASS) { limited = 1;"
            " if (GetLevelByClass(GetItemPropertySubType(p), OBJECT_SELF) > 0) own = 1; }"
            " else if (t == ITEM_PROPERTY_CAST_SPELL && lvl < 0) {"
            " string s = Get2DAString(\"iprp_spells\", \"SpellIndex\", GetItemPropertySubType(p));"
            " if (s != \"\") lvl = StringToInt(Get2DAString(\"spells\", \"Innate\", StringToInt(s))); }"
            " p = GetNextItemProperty(i); }"
            " SetLocalInt(OBJECT_SELF, \"%s\", limited && !own); SetLocalInt(OBJECT_SELF, \"%s\", lvl);",
            item_id, kVarLift, kVarLevel);
  engine::SetLocalInt(creature, kVarLift, 0);
  engine::SetLocalInt(creature, kVarLevel, -1);
  RunScriptChunk(script, IdOf(creature));
  *lifted = engine::GetLocalInt(creature, kVarLift) != 0;
  *level = engine::GetLocalInt(creature, kVarLevel);
}

void Crumble(uint32_t creature_id, uint32_t item_id) {
  char script[256];
  sprintf_s(script,
            "object i = StringToObject(\"%x\"); int n = GetItemStackSize(i);"
            " if (n > 1) SetItemStackSize(i, n - 1); else DestroyObject(i);",
            item_id);
  RunScriptChunk(script, creature_id);
}

// UseItem's guard: a Thief 13 reading a scroll only Use Magic Device lets it read.
bool ScrollGuard(void* creature, uint32_t item_id, void* item) {
  if (!Faithful() || engine::BaseItemRow(item) != kBaseItemSpellScroll) return true;
  const int rogue = RogueLevelOf(creature);
  if (!rules::HasUseMagicDevice(rogue)) return true;
  bool lifted = false;
  int level = -1;
  ScrollFacts(creature, item_id, &lifted, &level);
  const uint32_t id = IdOf(creature);
  if (!lifted) return true;  // its own class's scroll, or one with no class limit
  if (!rules::ScrollNeedsArcanaCheck(level)) {
    if (LogSlot()) Log("thief: 0x%08X Use Magic Device - reads scroll 0x%08X (spell level %d: no check)", id, item_id,
                       level);
    Tell(creature, "Use Magic Device: you read the scroll (a spell of level " + std::to_string(level < 0 ? 0 : level) +
                       ": no check).");
    return true;
  }
  void* stats = StatsOf(creature);
  const int dc = rules::ScrollArcanaDc(level);
  const int rolled = RollEngineDice(1, 20);
  const int d20 = ReliableTalentRoll(creature, ee::kSkillSpellcraft, rolled);
  const int bonus = stats ? SkillRankNow(stats, ee::kSkillSpellcraft, nullptr) : 0;
  const int total = d20 + bonus;
  const bool success = total >= dc;
  char roll[96];
  sprintf_s(roll, "Arcana %d%s %+d = %d vs DC %d", rolled, d20 != rolled ? " (Reliable Talent: 10)" : "", bonus, total,
            dc);
  if (LogSlot()) Log("thief: 0x%08X Use Magic Device - scroll 0x%08X, spell level %d: %s - %s", id, item_id, level,
                     roll, success ? "read" : "it crumbles");
  Tell(creature, std::string("Use Magic Device: ") + roll +
                     (success ? " - you read the scroll." : " - the scroll crumbles to dust in your hands."));
  if (success) return true;
  Crumble(id, item_id);
  return false;
}

// --- T1 Fast Hands -------------------------------------------------------------------

int FastHands(const SkillQuery& query, int value) {
  if (query.base_only || (query.skill != ee::kSkillOpenLock && query.skill != ee::kSkillDisableTrap) || !Faithful()) {
    return value;
  }
  if (!rules::HasFastHands(ClassLevel2024(query.stats, rules::CharacterClass::kRogue))) return value;
  const int sleight = SkillRankNow(query.stats, ee::kSkillPickPocket, query.vs);
  const int best = rules::FastHandsValue(value, sleight);
  if (best != value) {
    const uint32_t id = ObjectIdOf(query.stats);
    if (g_logged.insert((static_cast<uint64_t>(id) << 8) | (2 + query.skill)).second && LogSlot()) {
      Log("thief: 0x%08X Fast Hands - %s %+d uses Sleight of Hand's %+d", id,
          query.skill == ee::kSkillOpenLock ? "Open Lock" : "Disable Trap", value, sleight);
    }
  }
  return best;
}

}  // namespace

void InstallThiefRules() {
  if (!Faithful()) return;
  const bool fast = AddSkillAdjuster(&FastHands);
  const bool lift = HookExport("CNWSCreature::CheckUseMagicDeviceSkill (Thief 13: spell scrolls)",
                               ee::kCheckUseMagicDeviceSkill, reinterpret_cast<void*>(&CheckUseMagicDeviceSkill),
                               reinterpret_cast<void**>(&g_check_umd));
  const bool guard = lift && AddItemUseGuard(&ScrollGuard);
  Log("thief: %s; %s", fast ? "Fast Hands (locks and traps by the better of Thieves' Tools and Sleight of Hand)"
                            : "Fast Hands NOT on",
      guard ? "Use Magic Device (Thief 13: any spell scroll, an Arcana check from level 2)"
            : "Use Magic Device NOT on");
}
