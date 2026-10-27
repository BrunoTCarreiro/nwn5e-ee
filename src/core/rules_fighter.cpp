// The Fighter's 2024 features (port of 1.69's rules_fighter.cpp; docs/EE-PORT.md
// E4c), faithful (class_features), by Fighter level — prestige-class levels
// count as Fighter levels (decision 10, ClassLevel2024):
// - C1 Champion's Improved / Superior Critical: weapon and unarmed attacks
//   crit on 19-20 from Fighter 3, 18-20 from 15 (GetCriticalHitRoll,
//   rules_crit.cpp); as decision 8 says, a 19 that misses still misses;
// - C2 Remarkable Athlete (Champion 3): advantage on initiative
//   (rules_initiative.cpp);
// - F6 Studied Attacks (13): a miss marks the target; the Fighter's next
//   attack at it before the end of its next round has advantage
//   (rules_advantage.cpp asks, like Vex);
// - F1 Fighting Styles: Archery +2 to ranged attack rolls (rules_attack.cpp,
//   the sheet in rules_sheet.cpp); Defense +1 AC in armour (rules_attack.cpp
//   on attacks, CNWSCreature::GetArmorClass here for the sheet and GetAC);
//   Great Weapon Fighting (here); Two-Weapon Fighting (rules_two_weapon.cpp);
// - F5 Indomitable (rules_save_roll.cpp asks IndomitableReroll) and C5
//   Heroic Rally (here);
// - F2 Second Wind: its feat rows (data\rows) are granted by
//   rules_features.cpp; the spell row runs nwn5e_secwind, served compiled
//   (data\files, EP6's interim); from Fighter 5 the script sets
//   nwn5e_tactical_shift, which rules_opportunity.cpp reads (F7).
// NWN mode and off: the Fighting Style feats are hidden.
//
// The weapon-dice filter also serves the Savage Attacker Origin feat (feats
// faithful, rules_origin_feats.cpp: once per turn, the weapon's damage dice
// rolled twice, the higher roll kept; a critical hit's sets - one per
// multiplier, GetDamageRoll's critical loop - are one roll, rolled twice
// whole at the first ask), so its GetDamageRoll hook is installed in either
// mode.
//
// Studied Attacks and Heroic Rally count combat rounds (CountCombatRounds,
// rules_mastery.cpp), hooked here too, so they work with weapon_mastery=off.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "rest.h"
#include "rules/class_features.h"
#include "rules/feats2024.h"
#include "rules/weapons.h"
#include "records.h"
#include "rules_loaded.h"
#include "weapon_size.h"

// Savage Attacker (rules_origin_feats.cpp): true once per turn for a
// character who has it and hits with a weapon now.
bool TakeSavageAttacker(void* stats);
// Brutal Strike's d10s and Frenzy's d6s (rules_barbarian_strikes.cpp, Barbarian B10 / BZ1).
int BarbarianExtraDamage(void* stats, void* target, int offhand, int critical);

