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
// void CNWSCreatureStats::SetClass(uchar slot, uchar class) - the slot's class
// byte (stats + slot x 0x188 + 0x25C, GetClass); `!test`'s swap_class.
constexpr const char* kSetClass = "?SetClass@CNWSCreatureStats@@QEAAXEE@Z";
// The level history (kGetLevelStats, kLevelStatsClass above): GetLevelStats is
// 0-based (the list at stats +0x38, its count at +0x40, null past it); the
// class byte is what ReadStatsFromGff / SaveStats call "LvlStatClass". The
// level-up's check counts the classes in it ("invalid number of multiclasses"
// after a swap that left it on the old class, 0.22.11).
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
constexpr int kReactionFriend = 1;  // GetIsReactionTypeFriendly's value (0 neutral)
// CNWSCombatRound +0x2f40: the round's opportunity attacks left - nwn5e's one
// reaction (D2; the AoO broadcast decrements it: BZ3 Retaliation, R7 Uncanny Dodge).
constexpr size_t kRoundOpportunitiesLeft = 0x2f40;
// CNWSCombatAttackData +0x16: int16 damage per damage type, 32 of them (-1 unused)
// (GetDamage / SetDamage via CNWRules::GetDamageIndexFromFlags; +0x2e the base).
constexpr size_t kAttackDamageEntries = 0x16;
constexpr int kAttackDamageEntryCount = 32;

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
// int CNWSCreature::UseFeat(ushort feat, ushort subfeat, uint target, uint area, Vector* location):
// the player's feat use (HandlePlayerToServerInputMessage +0x32b) and ActionUseFeat;
// 1 once the use is queued (+0x9c05, +0x9cd6), 0 otherwise.
constexpr const char* kUseFeat = "?UseFeat@CNWSCreature@@QEAAHGGIIPEAVVector@@@Z";
// int CNWSCreature::AddAttackActions(unsigned int target, int, int bClearAllActions?, int)
constexpr const char* kAddAttackActions = "?AddAttackActions@CNWSCreature@@QEAAHIHHH@Z";
// UseFeat's Knockdown path asks it: no special attack with a ranged weapon (G1).
constexpr const char* kGetRangeWeaponEquipped = "?GetRangeWeaponEquipped@CNWSCreature@@QEAAHXZ";
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
constexpr int kIconPoison = 20;  // EFFECT_ICON_POISON: R6 Cunning Strike's Poisoned (no ability damage)
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
// CExoString CNWSScriptVarTable::GetString(CExoString& name) - returned by value (hidden result pointer).
constexpr const char* kVarTableGetString = "?GetString@CNWSScriptVarTable@@QEAA?AVCExoString@@AEAV2@@Z";
constexpr const char* kGetSTRStat = "?GetSTRStat@CNWSCreatureStats@@QEAAEXZ";
constexpr const char* kCalcStatModifier = "?CalcStatModifier@CNWSCreatureStats@@QEAADE@Z";
constexpr const char* kGetDEXMod = "?GetDEXMod@CNWSCreatureStats@@QEAADH@Z";
// int CNWSMessage::SendServerToPlayerChatMessage(uchar channel, uint sender, CExoString text,
// uint, const CExoString&) — a typed line arrives through it from HandlePlayerToServerChatMessage.
constexpr const char* kSendServerToPlayerChatMessage = "?SendServerToPlayerChatMessage@CNWSMessage@@QEAAHEIVCExoString@@IAEBV2@@Z";
constexpr const char* kHandlePlayerToServerChatMessage = "?HandlePlayerToServerChatMessage@CNWSMessage@@QEAAHPEAVCNWSPlayer@@E@Z";
constexpr const char* kResetFeatRemainingUses = "?ResetFeatRemainingUses@CNWSCreatureStats@@QEAAXXZ";
// X6 feature uses (Barbarian brief recon): uchar GetFeatRemainingUses(ushort
// feat) 0x4012F0 - the total inline, minus the feat entry's spent byte,
// clamped 0-100 (callers UseFeat, AIActionCastSpell, PlayerUpdate, talents,
// scripts); uchar GetFeatTotalUses(ushort) 0x401C70; Decrement / Increment
// change that feat id's byte (0x3F2750 / 0x40B9D0).
constexpr const char* kGetFeatRemainingUses = "?GetFeatRemainingUses@CNWSCreatureStats@@QEAAEG@Z";
constexpr const char* kGetFeatTotalUses = "?GetFeatTotalUses@CNWSCreatureStats@@QEAAEG@Z";
constexpr const char* kDecrementFeatRemainingUses = "?DecrementFeatRemainingUses@CNWSCreatureStats@@QEAAXG@Z";
// X4 2024 Resistance: int CNWSObject::DoDamageImmunity(CNWSCreature* attacker,
// int damage, uint damage-type flags, int simulate, int feedback) 0x491690 -
// the damage left after the object's % immunity to that type (it removes
// max(1, damage x % / 100)); OnApplyDamage asks it first, per type (vtable +0x160).
constexpr const char* kDoDamageImmunity = "?DoDamageImmunity@CNWSObject@@UEAAHPEAVCNWSCreature@@HIHH@Z";
// B2 no spells while raging: int CNWSCreature::AddCastSpellActions(uint spell,
// int class slot, int domain level, int metamagic, int spontaneous, Vector
// target location, uint target, int area target, int add to front, int fake,
// uchar projectile path, int instant, int, int feat, uchar) - a cast from the
// radial and ActionCastSpell*; int AddItemCastSpellActions(uint item, int
// property, int sub-property, Vector location, uint target, int area, int) -
// UseItem's item powers.
constexpr const char* kAddCastSpellActions = "?AddCastSpellActions@CNWSCreature@@QEAAHIHHHHVVector@@IHHHEHHHE@Z";
constexpr const char* kAddItemCastSpellActions = "?AddItemCastSpellActions@CNWSCreature@@QEAAHIHHVVector@@IHH@Z";
// void CNWSObject::RemoveEffect(CGameEffect*) - BZ2 Mindless Rage drops CN5's fear icon.
constexpr const char* kObjectRemoveEffect = "?RemoveEffect@CNWSObject@@QEAAXPEAVCGameEffect@@@Z";
// void CNWSObject::AddAction / AddActionToFront(uint action, ushort group, then 12
// (uint type, void* value) parameter pairs): every action reaches the queue
// through these (the scripts' Action*, the AI's, a player's orders). Phase 5b:
// a down henchman's are refused - the henchman AI scripts turn it commandable
// again (nw_ch_ac5 attacked, nw_ch_acb a harmful spell, nw_ch_ac4 its master's
// shouts) and it drank potions while down.
constexpr const char* kObjectAddAction = "?AddAction@CNWSObject@@QEAAXIGIPEAXI0I0I0I0I0I0I0I0I0I0I0@Z";
constexpr const char* kObjectAddActionToFront = "?AddActionToFront@CNWSObject@@QEAAXIGIPEAXI0I0I0I0I0I0I0I0I0I0I0@Z";
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
constexpr int32_t kCreatureSizeSmall = 2;
constexpr int32_t kCreatureSizeMedium = 3;
constexpr int kBaseItemWhip = 111;  // the client's size check answers 0 for it
// Weapon size (rules_weapon_size.cpp): the server's GetRelativeWeaponSize
// answers weapon size (CNWBaseItem uint8 +0x19) - creature size within
// -2..+1, else -10; the client's copy (not exported; 0x8012D0) answers the
// plain difference (the creature's appearance SIZECATEGORY) and starts with
// a whip check (cmp [rdx+40h], 6Fh; jne) — its anchor, a few bytes in.
constexpr const char* kGetRelativeWeaponSize = "?GetRelativeWeaponSize@CNWSCreature@@QEAAHPEAVCNWSItem@@@Z";
constexpr int kRelativeWeaponSizeInvalid = -10;
constexpr size_t kBaseItemWeaponSize = 0x19;
constexpr const char* kClientRelativeWeaponSizePattern = "83 7A 40 6F 75";
constexpr uintptr_t kClientRelativeWeaponSizeCheckMaxOffset = 0x40;
// The same size questions asked inline, patched in faithful (one byte each,
// all or none): CanEquipWeapon (weapon size - creature size > 1: "too
// large"), CanUseItem (the same, for the inventory's unusable mark), and the
// Tower Shield (base item 57) refused to Tiny and Small creatures in
// CanEquipItem, CanEquipShield and CanUseItem.
constexpr const char* kCanEquipWeapon = "?CanEquipWeapon@CNWSCreature@@IEAAEPEAVCNWSItem@@PEAIHHPEAVCNWSPlayer@@@Z";
constexpr const char* kCanEquipItem = "?CanEquipItem@CNWSCreature@@QEAAEPEAVCNWSItem@@PEAIHHHPEAVCNWSPlayer@@@Z";
constexpr const char* kCanEquipShield = "?CanEquipShield@CNWSCreature@@IEAAEPEAVCNWSItem@@HH@Z";
constexpr const char* kCanUseItem = "?CanUseItem@CNWSCreature@@QEAAHPEAVCNWSItem@@H@Z";
// Finesse: the engine asks GetWeaponFinesse (the feat, the weapon and the
// creature's size) for an attack's ability.
constexpr const char* kGetWeaponFinesse = "?GetWeaponFinesse@CNWSCreatureStats@@QEAAHPEAVCNWSItem@@@Z";
// The object's action queue: CExoLinkedListInternal* at +0x100 (head +0x0);
// node: data +0x10 (GetAtPos +0x4a). CNWSObjectActionNode: action id int32
// +0x0, first parameter in the 8-byte slot at +0x38 (AddActionNodeParameter
// +0x47); attack object = 0xC (ComputeAIState +0x47). While RunActions runs
// an action it takes the node off the queue and holds it as the current
// action: uint16 +0x90 (0xFFFF none), its target uint32 +0x94 (+0x97f).
constexpr size_t kObjectActionQueue = 0x100;
constexpr size_t kListHead = 0x0;
constexpr size_t kListNodeData = 0x10;
constexpr size_t kListNodeNext = 0x8;     // CExoLinkedListNode: prev +0, next +8, data +0x10
constexpr int kActionMoveToPoint = 1;     // the engine's action ids (AttackObject 0xC)
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
constexpr int kSaveWill = 3;

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
constexpr int kSkillPickPocket = 13;  // 2024's Sleight of Hand (T1 Fast Hands)
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
constexpr size_t kCreatureInCombat = 0x700;     // int32, GetIsInCombat; SetCombatState(1): 8 s timer +0x704
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
// The VFX editor (vfx_editor.cpp): its own folder (build\vfxedit), registered
// with detect_changes so a model written while the game runs is found;
// RemoveResourceDirectory + AddResourceDirectory re-reads it if not;
// Exists(resref, type, size*) checks. The editor's models are found before the
// served folder's.
constexpr uint32_t kVfxEditorPriority = 15000010;
constexpr const char* kRemoveResourceDirectory = "?RemoveResourceDirectory@CExoResMan@@QEAAHAEBVCExoString@@@Z";
constexpr const char* kResManExists = "?Exists@CExoResMan@@QEAAHAEBVCResRef@@GPEAI@Z";
constexpr uint16_t kResTypeMdl = 2002;
// The client reads a visual effect's models from the loaded visualeffects
// table each time it shows one (g_pRules +0x130 CTwoDimArrays, +0x280 the
// visualeffects C2DA - Load2DArrays +0x552; read at e.g. 0x96D37F with the
// column names inline), so the editor points its reserved row at a newly
// written model with C2DA::SetCExoStringEntry(int row, CExoString column,
// CExoString value) - strings by value: built with the engine's constructor,
// the callee owns them. SetBlankEntry(int row, CExoString column) clears one.
constexpr size_t kRulesTwoDimArrays = 0x130;
constexpr size_t kTwoDimArraysVisualEffects = 0x280;
constexpr const char* kC2DASetStringEntry = "?SetCExoStringEntry@C2DA@@QEAAHHVCExoString@@0@Z";
constexpr const char* kC2DASetBlankEntry = "?SetBlankEntry@C2DA@@QEAAHHVCExoString@@@Z";
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
// CNWRace: NumberNormalFeatsEveryNthLevel int32 +0x5C (the general feats at
// level 1 and at each Nth level) and ExtraFeatsAtFirstLevel int32 +0x48 (the
// human's bonus feat); both sides count level-1 general feats as their sum
// (the client's CalcNumberFeats / AddDefaultPackageFeats, 0x7EB040 read).
constexpr size_t kRaceNormalFeats = 0x5c;
constexpr size_t kRaceExtraFeatsAtFirstLevel = 0x48;
// CM8: the client's CNWCLevelUpStats::CanChooseFeat (0x7ED040; the one
// function with its own "Invalid Feat" message): (stats, feat id, general
// picks left, bonus picks left, CExoArrayList<uint16>* chosen) -> int. The
// Feats page (BuildFeatList) and the packages' default picks
// (AddDefaultPackageFeats) ask it for every feat.
constexpr const char* kCanChooseFeatMessage = "CNWCLevelUpStats::CanChooseFeat() Invalid Feat";
// The client's creature stats (CanChooseFeat's first argument; GetClass
// 0x7F10A0 and GetLevel 0x7F5D70 read): the class slot being levelled uint8
// +0x6A, the class count uint8 +0x69; 8 class slots of 0x150 bytes, each
// with its class id uint8 +0x295 and class level uint8 +0x296 (after the
// level-up being made).
constexpr size_t kClientStatsClassBeingLevelled = 0x6a;
constexpr size_t kClientStatsClassCount = 0x69;
constexpr int kClientStatsClassSlots = 8;
constexpr size_t kClientStatsClassSlotSize = 0x150;
constexpr size_t kClientStatsClassSlotClass = 0x295;
constexpr size_t kClientStatsClassSlotLevel = 0x296;
// The server's twin (0x3F1830; same arguments), which the automatic level-ups
// (LevelUpAutomatic — henchmen, the script's LevelUpHenchman, its one caller)
// ask for every feat they try (rules_feats.cpp).
// C5: the client stats' skill fields, as the Skills page's + and - (client
// 0x7f7a00 / 0x7ee3e0), its points (0x7ec0c0) and the package's picks
// (0x7eb300) read them: race uint16 +0x50 (racialtypes row), skill ranks
// int8* +0xd8 (one per skills.2da row; set through 0x7fa8b0, which also
// refreshes the totals beside them), skill points left uint16 +0x10e.
// CNWRules' skills.2da row count uint8 +0x107.
constexpr size_t kClientStatsRace = 0x50;
// C7a: the client stats' ability scores (Str .. Cha, uint8 +0x70..+0x75, as the
// level's skill points read them) and the feats it has (uint16* +0x118, count
// uint16 +0x120: CanChooseFeat's "already known" scan).
constexpr size_t kClientStatsAbilities = 0x70;
// The feat list's order (the Feats page's: BuildFeatList 0x9c6540 walks it and
// groups a master's sub-feats only while they come one after another): the
// feat ids uint16* CNWRules +0xf8, their count uint16 +0x102.
constexpr size_t kRulesFeatsSorted = 0xf8;
constexpr size_t kRulesFeatsSortedCount = 0x102;
// The Skills page (CCharacterSkillsPanel; its init 0x9cce50 registers
// RecommendButton -> 0x9c9a30, SkillsButtonBox -> 0x9cb730; BuildSkillList
// 0x9c4fd0): the creature's id uint32 +0xf80 (the client's GetCreature ->
// its stats +0x3b0); its rows (display order) void** +0x6f8, their count
// int32 +0x700, each row's skills.2da row int32* +0xa80. A row is greyed as
// the builder does it: CGuiCheckbox::SetEnabled(row, 0) and its name
// (row +0xb0) SetColor(0.5, 0.5, 0.5, 1).
constexpr size_t kSkillsPageCreature = 0xf80;
constexpr size_t kSkillsPageRows = 0x6f8;
constexpr size_t kSkillsPageRowCount = 0x700;
constexpr size_t kSkillsPageRowSkills = 0xa80;
constexpr size_t kSkillsRowName = 0xb0;
// Its list box (CGuiButtonListbox +0x660: the rows above are its own arrays,
// +0x98 / +0xa0 / +0x420; its selected row int32 +0x90 = page +0x6f0), its
// header line (text wrapper +0x630, "Allocate skill points."), its help box
// (text +0x228), its points box (text +0xbf8). CNWRules' skills (0x38 each,
// +0x120), HideFromLevelUp int32 +0x34: the only thing the builder skips by.
constexpr size_t kSkillsPageList = 0x660;
constexpr size_t kSkillsPageSelected = 0x6f0;
constexpr size_t kSkillsPageHeader = 0x630;
constexpr size_t kSkillsPageHelp = 0x228;
constexpr size_t kSkillsPagePoints = 0xbf8;
// The page's OK text button (its click {object +0x90, fn +0x98} at +0xe40 / +0xe48).
constexpr size_t kSkillsPageOk = 0xdb0;
// A list row's flags (+0x8): bit 0 set by SetEnabled(0). The page's row click
// (the list's select callback, 0x9cb730, stored after "SkillsButtonBox")
// tests it (mov eax,[rdx+8]; not eax; test al,1; jnz) and for a disabled row
// shows "Unavailable Skill" / "You may not learn this skill." (5847 / 5848).
constexpr size_t kGuiControlFlags = 0x8;
constexpr uint32_t kGuiControlDisabled = 0x1;
constexpr const char* kSkillsRowDisabledTest = "8B 42 08 F7 D0 A8 01 75";
// C8, "Skills again" (the chain: docs/CHARGEN.md C8): a Skills page is
// operator new(0xF98) + its constructor (this, creature id, level_up) - the
// one function referencing the page's vtable - then the exit delegate
// (+0x80, 16 bytes) copied and CGuiModalPanel::Activate; at a level-up the
// constructor adds the level's points (the points function) and keeps them
// at +0xF90 (the game's Recommended restores them). Its OK (0x9c8d10) asks
// CalcNumberFeats (stats, uint8* normal, uint8* bonus) whether a Feats page
// follows (the call below, unique in it), and after activating one points
// the level-up's page (+0x18 of GetLevelUp(*app)) at it (the pattern below:
// the app global's mov, the GetLevelUp call). The Feats page's OK appends its
// feats to the stats, then NextPage(creature, level_up, page, 0x800E, 0),
// then deletes its page (NextPage deactivates it).
constexpr size_t kSkillsPageSize = 0xf98;
constexpr size_t kSkillsPagePointsAtOpen = 0xf90;
constexpr const char* kRttiFeatsPageC8 = ".?AVCCharacterFeatsPanel@@";
constexpr const char* kSkillsOkFeatCountCall =
    "4C 8D 84 24 C0 00 00 00 48 8D 94 24 C8 00 00 00 48 8B 8D B0 03 00 00 E8";
