#pragma once

// NWN: EE 89.8193.37-17 (64-bit Windows): the engine's exported names nwn5e
// uses and the few raw offsets it needs (docs/EE-PORT.md principle 4: prefer
// exported functions; every offset says where it was seen in EE's code).
// Re-check the offsets on every EE update.

#include <stddef.h>
#include <stdint.h>

namespace ee {

// --- Exported functions (decorated names; x64: `this` is the first argument).

// unsigned short CNWRules::RollDice(unsigned char count, unsigned char sides)
constexpr const char* kRollDice = "?RollDice@CNWRules@@QEAAGEE@Z";
// void CNWSCreature::ResolveAttackRoll(CNWSObject* target)
constexpr const char* kResolveAttackRoll = "?ResolveAttackRoll@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z";
// CNWSCombatAttackData* CNWSCombatRound::GetAttack(int index)
constexpr const char* kGetAttack = "?GetAttack@CNWSCombatRound@@QEAAPEAVCNWSCombatAttackData@@H@Z";
// int CNWSCreatureStats::GetCriticalHitRoll(int bOffhand) — lowest d20 that threatens
constexpr const char* kGetCriticalHitRoll = "?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z";
// int CNWSCreatureStats::GetCriticalHitMultiplier(int bOffhand)
constexpr const char* kGetCriticalHitMultiplier = "?GetCriticalHitMultiplier@CNWSCreatureStats@@QEAAHH@Z";
// int CNWSCreatureStats::GetDamageRoll(CNWSObject*, int, int, int, int, int)
constexpr const char* kGetDamageRoll = "?GetDamageRoll@CNWSCreatureStats@@QEAAHPEAVCNWSObject@@HHHHH@Z";
// int CNWSCreatureStats::GetDamageBonus(CNWSCreature*, int)
constexpr const char* kGetDamageBonus = "?GetDamageBonus@CNWSCreatureStats@@QEAAHPEAVCNWSCreature@@H@Z";
// int CNWSCreature::GetTotalEffectBonus(unsigned char, CNWSObject*, int, int, unsigned char x4, int)
constexpr const char* kGetTotalEffectBonus = "?GetTotalEffectBonus@CNWSCreature@@QEAAHEPEAVCNWSObject@@HHEEEEH@Z";
// void CNWSCreatureStats::UpdateCombatInformation() — the character sheet's combat block
constexpr const char* kUpdateCombatInformation = "?UpdateCombatInformation@CNWSCreatureStats@@QEAAXXZ";
// int CServerExoApp::GetDifficultyOption(int option) — diffsettings.2da row
constexpr const char* kGetDifficultyOption = "?GetDifficultyOption@CServerExoApp@@QEAAHH@Z";
// int g_bEnableCombatDebugging — the engine's per-term combat breakdown
constexpr const char* kCombatDebugging = "?g_bEnableCombatDebugging@@3HA";

// --- diffsettings.2da rows (EE data, D).
constexpr int kDifficultyNoCriticalOnPc = 0;  // 1 at DM-Easy, Easy and Normal

// --- Raw offsets (D = seen in EE code; see each note).

// CNWSCreature -> its CNWSCombatRound* (ResolveAttackRoll +0x31).
constexpr size_t kCreatureCombatRound = 0x880;
// CNWSCombatRound: index of the attack being resolved, uint8 (ResolveAttackRoll +0x38).
constexpr size_t kRoundCurrentAttack = 0x2f2c;
// CNWSCombatAttackData (ResolveAttackRoll):
constexpr size_t kAttackThreatRoll = 0x0f;   // uint8, the confirmation d20 (+0x3ba)
constexpr size_t kAttackThreatFlag = 0x70;   // int32, 1 after a threat (+0x3b3); the combat
                                             // log prints "Threat Roll" when it is set
constexpr size_t kAttackResult = 0x78;       // uint8: 1 hit, 2 parried, 3 critical, 4 miss, 7 auto hit
constexpr uint8_t kAttackResultCritical = 3;

// CNWSCreatureStats -> its CCombatInformation*, the sheet's combat block
// (UpdateCombatInformation reads [this + 0xd70] throughout, e.g. +0x114a).
constexpr size_t kStatsCombatInformation = 0xd70;
// CCombatInformation (UpdateCombatInformation's tail, +0x12d3..+0x1324):
// main-hand threat range +0x13 (21 - GetCriticalHitRoll(0)) and multiplier
// +0x14 — the multiplier computed inline from the base item (+1 with the
// increased-multiplier feat), not by GetCriticalHitMultiplier; off-hand
// threat range +0x1c and multiplier +0x1d (GetCriticalHitMultiplier(1)).
constexpr size_t kCombatInfoMainHandMultiplier = 0x14;

// GetTotalEffectBonus keeps the current damage-bonus entry's dice count (0 =
// a flat constant) in its frame at [rsp + 0x44] around each of its 12 calls
// to GetCriticalHitMultiplier (all four blocks, D). Seen from the hooked
// multiplier, that is the return-address slot + 8 + 0x44.
constexpr size_t kEffectBonusDiceCountFromReturnSlot = 0x4c;
constexpr uint8_t kEffectBonusDiceCountRead[] = {0x8b, 0x44, 0x24, 0x44};  // mov eax, [rsp+44h]

}  // namespace ee
