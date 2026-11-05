// #36 Part 3b-1 (owner, 2026-11-05: "go with your recommendations" - 1A 2A 3B; docs/PLAN.md "Phase 4"), spellcasting
// faithful: NWN's own Spells page at creation and level-up (CCharacterSpellsPanel, .gui CG_SPELLS) for Bard, Sorcerer,
// Cleric, Druid, Paladin and Ranger (classes.if_spells2024: SkipSpellSelection 0), with 2024's counts.
// - The page counts per spell level: the client's spells left (not exported; a byte pattern) is GetSpellsKnownPerLevel
//   minus the spells known at that level, and since 0.75 GetSpellsKnownPerLevel answers the prepared total at every
//   level with slots. So the count is bounded by the total left - what the class prepares minus what it knows at all
//   its levels (rules::SpellsLeftOnPage): the Remaining number, a pick, Recommended.
// - The hub's quick level-up (and the page's Recommended: the same function, 0x7EB630) asks GetSpellsKnownPerLevel
//   itself; at that call (its return address, found at start) rules_spell_slots.cpp's answer is bounded the same way
//   (SpellsPageCap; rules::BoundedKnownCap). The other readers - a tab's enable test, 0x7EE8AE, the server's - keep it:
//   they ask whether a level has spells, not how many are left.
// - 3B: the known column's title (KnownTitle) reads 2024's totals - "Cantrips 3 of 3" on the cantrips tab, "Prepared
//   5 of 6" on the others - set after each ShowLevel (a tab, and a click's refresh).
// The Wizard stays on the Prepare spells window's Spellbook tab until Part 3b-2 (its SkipSpellSelection stays 1).

#include "spells_page.h"

#include <intrin.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/spell_lists.h"
#include "rules/spell_slots.h"

namespace {

using SpellsLeftFn = uint8_t (*)(void* client_stats, uint8_t spell_level);
using KnownCountFn = uint16_t (*)(void* client_stats, uint8_t slot, uint8_t spell_level);
using ShowLevelFn = void (*)(void* page, int spell_level);
using QuickLevelUpFn = uintptr_t (*)(void* client_stats, void* package, void* out);
using SetTextFn = void (*)(void* wrapper, const void* exo_string);
using ExoStringFn = void (*)(void* exo_string, const char* text);
using ExoStringDestroyFn = void (*)(void* exo_string);

SpellsLeftFn g_spells_left = nullptr;
KnownCountFn g_known_count = nullptr;
ShowLevelFn g_show_level = nullptr;
QuickLevelUpFn g_quick_level_up = nullptr;
SetTextFn g_set_text = nullptr;
ExoStringFn g_make_string = nullptr;
ExoStringDestroyFn g_destroy_string = nullptr;
uintptr_t g_quick_site = 0;                // the quick level-up's GetSpellsKnownPerLevel call (its return address)
thread_local void* t_quick_stats = nullptr;  // the client stats the quick level-up runs for
thread_local void* t_page_stats = nullptr;   // the client stats the page's last spells-left was for

struct Levelled {
  int slot = 0;
  int row = 0;
  int level = 0;
};

// The class the client is levelling (or creating) - true for a 2024 caster.
bool LevelledClass(void* client_stats, Levelled* out) {
  if (!client_stats) return false;
  const int slot = Field<uint8_t>(client_stats, ee::kClientStatsClassBeingLevelled);
  if (slot < 0 || slot >= ee::kClientStatsClassSlots) return false;
  out->slot = slot;
  out->row = Field<uint8_t>(client_stats, ee::kClientStatsClassSlotClass + slot * ee::kClientStatsClassSlotSize);
  out->level = Field<uint8_t>(client_stats, ee::kClientStatsClassSlotLevel + slot * ee::kClientStatsClassSlotSize);
  return rules::CasterKindOfClass(out->row) != rules::CasterKind::kNone;
}

// The level 1+ spells it knows (its prepared list), at the levels it has slots for.
int KnownPrepared(void* client_stats, const Levelled& c) {
  int known = 0;
  for (int spell_level = 1; spell_level <= 9; ++spell_level) {
    if (rules::SpellGain2024(c.row, c.level, spell_level) <= 0) continue;
    known += g_known_count(client_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level));
  }
  return known;
}

uint8_t SpellsLeft(void* client_stats, uint8_t spell_level) {
  const uint8_t left = g_spells_left(client_stats, spell_level);
  t_page_stats = client_stats;
  Levelled c;
  if (spell_level == 0 || spell_level > 9 || !LevelledClass(client_stats, &c)) return left;
  return static_cast<uint8_t>(
      rules::SpellsLeftOnPage(left, rules::PreparedSpells2024(c.row, c.level), KnownPrepared(client_stats, c)));
}

