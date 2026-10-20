// Weapon mastery (port of 1.69's rules_mastery.cpp; docs/EE-PORT.md E2).
//
// D3 (PLAN.md): who has weapon mastery with which weapons. Only characters
// (player characters and henchmen, decision 11) have it. Faithful: the 2024
// slot counts by class; slots are chosen with `!mastery` or fill with the
// weapon kinds the character fights with (announced in chat); one chosen
// weapon can be swapped after each long rest. Weapon Focus gives mastery with
// its weapon on top (2024 Weapon Master stand-in). NWN mode: Barbarians,
// Fighters, Paladins, Rangers and Rogues have mastery with every weapon they
// are proficient with, Weapon Focus the same. The choices are local
// variables on the creature, so they live in the save game.
//
// D4: what the masteries do. Vex (a hit that deals damage gives advantage on
// your next attack at that target until the end of your next round), Sap (a
// hit gives the target disadvantage on its next attack until your next round
// starts), Graze (a miss deals the attack's ability modifier as slashing
// damage), Slow (a hit that deals damage: speed -33% for a round, not
// stacking), Topple (a hit: Constitution save against 8 + ability modifier +
// proficiency bonus, or knocked down for a round), Cleave (a melee hit adds
// NWN's cleave attack, once per round, at the nearest other enemy within 5 ft
// of the target and in reach; its damage has no ability modifier unless
// negative), Push (faithful only: rules_push.cpp). Rounds are counted at EndCombatRound;
// "deals damage" is seen at OnApplyDamage. Effects (Graze damage, Slow,
// Topple's knockdown) are created through the script bridge, as the
// attacker, a moment after the swing.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include <windows.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "rest.h"
#include "rules/core.h"
#include "rules/nwn_conversion.h"
#include "rules/weapons.h"
#include "saves.h"