constexpr size_t kSkillsOkFeatCountCallAt = 23;  // the E8
constexpr const char* kSkillsOkLevelUpPage = "48 8B 05 ?? ?? ?? ?? 48 8B 08 E8 ?? ?? ?? ?? 48 85 C0 74 04 4C 89 78 18";
constexpr size_t kSkillsOkLevelUpCallAt = 10;  // the E8 (GetLevelUp)
constexpr size_t kLevelUpPage = 0x18;          // the level-up's page now
constexpr size_t kRulesSkills = 0x120;
constexpr size_t kSkillStride = 0x38;
constexpr size_t kSkillHideFromLevelUp = 0x34;
constexpr const char* kListboxDeleteButtons = "?DeleteButtons@CGuiButtonListbox@@QEAAXXZ";
// The Skills page's Reset button (C5): a CGuiTextButton (0xE8, as the page's
// own: Cancel +0xCC8, OK +0xDB0, Recommend +0xE98) nwn5e makes and binds to
// the served layout's "ResetButton" inside the page's init; a click calls
// {object +0x90, fn(object) +0x98}; it is destroyed (vtable[0](b, 1)) before
// the page's own deleting destructor (vtable slot 0) frees the page.
constexpr const char* kTextButtonConstructor = "??0CGuiTextButton@@QEAA@XZ";
constexpr size_t kTextButtonSize = 0xe8;
constexpr size_t kButtonClickObject = 0x90;
constexpr size_t kButtonClickFunction = 0x98;
constexpr const char* kRttiSkillsPage = ".?AVCCharacterSkillsPanel@@";
constexpr const char* kStringWrapperSetText = "?SetText@CAuroraStringWrapper@@QEAAXAEBVCExoString@@@Z";
constexpr const char* kCheckboxSetEnabled = "?SetEnabled@CGuiCheckbox@@QEAAXH@Z";
constexpr const char* kStringWrapperSetColor = "?SetColor@CAuroraStringWrapper@@QEAAXMMMM@Z";
constexpr const char* kClassIsSkillUseable = "?IsSkillUseable@CNWClass@@QEAAHG@Z";
constexpr size_t kClientStatsFeats = 0x118;
constexpr size_t kClientStatsFeatCount = 0x120;
constexpr size_t kClientStatsSkillRanks = 0xd8;
constexpr size_t kClientStatsSkillPoints = 0x10e;
constexpr size_t kRulesSkillCount = 0x107;
constexpr const char* kServerCanChooseFeat = "?CanChooseFeat@CNWSCreatureStats@@QEAAHGEEPEAV?$CExoArrayList@G@@@Z";
// CExoArrayList<T>: the elements' pointer +0, their count int32 +8.
constexpr size_t kExoArrayListData = 0;
constexpr size_t kExoArrayListCount = 8;
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
// C8: the skill points a level-up banks (uint16, just before the ranks):
// ValidateLevelUp starts the level's budget from it, LevelUp stores the
// client's points left there.
constexpr size_t kStatsBankedSkillPoints = 0xdc6;
constexpr const char* kStatsSetSkillRank = "?SetSkillRank@CNWSCreatureStats@@QEAAXED@Z";
constexpr const char* kValidateLevelUp = "?ValidateLevelUp@CNWSCreatureStats@@QEAAIPEAVCNWLevelStats@@EEE@Z";
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

