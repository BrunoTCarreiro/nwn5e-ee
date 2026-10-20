#include "creature.h"

#include <algorithm>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"

namespace {

using BaseAttackBonusFn = int (*)(void* stats, int pre_epic_only);
using LevelFn = uint8_t (*)(void* stats, int subtract_negative_levels);
using ClassFn = uint8_t (*)(void* stats, uint8_t slot);
using ClassLevelFn = uint8_t (*)(void* stats, uint8_t slot, int subtract_negative, int scaled);
using CreatureByIdFn = void* (*)(void* app, uint32_t id);

// Engine getters, called through their exports (so a hook on one applies).
struct Engine {
  BaseAttackBonusFn base_attack_bonus = reinterpret_cast<BaseAttackBonusFn>(EngineExport(ee::kGetBaseAttackBonus));
  LevelFn level = reinterpret_cast<LevelFn>(EngineExport(ee::kGetLevel));
  ClassFn klass = reinterpret_cast<ClassFn>(EngineExport(ee::kGetClass));
  ClassLevelFn class_level = reinterpret_cast<ClassLevelFn>(EngineExport(ee::kGetClassLevel));
  CreatureByIdFn creature_by_id = reinterpret_cast<CreatureByIdFn>(EngineExport(ee::kGetCreatureByGameObjectId));
  void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
};

const Engine& Api() {
  static const Engine engine;
  return engine;
}

LogOnce g_implausible_logged(200);

}  // namespace

void* CreatureOf(void* stats) { return stats ? Field<void*>(stats, ee::kStatsCreature) : nullptr; }

void* StatsOf(void* creature) { return creature ? Field<void*>(creature, ee::kCreatureStats) : nullptr; }

uint32_t ObjectIdOf(void* stats) {
  const void* creature = CreatureOf(stats);
  return creature ? Field<uint32_t>(creature, ee::kObjectId) : 0;
}

void* CreatureById(uint32_t id) {
  const Engine& api = Api();
  if (!api.creature_by_id || !api.app_manager || !*api.app_manager) return nullptr;
  void* app = Field<void*>(*api.app_manager, ee::kAppManagerServerApp);
  return app ? api.creature_by_id(app, id) : nullptr;
}

CreatureFacts ReadCreatureFacts(void* stats) {
  CreatureFacts facts;
  const void* creature = CreatureOf(stats);
  facts.is_player = creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0;
  facts.associate_type = creature ? Field<uint16_t>(creature, ee::kCreatureAssociateType) : 0;
  facts.character = rules::UsesCharacterRules(facts.is_player, facts.associate_type);
  facts.level = Api().level ? Api().level(stats, 0) : 0;
  facts.challenge_rating = Field<float>(stats, ee::kStatsChallengeRating);
  facts.plausible = facts.character || rules::IsPlausibleChallengeRating(facts.challenge_rating);
  if (!facts.plausible && g_implausible_logged.First(LogOnce::Key(ObjectIdOf(stats), 0, 0))) {
    Log("implausible CR read for object 0x%08X (raw 0x%08X); engine numbers kept", ObjectIdOf(stats),
        Field<uint32_t>(stats, ee::kStatsChallengeRating));
  }
  return facts;
}

bool CountsAsCharacter(void* stats, const CreatureFacts& facts) {
  if (facts.character) return true;
  const bool summon_like = facts.associate_type >= 2 && facts.associate_type <= 5;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  return !slots.empty() && rules::ClassFromNwnId(slots[0].nwn_class_id).has_value() && !summon_like;
}

const char* KindOf(const CreatureFacts& facts) {
  if (facts.is_player) return "PC";
  return facts.character ? "henchman" : "creature";
}

std::vector<rules::NwnClassSlot> ReadClassSlots(void* stats) {
  std::vector<rules::NwnClassSlot> slots;
  const Engine& api = Api();
  if (!api.klass || !api.class_level) return slots;
  const int count = std::min<int>(Field<uint8_t>(stats, ee::kStatsClassCount), ee::kMaxClassSlots);
  for (int i = 0; i < count; ++i) {
    const int id = api.klass(stats, static_cast<uint8_t>(i));
    if (id == 0xFF) break;
    slots.push_back({id, api.class_level(stats, static_cast<uint8_t>(i), 0, 0)});
  }
  return slots;
}

int ClassLevel2024(void* stats, rules::CharacterClass character_class) {
  int levels = 0;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    if (c.character_class == character_class) levels += c.levels;
  }
  return levels;
}

int ConvertEngineAttackBonus(void* stats, int engine_value, const CreatureFacts& facts, int* base_attack_bonus,
                             int* proficiency_bonus) {
  if (!facts.plausible || !Api().base_attack_bonus) return engine_value;
  const int bab = Api().base_attack_bonus(stats, 0);
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  if (base_attack_bonus) *base_attack_bonus = bab;
  if (proficiency_bonus) *proficiency_bonus = pb;
  return rules::ConvertAttackModifier(engine_value, bab, pb);
}
