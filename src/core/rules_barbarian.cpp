// Phase 3 Barbarian (docs/PLAN.md "Phase 3 — Barbarian (CM9)", decided by the
// owner 2026-10-24), with class_features faithful - part 1, the passives:
// - X1: NWN's Uncanny Dodge (195, 251-255) and Damage Reduction (196, 332-334,
//   epic 1067) have no 2024 counterpart: rules_features takes them off
//   characters, and the Barbarian's feat table served in faithful no longer
//   grants them (data/rows/cls_feat_barb.if_classfeat2024.txt; it also moves
//   Fast Movement to level 5).
// - B4 Unarmored Defense: with no armour (a chest item of base AC 0) the
//   Constitution modifier is added to AC (nothing when negative), a shield
//   allowed - GetArmorClassVersus (rules_attack.cpp) and the sheet's
//   GetArmorClass (rules_fighter.cpp), as the Defense style. A Monk's Wisdom
//   bonus, which the engine gives, isn't stacked with it (X5: the higher).
// - B5 Danger Sense (2): advantage on Dexterity saves unless Incapacitated
//   (the save d20, rules_save_roll.cpp).
// - B8 Fast Movement (5): +10 ft out of Heavy armour (base AC 6+). The engine
//   adds ruleset BARBARIAN_ENDURANCE_BONUS when HasFeat(194) holds
//   (GetMovementRateFactor +0x4A): for characters the feat is answered by
//   Barbarian level and armour, and the bonus is a third (10 ft of 30) -
//   monsters keep NWN's.
// - B9 Feral Instinct (7): advantage on initiative (rules_initiative.cpp).
// - B13 Primal Champion (20): Strength and Constitution +4, at most 25
//   (rules_abilities.cpp's 2024 scores).
// - B14 Weapon Mastery: melee weapons only (rules_mastery.cpp, the served
//   mastery rows).
// Part 2 (0.22.0), Rage - the feat stays NWN's (293 and its successors, the
// AI's and the dialogs'), spell 307 runs nwn5e_rage in faithful (a marker
// effect of the spell, so GetHasFeatEffect still reads it; the local
// nwn5e_rage_start tells the DLL), and the DLL keeps the 2024 Rage:
// - B1 uses: 2 / 3 / 4 / 5 / 6 by level, one pool for the Rage feats (X6,
//   feat_uses.cpp: the radial shows it); one back on a Short Rest, all on a
//   Long Rest. A player can't enter it in Heavy armour or while raging
//   (UseFeat's guard: nothing spent); an AI's Rage in Heavy armour gives nothing.
// - B2 while raging: Resistance to bludgeoning, piercing and slashing (X4,
//   resistance.cpp: halved once); Rage Damage +2 / +3 / +4 on Strength attacks
//   (melee, a thrown Throwing Axe, not a finesse weapon used with a better
//   Dexterity; rules_two_weapon's GetDamageBonus); advantage on Strength saves
//   (the save d20); no spells - AddCastSpellActions refuses spells.2da UserType
//   1, AddItemCastSpellActions scrolls, wands, rods and staffs (potions
//   allowed, the owner's call).
// - B3 the end: after a round (6 s of unpaused game time, game_clock.cpp)
//   with no attack roll at an enemy, no save forced on one and no chase (an
//   attack order on a living enemy it sees - the owner's caveat); or
//   incapacitated, unconscious, in Heavy armour, or 10 minutes.
// - B7 Primal Knowledge (3): Acrobatics, Intimidation, Perception, Stealth and
//   Survival with the better of Strength while raging (rules_skills.cpp).
// - BZ2 Mindless Rage (6): entering ends charm, domination and fear (CN5's
//   fear icon too) and gives immunity to them while raging.
// - B9 Instinctive Pounce (7): half again as fast for the first round.
// - B11 Relentless Rage (11): dropped to 0 HP while raging (not killed
//   outright), a Constitution save - DC 10, +5 per use since a rest - leaves
//   twice the Barbarian level in HP (rules_dying's DoDamage: dying faithful).
// - B12 Persistent Rage (15): all uses back on rolling initiative, once per
//   Long Rest (rules_initiative.cpp); the Rage lasts 10 minutes, only
//   unconsciousness or Heavy armour ends it early.
// - B13 Indomitable Might (18): a failed Strength save succeeds when the
//   Strength score meets the DC (rules_save_roll.cpp).
// Any creature that rages gets the 2024 Rage (spell 307's script is
// everyone's), at its Barbarian level; the uses pool is characters' only.
// `!rage`: the uses, the Rage, Relentless Rage's DC. Part 3 (Reckless Attack,
// Frenzy, Brutal Strike): rules_barbarian_strikes.cpp. Part 4: Intimidating
// Presence, Retaliation.
// NWN mode: NWN's Barbarian.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "feat_uses.h"
#include "feats.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "records.h"
#include "resistance.h"
#include "rest.h"
#include "ruleset.h"
#include "rules/class_features.h"
#include "rules/saves.h"
#include "rules/skills.h"
#include "rules/weapons.h"

