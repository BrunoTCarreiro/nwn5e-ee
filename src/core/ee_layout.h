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

// E1 — attacks, AC, the sheet.
// int CNWSCreatureStats::GetAttackModifierVersus(CNWSCreature* target)
constexpr const char* kGetAttackModifierVersus = "?GetAttackModifierVersus@CNWSCreatureStats@@QEAAHPEAVCNWSCreature@@@Z";
// short CNWSCreatureStats::GetArmorClassVersus(CNWSCreature* attacker, int touch)
constexpr const char* kGetArmorClassVersus = "?GetArmorClassVersus@CNWSCreatureStats@@QEAAFPEAVCNWSCreature@@H@Z";
// int CNWSCreatureStats::GetMeleeAttackBonus(int offhand, int, int) — the sheet (and touch attacks)
constexpr const char* kGetMeleeAttackBonus = "?GetMeleeAttackBonus@CNWSCreatureStats@@QEAAHHHH@Z";
// int CNWSCreatureStats::GetRangedAttackBonus(int, int) — the sheet (and touch attacks)
constexpr const char* kGetRangedAttackBonus = "?GetRangedAttackBonus@CNWSCreatureStats@@QEAAHHH@Z";
// unsigned char CNWSCreatureStats::GetAttacksPerRound() — main-hand attacks
constexpr const char* kGetAttacksPerRound = "?GetAttacksPerRound@CNWSCreatureStats@@QEAAEXZ";
// int CNWSCreatureStats::GetBaseAttackBonus(int bPreEpicOnly)
constexpr const char* kGetBaseAttackBonus = "?GetBaseAttackBonus@CNWSCreatureStats@@QEAAHH@Z";
// unsigned char CNWSCreatureStats::GetLevel(int bSubtractNegativeLevels)
constexpr const char* kGetLevel = "?GetLevel@CNWSCreatureStats@@QEAAEH@Z";
// unsigned char CNWSCreatureStats::GetClass(unsigned char slot) — classes.2da row, 0xFF past the last
constexpr const char* kGetClass = "?GetClass@CNWSCreatureStats@@QEAAEE@Z";
// unsigned char CNWSCreatureStats::GetClassLevel(unsigned char slot, int bSubtractNegative, int bScaled)
constexpr const char* kGetClassLevel = "?GetClassLevel@CNWSCreatureStats@@QEAAEEHH@Z";
// void CNWSCombatRound::InitializeNumberOfAttacks()
constexpr const char* kInitializeNumberOfAttacks = "?InitializeNumberOfAttacks@CNWSCombatRound@@QEAAXXZ";
// int CNWRules::GetRulesetIntEntry(unsigned __int64 key, int default) / float ... (key = FNV-1a-64 of the label)
constexpr const char* kGetRulesetIntEntry = "?GetRulesetIntEntry@CNWRules@@QEAAH_KH@Z";
constexpr const char* kGetRulesetFloatEntry = "?GetRulesetFloatEntry@CNWRules@@QEAAM_KM@Z";
// CNWSCreature* CServerExoApp::GetCreatureByGameObjectID(unsigned int id)
constexpr const char* kGetCreatureByGameObjectId = "?GetCreatureByGameObjectID@CServerExoApp@@QEAAPEAVCNWSCreature@@I@Z";
// CAppManager* g_pAppManager; its CServerExoApp* is at +8 (ResolveAttackRoll +0x3d2)
constexpr const char* kAppManager = "?g_pAppManager@@3PEAVCAppManager@@EA";
constexpr size_t kAppManagerServerApp = 0x8;
// CNWRules* g_pRules; its CNWBaseItemArray* is at +0x20 (UpdateCombatInformation +0x117e)
constexpr const char* kRules = "?g_pRules@@3PEAVCNWRules@@EA";
constexpr size_t kRulesBaseItems = 0x20;

// E1b — engine queries for weapons, races, sight and the area.
constexpr const char* kHasFeat = "?HasFeat@CNWSCreatureStats@@QEAAHG@Z";
constexpr const char* kGetItemInSlot = "?GetItemInSlot@CNWSInventory@@QEAAPEAVCNWSItem@@I@Z";
constexpr const char* kGetBaseItem = "?GetBaseItem@CNWBaseItemArray@@QEBAPEAVCNWBaseItem@@H@Z";
constexpr const char* kGetArea = "?GetArea@CNWSObject@@QEAAPEAVCNWSArea@@XZ";
constexpr const char* kGetAIStateReaction = "?GetAIStateReaction@CNWSObject@@QEAAEI@Z";
constexpr const char* kGetDead = "?GetDead@CNWSObject@@QEAAHXZ";
constexpr const char* kGetIsPCDying = "?GetIsPCDying@CNWSObject@@QEAAHXZ";
constexpr const char* kGetVisibleListElement = "?GetVisibleListElement@CNWSCreature@@QEAAPEAVCNWVisibilityNode@@I@Z";
constexpr int kReactionEnemy = 2;  // GetAIStateReaction (the AoO broadcast's enemy test)

// E1c — the round's target after a kill.
constexpr const char* kResolvePostMeleeDamage = "?ResolvePostMeleeDamage@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z";
constexpr const char* kResolvePostRangedDamage = "?ResolvePostRangedDamage@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z";
// CNWSCreature* CNWSCreature::GetNewCombatTarget(unsigned int exclude) — the engine's cleave search
constexpr const char* kGetNewCombatTarget = "?GetNewCombatTarget@CNWSCreature@@QEAAPEAV1@I@Z";
constexpr const char* kGetTotalAttacks = "?GetTotalAttacks@CNWSCombatRound@@QEAAEXZ";
// float CNWSCreature::MaxAttackRange(unsigned int target, int, int): (invalid, 0, 0) = the weapon's range
constexpr const char* kMaxAttackRange = "?MaxAttackRange@CNWSCreature@@QEAAMIHH@Z";
constexpr const char* kGetFaction = "?GetFaction@CNWSCreature@@QEAAPEAVCNWSFaction@@XZ";
constexpr uint32_t kObjectInvalid = 0x7f000000;

// E2a — opportunity attacks, potions.
// void CNWSCreature::BroadcastAttackOfOpportunity(unsigned int target, int) — 5 callers:
// AIUpdate +0xa8d (moving away), EventHandler +0xe3e (the AoO event that
// SignalMeleeDamage / SignalRangedDamage queue: ranged, unarmed, Disarm),
// AIActionCastSpell twice (casting), AIActionItemCastSpell +0xbec (potions).
constexpr const char* kBroadcastAttackOfOpportunity = "?BroadcastAttackOfOpportunity@CNWSCreature@@QEAAXIH@Z";
constexpr const char* kAIUpdate = "?AIUpdate@CNWSCreature@@UEAAXXZ";  // virtual; the export is the body
constexpr const char* kUseItem = "?UseItem@CNWSCreature@@QEAAHIEEIVVector@@IH@Z";
// int CNWSCreature::AddAttackActions(unsigned int target, int, int bClearAllActions?, int)
constexpr const char* kAddAttackActions = "?AddAttackActions@CNWSCreature@@QEAAHIHHH@Z";
// unsigned int CNWSCreature::GetNearestEnemy(float range, unsigned int exclude, int bMustBeSeen, int bCheckPath)
constexpr const char* kGetNearestEnemy = "?GetNearestEnemy@CNWSCreature@@QEAAIMIHH@Z";
constexpr const char* kGetItemByGameObjectId = "?GetItemByGameObjectID@CServerExoApp@@QEAAPEAVCNWSItem@@I@Z";
constexpr int kFeatSpringAttack = 392;  // 0x188: AIUpdate +0xa6d, the only HasFeat(0x188) in the binary

