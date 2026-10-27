// Shove: push on the radial's Special Attacks page, next to the Shove (0.45.1;
// owner's test of 0.45.0: "push and shove should be in the same common action
// section of the radial menu, right? right now push is in the class actions").
//
// The client's Special Attacks page (IR_XATTACK) is a fixed list, not data: its
// builder (BuildSpecialAttacks, 995090 in 89.8193.37; rcx the creature menu,
// rdx the page or null for the menu itself) adds Parry, Called Shot, Power
// Attack, Knockdown (slot 4), Improved Power Attack, Disarm (slot 6) and "More
// Special Attacks" through one entry helper (9957a0: owner, feat | extra << 16,
// page, slot, {RadialMenuUseFeat 998520, 0}, class 0xff) that skips a feat the
// player lacks. Only a cls_feat OnMenu entry puts a feat anywhere else - the
// class section, where 0.45.0 had it. Here, after the builder, nwn5e's feat
// 1313 takes slot 6: Disarm does nothing in faithful (SK6), so its entry goes.
// Over an entry the setter reuses the label and deletes the old icon but keeps
// the slot's disabled bit (+0x1f8 + slot * 4) and counts the label again
// (+0x7c4, read only by a log check): the bit is cleared first (the helper
// sets it again if the push should be greyed) and the count put back.
// RadialMenuUseFeat sends 1313 like Knockdown (target select, then the use).
// Faithful only (the feat is granted then); the quickbar's assign menu uses the
// same builder, so the push can go on the quickbar too.
// G2 (0.49.0; owner's D2): the page's eight slots are all taken - 0 Parry, 1
// Called Shot: Arm, 2 Improved Power Attack, 3 Called Shot: Leg, 4 Knockdown,
// 5 Power Attack, 6 Disarm, 7 More Special Attacks (the builder's SetEntry and
// AddFeatEntry calls) - so nwn5e's Grapple (1312) takes Called Shot: Leg's slot
// 3, next to the Shove (Called Shot has no 2024 counterpart); the arm stays.

#include <stdint.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "rules/class_features.h"
#include "rules/grapple.h"

