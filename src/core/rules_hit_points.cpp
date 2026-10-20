// CM3 (port of 1.69's rules_hit_points.cpp; docs/EE-PORT.md E4a), faithful:
// 2024 hit points, computed whenever the engine asks (GetMaxHitPoints): the
// first level's die at its maximum, then the fixed average (die / 2 + 1) per
// level, with the 2024 dice (Bard / Rogue d8, Sorcerer / Wizard d6; prestige
// classes use their parent class's, decision 10), + the Constitution modifier
// per level, at least 1. NWN's stored rolls, Toughness, Epic Toughness and the
// Pale Master's bonus are not counted (SRD 5.2 has none of them). Player
// characters and henchmen (decision 11); creatures keep their own. A
// character above level 20 counts its first 20. Nothing is written: the mode
// can switch back. NWN mode: NWN's.

#include <stdint.h>

#include <vector>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/hit_points.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"

namespace {

using GetMaxHitPointsFn = int16_t (*)(void* creature, int include_toughness);
using GetLevelStatsFn = void* (*)(void* stats, uint8_t index);

GetMaxHitPointsFn g_get_max_hit_points = nullptr;
LogOnce g_logged(300);

int HitDie(int nwn_class_id) {
  if (const auto c = rules::ClassFromNwnId(nwn_class_id)) return rules::HitDie2024(*c);
  void** rules_object = reinterpret_cast<void**>(EngineExport(ee::kRules));
  void* rules = rules_object ? *rules_object : nullptr;
  auto* classes = rules ? Field<uint8_t*>(rules, ee::kRulesClasses) : nullptr;
  if (!classes || nwn_class_id < 0 || nwn_class_id >= Field<uint8_t>(rules, ee::kRulesClassCount)) return 8;
  return Field<uint8_t>(classes + nwn_class_id * ee::kClassStride, ee::kClassHitDie);
}

// The hit die of each level, in the order taken: the level history when it
// covers every level (player characters), else the class slots in order.
std::vector<int> LevelDice(void* stats, int level) {
  static const auto level_stats = reinterpret_cast<GetLevelStatsFn>(EngineExport(ee::kGetLevelStats));
  level = level < rules::kMaxLevel2024 ? level : rules::kMaxLevel2024;
  std::vector<int> dice;
  if (level_stats && Field<int32_t>(stats, ee::kStatsLevelHistoryCount) >= level) {
    for (int i = 0; i < level; ++i) {
      void* entry = level_stats(stats, static_cast<uint8_t>(i));
      if (!entry) {
        dice.clear();
        break;
      }
      dice.push_back(HitDie(Field<uint8_t>(entry, ee::kLevelStatsClass)));
    }
    if (!dice.empty()) return dice;
  }
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    for (int i = 0; i < slot.levels && static_cast<int>(dice.size()) < level; ++i) dice.push_back(HitDie(slot.nwn_class_id));
  }
  return dice;
}

int16_t GetMaxHitPoints(void* creature, int include_toughness) {
  const int16_t nwn = g_get_max_hit_points(creature, include_toughness);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return nwn;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return nwn;
  const std::vector<int> dice = LevelDice(stats, facts.level);
  if (dice.empty()) return nwn;
  const int con = engine::AbilityModifier(stats, static_cast<int>(rules::Ability::kConstitution));
  const int hp = rules::MaxHitPoints2024(dice, con);
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), nwn, hp))) {
    Log("hit points: %s 0x%08X max %d (2024: d%d max at level 1, then average; Con %+d; %d levels) - NWN %d",
        KindOf(facts), ObjectIdOf(stats), hp, dice[0], con, static_cast<int>(dice.size()), nwn);
  }
  return static_cast<int16_t>(hp);
}

}  // namespace

void InstallHitPointRules() {
  if (g_config.hit_points != rules::RuleVariant::kFaithful) return;
  HookExport("CNWSCreature::GetMaxHitPoints (2024 hit points)", ee::kGetMaxHitPoints,
             reinterpret_cast<void*>(&GetMaxHitPoints), reinterpret_cast<void**>(&g_get_max_hit_points));
}