namespace {

using ComputeArmorClassFn = int (*)(void* item);
using CurrentAttackWeaponFn = void* (*)(void* round, int attack_type);
using MaxHitPointsFn = int16_t (*)(void* creature, int include_toughness);
using CreatureArmorClassFn = int16_t (*)(void* creature);
using DamageRollFn = int (*)(void* stats, void* target, int offhand, int critical, int sneak, int death,
                             int force_max);

ComputeArmorClassFn g_compute_armor_class = nullptr;
CurrentAttackWeaponFn g_current_attack_weapon = nullptr;
MaxHitPointsFn g_get_max_hit_points = nullptr;
CreatureArmorClassFn g_creature_armor_class = nullptr;
DamageRollFn g_get_damage_roll = nullptr;

LogOnce g_logged(500);

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

// Fighter levels of a character (player character or henchman), else 0.
int FighterLevelOf(void* stats) {
  if (!stats || !Faithful()) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return 0;
  return ClassLevel2024(stats, rules::CharacterClass::kFighter);
}

bool HasStyle(void* stats, int feat) {
  if (!stats || !Faithful()) return false;
  return ReadCreatureFacts(stats).character && engine::HasFeat(stats, feat);
}

bool CurrentAttackRanged(void* stats) {
  const uint8_t* data = CurrentAttackData(CreatureOf(stats));
  return data && Field<int32_t>(data, ee::kAttackRanged) != 0;
}

// --- F6 Studied Attacks -------------------------------------------------------

struct StudiedMark {
  uint32_t target;
  uint32_t rounds;  // the Fighter's ended rounds when it missed
  DWORD tick;
};
std::unordered_map<uint32_t, StudiedMark> g_studied;  // by attacker id
constexpr DWORD kMarkMaxAgeMs = 30000;                // rounds only end in combat; an old fight's mark lapses

// After an attack roll: a miss marks the target.
void StudiedAttacksAfterAttackRoll(AttackRoll& roll) {
  if (!roll.attacker || !roll.target || !roll.data || !Faithful()) return;
  if (!rules::MasteryAttackMiss(Field<uint8_t>(roll.data, ee::kAttackResult))) return;
  if (!rules::HasStudiedAttacks(FighterLevelOf(StatsOf(roll.attacker)))) return;
  const uint32_t target_id = IdOf(roll.target);
  if (!CreatureById(target_id)) return;  // creatures only
  const uint32_t attacker_id = IdOf(roll.attacker);
  g_studied[attacker_id] = StudiedMark{target_id, CombatRoundsEnded(attacker_id), GetTickCount()};
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(attacker_id, static_cast<int>(target_id), 2))) {
    Log("fighter: 0x%08X missed 0x%08X - Studied Attacks: advantage on its next attack at it (logged once per "
        "target)",
        attacker_id, target_id);
  }
}

// --- F1 Defense, Great Weapon Fighting ------------------------------------------

bool WearingArmor(void* creature) {
  void* chest = engine::ItemInSlot(creature, ee::kSlotChest);
  return chest && g_compute_armor_class && g_compute_armor_class(chest) > 0;
}

}  // namespace

int UnarmoredDefenseBonus(void* stats);  // rules_barbarian.cpp (Barbarian B4), on the sheet too
int ArmorBaseAdjustment(void* stats);    // rules_armor.cpp (2024 armour), on the sheet too

// Defense: +1 AC in armour (GetArmorClassVersus, rules_attack.cpp, and the sheet).
int DefenseArmorClassBonus(void* stats) {
  if (!HasStyle(stats, rules::kFeatFightingStyleDefense)) return 0;
  void* creature = CreatureOf(stats);
  if (!creature || !WearingArmor(creature)) return 0;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), 0, 5))) {
    Log("fighter: 0x%08X Defense: +%d AC in armour", ObjectIdOf(stats), rules::kDefenseArmorClassBonus);
  }
  return rules::kDefenseArmorClassBonus;
}