// E4c — class features (CM9, CM10) and the Fighter.
// void AddFeat(ushort): adds the feat once; a feat with uses per day gets a
// uses entry at full uses (+0xf7). void RemoveFeat(ushort): removes the feat
// and its uses entry. The player update compares the feat list and uses with
// the player's last copy (ComputeLastUpdate_GuiFeats), so the client follows.
constexpr const char* kAddFeat = "?AddFeat@CNWSCreatureStats@@QEAAXG@Z";
constexpr const char* kRemoveFeat = "?RemoveFeat@CNWSCreatureStats@@QEAAXG@Z";
// short CNWSCreature::GetArmorClass(): the sheet's AC (WriteGameObjUpdate_
// CharacterSheet), NWScript GetAC; GetArmorClassVersus doesn't call it.
constexpr const char* kCreatureGetArmorClass = "?GetArmorClass@CNWSCreature@@QEAAFXZ";
// int CNWSItem::ComputeArmorClass(): the item's base AC. "In armour" is the
// chest item's above 0 — GetDEXMod(1)'s own test (+0x2c slot 2, +0x45).
constexpr const char* kItemComputeArmorClass = "?ComputeArmorClass@CNWSItem@@QEAAHXZ";
// float CNWSCreature::GetMovementRateFactor(): + ruleset BARBARIAN_ENDURANCE_BONUS
// when HasFeat(194) (+0x4A), the Monk's + (level / 3) x 0.1 (a literal), caps
// 1.5 / 3.0 for monks (D, the Barbarian brief).
constexpr const char* kGetMovementRateFactor = "?GetMovementRateFactor@CNWSCreature@@QEAAMXZ";
// GetMovementRateFactor's first test: +0x788 == 1 returns the float at +0x78c
// as it is (a fixed rate - no class bonus, no caps); the Monk's Unarmored
// Movement (rules_monk.cpp) leaves it alone too.
constexpr size_t kCreatureFixedMovementRate = 0x788;
// int CNWSCreature::GetUseMonkAbilities() (0x396370): Monk (class 5) levels -
// or class 40's 1-3 - no armour (the chest item's ComputeArmorClass 0) and no
// shield (left hand base item 14 / 56 / 57); the weapon isn't checked.
constexpr const char* kGetUseMonkAbilities = "?GetUseMonkAbilities@CNWSCreature@@QEAAHXZ";
// char CNWSCreatureStats::GetACNaturalBase(int monk_part_only) (0x3f3e90): with
// the Monk abilities, + Wisdom (stats +0xd41, when positive, if the feat list
// or the bonus list holds 260) + Monk level / 5; argument 1 returns that part
// alone; then Pale Master and the epic Armor Skin.
constexpr const char* kGetACNaturalBase = "?GetACNaturalBase@CNWSCreatureStats@@QEAADH@Z";
// uchar CNWSCreature::CalculateDamagePower(CNWSObject* target, int offhand)
// (0x385610): with the unarmed Monk tables (GetUseMonkAttackTables(1)) at
// least ruleset KI_STRIKE_LEVEL_5..1 by feats 698, 697, 344, 343, 213. The
// values are DAMAGE_POWER_* (6 is ENERGY; +20 is 21).
constexpr const char* kCalculateDamagePower = "?CalculateDamagePower@CNWSCreature@@QEAAEPEAVCNWSObject@@H@Z";
constexpr int kSlotChest = 0x2;
// CNWBaseItem: weapon size uint8 +0x19 (GetDamageBonus +0x272: size minus the
// creature's size == 1, melee, not double = wielded two-handed); NumDice uint8
// +0x1a and DieToRoll uint8 +0x1b (GetDamageRoll +0x1cc / +0x1e6).
constexpr size_t kBaseItemSize = 0x19;
constexpr size_t kBaseItemNumDice = 0x1a;
constexpr size_t kBaseItemDieToRoll = 0x1b;
// GetDamageRoll(target, offhand, critical, sneak, death, force_max): 12
// RollDice calls; the weapon's own dice are the 1st (critical loop, +0x260)
// and the 3rd (+0x34c), RollDice(NumDice, DieToRoll); the 2nd (+0x2f2) is
// Overwhelming Critical, right after a GetRulesetIntEntry (+0x2df).
constexpr size_t kDamageRollRollDiceCalls = 12;

