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
// - Part 3b-2 (owner, 2026-11-05: "Not a fan of the spellbook appearing AFTER the class configuration can it be
//   integrated into the character creation somehow?"; 1A): a Wizard's levels 1+ on the page are its spellbook - six
//   at creation, two at each level-up (rules::SpellbookSpells2024; the count bounded by the book and the picks), the
//   title "Spellbook N of M", a spell already in its book not offered again (CNWSpell::GetSpellLevel answers 0xFF
//   at the page's Available pass for it). The picks land in the known list; prepare_spells.cpp takes them into the
//   book and keeps the prepared list to its count (the first four at creation).
// - Part 3b-3 (2A): a click on a known row drops the spell (the page's Remove; the level-up message sends what's gone
//   as removed, the server's LevelUp applies it) - stock NWN, for any non-memorising class and any number. Held to
//   SRD 5.2's level-up swaps (rules::LevelUpSwapAllowed): a pick of this level-up always; one known before it - the
//   page's own copy of the known lists at its setup (+0x1A20) - only Bard and Sorcerer one level 1+ spell, Bard,
//   Cleric, Druid and Sorcerer one cantrip; else refused, the title saying so.
// - 0.77.1 (owner, 2026-11-05: "again initial prepare spells pop up out of nowhere"): NWN's page setup gives a
//   class with LearnScroll at level 1 every cantrip (its known list's level 0 empty: all of them, the client's
//   AddKnownSpell in a loop) - 7 against 2024's 3, and the Prepare window opened to trim them. At that call the add is
//   skipped for a 2024 caster: the cantrips tab picks its 3.

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
#include "prepare_spells.h"
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
using GetKnownFn = uint32_t (*)(void* client_stats, uint8_t slot, uint8_t spell_level, uint8_t index);
using SpellLevelFn = uint8_t (*)(void* spell, uint8_t nwn_class);
using GetSpellFn = void* (*)(const void* spells, int id);
using ClientAddKnownFn = uintptr_t (*)(void* client_stats, uintptr_t slot, uintptr_t spell);
using RemoveFn = void (*)(void* page);
using ClientCreatureFn = void* (*)(uint32_t id);
constexpr int kNwnClassWizard = 10;

SpellsLeftFn g_spells_left = nullptr;
KnownCountFn g_known_count = nullptr;
ShowLevelFn g_show_level = nullptr;
QuickLevelUpFn g_quick_level_up = nullptr;
SetTextFn g_set_text = nullptr;
ExoStringFn g_make_string = nullptr;
ExoStringDestroyFn g_destroy_string = nullptr;
GetKnownFn g_get_known = nullptr;
SpellLevelFn g_spell_level = nullptr;
GetSpellFn g_get_spell = nullptr;
uintptr_t g_avail_site = 0;                   // ShowLevel's Available pass: its GetSpellLevel call's return address
thread_local std::vector<void*> t_book_spells;  // the levelled Wizard's book as CNWSpell* (for that pass)
thread_local bool t_book_loaded = false;
ClientAddKnownFn g_client_add_known = nullptr;
uintptr_t g_all_cantrips_site = 0;  // the page setup's every-cantrip add (its return address)
RemoveFn g_remove = nullptr;
ClientCreatureFn g_client_creature = nullptr;
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

// 3b-2: the Wizard's book (single player: the player character's; a Wizard at level 1 - new, or a new class - has
// none yet) and how many spells its book and the page's picks hold together.
std::vector<int> PageBook(const Levelled& c) { return c.level <= 1 ? std::vector<int>() : PrepareSpellsPlayerBook(); }

int BookWithPicks(void* client_stats, const Levelled& c) {
  std::vector<int> all = PageBook(c);
  for (int spell_level = 1; spell_level <= 9; ++spell_level) {
    const int count = g_known_count(client_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level));
    for (int i = 0; i < count && i < 255; ++i) {
      const int spell = static_cast<int>(g_get_known(client_stats, static_cast<uint8_t>(c.slot),
                                                     static_cast<uint8_t>(spell_level), static_cast<uint8_t>(i)));
      if (std::find(all.begin(), all.end(), spell) == all.end()) all.push_back(spell);
    }
  }
  return static_cast<int>(all.size());
}

void* RulesObject() {
  static void** const global = reinterpret_cast<void**>(EngineExport(ee::kRules));
  return global ? *global : nullptr;
}

// 3b-2: a spell already in the levelled Wizard's book isn't offered on the page's Available list again.
uint8_t SpellLevel(void* spell, uint8_t nwn_class) {
  const uint8_t level = g_spell_level(spell, nwn_class);
  if (reinterpret_cast<uintptr_t>(_ReturnAddress()) != g_avail_site || nwn_class != kNwnClassWizard || level == 0 ||
      level > 9 || !t_page_stats) {
    return level;
  }
  Levelled c;
  if (!LevelledClass(t_page_stats, &c) || c.row != kNwnClassWizard) return level;
  if (!t_book_loaded) {
    t_book_loaded = true;
    t_book_spells.clear();
    void* rules_object = RulesObject();
    void* spells = rules_object ? Field<void*>(rules_object, ee::kRulesSpells) : nullptr;
    for (int id : PageBook(c)) {
      void* entry = spells && g_get_spell ? g_get_spell(spells, id) : nullptr;
      if (entry) t_book_spells.push_back(entry);
    }
  }
  for (void* entry : t_book_spells) {
    if (entry == spell) return 0xFF;  // in the book already
  }
  return level;
}

