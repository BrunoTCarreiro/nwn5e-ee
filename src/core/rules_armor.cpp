// 2024 armour (0.46.0; owner, 2026-10-27, after the Rage test in chain mail:
// "make sure the NWN armor AC matches 2024 and go with A"), `armor` faithful.
//
// NWN's armour tiers - the chest item's base AC 0-8 - are read as the 2024
// armour the game's own descriptions name (rules/armor.h): padded, leather,
// studded leather, scale mail, chain mail, splint, half plate, plate. For a
// character's (a player's or a henchman's; a monster's AC comes from its CR):
// - Dexterity: CNWSCreatureStats::GetDEXMod(1) - the AC's Dexterity, capped by
//   armor.2da's DEXBONUS for the tier (0x3fd280; a cap of 0 means none, so no
//   table can say "heavy: no Dex") - answers the 2024 one: light all, medium at
//   most +2, heavy none (no bonus, no penalty);
// - the base: the difference between the 2024 armour's AC and NWN's (10 +
//   tier) - leather and studded -1, chain mail and splint +1, half plate -2 -
//   added where nwn5e already adds to the AC (GetArmorClassVersus for attacks,
//   rules_attack.cpp; CNWSCreature::GetArmorClass for the sheet, rules_fighter.cpp);
// - Heavy armour is chain mail, splint and plate (Rage, Fast Movement:
//   rules_barbarian.cpp); half plate is medium.
// Not yet: the Strength an armour asks for (Speed -10 ft below it) and its
// Stealth Disadvantage. NWN mode: NWN's armour.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/armor.h"

namespace {

using GetDexModFn = int8_t (*)(void* stats, int armor_cap);
using ComputeArmorClassFn = int (*)(void* item);

GetDexModFn g_get_dex_mod = nullptr;
ComputeArmorClassFn g_compute_armor_class = nullptr;
bool g_on = false;
LogOnce g_logged(200);

bool Faithful() { return g_config.armor == rules::RuleVariant::kFaithful; }

// The chest armour's tier (its base AC), or 0.
int ChestTier(void* creature) {
  void* chest = creature && g_compute_armor_class ? engine::ItemInSlot(creature, ee::kSlotChest) : nullptr;
  return chest ? g_compute_armor_class(chest) : 0;
}

bool IsCharacter(void* stats) {
  if (!stats) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  return facts.character && facts.plausible;
}

void LogArmour(void* stats, int tier, int engine_dex, int dex) {
  if (!g_config.log_attacks || !g_logged.First(LogOnce::Key(ObjectIdOf(stats), tier, engine_dex))) return;
  const rules::Armor2024& armor = rules::ArmorForNwnTier(tier);
  Log("armor: 0x%08X %s (NWN AC %d): 2024 AC %d + Dex %+d (NWN's Dex %+d); base %+d", ObjectIdOf(stats), armor.name,
      tier, armor.base_ac, dex, engine_dex, rules::ArmorBaseAdjustment2024(tier));
}

// The AC's Dexterity (armor_cap 1): the 2024 armour's.
int8_t GetDEXMod(void* stats, int armor_cap) {
  const int8_t engine = g_get_dex_mod(stats, armor_cap);
  if (armor_cap != 1 || !IsCharacter(stats)) return engine;
  void* creature = CreatureOf(stats);
  const int tier = ChestTier(creature);
  const int8_t full = g_get_dex_mod(stats, 0);  // uncapped
  const int dex = rules::ArmorDexterity2024(tier, full);
  LogArmour(stats, tier, engine, dex);
  return static_cast<int8_t>(dex);
}

}  // namespace

// rules_attack.cpp's GetArmorClassVersus and rules_fighter.cpp's sheet: the
// 2024 armour's base against NWN's tier (0 when off, unarmoured or a monster).
int ArmorBaseAdjustment(void* stats) {
  if (!g_on || !IsCharacter(stats)) return 0;
  return rules::ArmorBaseAdjustment2024(ChestTier(CreatureOf(stats)));
}

void InstallArmorRules() {
  g_compute_armor_class = reinterpret_cast<ComputeArmorClassFn>(EngineExport(ee::kItemComputeArmorClass));
  if (!Faithful()) return;
  if (!g_compute_armor_class) {
    Log("SKIP armor: no export %s - NWN's armour AC", ee::kItemComputeArmorClass);
    return;
  }
  g_on = HookExport("CNWSCreatureStats::GetDEXMod (2024 armour: the AC's Dexterity)", ee::kGetDEXMod,
                    reinterpret_cast<void*>(&GetDEXMod), reinterpret_cast<void**>(&g_get_dex_mod));
  Log(g_on ? "armor: 2024 armour - padded, leather, studded, scale, chain mail, splint, half plate, plate (AC, Dex "
             "caps; heavy = chain mail, splint, plate) (armor faithful)"
           : "armor: NOT on (GetDEXMod not hooked)");
}