// Client NUI (nwn5e's own windows anywhere: main menu, creation, level-up —
// docs/CHARGEN.md §10, B1). The client builds a script window in
// CNWCMessage::HandleServerToPlayerNuiEvent(minor) (not exported: anchored as
// the one function holding a `lea` of the enable_make<JsonWindow> vtable; it
// calls ReadJSON 3 times), which needs only the in-game GUI object (alive
// from start-up, the main menu included) — no server, player or module. nwn5e
// writes the message with the engine's own CNWMessage writers into its own
// CNWMessage and hands it over (the handler only reads through it). Major
// 0x37 minors: 1 create (INT token, CExoString id, INT mode 1, JSON),
// 2 destroy (INT token), 4 binds (INT count, then INT token, CExoString
// name, JSON value), 5 set layout. JSON = UBJSON (data/ubjson.h): type 0x10,
// DWORD length, VOIDPtr bytes. Every client window event passes
// JsonWindow::QueueEvent(window, type*, element*, index, payload*) (void;
// anchored by a byte pattern that includes the token offset), the token at
// JsonWindow +0x230.
constexpr const char* kCNWMessageConstructor = "??0CNWMessage@@QEAA@XZ";
constexpr const char* kCreateWriteMessage = "?CreateWriteMessage@CNWMessage@@QEAAXIIH@Z";
constexpr const char* kWriteINT = "?WriteINT@CNWMessage@@QEAAXHH@Z";
constexpr const char* kWriteBYTE = "?WriteBYTE@CNWMessage@@QEAAXEHH@Z";
constexpr const char* kWriteType = "?WriteType@CNWMessage@@IEAAXW4MessageType@1@@Z";
constexpr const char* kWriteDWORD = "?WriteDWORD@CNWMessage@@QEAAXIH@Z";
constexpr const char* kWriteVOIDPtr = "?WriteVOIDPtr@CNWMessage@@QEAAXPEAXH@Z";
constexpr const char* kGetWriteMessage = "?GetWriteMessage@CNWMessage@@QEAAHPEAPEAEPEAI@Z";
constexpr const char* kSetReadMessage = "?SetReadMessage@CNWMessage@@QEAAHPEAEIIH@Z";
constexpr const char* kReadJSON =
    "?ReadJSON@CNWMessage@@QEAA?AV?$basic_json@Vmap@std@@Vvector@2@V?$basic_string@DU?$char_traits@D@std@@V?$"
    "allocator@D@2@@2@_N_J_KNVallocator@2@Uadl_serializer@nlohmann@@V?$vector@EV?$allocator@E@std@@@2@@nlohmann@@XZ";