namespace {

constexpr int kFeatBarbarianFastMovement = 194;
constexpr int kAbilityConstitution = 2;
constexpr int kAbilityWisdom = 4;

using ComputeArmorClassFn = int (*)(void* item);
using MovementRateFn = float (*)(void* creature);

ComputeArmorClassFn g_compute_armor_class = nullptr;
MovementRateFn g_movement_rate = nullptr;
thread_local void* t_moving = nullptr;  // the creature whose movement rate the engine is computing
LogOnce g_logged(500);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// A character's (player character or henchman) class levels in faithful; 0 otherwise.
int LevelsOf(void* stats, rules::CharacterClass character_class) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, character_class);
}

int BarbarianLevelOf(void* stats) { return LevelsOf(stats, rules::CharacterClass::kBarbarian); }

// The chest item's base AC (0 with none, or clothing).
int ChestArmorClass(void* creature) {
  void* chest = creature ? engine::ItemInSlot(creature, ee::kSlotChest) : nullptr;
  return chest && g_compute_armor_class ? g_compute_armor_class(chest) : 0;
}

bool HoldsShield(void* creature) {
  switch (engine::BaseItemRowInSlot(creature, ee::kSlotLeftHand)) {
    case 14: case 56: case 57: return true;  // small, large, tower shield
    default: return false;
  }
}

// B8: feat 194 for characters - Barbarian 5+, out of Heavy armour.
bool FastMovementFilter(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if (feat != kFeatBarbarianFastMovement || !stats || !Faithful()) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return false;  // monsters: NWN's
  void* creature = CreatureOf(stats);
  *answer = creature && rules::HasBarbarianFastMovement(ClassLevel2024(stats, rules::CharacterClass::kBarbarian)) &&
            ChestArmorClass(creature) < rules::kHeavyArmorBaseAc;
  return true;
}

float GetMovementRateFactor(void* creature) {
  void* outer = t_moving;
  t_moving = creature;
  const float factor = g_movement_rate(creature);
  t_moving = outer;
  return factor;
}

float EnduranceBonus(float engine_value) {
  if (!t_moving || !Faithful()) return engine_value;
  void* stats = StatsOf(t_moving);
  if (!stats || !ReadCreatureFacts(stats).character) return engine_value;
  return rules::kBarbarianFastMovementFactor;
}

// --- Part 2: Rage ---------------------------------------------------------------

struct Vec3 {
  float x, y, z;
};

constexpr const char* kVarRageSpent = "nwn5e_rage_spent";            // X6: the Rage pool's spent uses (record)
constexpr const char* kVarRageStart = "nwn5e_rage_start";            // nwn5e_rage.nss: a Rage begins
constexpr const char* kVarRelentless = "nwn5e_relentless_rage";      // B11: uses since a rest (record)
constexpr const char* kVarPersistentUsed = "nwn5e_persistent_rage";  // B12: used since a Long Rest (record)
constexpr const char* kVarDown = "nwn5e_down";                       // rules_dying: a down player or henchman
constexpr const char* kRageTag = "nwn5e_rage";
constexpr int kRageIconRow = 220;         // effecticons.txt NWN5E_RAGE (the feat's own icon, ife_rage)
constexpr int kBaseItemThrowingAxe = 63;  // 2024's thrown Handaxe: a Strength attack
constexpr int kStateAsleep = 9;
constexpr int kAbilityStrength = 0;
constexpr int kAbilityDexterity = 1;
constexpr int kUserTypeSpell = 1;  // spells.2da UserType: 1 spells, 2 creature powers, 3 feats, 4 item powers
constexpr uint64_t kPendingMs = 15000;  // a queued Rage's cast, at most
constexpr uint64_t kWatchMs = 250;

using AddCastSpellActionsFn = int (*)(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic,
                                      int spontaneous, Vec3 location, uint32_t target, int area_target,
                                      int add_to_front, int fake, uint8_t projectile_path, int instant, int unknown,
                                      int feat, uint8_t unknown2);
using AddItemCastSpellActionsFn = int (*)(void* creature, uint32_t item, int property, int sub_property,
                                          Vec3 location, uint32_t target, int area_target, int flag);
using ItemByIdFn = void* (*)(void* app, uint32_t id);
using RemoveEffectFn = void (*)(void* object, void* effect);
using SaveBonusFn = int8_t (*)(void* stats, int exclude_effects);

struct Rage {
  uint64_t started = 0;      // game time (GameMs)
  uint64_t last_active = 0;  // game time: its last attack roll at an enemy, save forced on one, or chase
  uint64_t next_check = 0;   // real time
  int level = 1;             // its Barbarian level (1 for a creature raging without one)
};

