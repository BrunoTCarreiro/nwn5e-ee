#pragma once

// The client's area-effect code as core/anim_area_scatter reads and calls it (EE 89.8193.37-17, the 64-bit nwmain.exe):
// CNWCAreaOfEffectObject (an area on the client) and its copies, each a CNWCAnimBaseVisualEffect. Nothing here is
// exported: the hook finds each function by these byte patterns, and skips (logged) when any is missing. Every offset
// and value the hook reads memory or calls with is also carried by one of the patterns, at a fixed byte
// (kOffsetAnchors, at the end): found in the game, the pattern proves the constant; a constant edited without its
// pattern (or the other way round) fails the tests and makes the hook skip at install (AH fix round 1: before, the
// constants and the patterns' bytes were only kept equal by hand). tests/data/anim_area_copies_test.cpp checks every
// pattern against the game's own nwmain.exe (skipped without the game). Read from the win64 bytes; the Linux build with
// symbols (its offsets 0x20 higher from +0x170 on, its vtables one slot later) names them. Constants only.

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
// LoadEffect's Hilite call for each group: glow 0 (r9d), slot 3 (r8b), the colour (1, 1, 1) on the stack, the call (the
// E8, kHiliteSlot3CallAt) - every direct call to Hilite in LoadEffect ends this way, three of them
// (data::AreaHiliteCallsAsLoaded).
constexpr const char* kHiliteSlot3CallPattern = "45 33 C9 41 B0 03 48 8D 54 24 50 E8";
constexpr size_t kHiliteSlot3CallAt = 11;
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

// --- Every constant the hook reads memory or calls with, tied to the pattern that carries it ---
// `value`'s `size` little-endian bytes are the pattern's bytes from `at` (none of them ??): the instruction's
// displacement or immediate there is that offset or value (data::OffsetAnchored; the test checks every entry, the hook
// checks them again at install). Not here: kCopyModelNameSize (CResRef's 16 chars, read within the copy) and the int
// stride of the hilite flags (Hilite's and UnHilite's index scale, x4).
struct OffsetAnchor {
  const char* name;
  uint32_t value;
  int size;  // bytes: 1 a disp8 / imm8, 4 a disp32 / imm32
  const char* pattern;
  int at;
};
constexpr OffsetAnchor kOffsetAnchors[] = {
    {"kCopies", static_cast<uint32_t>(kCopies), 1, kCopiesPattern, 27},                // mov [rsi+0x78], rbx
    {"kCopyStride", static_cast<uint32_t>(kCopyStride), 4, kCopiesPattern, 1},         // mov edx, 0x118 (element size)
    {"kGroup1Count", static_cast<uint32_t>(kGroup1Count), 4, kGroup1CountPattern, 2},  // cmp [rsi+0x194], 0
    {"kTotalCount", static_cast<uint32_t>(kTotalCount), 4, kTotalCountPattern, 8},     // mov [rsi+0x1a0], eax
    {"kCopyAlive", static_cast<uint32_t>(kCopyAlive), 4, kAliveTestPattern, 3},        // mov rcx, [rbx+0x170]
    {"kCopyStart", static_cast<uint32_t>(kCopyStart), 4, kStartSkippedPattern, 3},     // mov rdx, [rbx+0x178]
    {"kCopyState", static_cast<uint32_t>(kCopyState), 4, kSetAnimationPattern, 27},    // mov rax, [rcx+0x1d0]
    {"kCopyTimer", static_cast<uint32_t>(kCopyTimer), 4, kSetAnimationPattern, 83},    // mov rax, [rdi+0x180]
    {"kCopyModelName", static_cast<uint32_t>(kCopyModelName), 1, kModelNamePattern, 15},  // lea rcx, [r15+0x30]
    {"kCopyModel", static_cast<uint32_t>(kCopyModel), 4, kModelNamePattern, 3},        // mov [r15+0xc8], rax
    {"kCopyHiliteOn", static_cast<uint32_t>(kCopyHiliteOn), 4, kUnHilitePattern, 25},  // mov [rcx+4*rax+0x9c], 0
    {"kAreaHiliteSlot", kAreaHiliteSlot, 1, kHiliteSlot3CallPattern, 5},              // mov r8b, 3
    {"kAreaImpact", kAreaImpact, 4, kImpactCallPattern, 1},                            // mov edx, 1
};

}  // namespace area_engine
}  // namespace data