namespace {

// --- D3 ---------------------------------------------------------------------

LogOnce g_logged_slots(256);

constexpr int kMaxSlots = 6;
const char* const kSlotVars[kMaxSlots] = {"nwn5e_mastery_1", "nwn5e_mastery_2", "nwn5e_mastery_3",
                                          "nwn5e_mastery_4", "nwn5e_mastery_5", "nwn5e_mastery_6"};
const char kSwapVar[] = "nwn5e_mastery_swap";

bool Faithful() { return g_config.weapon_mastery == rules::RuleVariant::kFaithful; }
bool MasteryOn() { return g_config.weapon_mastery != rules::RuleVariant::kOff; }

bool HasFeat(void* stats, int feat) { return stats && feat >= 0 && engine::HasFeat(stats, feat); }

bool Proficient(void* stats, int base_item) {
  const std::vector<int> feats = rules::ProficiencyFeatsForNwnBaseItem(base_item);
  if (feats.empty()) return true;
  for (int feat : feats) {
    if (HasFeat(stats, feat)) return true;
  }
  return false;
}

bool HasFocus(void* stats, int base_item) { return HasFeat(stats, rules::WeaponFocusFeatForNwnBaseItem(base_item)); }

std::vector<rules::ClassLevels> Classes(void* stats) { return rules::GroupClassLevels(ReadClassSlots(stats)); }

bool MasteryClass(void* stats) {
  for (const rules::ClassLevels& entry : Classes(stats)) {
    if (rules::HasWeaponMastery(entry.character_class)) return true;
  }
  return false;
}

struct Slots {
  int count = 0;
  std::vector<int> chosen = std::vector<int>(kMaxSlots, -1);  // base items, -1 empty
  bool swap_ready = false;
};

Slots ReadSlots(void* creature, void* stats) {
  Slots slots;
  slots.count = rules::WeaponMasterySlots(Classes(stats));
  if (slots.count > kMaxSlots) slots.count = kMaxSlots;
  for (int i = 0; i < kMaxSlots; ++i) {
    const int value = engine::GetLocalInt(creature, kSlotVars[i]);
    slots.chosen[i] = value > 0 ? value - 1 : -1;
  }
  slots.swap_ready = engine::GetLocalInt(creature, kSwapVar) != 0;
  return slots;
}

int UsedSlots(const Slots& slots) {
  int used = 0;
  for (int i = 0; i < slots.count; ++i) used += slots.chosen[i] >= 0 ? 1 : 0;
  return used;
}

std::string Label(int base_item) {
  const char* name = rules::NwnWeaponName(base_item);
  if (!name) return "?";
  return std::string(name) + " (" + rules::MasteryName(rules::MasteryForNwnBaseItem(base_item)) + ")";
}

std::string SlotList(const Slots& slots) {
  std::string text;
  for (int i = 0; i < slots.count; ++i) {
    if (i) text += ", ";
    text += slots.chosen[i] >= 0 ? Label(slots.chosen[i]) : "empty";
  }
  return text;
}

std::string FocusList(void* stats) {
  std::string text;
  for (int base_item : rules::NwnMasteryWeapons()) {
    if (!HasFocus(stats, base_item)) continue;
    if (!text.empty()) text += ", ";
    text += Label(base_item);
  }
  return text;
}

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

// The player character to tell about this character: itself, or a henchman's master.
uint32_t PlayerFor(void* creature, const CreatureFacts& facts) {
  return facts.is_player ? IdOf(creature) : Field<uint32_t>(creature, ee::kCreatureMaster);
}

void Announce(void* creature, const CreatureFacts& facts, const std::string& for_player,
              const std::string& for_henchman) {
  TellPlayer(PlayerFor(creature, facts), facts.is_player ? for_player : for_henchman);
}

// The base item of the weapon making the creature's current attack; -1 unarmed or unknown.
int CurrentAttackBaseItem(void* creature) {
  using WeaponFn = void* (*)(void* round, int attack_type);
  static const auto weapon_of = reinterpret_cast<WeaponFn>(EngineExport(ee::kGetCurrentAttackWeapon));
  void* round = creature ? Field<void*>(creature, ee::kCreatureCombatRound) : nullptr;
  const uint8_t* attack = CurrentAttackData(creature);
  if (!round || !attack || !weapon_of) return -1;
  return engine::BaseItemRow(weapon_of(round, attack[ee::kAttackType]));
}

void OnRest(void* stats) {
  if (!Faithful()) return;
  void* creature = CreatureOf(stats);
  if (!creature) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  const Slots slots = ReadSlots(creature, stats);
  if (slots.count == 0 || slots.swap_ready) return;
  engine::SetLocalInt(creature, kSwapVar, 1);
  if (g_config.log_attacks) Log("mastery: object 0x%08X rested: one swap available", IdOf(creature));
  if (facts.is_player && rules::FreeMasterySlot(slots.chosen, slots.count) < 0) {
    TellPlayer(IdOf(creature), "nwn5e: rested - you can swap one weapon mastery: !mastery <old weapon> for <new weapon>");
  }
}

std::vector<std::string> Status(void* creature, void* stats) {
  const Slots slots = ReadSlots(creature, stats);
  const std::string focus = FocusList(stats);
  std::vector<std::string> lines;
  if (slots.count == 0) {
    lines.push_back("nwn5e: your classes have no Weapon Mastery (Barbarian, Fighter, Paladin, Ranger and Rogue do)." +
                    (focus.empty() ? std::string() : " Weapon Focus gives you: " + focus + "."));
    return lines;
  }
  lines.push_back("nwn5e: weapon mastery, " + std::to_string(UsedSlots(slots)) + " of " + std::to_string(slots.count) +
                  " slots: " + SlotList(slots) + "." + (focus.empty() ? "" : " Weapon Focus: " + focus + "."));
  lines.push_back(std::string("nwn5e: !mastery <weapon> fills an empty slot (empty slots also fill with the weapons "
                              "you fight with); !mastery <old> for <new> swaps one after each long rest (") +
                  (slots.swap_ready ? "swap ready)." : "no swap until you rest)."));
  return lines;
}

std::string UnknownWeapon(const std::string& text) {
  return "nwn5e: unknown weapon \"" + text + "\" (e.g. longsword, short sword, dagger, shortbow, greataxe).";
}

std::string AlreadyMastered(void* stats, const Slots& slots, int base_item) {
  if (rules::MasterySlotOf(slots.chosen, slots.count, base_item) >= 0 || HasFocus(stats, base_item)) {
    return "nwn5e: you already have mastery with " + Label(base_item) + ".";
  }
  return std::string();
}

// "!mastery <weapon>" fills an empty slot; "!mastery <A> for <B>" swaps one
// slot after a long rest, in either word order (the one already in a slot is
// given up).
std::string Choose(void* creature, void* stats, const std::string& args) {
  std::string lower = args;
  for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  const size_t split = lower.find(" for ");
  const std::string first = split == std::string::npos ? args : args.substr(0, split);
  const int first_item = rules::NwnWeaponFromName(first);
  if (first_item < 0) return UnknownWeapon(first);
  const Slots slots = ReadSlots(creature, stats);
  if (slots.count == 0) return Status(creature, stats)[0];
  const uint32_t id = IdOf(creature);
  if (split == std::string::npos) {
    if (!Proficient(stats, first_item)) return "nwn5e: you are not proficient with " + Label(first_item) + ".";
    const std::string already = AlreadyMastered(stats, slots, first_item);
    if (!already.empty()) return already;
    const int free_slot = rules::FreeMasterySlot(slots.chosen, slots.count);
    if (free_slot < 0) {
      return "nwn5e: all " + std::to_string(slots.count) + " slots are used (" + SlotList(slots) +
             "). After a long rest you can swap one: !mastery <weapon> for " + rules::NwnWeaponName(first_item) + ".";
    }
    engine::SetLocalInt(creature, kSlotVars[free_slot], first_item + 1);
    if (g_config.log_attacks) Log("mastery: object 0x%08X slot %d = %s (chosen)", id, free_slot + 1, Label(first_item).c_str());
    return "nwn5e: weapon mastery with " + Label(first_item) + " - slot " + std::to_string(free_slot + 1) + " of " +
           std::to_string(slots.count) + ".";
  }
  const std::string second = args.substr(split + 5);
  const int second_item = rules::NwnWeaponFromName(second);
  if (second_item < 0) return UnknownWeapon(second);
  rules::MasterySwap swap = {-1, -1};
  if (!rules::ResolveMasterySwap(slots.chosen, slots.count, first_item, second_item, &swap)) {
    return "nwn5e: to swap, name one weapon in your slots and one that is not (your slots: " + SlotList(slots) + ").";
  }
  if (!Proficient(stats, swap.take)) return "nwn5e: you are not proficient with " + Label(swap.take) + ".";
  if (HasFocus(stats, swap.take)) return AlreadyMastered(stats, slots, swap.take);
  if (!slots.swap_ready) return "nwn5e: you can swap one weapon mastery after each long rest - rest first.";
  const int slot = rules::MasterySlotOf(slots.chosen, slots.count, swap.give_up);
  engine::SetLocalInt(creature, kSlotVars[slot], swap.take + 1);
  engine::SetLocalInt(creature, kSwapVar, 0);
  if (g_config.log_attacks) {
    Log("mastery: object 0x%08X slot %d = %s instead of %s (swap)", id, slot + 1, Label(swap.take).c_str(),
        Label(swap.give_up).c_str());
  }
  return "nwn5e: weapon mastery with " + Label(swap.take) + " instead of " + Label(swap.give_up) + ".";
}

std::vector<std::string> MasteryCommand(void* creature, const std::string& args) {
  if (!MasteryOn()) return {"nwn5e: weapon mastery is off (weapon_mastery=off in nwn5e.ini)."};
  void* stats = StatsOf(creature);
  if (!stats) return {};
  if (!Faithful()) {
    const std::string focus = FocusList(stats);
    return {std::string("nwn5e: weapon mastery (NWN mode): ") +
            (MasteryClass(stats) ? "every weapon you are proficient with." : "your classes have none.") +
            (focus.empty() ? "" : " Weapon Focus: " + focus + ".")};
  }
  if (args.empty()) return Status(creature, stats);
  return {Choose(creature, stats, args)};
}

// Fills an empty slot with the weapon kind a character fights with, and says
// so in chat (faithful).
void NoteWeaponUsed(void* creature, int base_item) {
  if (!Faithful() || !creature) return;
  void* stats = StatsOf(creature);
  if (!stats) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  const int kind = base_item < 0 ? -1 : rules::MasteryKindOfNwnBaseItem(base_item);
  if (kind < 0 || !rules::NwnWeaponName(kind)) return;
  const Slots slots = ReadSlots(creature, stats);
  if (g_config.log_attacks && g_logged_slots.First(LogOnce::Key(IdOf(creature), slots.count, UsedSlots(slots)))) {
    Log("mastery: object 0x%08X %s: %d slots: %s", IdOf(creature), KindOf(facts), slots.count, SlotList(slots).c_str());
  }
  if (slots.count == 0 || rules::MasterySlotOf(slots.chosen, slots.count, kind) >= 0 || HasFocus(stats, kind)) return;
  const int free_slot = rules::FreeMasterySlot(slots.chosen, slots.count);
  if (free_slot < 0 || !Proficient(stats, base_item)) return;
  engine::SetLocalInt(creature, kSlotVars[free_slot], kind + 1);
  if (g_config.log_attacks) {
    Log("mastery: object 0x%08X slot %d = %s (used in a fight)", IdOf(creature), free_slot + 1, Label(kind).c_str());
  }
  const std::string where = " - slot " + std::to_string(free_slot + 1) + " of " + std::to_string(slots.count) + ".";
  Announce(creature, facts, "nwn5e: you now have weapon mastery with " + Label(kind) + where + " Change it with !mastery.",
           "nwn5e: your henchman now has weapon mastery with " + Label(kind) + where);
}

// --- D4 ---------------------------------------------------------------------

using EndCombatRoundFn = void (*)(void* round);
using OnApplyDamageFn = int (*)(void* handler, void* object, void* effect, int loading);
using AddCleaveAttackFn = void (*)(void* round, uint32_t target, int great_cleave);
using MaxAttackRangeFn = float (*)(void* creature, uint32_t target, int a, int b);
using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);