namespace {

int16_t CreatureArmorClass(void* creature) {
  const int16_t armor_class = g_creature_armor_class(creature);
  void* stats = StatsOf(creature);
  return static_cast<int16_t>(armor_class + DefenseArmorClassBonus(stats) + UnarmoredDefenseBonus(stats) +
                              ArmorBaseAdjustment(stats));
}

// The sheet with class features in NWN mode and 2024 armour on: the armour only.
int16_t CreatureArmorClassArmorOnly(void* creature) {
  return static_cast<int16_t>(g_creature_armor_class(creature) + ArmorBaseAdjustment(StatsOf(creature)));
}

// Great Weapon Fighting: the weapon's damage dice for a melee attack with a
// two-handed or versatile weapon held in two hands are rolled one by one, a 1
// or 2 counting as 3 — GetDamageRoll's two weapon-dice RollDice calls
// (ee_layout.h), while GetDamageRoll runs for such an attack.
constexpr int kMaxSavageSets = 8;  // weapon-dice sets one hit can roll (a critical multiplier)

struct GreatWeaponRoll {
  bool active = false;  // Great Weapon Fighting
  bool savage = false;  // Savage Attacker: the hit's dice twice, the higher roll kept
  uint32_t id = 0;
  int count = 0;
  int sides = 0;
  int sets = 0;                     // Savage Attacker: the weapon-dice sets this hit rolls
  int next = 0;                     // the next set the engine asks for
  int kept[kMaxSavageSets] = {};    // the kept roll, set by set (rolled at the first ask)
};
thread_local GreatWeaponRoll t_great_weapon;
std::vector<uintptr_t> g_weapon_dice_returns;

// The weapon's base item when Great Weapon Fighting applies to this damage roll, else null.
const void* GreatWeaponBase(void* stats, int offhand) {
  if (offhand || !HasStyle(stats, rules::kFeatFightingStyleGreatWeapon) || CurrentAttackRanged(stats)) {
    return nullptr;
  }
  void* creature = CreatureOf(stats);
  void* round = creature ? Field<void*>(creature, ee::kCreatureCombatRound) : nullptr;
  void* item = round && g_current_attack_weapon ? g_current_attack_weapon(round, 0) : nullptr;
  const int row = engine::BaseItemRow(item);
  const bool two_handed = rules::IsTwoHandedWeapon2024(row), versatile = rules::IsVersatileWeapon2024(row);
  if (!two_handed && !versatile) return nullptr;
  const void* base = engine::BaseItem(row);
  if (!base || Field<uint8_t>(base, ee::kBaseItemWield) == ee::kWieldDouble) return nullptr;
  const int larger_than_wielder = Field<uint8_t>(base, ee::kBaseItemSize) - WeaponWielderSize(creature);
  if (larger_than_wielder >= 1) return base;  // wielded two-handed (any size larger: the size rule)
  // Otherwise in two hands = nothing in the off hand, and a two-handed weapon
  // or a versatile one of the wielder's own size.
  if (engine::ItemInSlot(creature, ee::kSlotLeftHand)) return nullptr;
  return two_handed || larger_than_wielder == 0 ? base : nullptr;
}

int GetDamageRoll(void* stats, void* target, int offhand, int critical, int sneak, int death, int force_max) {
  const GreatWeaponRoll outer = t_great_weapon;
  t_great_weapon = GreatWeaponRoll();
  if (const void* base = Faithful() ? GreatWeaponBase(stats, offhand) : nullptr) {
    t_great_weapon.active = true;
    t_great_weapon.id = ObjectIdOf(stats);
    t_great_weapon.count = Field<uint8_t>(base, ee::kBaseItemNumDice);
    t_great_weapon.sides = Field<uint8_t>(base, ee::kBaseItemDieToRoll);
  }
  void* creature = CreatureOf(stats);
  const void* weapon = engine::BaseItem(creature ? AttackWeaponBaseItem(creature) : -1);
  if (weapon && TakeSavageAttacker(stats)) {
    t_great_weapon.savage = true;
    t_great_weapon.id = ObjectIdOf(stats);
    t_great_weapon.count = Field<uint8_t>(weapon, ee::kBaseItemNumDice);
    t_great_weapon.sides = Field<uint8_t>(weapon, ee::kBaseItemDieToRoll);
    // A critical hit rolls the weapon's dice once per multiplier (the engine's
    // own count, asked through the export so rules_crit's answer applies).
    int sets = 1;
    if (critical) {
      using MultiplierFn = int (*)(void* stats, int offhand);
      static const auto multiplier = reinterpret_cast<MultiplierFn>(EngineExport(ee::kGetCriticalHitMultiplier));
      sets = multiplier ? multiplier(stats, offhand) : 2;
    }
    t_great_weapon.sets = (std::max)(0, (std::min)(sets, kMaxSavageSets));
  }
  const int damage = g_get_damage_roll(stats, target, offhand, critical, sneak, death, force_max);
  t_great_weapon = outer;
  return damage + BarbarianExtraDamage(stats, target, offhand, critical);
}

bool GreatWeaponDice(uintptr_t caller, int count, int sides, int* result) {
  if ((!t_great_weapon.active && !t_great_weapon.savage) || count != t_great_weapon.count ||
      sides != t_great_weapon.sides) {
    return false;
  }
  if (std::find(g_weapon_dice_returns.begin(), g_weapon_dice_returns.end(), caller) == g_weapon_dice_returns.end()) {
    return false;
  }
  int raised = 0;
  auto roll_once = [&]() {
    if (!t_great_weapon.active) return RollEngineDice(count, sides);
    int total = 0;
    for (int i = 0; i < count; ++i) {
      const int roll = RollEngineDice(1, sides);
      raised += roll < 3 ? 1 : 0;
      total += rules::GreatWeaponFightingDie(roll);
    }
    return total;
  };
  int total = 0;
  if (t_great_weapon.savage && t_great_weapon.next == 0 && t_great_weapon.sets > 0) {
    // The first ask of the hit: its whole roll (every set), twice.
    std::vector<int> first, second;
    for (int i = 0; i < t_great_weapon.sets; ++i) first.push_back(roll_once());
    for (int i = 0; i < t_great_weapon.sets; ++i) second.push_back(roll_once());
    const bool keep_second = rules::KeepSecondSavageRoll(first, second);
    const std::vector<int>& kept = keep_second ? second : first;
    int first_total = 0, second_total = 0;
    for (int i = 0; i < t_great_weapon.sets; ++i) {
      t_great_weapon.kept[i] = kept[i];
      first_total += first[i];
      second_total += second[i];
    }
    if (g_config.log_attacks) {
      Log("feats: 0x%08X Savage Attacker: %dd%d%s rolled twice, %d / %d -> %d", t_great_weapon.id,
          count * t_great_weapon.sets, sides, t_great_weapon.sets > 1 ? " (critical hit)" : "", first_total,
          second_total, keep_second ? second_total : first_total);
    }
  }
  if (t_great_weapon.savage && t_great_weapon.next < t_great_weapon.sets) {
    total = t_great_weapon.kept[t_great_weapon.next++];
  } else {
    total = roll_once();  // not Savage Attacker's, or more sets asked than the hit was to roll
  }
  if (g_config.log_attacks && raised &&
      g_logged.First(LogOnce::Key(t_great_weapon.id, count * 100 + sides, 4))) {
    Log("fighter: 0x%08X Great Weapon Fighting: %dd%d, %d die(s) of 1-2 counted as 3 (logged once per dice)",
        t_great_weapon.id, count, sides, raised);
  }
  *result = total;
  return true;
}

// The weapon-dice calls: of GetDamageRoll's RollDice calls, the first three
// are the critical loop's, Overwhelming Critical's (a ruleset read right
// before it) and the plain roll's; the 1st and 3rd are the weapon's.
bool FindWeaponDiceCalls() {
  const CodeRange code = FunctionAt(EngineExport(ee::kGetDamageRoll));
  const std::vector<uintptr_t> dice = FindCallsTo(code, EngineExport(ee::kRollDice));
  const std::vector<uintptr_t> ruleset = FindCallsTo(code, EngineExport(ee::kGetRulesetIntEntry));
  auto after_ruleset_read = [&](uintptr_t site) {
    for (uintptr_t read : ruleset) {
      if (read < site && site - read <= 0x20) return true;
    }
    return false;
  };
  if (dice.size() != ee::kDamageRollRollDiceCalls || after_ruleset_read(dice[0]) || !after_ruleset_read(dice[1]) ||
      after_ruleset_read(dice[2])) {
    Log("SKIP fighter: Great Weapon Fighting - GetDamageRoll's weapon dice not found (%zu RollDice calls)",
        dice.size());
    return false;
  }
  g_weapon_dice_returns = {dice[0] + kCallLength, dice[2] + kCallLength};
  Log("fighter: Great Weapon Fighting at GetDamageRoll +0x%llx / +0x%llx",
      static_cast<unsigned long long>(dice[0] - code.begin), static_cast<unsigned long long>(dice[2] - code.begin));
  return true;
}

// --- F5 Indomitable, C5 Heroic Rally -------------------------------------------
// Indomitable: a failed save is rolled again with the Fighter level added,
// 1 / 2 / 3 times per long rest — kept in nwn5e_indomitable_used, cleared when
// the engine restores feat uses after a rest. Heroic Rally (Champion 18): once
// per combat round, Bloodied and above 0 HP, regain 5 + Con modifier (healed
// through the script bridge).

constexpr const char* kVarIndomitableUsed = "nwn5e_indomitable_used";
std::unordered_map<uint32_t, uint32_t> g_rally_round;  // by creature id: the round count last checked

void ResetIndomitable(void* stats) {
  void* creature = CreatureOf(stats);
  if (creature && engine::GetLocalInt(creature, kVarIndomitableUsed)) {
    engine::SetLocalInt(creature, kVarIndomitableUsed, 0);
    if (g_config.log_attacks) Log("fighter: 0x%08X rested: Indomitable uses back", IdOf(creature));
  }
}

void HeroicRally(void* creature) {
  if (!Faithful() || !g_get_max_hit_points) return;
  const uint32_t id = IdOf(creature);
  const uint32_t rounds = CombatRoundsEnded(id);
  if (rounds == 0) return;  // never in a fight
  uint32_t& last = g_rally_round[id];
  if (last == rounds) return;  // already this round
  last = rounds;
  void* stats = StatsOf(creature);
  if (!rules::HasHeroicRally(FighterLevelOf(stats))) return;
  const int hit_points = Field<int16_t>(creature, ee::kObjectCurrentHitPoints);
  const int max_hit_points = g_get_max_hit_points(creature, 1);
  if (hit_points < 1 || !rules::IsBloodied(hit_points, max_hit_points)) return;
  const int healed = std::min(max_hit_points - hit_points,
                              rules::HeroicRallyHitPoints(engine::AbilityModifier(stats, 2)));
  if (healed <= 0) return;
  ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", ("EffectHeal(" + std::to_string(healed) + ")").c_str(), 0.0f, 0.0f);
  Log("fighter: 0x%08X Heroic Rally: %d -> %d HP (max %d)", id, hit_points, hit_points + healed, max_hit_points);
  if (ReadCreatureFacts(stats).is_player) {
    TellPlayer(id, "nwn5e: Heroic Rally - you regain " + std::to_string(healed) + " hit points.");
  }
}

// --- NWN mode: the Fighting Style rows hidden -----------------------------------

void HideFightingStyleRows(void* rules_object, bool reload) {
  auto* feats = Field<uint8_t*>(rules_object, ee::kRulesFeats);
  const int count = Field<uint16_t>(rules_object, ee::kRulesFeatCount);
  if (!feats) return;
  int hidden = 0;
  for (int id = rules::kFeatFightingStyleArchery; id <= rules::kFeatFightingStyleTwoWeapon && id < count; ++id) {
    SetField<uint8_t>(feats + id * ee::kFeatStride, ee::kFeatMinLevel, 0xFF);
    ++hidden;
  }
  if (!reload) {
    Log("fighter: %s - the Fighting Style feats hidden (%d)",
        g_config.class_features == rules::RuleVariant::kNwn ? "NWN mode" : "off", hidden);
  }
}

}  // namespace

