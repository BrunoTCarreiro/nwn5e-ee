// #140, radial casting retargets with the area shown (docs/PLAN.md "Phase 7"; owner, 2026-11-06: "I have an issue
// with spells picked from the radial menu not showing the area of effect of a spell ... we're targeting blind";
// decided "Q1. right Q2. b Q3. a Q4. before"). The rules: rules/radial_targeting.h.
//
// The client's radial leaves cast at once at what the menu was opened on. The spell menu's handler (guispellmenu)
// keeps it at +0x88C (an object id; OBJECT_INVALID: the ground) and +0x880 (a point); +0x890 set means the
// quickbar's assign mode. Its leaves: a spell (the cast callback #36 knows, 0x9A4540 in 8193.37), a spell-like
// ability (0x9A4610: CastSpellLikeAbility), a feat (0x9A4770). The creature menu's own feat leaf, the class
// abilities' and Special Attacks' (RadialMenuUseFeat, 0x998520; param feat | extra << 16) keeps them at +0x7F8 and
// +0x830, assign mode +0x828. Both feat leaves already hand over the feat cursor for a feat that isn't self-only
// (feat.2da TARGETSELF, CNWFeat +0x2C) when the menu was opened on yourself, and use it at once (0x805310: the
// client creature, the feat, the target, the point) otherwise.
//
// The quickbar's spell button starts the spell cursor: a 0x168-byte object (the game's operator new, its
// constructor 0x704290), then 0x704500(cursor, param, an empty CResRef, 0 for a spell / 1 for a spell-like
// ability), which sets the valid targets from spells.2da TargetType, the area preview from TargetShape (0x704BD0)
// and activates it; a pick casts with the same param (class, domain level - #36's upcast level -, metamagic), Esc
// drops it, both free it. The feat cursor is 0x160 bytes (0x702800, then 0x702C30(cursor, 1, feat, CResRef)), its
// area its spell's. Here each leaf asks rules::RadialRetargets: true starts the cursor instead of the cast; false
// leaves the game's own (a feat that isn't self-only opened on yourself, which the game would hand the cursor, is
// used on yourself when the rules say so). What the menu was opened on: yourself (the controlled creature), a
// creature friendly to it (an ally: GetAIStateReaction friend), another creature, door, placeable or item, or the
// ground (anything else). The spells' facts: spells.2da Range, TargetType, HostileSetting, TargetShape, and
// feat.2da SPELLID, TARGETSELF, read once (as the game serves them). `radial_retarget`.

#include <windows.h>

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules/radial_targeting.h"
#include "spell_impact.h"