namespace {

using BuildFn = void (*)(void* creature_menu, void* page);
using AddFeatEntryFn = void (*)(void* owner, uint32_t feat, void* page, int slot, const void* handler, uint8_t class_index);
using PlayerCreatureFn = void* (*)(void* client_app);

BuildFn g_build = nullptr;
AddFeatEntryFn g_add_entry = nullptr;
PlayerCreatureFn g_player_creature = nullptr;
uintptr_t g_use_feat_handler = 0;  // RadialMenuUseFeat
void** g_app_manager = nullptr;    // &g_pAppManager
int g_logged = 0;

constexpr const char* kAppManager = "?g_pAppManager@@3PEAVCAppManager@@EA";
// The Disarm entry in the builder: lea rax,[RadialMenuUseFeat]; mov [rbp-40h],rax; mov [rbp-38h],esi;
// mov byte [rsp+28h],0FFh; lea rax,[rbp-40h]; movaps xmm0,[rbp-40h]; mov [rsp+20h],rax;
// movdqa [rbp-40h],xmm0; mov r9d,6; mov r8,r12; mov rcx,r15; call AddFeatEntry
constexpr const char* kDisarmEntryPattern =
    "48 8D 05 ?? ?? ?? ?? 48 89 45 C0 89 75 C8 C6 44 24 28 FF 48 8D 45 C0 0F 28 45 C0 48 89 44 24 20 66 0F 7F 45 C0 "
    "41 B9 06 00 00 00 4D 8B C4 49 8B CF E8";
constexpr size_t kHandlerLeaRel = 3;       // its rel32 (the instruction ends at +7)
constexpr size_t kAddEntryCall = 0x31;     // the E8
// The builder's start: mov r12,rdx; mov r15,rcx; test rdx,rdx; cmove r12,rcx; mov rax,[g_pAppManager];
// mov rcx,[rax]; call PlayerCreature; mov rbx,rax; test rax,rax; jz ...; test byte [rax+37Ch],80h
constexpr const char* kBuilderStartPattern =
    "4C 8B E2 4C 8B F9 48 85 D2 4C 0F 44 E1 48 8B 05 ?? ?? ?? ?? 48 8B 08 E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 0F 84 ?? "
    "?? ?? ?? F6 80 7C 03 00 00 80";
constexpr size_t kPlayerCall = 23;         // the E8

// nwn5e's entries: the feat, its slot, what it goes over.
struct Entry {
  int feat;
  int slot;
  const char* name;
  const char* over;
};
constexpr Entry kEntries[] = {
    {rules::kFeatShovePush, 6, "Shove: push", "Disarm"},
    {rules::kFeatGrapple, 3, "Grapple", "Called Shot: Leg"},
};
constexpr size_t kPlayerMayAttack = 0x37c;  // bit 0x80: the page is built at all
constexpr size_t kClientCreatureStats = 0x3b0;
constexpr size_t kStatsFeatLists[] = {0x118, 0x128};  // CExoArrayList<uint16>: data, count at +8
constexpr size_t kPageLabels = 0xf8;        // per slot, 8 bytes
constexpr size_t kPageDisabled = 0x1f8;     // per slot, 4 bytes, bit 0
constexpr size_t kPageLabelCount = 0x7c4;

struct Handler {  // as the builder makes it: {RadialMenuUseFeat, 0}
  uintptr_t function;
  uint32_t param;
  uint32_t pad;
};

bool PlayerHasFeat(void* player, uint16_t feat) {
  void* stats = Field<void*>(player, kClientCreatureStats);
  if (!stats) return false;
  for (size_t list : kStatsFeatLists) {
    const uint16_t* data = Field<uint16_t*>(stats, list);
    const int32_t count = Field<int32_t>(stats, list + 8);
    for (int32_t i = 0; data && i < count && i < 4096; ++i) {
      if (data[i] == feat) return true;
    }
  }
  return false;
}

void BuildSpecialAttacks(void* creature_menu, void* page) {
  g_build(creature_menu, page);
  void* menu = page ? page : creature_menu;
  void* app = g_app_manager && *g_app_manager ? *static_cast<void**>(*g_app_manager) : nullptr;
  void* player = app && menu ? g_player_creature(app) : nullptr;
  if (!player || !(Field<uint8_t>(player, kPlayerMayAttack) & 0x80)) return;
  for (const Entry& entry : kEntries) {
    const uint16_t feat = static_cast<uint16_t>(entry.feat);
    if (!PlayerHasFeat(player, feat)) continue;
    const bool had_label = Field<void*>(menu, kPageLabels + entry.slot * 8) != nullptr;
    auto* disabled = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(menu) + kPageDisabled + entry.slot * 4);
    *disabled &= ~1u;
    const Handler handler{g_use_feat_handler, 0, 0};
    g_add_entry(creature_menu, feat, menu, entry.slot, &handler, 0xff);
    if (had_label) {
      auto* labels = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(menu) + kPageLabelCount);
      if (*labels > 0) --*labels;
    }
    if (g_logged < 6) {
      ++g_logged;
      Log("radial: %s in the Special Attacks page's slot %d%s%s", entry.name, entry.slot, had_label ? " (over " : "",
          had_label ? (std::string(entry.over) + ")").c_str() : "");
    }
  }
}

}  // namespace

void InstallRadialSpecialAttacks() {
  if (g_config.knockdown != rules::RuleVariant::kFaithful) return;
  const uintptr_t disarm = FindPattern("the radial's Special Attacks page (Disarm's entry)", kDisarmEntryPattern);
  const uintptr_t builder = disarm ? FunctionEntry(disarm) : 0;
  const uintptr_t start =
      builder ? FindPatternIn("the Special Attacks page's start", FunctionAt(builder), kBuilderStartPattern) : 0;
  g_add_entry = reinterpret_cast<AddFeatEntryFn>(disarm ? CallTarget(disarm + kAddEntryCall) : 0);
  g_use_feat_handler = disarm ? disarm + 7 + *reinterpret_cast<const int32_t*>(disarm + kHandlerLeaRel) : 0;
  g_player_creature = reinterpret_cast<PlayerCreatureFn>(start ? CallTarget(start + kPlayerCall) : 0);
  g_app_manager = reinterpret_cast<void**>(EngineExport(kAppManager));
  if (!builder || !start || !g_add_entry || !g_use_feat_handler || !g_player_creature || !g_app_manager) {
    Log("SKIP radial: Shove: push not on the Special Attacks page - a piece wasn't found (!shove push still works)");
    return;
  }
  if (HookAddress("radial Special Attacks page (Shove: push in Disarm's slot)", builder,
                  reinterpret_cast<void*>(&BuildSpecialAttacks), reinterpret_cast<void**>(&g_build))) {
    Log("radial: Shove: push on the Special Attacks page, slot 6 (Disarm's - nothing in faithful); Grapple in slot 3 "
        "(Called Shot: Leg's)");
  }
}
