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
// - 0.83.0 (owner, 2026-11-06: "instead of addin 'in' and 'out' is it possible to change the visibility of the icons
//   used to add and remove spells? ... a spell that was added shoould keep the icon that allows the user to remove
//   it while all the other ones that would result in a 'no more swaps' warning would get their icons hidden"; the
//   0.78.1-0.78.3 labels "- In" / "- Out" gone): each row is a CGuiCharGenFeatButton whose +0xC8 child button
//   moves it (the page's Add or Remove) and whose +0x198 child is the info button shown on hover. Once a tab is
//   built, a known row whose spell can't be dropped now (known before this level-up, its list's swap spent or none
//   for the class: rules::RemoveIconShown) has its move button taken out of the panel (CGuiObject::RemoveFromPanel,
//   as the familiar page's row does). A drop rebuilds the tab (ShowLevel), so the other rows lose theirs at once; a
//   pick that undoes a swap (the dropped spell picked back) rebuilds it too, the buttons back. "No more swaps"
//   stays, should one show.
// - 0.83.1 (owner, 2026-11-06: "ALso remove the icon that adds spells to the prepared column once it is no longer
//   possible to add new spells"): once the tab has nothing left to pick (rules::AddIconShown - after it's built, and
//   after a pick), every available row's move button goes too; a drop rebuilds the tab, the buttons back.
//   And (owner: "in cantrips, do not move automatically to level 1 spells once all the cantrips are seçected"):
//   Add moves on to the next tab when the one picked on is done and the next has picks left - two looks at the next
//   level's spells left (calls into the client's spells left right after "inc dl"). From the cantrips tab they're
//   answered 0, so the page stays on it.
// - 0.83.3 (owner, 2026-11-06, WZ-3: "On level up it says 6 of 8 but only shows 4 spells", then "go for A"): a
//   Wizard's level-up page lists its whole spellbook in the known column - ShowLevel's known pass (its two calls of
//   the client's known count and its one of the known spell, by return address) reads the book's spells at the
//   tab's level, then the page's other known ones (rules::WizardPageRows); everything else, the page's OK among
//   them, reads the client's own lists, so only the picks go to the server. A book spell counts as known before:
//   no remove icon, and Remove refuses it.
// - 0.78.4 (owner, 2026-11-05: "re-adding it back doesn't increase the counter to 6 and the OK button remains
//   greyed out"): the OK button needs all ten levels marked done (+0x19E0), and NWN marks a level only at the
//   page's setup and for the tab picked on. 2024's total is shared by the spell levels, so a pick on one tab leaves
//   the others' marks stale (a Sorcerer 3's level 2, 2 left at the setup, stays not done once level 1's picks fill
//   the total). After a pick or a drop, every level with nothing left is marked done and the OK button updated; a
//   pick refreshes the title too (Add rebuilds nothing).
// - 0.84.3 (owner, 2026-11-06: "the button didn't change after the spell being unprepared"): the client's player
//   update changes a class's known list (the Prepare spells window's Accept) without refreshing the quickbar - NWN
//   refreshes it for memorized spells and uses left only - so a spell dropped kept its count until the next
//   refresh. After the update's remove and add known spell (by return address), the quickbar's own refresh runs:
//   the button reads NP at once (rules_spell_slots.cpp).
// - 0.84.4 (owner, 2026-11-06: "why does the radial wheel show spells that have 0 slots? for example I can assign
//   color spray although that's not an available spell to prepare, nor is it in the spells menu"): the client's
//   RemoveKnownSpell finds a spell's level through the slot's class, so a spell that class has no level for (a
//   `!test` class swap - a Wizard's Color Spray under the Cleric that took its slot) stayed in the client's list:
//   the radial (a spellbook-restricted class lists its known spells) showed it with no uses. After the remove,
//   the spell comes out of whichever level of the slot still holds it.
// - 0.77.1 (owner, 2026-11-05: "again initial prepare spells pop up out of nowhere"): NWN's page setup gives a
//   class with LearnScroll at level 1 every cantrip (its known list's level 0 empty: all of them, the client's
//   AddKnownSpell in a loop) - 7 against 2024's 3, and the Prepare window opened to trim them. At that call the add is
//   skipped for a 2024 caster: the cantrips tab picks its 3.

#include "spells_page.h"