EndCombatRoundFn g_end_combat_round = nullptr;
OnApplyDamageFn g_on_apply_damage = nullptr;

struct VexMark {
  uint32_t target;
  uint32_t attacker_rounds;  // the attacker's ended rounds when it hit
  DWORD tick;
};
struct SapMark {
  uint32_t source;
  uint32_t source_rounds;
  DWORD tick;
};
struct AwaitDamage {
  uint32_t target;
  DWORD tick;
};
struct CleavePending {
  uint32_t target;
  uint32_t attacker_rounds;
  DWORD tick;
  bool rolled = false;        // the extra attack's roll has happened
  int attack_index = -1;      // its slot in the round, for the damage adjustment
  uint32_t first_target = 0;  // the round's target to go back to after it
};

// All on the engine's thread (combat is single-threaded, as on 1.69).
std::unordered_map<uint32_t, uint32_t> g_rounds_ended;      // by creature id
std::unordered_map<uint32_t, VexMark> g_vex;                // by attacker id
std::unordered_map<uint32_t, SapMark> g_sap;                // by sapped creature id
std::unordered_map<uint32_t, DWORD> g_slowed;               // Slow / Topple: last applied, by target id
std::unordered_map<uint32_t, DWORD> g_toppled;
std::unordered_map<uint32_t, AwaitDamage> g_slow_awaiting;  // Slow applies once the damage lands
std::unordered_map<uint32_t, AwaitDamage> g_vex_awaiting;   // Vex's mark is withdrawn if it dealt none
std::unordered_map<uint32_t, uint32_t> g_cleave_used;       // attacker -> its ended rounds + 1 when it cleaved
std::unordered_map<uint32_t, CleavePending> g_cleave_pending;
thread_local int t_attack_weapon = -1;  // the weapon read before the attack roll