// void CGuiMan::UpdateAndRender(float): every frame, before the NUI windows render.
constexpr const char* kGuiUpdateAndRender = "?UpdateAndRender@CGuiMan@@QEAAXM@Z";
// void CGuiPanel::Deactivate(int exit_code): every GUI page closing (the creation pages too).
constexpr const char* kGuiPanelDeactivate = "?Deactivate@CGuiPanel@@QEAAXH@Z";
constexpr const char* kRttiEnableMakeJsonWindow = ".?AV?$enable_make@VJsonWindow@JSON@Nui@@@@";
constexpr const char* kRttiJsonWindow = ".?AVJsonWindow@JSON@Nui@@";
constexpr const char* kQueueEventPattern = "49 83 78 10 00 0F 84 ?? ?? ?? ?? C7 45 BF 00 00 00 7F 8B 81 30 02 00 00";
constexpr int kClientNuiHandlerReadJsonCalls = 3;
constexpr size_t kJsonWindowToken = 0x230;
constexpr int kMessageTypeExoString = 0x0e;
constexpr int kMessageTypeJson = 0x10;
constexpr uint8_t kNuiMinorCreate = 1;
constexpr uint8_t kNuiMinorDestroy = 2;
constexpr uint8_t kNuiMinorBinds = 4;

