// #126 / #182, the radial menu's NWN / 3e entries left out in faithful (docs/PLAN.md #126; owner, 2026-11-21: "we
// need to look for nwn/3e specific menu items to remove them"). What goes: rules/radial_cleanup.h. How, in the client
// (89.8193.37 RVAs; the #126 brief):
//
// - The pages are code, not data, and a CRadialMenu has no "clear" (an entry is SetEntryx / SetSubMenuCreatorx'd or
//   not). An entry tied to a feat is added only when the page's builder finds the feat on the client creature's feat
//   lists: RadialFeatsMasked takes the page's 3e feats off them while the builder runs (radial_class_feats.cpp's way
//   with cls_feat OnMenu). An entry the page always adds is a call to the setter after its icon's lea: that call
//   becomes a NOP (RemoveRadialFixedEntries) - its slot stays empty. A DisableEntry the page makes for the slot after
//   it only sets a bit HandleMenuAction / BuildIcon read for an entry that exists: harmless on an empty slot.
// - Special Attacks (995090, hooked by radial_special_attacks.cpp): Parry's entry (isk_parry, always); its 3e feats.
// - Special Abilities (994d10, found by the Taunt icon's one lea): Crafting Skills (ir_craft, slot 0) and Taunt
//   (isk_taunt, slot 1), both always added. Its class sections: radial_class_feats.cpp.
// - Cast Spell: the spell menu's constructor (9a10e0, found by the IR_SPELL_ABS icon's one lea; the creature menus'
//   Cast Spell, the quickbar's Assign Spell, the innate abilities' page, ... all build through it) adds the 3e
//   Metamagic page (IR_METAMAGIC, slot 0 - or a class's slot - when the character has Empower ... Still Spell:
//   the place the 2024 Sorcerer's Metamagic takes later), Epic Spells (IR_EPICSPELL, slot 2, with Epic Character)
//   and, for a caster, Spell Special Abilities (IR_SPELL_ABS, slot 6: Counterspell and Defensive Casting Mode, built
//   when opened by 9a37a0 - nothing else on it). The constructor is hooked to mask the feats; the last call removed.
// - The quickbar's assign menus call these same builders (9acc90 -> 995090, 9acbd0 -> 994d10, 9ac160 -> 9a10e0):
//   what they offer is cleaned with the pages.
// - The spontaneous Cure / Inflict ring (CreateSpontaneousCastingMenuRadial, 992920, from 9927f0) has no caller in
//   this build (no call, jump, lea or pointer reaches 9927f0), and spellcasting faithful serves classes.2da with
//   CanCastSpontaneously 0 (#36) and refuses a character Cleric's spontaneous order (#42): nothing left to remove.
// - Which ring opened (0.99.111's first test, owner: "there's nothing much on the radial" - the log showed only
//   Special Abilities pages built for a menu with no creature: the ring the ground opens, 84def0 - Cast Spell, Move
//   To, More Actions, Special Abilities -, which never had Special Attacks): the creature menu's ring builder (98e550:
//   yours 98ec00, a DM's, your associate's, another creature's 993790) and the ground's are hooked to log the first
//   rings - whose, and the slots set (+0xf8 + slot * 8, a label each); the ground's builds its own CRadialMenu, seen
//   by the game's constructor count (g_nCRadialMenu_ctorCounter: it may return without one).

#include "radial_pages.h"

#include <stdio.h>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"

