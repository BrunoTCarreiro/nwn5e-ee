// Creature size (`size`). 2024 (SRD 5.2) has no size rules for weapons,
// shields, AC, attack rolls or unarmed damage — any creature wields any
// weapon, and the Heavy property has its own rule: Disadvantage with a Heavy
// melee weapon below Strength 13, a Heavy ranged one below Dexterity 13.
// NWN (3e) lets a creature wield a weapon at most one size larger than itself
// (two-handed), keeps the Tower Shield from Tiny and Small creatures, gives
// Small +1 AC and +1 to attack (Tiny +2, Large -1, Huge -2), and smaller
// unarmed dice to Small creatures (monks included). So a halfling or gnome
// couldn't hold a greatsword, spear, quarterstaff or longbow.
//
// Faithful:
// - a weapon two or more sizes larger counts as one size larger — wielded
//   two-handed — in the server's GetRelativeWeaponSize (attacks per round,
//   the attack and damage terms, the AI's weapon choice...) and the client's
//   copy (CNWCCreature::GetRelativeWeaponSize, anchored by its whip check:
//   the inventory's off-hand checks, the weapon's hold and animations);
// - the checks the engine makes inline get one-byte patches (all or none):
//   CanEquipWeapon's "too large", CanUseItem's (the inventory's unusable
//   mark), the Tower Shield's size check in CanEquipItem, CanEquipShield and
//   CanUseItem;
// - the size modifiers (CREATURE_SIZE_*) are answered 0, and a Small
//   creature's unarmed dice (UNARMED_SDAMAGE_DIE, the monk's
//   MONK_DAMAGE_TIER_*_SDAMAGE_*) are the Medium ones;
// - the Heavy rule is a Disadvantage source in the attack's roll mode
//   (rules_advantage.cpp).
// NWN mode: NWN's size rules, no Heavy rule.
//
// - and a Small creature wields weapons as a Medium one (owner, 2026-10-21):
//   a longsword in one hand, a greatsword in two, a dagger light — in the
//   size functions above (server and client: the hold and animations) and,
//   where the engine does the arithmetic inline, for the length of the call
//   (CanEquipWeapon here, the round's off-hand attacks in rules_two_weapon.cpp).
// Not changed: weapon models aren't scaled to the wielder. Knockdown /
// Disarm's size terms are the Shove rule's (SK6), Hide / Spot's the
// perception rule's (SK5).

#include <windows.h>

#include <stdint.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "rules/weapons.h"
#include "ruleset.h"
#include "weapon_size.h"

namespace {

using RelativeSizeFn = int (*)(void* creature, void* item);
using CanEquipWeaponFn = uint8_t (*)(void* creature, void* item, uint32_t* slot, int a, int b, void* player);
RelativeSizeFn g_server_relative_size = nullptr;
RelativeSizeFn g_client_relative_size = nullptr;
CanEquipWeaponFn g_can_equip_weapon = nullptr;
LogOnce g_logged(64);

bool Faithful() { return g_config.size == rules::RuleVariant::kFaithful; }

// The server's: weapon size - creature size within -2..+1, else -10 ("no").
int ServerRelativeWeaponSize(void* creature, void* item) {
  const int relative = g_server_relative_size(creature, item);
  if (!creature || !item) return relative;
  const void* base_item = engine::BaseItem(Field<int32_t>(item, ee::kItemBaseItem));
  if (!base_item) return relative;
  const int size = Field<uint8_t>(base_item, ee::kBaseItemWeaponSize) - WeaponWielderSize(creature);
  if (size < -2) return relative;  // far smaller than the creature: NWN's answer
  if (size <= 1) return size;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(Field<uint32_t>(creature, ee::kObjectId), size, 0))) {
    Log("size: object 0x%08X wields a weapon %d sizes larger two-handed (2024: size doesn't limit weapons)",
        Field<uint32_t>(creature, ee::kObjectId), size);
  }
  return 1;
}

// The client's: the plain difference.
int ClientRelativeWeaponSize(void* creature, void* item) {
  int relative = g_client_relative_size(creature, item);
  const int row = item ? Field<int32_t>(item, ee::kItemBaseItem) : -1;
  const void* base_item = row == ee::kBaseItemWhip ? nullptr : engine::BaseItem(row);
  if (base_item && Field<uint8_t>(base_item, ee::kBaseItemWeaponSize) - relative == ee::kCreatureSizeSmall) {
    relative -= 1;  // a Small creature wields as a Medium one
  }
  return relative > 1 ? 1 : relative;
}