// The game's own creation pages (docs/CHARGEN.md §10, route A; classic GUI,
// none of it exported: vtables by RTTI, handlers by layout tag, constructors
// by their vtable references). A page is a CGuiPanel whose controls are
// members: exit delegate {object +0x80, fn(object, code) +0x88}, layout
// CResRef +0xC1; CGuiPanel::Activate loads the layout, then calls the page's
// init (vtable slot 12), which binds each member to its layout object with
// ImportLayoutData(control, page, "Tag") and writes its callbacks — a click
// {object, fn(object)} at button +0x90, a list's selection {object,
// fn(object, index)} at list +0x80. OK / Cancel end with
// CGuiModalPanel::Deactivate(code) (the exit delegate runs inside) and
// vtable[0](page, 1).
constexpr const char* kGuiModalPanelActivate = "?Activate@CGuiModalPanel@@QEAAXXZ";
constexpr const char* kGuiModalPanelDeactivate = "?Deactivate@CGuiModalPanel@@QEAAXH@Z";
constexpr const char* kGuiPanelActivate = "?Activate@CGuiPanel@@QEAAXXZ";
constexpr const char* kImportLayoutData = "?ImportLayoutData@CGuiObject@@QEAAHPEAVCGuiPanel@@PEBD@Z";
constexpr const char* kSetTextStrref = "?SetText@CAuroraStringWrapper@@QEAAXI@Z";
constexpr const char* kSetReadOnly = "?SetReadOnly@CGuiEdit@@QEAAX_N@Z";
constexpr const char* kRegisterUpdate = "?RegisterUpdate@CGuiObject@@QEAAXXZ";
constexpr const char* kTextButtonSetEnabled = "?SetEnabled@CGuiTextButton@@QEAAXH@Z";
constexpr const char* kSetModel = "?SetModel@CGuiObject@@QEAAXPEBD@Z";
constexpr const char* kListAddButton = "?AddButton@CGuiButtonListbox@@QEAAHPEAVCGuiRadioButton@@H@Z";
constexpr const char* kListDeleteButtons = "?DeleteButtons@CGuiButtonListbox@@QEAAXXZ";
constexpr const char* kTabSetSelectTab = "?SelectTab@CGuiTabSet@@QEAAXH@Z";
constexpr const char* kResRefAssign = "??4CResRef@@QEAAAEAV0@PEBD@Z";
constexpr const char* kCheckboxConstructor = "??0CGuiCheckbox@@QEAA@XZ";
// The engine's operator new: inside CGuiPanel::Activate, the scene view's
// allocation (lea r14,[rbx+1C]; lea ecx,[rdi+58]; call new).
constexpr const char* kOperatorNewPattern = "4C 8D 73 1C 8D 4F 58 E8 ?? ?? ?? ??";
constexpr size_t kOperatorNewCall = 7;  // the E8 in the pattern
constexpr const char* kRttiHub = ".?AVCCharacterGenerationPanel@@";
constexpr const char* kRttiClassPage = ".?AVCCharacterClassPanel@@";
constexpr const char* kRttiAlignmentPage = ".?AVCCharacterAlignmentPanel@@";
constexpr const char* kRttiCharGenRadioButton = ".?AVCGuiCharGenRadioButton@@";
// The Customize chain's next page (C1): one unexported function (0x9C69A0)
// that every OK of Skills / Feats / Domain / School / Spells / Familiar calls
// — (creature id, level-up, the calling page, its exit code, reset) -> int —
// steps a static state through Domain (class PickDomains int +0x4F4), School
// (PickSchool int +0x4F8), Spells and Familiar (MinAssociateLevel uint8
// +0x524) for the class just taken (Domain and School at class level 1 only).
// Anchored as the one call the Feats and School pages' OK handlers share
// besides GetCreature, checked by its PickSchool test (cmp [rbx+4F8h], 0).
// The server reads the same flags (SaveClassInfo writes School only when
// PickSchool is set): change them only for the length of one call.
constexpr const char* kRttiFeatsPage = ".?AVCCharacterFeatsPanel@@";
// The Feats page (0x1240 bytes): its creature id +0xF8, the feats chosen at
// this level (the CExoArrayList<uint16> its list builder hands CanChooseFeat)
// +0x1230, its Title label +0xB30 (text +0x80: +0xBB0). Its init (vtable
// slot 12) ends in a jump to the list builder (BuildFeatList, 0x9c6540), which
// every add, remove, Recommended and Reset calls again; the init's
// GetCreature call is the one after `mov ecx,[rdi+0xF8]`.
constexpr size_t kFeatsPageCreature = 0xf8;
constexpr size_t kFeatsPageChosen = 0x1230;
constexpr size_t kFeatsPageTitleText = 0xbb0;
constexpr const char* kFeatsInitTailJump = "48 8B CF 48 8B 5C 24 60 48 8B 74 24 68 48 83 C4 50 5F E9";
constexpr size_t kFeatsInitTailJumpAt = 18;  // the E9
constexpr const char* kFeatsInitGetCreature = "8B 8F F8 00 00 00 E8";
constexpr size_t kFeatsInitGetCreatureAt = 6;  // the E8
constexpr const char* kRttiSchoolsPage = ".?AVCCharacterSchoolsPanel@@";
constexpr const char* kNextPagePickSchoolTest = "83 BB F8 04 00 00 00";
constexpr size_t kClassPickDomains = 0x4f4;
constexpr size_t kClassPickSchool = 0x4f8;
constexpr size_t kClassMinAssociateLevel = 0x524;
constexpr int kVtablePageInit = 12;  // PostAttachmentInitialize
constexpr int kPageVtableSlots = 41;
constexpr size_t kPageExitObject = 0x80;
constexpr size_t kPageExitFunction = 0x88;
constexpr size_t kPageLayout = 0xc1;
constexpr size_t kHubCustomizeMode = 0xf4;  // int: editing an existing character
// The Alignment page (0x1050 bytes): its creature id +0x1018; its OK fetches
// the client creature (the handler's first call) and writes uint16 law/chaos
// +0xC6 and good/evil +0xC8 of the stats at creature +0x3B0 (15 / 50 / 85),
// then exits with 0x8001 — the hub marks the step done and enables Abilities.
constexpr size_t kAlignmentPageSize = 0x1050;
constexpr size_t kAlignmentPageCreature = 0x1018;
constexpr size_t kClientCreatureStats = 0x3b0;
constexpr size_t kClientStatsLawChaos = 0xc6;
constexpr size_t kClientStatsGoodEvil = 0xc8;
constexpr int kAlignmentNeutral = 50;
constexpr int kExitAlignmentDone = 0x8001;
// The Customize chain's exit codes: the hub marks the Packages step done on
// the Feats page's (0x800E) and ignores the Skills page's (0x8010).
constexpr int kExitSkillsPage = 0x8010;
constexpr int kExitFeatsPage = 0x800e;
// The Class page (0xE30 bytes; ctor (page, creature id, mode)): Title label
// +0xF8 (flags +0x100, id +0x110, text +0x178, click +0x190), HelpBox +0x1A0
// (body +0x220, title +0x628), list +0x658 (selection +0x6D8, selected index
// +0x6E8, ids int* +0xA78), Cancel +0xB70 (click +0xC00), OK +0xC58 (click
// +0xCE8), Recommended +0xD40 (click +0xDD0).
constexpr size_t kClassPageSize = 0xe30;
constexpr size_t kClassPageCreature = 0xe28;  // uint32, the ctor's creature id
constexpr size_t kPageTitle = 0xf8;
constexpr size_t kPageTitleFlags = 0x100;
constexpr size_t kPageTitleId = 0x110;
constexpr size_t kPageTitleText = 0x178;
constexpr size_t kPageTitleClick = 0x190;
constexpr size_t kPageHelpBox = 0x1a0;
constexpr size_t kPageHelpBody = 0x220;
constexpr size_t kPageHelpTitle = 0x628;
constexpr size_t kPageList = 0x658;
constexpr size_t kPageListSelect = 0x6d8;
constexpr size_t kPageListSelected = 0x6e8;
constexpr size_t kPageListIds = 0xa78;
constexpr size_t kPageCancel = 0xb70;
constexpr size_t kPageCancelClick = 0xc00;
constexpr size_t kPageOk = 0xc58;
constexpr size_t kPageOkClick = 0xce8;
constexpr size_t kPageRecommend = 0xd40;
constexpr size_t kPageRecommendClick = 0xdd0;
constexpr size_t kListTabSet = 0x80;  // the list's CGuiTabSet (SelectTab)
// The Ability Scores page (0x1E30 bytes; ctor (page, creature id) = the call
// after `new` in the hub's AbilitiesButton handler; init 0x8EA6A0 read):
// per ability a name label and a value label (CGuiCharGenLabel 0xA8, text
// +0x80) and down / up arrows (CGuiTimerButton 0xF0, click {object, fn} at
// +0xD0); "Remaining Points" box +0x6E0 (text +0x760), "Cost To Increase"
// box +0xAD0 (text +0xB50); HelpEdit +0xB78 (body +0xBF8, title +0x1000);
// Title +0xF8 (text +0x178); Cancel +0x1B70 (click +0x1C00), OK +0x1C58
// (click +0x1CE8), Recommended +0x1D40 (click +0x1DD0); creature id +0x1E28.
// Its Cancel = Deactivate(2) + delete (the hub ignores 2).
constexpr const char* kRttiAbilitiesPage = ".?AVCCharacterAbilitiesPanel@@";
constexpr const char* kButtonSetEnabled = "?SetEnabled@CGuiButton@@QEAAXH@Z";
constexpr const char* kSetColor = "?SetColor@CAuroraStringWrapper@@QEAAXMMMM@Z";
constexpr size_t kAbilitiesPageSize = 0x1e30;
constexpr size_t kAbilitiesPageCreature = 0x1e28;
constexpr size_t kAbilitiesHelpBox = 0xb78;
constexpr size_t kAbilitiesHelpBody = 0xbf8;
constexpr size_t kAbilitiesHelpTitle = 0x1000;
constexpr size_t kAbilitiesPoints = 0x6e0;
constexpr size_t kAbilitiesPointsText = 0x760;
constexpr size_t kAbilitiesCost = 0xad0;
constexpr size_t kAbilitiesCostText = 0xb50;
constexpr size_t kAbilitiesCancel = 0x1b70;
constexpr size_t kAbilitiesCancelClick = 0x1c00;
constexpr size_t kAbilitiesOk = 0x1c58;
constexpr size_t kAbilitiesOkClick = 0x1ce8;
constexpr size_t kAbilitiesRecommend = 0x1d40;
constexpr size_t kAbilitiesRecommendClick = 0x1dd0;
constexpr size_t kLabelText = 0x80;
constexpr size_t kTimerButtonClick = 0xd0;
// A page title's flags: 4 = take the text box from the layout (Obj_Label_Width
// / Height, read by the label's layout import 0x1FE260); without it the box has
// no size and the centred caption lands off its place. Set before the import.
constexpr uint32_t kLabelSizeFromLayout = 4;
// A CGuiTextButton's caption (CAuroraStringWrapper). Disabling one takes two
// calls, as the pages do: SetEnabled(0) stops the clicks, SetColor(0.5, 0.5,
// 0.5, 1) greys the caption (1, 1, 1, 1 when enabled).
constexpr size_t kTextButtonCaption = 0xd0;
constexpr float kDisabledShade = 0.5f;
struct AbilityRow {
  const char* label;
  const char* value;
  const char* down;
  const char* up;
  size_t label_at, value_at, down_at, up_at;
};
// Str, Dex, Con, Int, Wis, Cha (rules::Ability order).
constexpr AbilityRow kAbilityRows[6] = {
    {"StrLabel", "StrEdit", "StrDownButton", "StrUpButton", 0x830, 0x8d8, 0x17b0, 0x18a0},
    {"DexLabel", "DexEdit", "DexDownButton", "DexUpButton", 0x440, 0x4e8, 0x13f0, 0x14e0},
    {"ConLabel", "ConEdit", "ConDownButton", "ConUpButton", 0x2f0, 0x398, 0x1210, 0x1300},
    {"IntLabel", "IntEdit", "IntDownButton", "IntUpButton", 0x590, 0x638, 0x15d0, 0x16c0},
    {"WisLabel", "WisEdit", "WisDownButton", "WisUpButton", 0x980, 0xa28, 0x1990, 0x1a80},
    {"ChaLabel", "ChaEdit", "ChaDownButton", "ChaUpButton", 0x1a0, 0x248, 0x1030, 0x1120},
};
// nwn5e's own exit code between its two Background pages: the hub's switch
// has no case for it (its default only refreshes the character sheet).
constexpr int kExitBackgroundNext = 0x8100;
// A list item: CGuiCharGenRadioButton, 0xC8 bytes, caption +0xB0.
constexpr size_t kListItemSize = 0xc8;
constexpr size_t kListItemCaption = 0xb0;
constexpr const char* kListItemModel = "CTL_CG_BTN_CLASS";