#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "prepare_spells.h"
#include "rules/cantrips.h"
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
using ClientRemoveKnownFn = uintptr_t (*)(void* client_stats, uintptr_t slot, uintptr_t spell);
using QuickbarRefreshFn = void (*)(void* quickbar);
using RemoveFn = void (*)(void* page);
using AddFn = void (*)(void* page);
using UpdateOkFn = void (*)(void* page);  // the page's OK button from its levels' done marks
using RemoveFromPanelFn = void (*)(void* object);  // CGuiObject::RemoveFromPanel
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
// 0.84.3: the player update's remove and add known spell (return addresses), the quickbar's refresh, its chain.
ClientRemoveKnownFn g_client_remove_known = nullptr;
QuickbarRefreshFn g_quickbar_refresh = nullptr;
uintptr_t g_update_remove_site = 0;
int g_stale_logged = 0;  // 0.84.4: the log's first lines only
uintptr_t g_update_add_site = 0;
void** g_app_manager = nullptr;
constexpr size_t kClientAppInternal = 0x8;
constexpr size_t kAppGui = 0x90;
constexpr size_t kGuiQuickbar = 0x60;
RemoveFn g_remove = nullptr;
AddFn g_add = nullptr;
UpdateOkFn g_update_ok = nullptr;
RemoveFromPanelFn g_remove_from_panel = nullptr;
std::vector<uintptr_t> g_next_tab_sites;  // 0.83.1: Add's looks at the next tab's spells left (return addresses)
// 0.83.3: ShowLevel's known pass - its known counts and its known spell read (return addresses); the rows it lists
// for a Wizard, kept for the pass (the client stats and the tab's level they were made for).
std::vector<uintptr_t> g_known_pass_count_sites;
uintptr_t g_known_pass_get_site = 0;
struct WizardRows {
  bool valid = false;
  void* client_stats = nullptr;
  int spell_level = -1;
  std::vector<int> ids;
};
thread_local WizardRows t_wizard_rows;
int g_hidden_logged = 0;  // the log's first lines only
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

// #39 build 2: Sorcerous Burst is the Sorcerer's only - NWN's one Wiz_Sorc column would give it the Wizard too.
bool IsSorcererOnly(const void* spell) {
  void* rules_object = RulesObject();
  void* spells = rules_object ? Field<void*>(rules_object, ee::kRulesSpells) : nullptr;
  return spells && g_get_spell && spell == g_get_spell(spells, rules::kSorcerousBurst);
}

// 3b-2: a spell already in the levelled Wizard's book isn't offered on the page's Available list again.
uint8_t SpellLevel(void* spell, uint8_t nwn_class) {
  const uint8_t level = g_spell_level(spell, nwn_class);
  if (nwn_class == kNwnClassWizard && level != 0xFF && IsSorcererOnly(spell)) return 0xFF;  // #39 build 2
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

// 0.83.3: the rows a Wizard's level-up page lists at a tab's level (null: not a Wizard's level 1+ tab) - its book's
// spells of that Wizard level, then its other known spells (prepared, picked; rules::WizardPageRows).
const std::vector<int>* WizardRowsFor(void* client_stats, uint8_t slot, uint8_t spell_level) {
  Levelled c;
  if (spell_level < 1 || spell_level > 9 || !LevelledClass(client_stats, &c) || c.row != kNwnClassWizard ||
      slot != c.slot) {
    return nullptr;
  }
  if (!t_wizard_rows.valid || t_wizard_rows.client_stats != client_stats || t_wizard_rows.spell_level != spell_level) {
    void* rules_object = RulesObject();
    void* spells = rules_object ? Field<void*>(rules_object, ee::kRulesSpells) : nullptr;
    std::vector<int> book;
    for (int id : PageBook(c)) {
      void* entry = spells && g_get_spell ? g_get_spell(spells, id) : nullptr;
      if (entry && g_spell_level(entry, kNwnClassWizard) == spell_level) book.push_back(id);
    }
    std::vector<int> known;
    const int count = g_known_count(client_stats, slot, spell_level);
    for (int i = 0; i < count && i < 255; ++i) {
      known.push_back(static_cast<int>(g_get_known(client_stats, slot, spell_level, static_cast<uint8_t>(i))));
    }
    t_wizard_rows = {true, client_stats, spell_level, rules::WizardPageRows(book, known)};
  }
  return &t_wizard_rows.ids;
}

uint16_t KnownCount(void* client_stats, uint8_t slot, uint8_t spell_level) {
  const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (uintptr_t site : g_known_pass_count_sites) {
    if (site != from) continue;
    if (const std::vector<int>* rows = WizardRowsFor(client_stats, slot, spell_level)) {
      return static_cast<uint16_t>(rows->size());
    }
  }
  return g_known_count(client_stats, slot, spell_level);
}

uint32_t GetKnown(void* client_stats, uint8_t slot, uint8_t spell_level, uint8_t index) {
  if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_known_pass_get_site) {
    const std::vector<int>* rows = WizardRowsFor(client_stats, slot, spell_level);
    if (rows && index < rows->size()) return static_cast<uint32_t>((*rows)[index]);
  }
  return g_get_known(client_stats, slot, spell_level, index);
}

