#pragma once

// The client's area-effect code as core/anim_area_scatter reads and calls it (EE 89.8193.37-17, the 64-bit nwmain.exe):
// CNWCAreaOfEffectObject (an area on the client) and its copies, each a CNWCAnimBaseVisualEffect. Nothing here is
// exported: the hook finds each function and checks each offset by these byte patterns where the game uses them, and
// skips (logged) when any is missing. tests/data/anim_area_copies_test.cpp checks every one against the game's own
// nwmain.exe (skipped without the game). Read from the win64 bytes; the Linux build with symbols (its offsets 0x20
// higher from +0x170 on, its vtables one slot later) names them. Constants only.

#include <stddef.h>
#include <stdint.h>

namespace data {
namespace area_engine {

// CNWCAreaOfEffectObject::LoadEffect(row): the one function using this warning text.
constexpr const char* kLoadEffectText = "CNWCAreaOfEffectObject::LoadEffect(): Failed to load area effect model 1.";

// --- The area object (CNWCAreaOfEffectObject) ---
constexpr size_t kCopies = 0x78;       // CNWCAnimBaseVisualEffect*: the copies, kCopyStride bytes each (copies[0] is also
                                       // the object's own animation base, CNWCObject::GetAnimationBase)
constexpr size_t kCopyStride = 0x118;  // sizeof(CNWCAnimBaseVisualEffect)
constexpr size_t kGroup1Count = 0x194; // int: NUMACT01, group 1's copies (group 2's at +0x198, group 3's at +0x19c)
constexpr size_t kTotalCount = 0x1a0;  // int: the three groups' counts summed - copies[0..total)
constexpr size_t kCopyAlive = 0x170;   // int*: each copy's time left alive, ms (0: not placed, between lives, or for good)
constexpr size_t kCopyStart = 0x178;   // int*: each copy's start time, ms (0: the update skips it - nothing pending)
constexpr size_t kCopyTimer = 0x180;   // int*: each copy's animation timer, ms (an impact's length; at 0 the duration)
constexpr size_t kCopyState = 0x1d0;   // uint16_t*: each copy's animation - 0 none, 1 impact, 2 duration, 3 cessation

// --- A copy (CNWCAnimBaseVisualEffect, a CNWCAnimBase) ---
constexpr size_t kCopyModelName = 0x30;    // CResRef: the model it loaded, 16 chars (not always zero-terminated)
constexpr size_t kCopyModelNameSize = 16;
constexpr size_t kCopyModel = 0xc8;        // the model's Gob (null: none loaded)
constexpr size_t kCopyHiliteOn = 0x9c;     // int[5]: each hilite slot on (Hilite stores graphics.hilite.enabled there)
constexpr const char* kCopyRtti = ".?AVCNWCAnimBaseVisualEffect@@";
constexpr int kCopyLoadModelSlot = 3;      // vtable slot: LoadModel(CResRef, byte)

constexpr uint8_t kAreaHiliteSlot = 3;     // LoadEffect: Hilite(Vector(1, 1, 1), slot 3, glow 0) on every copy
constexpr uint16_t kAreaImpact = 1;        // SetAnimation's animations: 1 impact (once), 2 duration (looped), 3 cessation

// --- #161 VA-5: the loader's group 1 and the update (CNWCAreaOfEffectObject::UpdateInstancePosition) ---
// The loader's group 1 loop: its count test and its step (mov r12d, 300: each copy's start k x 300 ms, copy 0's 0).
constexpr const char* kGroup1CountPattern = "83 BE 94 01 00 00 00";
constexpr const char* kGroup1StartPattern = "41 BC 2C 01 00 00";
// The update: a copy not alive, its start 0 skipped (je); a copy alive (its kCopyAlive entry) handled first.
constexpr const char* kStartSkippedPattern = "48 8B 93 78 01 00 00 8B 04 BA 85 C0 0F 84";
constexpr const char* kAliveTestPattern = "48 8B 8B 70 01 00 00 48 63 FE 8B 04 B9 85 C0 0F 85";

// --- AH (#39): the copies and their white highlight ---
// LoadEffect builds the copies kCopyStride bytes each (the array constructor's element size) and keeps them at kCopies.
constexpr const char* kCopiesPattern =
    "BA 18 01 00 00 48 8B CB E8 ?? ?? ?? ?? 45 33 F6 EB 06 45 33 F6 41 8B DE 48 89 5E 78";
// ... and their total, the groups' counts summed (+0x19c + +0x198 + kGroup1Count) at kTotalCount, at least 1.
constexpr const char* kTotalCountPattern = "03 86 94 01 00 00 89 86 A0 01 00 00 83 F8 01";
// LoadEffect's Hilite call for each group: glow 0 (r9d), slot 3 (r8b), the colour (1, 1, 1) on the stack - three of them.
constexpr uint8_t kHiliteSlot3Call[] = {0x45, 0x33, 0xC9, 0x41, 0xB0, 0x03, 0x48, 0x8D, 0x54, 0x24, 0x50, 0xE8};
constexpr int kHiliteSlot3Calls = 3;
// CNWCAnimBase::Hilite(const Vector&, byte slot, int glow): a model (kCopyModel) and slot < 5, the colour stored, the
// slot's on flag (kCopyHiliteOn) = the global graphics.hilite.enabled, its glow, then RefreshHilite (the E8).
constexpr const char* kHilitePattern =
    "48 83 EC 28 48 83 B9 C8 00 00 00 00 74 ?? 41 80 F8 05 73 ?? F2 0F 10 02 45 0F B6 D0 49 8D 42 08 48 8D 04 40 "
    "4C 8D 04 81 8B 42 08 F2 41 0F 11 00 41 89 40 08 8B 05 ?? ?? ?? ?? 42 89 84 91 9C 00 00 00 33 C0 39 05 ?? ?? ?? ?? "
    "41 0F 45 C1 42 89 84 91 B0 00 00 00 E8";
constexpr size_t kHiliteRefreshCallAt = 86;
// CNWCAnimBase::UnHilite(byte slot) -> int: a model and slot < 5, the slot's on flag cleared, then RefreshHilite (the
// E8) - which, with no slot left on, sets the model's illumination to (0, 0, 0): its material drawn as on a creature.
constexpr const char* kUnHilitePattern =
    "48 83 EC 28 48 83 B9 C8 00 00 00 00 74 ?? 80 FA 05 73 ?? 0F B6 C2 C7 84 81 9C 00 00 00 00 00 00 00 E8";
constexpr size_t kUnHiliteRefreshCallAt = 33;
// The copy's LoadModel (kCopyRtti's vtable, kCopyLoadModelSlot): the model's Gob at kCopyModel, its CResRef copied to
// kCopyModelName.
constexpr const char* kModelNamePattern = "49 89 87 C8 00 00 00 48 85 C0 74 ?? 49 8D 4F 30 48 8B D7 E8";

// --- AH (#39): a lone copy's own sequence ---
// CNWCAreaOfEffectObject::SetAnimation(uint16 anim, byte copy) -> int: copies (kCopies) or 0; the copy's state
// (kCopyState) = anim; impact (1): its timer (kCopyTimer) = the copy's GetAnimationLength (vtable +0x48), at least 1,
// and the impact played once (flags 1; AIUpdate starts the duration, looped, when the timer runs out).
constexpr const char* kSetAnimationPattern =
    "40 57 48 83 EC 30 48 83 79 78 00 48 8B F9 75 08 33 C0 48 83 C4 30 5F C3 48 8B 81 D0 01 00 00 48 89 5C 24 40 "
    "41 0F B6 D8 48 89 74 24 48 48 69 F3 18 01 00 00 66 89 14 58 66 83 FA 01 75 ?? 48 8B 49 78 BA 01 00 00 00 48 03 CE "
    "48 8B 01 FF 50 48 48 8B 87 80 01 00 00 F3 48 0F 2C C8 89 0C 98";
// The update placing a copy (SetInstanceRandomPosition, AttachToScene the first time), then SetAnimation(1, copy) (the
// E8) - in the function with kStartSkippedPattern. What a placed copy gets, and what the hook gives a lone copy.
constexpr const char* kImpactCallPattern = "BA 01 00 00 00 44 0F B6 C6 48 8B CB E8";
constexpr size_t kImpactCallAt = 12;

}  // namespace area_engine
}  // namespace data