// NUI (EE's scriptable windows). HandlePlayerToServerNuiEvent reads each event
// (token, type, element, array index, payload) and, for a window the player
// has open, stores it with CNWSModule::SetCurrentNuiEvent (+0x82f) before
// running the window's or the module's event script (RunEventScript 0x15) —
// then still uses the window's state, so a window must not be destroyed from
// inside the event (nwn5e closes windows a tick later). Nui::JSON::Event:
// player object id +0x00, window token +0x04, type std::string +0x08 ("click",
// "close", ...; the handler tests its length for "close"), element id
// std::string +0x28, array index int32 +0x48, payload json +0x50.
constexpr const char* kSetCurrentNuiEvent = "?SetCurrentNuiEvent@CNWSModule@@QEAAXAEBUEvent@JSON@Nui@@@Z";
constexpr size_t kNuiEventPlayer = 0x00;
constexpr size_t kNuiEventToken = 0x04;
constexpr size_t kNuiEventType = 0x08;
constexpr size_t kNuiEventElement = 0x28;
constexpr size_t kNuiEventArrayIndex = 0x48;

// --- diffsettings.2da rows (EE data, D).
constexpr int kDifficultyNoCriticalOnPc = 0;  // 1 at DM-Easy, Easy and Normal

// --- Raw offsets (D = seen in EE code; see each note).