// E2b — effects and conditions.
// CNWSObject's applied effects: CGameEffect** at +0x150, count int32 at +0x158
// (HasSpellEffectApplied). CGameEffect: id uint64 +0x00, type uint16 +0x08
// (GetFilteredEffectList), duration type / flags uint16 +0x0a, duration float
// +0x0c (1.69's layout, unchanged before +0x18, I), creator uint32 +0x18
// (SetCreator), spell id +0x1c, integer count int32 +0x40 and int32* +0x48
// (SetNumIntegers / GetInteger).
constexpr size_t kObjectEffects = 0x150;
constexpr size_t kObjectEffectCount = 0x158;
constexpr size_t kEffectType = 0x08;
constexpr size_t kEffectFlags = 0x0a;
constexpr size_t kEffectDuration = 0x0c;
constexpr size_t kEffectCreator = 0x18;
constexpr size_t kEffectIntegerCount = 0x40;
constexpr size_t kEffectIntegers = 0x48;
constexpr uint16_t kEffectDurationTemporary = 1;
// Effect true types: EE kept 1.69's numbers (poison 35, concealment 76 seen
// in EE's EffectPoison / EffectConcealment; the rest as 1.69, I).
constexpr uint16_t kEffectTypeSetState = 8;
constexpr uint16_t kEffectTypeEntangle = 18;
constexpr uint16_t kEffectTypeDeaf = 21;
constexpr uint16_t kEffectTypePoison = 35;
constexpr uint16_t kEffectTypeIcon = 67;
constexpr uint16_t kEffectTypeConcealment = 76;
constexpr int kStateFrightened = 3;
constexpr int kStateDazed = 5;
constexpr int kStateStunned = 6;
constexpr int kStateParalyzed = 8;
constexpr int kStateAsleep = 9;
constexpr int kStatePetrified = 15;
constexpr int kIconFrightened = 14;
constexpr int kRacialTypeAll = 28;  // a concealment effect's race filter: everyone
// GetEffectImmunity(type, versus, 1): 1 also checks the immunity feats (its HasFeat calls).
constexpr const char* kGetEffectImmunity = "?GetEffectImmunity@CNWSCreatureStats@@QEAAHEPEAVCNWSCreature@@H@Z";
constexpr int kImmunityMindSpells = 1;
constexpr int kImmunityFear = 4;
constexpr int kImmunityCriticalHit = 31;  // ResolveAttackRoll passes 31 (rdata 0xde7d30)
constexpr const char* kOnApplySetState = "?OnApplySetState@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
constexpr const char* kOnApplyEntangled = "?OnApplyEntangled@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
constexpr const char* kOnApplyAttackDecrease = "?OnApplyAttackDecrease@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
constexpr const char* kOnApplyAbilityDecrease = "?OnApplyAbilityDecrease@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
constexpr const char* kResolveDefensiveEffects = "?ResolveDefensiveEffects@CNWSCreature@@QEAAHPEAVCNWSObject@@H@Z";
constexpr const char* kAIActionCastSpell = "?AIActionCastSpell@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
// The creature's AI-state byte: bit 0x10 = can hear (OnApplyDeaf clears it;
// AIActionCastSpell adds 20% spell failure to verbal spells without it).
constexpr size_t kCreatureAiState = 0x8d4;
constexpr uint8_t kAiStateCanHear = 0x10;

// The script bridge: int CVirtualMachine::RunScriptChunk(const CExoString& code,
// unsigned int self, int bSelfValid, int bWrapIntoMain) — as NWScript's
// ExecuteScriptChunk calls it; CExoString built and freed by the engine.
constexpr const char* kVirtualMachine = "?g_pVirtualMachine@@3PEAVCVirtualMachine@@EA";
constexpr const char* kRunScriptChunk = "?RunScriptChunk@CVirtualMachine@@QEAAHAEBVCExoString@@IHH@Z";
constexpr const char* kExoStringFromChars = "??0CExoString@@QEAA@PEBD@Z";
constexpr const char* kExoStringDestructor = "??1CExoString@@QEAA@XZ";

// E2c — weapon mastery and its plumbing.
constexpr size_t kObjectVarTable = 0x108;  // CNWSScriptVarTable (CopyScriptVars +0x1f)
constexpr const char* kVarTableGetInt = "?GetInt@CNWSScriptVarTable@@QEAAHAEAVCExoString@@@Z";
constexpr const char* kVarTableSetInt = "?SetInt@CNWSScriptVarTable@@QEAAXAEAVCExoString@@HH@Z";
constexpr const char* kGetSTRStat = "?GetSTRStat@CNWSCreatureStats@@QEAAEXZ";
constexpr const char* kCalcStatModifier = "?CalcStatModifier@CNWSCreatureStats@@QEAADE@Z";
constexpr const char* kGetDEXMod = "?GetDEXMod@CNWSCreatureStats@@QEAADH@Z";
// int CNWSMessage::SendServerToPlayerChatMessage(uchar channel, uint sender, CExoString text,
// uint, const CExoString&) — a typed line arrives through it from HandlePlayerToServerChatMessage.
constexpr const char* kSendServerToPlayerChatMessage = "?SendServerToPlayerChatMessage@CNWSMessage@@QEAAHEIVCExoString@@IAEBV2@@Z";
constexpr const char* kHandlePlayerToServerChatMessage = "?HandlePlayerToServerChatMessage@CNWSMessage@@QEAAHPEAVCNWSPlayer@@E@Z";
constexpr const char* kResetFeatRemainingUses = "?ResetFeatRemainingUses@CNWSCreatureStats@@QEAAXXZ";
constexpr const char* kEndCombatRound = "?EndCombatRound@CNWSCombatRound@@QEAAXXZ";
constexpr const char* kOnApplyDamage = "?OnApplyDamage@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
// void CNWSCombatRound::AddCleaveAttack(unsigned int target, int bGreatCleave)
constexpr const char* kAddCleaveAttack = "?AddCleaveAttack@CNWSCombatRound@@QEAAXIH@Z";
// CNWSItem* CNWSCombatRound::GetCurrentAttackWeapon(int attack_type)
constexpr const char* kGetCurrentAttackWeapon = "?GetCurrentAttackWeapon@CNWSCombatRound@@QEAAPEAVCNWSItem@@H@Z";
// unsigned char CNWSCreature::SavingThrowRoll(uchar save, ushort dc, uchar type, uint versus,
// int bFeedback, ushort feat, int bFromCombat): 0 failed, 1 made, 2 immune
constexpr const char* kSavingThrowRoll = "?SavingThrowRoll@CNWSCreature@@QEAAEEGEIHGH@Z";
constexpr size_t kObjectTemporaryHitPoints = 0xe8;  // int16, added by GetCurrentHitPoints(0)
constexpr int kSaveFortitude = 1;
constexpr int kFeatWeaponFinesse = 42;