namespace {

using LeafFn = void (*)(void* handler, int64_t param);
using GameNewFn = void* (*)(size_t size);
using CursorCtorFn = void* (*)(void* cursor);
using SpellCursorStartFn = void (*)(void* cursor, int64_t param, const void* cursor_resref, int ability);
using FeatCursorStartFn = void (*)(void* cursor, uint8_t from_menu, int64_t feat, const void* cursor_resref);
using UseFeatFn = void (*)(void* client_creature, uint32_t feat, uint32_t target, const void* point);
using ControlledFn = void* (*)(void* client_app);
using GetObjectFn = void* (*)(void* server_app, uint32_t id);

LeafFn g_spell_leaf = nullptr;    // originals (trampolines)
LeafFn g_ability_leaf = nullptr;
LeafFn g_menu_feat_leaf = nullptr;
LeafFn g_creature_feat_leaf = nullptr;
GameNewFn g_game_new = nullptr;
CursorCtorFn g_spell_cursor_ctor = nullptr;
SpellCursorStartFn g_spell_cursor_start = nullptr;
CursorCtorFn g_feat_cursor_ctor = nullptr;
FeatCursorStartFn g_feat_cursor_start = nullptr;
UseFeatFn g_use_feat = nullptr;
ControlledFn g_controlled = nullptr;
void** g_controlled_app = nullptr;
GetObjectFn g_get_object = nullptr;
void** g_app_manager = nullptr;

// spells.2da and feat.2da, as the game serves them (read at the first radial pick).
struct Tables {
  bool read = false;
  std::vector<std::string> range, target_type, hostile, shape, feat_spell, feat_self;
};
Tables g_tables;
int g_logged = 0;
constexpr int kMaxLogged = 60;

const Tables& ReadTables() {
  if (!g_tables.read) {
    g_tables.read = true;
    g_tables.range = GameTableColumn("spells", "Range");
    g_tables.target_type = GameTableColumn("spells", "TargetType");
    g_tables.hostile = GameTableColumn("spells", "HostileSetting");
    g_tables.shape = GameTableColumn("spells", "TargetShape");
    g_tables.feat_spell = GameTableColumn("feat", "SPELLID");
    g_tables.feat_self = GameTableColumn("feat", "TARGETSELF");
    Log("radial (#140): read %zu spells.2da rows (%zu with a TargetShape column), %zu feat.2da rows",
        g_tables.range.size(), g_tables.shape.size(), g_tables.feat_spell.size());
  }
  return g_tables;
}

bool SpellFacts(int spell, rules::RadialSpell* out) {
  const Tables& t = ReadTables();
  const size_t row = static_cast<size_t>(spell);
  if (spell < 0 || row >= t.range.size() || row >= t.target_type.size() || row >= t.hostile.size() ||
      row >= t.shape.size()) {
    return false;
  }
  *out = rules::RadialSpellFromRow(t.range[row], t.target_type[row], t.hostile[row], t.shape[row]);
  return true;
}

// The feat's spell (feat.2da SPELLID; -1: none) and whether it only targets its user (TARGETSELF).
int FeatSpell(int feat, bool* self_only) {
  const Tables& t = ReadTables();
  const size_t row = static_cast<size_t>(feat);
  if (feat < 0 || row >= t.feat_spell.size() || t.feat_spell[row].empty()) return -1;
  *self_only = row < t.feat_self.size() && t.feat_self[row] == "1";
  return atoi(t.feat_spell[row].c_str());
}

void* ControlledCreature() {
  return g_controlled && g_controlled_app && *g_controlled_app ? g_controlled(*g_controlled_app) : nullptr;
}

void* ServerApp() {
  return g_app_manager && *g_app_manager ? Field<void*>(*g_app_manager, ee::kAppManagerServerApp) : nullptr;
}

rules::RadialOn Classify(uint32_t target, uint32_t self_id, uint8_t* kind) {
  *kind = 0;
  if (target == self_id) return rules::RadialOn::kSelf;
  if (target == ee::kObjectInvalid) return rules::RadialOn::kGround;
  void* app = ServerApp();
  void* object = app && g_get_object ? g_get_object(app, target) : nullptr;
  if (!object) return rules::RadialOn::kGround;
  *kind = Field<uint8_t>(object, ee::kGameObjectType);
  if (*kind == ee::kObjectTypeCreature) {
    void* self = CreatureById(self_id);
    return self && engine::IsFriend(self, target) ? rules::RadialOn::kAlly : rules::RadialOn::kOther;
  }
  if (*kind == ee::kObjectTypeItem || *kind == ee::kObjectTypePlaceable || *kind == ee::kObjectTypeDoor) {
    return rules::RadialOn::kOther;
  }
  return rules::RadialOn::kGround;
}

void Note(const char* what, int spell, rules::RadialOn on, uint32_t target, uint8_t kind, const char* outcome) {
  if (g_logged >= kMaxLogged) return;
  ++g_logged;
  Log("radial (#140): %s %s (%d) on %s (0x%08x, kind %d) -> %s%s", what, SpellNameOf(spell).c_str(), spell,
      rules::RadialOnName(on), target, kind, outcome, g_logged == kMaxLogged ? " (no more of these lines)" : "");
}

bool StartSpellCursor(int64_t param, bool ability) {
  void* cursor = g_game_new(ee::kSpellCursorSize);
  if (!cursor) return false;
  cursor = g_spell_cursor_ctor(cursor);
  alignas(16) uint8_t no_cursor[32] = {};  // an empty CResRef: the game's own cursor
  g_spell_cursor_start(cursor, param, no_cursor, ability ? 1 : 0);
  return true;
}

bool StartFeatCursor(int64_t param) {
  void* cursor = g_game_new(ee::kFeatCursorSize);
  if (!cursor) return false;
  cursor = g_feat_cursor_ctor(cursor);
  alignas(16) uint8_t no_cursor[32] = {};
  g_feat_cursor_start(cursor, 1, param, no_cursor);
  return true;
}

// A spell or spell-like ability leaf: true when the cursor took over.
bool SpellLeaf(void* handler, int64_t param, bool ability) {
  if (!g_config.radial_retarget || !handler || Field<void*>(handler, ee::kRadialHandlerAssigning)) return false;
  void* creature = ControlledCreature();
  const int spell = static_cast<int>(param & 0xffff);
  rules::RadialSpell facts;
  if (!creature || !SpellFacts(spell, &facts)) return false;
  const uint32_t target = Field<uint32_t>(handler, ee::kSpellMenuTarget);
  uint8_t kind = 0;
  const rules::RadialOn on = Classify(target, Field<uint32_t>(creature, ee::kClientObjectId), &kind);
  const char* what = ability ? "ability" : "spell";
  if (!rules::RadialRetargets(on, facts)) {
    Note(what, spell, on, target, kind, "cast at once");
    return false;
  }
  const bool started = StartSpellCursor(param, ability);
  Note(what, spell, on, target, kind, started ? "the targeting cursor, its area shown" : "no cursor (cast at once)");
  return started;
}

void SpellLeafHook(void* handler, int64_t param) {
  if (!SpellLeaf(handler, param, false)) g_spell_leaf(handler, param);
}

void AbilityLeafHook(void* handler, int64_t param) {
  if (!SpellLeaf(handler, param, true)) g_ability_leaf(handler, param);
}

struct FeatMenu {
  size_t target, point, assigning;
};
constexpr FeatMenu kSpellMenu{ee::kSpellMenuTarget, ee::kSpellMenuPoint, ee::kRadialHandlerAssigning};
constexpr FeatMenu kCreatureMenu{ee::kCreatureMenuTarget, ee::kCreatureMenuPoint, ee::kCreatureMenuAssigning};

// A feat leaf: true when the cursor took over, or the feat was used on yourself where the game would have handed
// the cursor.
bool FeatLeaf(void* handler, int64_t param, const FeatMenu& menu, int64_t cursor_param, uint32_t use_param) {
  if (!g_config.radial_retarget || !handler || Field<void*>(handler, menu.assigning)) return false;
  for (int64_t mode : ee::kCreatureMenuModeFeats) {
    if (param == mode) return false;  // a mode the game toggles before anything else
  }
  void* creature = ControlledCreature();
  bool self_only = false;
  const int spell = creature ? FeatSpell(static_cast<int>(param & 0xffff), &self_only) : -1;
  rules::RadialSpell facts;
  if (spell < 0 || !SpellFacts(spell, &facts)) return false;
  facts.at_once = facts.at_once || self_only;
  const uint32_t target = Field<uint32_t>(handler, menu.target);
  uint8_t kind = 0;
  const rules::RadialOn on = Classify(target, Field<uint32_t>(creature, ee::kClientObjectId), &kind);
  if (rules::RadialRetargets(on, facts)) {
    const bool started = StartFeatCursor(cursor_param);
    Note("feat", spell, on, target, kind, started ? "the targeting cursor, its area shown" : "no cursor (used at once)");
    return started;
  }
  if (on == rules::RadialOn::kSelf && !self_only) {
    // The game would hand over the cursor; the rules say yours.
    g_use_feat(creature, use_param, target, static_cast<const uint8_t*>(handler) + menu.point);
    Note("feat", spell, on, target, kind, "used on yourself");
    return true;
  }
  Note("feat", spell, on, target, kind, "used at once");
  return false;
}

void MenuFeatLeafHook(void* handler, int64_t param) {
  if (!FeatLeaf(handler, param, kSpellMenu, param, static_cast<uint32_t>(param))) g_menu_feat_leaf(handler, param);
}

void CreatureFeatLeafHook(void* handler, int64_t param) {
  if (!FeatLeaf(handler, param, kCreatureMenu, rules::CreatureMenuFeatCursorParam(param),
                rules::CreatureMenuFeatUseParam(param))) {
    g_creature_feat_leaf(handler, param);
  }
}

}  // namespace