constexpr DWORD kAwaitDamageMs = 5000;
constexpr DWORD kRoundMs = 6000;
constexpr DWORD kMarkMaxAgeMs = 30000;  // rounds only end in combat; a mark from an old fight lapses
constexpr float kRoundSeconds = 6.0f;
constexpr float kSwingDelay = 0.5f;     // effects land with the swing
constexpr float kCleaveSplashMetres = 1.524f;  // 5 ft
int g_effect_log_lines = 0;

bool EffectLogSlot() { return g_config.log_attacks && g_effect_log_lines < 1000 && ++g_effect_log_lines; }

uint32_t RoundsEnded(uint32_t id) {
  const auto it = g_rounds_ended.find(id);
  return it == g_rounds_ended.end() ? 0 : it->second;
}

bool RecentlyApplied(std::unordered_map<uint32_t, DWORD>* applied, uint32_t target_id, DWORD now) {
  const auto it = applied->find(target_id);
  if (it != applied->end() && now - it->second < kRoundMs) return true;
  (*applied)[target_id] = now;
  return false;
}

void EndCombatRound(void* round) {
  void* owner = round ? Field<void*>(round, ee::kRoundOwner) : nullptr;
  if (owner) {
    ++g_rounds_ended[IdOf(owner)];
  }
  g_end_combat_round(round);
  if (owner) PushOnRoundEnd(owner);
}