// 0.77.1: the page setup's every-cantrip add skipped for a 2024 caster (the cantrips tab picks them).
uintptr_t ClientAddKnown(void* client_stats, uintptr_t slot, uintptr_t spell) {
  if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_all_cantrips_site) {
    Levelled c;
    if (LevelledClass(client_stats, &c)) return 0;
  }
  return g_client_add_known(client_stats, slot, spell);
}

uint8_t SpellsLeft(void* client_stats, uint8_t spell_level) {
  const uint8_t left = g_spells_left(client_stats, spell_level);
  t_page_stats = client_stats;
  Levelled c;
  if (spell_level == 0 || spell_level > 9 || !LevelledClass(client_stats, &c)) return left;
  if (c.row == kNwnClassWizard) {  // 3b-2: its spellbook
    return static_cast<uint8_t>(
        rules::SpellsLeftOnPage(left, rules::SpellbookSpells2024(c.level), BookWithPicks(client_stats, c)));
  }
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

// 3B: the known column's title, 2024's totals - or `note` (a refused swap).
void SetTitle(void* page, void* client_stats, int spell_level, const char* note) {
  Levelled c;
  if (!page || !LevelledClass(client_stats, &c)) return;
  // The title is a label like Remaining's (the same class, by its vtable) - else the layout isn't the one mapped.
  const void* remain = static_cast<uint8_t*>(page) + ee::kSpellsPageRemainLabel;
  const void* title = static_cast<uint8_t*>(page) + ee::kSpellsPageKnownTitle;
  if (Field<void*>(remain, 0) != Field<void*>(title, 0)) return;
  char text[64];
  if (note) {
    sprintf_s(text, "%s", note);
  } else if (spell_level == 0) {
    sprintf_s(text, "Cantrips %d of %d", g_known_count(client_stats, static_cast<uint8_t>(c.slot), 0),
              rules::CantripsKnown2024(c.row, c.level));
  } else if (c.row == kNwnClassWizard) {
    sprintf_s(text, "Spellbook %d of %d", BookWithPicks(client_stats, c), rules::SpellbookSpells2024(c.level));
  } else {
    sprintf_s(text, "Prepared %d of %d", KnownPrepared(client_stats, c), rules::PreparedSpells2024(c.row, c.level));
  }
  SetWrapperText(static_cast<uint8_t*>(page) + ee::kSpellsPageKnownTitle + ee::kGuiLabelWrapper, text);
}

void ShowLevel(void* page, int spell_level) {
  t_page_stats = nullptr;
  t_book_loaded = false;
  g_show_level(page, spell_level);
  SetTitle(page, t_page_stats, spell_level, nullptr);
}

// The page's client stats (its creature, page +0x19D8).
void* PageStats(void* page) {
  void* creature = g_client_creature ? g_client_creature(Field<uint32_t>(page, ee::kSpellsPageCreature)) : nullptr;
  return creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
}

bool ListHas(const void* list, uint32_t spell) {  // a CExoArrayList<uint32>: data +0, count +8
  const uint32_t* data = Field<const uint32_t*>(list, 0);
  const int count = Field<int32_t>(list, ee::kExoArrayListCount);
  for (int i = 0; data && i < count && i < 4096; ++i) {
    if (data[i] == spell) return true;
  }
  return false;
}

bool KnownNow(void* client_stats, const Levelled& c, int spell_level, uint32_t spell) {
  const int count = g_known_count(client_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level));
  for (int i = 0; i < count && i < 255; ++i) {
    if (g_get_known(client_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level),
                    static_cast<uint8_t>(i)) == spell) {
      return true;
    }
  }
  return false;
}

// 3b-3: the spells known before this level-up (the page's copy) already dropped, in that list (cantrips, or 1+).
int SwapsDone(void* page, void* client_stats, const Levelled& c, bool cantrip) {
  int done = 0;
  for (int spell_level = cantrip ? 0 : 1; spell_level <= (cantrip ? 0 : 9); ++spell_level) {
    const void* saved = static_cast<uint8_t*>(page) + ee::kSpellsPageSavedKnown + spell_level * ee::kExoArrayListSize;
    const uint32_t* data = Field<const uint32_t*>(saved, 0);
    const int count = Field<int32_t>(saved, ee::kExoArrayListCount);
    for (int i = 0; data && i < count && i < 4096; ++i) {
      if (!KnownNow(client_stats, c, spell_level, data[i])) ++done;
    }
  }
  return done;
}