// The engine's thread only.
std::unordered_map<uint32_t, Rage> g_raging;
std::unordered_map<uint32_t, uint64_t> g_pending;  // a Rage queued or seen starting: real time
AddCastSpellActionsFn g_add_cast_spell_actions = nullptr;
AddItemCastSpellActionsFn g_add_item_cast_spell_actions = nullptr;
ItemByIdFn g_item_by_id = nullptr;
void** g_app_manager = nullptr;
RemoveEffectFn g_remove_effect = nullptr;
SaveBonusFn g_fort_save = nullptr;
std::vector<int> g_spell_user_types;  // spells.2da UserType by row, read on first need

uint32_t IdOf(const void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

bool IsPlayerCreature(void* creature) {
  void* stats = StatsOf(creature);
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayerCreature(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

// The level a Rage runs at: any raging creature's Barbarian level (spell 307 is everyone's).
int RageLevel(void* stats) { return std::max(1, stats ? ClassLevel2024(stats, rules::CharacterClass::kBarbarian) : 0); }

bool InHeavyArmour(void* creature) { return ChestArmorClass(creature) >= rules::kHeavyArmorBaseAc; }

bool HasRageIcon(const void* creature) {
  bool found = false;
  effects::ForEachOfType(creature, ee::kEffectTypeIcon, [&](const void* effect) {
    if (effects::Integer(effect, 0) == kRageIconRow) found = true;
  });
  return found;
}

Rage* RageOf(const void* creature) {
  if (g_raging.empty() || !creature) return nullptr;
  const auto it = g_raging.find(IdOf(creature));
  return it == g_raging.end() ? nullptr : &it->second;
}

// 2024 Unconscious: dead or dying, at 0 HP, down (rules_dying), or asleep.
bool Unconscious(void* creature) {
  return engine::IsDeadOrDying(creature) || Field<int16_t>(creature, ee::kObjectCurrentHitPoints) < 1 ||
         engine::GetLocalInt(creature, kVarDown) != 0 || effects::HasState(creature, kStateAsleep);
}

// The target of the attack order at the head of its action queue (outside
// RunActions the running action is back at the head), or invalid.
uint32_t AttackOrderTarget(void* creature) {
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  const void* head = list ? Field<void*>(list, ee::kListHead) : nullptr;
  const void* action = head ? Field<void*>(head, ee::kListNodeData) : nullptr;
  if (!action || Field<int32_t>(action, ee::kActionId) != ee::kActionAttackObject) return ee::kObjectInvalid;
  return Field<uint32_t>(action, ee::kActionFirstParam);
}

int HeadActionId(void* creature) {
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  const void* head = list ? Field<void*>(list, ee::kListHead) : nullptr;
  const void* action = head ? Field<void*>(head, ee::kListNodeData) : nullptr;
  return action ? Field<int32_t>(action, ee::kActionId) : -1;
}

bool ChasingTarget(void* creature, uint32_t target) {
  if (target == 0 || target == ee::kObjectInvalid) return false;
  void* other = CreatureById(target);
  return other && !engine::IsDeadOrDying(other) && engine::IsEnemy(creature, target) && engine::Sees(creature, target);
}

// B3's chase (the owner's caveat): an attack order on a living enemy it sees -
// the engine's attack target, or (0.41.1; owner: "rage always ends before the
// PC can attack an enemy, even if they have an enemy targeted") the attack
// order at the head of its queue: running at the enemy, the attack target
// field isn't set yet.
bool Chasing(void* creature) {
  return ChasingTarget(creature, Field<uint32_t>(creature, ee::kCreatureAttackTarget)) ||
         ChasingTarget(creature, AttackOrderTarget(creature));
}

void EndRage(void* creature, const char* why) {
  const uint32_t id = IdOf(creature);
  const auto it = g_raging.find(id);
  if (it == g_raging.end()) return;
  const unsigned long long seconds = (GameMs() - it->second.started) / 1000;
  g_raging.erase(it);
  char script[400];
  sprintf_s(script,
            "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectSpellId(e) == %d || "
            "GetEffectTag(e) == \"%s\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }",
            rules::kSpellRage, kRageTag);
  RunScriptChunk(script, id);
  Log("barbarian: 0x%08X Rage ends after %llu s - %s", id, seconds, why);
  Tell(creature, std::string("your Rage ends - ") + why + ".");
}

// What ends a Rage now, or nullptr.
const char* RageEnds(void* creature, Rage& rage) {
  if (Unconscious(creature)) return "you are unconscious";
  if (InHeavyArmour(creature)) return "Heavy armour";
  const uint64_t now = GameMs();
  if (now - rage.started >= static_cast<uint64_t>(rules::kRageMaxSeconds) * 1000) return "10 minutes";
  if (rules::HasPersistentRage(rage.level)) return nullptr;  // B12: nothing else ends it
  if (IncapacitatedByCondition(creature)) return "incapacitated";
  if (Chasing(creature)) rage.last_active = now;
  if (now - rage.last_active >= static_cast<uint64_t>(rules::kRageLapseSeconds) * 1000) {
    Log("barbarian: 0x%08X Rage lapses - attack target 0x%08X, head action %d (its target 0x%08X)", IdOf(creature),
        Field<uint32_t>(creature, ee::kCreatureAttackTarget), HeadActionId(creature), AttackOrderTarget(creature));
    return "a round without attacking or chasing an enemy";
  }
  return nullptr;
}

// BZ2: CN5's fear is an icon effect nwn5e put on the creature (its record).
void DropFear(void* creature) {
  if (!g_remove_effect) return;
  std::vector<void*> fears;
  effects::ForEachOfType(creature, ee::kEffectTypeIcon, [&](const void* effect) {
    if (effects::Integer(effect, 0) == ee::kIconFrightened) fears.push_back(const_cast<void*>(effect));
  });
  for (void* effect : fears) g_remove_effect(creature, effect);
}

void StartRage(void* creature) {
  engine::SetLocalInt(creature, kVarRageStart, 0);
  void* stats = StatsOf(creature);
  if (!stats) return;
  const uint32_t id = IdOf(creature);
  Rage rage;
  rage.started = rage.last_active = GameMs();
  rage.level = RageLevel(stats);
  g_raging[id] = rage;
  if (InHeavyArmour(creature)) {
    if (IsPlayerCreature(creature)) {  // armour put on during the cast (UseFeat's guard refuses it before)
      EndRage(creature, "you can't rage in Heavy armour");
      return;
    }
    // An AI's Rage (the guard lets it through, so a henchman's AI isn't left
    // idle retrying): the use is spent for nothing, and the script's marker
    // stays its 10 minutes, so the AI (GetHasFeatEffect) doesn't try again.
    g_raging.erase(id);
    Log("barbarian: 0x%08X rages in Heavy armour - no Rage (its marker stays, so its AI doesn't retry)", id);
    return;
  }
  const bool pounce = rules::HasInstinctivePounce(rage.level);
  const bool mindless = rules::HasMindlessRage(rage.level);
  const bool persistent = rules::HasPersistentRage(rage.level);
  const std::string seconds = std::to_string(rules::kRageMaxSeconds) + ".0";
  std::string script = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect(EffectIcon(" +
                       std::to_string(kRageIconRow) + ")), \"" + kRageTag + "\"), OBJECT_SELF, " + seconds + ");";
  if (pounce) {
    script += "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(ExtraordinaryEffect("
              "EffectMovementSpeedIncrease(" + std::to_string(rules::kInstinctivePounceSpeedPercent) + ")), \"" +
              kRageTag + "\"), OBJECT_SELF, 6.0);";
  }
  if (mindless) {
    script += "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { int t = GetEffectType(e);"
              " if (t == EFFECT_TYPE_CHARMED || t == EFFECT_TYPE_DOMINATED || t == EFFECT_TYPE_FRIGHTENED)"
              " RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }"
              "effect i = EffectLinkEffects(EffectImmunity(IMMUNITY_TYPE_CHARM), EffectLinkEffects("
              "EffectImmunity(IMMUNITY_TYPE_DOMINATE), EffectImmunity(IMMUNITY_TYPE_FEAR)));"
              "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(ExtraordinaryEffect(i), \"" +
              std::string(kRageTag) + "\"), OBJECT_SELF, " + seconds + ");";
    DropFear(creature);
  }
  RunScriptChunk(script.c_str(), id);
  const int damage = rules::RageDamage2024(rage.level);
  Log("barbarian: 0x%08X Rage starts (level %d: +%d damage, resistance%s%s%s)", id, rage.level, damage,
      pounce ? ", Instinctive Pounce" : "", mindless ? ", Mindless Rage" : "", persistent ? ", Persistent Rage" : "");
  std::string line = "Rage: +" + std::to_string(damage) +
                     " damage on Strength attacks, resistance to bludgeoning, piercing and slashing, advantage on "
                     "Strength saves; no spells.";
  line += persistent ? " It lasts 10 minutes; only unconsciousness or Heavy armour ends it early."
                     : " Attack or chase an enemy every round to keep it (10 minutes at most).";
  if (pounce) line += " Instinctive Pounce: half again as fast this round.";
  if (mindless) line += " Mindless Rage: immune to charm and fear.";
  Tell(creature, line);
}

// The creature update: a queued Rage starting (nwn5e_rage.nss ran), and each
// Rage's end.
void WatchRage(void* creature) {
  if (g_pending.empty() && g_raging.empty()) return;
  const uint32_t id = IdOf(creature);
  if (!g_pending.empty()) {
    const auto pending = g_pending.find(id);
    if (pending != g_pending.end()) {
      if (engine::GetLocalInt(creature, kVarRageStart)) {
        g_pending.erase(pending);
        StartRage(creature);
        return;
      }
      if (GetTickCount64() - pending->second > kPendingMs) g_pending.erase(pending);
    }
  }
  Rage* rage = RageOf(creature);
  if (!rage) return;
  const uint64_t now = GetTickCount64();
  if (now < rage->next_check) return;
  rage->next_check = now + kWatchMs;
  if (const char* why = RageEnds(creature, *rage)) EndRage(creature, why);
}

// B3: an attack roll at an enemy keeps a Rage going; an AI's Rage that didn't
// go through UseFeat is seen here first.
void BeforeAttackRoll(AttackRoll& roll) {
  if (!roll.attacker) return;
  if (Rage* rage = RageOf(roll.attacker)) {
    if (roll.target && engine::IsEnemy(roll.attacker, IdOf(roll.target))) rage->last_active = GameMs();
    return;
  }
  if (engine::GetLocalInt(roll.attacker, kVarRageStart)) g_pending[IdOf(roll.attacker)] = GetTickCount64();
}

// UseFeat: a Rage queued - it starts when its script runs.
void RageUsed(void* creature, int feat) {
  if (rules::IsRageFeat(feat)) g_pending[IdOf(creature)] = GetTickCount64();
}

// UseFeat's guard: no Rage while raging, nor a player's in Heavy armour (nothing spent).
bool RageGuard(void* creature, int feat) {
  if (!rules::IsRageFeat(feat)) return true;
  if (RageOf(creature)) {
    if (HasRageIcon(creature)) {
      Tell(creature, "you are already raging.");
      return false;
    }
    g_raging.erase(IdOf(creature));  // its effects are gone (dispelled): a new Rage may start
  }
  if (InHeavyArmour(creature) && IsPlayerCreature(creature)) {
    Tell(creature, "you can't rage in Heavy armour.");
    return false;
  }
  return true;
}

// X6: the Rage pool, for characters.
bool RageUsesTotal(void* stats, int feat, int* total) {
  if (!rules::IsRageFeat(feat)) return false;
  const int level = BarbarianLevelOf(stats);
  if (level < 1) return false;
  *total = rules::RageUses2024(level);
  return true;
}

int SpellUserType(uint32_t spell) {
  if (g_spell_user_types.size() < 2) {
    g_spell_user_types.clear();
    for (const std::string& value : GameTableColumn("spells", "UserType")) {
      g_spell_user_types.push_back(atoi(value.c_str()));
    }
  }
  return spell < g_spell_user_types.size() ? g_spell_user_types[spell] : 0;
}

// B2: no spells while raging (feat and creature abilities - Rage itself - pass).
int AddCastSpellActions(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic,
                        int spontaneous, Vec3 location, uint32_t target, int area_target, int add_to_front, int fake,
                        uint8_t projectile_path, int instant, int unknown, int feat, uint8_t unknown2) {
  if (creature && RageOf(creature) && SpellUserType(spell) == kUserTypeSpell) {
    Log("barbarian: 0x%08X raging - spell %u refused", IdOf(creature), spell);
    Tell(creature, "you can't cast spells while raging.");
    return 0;
  }
  return g_add_cast_spell_actions(creature, spell, class_slot, domain_level, metamagic, spontaneous, location, target,
                                  area_target, add_to_front, fake, projectile_path, instant, unknown, feat, unknown2);
}

// Scrolls, wands, rods and magic staffs cast spells; potions (the owner's
// call) and other item powers are allowed.
bool CastsSpells(uint32_t item_id) {
  if (!g_item_by_id || !g_app_manager || !*g_app_manager) return false;
  void* app = Field<void*>(*g_app_manager, ee::kAppManagerServerApp);
  void* item = app ? g_item_by_id(app, item_id) : nullptr;
  switch (engine::BaseItemRow(item)) {
    case 44: case 45: case 46: case 75: case 105: case 106: return true;  // rods, staffs, wands, scrolls
    default: return false;
  }
}

int AddItemCastSpellActions(void* creature, uint32_t item, int property, int sub_property, Vec3 location,
                            uint32_t target, int area_target, int flag) {
  if (creature && RageOf(creature) && CastsSpells(item)) {
    Log("barbarian: 0x%08X raging - item 0x%08X (casts a spell) refused", IdOf(creature), item);
    Tell(creature, "you can't cast spells while raging - scrolls, wands, rods and staffs included.");
    return 0;
  }
  return g_add_item_cast_spell_actions(creature, item, property, sub_property, location, target, area_target, flag);
}

// X4: B, P and S damage halved while raging.
uint32_t RageResistance(void* object) {
  return RageOf(object) ? (kDamageBludgeoning | kDamagePiercing | kDamageSlashing) : 0;
}

void ClearRecord(void* creature, const char* name) {
  if (engine::GetLocalInt(creature, name) != 0) SetRecord(creature, name, 0);
}

// X6 / B1 / B11: a Short Rest gives a Rage use back and resets Relentless Rage's DC.
std::string RageShortRest(void* creature) {
  if (!creature) return "";
  if (RageOf(creature)) EndRage(creature, "a rest");
  EnsureRecords(creature);
  ClearRecord(creature, kVarRelentless);
  const int spent = SpentFeatUses(creature, kVarRageSpent);
  if (spent <= 0 || BarbarianLevelOf(StatsOf(creature)) < 1) return "";
  SetSpentFeatUses(creature, kVarRageSpent, spent - 1);
  return " Rage: a use back.";
}

// The end of a Long Rest (the pool itself: feat_uses.cpp).
void RageLongRest(void* stats) {
  void* creature = CreatureOf(stats);
  if (!creature) return;
  if (RageOf(creature)) EndRage(creature, "a rest");
  EnsureRecords(creature);
  ClearRecord(creature, kVarRelentless);
  ClearRecord(creature, kVarPersistentUsed);
}

std::vector<std::string> RageCommand(void* creature, const std::string& args) {
  void* stats = StatsOf(creature);
  const int level = BarbarianLevelOf(stats);
  if (level < 1) return {"nwn5e: Rage - you have no Barbarian levels (or class_features isn't faithful)."};
  EnsureRecords(creature);
  if (args == "reset") {
    SetSpentFeatUses(creature, kVarRageSpent, 0);
    ClearRecord(creature, kVarRelentless);
    ClearRecord(creature, kVarPersistentUsed);
    return {"nwn5e: Rage - every use back, Relentless Rage's DC 10, Persistent Rage ready (a test command)."};
  }
  const int total = rules::RageUses2024(level);
  const int left = std::max(0, total - SpentFeatUses(creature, kVarRageSpent));
  std::string line = "nwn5e: Rage - " + std::to_string(left) + " of " + std::to_string(total) +
                     " uses left (Barbarian " + std::to_string(level) + "), Rage Damage +" +
                     std::to_string(rules::RageDamage2024(level));
  if (const Rage* rage = RageOf(creature)) {
    const uint64_t now = GameMs();
    line += "; raging for " + std::to_string((now - rage->started) / 1000) + " s";
    if (!rules::HasPersistentRage(level)) {
      line += ", last attack, forced save or chase " + std::to_string((now - rage->last_active) / 1000) + " s ago";
    }
  } else {
    line += "; not raging";
  }
  if (rules::HasRelentlessRage(level)) {
    line += "; Relentless Rage DC " + std::to_string(rules::RelentlessRageDc(engine::GetLocalInt(creature, kVarRelentless)));
  }
  if (rules::HasPersistentRage(level)) {
    line += engine::GetLocalInt(creature, kVarPersistentUsed) ? "; Persistent Rage used" : "; Persistent Rage ready";
  }
  return {line + ". (`!rage reset`: every use back.)"};
}

}  // namespace

// B4: the AC an unarmoured Barbarian adds (GetArmorClassVersus and the sheet).
int UnarmoredDefenseBonus(void* stats) {
  if (BarbarianLevelOf(stats) < 1) return 0;
  void* creature = CreatureOf(stats);
  if (!creature || ChestArmorClass(creature) > 0) return 0;
  // X5: a Monk without a shield already has its Wisdom (GetACNaturalBase).
  const int monk_wis = ClassLevel2024(stats, rules::CharacterClass::kMonk) > 0 && !HoldsShield(creature)
                           ? engine::AbilityModifier(stats, kAbilityWisdom)
                           : 0;
  const int bonus = rules::UnarmoredDefenseConBonus(engine::AbilityModifier(stats, kAbilityConstitution), monk_wis);
  if (bonus > 0 && g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), bonus, 1))) {
    Log("barbarian: 0x%08X Unarmored Defense: +%d AC (Constitution, no armour)", ObjectIdOf(stats), bonus);
  }
  return bonus;
}

// Advantage a class gives a creature's save on `ability` (the save d20): B5
// Danger Sense on Dexterity saves, unless Incapacitated.
int ClassSaveAdvantage(void* creature, rules::Ability ability) {
  void* stats = StatsOf(creature);
  if (ability == rules::Ability::kDexterity && rules::HasDangerSense(BarbarianLevelOf(stats)) &&
      !IncapacitatedByCondition(creature)) {
    return 1;
  }
  if (ability == rules::Ability::kStrength && RageOf(creature)) return 1;  // B2: Strength saves while raging
  return 0;
}

// B9 Feral Instinct: advantage on initiative (rules_initiative.cpp).
bool HasFeralInstinctAdvantage(void* creature) {
  return creature && rules::HasFeralInstinct(BarbarianLevelOf(StatsOf(creature)));
}

// Part 3 (rules_barbarian_strikes.cpp): Reckless Attack and Frenzy while raging.
bool BarbarianRaging(void* creature) { return RageOf(creature) != nullptr; }

// B2 Rage Damage (rules_two_weapon's GetDamageBonus): a raging creature's
// Strength attacks - melee, or a thrown Throwing Axe; not a finesse weapon
// used with a better Dexterity. 0 outside an attack (the sheet).
int RageDamageBonus(void* stats, int off_hand) {
  if (g_raging.empty() || !stats) return 0;
  void* creature = CreatureOf(stats);
  const Rage* rage = RageOf(creature);
  if (!rage) return 0;
  const uint8_t* data = CurrentAttackData(creature);
  if (!data) return 0;
  const int row = engine::BaseItemRowInSlot(creature, off_hand ? ee::kSlotLeftHand : ee::kSlotRightHand);
  if (Field<int32_t>(data, ee::kAttackRanged) != 0 && row != kBaseItemThrowingAxe) return 0;
  if (row >= 0 && rules::IsFinesseNwnBaseItem(row) && g_config.finesse == rules::RuleVariant::kFaithful &&
      engine::AbilityModifier(stats, kAbilityDexterity) > engine::AbilityModifier(stats, kAbilityStrength)) {
    return 0;  // 2024 Finesse picked Dexterity: not a Strength attack
  }
  return rules::RageDamage2024(rage->level);
}

// B7 Primal Knowledge (rules_skills' 2024 skill bonus): the ability modifier
// for `skill`, Strength when better while raging.
int RageSkillAbility(void* creature, rules::Skill2024 skill, int modifier) {
  const Rage* rage = RageOf(creature);
  if (!rage || !rules::HasPrimalKnowledge(rage->level)) return modifier;
  switch (skill) {
    case rules::Skill2024::kAcrobatics:
    case rules::Skill2024::kIntimidation:
    case rules::Skill2024::kPerception:
    case rules::Skill2024::kStealth:
    case rules::Skill2024::kSurvival:
      return std::max(modifier, engine::AbilityModifier(StatsOf(creature), kAbilityStrength));
    default:
      return modifier;
  }
}

// B3 (rules_save_roll's wrapper): a save a raging creature forced on an enemy
// keeps its Rage going.
void NoteSaveForced(uint32_t by, void* creature) {
  if (g_raging.empty() || !creature) return;
  const auto it = g_raging.find(by);
  if (it == g_raging.end() || by == IdOf(creature)) return;
  void* raging = CreatureById(by);
  if (raging && engine::IsEnemy(raging, IdOf(creature))) it->second.last_active = GameMs();
}

// B13 Indomitable Might (rules_save_roll's wrapper, after a failed Strength
// save): a character Barbarian 18+ whose Strength score meets the DC succeeds.
bool IndomitableMightSucceeds(void* creature, int dc) {
  void* stats = StatsOf(creature);
  if (!rules::HasIndomitableMight(BarbarianLevelOf(stats))) return false;
  const int score = engine::AbilityScore(stats, kAbilityStrength);
  if (rules::IndomitableMightTotal(0, score) < dc) return false;
  Log("barbarian: 0x%08X Indomitable Might - Strength %d meets DC %d", IdOf(creature), score, dc);
  return true;
}

// B11 Relentless Rage (rules_dying's DoDamage: a creature dropped to 0 HP, not
// killed outright): the hit points to set, or 0.
int RelentlessRage(void* creature) {
  const Rage* rage = RageOf(creature);
  void* stats = StatsOf(creature);
  if (!rage || !stats || !rules::HasRelentlessRage(rage->level)) return 0;
  EnsureRecords(creature);
  const int uses = engine::GetLocalInt(creature, kVarRelentless);
  const int dc = rules::RelentlessRageDc(uses);
  SetRecord(creature, kVarRelentless, uses + 1);
  const int d20 = RollEngineDice(1, 20);
  const int bonus = g_fort_save ? g_fort_save(stats, 0) : engine::AbilityModifier(stats, kAbilityConstitution);
  const bool saved = d20 + bonus >= dc;
  const int hit_points = rules::RelentlessRageHitPoints(rage->level);
  Log("barbarian: 0x%08X Relentless Rage - Constitution save %d %+d = %d vs DC %d: %s", IdOf(creature), d20, bonus,
      d20 + bonus, dc, saved ? "back up" : "down");
  Tell(creature, "Relentless Rage - Constitution save " + std::to_string(d20) + (bonus < 0 ? " - " : " + ") +
                     std::to_string(bonus < 0 ? -bonus : bonus) + " = " + std::to_string(d20 + bonus) + " vs DC " +
                     std::to_string(dc) +
                     (saved ? ": you stay up with " + std::to_string(hit_points) + " HP." : ": a failure, you drop."));
  return saved ? hit_points : 0;
}

// B12 Persistent Rage (rules_initiative's ResolveInitiative): rolling
// initiative gives every Rage use back, once per Long Rest.
void PersistentRageOnInitiative(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!rules::HasPersistentRage(BarbarianLevelOf(stats))) return;
  EnsureRecords(creature);
  if (SpentFeatUses(creature, kVarRageSpent) <= 0 || engine::GetLocalInt(creature, kVarPersistentUsed)) return;
  SetSpentFeatUses(creature, kVarRageSpent, 0);
  SetRecord(creature, kVarPersistentUsed, 1);
  Log("barbarian: 0x%08X Persistent Rage - every Rage use back", IdOf(creature));
  Tell(creature, "Persistent Rage - every Rage use comes back (once per Long Rest).");
}