namespace {

using PlayerCreatureFn = void* (*)(void* client_app);
using SpellMenuCtorFn = void* (*)(void* self, void* point, uint64_t target, uint64_t assigning, uint64_t from_menu);
using CreatureRingFn = void (*)(void* creature_menu, uint64_t a2);
using GroundRingFn = void (*)(void* owner, const void* point);

PlayerCreatureFn g_player_creature = nullptr;
SpellMenuCtorFn g_spell_menu = nullptr;
void** g_app_manager = nullptr;  // &g_pAppManager
int g_cast_logged = 0;
CreatureRingFn g_creature_ring = nullptr;
GroundRingFn g_ground_ring = nullptr;
const int32_t* g_menus_made = nullptr;  // &g_nCRadialMenu_ctorCounter
int g_rings_logged = 0;

constexpr const char* kAppManager = "?g_pAppManager@@3PEAVCAppManager@@EA";
constexpr const char* kSetEntry = "?SetEntryx@CRadialMenu@@QEAAXHPEBDIPEAXP8CCallbackHandlerBase@@EAAX_J@Z2@Z";
constexpr const char* kSetCreator =
    "?SetSubMenuCreatorx@CRadialMenu@@QEAAXHPEBDIPEAXP8CCallbackHandlerBase@@EAAPEAV1@_J@Z2@Z";
constexpr size_t kClientCreatureStats = 0x3b0;
constexpr size_t kStatsFeatLists[] = {0x118, 0x128};  // CExoArrayList<uint16>: data, count at +8
constexpr uint16_t kNoFeat = 0xffff;                  // past feat.2da: CNWRules::GetFeat gives null, nothing has it
constexpr size_t kEntryCallWithin = 64;               // the setter's call after the icon's lea (12-42 bytes here)
constexpr const char* kSpecialAbilitiesAnchor = "isk_taunt";  // referenced by the Special Abilities page alone
constexpr const char* kCastSpellAnchor = "IR_SPELL_ABS";      // by the spell menu's constructor alone
constexpr const char* kGroundRingAnchor = "IR_MOVETO";        // by the ground's ring alone
constexpr const char* kMenusMade = "?g_nCRadialMenu_ctorCounter@@3HA";
// The creature menu's ring builder, at its own-ring check: mov eax,[rax+8]; cmp [rdi+7F8h],eax; jne; mov rcx,rdi;
// mov rbx,[rsp+60h]; add rsp,40h; pop rdi; jmp <your ring>; cmp [rdi+804h],eax (your associate's?); jne
constexpr const char* kCreatureRingPattern =
    "8B 40 08 39 87 F8 07 00 00 75 12 48 8B CF 48 8B 5C 24 60 48 83 C4 40 5F E9 ?? ?? ?? ?? 39 87 04 08 00 00 0F 85";
constexpr size_t kMenuTarget = 0x7f8;
constexpr size_t kMenuTargetMaster = 0x804;
constexpr size_t kMenuLabels = 0xf8;  // per slot, 8 bytes
constexpr size_t kClientObjectId = 0x8;
constexpr int kRingsLogged = 8;
// The ground's ring's game entries (84def0; a DM's two more not counted): Cast Spell, Move To, More Actions,
// Special Abilities.
constexpr unsigned kGroundRingSlots = 0x4e;

const char* PageName(rules::RadialPage page) {
  switch (page) {
    case rules::RadialPage::kSpecialAttacks:
      return "Special Attacks";
    case rules::RadialPage::kSpecialAbilities:
      return "Special Abilities";
    case rules::RadialPage::kCastSpell:
      return "Cast Spell";
  }
  return "?";
}

void* Player() {
  void* app = g_app_manager && *g_app_manager ? *static_cast<void**>(*g_app_manager) : nullptr;
  return app && g_player_creature ? g_player_creature(app) : nullptr;
}

// The one function containing the only lea of `text`; 0 (logged) otherwise.
uintptr_t FunctionReferencing(const char* what, const char* text) {
  const std::vector<uintptr_t> references = FindStringReferences(text);
  if (references.size() != 1) {
    Log("SKIP radial: %s - \"%s\" referenced %zu times, not once", what, text, references.size());
    return 0;
  }
  return FunctionEntry(references[0]);
}

void* SpellMenu(void* self, void* point, uint64_t target, uint64_t assigning, uint64_t from_menu) {
  RadialFeatsMasked masked(Player(), rules::RadialPage::kCastSpell);
  void* menu = g_spell_menu(self, point, target, assigning, from_menu);
  if (masked.Count() && g_cast_logged < 3) {
    ++g_cast_logged;
    Log("radial: Cast Spell built without the character's 3e Metamagic / Epic Character feats (%s)",
        masked.Feats().c_str());
  }
  return menu;
}

uint32_t PlayerId() {
  void* player = Player();
  return player ? Field<uint32_t>(player, kClientObjectId) : rules::kRadialNoObject;
}

void CreatureRing(void* creature_menu, uint64_t a2) {
  g_creature_ring(creature_menu, a2);
  if (!creature_menu || g_rings_logged >= kRingsLogged) return;
  ++g_rings_logged;
  unsigned slots = 0;
  for (int slot = 0; slot < 8; ++slot) {
    if (Field<void*>(creature_menu, kMenuLabels + slot * 8)) slots |= 1u << slot;
  }
  const uint32_t target = Field<uint32_t>(creature_menu, kMenuTarget);
  const rules::RadialRing ring =
      rules::RadialRingOf(target, Field<uint32_t>(creature_menu, kMenuTargetMaster), PlayerId());
  Log("radial: %s ring opened (0x%08X) - %s", rules::RadialRingName(ring), target,
      rules::RadialRingSlots(ring, slots).c_str());
}

void GroundRing(void* owner, const void* point) {
  const int32_t before = g_menus_made ? *g_menus_made : 0;
  g_ground_ring(owner, point);
  if (!g_menus_made || *g_menus_made == before || g_rings_logged >= kRingsLogged) return;
  ++g_rings_logged;
  Log("radial: the ground's ring opened - %s (Special Attacks is on your own ring: right-click your character or "
      "its portrait)",
      rules::RadialRingSlots(rules::RadialRing::kGround, kGroundRingSlots).c_str());
}

// The log of the first rings opened - whose, and what they hold - in either mode.
void InstallRingLog() {
  const uintptr_t check = FindPattern("the creature menu's ring builder (its own-ring check)", kCreatureRingPattern);
  HookAddress("radial: the creature menu's rings (logged: whose, the slots set)", check ? FunctionEntry(check) : 0,
              reinterpret_cast<void*>(&CreatureRing), reinterpret_cast<void**>(&g_creature_ring));
  g_menus_made = reinterpret_cast<const int32_t*>(EngineExport(kMenusMade));
  const uintptr_t ground = g_menus_made ? FunctionReferencing("the ground's ring", kGroundRingAnchor) : 0;
  HookAddress("radial: the ground's ring (logged)", ground, reinterpret_cast<void*>(&GroundRing),
              reinterpret_cast<void**>(&g_ground_ring));
}

}  // namespace

