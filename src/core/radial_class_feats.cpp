// Another creature's radial menu without your self-only class feats (0.41.2;
// owner: "why do enemies have in their radial menu, a barbarian section that
// is filled with greyed out options? I can't cast reckless attack on an enemy
// so why is it taking up a slot in their radial menu?").
//
// The client (CGuiCreatureMenu, nwccreaturemenu.cpp) puts a class's abilities
// on a creature's radial from the class's feat list (cls_feat_*: entries of 16
// bytes at CNWClass +0x2b0, count +0x2b8; the feat id at +0, OnMenu at +0xc):
// every OnMenu 1 feat the character has, whoever the menu is for. One
// unexported function collects, per class of the player (count: client stats
// +0x69), a section (2+ such feats), the feat itself (1) or nothing (0); the
// main menu and its "more abilities" page lay those out, and a section's own
// page (CreateClassAbilityMenu) is built when it opens. The entry builder then
// greys a self-only feat (feat.2da TargetSelf, CNWFeat +0x2c) when the menu's
// target (+0x7f8) isn't the player - the game's own Rage did that too. Here,
// for a menu that isn't the player's own, the player's self-only class feats
// are off the menu (OnMenu 0, put back after) while the collector and the
// section page run: no greyed entries, and a class left with nothing gets no
// slot. `radial_hide_self_feats`.

#include <windows.h>

#include <stdint.h>

#include <utility>
#include <vector>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"

namespace {

using CollectClassMenusFn = void* (*)(void* menu, void* records);
using BuildClassMenuFn = void* (*)(void* menu, uint64_t index, uint64_t a3, uint64_t a4);
using PlayerCreatureFn = void* (*)(void* client_app);
using ClassOfSlotFn = uint8_t (*)(void* stats, uint8_t slot);
using GetFeatFn = void* (*)(void* rules, uint16_t feat);

CollectClassMenusFn g_collect = nullptr;
BuildClassMenuFn g_build = nullptr;
PlayerCreatureFn g_player_creature = nullptr;
ClassOfSlotFn g_class_of_slot = nullptr;
GetFeatFn g_get_feat = nullptr;
void** g_app_manager = nullptr;  // &g_pAppManager
void** g_rules = nullptr;        // &g_pRules

constexpr const char* kGetFeat = "?GetFeat@CNWRules@@QEAAPEAVCNWFeat@@G@Z";
constexpr const char* kAppManager = "?g_pAppManager@@3PEAVCAppManager@@EA";
constexpr const char* kRules = "?g_pRules@@3PEAVCNWRules@@EA";
// CreateClassAbilityMenu: mov [rsp+68h],rcx; mov rax,[g_pAppManager]; mov rcx,[rax]; call PlayerCreature;
// mov rbx,rax; mov [rbp-30h],rax; test rax,rax; jnz; xor eax,eax; jmp; mov r12,[rax+3B0h]; test r12,r12; jz;
// mov ecx,7F8h
constexpr const char* kBuilderPattern =
    "48 89 4C 24 68 48 8B 05 ?? ?? ?? ?? 48 8B 08 E8 ?? ?? ?? ?? 48 8B D8 48 89 45 D0 48 85 C0 75 07 33 C0 E9 ?? ?? "
    "?? ?? 4C 8B A0 B0 03 00 00 4D 85 E4 74 ED B9 F8 07 00 00";
constexpr size_t kPlayerCallOffset = 15;  // the E8 in the pattern
constexpr const char* kClassOfSlotPattern = "48 8B 8B B0 03 00 00 E8";  // mov rcx,[rbx+3B0h]; call ClassOfSlot
// The "more abilities" page: mov rbp,rcx; lea rdx,[rsp+28h]; call Collect; nop; mov rax,[g_pAppManager];
// mov rcx,[rax]; call PlayerCreature; mov ecx,7F8h
constexpr const char* kMorePagePattern =
    "48 8B E9 48 8D 54 24 28 E8 ?? ?? ?? ?? 90 48 8B 05 ?? ?? ?? ?? 48 8B 08 E8 ?? ?? ?? ?? B9 F8 07 00 00";
constexpr size_t kCollectCallOffset = 8;
constexpr size_t kClientObjectId = 0x8;
constexpr size_t kClientCreatureStats = 0x3b0;
constexpr size_t kClientStatsClassCount = 0x69;
constexpr size_t kMenuTarget = 0x7f8;
constexpr size_t kRulesClasses = 0x110;
constexpr size_t kClassStride = 0x530;
constexpr size_t kClassFeats = 0x2b0;
constexpr size_t kClassFeatCount = 0x2b8;
constexpr size_t kClassFeatStride = 0x10;
constexpr size_t kClassFeatOnMenu = 0xc;
constexpr size_t kFeatTargetSelf = 0x2c;
int g_logged = 0;

// The player's self-only class feats off the menu while one build runs, when
// the menu is another creature's.
class SelfFeatsOff {
 public:
  explicit SelfFeatsOff(void* menu) {
    void* app = g_app_manager && *g_app_manager ? *static_cast<void**>(*g_app_manager) : nullptr;
    void* player = app && menu ? g_player_creature(app) : nullptr;
    void* rules = g_rules ? *g_rules : nullptr;
    if (!player || !rules || Field<uint32_t>(menu, kMenuTarget) == Field<uint32_t>(player, kClientObjectId)) return;
    void* stats = Field<void*>(player, kClientCreatureStats);
    uint8_t* classes = Field<uint8_t*>(rules, kRulesClasses);
    if (!stats || !classes) return;
    const uint8_t count = Field<uint8_t>(stats, kClientStatsClassCount);
    for (uint8_t slot = 0; slot < count && slot < 8; ++slot) {
      uint8_t* cls = classes + static_cast<size_t>(g_class_of_slot(stats, slot)) * kClassStride;
      uint8_t* feats = Field<uint8_t*>(cls, kClassFeats);
      const uint16_t feat_count = Field<uint16_t>(cls, kClassFeatCount);
      for (uint16_t i = 0; feats && i < feat_count; ++i) {
        uint8_t* entry = feats + static_cast<size_t>(i) * kClassFeatStride;
        auto* on_menu = reinterpret_cast<int32_t*>(entry + kClassFeatOnMenu);
        if (*on_menu != 1) continue;
        const void* feat = g_get_feat(rules, Field<uint16_t>(entry, 0));
        if (!feat || Field<int32_t>(feat, kFeatTargetSelf) == 0) continue;
        hidden_.push_back({on_menu, *on_menu});
        *on_menu = 0;
      }
    }
  }
  ~SelfFeatsOff() {
    for (auto it = hidden_.rbegin(); it != hidden_.rend(); ++it) *it->first = it->second;
  }
  size_t Count() const { return hidden_.size(); }

