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
// Owner (2026-10-21): revisit the starting items later for game balance
// (stack sizes — 50 throwing axes for 8 javelins, 99 arrows for 20 — and
// NWN's starting healing potions, left out as the 2024 kits have none).

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

// The background's kit for a character made on the creation screen (called
// once, as it enters with its background).
void GiveBackgroundKit(uint32_t creature_id, rules::Background background) {
  if (g_config.starting_equipment != rules::RuleVariant::kFaithful) return;
  const rules::BackgroundKit kit = rules::BackgroundKit2024(background);
  if (kit.items.empty() && kit.gold == 0) return;
  std::string code = "object o = OBJECT_SELF;";
  std::string given;
  for (const rules::KitItem& item : kit.items) {
    char line[160];
    const int creates = item.one_stack ? 1 : item.count;
    for (int i = 0; i < creates; ++i) {
      sprintf_s(line, " CreateItemOnObject(\"%s\", o, %d);", item.blueprint, item.one_stack ? item.count : 1);
      code += line;
    }
    sprintf_s(line, "%s%s x%d", given.empty() ? "" : ", ", item.blueprint, item.count);
    given += line;
  }
  if (kit.gold > 0) code += " GiveGoldToCreature(o, " + std::to_string(kit.gold) + ");";
  const int result = RunScriptChunk(code.c_str(), creature_id);
  Log("starting equipment: 0x%08X the %s background's kit - %s, %d gold (script %d)", creature_id,
      rules::BackgroundData(background)->name, given.c_str(), kit.gold, result);
}