// E2d — Push: the round pause, moving a creature, the line tests, the fall.
// int CNWSCombatRound::CheckActionLength(unsigned int by, int ms): 0 when the
// round is paused by another creature or `ms` no longer fits in it (+0x9, +0x34).
constexpr const char* kCheckActionLength = "?CheckActionLength@CNWSCombatRound@@QEAAHIH@Z";
// void SetRoundPaused(int paused, unsigned int by) writes +0x2f1c / +0x2f20 (+0x7, +0xd).
constexpr const char* kSetRoundPaused = "?SetRoundPaused@CNWSCombatRound@@QEAAXHI@Z";
// void SetPauseTimer(int ms, int hold): hold 0 adds ms to a running timer +0x2f24 (+0x21).
constexpr const char* kSetPauseTimer = "?SetPauseTimer@CNWSCombatRound@@QEAAXHH@Z";
// void DecrementRoundLength(int ms, int): only while the round runs (+0x2f04, +0xa).
constexpr const char* kDecrementRoundLength = "?DecrementRoundLength@CNWSCombatRound@@QEAAXHH@Z";
constexpr size_t kRoundPaused = 0x2f1c;     // int32
constexpr size_t kRoundPausedBy = 0x2f20;   // uint32 object id
constexpr size_t kRoundPauseTimer = 0x2f24; // int32 ms
// float CNWSArea::ComputeHeight(Vector): -6.0 only off the tile grid (+0x14a);
// inside it, a point on no face gets the tile's height — not a walkability test.
constexpr const char* kComputeHeight = "?ComputeHeight@CNWSArea@@QEAAMVVector@@@Z";
constexpr float kAreaOffGridHeight = -6.0f;
// int CNWSArea::TestLineWalkable(x1, y1, x2, y2): 1 when every tile segment of
// the line is walkable (ComputeSafeLocation +0x1bc, where 1.69 called
// NoNonWalkPolysOnLine).
constexpr const char* kTestLineWalkable = "?TestLineWalkable@CNWSArea@@QEAAHMMMM@Z";
// The area's static objects (placeables' walkmesh boxes; doors are not in it):
// int CNWArea::NoNonWalkPolysInDoors(x1, y1, x2, y2, zLow, zHigh, radius,
// exclude id, unsigned int& blocker, int flag, CExoArrayList<CGameObject*>*
// exclude list, int) -> 1 clear, 0 blocked (blocker written, +0x2e9); as
// ComputeSafeLocation +0x262 calls it, on the CNWSArea pointer itself.
constexpr const char* kStaticObjectsOnLine =
    "?NoNonWalkPolysInDoors@CNWArea@@QEAAHMMMMMMMIAEAIHPEAV?$CExoArrayList@PEAVCGameObject@@@@H@Z";
constexpr size_t kPathInfoRadius = 0x4;     // float (BumpFriends +0x22d's static-object radius)
// As AIActionJumpToPoint moves within an area: SetPosition(v, 0) (+0x931),
// then UpdateSubareasOnJumpPosition(v, area id) (+0x95d) with the area's
// object id at CNWSArea +0x140 (+0x94c).
constexpr const char* kSetPosition = "?SetPosition@CNWSObject@@QEAAXVVector@@H@Z";
constexpr const char* kUpdateSubareasOnJump = "?UpdateSubareasOnJumpPosition@CNWSCreature@@QEAAHVVector@@I@Z";
constexpr size_t kAreaObjectId = 0x140;
constexpr size_t kObjectPositionZ = 0xac;
// void CNWSCreature::SetAnimation(int) (virtual; stores +0xd8 at +0x108).
constexpr const char* kSetAnimation = "?SetAnimation@CNWSCreature@@UEAAXH@Z";
// OnApplyKnockdown: dot(direction to the attacker, facing) >= 0.707 (+0x38d)
// falls with 7, else 5.
constexpr int kAnimationFallFacing = 7;
constexpr int kAnimationFallTurned = 5;
constexpr float kFallFacingDot = 0.707f;
// Creature size, int32: 1 tiny .. 4 large, 5 huge (ExecuteCommandGetCreatureSize +0x4c).
constexpr size_t kCreatureSize = 0x718;
constexpr int32_t kCreatureSizeLarge = 4;
// The object's action queue: CExoLinkedListInternal* at +0x100 (head +0x0);
// node: data +0x10 (GetAtPos +0x4a). CNWSObjectActionNode: action id int32
// +0x0, first parameter in the 8-byte slot at +0x38 (AddActionNodeParameter
// +0x47); attack object = 0xC (ComputeAIState +0x47). While RunActions runs
// an action it takes the node off the queue and holds it as the current
// action: uint16 +0x90 (0xFFFF none), its target uint32 +0x94 (+0x97f).
constexpr size_t kObjectActionQueue = 0x100;
constexpr size_t kListHead = 0x0;
constexpr size_t kListNodeData = 0x10;
constexpr size_t kActionId = 0x0;
constexpr size_t kActionFirstParam = 0x38;
constexpr int kActionAttackObject = 0xC;
constexpr size_t kObjectCurrentAction = 0x90;
constexpr size_t kObjectCurrentActionTarget = 0x94;
constexpr uint16_t kActionNone = 0xFFFF;
// The creature's attempted attack target, uint32 (AddAttackActions +0x30d).
constexpr size_t kCreatureAttemptedAttackTarget = 0x728;