// C1: the lowest d20 that crits for this creature's weapon attacks (20 = no change).
int ChampionCriticalFloorFor(void* stats) {
  const int floor = rules::ChampionCriticalFloor(FighterLevelOf(stats));
  if (floor < 20 && g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), floor, 1))) {
    Log("fighter: 0x%08X Champion crits on %d-20", ObjectIdOf(stats), floor);
  }
  return floor;
}

// C2: advantage on initiative.
bool HasRemarkableAthleteAdvantage(void* creature) {
  return creature && rules::HasRemarkableAthlete(FighterLevelOf(StatsOf(creature)));
}

// F6: advantage from an earlier miss at this target, used up by this attack.
MasteryRollSources TakeStudiedAttacks(void* attacker, uint32_t target_id) {
  MasteryRollSources sources;
  if (!attacker || !Faithful() || g_studied.empty()) return sources;
  const uint32_t attacker_id = IdOf(attacker);
  const auto mark = g_studied.find(attacker_id);
  if (mark == g_studied.end()) return sources;
  const bool live = GetTickCount() - mark->second.tick <= kMarkMaxAgeMs &&
                    rules::VexActive(CombatRoundsEnded(attacker_id), mark->second.rounds);
  if (!live) {
    g_studied.erase(mark);
  } else if (mark->second.target == target_id) {
    ++sources.advantage;
    sources.note = "Studied Attacks advantage";
    g_studied.erase(mark);
  }
  return sources;
}