// The four leaves and the two cursors, found by their bytes; any missing: the radial stays the game's (logged).
void InstallRadialRetarget() {
  if (!g_config.radial_retarget) {
    Log("radial (#140): radial_retarget=0 - the radial casts at once (the game's own)");
    return;
  }
  const uintptr_t spell_leaf = FindPattern("the radial's spell leaf (#140)", ee::kRadialCastCallbackPattern);
  const uintptr_t ability_leaf = FindPattern("the radial's spell-like ability leaf (#140)",
                                             ee::kRadialAbilityCallbackPattern);
  const uintptr_t menu_feat_leaf = FindPattern("the spell menu's feat leaf (#140)", ee::kRadialMenuFeatPattern);
  const uintptr_t creature_feat_leaf = FindPattern("the creature menu's RadialMenuUseFeat (#140)",
                                                   ee::kCreatureMenuUseFeatPattern);
  const uintptr_t spell_cursor = FindPattern("the quickbar's spell cursor (#140)", ee::kQuickbarSpellCursorPattern);
  const uintptr_t feat_cursor = menu_feat_leaf ? FindPatternIn("the radial's feat cursor (#140)",
                                                               FunctionAt(menu_feat_leaf), ee::kRadialFeatCursorPattern)
                                               : 0;
  const uintptr_t use_feat = menu_feat_leaf ? FindPatternIn("the radial's feat use (#140)", FunctionAt(menu_feat_leaf),
                                                            ee::kRadialUseFeatPattern)
                                            : 0;
  const uint8_t* lookup = spell_leaf ? reinterpret_cast<const uint8_t*>(spell_leaf + ee::kRadialCastControlled)
                                     : nullptr;
  if (lookup && lookup[0] == 0x48 && lookup[1] == 0x8B && lookup[2] == 0x05 && lookup[7] == 0x48 &&
      lookup[8] == 0x8B && lookup[9] == 0x08 && lookup[10] == 0xE8) {
    const int32_t rel = *reinterpret_cast<const int32_t*>(lookup + 3);
    g_controlled_app = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(lookup) + 7 + rel);
    g_controlled = reinterpret_cast<ControlledFn>(CallTarget(reinterpret_cast<uintptr_t>(lookup) + 10));
  }
  if (spell_cursor) {
    g_game_new = reinterpret_cast<GameNewFn>(CallTarget(spell_cursor + ee::kQuickbarSpellCursorNew));
    g_spell_cursor_ctor = reinterpret_cast<CursorCtorFn>(CallTarget(spell_cursor + ee::kQuickbarSpellCursorCtor));
    g_spell_cursor_start =
        reinterpret_cast<SpellCursorStartFn>(CallTarget(spell_cursor + ee::kQuickbarSpellCursorStart));
  }
  if (feat_cursor) {
    g_feat_cursor_ctor = reinterpret_cast<CursorCtorFn>(CallTarget(feat_cursor + ee::kRadialFeatCursorCtor));
    g_feat_cursor_start = reinterpret_cast<FeatCursorStartFn>(CallTarget(feat_cursor + ee::kRadialFeatCursorStart));
  }
  if (use_feat) g_use_feat = reinterpret_cast<UseFeatFn>(CallTarget(use_feat + ee::kRadialUseFeatCall));
  g_get_object = reinterpret_cast<GetObjectFn>(EngineExport(ee::kGetGameObject));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!spell_leaf || !ability_leaf || !menu_feat_leaf || !creature_feat_leaf || !g_controlled || !g_game_new ||
      !g_spell_cursor_ctor || !g_spell_cursor_start || !g_feat_cursor_ctor || !g_feat_cursor_start || !g_use_feat ||
      !g_get_object || !g_app_manager) {
    Log("SKIP radial (#140): the radial's leaves or the cursors aren't mapped (spell %p, ability %p, feat %p / %p, "
        "cursors %p / %p, creature %p) - the radial casts at once",
        reinterpret_cast<void*>(spell_leaf), reinterpret_cast<void*>(ability_leaf),
        reinterpret_cast<void*>(menu_feat_leaf), reinterpret_cast<void*>(creature_feat_leaf),
        reinterpret_cast<void*>(spell_cursor), reinterpret_cast<void*>(feat_cursor),
        reinterpret_cast<void*>(g_controlled));
    return;
  }
  HookAddress("the radial's spell leaf (#140: retarget, the area shown)", spell_leaf,
              reinterpret_cast<void*>(&SpellLeafHook), reinterpret_cast<void**>(&g_spell_leaf));
  HookAddress("the radial's spell-like ability leaf (#140)", ability_leaf, reinterpret_cast<void*>(&AbilityLeafHook),
              reinterpret_cast<void**>(&g_ability_leaf));
  HookAddress("the spell menu's feat leaf (#140)", menu_feat_leaf, reinterpret_cast<void*>(&MenuFeatLeafHook),
              reinterpret_cast<void**>(&g_menu_feat_leaf));
  HookAddress("the creature menu's RadialMenuUseFeat (#140)", creature_feat_leaf,
              reinterpret_cast<void*>(&CreatureFeatLeafHook), reinterpret_cast<void**>(&g_creature_feat_leaf));
}