// E3a — saving throws and spell DCs.
// char CNWSCreatureStats::Get{Fort,Reflex,Will}SavingThrow(int bBaseOnly): the
// answer is in al only. SavingThrowRoll asks with bBaseOnly = 1 (+0x4a) and
// adds the effects itself; the sheet and scripts ask with 0.
constexpr const char* kGetFortSavingThrow = "?GetFortSavingThrow@CNWSCreatureStats@@QEAADH@Z";
constexpr const char* kGetReflexSavingThrow = "?GetReflexSavingThrow@CNWSCreatureStats@@QEAADH@Z";
constexpr const char* kGetWillSavingThrow = "?GetWillSavingThrow@CNWSCreatureStats@@QEAADH@Z";
// uchar CNWClass::Get{Fort,Ref,Will}SaveBonus(uchar level); classes at
// [g_pRules + 0x110], 0x530 each (GetBaseFortSavingThrow +0x56).
constexpr const char* kGetFortSaveBonus = "?GetFortSaveBonus@CNWClass@@QEAAEE@Z";
constexpr const char* kGetRefSaveBonus = "?GetRefSaveBonus@CNWClass@@QEAAEE@Z";
constexpr const char* kGetWillSaveBonus = "?GetWillSaveBonus@CNWClass@@QEAAEE@Z";
constexpr size_t kRulesClasses = 0x110;
constexpr size_t kClassStride = 0x530;
// Blueprint save bonuses, int8 (the getters +0xd1 / +0xdb).
constexpr size_t kStatsFortBlueprintBonus = 0xe2c;
constexpr size_t kStatsWillBlueprintBonus = 0xe2d;
constexpr size_t kStatsReflexBlueprintBonus = 0xe2e;
constexpr int kFeatGreatFortitude = 14;
constexpr int kFeatLightningReflexes = 24;
constexpr int kFeatIronWill = 22;
constexpr int kFeatDivineGrace = 217;
constexpr int kFeatLucky = 248;
constexpr int kFeatLuckOfHeroes = 382;
constexpr int kFeatDarkBlessing = 473;
// int CNWSCreature::CalculateSpellSaveDC(int spell): casting class slot uint8
// +0x22c (0xFE innate: Cha; 0xFF none: the feat id uint16 +0x23c names a
// class feat, 0xFFFF none), cast from an item int32 +0x820, polymorphed +0x97c;
// spell innate level uint8 +0x6f, school uint8 +0x29 (its body, D).
constexpr const char* kCalculateSpellSaveDC = "?CalculateSpellSaveDC@CNWSCreature@@QEAAHH@Z";
constexpr const char* kGetSpellcastingMod = "?GetSpellcastingMod@CNWSCreatureStats@@QEAADE@Z";
constexpr size_t kCreatureCastingSlot = 0x22c;
constexpr size_t kCreatureCastingFeat = 0x23c;
constexpr size_t kCreatureCastingFromItem = 0x820;
constexpr size_t kCreaturePolymorphed = 0x97c;
constexpr size_t kSpellInnateLevel = 0x6f;
constexpr size_t kSpellSchool = 0x29;
// SavingThrowRoll: kinds 1-3 only; its one RollDice is the d20 (+0xc60).
constexpr int kSaveTypeMindSpells = 1;
constexpr int kSaveTypePoison = 2;
constexpr int kSaveTypeFear = 4;
// Natural 20 (EP12): `cmp esi,20; je success` at SavingThrowRoll +0x1168, the
// anchor unique in the whole binary; the je (74 4E) becomes two nops.
constexpr uint8_t kSaveNatural20Anchor[] = {0x83, 0xFE, 0x14, 0x74, 0x4E, 0x42, 0x8D, 0x0C, 0x36, 0x41,
                                            0x03, 0xCD, 0x0F, 0xB7, 0x45, 0x57, 0x3B, 0xC8, 0x7D, 0x3F};
constexpr size_t kSaveNatural20JumpOffset = 3;
// Natural 1: CServerInfo +0x148 (settings.tml ruleset.natural-ones-fail-saving-
// throws), read at SavingThrowRoll +0x1120 and by the message writer.
constexpr const char* kGetServerInfo = "?GetServerInfo@CServerExoApp@@UEAAPEAVCServerInfo@@XZ";
constexpr size_t kServerInfoNaturalOnesFailSaves = 0x148;
// The script save commands (FortitudeSave / ReflexSave / WillSave, one
// handler) and GetReflexAdjustedDamage; OBJECT_SELF's id at commands +0xc.
constexpr const char* kExecuteCommandSavingThrow = "?ExecuteCommandSavingThrow@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr const char* kExecuteCommandGetReflexAdjustedDamage =
    "?ExecuteCommandGetReflexAdjustedDamage@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr size_t kVmCommandsObjectSelf = 0xc;
// CGameObject* CServerExoApp::GetGameObject(uint id); vtable +0x20 = AsNWSObject;
// the object's last spell id int32 +0x218 (GetSpellId; an area of effect keeps its own).
constexpr const char* kGetGameObject = "?GetGameObject@CServerExoApp@@QEAAPEAVCGameObject@@I@Z";
constexpr size_t kVtableAsNwsObject = 0x20;
constexpr size_t kObjectLastSpellId = 0x218;

// Strings (EP4): int CTlkTable::FetchInternal(uint strref, STR_RES& out, int);
// STR_RES = {CExoString text +0x0, CResRef +0x10, uint32 +0x24} (its copy
// operator ??4STR_RES; GetSimpleString +0x71 reads the text at +0).
constexpr const char* kTlkFetchInternal = "?FetchInternal@CTlkTable@@IEAAHIAEAUSTR_RES@@H@Z";
constexpr size_t kStrResText = 0x0;
constexpr const char* kExoStringAssignChars = "??4CExoString@@QEAAAEAV0@PEBD@Z";

// E3c — spell attacks (SK10). int ExecuteCommandDoTouchAttack(int command, int params):
// one handler for TouchAttackMelee (146) / TouchAttackRanged (147); a creature
// caster's bonus from GetMeleeAttackBonus(0, 1, 1) / GetRangedAttackBonus(1, 1)
// (+0x12b / +0x13a), AC from GetArmorClassVersus(target, caster, 1) (+0x184),
// then RollDice: d20 (+0x199), confirmation (+0x23d); a non-creature caster's
// d20 and confirmation follow (+0x327, +0x35f) — 4 RollDice in all.
constexpr const char* kExecuteCommandDoTouchAttack = "?ExecuteCommandDoTouchAttack@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr int kCommandTouchAttackRanged = 0x93;
constexpr int kEffectBonusTypeTouchAttack = 6;
// Shove (SK6). void CNWSCombatRound::AddSpecialAttack(ushort id): its only
// callers are UseFeat's; the id lands at attack data +0x7a (1.69 +0x52).
// void CNWSCreature::ResolveMeleeSpecialAttack(int, int, CNWSObject*, int): one
// ResolveAttackRoll (+0x9a), ResolveDamage (+0xbd), ResolvePostMeleeDamage
// (+0xc8); Knockdown allows one size larger (+0x2fd), Improved two (+0x38e);
// then GetSkillRank(target, Discipline, attacker, 0) (+0x3f2) + CRT rand d20
// against the attack total: >= resists (result 5), else the knockdown effect.
constexpr const char* kAddSpecialAttack = "?AddSpecialAttack@CNWSCombatRound@@QEAAXG@Z";
constexpr const char* kResolveMeleeSpecialAttack = "?ResolveMeleeSpecialAttack@CNWSCreature@@IEAAXHHPEAVCNWSObject@@H@Z";
constexpr const char* kResolveDamage = "?ResolveDamage@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z";
constexpr size_t kAttackSpecial = 0x7a;  // uint16 special attack (feat) id
constexpr uint16_t kFeatDisarm = 9;
constexpr uint16_t kFeatImprovedDisarm = 16;
constexpr uint16_t kFeatImprovedKnockdown = 17;
constexpr uint16_t kFeatKnockdown = 23;
constexpr uint8_t kAttackResultAutomaticHit = 7;
constexpr int kSkillDiscipline = 3;
constexpr int kSaveReflex = 2;