// Objects and creatures.
constexpr size_t kObjectId = 0x8;                     // uint32 (ResolveAttackRoll +0x32b: AddParryAttack(id))
constexpr size_t kObjectCurrentHitPoints = 0xe0;      // int16 read, int32 written (GetCurrentHitPoints, SetCurrentHitPoints)
constexpr size_t kCreatureStats = 0xad0;              // CNWSCreature -> CNWSCreatureStats* (ResolveAttackRoll +0x85)
constexpr size_t kStatsCreature = 0x30;               // CNWSCreatureStats -> CNWSCreature* (GetLevel +0x79)
// Phase 5a: rests (docs/PLAN.md Phase 5 recon). CNWSCreature::Rest(bAssociate,
// bEnemyLOS) starts every rest but ForceRest; AIActionRest restores HP and
// spell levels along the way at +0x854 / +0x85C ms each (computed only when
// 0); +0x574 is the "exerted" timer (damage, a spell cast: ExcitedDuration.2da,
// 10 s), +0x8E0 the activity bits (0x10 resting).
constexpr const char* kCreatureRest = "?Rest@CNWSCreature@@QEAAXHH@Z";
constexpr const char* kIncrementFeatRemainingUses = "?IncrementFeatRemainingUses@CNWSCreatureStats@@QEAAXG@Z";
constexpr const char* kGetAssociateId = "?GetAssociateId@CNWSCreature@@QEAAIGH@Z";
constexpr size_t kCreatureRestHpRate = 0x854;
constexpr size_t kCreatureRestSpellRate = 0x85c;
constexpr size_t kCreatureExcitedTimer = 0x574;
constexpr size_t kCreatureActivities = 0x8e0;
constexpr uint32_t kActivityResting = 0x10;
constexpr int kAssociateTypeHenchman = 1;
// Phase 5b: dying (docs/PLAN.md Phase 5 recon). CNWSCreature::DoDamage applies
// a hit's damage (temporary HP first, the immortal floor at 1) and writes HP
// (+0xE0, int32); its one caller, OnApplyDamage, then lays a player character
// between -10 and 0 down (dying, uncommandable) and signals OnPlayerDying, or
// applies death at -10 or below. The module's event handler stores the
// event's player (+0x388 OnPlayerDeath, +0x38C OnPlayerDying, +0x390
// OnPlayerRespawn) and runs the script: RunEventScript(9 / 10 / 11), its
// result unused (D, 0x47CF74..0x47D0C1).
constexpr const char* kCreatureDoDamage = "?DoDamage@CNWSCreature@@UEAAXH@Z";
constexpr const char* kModuleRunEventScript = "?RunEventScript@CNWSModule@@QEAAHHPEAVCExoString@@@Z";
constexpr int kModuleEventPlayerDeath = 9;
constexpr int kModuleEventPlayerDying = 10;
constexpr int kModuleEventPlayerRespawn = 11;
constexpr size_t kModuleLastPlayerDied = 0x388;
constexpr size_t kModuleLastPlayerDying = 0x38c;
constexpr size_t kModuleLastRespawner = 0x390;
constexpr int kHitPointsDead = -10;
// CNWSEffectListHandler::OnApplyDeath: an immortal creature (+0x8B8) is set to
// 1 HP and the handler returns 1 (the effect done, no death) - the path nwn5e
// takes for death magic on a player's henchman (D, 0x42AAAE).
constexpr const char* kOnApplyDeath = "?OnApplyDeath@CNWSEffectListHandler@@QEAAHPEAVCNWSObject@@PEAVCGameEffect@@H@Z";
// CServerExoApp::GetActivePauseState(): 0 running, 1 paused, 2 paused by a DM
// (the pause byte, CServerExoAppInternal +0x10108, D). Creature updates go on
// while the game is paused (0.19.4's death saves kept rolling), so rules that
// count time ask it.
constexpr const char* kGetActivePauseState = "?GetActivePauseState@CServerExoApp@@QEAAEXZ";
// A creature's posture (uint8 +0x6B4): 3 / 4 lying dead or dying (OnApplyDamage
// and OnApplyDeath set it by the blow's direction), 5 / 6 knocked down, else
// standing or sitting. CNWSCreature::SetAnimation(0 or 1) turns "stand" into
// the posture's own animation (4 -> 6 or 8, 3 -> 8); the animation is int32
// +0xD8 (CNWSObject::SetAnimation). Only ClearAction and
// RecomputeAmbientAnimationState set a standing posture - both keep a dead or
// dying creature lying (D, every write to +0x6B4 in the .text).
constexpr size_t kCreaturePosture = 0x6b4;
constexpr uint8_t kPostureLyingBack = 3;
constexpr uint8_t kPostureLyingFront = 4;
// Standing: what ClearAction (+0x4B8) and RecomputeAmbientAnimationState
// (+0x2A) write for a creature that is neither dead nor dying (0 only in the
// constructor; AIActionSit writes 1 too). 0.22.10: a henchman healed while
// down kept OnApplyDamage's lying posture - nothing wrote standing again.
constexpr uint8_t kPostureStanding = 1;
constexpr size_t kObjectAnimation = 0xd8;
constexpr const char* kCreatureSetAnimation = "?SetAnimation@CNWSCreature@@UEAAXH@Z";
// Phase 5a: the HUD's Short Rest button (hud_rest.cpp). The buttons under the
// portrait belong to CPanelPartyBar (vtable slot 11 = its per-frame Update;
// built in code: SetModel + AddToPanel on pnl_party_bar.mdl's hooks); each is
// a CGuiButton (flags +0x8, bit 0 disabled; its model +0x10; tooltip strref
// +0x70; click {object +0x90, fn +0x98}; the party bar's subclass shows the
// feedback id at +0xD0 when clicked disabled). Rest +0x788, Spellbook +0x6B0,
// Player List +0x860 (disabled in single player: *g_pAppManager +0x28 = 0).
// A model's ReplaceTexture(old, new) is its vtable slot 12 (Gob).
constexpr const char* kRttiPartyBar = ".?AVCPanelPartyBar@@";
constexpr int kGuiPanelVtableUpdate = 11;
constexpr size_t kPartyBarSpellButton = 0x6b0;
constexpr size_t kPartyBarRestButton = 0x788;
constexpr size_t kPartyBarPlayerListButton = 0x860;
constexpr size_t kGuiObjectModel = 0x10;
constexpr size_t kButtonTooltip = 0x70;
constexpr size_t kPartyBarButtonFeedback = 0xd0;
constexpr uint16_t kFeedbackCantRest = 0xd6;
constexpr uint32_t kStrSpellbook = 7038;
constexpr int kGobVtableReplaceTexture = 12;
constexpr size_t kAppManagerMultiplayer = 0x28;
constexpr const char* kAppManagerGlobal = "?g_pAppManager@@3PEAVCAppManager@@EA";
constexpr const char* kGuiObjectSetModel = "?SetModel@CGuiObject@@QEAAXPEBD@Z";
constexpr const char* kGuiObjectAddToPanel = "?AddToPanel@CGuiObject@@QEAAXPEAV1@PEBD@Z";
constexpr const char* kGuiButtonSetEnabled = "?SetEnabled@CGuiButton@@QEAAXH@Z";
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