 private:
  std::vector<std::pair<int32_t*, int32_t>> hidden_;
};

// rcx is the creature menu at both call sites (the main menu and the "more
// abilities" page), though the collector itself doesn't read it.
void* CollectClassMenus(void* menu, void* records) {
  SelfFeatsOff off(menu);
  if (off.Count() && g_logged < 5) {
    ++g_logged;
    Log("radial: another creature's menu - %zu self-only class feats left off it", off.Count());
  }
  return g_collect(menu, records);
}

void* BuildClassMenu(void* menu, uint64_t index, uint64_t a3, uint64_t a4) {
  SelfFeatsOff off(menu);
  return g_build(menu, index, a3, a4);
}

}  // namespace

void InstallRadialClassFeats() {
  if (!g_config.radial_hide_self_feats) return;
  const uintptr_t match = FindPattern("the radial's class section builder (self-only feats)", kBuilderPattern);
  const uintptr_t builder = match ? FunctionEntry(match) : 0;
  g_player_creature = reinterpret_cast<PlayerCreatureFn>(match ? CallTarget(match + kPlayerCallOffset) : 0);
  const uintptr_t class_call =
      builder ? FindPatternIn("the class section's class lookup", FunctionAt(builder), kClassOfSlotPattern) : 0;
  g_class_of_slot = reinterpret_cast<ClassOfSlotFn>(class_call ? CallTarget(class_call + 7) : 0);
  const uintptr_t more = FindPattern("the radial's more-abilities page (self-only feats)", kMorePagePattern);
  const uintptr_t collect = more ? CallTarget(more + kCollectCallOffset) : 0;
  g_get_feat = reinterpret_cast<GetFeatFn>(EngineExport(kGetFeat));
  g_app_manager = reinterpret_cast<void**>(EngineExport(kAppManager));
  g_rules = reinterpret_cast<void**>(EngineExport(kRules));
  if (!builder || !collect || !g_player_creature || !g_class_of_slot || !g_get_feat || !g_app_manager || !g_rules) {
    Log("SKIP radial: self-only class feats stay (greyed) on other creatures' menus - a piece wasn't found");
    return;
  }
  if (!HookAddress("radial class slots (self-only feats off other creatures' menus)", collect,
                   reinterpret_cast<void*>(&CollectClassMenus), reinterpret_cast<void**>(&g_collect))) {
    return;
  }
  if (HookAddress("radial class section page (self-only feats off other creatures' menus)", builder,
                  reinterpret_cast<void*>(&BuildClassMenu), reinterpret_cast<void**>(&g_build))) {
    Log("radial: self-only class feats (Rage, Reckless Attack ...) left off other creatures' menus "
        "(radial_hide_self_feats)");
  }
}