// E3d — skill DCs (SK3), conditions on checks (SK11).
// Object kinds (CGameObject +0xc, byte): 7 trigger, 9 placeable, 10 door;
// vtable casts +0x80 AsNWSTrigger, +0x30 AsNWSDoor, +0x98 AsNWSPlaceable.
constexpr size_t kGameObjectType = 0xc;
constexpr uint8_t kObjectTypeTrigger = 7;
constexpr uint8_t kObjectTypePlaceable = 9;
constexpr uint8_t kObjectTypeDoor = 10;
constexpr size_t kVtableAsTrigger = 0x80;
constexpr size_t kVtableAsDoor = 0x30;
constexpr size_t kVtableAsPlaceable = 0x98;
// Lock DCs (uint8): door unlock +0x41f / lock +0x420; placeable +0x3a8 / +0x3a9.
// Trap DCs: trigger detect / disarm int32 +0x3f8 / +0x3fc; door uint8 +0x471 /
// +0x470; placeable uint8 +0x3b0 / +0x3b8 (ExecuteCommandGetLockInfo / GetTrapInfo).
constexpr size_t kDoorUnlockDc = 0x41f;
constexpr size_t kDoorLockDc = 0x420;
constexpr size_t kPlaceableUnlockDc = 0x3a8;
constexpr size_t kPlaceableLockDc = 0x3a9;
constexpr size_t kTriggerDetectDc = 0x3f8;
constexpr size_t kTriggerDisarmDc = 0x3fc;
constexpr size_t kDoorDetectDc = 0x471;
constexpr size_t kDoorDisarmDc = 0x470;
constexpr size_t kPlaceableDetectDc = 0x3b0;
constexpr size_t kPlaceableDisarmDc = 0x3b8;
// Engine skill actions, uint AIAction*(CNWSObjectActionNode*); the target id
// is the node's first parameter (+0x38). Each makes one 2024-relevant
// GetSkillRank call and logs through BroadcastSkillData.
constexpr const char* kAIActionUnlockObject = "?AIActionUnlockObject@CNWSObject@@IEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionLockObject = "?AIActionLockObject@CNWSObject@@IEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionDisarmTrap = "?AIActionDisarmTrap@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionFlagTrap = "?AIActionFlagTrap@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionRecoverTrap = "?AIActionRecoverTrap@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionExamineTrap = "?AIActionExamineTrap@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionSetTrap = "?AIActionSetTrap@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionAnimalEmpathy = "?AIActionAnimalEmpathy@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kAIActionHeal = "?AIActionHeal@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
// Search: UpdateTrapCheck (one caller, AIUpdate); 6 GetSkillRank calls with
// the trap's detect DC in edi at each (D), 3 RollDice (d10 or d20).
constexpr const char* kUpdateTrapCheck = "?UpdateTrapCheck@CNWSCreature@@IEAAXXZ";
// Lore: UseLoreOnItem compares the total with SkillVsItemCost rows (a skill value, not a DC).
constexpr const char* kUseLoreOnItem = "?UseLoreOnItem@CNWSCreature@@QEAAHI@Z";
// Spellcraft: identify a spell (BroadcastSpellCast(spell id, uchar, ushort)), counterspell
// (BroadcastCounterSpellData(CNWSpell*, msg)); DC = innate level + 15.
constexpr const char* kBroadcastSpellCast = "?BroadcastSpellCast@CNWSCreature@@QEAAXIEG@Z";
constexpr const char* kBroadcastCounterSpellData =
    "?BroadcastCounterSpellData@CNWSObject@@QEAAXPEAVCNWSpell@@PEAVCNWCCMessageData@@@Z";
// The Examine Trap window: (player, trap id, creature, int); Disable Trap + 20 vs DisarmDC.
constexpr const char* kExamineGuiTrapData =
    "?SendServerToPlayerExamineGui_TrapData@CNWSMessage@@QEAAHPEAVCNWSPlayer@@IPEAVCNWSCreature@@H@Z";
// Script checks: GetIsSkillSuccessful (pops object, skill, DC; one GetSkillRank,
// one RollDice), GetIsDoorActionPossible (pops object, action; skill + 20 vs the lock DC).
constexpr const char* kExecuteCommandGetIsSkillSuccessful =
    "?ExecuteCommandGetIsSkillSuccessful@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr const char* kExecuteCommandGetIsDoorActionPossible =
    "?ExecuteCommandGetIsDoorActionPossible@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr const char* kStackPopInteger = "?StackPopInteger@CVirtualMachine@@QEAAHPEAH@Z";
constexpr const char* kStackPopObject = "?StackPopObject@CVirtualMachine@@QEAAHPEAI@Z";
constexpr const char* kStackPushInteger = "?StackPushInteger@CVirtualMachine@@QEAAHH@Z";
constexpr const char* kStackPushObject = "?StackPushObject@CVirtualMachine@@QEAAHI@Z";
// void CNWSCreature::BroadcastSkillData(CNWCCMessageData*): the combat log's
// skill line (frees the message): ints [2] skill total, [3] DC.
constexpr const char* kBroadcastSkillData = "?BroadcastSkillData@CNWSCreature@@QEAAXPEAVCNWCCMessageData@@@Z";
constexpr const char* kMessageGetInteger = "?GetInteger@CNWCCMessageData@@QEAAHH@Z";
constexpr const char* kMessageSetInteger = "?SetInteger@CNWCCMessageData@@QEAAXHH@Z";
constexpr int kSkillMessageTotal = 2;
constexpr int kSkillMessageDc = 3;
// int C2DA::GetINTEntry(int row, const CExoString& column, int* out): Set Trap's
// SetDC / DetectDCMod / DisarmDCMod, the heal kit's poison / disease DCs.
constexpr const char* kTwoDaGetIntEntry = "?GetINTEntry@C2DA@@QEAAHHAEBVCExoString@@PEAH@Z";
constexpr const char* kGetBlind = "?GetBlind@CNWSCreature@@QEAAHXZ";
constexpr int kTumbleDc = 15;  // BroadcastAttackOfOpportunity's Tumble check (SetInteger(3, 15))
// skills.2da rows.
constexpr int kSkillAnimalEmpathy = 0;
constexpr int kSkillDisableTrap = 2;
constexpr int kSkillListen = 6;
constexpr int kSkillLore = 7;
constexpr int kSkillOpenLock = 9;
constexpr int kSkillSearch = 14;
constexpr int kSkillSetTrap = 15;
constexpr int kSkillSpellcraft = 16;
constexpr int kSkillSpot = 17;
constexpr int kSkillTumble = 21;

// E3d2 — Stealth and Perception (SK5). SetStealthMode(uchar) acts on a change
// of the stealth byte +0x6ed and rolls nothing. DoStealthDetection(target,
// bSpot, int*, int*, flag): every 6 s the creature's Hide / Move Silently d20s
// (+0x7c4 / +0x7c5, only while hiding), Spot die (d10 / d20, +0x7c6) and
// Listen die (+0x7c7) — its 5 RollDice in that order; then DoListenDetection /
// DoSpotDetection(target, flag). Each of those: 4 GetSkillRank calls (Listen:
// the hider's Move Silently zero test, Move Silently, Listen halved / full;
// Spot: the hider's Hide, the zero test, Spot halved / full), 2 GetDetectMode
// (which path), GetFavoredEnemyBonus once, the size term (Listen: hider size
// 1/2/4/5 -> -8/-4/+4/+8; Spot: hider - observer size -2..+2 -> -8..+8) added
// to the observer's score, -10 when the observer is in combat (+0x700).
constexpr const char* kSetStealthMode = "?SetStealthMode@CNWSCreature@@QEAAXE@Z";
constexpr const char* kDoStealthDetection = "?DoStealthDetection@CNWSCreature@@IEAAHPEAV1@HPEAH1H@Z";
constexpr const char* kDoListenDetection = "?DoListenDetection@CNWSCreature@@IEAAHPEAV1@H@Z";
constexpr const char* kDoSpotDetection = "?DoSpotDetection@CNWSCreature@@IEAAHPEAV1@H@Z";
constexpr const char* kGetDetectMode = "?GetDetectMode@CNWSCreature@@QEAAEXZ";
constexpr const char* kGetFavoredEnemyBonus = "?GetFavoredEnemyBonus@CNWSCreatureStats@@QEAAHPEAVCNWSCreature@@@Z";
constexpr size_t kCreatureDetectMode = 0x6ec;   // uint8: the Search action
constexpr size_t kCreatureStealthMode = 0x6ed;  // uint8: 1 hiding
constexpr size_t kCreatureInCombat = 0x700;     // int32
constexpr size_t kCreatureHideRoll = 0x7c4;     // uint8 dice the checks use
constexpr size_t kCreatureMoveSilentlyRoll = 0x7c5;
constexpr int kSkillHide = 5;