// The attack's ability modifier (Graze, Topple).
int AttackAbility(void* stats, const uint8_t* attack, int base_item) {
  const bool ranged = Field<int32_t>(attack, ee::kAttackRanged) != 0;
  return rules::AttackAbilityModifier(ranged, HasFeat(stats, ee::kFeatWeaponFinesse),
                                      rules::IsFinesseNwnBaseItem(base_item), engine::StrengthModifier(stats),
                                      engine::DexterityModifier(stats));
}

// The nearest other enemy within 5 ft of the first target and in the attacker's reach.
struct CleaveSearch {
  void* attacker;
  void* first;
  uint32_t attacker_id;
  float reach;
  uint32_t best;
  float best_distance;
};

void ConsiderCleave(void* context, uint32_t id, void* creature) {
  auto* s = static_cast<CleaveSearch*>(context);
  if (id == s->attacker_id || !engine::IsEnemy(s->attacker, id) || engine::IsDeadOrDying(creature)) return;
  if (engine::EdgeDistance(s->first, creature) > kCleaveSplashMetres) return;
  const float distance = engine::EdgeDistance(s->attacker, creature);
  if (!(distance < s->reach) || distance >= s->best_distance) return;
  s->best = id;
  s->best_distance = distance;
}

uint32_t CleaveTarget(void* attacker, void* first, uint32_t attacker_id) {
  static const auto max_range = reinterpret_cast<MaxAttackRangeFn>(EngineExport(ee::kMaxAttackRange));
  CleaveSearch search{attacker, first, attacker_id,
                      max_range ? max_range(attacker, ee::kObjectInvalid, 0, 1) : 2.5f, ee::kObjectInvalid, 1.0e9f};
  engine::ForEachOtherCreatureInArea(first, &ConsiderCleave, &search);
  return search.best;
}

// The round's target goes back to the first target after the extra attack (if
// that one still stands), so the round's other attacks stay on it.
void RestoreRoundTarget(void* attacker, void* round, uint32_t first_target) {
  void* first = CreatureById(first_target);
  if (!first || engine::IsDeadOrDying(first)) return;
  SetField<uint32_t>(round, ee::kRoundTarget, first_target);
  SetField<int32_t>(attacker, ee::kCreatureTargetSwitched, 1);
}

void BeforeAttackRoll(AttackRoll& roll) {
  t_attack_weapon = CurrentAttackBaseItem(roll.attacker);
  NoteWeaponUsed(roll.attacker, t_attack_weapon);
}

