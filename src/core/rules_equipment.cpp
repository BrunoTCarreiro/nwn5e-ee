// C2 (docs/CHARGEN.md), `starting_equipment` faithful: the creation screen's
// Packages step offers the 2024 starting kits — one package per class (the
// Fighter two: SRD option A, greatsword; option B, archer), each with its
// SRD 5.2 kit as NWN items and gold (tools/data/kits.py: data/files/packeq5e_*,
// data/rows/*.if_kits*.txt, served only in this mode) — and NWN's themed
// packages leave the list (kept for henchmen). Each class's Recommended
// package is its 2024 one. When a character made on the creation screen
// enters, its background's kit is added (rules::BackgroundKit2024), with the
// background (rules_abilities.cpp).
//
// Balance (owner, 2026-10-23), the 2024 kits kept but: full stacks ("for a
// videogame with no ammo recovery, I think just give full stacks"), NWN's
// gold - the class build's, the class's default NWN package's; the
// background adds none ("keep NWN's gold") - and NWN's three Potions of Cure
// Light Wounds in every class kit ("the tutorial having some combat healing
// potions are required as there's no place to buy them before combat
// happens"). rules::BackgroundKit2024 stays the 2024 data.
constexpr int kFullAmmunitionStack = 99;  // arrows, bolts, bullets: the base items' stack

#include <stdio.h>

#include <string>

#include "config.h"
#include "data_serving.h"
#include "effects.h"
#include "log.h"
#include "rules/backgrounds.h"

void InstallStartingEquipment() {
  if (g_config.starting_equipment != rules::RuleVariant::kFaithful) return;
  ServeConditionalRows("kits");
  Log("starting equipment: the 2024 kits are the creation screen's packages; a background's kit comes when the "
      "character enters (starting_equipment faithful)");
}

// The 2024 carrying capacity (`carrying` faithful): NWN's encumbrance table and
// overload penalty served as SRD 5.2's (tools/data/carrying_capacity.py).
void InstallCarrying() {
  if (g_config.carrying != rules::RuleVariant::kFaithful) {
    Log("carrying: NWN's encumbrance table (carrying nwn)");
    return;
  }
  ServeConditionalRows("carry2024");
  Log("carrying: Strength x 15 lb. carried with no penalty, more overloaded - Speed about 5 ft (carrying faithful)");
}

// The background's kit for a character made on the creation screen (called
// once, as it enters with its background). An item the character already
// has (carried or worn: the class kit's robe, quarterstaff, dagger, spear) is
// not given twice (owner, 2026-10-21).
void GiveBackgroundKit(uint32_t creature_id, rules::Background background) {
  if (g_config.starting_equipment != rules::RuleVariant::kFaithful) return;
  const rules::BackgroundKit kit = rules::BackgroundKit2024(background);
  if (kit.items.empty()) return;
  std::string code = "object o = OBJECT_SELF; object it; int has; int slot;";
  std::string given;
  for (const rules::KitItem& item : kit.items) {
    char line[512];
    sprintf_s(line,
              " has = 0; it = GetFirstItemInInventory(o);"
              " while (GetIsObjectValid(it)) { if (GetResRef(it) == \"%s\") has = 1; it = GetNextItemInInventory(o); }"
              " for (slot = 0; slot < NUM_INVENTORY_SLOTS; slot++) if (GetResRef(GetItemInSlot(slot, o)) == \"%s\") has = 1;"
              " if (!has) {",
              item.blueprint, item.blueprint);
    code += line;
    const int creates = item.one_stack ? 1 : item.count;
    const int stack = item.one_stack ? kFullAmmunitionStack : 1;  // full stacks (no ammunition recovery)
    for (int i = 0; i < creates; ++i) {
      sprintf_s(line, " CreateItemOnObject(\"%s\", o, %d);", item.blueprint, stack);
      code += line;
    }
    code += " }";
    sprintf_s(line, "%s%s x%d", given.empty() ? "" : ", ", item.blueprint, item.one_stack ? stack : item.count);
    given += line;
  }
  // No gold: the starting gold is NWN's, the class build's (the kit's 2024 GP: kit.gold, not given).
  const int result = RunScriptChunk(code.c_str(), creature_id);
  Log("starting equipment: 0x%08X the %s background's kit - %s (each unless already had; its %d GP not given: NWN's "
      "gold) (script %d)",
      creature_id, rules::BackgroundData(background)->name, given.c_str(), kit.gold, result);
}