// E4 — the character model. The rules object's constructor loads every table
// (LoadFeatInfo, LoadClassInfo, LoadRaceInfo, spells, ...); ReloadAll again
// (10 callers, module loads among them).
constexpr const char* kRulesConstructor = "??0CNWRules@@QEAA@XZ";
constexpr const char* kRulesReloadAll = "?ReloadAll@CNWRules@@QEAAXXZ";
// CM0 served data. SetupDefaultSearchPath (client init, after the aliases and
// the resource manager, before any 2da loads) is where nwn5e's folder joins:
// an alias (CExoAliasList::Add(alias, path); the list at *(*g_pExoBase + 0x20))
// and AddResourceDirectory("NWN5E:", priority, 0, nullptr). 15,000,000 is
// free: above override (12M) and patch (13M), below module (23M) and haks
// (30-31M) (EP5). CExoResMan::Get(resref, type) gives a resource's current
// copy (shared_ptr<DataBlock>: data = **(char***)block, size =
// *(size_t*)(*(char**)block + 8)).
constexpr const char* kSetupDefaultSearchPath = "?SetupDefaultSearchPath@CExoResMan@@QEAAXXZ";
constexpr const char* kExoBase = "?g_pExoBase@@3PEAVCExoBase@@EA";
constexpr size_t kExoBaseAliasList = 0x20;  // inside *g_pExoBase (CExoBaseInternal*)
constexpr const char* kAliasListAdd = "?Add@CExoAliasList@@QEAAXVCExoString@@0@Z";
constexpr const char* kExoResMan = "?g_pExoResMan@@3PEAVCExoResMan@@EA";
constexpr const char* kAddResourceDirectory =
    "?AddResourceDirectory@CExoResMan@@QEAAHAEBVCExoString@@IHQ6A_NPEAVCExoKeyTable@@AEBVCResRef@@G@Z@Z";
constexpr const char* kResManGet = "?Get@CExoResMan@@QEAA?AV?$shared_ptr@VDataBlock@@@std@@AEBVCResRef@@G@Z";
constexpr const char* kResRefAssignChars = "??4CResRef@@QEAAAEAV0@PEBD@Z";
constexpr const char* kDataBlockPtrDestructor = "??1?$shared_ptr@VDataBlock@@@std@@QEAA@XZ";
constexpr uint32_t kServedPriority = 15000000;
constexpr uint16_t kResourceType2da = 2017;
// Loaded tables (CNWRules): XP table int32 x 41 +0x28; feats +0xD8 (0x74
// each, count uint16 +0x100; CNWFeat name strref +0x0, MinLevel uint8 +0x59);
// classes +0x110 (0x530 each, count uint8 +0x105).
constexpr size_t kRulesXpTable = 0x28;
constexpr size_t kRulesFeats = 0xd8;
constexpr size_t kRulesFeatCount = 0x100;
constexpr size_t kFeatStride = 0x74;
constexpr size_t kFeatName = 0x0;
constexpr size_t kFeatMinLevel = 0x59;
constexpr size_t kRulesClassCount = 0x105;
// CNWClass: hit die uint8 +0x74, bonus-feat slots uint8 +0x2B9 + level
// (1..60, GetBonusFeats), alignment limits uint8 +0x4A1 / uint8 +0x4A2 /
// int32 +0x4A4 (GetIsAlignmentAllowed), XPPenalty int32 +0x4E0 (AddExperience).
constexpr size_t kClassHitDie = 0x74;
constexpr size_t kClassBonusFeats = 0x2b9;
constexpr size_t kClassAlignRestrict = 0x4a1;
constexpr size_t kClassAlignRestrictType = 0x4a2;
constexpr size_t kClassInvertRestrict = 0x4a4;
constexpr size_t kClassXpPenalty = 0x4e0;
// CNWRace: NormalFeatEveryNthLevel int32 +0x58 (0 = the general feat at level 1 only).
constexpr size_t kRaceFeatEveryNthLevel = 0x58;
// CM1: CanLevelUp (11 callers; PCs stop at 40, everyone at CServerInfo +0xf4);
// SetExperience(xp, int), called once by the script command SetXP; stats XP
// uint32 +0xa8, is a PC int32 +0xb0.
constexpr const char* kCanLevelUp = "?CanLevelUp@CNWSCreatureStats@@QEAAHXZ";
constexpr const char* kSetExperience = "?SetExperience@CNWSCreatureStats@@QEAAXIH@Z";
constexpr const char* kExecuteCommandSetXP = "?ExecuteCommandSetXP@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr size_t kStatsExperience = 0xa8;
constexpr size_t kStatsIsPlayer = 0xb0;
// CM2: GetIsClassAvailable(class) (LevelUpAutomatic, ValidateLevelUp, the
// level-up screen's class list). EP16: MULTICLASS_LIMIT is read only by
// ValidateCharacter (1..8) and the client's level-up panel (to 8).
constexpr const char* kGetIsClassAvailable = "?GetIsClassAvailable@CNWSCreatureStats@@QEAAHE@Z";
// CM3: short GetMaxHitPoints(int) (virtual; the export is the body).
constexpr const char* kGetMaxHitPoints = "?GetMaxHitPoints@CNWSCreature@@UEAAFH@Z";
// CM14: CheckItemAlignmentRestrictions(item) -> 1 usable (CanEquipItem, CanUseItem).

// E4b — ability scores (CM4, CM5). One CNWRules serves the server and the
// client's creation screen (both read g_pRules), so loaded-table changes reach both.
// Races at rules +0x118, 0x68 each, count uint8 +0x106 (LoadRaceInfo):
// adjustments int8 +0x18 Str, +0x19 Dex, +0x1A Int, +0x1B Cha, +0x1C Wis,
// +0x1D Con; point-buy budget int32 +0x54 (AbilitiesPointBuyNumber, 30).
constexpr size_t kRulesRaces = 0x118;
constexpr size_t kRaceStride = 0x68;
constexpr size_t kRulesRaceCount = 0x106;
constexpr size_t kRaceAbilityAdjust = 0x18;
constexpr size_t kRacePointBuyBudget = 0x54;
// The class's Recommended scores, uint8 Str .. Cha at +0x49b (LoadClassInfo;
// the creation screen's Recommended button copies them unchecked).
constexpr size_t kClassRecommendedScores = 0x49b;
// Base scores uint8 at stats +0xd38 + 2 x (Str .. Cha), each followed by its
// cached modifier; Set*Base write both (SetCONBase(value, 1): current HP
// follows the new maximum).
constexpr size_t kStatsAbilityBase = 0xd38;
constexpr const char* kSetAbilityBase[6] = {
    "?SetSTRBase@CNWSCreatureStats@@QEAAXE@Z", "?SetDEXBase@CNWSCreatureStats@@QEAAXE@Z",
    "?SetCONBase@CNWSCreatureStats@@QEAAXEH@Z", "?SetINTBase@CNWSCreatureStats@@QEAAXE@Z",
    "?SetWISBase@CNWSCreatureStats@@QEAAXE@Z", "?SetCHABase@CNWSCreatureStats@@QEAAXE@Z"};