void Remove(void* page) {
  const int index = page ? Field<int32_t>(page, ee::kSpellsPageSelected) : -1;
  const uint32_t* ids = page ? Field<const uint32_t*>(page, ee::kSpellsPageKnownIds) : nullptr;
  void* const client_stats = page && index >= 0 && ids ? PageStats(page) : nullptr;
  Levelled c;
  if (!client_stats || !LevelledClass(client_stats, &c)) {
    g_remove(page);
    return;
  }
  const int spell_level = Field<uint8_t>(page, ee::kSpellsPageLevel);
  const uint32_t spell = ids[index];
  const void* saved = static_cast<uint8_t*>(page) + ee::kSpellsPageSavedKnown + spell_level * ee::kExoArrayListSize;
  if (spell_level >= 0 && spell_level <= 9 && ListHas(saved, spell)) {  // known before: a swap
    const bool cantrip = spell_level == 0;
    if (!rules::LevelUpSwapAllowed(c.row, cantrip, SwapsDone(page, client_stats, c, cantrip))) {
      SetTitle(page, client_stats, spell_level, "No more swaps");
      return;
    }
  }
  g_remove(page);
  SetTitle(page, client_stats, spell_level, nullptr);
}

}  // namespace

int SpellsPageCap(uintptr_t caller, int spell_level, int cap) {
  if (!t_quick_stats || caller != g_quick_site || spell_level < 1 || spell_level > 9 || !g_known_count) return cap;
  Levelled c;
  if (!LevelledClass(t_quick_stats, &c)) return cap;
  const int here = g_known_count(t_quick_stats, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(spell_level));
  if (c.row == kNwnClassWizard) {
    return rules::BoundedKnownCap(cap, here, rules::SpellbookSpells2024(c.level), BookWithPicks(t_quick_stats, c));
  }
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
  const uintptr_t avail_level = show ? FindPatternIn("Spells page: the Available pass's spell level", FunctionAt(show),
                                                    ee::kSpellsPageAvailLevelPattern)
                                     : 0;
  const uintptr_t avail_known = show ? FindPatternIn("Spells page: the Available pass's known spell", FunctionAt(show),
                                                    ee::kSpellsPageAvailKnownPattern)
                                     : 0;
  g_avail_site = avail_level ? avail_level + 13 : 0;  // the call (+8) and its 5 bytes
  g_get_known = reinterpret_cast<GetKnownFn>(avail_known ? CallTarget(avail_known + 8) : 0);
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  const uintptr_t all_cantrips = FindPattern("the Spells page's every-cantrip add (#36)", ee::kSpellsPageAllCantripsPattern);
  const uintptr_t client_add = all_cantrips ? CallTarget(all_cantrips + 10) : 0;
  const uintptr_t remove_inside = FindPattern("the Spells page's Remove (#36 3b-3)", ee::kSpellsPageRemovePattern);
  const uintptr_t remove = remove_inside ? FunctionEntry(remove_inside) : 0;
  const uintptr_t creature_call = show ? FindPatternIn("Spells page: its creature", FunctionAt(show),
                                                      ee::kSpellsPageCreaturePattern)
                                       : 0;
  g_client_creature = reinterpret_cast<ClientCreatureFn>(creature_call ? CallTarget(creature_call + 8) : 0);
  g_all_cantrips_site = all_cantrips ? all_cantrips + 15 : 0;  // the call (+10) and its 5 bytes
  const uintptr_t quick = quick_inside ? FunctionEntry(quick_inside) : 0;
  g_quick_site = quick_inside ? quick_inside + 26 : 0;  // the pattern's call (+21) and its 5 bytes
  if (!left || !show || !quick || !g_known_count || !g_set_text || !g_make_string || !g_destroy_string ||
      !g_avail_site || !g_get_known || !g_get_spell || !client_add || !remove || !g_client_creature) {
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
                  reinterpret_cast<void*>(&QuickLevelUp), reinterpret_cast<void**>(&g_quick_level_up)) &&
      HookExport("CNWSpell::GetSpellLevel (a Wizard's book spells not offered again on the page, #36)",
                 ee::kSpellGetLevel, reinterpret_cast<void*>(&SpellLevel), reinterpret_cast<void**>(&g_spell_level)) &&
      HookAddress("client AddKnownSpell (no every-cantrip gift to a 2024 Wizard on the page, #36)", client_add,
                  reinterpret_cast<void*>(&ClientAddKnown), reinterpret_cast<void**>(&g_client_add_known)) &&
      HookAddress("CCharacterSpellsPanel Remove (the level-up swaps, SRD 5.2's, #36)", remove,
                  reinterpret_cast<void*>(&Remove), reinterpret_cast<void**>(&g_remove));
  Log("spells: NWN's Spells page at creation and level-up with 2024's counts - the total across levels, the title's "
      "totals; a Wizard's spellbook; the level-up swaps (#36 Part 3b)%s",
      hooked ? "" : " - SOME hooks missing");
  return hooked;
}
