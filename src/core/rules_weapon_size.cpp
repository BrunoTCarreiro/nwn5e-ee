// Weapon size (`weapon_size`). 2024 (SRD 5.2) lets any creature wield any
// weapon — size plays no part — and gives the Heavy property its own rule:
// Disadvantage with a Heavy melee weapon below Strength 13, a Heavy ranged
// one below Dexterity 13. NWN lets a creature wield a weapon at most one size
// larger than itself (two-handed); a Small creature can't hold a Large one
// (a halfling's greatsword, spear, quarterstaff, longbow).
//
// Faithful: a weapon two or more sizes larger counts as one size larger — the
// creature wields it two-handed — in the server's GetRelativeWeaponSize
// (equipping, the AI's weapon choice, attacks per round, the attack and
// damage terms: every size question goes through it) and the client's own
// copy (CNWCCreature::GetRelativeWeaponSize, anchored by its whip check; the
// inventory's equip checks and the weapon's hold and animations). The Heavy
// rule is a Disadvantage source in the attack roll's mode
// (rules_advantage.cpp). NWN mode: NWN's size rule, no Heavy rule.
//
// Not changed: NWN still has a Small creature hold a Medium weapon (a
// longsword) two-handed, where 2024 has one hand; the weapon model isn't
// scaled to the wielder.

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
#include "weapon_size.h"

namespace {

using RelativeSizeFn = int (*)(void* creature, void* item);
RelativeSizeFn g_server_relative_size = nullptr;
RelativeSizeFn g_client_relative_size = nullptr;
LogOnce g_logged(64);

bool Faithful() { return g_config.weapon_size == rules::RuleVariant::kFaithful; }

// The server's: weapon size - creature size within -2..+1, else -10 ("no").
int ServerRelativeWeaponSize(void* creature, void* item) {
  const int relative = g_server_relative_size(creature, item);
  if (relative != ee::kRelativeWeaponSizeInvalid || !creature || !item) return relative;
  const void* base_item = engine::BaseItem(Field<int32_t>(item, ee::kItemBaseItem));
  if (!base_item) return relative;
  const int size = Field<uint8_t>(base_item, ee::kBaseItemWeaponSize) - Field<int32_t>(creature, ee::kCreatureSize);
  if (size <= 1) return relative;  // far smaller than the creature: NWN's answer
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(Field<uint32_t>(creature, ee::kObjectId), size, 0))) {
    Log("weapon size: object 0x%08X wields a weapon %d sizes larger two-handed (2024: size doesn't limit weapons)",
        Field<uint32_t>(creature, ee::kObjectId), size);
  }
  return 1;
}

// The client's: the plain difference.
int ClientRelativeWeaponSize(void* creature, void* item) {
  const int relative = g_client_relative_size(creature, item);
  return relative > 1 ? 1 : relative;
}

}  // namespace

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
  HookExport("CNWSCreature::GetRelativeWeaponSize (weapon size: any size wields any weapon, 2024)",
             ee::kGetRelativeWeaponSize, reinterpret_cast<void*>(&ServerRelativeWeaponSize),
             reinterpret_cast<void**>(&g_server_relative_size));
  const uintptr_t whip_check = FindPattern("weapon size: the client's GetRelativeWeaponSize (its whip check)",
                                           ee::kClientRelativeWeaponSizePattern);
  const uintptr_t client = whip_check ? FunctionEntry(whip_check) : 0;
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  if (!client || whip_check - client > ee::kClientRelativeWeaponSizeCheckMaxOffset) {
    Log("SKIP weapon size (client): its GetRelativeWeaponSize not found - the inventory keeps NWN's size rule");
    return;
  }
  Log("weapon size: the client's GetRelativeWeaponSize at rva 0x%zx", client - base);
  HookAddress("CNWCCreature::GetRelativeWeaponSize (weapon size, the client's)", client,
              reinterpret_cast<void*>(&ClientRelativeWeaponSize), reinterpret_cast<void**>(&g_client_relative_size));
}