constexpr const char* kGetAbilityStat[6] = {
    "?GetSTRStat@CNWSCreatureStats@@QEAAEXZ", "?GetDEXStat@CNWSCreatureStats@@QEAAEXZ",
    "?GetCONStat@CNWSCreatureStats@@QEAAEXZ", "?GetINTStat@CNWSCreatureStats@@QEAAEXZ",
    "?GetWISStat@CNWSCreatureStats@@QEAAEXZ", "?GetCHAStat@CNWSCreatureStats@@QEAAEXZ"};
// The level history: CNWLevelStats* GetLevelStats(uchar index); count int32
// stats +0x40. CNWLevelStats: the ability raised +0x15a (6 = none), HP +0x15b,
// class +0x15c.
constexpr const char* kGetLevelStats = "?GetLevelStats@CNWSCreatureStats@@QEAAPEAVCNWLevelStats@@E@Z";
constexpr size_t kStatsLevelHistoryCount = 0x40;
constexpr size_t kLevelStatsAbility = 0x15a;
constexpr size_t kLevelStatsClass = 0x15c;
// void LevelUp(CNWLevelStats*, uchar, uchar, uchar, int): adds the level
// (+0x5b) and raises +0x15a's ability (+0x299).
constexpr const char* kLevelUp = "?LevelUp@CNWSCreatureStats@@QEAAXPEAVCNWLevelStats@@EEEH@Z";
// EP15 (b): the ability page every 4 levels, all three or none. The server
// reads the chosen ability (HandlePlayerToServerLevelUpMessage +0x181,
// `test al,3; jne` -> jmp); the client's level-up writer and its class panel
// (not exported) take the same branch on the level.
constexpr const char* kHandleLevelUpMessage = "?HandlePlayerToServerLevelUpMessage@CNWSMessage@@QEAAHPEAVCNWSPlayer@@E@Z";
constexpr const char* kLevelUpServerPattern = "ff c0 a8 03 75 ?? 45 8b c6 41 8d 56 07";
constexpr const char* kLevelUpWriterPattern = "a8 03 75 ?? 41 0f b6 47 76 41 3a 45 76";
constexpr const char* kLevelUpPanelPattern = "e8 ?? ?? ?? ?? a8 03 75 ?? b9 ?? ?? 00 00 e8 ?? ?? ?? ?? 48 89 44 24 ??";

// E3b — skills, concentration, the 3e-only skills, initiative.
// char CNWSCreatureStats::GetSkillRank(uchar skill, CNWSObject* vs, int bBaseRanksOnly):
// every engine and script skill total (83 call sites); DMs get 127 (+0x2f).
constexpr const char* kGetSkillRank = "?GetSkillRank@CNWSCreatureStats@@QEAADEPEAVCNWSObject@@H@Z";
constexpr const char* kGetIsDM = "?GetIsDM@CNWSCreatureStats@@QEAAHXZ";
// char GetAbilityMod(uchar ability 0 Str .. 5 Cha): the cached modifier bytes GetSkillRank reads.
constexpr const char* kGetAbilityMod = "?GetAbilityMod@CNWSCreatureStats@@QEAADE@Z";
constexpr size_t kStatsSkillRanks = 0xdc8;  // int8* ranks per skills.2da row (GetSkillRank, 1.69 +0x478)
constexpr int kEffectBonusTypeSkill = 5;    // GetTotalEffectBonus(5, vs, 0, 0, 0, 0, skill, 0xFF, 0) in GetSkillRank
// The script command GetSkillRank: one GetSkillRank call (found at load).
constexpr const char* kExecuteCommandGetSkillRank = "?ExecuteCommandGetSkillRank@CNWSVirtualMachineCommands@@QEAAHHH@Z";
// void CNWSCreature::SetCombatMode(uchar mode, int force): the only writer of the
// mode byte (+0x6ef); player toggles, SetActionMode, UseSkill and the AI all go through it.
constexpr const char* kSetCombatMode = "?SetCombatMode@CNWSCreature@@QEAAXEH@Z";
constexpr uint8_t kCombatModeParry = 1;
constexpr uint8_t kCombatModeDefensiveCasting = 9;  // AIActionCastSpell +0x1bf0 rolls Concentration in it
// uint AIActionTaunt(CNWSObjectActionNode*): first pass (+0x850 == 0) plays the
// animation and re-queues; the second pass rolls (CRT rand), applies the
// taunt, makes the target hostile, then SetCombatMode(0, 1), cooldown
// [[creature+0x878]+0x48] = 6000, +0x850 = 0, returns 2 (done; 3 = failed).
constexpr const char* kAIActionTaunt = "?AIActionTaunt@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr size_t kCreatureTauntPass = 0x850;       // int32
constexpr size_t kCreatureSkillCooldowns = 0x878;  // pointer; +0x48 = Taunt's cooldown, int32 ms
constexpr size_t kSkillCooldownTaunt = 0x48;
constexpr int kTauntCooldownMs = 6000;  // SKILL_TAUNT_COOLDOWN
constexpr uint32_t kActionDone = 2;
// void CNWSStore::AddCustomer(CNWSPlayer*, char markup, char markdown) (only caller: OpenStore).
constexpr const char* kAddCustomer = "?AddCustomer@CNWSStore@@QEAAXPEAVCNWSPlayer@@DD@Z";
// Use Magic Device: GetSkillRank(19) inside these three (signed compares).
constexpr int kSkillUseMagicDevice = 19;
constexpr const char* kCheckUseMagicDeviceSkill = "?CheckUseMagicDeviceSkill@CNWSCreature@@IEAAHPEAVCNWSItem@@H@Z";
constexpr const char* kCheckItemAlignmentRestrictions =
    "?CheckItemAlignmentRestrictions@CNWSCreature@@IEAAHPEAVCNWSItem@@@Z";
constexpr const char* kApplyHolyAvenger =
    "?ApplyHolyAvenger@CNWSItemPropertyHandler@@QEAAHPEAVCNWSItem@@PEAVCNWItemProperty@@PEAVCNWSCreature@@IH@Z";