// Archery: +2 to the current attack roll when it is ranged (GetAttackModifierVersus).
int FightingStyleAttackBonus(void* stats) {
  if (!Faithful() || !CurrentAttackRanged(stats) || !HasStyle(stats, rules::kFeatFightingStyleArchery)) return 0;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), 0, 3))) {
    Log("fighter: 0x%08X Archery: +%d to ranged attack rolls", ObjectIdOf(stats), rules::kArcheryAttackBonus);
  }
  return rules::kArcheryAttackBonus;
}

// Archery on the character sheet's ranged attack bonus.
int FightingStyleSheetRangedBonus(void* stats) {
  return HasStyle(stats, rules::kFeatFightingStyleArchery) ? rules::kArcheryAttackBonus : 0;
}

// Two-Weapon Fighting (the style): the ability modifier on the off-hand attack's damage.
bool HasTwoWeaponFightingStyle(void* stats) { return HasStyle(stats, rules::kFeatFightingStyleTwoWeapon); }

// F5: the bonus for a failed save's reroll (the Fighter level), spending a
// use — or 0 (no use left, not a Fighter 9+, not faithful).
int IndomitableReroll(void* creature) {
  if (!creature) return 0;
  const int level = FighterLevelOf(StatsOf(creature));
  const int uses = rules::IndomitableUses(level);
  if (uses <= 0) return 0;
  const int used = engine::GetLocalInt(creature, kVarIndomitableUsed);
  if (used >= uses) return 0;
  engine::SetLocalInt(creature, kVarIndomitableUsed, used + 1);
  Log("fighter: 0x%08X Indomitable: failed save rerolled with +%d (%d of %d uses)", IdOf(creature), level, used + 1,
      uses);
  return level;
}