void AfterAttackRoll(AttackRoll& roll) {
  const int base_item = t_attack_weapon;
  t_attack_weapon = -1;
  if (base_item < 0 || !roll.data) return;
  const rules::Mastery mastery = rules::MasteryForNwnBaseItem(base_item);
  // Push: faithful only (D4: NWN mode is the same without Push).
  const bool push = mastery == rules::Mastery::kPush && Faithful();
  if (mastery != rules::Mastery::kVex && mastery != rules::Mastery::kSap && mastery != rules::Mastery::kGraze &&
      mastery != rules::Mastery::kSlow && mastery != rules::Mastery::kTopple && mastery != rules::Mastery::kCleave &&
      !push) {
    return;
  }
  void* attacker = roll.attacker;
  void* round = Field<void*>(attacker, ee::kCreatureCombatRound);
  if (!round) return;
  const int result = roll.data[ee::kAttackResult];
  const bool hit = rules::MasteryAttackHit(result);
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t rolled_at = Field<uint32_t>(roll.target, ee::kObjectId);
  // The first roll at a pending Cleave target, in the same round, is the
  // extra attack Cleave added; a round that ended without it lost it.
  {
    const auto pending = g_cleave_pending.find(attacker_id);
    if (pending != g_cleave_pending.end() && !pending->second.rolled) {
      if (RoundsEnded(attacker_id) != pending->second.attacker_rounds) {
        g_cleave_pending.erase(pending);
      } else if (pending->second.target == rolled_at) {
        pending->second.rolled = true;
        pending->second.attack_index = Field<uint8_t>(round, ee::kRoundCurrentAttack);
        RestoreRoundTarget(attacker, round, pending->second.first_target);
        if (EffectLogSlot()) {
          Log("mastery: 0x%08X Cleave extra attack at 0x%08X: %s (result %d)", attacker_id, rolled_at,
              hit ? "hit" : "miss", result);
        }
        if (!hit) g_cleave_pending.erase(pending);
        return;  // the extra attack itself does not cleave again
      }
    }
  }
  if (mastery == rules::Mastery::kGraze ? !rules::MasteryAttackMiss(result) : !hit) return;
  void* target = CreatureById(rolled_at);  // creatures only (not doors / placeables)
  if (!target || !HasWeaponMasteryWith(attacker, base_item)) return;
  void* stats = StatsOf(attacker);
  const uint32_t rounds = RoundsEnded(attacker_id);
  const DWORD now = GetTickCount();
  std::string what;
  switch (mastery) {
    case rules::Mastery::kVex:
      g_vex[attacker_id] = VexMark{rolled_at, rounds, now};
      g_vex_awaiting[attacker_id] = AwaitDamage{rolled_at, now};
      what = "Vex (advantage on its next attack at that target, if the hit deals damage)";
      break;
    case rules::Mastery::kSap:
      g_sap[rolled_at] = SapMark{attacker_id, rounds, now};
      what = "Sap (target's next attack has disadvantage)";
      break;
    case rules::Mastery::kGraze: {
      const int damage = rules::GrazeDamage(AttackAbility(stats, roll.data, base_item));
      if (damage <= 0) return;
      char effect[64];
      sprintf_s(effect, "EffectDamage(%d, DAMAGE_TYPE_SLASHING)", damage);
      ApplyEffectAs(attacker_id, rolled_at, "DURATION_TYPE_INSTANT", effect, 0.0f, kSwingDelay);
      what = "Graze: " + std::to_string(damage) + " slashing damage on a miss";
      break;
    }
    case rules::Mastery::kSlow:
      g_slow_awaiting[attacker_id] = AwaitDamage{rolled_at, now};  // applied when the damage lands
      what = "Slow: waiting for the hit's damage";
      break;
    case rules::Mastery::kTopple: {
      if (RecentlyApplied(&g_toppled, rolled_at, now)) return;
      static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));
      if (!save_roll) return;
      const CreatureFacts facts = ReadCreatureFacts(stats);
      const int dc = rules::ToppleDc(AttackAbility(stats, roll.data, base_item),
                                     rules::ProficiencyBonusForCreature(facts.character, facts.level,
                                                                        facts.challenge_rating));
      RememberSaveDc2024(attacker_id, dc);  // already 2024: the save roll must not convert it (C6)
      const uint8_t save = save_roll(target, ee::kSaveFortitude, static_cast<uint16_t>(dc), 0, attacker_id, 1, 0, 1);
      if (save == 0) {
        ApplyEffectAs(attacker_id, rolled_at, "DURATION_TYPE_TEMPORARY", "EffectKnockdown()", kRoundSeconds,
                        kSwingDelay);
      }
      what = "Topple: Constitution save DC " + std::to_string(dc) +
             (save == 0 ? " failed, knocked down for a round" : save == 2 ? " - immune" : " made");
      break;
    }
    case rules::Mastery::kPush:
      what = PushOnHit(attacker, target);  // rules_push.cpp
      if (what.empty()) return;
      break;
    case rules::Mastery::kCleave: {
      if (Field<int32_t>(roll.data, ee::kAttackRanged) != 0) return;  // melee attack rolls only
      const auto used = g_cleave_used.find(attacker_id);
      if (used != g_cleave_used.end() && used->second == rounds + 1) return;  // once per round
      const uint32_t second = CleaveTarget(attacker, target, attacker_id);
      if (second == ee::kObjectInvalid) return;
      static const auto add_cleave = reinterpret_cast<AddCleaveAttackFn>(EngineExport(ee::kAddCleaveAttack));
      if (!add_cleave) return;
      // As the engine's Great Cleave does: the round's target becomes the
      // second creature, then the cleave attack is queued next.
      SetField<uint32_t>(round, ee::kRoundTarget, second);
      SetField<int32_t>(attacker, ee::kCreatureTargetSwitched, 1);
      add_cleave(round, second, 0);
      g_cleave_used[attacker_id] = rounds + 1;
      CleavePending pending;
      pending.target = second;
      pending.attacker_rounds = rounds;
      pending.tick = now;
      pending.first_target = rolled_at;
      g_cleave_pending[attacker_id] = pending;
      char text[96];
      sprintf_s(text, "Cleave: extra attack at 0x%08X (its damage without Strength)", second);
      what = text;
      break;
    }
    default:
      return;
  }
  if (EffectLogSlot()) {
    Log("mastery: 0x%08X %s 0x%08X with %s: %s", attacker_id, hit ? "hit" : "missed", rolled_at,
        Label(base_item).c_str(), what.c_str());
  }
}