RadialFeatsMasked::RadialFeatsMasked(void* player, rules::RadialPage page) {
  if (!player || g_config.radial != rules::RuleVariant::kFaithful) return;
  void* stats = Field<void*>(player, kClientCreatureStats);
  if (!stats) return;
  for (size_t list : kStatsFeatLists) {
    uint16_t* data = Field<uint16_t*>(stats, list);
    const int32_t count = Field<int32_t>(stats, list + 8);
    for (int32_t i = 0; data && i < count && i < 4096; ++i) {
      if (!rules::RadialPageHidesFeat(g_config.radial, page, data[i])) continue;
      masked_.push_back({&data[i], data[i]});
      data[i] = kNoFeat;
    }
  }
}

RadialFeatsMasked::~RadialFeatsMasked() {
  for (auto it = masked_.rbegin(); it != masked_.rend(); ++it) *it->first = it->second;
}

std::string RadialFeatsMasked::Feats() const {
  std::string feats;
  for (const auto& masked : masked_) {
    char number[16];
    snprintf(number, sizeof(number), "%s%u", feats.empty() ? "" : " ", static_cast<unsigned>(masked.second));
    feats += number;
  }
  return feats;
}

bool RemoveRadialFixedEntries(rules::RadialPage page, uintptr_t builder) {
  size_t count = 0;
  const rules::RadialFixedEntry* entries = rules::RadialFixedEntries(g_config.radial, &count);
  const CodeRange range = builder ? FunctionAt(builder) : CodeRange();
  const uintptr_t set_entry = EngineExport(kSetEntry);
  const uintptr_t set_creator = EngineExport(kSetCreator);
  std::vector<CallRemoval> calls;
  std::vector<std::string> names;
  names.reserve(count);
  std::string list;
  for (size_t i = 0; entries && i < count; ++i) {
    const rules::RadialFixedEntry& entry = entries[i];
    if (entry.page != page) continue;
    char name[128];
    snprintf(name, sizeof(name), "%s (%s slot %d)", entry.name, PageName(page), entry.slot);
    names.push_back(name);
    list += (list.empty() ? "" : ", ") + std::string(entry.name);
    const uintptr_t target = entry.sub_page ? set_creator : set_entry;
    uintptr_t icon = 0;
    int references = 0;
    for (uintptr_t reference : FindStringReferences(entry.icon)) {
      if (range.Contains(reference)) {
        icon = reference;
        ++references;
      }
    }
    uintptr_t call = 0;
    if (references == 1 && target) {
      for (uintptr_t site : FindCallsTo(range, target)) {
        if (site > icon && site - icon <= kEntryCallWithin) {
          call = site;
          break;
        }
      }
    }
    calls.push_back({names.back().c_str(), call, target});
  }
  if (calls.empty()) return false;
  char what[96];
  snprintf(what, sizeof(what), "radial (faithful, #126): the %s page", PageName(page));
  if (!range.Valid()) {
    Log("SKIP %s: its code was not found - %s stay", what, list.c_str());
    return false;
  }
  if (!RemoveCalls(what, calls.data(), calls.size())) return false;
  Log("radial: the %s page without %s (faithful)", PageName(page), list.c_str());
  return true;
}

void InstallRadialPages(void* (*player_creature)(void* client_app)) {
  g_player_creature = player_creature;
  g_app_manager = reinterpret_cast<void**>(EngineExport(kAppManager));
  InstallRingLog();
  if (g_config.radial != rules::RuleVariant::kFaithful) return;
  RemoveRadialFixedEntries(rules::RadialPage::kSpecialAbilities,
                           FunctionReferencing("the Special Abilities page", kSpecialAbilitiesAnchor));
  const uintptr_t spell_menu = FunctionReferencing("the spell menu (Cast Spell)", kCastSpellAnchor);
  RemoveRadialFixedEntries(rules::RadialPage::kCastSpell, spell_menu);
  if (!g_player_creature || !g_app_manager) {
    Log("SKIP radial: Cast Spell keeps 3e Metamagic and Epic Spells for a character with their feats - the client's "
        "creature getter wasn't found");
    return;
  }
  if (HookAddress("radial Cast Spell (the spell menu; 3e Metamagic and Epic Spells left out)", spell_menu,
                  reinterpret_cast<void*>(&SpellMenu), reinterpret_cast<void**>(&g_spell_menu))) {
    Log("radial: Cast Spell without the 3e Metamagic page (Empower ... Still Spell; the 2024 Sorcerer's Metamagic "
        "goes there later) and Epic Spells, for a character with their feats (faithful)");
  }
}
