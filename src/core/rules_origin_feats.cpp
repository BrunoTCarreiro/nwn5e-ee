// The SRD 5.2 Origin feats nwn5e builds (CM8, `feats` faithful), beside
// Alert (NWN's Improved Initiative): Savage Attacker and Skilled
// (data/rows/feat.origin.txt). A human's extra level-1 feat (Versatile) can
// be any of them; the Soldier background gives Savage Attacker. Magic
// Initiate waits for spells (Phase 4).
// - Savage Attacker: once per turn, when you hit with a weapon, its damage
//   dice are rolled twice and the higher roll is kept (the weapon-dice filter
//   in rules_fighter.cpp asks TakeSavageAttacker).
// - Skilled: three more skill proficiencies of any kind, chosen with !skills
//   (rules::SkillAllowance::feat_skills, rules_skills.cpp).
// NWN mode: the rows are hidden.

#include <stdint.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "log.h"
#include "mastery.h"
#include "rules/feats2024.h"
#include "rules_loaded.h"

bool HasSavageAttackerFromBackground(void* creature);  // rules_abilities.cpp (the Soldier)

namespace {

bool Faithful() { return g_config.feats == rules::RuleVariant::kFaithful; }

std::unordered_map<uint32_t, uint32_t> g_savage_round;  // creature -> the round (ended count + 1) it was used

void HideOriginRows(void* rules_object, bool reload) {
  auto* feats = Field<uint8_t*>(rules_object, ee::kRulesFeats);
  const int count = Field<uint16_t>(rules_object, ee::kRulesFeatCount);
  if (!feats) return;
  for (int id : {rules::kFeatSavageAttacker, rules::kFeatSkilled}) {
    if (id < count) SetField<uint8_t>(feats + id * ee::kFeatStride, ee::kFeatMinLevel, 0xFF);
  }
  if (!reload) Log("feats: NWN's - Savage Attacker and Skilled hidden");
}

}  // namespace

bool TakeSavageAttacker(void* stats) {
  if (!Faithful() || !stats) return false;
  void* creature = CreatureOf(stats);
  if (!creature) return false;
  if (!engine::HasFeat(stats, rules::kFeatSavageAttacker) && !HasSavageAttackerFromBackground(creature)) return false;
  const uint32_t id = ObjectIdOf(stats);
  const uint32_t round = CombatRoundsEnded(id) + 1;
  uint32_t& used = g_savage_round[id];
  if (used == round) return false;  // once per turn
  used = round;
  return true;
}

void InstallOriginFeats() {
  if (!Faithful()) {
    OnRulesLoaded(&HideOriginRows);
    return;
  }
  if (!CountCombatRounds()) Log("SKIP feats: Savage Attacker's once per turn - combat rounds are not counted");
  Log("feats: Origin feats Savage Attacker (once per turn, weapon damage dice twice) and Skilled (3 skills, !skills)");
}