int HitPointsOf(const void* object) {
  return Field<int16_t>(object, ee::kObjectCurrentHitPoints) + Field<int16_t>(object, ee::kObjectTemporaryHitPoints);
}

// A damage effect landed: Slow takes hold if it dealt damage, Vex's mark is
// withdrawn if it dealt none.
void AfterDamage(uint32_t attacker_id, void* target, int damage) {
  const uint32_t target_id = IdOf(target);
  const DWORD now = GetTickCount();
  const auto slow = g_slow_awaiting.find(attacker_id);
  if (slow != g_slow_awaiting.end() && slow->second.target == target_id) {
    const bool fresh = now - slow->second.tick <= kAwaitDamageMs;
    g_slow_awaiting.erase(slow);
    if (fresh && damage > 0 && !RecentlyApplied(&g_slowed, target_id, now)) {  // 2024: no more than 10 ft
      ApplyEffectAs(attacker_id, target_id, "DURATION_TYPE_TEMPORARY", "EffectMovementSpeedDecrease(33)",
                      kRoundSeconds, 0.0f);
      if (EffectLogSlot()) Log("mastery: 0x%08X Slow on 0x%08X: speed -33%% for a round", attacker_id, target_id);
    } else if (fresh && damage <= 0 && EffectLogSlot()) {
      Log("mastery: 0x%08X Slow on 0x%08X: no damage dealt, no Slow", attacker_id, target_id);
    }
  }
  const auto vex = g_vex_awaiting.find(attacker_id);
  if (vex != g_vex_awaiting.end() && vex->second.target == target_id) {
    const bool fresh = now - vex->second.tick <= kAwaitDamageMs;
    g_vex_awaiting.erase(vex);
    const auto mark = g_vex.find(attacker_id);
    if (fresh && damage <= 0 && mark != g_vex.end() && mark->second.target == target_id) {
      g_vex.erase(mark);
      if (EffectLogSlot()) Log("mastery: 0x%08X Vex on 0x%08X withdrawn: no damage dealt", attacker_id, target_id);
    }
  }
}

int OnApplyDamage(void* handler, void* object, void* effect, int loading) {
  const bool awaiting = !g_slow_awaiting.empty() || !g_vex_awaiting.empty() || PushAwaiting();
  void* target = object && awaiting ? CreatureById(Field<uint32_t>(object, ee::kObjectId)) : nullptr;
  const int before = target ? HitPointsOf(target) : 0;
  const int result = g_on_apply_damage(handler, object, effect, loading);
  if (target && effect) {
    AfterDamage(effects::Creator(effect), target, before - HitPointsOf(target));
    PushOnDamage(effects::Creator(effect), target);  // any hit pushes (2024), damage or not
  }
  return result;
}

}  // namespace