// The weapon-damage dice (Great Weapon Fighting, Savage Attacker).
void InstallWeaponDiceRules() {
  g_current_attack_weapon = reinterpret_cast<CurrentAttackWeaponFn>(EngineExport(ee::kGetCurrentAttackWeapon));
  if (g_current_attack_weapon && FindWeaponDiceCalls() &&
      HookExport("CNWSCreatureStats::GetDamageRoll (Great Weapon Fighting, Savage Attacker)", ee::kGetDamageRoll,
                 reinterpret_cast<void*>(&GetDamageRoll), reinterpret_cast<void**>(&g_get_damage_roll))) {
    AddDiceFilter(&GreatWeaponDice);
  }
}

// --- C7a: !style ------------------------------------------------------------------
// "Whenever you gain a Fighter level, you can replace the [Fighting Style]
// feat you chose with a different Fighting Style feat" (SRD 5.2): once per
// Fighter level gained, the Fighter level of the last swap kept as a record.
constexpr const char* kVarStyleSwap = "nwn5e_style_swap_level";

int StyleFeatByName(std::string name) {
  for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (name == "archery") return rules::kFeatFightingStyleArchery;
  if (name == "defense" || name == "defence") return rules::kFeatFightingStyleDefense;
  if (name.rfind("great", 0) == 0) return rules::kFeatFightingStyleGreatWeapon;
  if (name.rfind("two", 0) == 0) return rules::kFeatFightingStyleTwoWeapon;
  return -1;
}

const char* StyleName(int feat) {
  switch (feat) {
    case rules::kFeatFightingStyleArchery: return "Archery";
    case rules::kFeatFightingStyleDefense: return "Defense";
    case rules::kFeatFightingStyleGreatWeapon: return "Great Weapon Fighting";
    case rules::kFeatFightingStyleTwoWeapon: return "Two-Weapon Fighting";
    default: return "?";
  }
}

// The name the command takes ("great weapon" and "two-weapon" as typed).
const char* StyleCommandName(int feat) {
  switch (feat) {
    case rules::kFeatFightingStyleArchery: return "archery";
    case rules::kFeatFightingStyleDefense: return "defense";
    case rules::kFeatFightingStyleGreatWeapon: return "great weapon";
    case rules::kFeatFightingStyleTwoWeapon: return "two-weapon";
    default: return "?";
  }
}