// Spells: CNWRules +0xD0 = CNWSpellArray* {int32 count +0x0, CNWSpell* +0x8};
// UseConcentration int32 at spell +0x174 (Load +0x51f); read only by
// OnApplyDamage (== 1: damage interrupts) and AIActionCastSpell (!= 0: entangled check).
constexpr size_t kRulesSpells = 0xd0;
constexpr size_t kSpellArrayCount = 0x0;
constexpr const char* kGetSpell = "?GetSpell@CNWSpellArray@@QEBAPEAVCNWSpell@@H@Z";
constexpr const char* kLoadSpellArray = "?Load@CNWSpellArray@@QEAAXXZ";
constexpr size_t kSpellUseConcentration = 0x174;
// void CNWSCreature::ResolveInitiative(): one RollDice(1, 20) (+0x43), then
// GetDEXMod(0) and the feat keys EPIC_SUPERIOR / IMPROVED / BLOODED / THUG
// _INITIATIVE_BONUS, each read only here.
constexpr const char* kResolveInitiative = "?ResolveInitiative@CNWSCreature@@QEAAXXZ";
constexpr const char* kGetInvisible = "?GetInvisible@CNWSCreature@@QEAAHPEAVCNWSObject@@H@Z";
constexpr int kFeatImprovedInitiative = 377;
// racialtypes.2da rows, as on 1.69.
constexpr int kRaceDwarf = 0;
constexpr int kRaceElf = 1;
constexpr int kRaceGnome = 2;
constexpr int kRaceHuman = 6;

// --- diffsettings.2da rows (EE data, D).
constexpr int kDifficultyNoCriticalOnPc = 0;  // 1 at DM-Easy, Easy and Normal

// --- Raw offsets (D = seen in EE code; see each note).

// Objects and creatures.
constexpr size_t kObjectId = 0x8;                     // uint32 (ResolveAttackRoll +0x32b: AddParryAttack(id))
constexpr size_t kObjectCurrentHitPoints = 0xe0;      // int16 read, int32 written (GetCurrentHitPoints, SetCurrentHitPoints)
constexpr size_t kCreatureStats = 0xad0;              // CNWSCreature -> CNWSCreatureStats* (ResolveAttackRoll +0x85)
constexpr size_t kStatsCreature = 0x30;               // CNWSCreatureStats -> CNWSCreature* (GetLevel +0x79)
constexpr size_t kCreatureIsPlayerCharacter = 0x89c;  // int32; ResolveAttackRoll's NoCriticalOnPC check (+0x3e9)
constexpr size_t kCreatureMaster = 0x8fc;             // uint32 object id (ExecuteCommandGetAssociateType)
constexpr size_t kCreatureAssociateType = 0x908;      // uint16: 1 henchman, 2 companion, 3 familiar, 4 summon, 5 dominated
constexpr size_t kStatsChallengeRating = 0xd0;        // float
constexpr size_t kStatsClassCount = 0xd8;             // uint8, at most 8 (GetClass / GetClassLevel bounds)
constexpr int kMaxClassSlots = 8;                     // class block +0xE0, 0x188 each, next field at +0xD20
constexpr size_t kStatsRace = 0xd20;                  // uint16 racialtypes row (GetAttackModifierVersus +0x1448)
constexpr size_t kObjectPositionX = 0xa4;             // float x, y, z at +0xa4 / +0xa8 / +0xac (SetPosition)
constexpr size_t kObjectPositionY = 0xa8;
constexpr size_t kObjectOrientationX = 0xb0;          // float x, y (SetOrientation writes +0xb0)
constexpr size_t kObjectOrientationY = 0xb4;
constexpr size_t kCreatureAnimation = 0xd8;           // int32; 0x04 / 0x56 / 0x57 = running (AIUpdate's AoO check)
constexpr size_t kCreaturePathInfo = 0x530;           // CPathfindInformation* (UpdatePersonalSpace)
constexpr size_t kPathInfoPersonalSpace = 0x8;        // float, appearance.2da CREPERSPACE (UpdatePersonalSpace)
constexpr size_t kCreatureInventory = 0x950;          // CNWSInventory* (GetRangedAttackBonus +0x20)
constexpr size_t kItemBaseItem = 0x40;                // int32 baseitems.2da row
constexpr size_t kBaseItemWield = 0x08;               // uint8 WeaponWield: 7 shield, 8 double (InitializeNumberOfAttacks)
constexpr uint8_t kWieldDouble = 8;
constexpr size_t kVisibilityFlags = 0x4;              // CNWVisibilityNode, bit 0 = seen (GetAttackModifierVersus +0x25b4)
// CNWSArea's object ids (uint32*) and count (int32), read by Get{First,Next}
// ObjectInArea — which also move a shared position (+0x238) that scripts'
// loops use, so nwn5e reads the array itself.
constexpr size_t kAreaObjectIds = 0x228;
constexpr size_t kAreaObjectCount = 0x230;
constexpr int kSlotRightHand = 0x10;
constexpr int kSlotLeftHand = 0x20;
constexpr int kRaceHalfling = 3;      // the constant GetAttackModifierVersus compares (rdata 0xdc3e68)
constexpr int kBaseItemSling = 61;    // 0x3d
constexpr int kFeatImprovedTwoWeaponFighting = 20;  // InitializeNumberOfAttacks: 2 off-hand attacks
constexpr int kFeatPointBlankShot = 27;
constexpr int kFeatTwoWeaponFighting = 41;

// The sheet's combat block (CCombatInformation at stats +0xd70): main-hand
// attacks per round, uint8 at +0x00 — UpdateCombatInformation's own
// (BAB + 4) / 5, at most 6 (+0x849).
constexpr size_t kCombatInfoAttacks = 0x00;

// CNWSCombatRound counts, int32 (InitializeNumberOfAttacks).
constexpr size_t kRoundMainHandAttacks = 0x2f50;
constexpr size_t kRoundOffHandAttacks = 0x2f54;
constexpr size_t kRoundExtraAttacks = 0x2f60;
constexpr size_t kRoundOwner = 0x2f78;  // CNWSCreature* (InitializeNumberOfAttacks +0x14: its inventory)
constexpr size_t kRoundTarget = 0x2f4c;  // uint32, the round's target (ResolvePostMeleeDamage's cleave writes it)
// CNWSCreature: set to 1 when the round's target moves on (the cleave, +0x712);
// its current attack target, as NWScript's GetAttackTarget reads it.
constexpr size_t kCreatureTargetSwitched = 0x708;
constexpr size_t kCreatureAttackTarget = 0x724;

// CNWSCreature -> its CNWSCombatRound* (ResolveAttackRoll +0x31).
constexpr size_t kCreatureCombatRound = 0x880;
// CNWSCombatRound: index of the attack being resolved, uint8 (ResolveAttackRoll +0x38).
constexpr size_t kRoundCurrentAttack = 0x2f2c;
// CNWSCombatAttackData (ResolveAttackRoll):
constexpr size_t kAttackD20 = 0x0e;          // uint8, the attack d20 (+0x218)
constexpr size_t kAttackModifier = 0x10;     // int32, the attack modifier (+0x201)
constexpr size_t kAttackRanged = 0x5c;       // int32, non-zero for a ranged attack (+0x59)
constexpr size_t kAttackType = 0x56;         // uint8, GetCurrentAttackWeapon's argument (GetDamageBonus +0x113)
constexpr size_t kAttackKilledTarget = 0x68; // int32, 1 after the attack killed (ResolvePost{Melee,Ranged}Damage)
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