// 0.77.1: the page setup's every-cantrip add skipped for a 2024 caster (the cantrips tab picks them).
// 0.84.3: the quickbar's numbers read again (the game's own refresh of all its buttons).
void RefreshQuickbar() {
  if (!g_quickbar_refresh || !g_app_manager || !*g_app_manager) return;
  void* client = *static_cast<void**>(*g_app_manager);
  void* app = client ? Field<void*>(client, kClientAppInternal) : nullptr;
  void* gui = app ? Field<void*>(app, kAppGui) : nullptr;
  void* quickbar = gui ? Field<void*>(gui, kGuiQuickbar) : nullptr;
  if (quickbar) g_quickbar_refresh(quickbar);
}

uintptr_t ClientAddKnown(void* client_stats, uintptr_t slot, uintptr_t spell) {
  const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (from == g_all_cantrips_site) {
    Levelled c;
    if (LevelledClass(client_stats, &c)) return 0;
  }
  const uintptr_t result = g_client_add_known(client_stats, slot, spell);
  if (from == g_update_add_site) RefreshQuickbar();  // 0.84.3: a spell the server added
  return result;
}

// 0.84.4: `spell` taken out of every level of the slot's known lists (as the remove does at the level its class
// gives: the ids shifted down, the count less one); the number taken out.
int DropFromKnownLists(void* client_stats, uint8_t slot, uint32_t spell) {
  if (!client_stats || slot >= ee::kClientStatsClassSlots) return 0;
  int dropped = 0;
  for (int level = 0; level <= 9; ++level) {
    uint8_t* list = static_cast<uint8_t*>(client_stats) + ee::kClientStatsKnownLists +
                    slot * ee::kClientStatsClassSlotSize + level * ee::kClientStatsKnownListSize;
    uint32_t* ids = *reinterpret_cast<uint32_t**>(list);
    int32_t* count = reinterpret_cast<int32_t*>(list + 8);
    for (int i = 0; ids && i < *count;) {
      if (ids[i] != spell) {
        ++i;
        continue;
      }
      memmove(ids + i, ids + i + 1, static_cast<size_t>(*count - i - 1) * sizeof(uint32_t));
      --*count;
      ++dropped;
    }
  }
  return dropped;
}

// 0.84.3: a spell the server dropped from a known list - its quickbar button reads NP at once. 0.84.4: one its
// class has no level for comes out too.
uintptr_t ClientRemoveKnown(void* client_stats, uintptr_t slot, uintptr_t spell) {
  const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
  const uintptr_t result = g_client_remove_known(client_stats, slot, spell);
  const int stale = DropFromKnownLists(client_stats, static_cast<uint8_t>(slot), static_cast<uint32_t>(spell));
  if (stale > 0 && g_stale_logged < 10) {
    ++g_stale_logged;
    Log("spells: client known list - spell %u taken out of class slot %u (its class has no level for it)",
        static_cast<unsigned>(spell), static_cast<unsigned>(slot & 0xFF));
  }
  if (from == g_update_remove_site) RefreshQuickbar();
  return result;
}