// !style [<old> for <new>] - the new one a style the Fighter doesn't have
// (owner, 2026-10-22: with Archery and Defense both taken, "it always says
// "You already have archery"": the help named a swap between the two).
std::vector<std::string> StyleCommand(void* creature, const std::string& args) {
  void* stats = StatsOf(creature);
  const int level = FighterLevelOf(stats);
  if (level < 1) return {"nwn5e: !style swaps a Fighter's Fighting Style (class_features faithful)."};
  EnsureRecords(creature);
  const int last = engine::GetLocalInt(creature, kVarStyleSwap);
  const bool ready = level > last;
  std::string held;
  std::string free;  // the styles a swap can go to, as the command takes them
  int first_held = -1;
  int first_free = -1;
  for (int feat = rules::kFeatFightingStyleArchery; feat <= rules::kFeatFightingStyleTwoWeapon; ++feat) {
    if (engine::HasFeat(stats, feat)) {
      held += (held.empty() ? "" : ", ") + std::string(StyleName(feat));
      if (first_held < 0) first_held = feat;
    } else {
      free += (free.empty() ? "" : ", ") + std::string(StyleCommandName(feat));
      if (first_free < 0) first_free = feat;
    }
  }
  std::string swap;
  if (!ready) {
    swap = "Swapped at this Fighter level already - the next swap comes with your next Fighter level.";
  } else if (first_held < 0) {
    swap = "No Fighting Style to swap.";
  } else if (first_free < 0) {
    swap = "Every style is yours already - nothing to swap to.";
  } else {
    swap = std::string("You can swap one for a style you don't have (") + free + "), once per Fighter level: !style " +
           StyleCommandName(first_held) + " for " + StyleCommandName(first_free) + ".";
  }
  const std::string status = "nwn5e: Fighting Style: " + (held.empty() ? std::string("none") : held) + ". " + swap;
  const size_t at = args.find(" for ");
  if (args.empty() || at == std::string::npos) return {status};
  const int old_style = StyleFeatByName(args.substr(0, at));
  const int new_style = StyleFeatByName(args.substr(at + 5));
  if (old_style < 0 || new_style < 0) {
    return {"nwn5e: styles are archery, defense, great weapon, two-weapon.", status};
  }
  if (!ready) return {status};
  if (!engine::HasFeat(stats, old_style)) return {std::string("nwn5e: you don't have ") + StyleName(old_style) + ".", status};
  if (engine::HasFeat(stats, new_style)) {
    return {std::string("nwn5e: you have ") + StyleName(new_style) + " already - a swap goes to a style you don't have.",
            status};
  }
  engine::RemoveFeat(stats, old_style);
  if (!engine::AddFeat(stats, new_style)) {
    engine::AddFeat(stats, old_style);
    return {"nwn5e: the swap didn't take.", status};
  }
  SetRecord(creature, kVarStyleSwap, level);
  Log("fighter: 0x%08X Fighting Style %s swapped for %s at Fighter %d", IdOf(creature), StyleName(old_style),
      StyleName(new_style), level);
  return {std::string("nwn5e: ") + StyleName(old_style) + " swapped for " + StyleName(new_style) + "."};
}

void InstallFighterRules() {
  const bool savage_attacker = g_config.feats == rules::RuleVariant::kFaithful;
  if (!Faithful()) {
    OnRulesLoaded(&HideFightingStyleRows);
    if (savage_attacker) InstallWeaponDiceRules();
    if (g_config.armor == rules::RuleVariant::kFaithful) {  // 2024 armour on the sheet (rules_armor.cpp)
      HookExport("CNWSCreature::GetArmorClass (sheet and GetAC: 2024 armour)", ee::kCreatureGetArmorClass,
                 reinterpret_cast<void*>(&CreatureArmorClassArmorOnly),
                 reinterpret_cast<void**>(&g_creature_armor_class));
    }
    return;
  }
  g_compute_armor_class = reinterpret_cast<ComputeArmorClassFn>(EngineExport(ee::kItemComputeArmorClass));
  g_get_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  if (!g_compute_armor_class) Log("SKIP fighter: Defense - no export %s", ee::kItemComputeArmorClass);
  if (!g_get_max_hit_points) Log("SKIP fighter: Heroic Rally - no export %s", ee::kGetMaxHitPoints);
  if (!CountCombatRounds()) Log("SKIP fighter: Studied Attacks and Heroic Rally - combat rounds are not counted");
  HookExport("CNWSCreature::GetArmorClass (sheet and GetAC: Defense, 2024 armour)", ee::kCreatureGetArmorClass,
             reinterpret_cast<void*>(&CreatureArmorClass), reinterpret_cast<void**>(&g_creature_armor_class));
  InstallWeaponDiceRules();
  AddAttackRollListeners(nullptr, &StudiedAttacksAfterAttackRoll);
  AddCreatureUpdateListener(&HeroicRally);
  AddRestListener(&ResetIndomitable);
  RegisterRecords({kVarStyleSwap});
  AddChatCommand("!style", &StyleCommand);
  Log("fighter: Champion, Remarkable Athlete, Studied Attacks, Fighting Styles, Indomitable, Heroic Rally "
      "(faithful)");
}