uint8_t CanEquipWeapon(void* creature, void* item, uint32_t* slot, int a, int b, void* player) {
  WeaponSizeAsMedium as_medium(creature);
  return g_can_equip_weapon(creature, item, slot, a, b, player);
}

void PatchInlineSizeChecks() {
  static const uint8_t kJle[] = {0x7e}, kJa[] = {0x77}, kJmp[] = {0xeb};
  static const uint8_t kThree[] = {0x03}, kAny[] = {0x7f};
  static const uint8_t kCmovbe[] = {0x0f, 0x46, 0xf8}, kNop3[] = {0x0f, 0x1f, 0x00};
  static const PatternPatch kPatches[] = {
      {"CanEquipWeapon: no \"too large\"", ee::kCanEquipWeapon, "41 83 FF 01 7E ?? 83 FF 01", 4, kJle, kJmp, 1},
      {"CanUseItem: no \"too large\"", ee::kCanUseItem, "2B CB 83 C1 02 83 F9 03 B8 00 00 00 00 0F 47 F8", 7, kThree,
       kAny, 1},
      {"CanUseItem: the Tower Shield for every size", ee::kCanUseItem, "FF C8 83 F8 01 B8 00 00 00 00 0F 46 F8", 10,
       kCmovbe, kNop3, 3},
      {"CanEquipItem: the Tower Shield for every size", ee::kCanEquipItem, "8B 87 18 07 00 00 FF C8 83 F8 01 77", 11,
       kJa, kJmp, 1},
      {"CanEquipShield: the Tower Shield for every size", ee::kCanEquipShield,
       "8B 81 18 07 00 00 FF C8 83 F8 01 77", 11, kJa, kJmp, 1},
  };
  ApplyPatternPatches("size: equipping and usability (2024: size limits no weapon or shield)", kPatches,
                      sizeof(kPatches) / sizeof(kPatches[0]));
}

int NoSizeModifier(int) { return 0; }

template <const char* kMediumLabel>
int MediumValue(int engine_value) {
  return RulesetIntValue(kMediumLabel, engine_value);
}

// The Medium counterparts of a Small creature's unarmed dice.
constexpr char kUnarmedDie[] = "UNARMED_DAMAGE_DIE";
constexpr char kMonkDice[] = "MONK_DAMAGE_TIER_DICE";
constexpr char kMonkDie0[] = "MONK_DAMAGE_TIER_ZERO_DAMAGE_DIE";
constexpr char kMonkDie1[] = "MONK_DAMAGE_TIER_ONE_DAMAGE_DIE";
constexpr char kMonkDie2[] = "MONK_DAMAGE_TIER_TWO_DAMAGE_DIE";
constexpr char kMonkDie3[] = "MONK_DAMAGE_TIER_THREE_DAMAGE_DIE";
constexpr char kMonkDie4[] = "MONK_DAMAGE_TIER_FOUR_DAMAGE_DIE";