// A level's spells left to pick on the page: the engine's, bounded by 2024's total (a Wizard's: its spellbook).
uint8_t LeftOnPage(void* client_stats, uint8_t spell_level) {
  const uint8_t left = g_spells_left(client_stats, spell_level);
  Levelled c;
  if (spell_level == 0 || spell_level > 9 || !LevelledClass(client_stats, &c)) return left;
  if (c.row == kNwnClassWizard) {  // 3b-2: its spellbook
    return static_cast<uint8_t>(
        rules::SpellsLeftOnPage(left, rules::SpellbookSpells2024(c.level), BookWithPicks(client_stats, c)));
  }
  return static_cast<uint8_t>(
      rules::SpellsLeftOnPage(left, rules::PreparedSpells2024(c.row, c.level), KnownPrepared(client_stats, c)));
}

uint8_t SpellsLeft(void* client_stats, uint8_t spell_level) {
  t_page_stats = client_stats;
  if (spell_level == 1) {  // 0.83.1: Add looking past the cantrips tab - nothing there, so it stays
    const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
    Levelled c;
    for (uintptr_t site : g_next_tab_sites) {
      if (site == from && LevelledClass(client_stats, &c)) return 0;
    }
  }
  return LeftOnPage(client_stats, spell_level);
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

void HideUnremovableButtons(void* page);  // below (0.83.0)
void HideAddButtons(void* page);          // below (0.83.1)

void ShowLevel(void* page, int spell_level) {
  t_page_stats = nullptr;
  t_book_loaded = false;
  t_wizard_rows.valid = false;  // 0.83.3: made fresh for this pass
  g_show_level(page, spell_level);
  t_wizard_rows.valid = false;
  SetTitle(page, t_page_stats, spell_level, nullptr);
  HideUnremovableButtons(page);
  HideAddButtons(page);
}

// The page's client stats (its creature, page +0x19D8).
void* PageStats(void* page) {
  void* creature = g_client_creature ? g_client_creature(Field<uint32_t>(page, ee::kSpellsPageCreature)) : nullptr;
  return creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
}

bool ListHas(const void* list, uint32_t spell);

// A spell known before this level-up: in the page's copy of the known lists at its setup, or (0.83.3) a Wizard's
// book spell (its whole book is listed; the book never loses one).
bool KnownBefore(const Levelled& c, int spell_level, const void* saved, uint32_t spell) {
  if (ListHas(saved, spell)) return true;
  if (c.row != kNwnClassWizard || spell_level < 1) return false;
  const std::vector<int> book = PageBook(c);
  return std::find(book.begin(), book.end(), static_cast<int>(spell)) != book.end();
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

// 0.83.0: once a tab is built, a known row whose spell can't be dropped now - known before this level-up, its
// list's swap spent or none for the class (rules::RemoveIconShown) - has its move button (the row's +0xC8 child)
// taken out of the panel, as the familiar page's row takes out its own. The rows and their spell ids: the known
// list's button and id arrays (the ids are the page's +0x11C8, Remove's).
// The move buttons of a list's rows whose spell `hide_if` names, taken out of the panel - the row as mapped only
// (its two child buttons of one class, CGuiMouseoverChildButton). The count hidden.
template <typename HideIf>
int HideMoveButtons(const uint8_t* list, HideIf hide_if) {
  const uint32_t* ids = Field<const uint32_t*>(list, ee::kGuiListboxIds);
  const int id_count = Field<int32_t>(list, ee::kGuiListboxIds + ee::kExoArrayListCount);
  void* const* rows = Field<void* const*>(list, ee::kGuiListboxButtons);
  const int row_count = Field<int32_t>(list, ee::kGuiListboxButtons + ee::kExoArrayListCount);
  int hidden = 0;
  for (int i = 0; ids && rows && i < id_count && i < row_count && i < 4096; ++i) {
    uint8_t* row = static_cast<uint8_t*>(rows[i]);
    if (!row || !hide_if(ids[i])) continue;
    uint8_t* move = row + ee::kFeatButtonMoveButton;
    if (Field<void*>(move, 0) != Field<void*>(row + ee::kFeatButtonInfoButton, 0)) continue;
    g_remove_from_panel(move);
    ++hidden;
  }
  return hidden;
}

void HideUnremovableButtons(void* page) {
  void* const client_stats = page && g_remove_from_panel ? PageStats(page) : nullptr;
  Levelled c;
  if (!client_stats || !LevelledClass(client_stats, &c)) return;
  const int spell_level = Field<uint8_t>(page, ee::kSpellsPageLevel);
  if (spell_level > 9) return;
  const void* saved = static_cast<uint8_t*>(page) + ee::kSpellsPageSavedKnown + spell_level * ee::kExoArrayListSize;
  const bool cantrip = spell_level == 0;
  const bool swap_left = rules::LevelUpSwapAllowed(c.row, cantrip, SwapsDone(page, client_stats, c, cantrip));
  const int hidden = HideMoveButtons(static_cast<uint8_t*>(page) + ee::kSpellsPageKnownList, [&](uint32_t spell) {
    return !rules::RemoveIconShown(!KnownBefore(c, spell_level, saved, spell), swap_left);
  });
  if (hidden > 0 && g_hidden_logged < 20) {
    ++g_hidden_logged;
    Log("spells: the Spells page's level %d tab - %d known spells show no remove icon (known before, no swap "
        "left; #36)", spell_level, hidden);
  }
}

// 0.83.1: nothing left to pick on the tab - no add button on any available row.
void HideAddButtons(void* page) {
  void* const client_stats = page && g_remove_from_panel ? PageStats(page) : nullptr;
  Levelled c;
  if (!client_stats || !LevelledClass(client_stats, &c)) return;
  const int spell_level = Field<uint8_t>(page, ee::kSpellsPageLevel);
  if (spell_level > 9 || rules::AddIconShown(LeftOnPage(client_stats, static_cast<uint8_t>(spell_level)))) return;
  const int hidden =
      HideMoveButtons(static_cast<uint8_t*>(page) + ee::kSpellsPageAvailList, [](uint32_t) { return true; });
  if (hidden > 0 && g_hidden_logged < 20) {
    ++g_hidden_logged;
    Log("spells: the Spells page's level %d tab - nothing left to pick, %d add icons hidden (#36)", spell_level,
        hidden);
  }
}

// 0.78.4: every level with nothing left marked done (2024's total is shared by the levels; NWN marks only the tab
// picked on), then the OK button updated. A mark is only ever set: NWN's own "not done" for a level with spells
// left stays.
void RefreshLevelsDone(void* page) {
  void* const client_stats = PageStats(page);
  Levelled c;
  if (!client_stats || !LevelledClass(client_stats, &c)) return;
  for (int spell_level = 0; spell_level <= 9; ++spell_level) {
    if (LeftOnPage(client_stats, static_cast<uint8_t>(spell_level)) == 0) {
      SetField<int32_t>(page, ee::kSpellsPageLevelDone + spell_level * sizeof(int32_t), 1);
    }
  }
  g_update_ok(page);
}

// 0.78.4: a click on an available row (Add rebuilds nothing) - then the levels' done marks and the title. 0.83.0: a
// pick that undoes a swap (a dropped spell picked back) rebuilds the tab, the remove buttons the swap took back.
void Add(void* page) {
  void* const client_stats = page ? PageStats(page) : nullptr;
  Levelled c;
  const bool levelled = client_stats && LevelledClass(client_stats, &c);
  const int spell_level = page ? Field<uint8_t>(page, ee::kSpellsPageLevel) : 0;
  const bool cantrip = spell_level == 0;
  const int swaps_before = levelled ? SwapsDone(page, client_stats, c, cantrip) : 0;
  g_add(page);
  if (!page) return;
  RefreshLevelsDone(page);
  SetTitle(page, PageStats(page), Field<uint8_t>(page, ee::kSpellsPageLevel), nullptr);
  if (levelled && Field<uint8_t>(page, ee::kSpellsPageLevel) == spell_level &&
      SwapsDone(page, client_stats, c, cantrip) < swaps_before) {
    ShowLevel(page, spell_level);
    RefreshLevelsDone(page);
  } else if (levelled && Field<uint8_t>(page, ee::kSpellsPageLevel) == spell_level) {
    HideAddButtons(page);  // 0.83.1: the last pick - the add buttons go
  }
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
  if (spell_level >= 0 && spell_level <= 9 && KnownBefore(c, spell_level, saved, spell)) {  // known before: a swap
    const bool cantrip = spell_level == 0;
    if (!rules::LevelUpSwapAllowed(c.row, cantrip, SwapsDone(page, client_stats, c, cantrip))) {
      SetTitle(page, client_stats, spell_level, "No more swaps");
      return;
    }
  }
  g_remove(page);
  ShowLevel(page, spell_level);  // the tab rebuilt - 0.83.0: the remove buttons as the swaps left allow
  RefreshLevelsDone(page);       // 0.78.4
}

}  // namespace

bool ClientKnowsSpell(void* client_stats, int slot, int spell_level, uint32_t spell) {
  if (!client_stats || !g_known_count || !g_get_known || slot < 0 || slot > 255 || spell_level < 0 ||
      spell_level > 9) {
    return true;
  }
  const int count = g_known_count(client_stats, static_cast<uint8_t>(slot), static_cast<uint8_t>(spell_level));
  for (int i = 0; i < count && i < 255; ++i) {
    if (g_get_known(client_stats, static_cast<uint8_t>(slot), static_cast<uint8_t>(spell_level),
                    static_cast<uint8_t>(i)) == spell) {
      return true;
    }
  }
  return false;
}

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
  const uintptr_t add_inside = FindPattern("the Spells page's Add (#36)", ee::kSpellsPageAddPattern);
  const uintptr_t add = add_inside ? FunctionEntry(add_inside) : 0;
  g_remove_from_panel = reinterpret_cast<RemoveFromPanelFn>(EngineExport(ee::kGuiRemoveFromPanel));
  // 0.83.3: ShowLevel's known pass - the known count's first two calls in it and the known spell's first (the
  // Available pass, after it, has one of each).
  const uintptr_t known_count_fn = reinterpret_cast<uintptr_t>(g_known_count);
  const uintptr_t get_known_fn = reinterpret_cast<uintptr_t>(g_get_known);
  std::vector<uintptr_t> count_calls =
      show && known_count_fn ? FindCallsTo(FunctionAt(show), known_count_fn) : std::vector<uintptr_t>();
  std::vector<uintptr_t> get_calls =
      show && get_known_fn ? FindCallsTo(FunctionAt(show), get_known_fn) : std::vector<uintptr_t>();
  std::sort(count_calls.begin(), count_calls.end());
  std::sort(get_calls.begin(), get_calls.end());
  g_known_pass_count_sites.clear();
  g_known_pass_get_site = 0;
  const bool known_pass = count_calls.size() == 3 && get_calls.size() == 2 && count_calls[0] < get_calls[0] &&
                          get_calls[0] < count_calls[1] && count_calls[1] < count_calls[2] &&
                          count_calls[2] < get_calls[1];
  if (known_pass) {
    g_known_pass_count_sites = {count_calls[0] + 5, count_calls[1] + 5};
    g_known_pass_get_site = get_calls[0] + 5;
  } else {
    Log("SKIP spells #36: a Wizard's level-up page lists its prepared spells only (%zu / %zu known-list calls)",
        count_calls.size(), get_calls.size());
  }
  // 0.83.1: Add's two looks at the next tab - its calls into the client's spells left just after "inc dl; mov rcx,
  // rdi" (the level + 1); the third, for its own tab, isn't.
  g_next_tab_sites.clear();
  if (add && left) {
    static const uint8_t kNextLevel[] = {0xFE, 0xC2, 0x48, 0x8B, 0xCF};
    for (uintptr_t call : FindCallsTo(FunctionAt(add), left)) {
      if (memcmp(reinterpret_cast<const void*>(call - sizeof(kNextLevel)), kNextLevel, sizeof(kNextLevel)) == 0) {
        g_next_tab_sites.push_back(call + 5);
      }
    }
  }
  if (g_next_tab_sites.size() != 2) {
    Log("SKIP spells #36: the cantrips tab may move on to level 1 by itself (%zu of Add's next-tab looks found)",
        g_next_tab_sites.size());
    g_next_tab_sites.clear();
  }
  const uintptr_t ok_call =
      add ? FindPatternIn("Spells page: Add's OK update", FunctionAt(add), ee::kSpellsPageAddOkUpdatePattern) : 0;
  g_update_ok = reinterpret_cast<UpdateOkFn>(ok_call ? CallTarget(ok_call + 11) : 0);  // the second call
  g_all_cantrips_site = all_cantrips ? all_cantrips + 15 : 0;  // the call (+10) and its 5 bytes
  const uintptr_t quick = quick_inside ? FunctionEntry(quick_inside) : 0;
  g_quick_site = quick_inside ? quick_inside + 26 : 0;  // the pattern's call (+21) and its 5 bytes
  if (!left || !show || !quick || !g_known_count || !g_set_text || !g_make_string || !g_destroy_string ||
      !g_avail_site || !g_get_known || !g_get_spell || !client_add || !remove || !g_client_creature ||
      !add || !g_update_ok || !g_remove_from_panel) {
    Log("SKIP spells #36 3b: the Spells page isn't mapped (left %p, show %p, quick %p, known %p)",
        reinterpret_cast<void*>(left), reinterpret_cast<void*>(show), reinterpret_cast<void*>(quick),
        reinterpret_cast<void*>(g_known_count));
    return false;
  }
  // 0.84.3: the player update's known-list calls and the quickbar's refresh.
  const uintptr_t update = FunctionByString("the client's player update (#36 UC-9)", ee::kPlayerUpdateName);
  const CodeRange update_range = update ? FunctionAt(update) : CodeRange{};
  const uintptr_t remove_call = update ? FindPatternIn("the player update's known-spell remove (#36 UC-9)",
                                                       update_range, ee::kPlayerUpdateRemoveKnownPattern)
                                       : 0;
  const uintptr_t refresh_call = update ? FindPatternIn("the player update's quickbar refresh (#36 UC-9)",
                                                        update_range, ee::kPlayerUpdateQuickbarRefreshPattern)
                                        : 0;
  const std::vector<uintptr_t> update_adds =
      update ? FindCallsTo(update_range, client_add) : std::vector<uintptr_t>();
  const uintptr_t client_remove = remove_call ? CallTarget(remove_call + 9) : 0;
  g_quickbar_refresh = reinterpret_cast<QuickbarRefreshFn>(refresh_call ? CallTarget(refresh_call + 3) : 0);
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  const bool refresh_found = client_remove && g_quickbar_refresh && g_app_manager && update_adds.size() == 1;
  g_update_remove_site = refresh_found ? remove_call + 14 : 0;  // the call (+9) and its 5 bytes
  g_update_add_site = refresh_found ? update_adds[0] + 5 : 0;
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
                  reinterpret_cast<void*>(&Remove), reinterpret_cast<void**>(&g_remove)) &&
      HookAddress("CCharacterSpellsPanel Add (the title, the OK button, a swap undone, #36)", add,
                  reinterpret_cast<void*>(&Add), reinterpret_cast<void**>(&g_add)) &&
      (!known_pass ||
       (HookAddress("client known count (a Wizard's level-up page lists its whole book, #36)", known_count_fn,
                    reinterpret_cast<void*>(&KnownCount), reinterpret_cast<void**>(&g_known_count)) &&
        HookAddress("client known spell (a Wizard's level-up page lists its whole book, #36)", get_known_fn,
                    reinterpret_cast<void*>(&GetKnown), reinterpret_cast<void**>(&g_get_known))));
  Log("spells: NWN's Spells page at creation and level-up with 2024's counts - the total across levels, the title's "
      "totals; a Wizard's spellbook; the level-up swaps, no remove icon on a spell that can't be dropped (#36 "
      "Part 3b)%s",
      hooked ? "" : " - SOME hooks missing");
  const bool remove_hooked =
      client_remove && hooked &&
      HookAddress("client RemoveKnownSpell (a spell its class has no level for taken out too; the quickbar "
                  "refreshed after the server's known-list change, #36)",
                  client_remove, reinterpret_cast<void*>(&ClientRemoveKnown),
                  reinterpret_cast<void**>(&g_client_remove_known));
  Log(remove_hooked ? "spells: a known spell its class has no level for leaves the client's list too (a !test "
                      "class swap's old spells; #36)"
                    : "SKIP spells #36: a known spell its class has no level for stays in the client's list");
  const bool refresh = remove_hooked && refresh_found;
  if (!refresh) g_update_add_site = 0;
  Log(refresh ? "spells: the quickbar reads its numbers again after the server changes a known list (a spell "
                "dropped reads NP at once; #36 UC-9)"
              : "SKIP spells #36 UC-9: the quickbar's numbers wait for its next refresh after a known-list change "
                "(remove %p, refresh %p, %zu adds)",
      reinterpret_cast<void*>(client_remove), reinterpret_cast<void*>(g_quickbar_refresh), update_adds.size());
  return hooked;
}