bool HasWeaponMasteryWith(void* creature, int base_item) {
  if (!MasteryOn() || !creature) return false;
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).character) return false;
  const int kind = rules::MasteryKindOfNwnBaseItem(base_item);
  if (!rules::NwnWeaponName(kind)) return false;
  if (HasFocus(stats, kind)) return true;
  if (!Faithful()) return MasteryClass(stats) && Proficient(stats, base_item);
  const Slots slots = ReadSlots(creature, stats);
  return rules::MasterySlotOf(slots.chosen, slots.count, kind) >= 0;
}

MasteryRollSources TakeMasteryRollSources(void* attacker, uint32_t target_id) {
  MasteryRollSources sources;
  if (!MasteryOn() || !attacker) return sources;
  const uint32_t attacker_id = IdOf(attacker);
  const DWORD now = GetTickCount();
  const auto vex = g_vex.find(attacker_id);
  if (vex != g_vex.end()) {
    const bool live = now - vex->second.tick <= kMarkMaxAgeMs &&
                      rules::VexActive(RoundsEnded(attacker_id), vex->second.attacker_rounds);
    if (!live) {
      g_vex.erase(vex);
    } else if (vex->second.target == target_id) {
      ++sources.advantage;
      sources.note = "Vex advantage";
      g_vex.erase(vex);
    }
  }
  const auto sap = g_sap.find(attacker_id);
  if (sap != g_sap.end()) {
    if (now - sap->second.tick <= kMarkMaxAgeMs &&
        rules::SapActive(RoundsEnded(sap->second.source), sap->second.source_rounds)) {
      ++sources.disadvantage;
      sources.note += sources.note.empty() ? "Sap disadvantage" : ", Sap disadvantage";
    }
    g_sap.erase(sap);
  }
  return sources;
}

uint32_t CombatRoundsEnded(uint32_t id) {
  return RoundsEnded(id);
}

int MasteryCleaveDamage(void* stats, int bonus, float two_handed_multiplier) {
  if (!MasteryOn() || !stats) return bonus;
  void* creature = CreatureOf(stats);
  if (!creature) return bonus;
  const uint32_t id = IdOf(creature);
  {
    const auto pending = g_cleave_pending.find(id);
    if (pending == g_cleave_pending.end()) return bonus;
    if (GetTickCount() - pending->second.tick > kMarkMaxAgeMs ||
        RoundsEnded(id) > pending->second.attacker_rounds + 1) {
      g_cleave_pending.erase(pending);
      return bonus;
    }
    void* round = Field<void*>(creature, ee::kCreatureCombatRound);
    if (!round || !pending->second.rolled ||
        Field<uint8_t>(round, ee::kRoundCurrentAttack) != pending->second.attack_index) {
      return bonus;
    }
    g_cleave_pending.erase(pending);
  }
  const int strength = engine::StrengthModifier(stats);
  const int nwn_term =
      strength > 0 && two_handed_multiplier > 0.0f ? static_cast<int>(strength * two_handed_multiplier) : strength;
  const int term = rules::OffHandDamageAbilityModifier(strength, false);  // only when negative
  if (EffectLogSlot()) Log("mastery: 0x%08X Cleave attack damage: Strength %+d -> %+d", id, nwn_term, term);
  return bonus - nwn_term + term;
}

void InstallMasteryRules() {
  if (!MasteryOn()) return;
  HookExport("CNWSCombatRound::EndCombatRound (mastery rounds)", ee::kEndCombatRound,
             reinterpret_cast<void*>(&EndCombatRound), reinterpret_cast<void**>(&g_end_combat_round));
  HookExport("CNWSEffectListHandler::OnApplyDamage (Vex / Slow / Push need the hit)", ee::kOnApplyDamage,
             reinterpret_cast<void*>(&OnApplyDamage), reinterpret_cast<void**>(&g_on_apply_damage));
  AddAttackRollListeners(&BeforeAttackRoll, &AfterAttackRoll);
  AddChatCommand("!mastery", &MasteryCommand);
  if (Faithful()) AddRestListener(&OnRest);
  if (Faithful()) InstallPushRules();
}