void AnswerSizeRuleset() {
  const char* why = "size: no 3e size modifier (2024)";
  for (const char* label : {"CREATURE_SIZE_TINY_AC_BONUS", "CREATURE_SIZE_SMALL_AC_BONUS",
                            "CREATURE_SIZE_LARGE_AC_PENALTY", "CREATURE_SIZE_HUGE_AC_PENALTY",
                            "CREATURE_SIZE_TINY_ATTACK_BONUS", "CREATURE_SIZE_SMALL_ATTACK_BONUS",
                            "CREATURE_SIZE_LARGE_ATTACK_PENALTY", "CREATURE_SIZE_HUGE_ATTACK_PENALTY"}) {
    AnswerRulesetInt(label, why, &NoSizeModifier);
  }
  const char* dice = "size: a Small creature's unarmed dice are the Medium ones (2024)";
  AnswerRulesetInt("UNARMED_SDAMAGE_DIE", dice, &MediumValue<kUnarmedDie>);
  AnswerRulesetInt("MONK_DAMAGE_TIER_ZERO_SDAMAGE_DIE", dice, &MediumValue<kMonkDie0>);
  AnswerRulesetInt("MONK_DAMAGE_TIER_ONE_SDAMAGE_DIE", dice, &MediumValue<kMonkDie1>);
  AnswerRulesetInt("MONK_DAMAGE_TIER_TWO_SDAMAGE_DIE", dice, &MediumValue<kMonkDie2>);
  AnswerRulesetInt("MONK_DAMAGE_TIER_THREE_SDAMAGE_DIE", dice, &MediumValue<kMonkDie3>);
  AnswerRulesetInt("MONK_DAMAGE_TIER_FOUR_SDAMAGE_DIE", dice, &MediumValue<kMonkDie4>);
  for (const char* label : {"MONK_DAMAGE_TIER_ZERO_SDAMAGE_DICE", "MONK_DAMAGE_TIER_ONE_SDAMAGE_DICE",
                            "MONK_DAMAGE_TIER_TWO_SDAMAGE_DICE", "MONK_DAMAGE_TIER_THREE_SDAMAGE_DICE",
                            "MONK_DAMAGE_TIER_FOUR_SDAMAGE_DICE"}) {
    AnswerRulesetInt(label, dice, &MediumValue<kMonkDice>);
  }
}

}  // namespace

int WeaponWielderSize(void* creature) {
  const int size = creature ? Field<int32_t>(creature, ee::kCreatureSize) : ee::kCreatureSizeMedium;
  return Faithful() && size == ee::kCreatureSizeSmall ? ee::kCreatureSizeMedium : size;
}

WeaponSizeAsMedium::WeaponSizeAsMedium(void* creature) {
  if (!Faithful() || !creature || Field<int32_t>(creature, ee::kCreatureSize) != ee::kCreatureSizeSmall) return;
  creature_ = creature;
  SetField<int32_t>(creature, ee::kCreatureSize, ee::kCreatureSizeMedium);
}

WeaponSizeAsMedium::~WeaponSizeAsMedium() {
  if (creature_) SetField<int32_t>(creature_, ee::kCreatureSize, ee::kCreatureSizeSmall);
}

int HeavyWeaponDisadvantage(void* attacker, bool ranged, std::string* note) {
  if (!Faithful() || !attacker) return 0;
  void* stats = StatsOf(attacker);
  const int base_item = AttackWeaponBaseItem(attacker);
  if (!stats || base_item < 0 || !rules::IsHeavyWeapon2024(base_item)) return 0;
  const int ability = ranged ? 1 : 0;  // Dexterity, Strength
  const int score = engine::AbilityScore(stats, ability);
  if (score <= 0 || score >= 13) return 0;
  if (note) {
    *note = std::string("Heavy weapon, ") + (ranged ? "Dexterity " : "Strength ") + std::to_string(score) +
            " (13 needed): disadvantage";
  }
  return 1;
}

void InstallWeaponSizeRules() {
  if (!Faithful()) return;
  HookExport("CNWSCreature::GetRelativeWeaponSize (size: any size wields any weapon, 2024)",
             ee::kGetRelativeWeaponSize, reinterpret_cast<void*>(&ServerRelativeWeaponSize),
             reinterpret_cast<void**>(&g_server_relative_size));
  HookExport("CNWSCreature::CanEquipWeapon (size: a Small creature wields as a Medium one)", ee::kCanEquipWeapon,
             reinterpret_cast<void*>(&CanEquipWeapon), reinterpret_cast<void**>(&g_can_equip_weapon));
  PatchInlineSizeChecks();
  AnswerSizeRuleset();
  const uintptr_t whip_check = FindPattern("size: the client's GetRelativeWeaponSize (its whip check)",
                                           ee::kClientRelativeWeaponSizePattern);
  const uintptr_t client = whip_check ? FunctionEntry(whip_check) : 0;
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  if (!client || whip_check - client > ee::kClientRelativeWeaponSizeCheckMaxOffset) {
    Log("SKIP size (client): its GetRelativeWeaponSize not found - the inventory keeps NWN's size rule");
    return;
  }
  Log("size: the client's GetRelativeWeaponSize at rva 0x%zx", client - base);
  HookAddress("CNWCCreature::GetRelativeWeaponSize (size, the client's)", client,
              reinterpret_cast<void*>(&ClientRelativeWeaponSize), reinterpret_cast<void**>(&g_client_relative_size));
}