uintptr_t QuickLevelUp(void* client_stats, void* package, void* out) {
  void* const before = t_quick_stats;
  t_quick_stats = client_stats;
  const uintptr_t result = g_quick_level_up(client_stats, package, out);
  t_quick_stats = before;
  return result;
}

void SetWrapperText(void* wrapper, const std::string& text) {
  alignas(16) uint8_t exo[32] = {};  // CExoString: the engine owns its buffer
  g_make_string(exo, text.c_str());
  g_set_text(wrapper, exo);
  g_destroy_string(exo);
}

// 3B: the known column's title, 2024's totals.
void ShowLevel(void* page, int spell_level) {
  t_page_stats = nullptr;
  g_show_level(page, spell_level);
  Levelled c;
  void* const client_stats = t_page_stats;
  if (!page || !LevelledClass(client_stats, &c)) return;
  // The title is a label like Remaining's (the same class, by its vtable) - else the layout isn't the one mapped.
  const void* remain = static_cast<uint8_t*>(page) + ee::kSpellsPageRemainLabel;
  const void* title = static_cast<uint8_t*>(page) + ee::kSpellsPageKnownTitle;
  if (Field<void*>(remain, 0) != Field<void*>(title, 0)) return;
  char text[64];
  if (spell_level == 0) {
    sprintf_s(text, "Cantrips %d of %d", g_known_count(client_stats, static_cast<uint8_t>(c.slot), 0),
              rules::CantripsKnown2024(c.row, c.level));
  } else {
    sprintf_s(text, "Prepared %d of %d", KnownPrepared(client_stats, c), rules::PreparedSpells2024(c.row, c.level));
  }
  SetWrapperText(static_cast<uint8_t*>(page) + ee::kSpellsPageKnownTitle + ee::kGuiLabelWrapper, text);
}

}  // namespace

int SpellsPageCap(uintptr_t caller, int spell_level, int cap) {
  if (!t_quick_stats || caller != g_quick_site || spell_level < 1 || spell_level > 9 || !g_known_count) return cap;
  Levelled c;
  if (!LevelledClass(t_quick_stats, &c)) return cap;
  const int here = g_known_count(t_quick_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level));
  return rules::BoundedKnownCap(cap, here, rules::PreparedSpells2024(c.row, c.level), KnownPrepared(t_quick_stats, c));
}

bool InstallSpellsPage() {
  const uintptr_t left = FindPattern("the client's spells left (#36 3b)", ee::kClientSpellsLeftPattern);
  const uintptr_t show_inside = FindPattern("the Spells page's ShowLevel (#36 3b)", ee::kSpellsPageShowLevelPattern);
  const uintptr_t quick_inside =
      FindPattern("the quick level-up's spell picks (#36 3b)", ee::kClientQuickLevelUpSpellPattern);
  const uintptr_t known_call = left ? FindPatternIn("spells left: the known count", FunctionAt(left),
                                                    ee::kClientSpellsLeftKnownCallPattern)
                                    : 0;
  g_known_count = reinterpret_cast<KnownCountFn>(known_call ? CallTarget(known_call + 14) : 0);
  g_set_text = reinterpret_cast<SetTextFn>(EngineExport(ee::kStringWrapperSetText));
  g_make_string = reinterpret_cast<ExoStringFn>(EngineExport(ee::kExoStringFromChars));
  g_destroy_string = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  const uintptr_t show = show_inside ? FunctionEntry(show_inside) : 0;
  const uintptr_t quick = quick_inside ? FunctionEntry(quick_inside) : 0;
  g_quick_site = quick_inside ? quick_inside + 26 : 0;  // the pattern's call (+21) and its 5 bytes
  if (!left || !show || !quick || !g_known_count || !g_set_text || !g_make_string || !g_destroy_string) {
    Log("SKIP spells #36 3b: the Spells page isn't mapped (left %p, show %p, quick %p, known %p)",
        reinterpret_cast<void*>(left), reinterpret_cast<void*>(show), reinterpret_cast<void*>(quick),
        reinterpret_cast<void*>(g_known_count));
    return false;
  }
  const bool hooked =
      HookAddress("client spells left (the Spells page: 2024's total, #36)", left, reinterpret_cast<void*>(&SpellsLeft),
                  reinterpret_cast<void**>(&g_spells_left)) &&
      HookAddress("CCharacterSpellsPanel ShowLevel (2024's totals in the title, #36)", show,
                  reinterpret_cast<void*>(&ShowLevel), reinterpret_cast<void**>(&g_show_level)) &&
      HookAddress("client quick level-up (its spell picks to 2024's total, #36)", quick,
                  reinterpret_cast<void*>(&QuickLevelUp), reinterpret_cast<void**>(&g_quick_level_up));
  Log("spells: NWN's Spells page at creation and level-up with 2024's counts - the total across levels, the title's "
      "totals (#36 Part 3b-1)%s",
      hooked ? "" : " - SOME hooks missing");
  return hooked;
}