void InstallBarbarianRules() {
  if (!Faithful()) return;
  ServeConditionalRows("classfeat2024");  // X1 + B8: cls_feat_barb.if_classfeat2024.txt
  g_compute_armor_class = reinterpret_cast<ComputeArmorClassFn>(EngineExport(ee::kItemComputeArmorClass));
  if (!g_compute_armor_class) Log("barbarian: no %s - Unarmored Defense and Fast Movement see no armour",
                                  ee::kItemComputeArmorClass);
  if (!AddFeatFilter(&FastMovementFilter)) Log("SKIP barbarian: Fast Movement - HasFeat not hooked");
  if (!HookExport("CNWSCreature::GetMovementRateFactor (Fast Movement: whose rate)", ee::kGetMovementRateFactor,
                  reinterpret_cast<void*>(&GetMovementRateFactor), reinterpret_cast<void**>(&g_movement_rate)) ||
      !AnswerRulesetFloat("BARBARIAN_ENDURANCE_BONUS", "Fast Movement: +10 ft (a third) for characters",
                          &EnduranceBonus)) {
    Log("barbarian: Fast Movement keeps NWN's +10%%");
  }
  Log("barbarian: X1 (no NWN Uncanny Dodge / Damage Reduction), Unarmored Defense, Danger Sense, Fast Movement, "
      "Feral Instinct, Primal Champion, melee Weapon Mastery (class_features faithful)");

  // Part 2: Rage (spell 307's served row runs nwn5e_rage: spells.if_classfeat2024.txt).
  RegisterRecords({kVarRelentless, kVarPersistentUsed});
  g_item_by_id = reinterpret_cast<ItemByIdFn>(EngineExport(ee::kGetItemByGameObjectId));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  g_remove_effect = reinterpret_cast<RemoveEffectFn>(EngineExport(ee::kObjectRemoveEffect));
  g_fort_save = reinterpret_cast<SaveBonusFn>(EngineExport(ee::kGetFortSavingThrow));
  if (!StartGameClock()) Log("barbarian: no game clock - a Rage never lapses, only its 10 minutes end it");
  if (!AddCreatureUpdateListener(&WatchRage)) {
    Log("SKIP barbarian: Rage - no creature update to start and end it");
    return;
  }
  if (!AddFeatUseListener(&RageUsed) || !AddFeatUseGuard(&RageGuard)) {
    Log("barbarian: no UseFeat hook - a Rage starts at its first attack; Heavy armour ends it at once");
  }
  if (!AddAttackRollListeners(&BeforeAttackRoll, nullptr)) Log("barbarian: no attack-roll hook - attacks don't keep a Rage going");
  if (!AddFeatUsesProvider(kVarRageSpent, &RageUsesTotal)) Log("barbarian: Rage keeps NWN's uses");
  if (!AddResistanceProvider(&RageResistance)) Log("barbarian: no DoDamageImmunity hook - a Rage gives no resistance");
  if (!HookExport("CNWSCreature::AddCastSpellActions (Rage: no spells)", ee::kAddCastSpellActions,
                  reinterpret_cast<void*>(&AddCastSpellActions), reinterpret_cast<void**>(&g_add_cast_spell_actions))) {
    Log("barbarian: a raging creature can still cast spells");
  }
  if (!HookExport("CNWSCreature::AddItemCastSpellActions (Rage: no scrolls, wands)", ee::kAddItemCastSpellActions,
                  reinterpret_cast<void*>(&AddItemCastSpellActions),
                  reinterpret_cast<void**>(&g_add_item_cast_spell_actions))) {
    Log("barbarian: a raging creature can still use scrolls and wands");
  }
  AddShortRestListener(&RageShortRest);
  if (!AddRestListener(&RageLongRest)) Log("barbarian: the end of a rest is not hooked - Relentless Rage's DC stays");
  AddChatCommand("!rage", &RageCommand);
  Log("barbarian: Rage - 2024 uses, resistance, Rage Damage, Strength advantage, no spells, the 2024 end, Primal "
      "Knowledge, Mindless Rage, Instinctive Pounce, Relentless / Persistent Rage, Indomitable Might");
}
