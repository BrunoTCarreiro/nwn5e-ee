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
