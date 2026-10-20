# EE port brief (2026-10-20)

How each feature built on 1.69 is rebuilt in NWN: Enhanced Edition
89.8193.37-17 (64-bit Windows, single player) by route B — after the spike
proved hooking by exported name in game (README). Sources: four read-only
research passes over EE's `nwmain.exe` (export table, call sites, disassembly
of exported bodies, `.pdata` bounds), the unstripped Linux build
`bin/linux-x86/nwmain-linux` (symbols, no debug types), EE's 601 2da tables
and `ovr/nwscript.nss`, checked against the 1.69 sources (nwn5e @ d6cf14a).
**D** = checked in EE's files or code, **I** = inferred. Decisions EP1–EP20
are at the end, each with a recommendation.

## The picture

- **~46 built features.** Effort in EE: about 27 small, 15 medium, 4 large.
- **Function hooks carry over by name.** Every 1.69 hook target that matters
  is exported in EE, often with the same signature (GetCriticalHitRoll,
  GetSkillRank, SavingThrowRoll, GetMaxHitPoints, AddFeat, ...). A few were
  renamed or reshaped: CanLevelUpInClass → `GetIsClassAvailable`,
  GetReactionTo → `GetAIStateReaction`, GetIsImmune → `GetEffectImmunity`,
  GetActionResponsePossible → gone (`CheckActionLength`).
- **Byte patches mostly become data.** 1.69 patched ~400 bytes at fixed
  addresses. In EE most of those rules are `ruleset.2da` values (the
  iterative and additional-attack −5, two-weapon penalties, two-handed
  Strength, Battle Training / Good Aim / Defensive Training, the situational
  attack terms, the save-roll feat bonuses, Blooded / Thug / Epic initiative,
  point buy, Pick Pocket DCs, Rogue-only traps, defensive casting, coup de
  grace), a 2da value (`diffsettings.2da` NoCriticalOnPC = B7), or a field of
  a loaded table (class XP penalty and alignment limits, race ability
  adjustments / point budget / general-feat cadence, class bonus-feat slots,
  spell UseConcentration). The 39 point-buy client patches disappear.
- **What stays hard:** crit confirmation (no lever: a dice hook), dice-only
  crit damage (a load-time call-site scan), the engine's fixed skill DCs and
  perception arithmetic (inline, some dice are CRT `rand`), the natural 20 on
  saves (inline), Push (new movement / line-test functions), the ability page
  every 4 levels (client-side).
- **Every engine offset changed** (64-bit layouts; CNWClass is now 0x530
  bytes). The agents found the ones the routes below need in EE code (D);
  the rest are re-found per rule.

## Key enabler: per-mode `ruleset.2da` values (D)

`CNWRules::GetRulesetIntEntry(key, default)` (710 call + 21 jump sites) and
`GetRulesetFloatEntry` (70) are called **at use time**. The key is the
**standard FNV-1a-64 of the label** (basis 0xcbf29ce484222325, prime
0x100000001b3; the compiler folds the first character into the seed, e.g.
seed 0xaf64034c86022ed1 = fnv1a("N") + "UM_ATTACKS_OF_OPPORTUNITY"; 475 call
sites checked against the 522 labels). One hook with a label → (faithful,
nwn, off) table answers per mode at runtime — data files stay vanilla, and a
thread-local "current creature" makes proficiency-valued keys possible.
Caveat: `SPELLCRAFT_NUM_RANKS_PER_SAVE_BONUS` is a divisor — never answer 0.

## Principles (proposed — EP1, EP2)

1. **Levers in order:** loaded-table data and ruleset values → NWScript →
   hook an exported function by name → **anchored patch** (bytes found by
   pattern inside a named export at load, checked, rule skipped and logged
   if not found) → never a fixed EE address.
2. **Per-mode differences live in memory** (ruleset hook, table fields
   written after each load), never in shipped tables.
3. **NWN mode = EE's vanilla behaviour** where EE differs from 1.69.
4. **Prefer exported accessors** (GetClass, GetClassLevel, GetLevel,
   GetCurrentAttackWeapon, ...). Raw offsets go in one table
   (`src/core/ee_layout.h`) with where each was seen, checked against an
   accessor at startup where one exists; re-checked on every EE update.
5. **Effects and messages through a script bridge** (`CVirtualMachine::
   RunScript` / `RunScriptChunk` + `SetScriptParameter`) where engine objects
   would need the engine's allocator (its `operator new` is not exported and
   the CRT is static, D).
6. Every hook logs at install; a missing export skips its rule, never crashes.

## Shared infrastructure (build once)

| Piece | EE exports (D) | Used by |
|---|---|---|
| Export hooks + caller naming | `GetProcAddress`; `ExportContaining` (spike 0.0.2) | all |
| Anchored-pattern helper | scan inside an export's `.pdata` bounds | nat-20 saves, crit dice scan, fallbacks |
| Ruleset hook + FNV table + creature context | `?GetRulesetIntEntry@CNWRules@@QEAAH_KH@Z`, `?GetRulesetFloatEntry@CNWRules@@QEAAM_KM@Z` | E1, R1, D1, extra attack, advantage, C7, SK3, SK7, SK9, point buy, coup de grace (~40 labels) |
| Dice hook + per-thread check context | `?RollDice@CNWRules@@QEAAGEE@Z` (76 sites) | crit confirmation, advantage, saves, initiative, scripted checks, touch attacks, Search, GWF, Halfling Luck |
| Attack roll pre/post | `?ResolveAttackRoll@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z` (5) | crits, advantage, mastery, conditions, shove, potion, trace |
| Save roll wrapper | `?SavingThrowRoll@CNWSCreature@@QEAAEEGEIHGH@Z` (21) | saves, conditions, Topple, Indomitable |
| Skill hook with per-context answers | `?GetSkillRank@CNWSCreatureStats@@QEAADEPEAVCNWSObject@@H@Z` (83) | skills, SK3, SK5, Shove, SK8 |
| Rules-loaded post-hooks | `??0CNWRules@@QEAA@XZ`, `?ReloadAll@CNWRules@@QEAAXXZ`, `Load{Feat,Class,Race}Info`, `?Load@CNWSpellArray@@QEAAXXZ` | XP table, alignment, multiclass, feat cadence, abilities, hidden rows, concentration |
| Served data | `?Add@CExoAliasList@@...`, `?AddResourceDirectory@CExoResMan@@...` (priority), `?Get@CExoResMan@@...` (reads the game's own tables — the KEY/BIF reader goes) | CM0 and everything with rows |
| Strings | `?FetchInternal@CTlkTable@@IEAAHIAEAUSTR_RES@@H@Z` (GetSimpleString is 1 of ~860 callers) | nwn5e strings, 2024 labels |
| Creature facts | GetClass / GetClassLevel / GetLevel / GetLevelStats; stats +0xAD0 | HP, PB, multiclass, features, henchmen |
| Locals | `CNWSScriptVarTable::GetInt / SetInt` | abilities, features, mastery, Tactical Shift |
| Per-creature update | `?AIUpdate@CNWSCreature@@UEAAXXZ` (virtual; body hook) | abilities sync, Heroic Rally, Push, potion |
| Chat dispatcher + tell | `?SendServerToPlayerChatMessage@CNWSMessage@@QEAAHEIVCExoString@@IAEBV2@@Z`, `SendServerToPlayerChat_ServerTell` | `!asi !background !features !mastery !saves !skills` |
| Rest | `?ResetFeatRemainingUses@CNWSCreatureStats@@QEAAXXZ` (2) | mastery swap, Indomitable |
| Difficulty options | `?GetDifficultyOption@CServerExoApp@@QEAAHH@Z` (6) | B7; NoAoOWithRanged / Potion |
| Script bridge | `RunScript`, `RunScriptChunk`, `SetScriptParameter` | Graze, Slow, Topple, icons, HP writes |
| Config, modes, log | ported from 1.69 (x64: no `__fastcall`, `%p`) | all |

## Feature routes

### Combat core (Phase 1a/1b)

| Feature | EE route | Effort | Notes |
|---|---|---|---|
| Threat on 20 only | hook GetCriticalHitRoll (spike) | S | baseitems data alone misses keen / Improved Critical / Champion |
| ×2 multiplier | hook `GetCriticalHitMultiplier` (49 sites) | S | |
| No confirmation, no "Threat Roll" | attack-roll hook + dice state machine (next d20 after a threat confirms); clear attack data +0x70 / +0x0F after | M | EP7; no ruleset key exists |
| Dice ×2, flat ×1 | GetDamageRoll / GetTotalEffectBonus call sites found by a load-time scan inside the exports (shape checked, rule skipped if different) | M | can't fix the total afterwards: DR / resistance apply inside |
| Crits on the party (B7) | hook `GetDifficultyOption`, option 0 → 0 | S | EE's NoCriticalOnPC is on at Easy **and Normal**; EP8 |
| PB instead of BAB + sheet | hooks `GetAttackModifierVersus`, `GetMeleeAttackBonus`, `GetRangedAttackBonus`, `UpdateCombatInformation` | M | touch attacks share these (SK10) |
| AC by CR | hook `GetArmorClassVersus` (returns int16) | S | CR at stats +0xD0 |
| Extra Attack, no −5 | hook `GetAttacksPerRound`; ruleset MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER(_MONK) → 0 | S | the key does not change attack counts (recon corrected); EP10 |
| Henchmen = PC rules | associate type at creature +0x908 | S | |
| Retarget after a kill | post-hooks `ResolvePostMeleeDamage` / `ResolvePostRangedDamage`; melee target from `GetNewCombatTarget`; ranged "in the fight" walk from exports | M | |
| Advantage / disadvantage | attack-roll hook decides; the first d20 RollDice returns the kept die; situational keys (flank, invisible, prone, moving, blind, stunned, ranged in melee) → 0 | M–L | no more subtracting 3e terms |
| No racial bonuses (R1) | keys OFFENSIVE_TRAINING / GOOD_AIM / DEFENSIVE_TRAINING → 0; halfling sling +1 still subtracted in hooks | S | sling bonus is hard-coded |
| Two-weapon fighting (D1) | 5 TWF keys → 0; OFFHAND_WEAPON_STRENGTH_MODIFIER 0.0 / 1.0 with the style; off-hand count via post-hook `InitializeNumberOfAttacks` | S–M | `CalculateOffHandAttacks` is inlined |
| Two-handed Str ×1 (E1) | TWO_HANDED_WEAPON_STRENGTH_MODIFIER → 1.0 | S | sheet damage follows |
| Debug switches | existing hooks; `combat_breakdown` = exported `g_bEnableCombatDebugging` | S | |

### Saves, skills and checks (Phase 1c, Phase 2)

| Feature | EE route | Effort | Notes |
|---|---|---|---|
| 2024 saves (C1–C8) | hooks on the 3 save getters, `CalculateSpellSaveDC`, the save-roll wrapper; d20 via the dice hook; natural 1 = the server option (forced per mode in memory); natural 20 = one anchored patch; C7 feat bonuses = ~20 ruleset keys → 0 | M | EP12 |
| Spell save abilities | wrap `ExecuteCommandSavingThrow` (one handler now) and `ExecuteCommandGetReflexAdjustedDamage` | S | combat-log label: EP13 |
| Skills (SK1–SK2) | hook GetSkillRank (same signature); ×2 for scripts / Lore via wrappers | S | sheet reads GetSkillRank |
| Skill DCs (SK3) | keys for Pick Pocket, Rogue-only traps, trap synergy; scripts: convert the DC in `ExecuteCommandGetIsSkillSuccessful`; engine actions: shift the skill by DC − conv(DC) while each runs + fix `BroadcastSkillData` (the log line of 17 checks) | L | EP11; lock / trap d20s are CRT `rand` |
| Stealth and Perception (SK5) | hook `SetStealthMode`; wrap `DoListenDetection` / `DoSpotDetection`, write the cached dice; Favored Enemy 0 during checks | L | in-function arithmetic handled by skill shifts |
| Shove / no Disarm (SK6) | hook `AddSpecialAttack` (drop Disarm); wrap `ResolveMeleeSpecialAttack` (auto-hit, no damage, our save, Discipline ±127) | M | |
| Concentration (SK7) | spell UseConcentration (+0x174) → 0 after load; DEFENSIVE_CASTING_BASE_DC → −1000 | S | |
| 3e-only skills (SK8) | `SetCombatMode` refuses parry; `AIActionTaunt`; `CNWSStore::AddCustomer` 0/0; `CheckUseMagicDeviceSkill` → 0 | S | EP14 |
| Initiative (SK9) | wrap `ResolveInitiative`; its RollDice gets advantage / Luck; keys BLOODED / THUG → 0, IMPROVED / EPIC_SUPERIOR_INITIATIVE → PB | S | |
| Spell attacks (SK10) | wrap `ExecuteCommandDoTouchAttack` (one handler): PB + casting mod, full AC, dice hook | M | |
| Conditions on checks (SK11) | contexts above; skip `UpdateTrapCheck` when blind | S | lock / trap / heal-kit d20s stay a gap |

### Character model (Phase 3) and the Fighter

| Feature | EE route | Effort | Notes |
|---|---|---|---|
| Served data (CM0) | alias + `AddResourceDirectory` at a chosen priority; tables built from `CExoResMan::Get` | M | EP5; KEY/BIF reader retired |
| Strings / labels | `CTlkTable::FetchInternal` hook | S | EP4 |
| XP and level cap (CM1) | XP table at CNWRules +0x28 after load; hook `CanLevelUp` (cap 20); flag in `ExecuteCommandSetXP` + hook `SetExperience` | S | EE's cap is a plain 40, no HotU check |
| Multiclassing (CM2) | class XPPenalty (+0x4E0) → 0; hook `GetIsClassAvailable` (13s, no prestige) | S | EP16 (class count) |
| Alignment (CM14) | class +0x4A1/+0x4A2/+0x4A4 cleared after load; hook `CheckItemAlignmentRestrictions` → 1 | S | the creation-screen patch is no longer needed |
| Hit points (CM3) | hook `GetMaxHitPoints` (virtual, returns short) | S | |
| Feat cadence (CM7) | race NormalFeatEveryNthLevel (+0x58) → 0; class bonus slots +0x2B9 + level; hidden rows MinLevel 0xFF via `GetFeat` | S | the ability page: EP15 |
| Ability scores / backgrounds (CM4, CM5) | keys CHARGEN_BASE_ABILITY_MAX 15, CHARGEN_ABILITY_COST_INCREMENT2 13, CHARGEN_ENABLE_RECOMMENDED_BUTTON 0; race budget +0x54 = 27, adjustments +0x18..+0x1D = 0; sync via `SetSTRBase` ... | S (sync M) | closes 1.69's "Recommended" gap |
| Class features frame (CM9, CM10) | `GetIsClassAvailable`; `AddFeat` / `RemoveFeat` / `SetFeatRemainingUses` / `GetFeatTotalUses`; AIUpdate | S | |
| Fighter (F0–F7) | Champion via GetCriticalHitRoll; Defense via `GetArmorClass` + `ComputeArmorClass`; GWF via the dice hook inside a `GetDamageRoll` context (weapon dice filter); Second Wind rows + script; Heroic Rally via AIUpdate; Indomitable via the save wrapper | M | GWF filter is I; EP6 for the script |
| Chat / labels | chat hook (new signature) + tell; FetchInternal | S | EP17 |

### Opportunity attacks, weapon mastery, conditions

| Feature | EE route | Effort | Notes |
|---|---|---|---|
| Opportunity attacks (D2) | hook `BroadcastAttackOfOpportunity`; faithful passes only the moving caller (AIUpdate); "runs away from" via `GetAIStateReaction` inside the broadcast | M | all 7 byte patches go; Spring Attack: EP19 |
| Resume after potion | hook `UseItem`, queue `AddAttackActions` after a potion | S | EP20: EE may already keep the order |
| Mastery: who (D3) | same design by name (rest hook, locals, chat) | M | |
| Vex, Sap | `EndCombatRound`, `OnApplyDamage` + attack-roll hook; marks optionally as `TagEffect` + `EffectIcon` | S | attack result at +0x78 |
| Graze, Slow, Topple | effects through the script bridge; Topple save via SavingThrowRoll | M | EP18 |
| Cleave | `AddCleaveAttack`, `GetDamageBonus` | M | EE's AddCleaveAttack now shortens the round |
| Push | AIUpdate slide; `SetPosition`, `UpdateSubareasOnJumpPosition`, `ComputeHeight`, the area line tests, `CheckActionLength` + round pause calls | L | most new work |
| Conditions (CN1–CN4, CN6) | `OnApplyEntangled` context drops the −2 / −4 sub-effects; COUP_DE_GRACE_LEVEL_LIMIT; `ResolveDefensiveEffects` context for the miss-chance roll | M | entangle penalties are hard-coded |
| Frightened (CN5) | hook `OnApplySetState`; icon via the bridge | S–M | |
| Deafened (CN8) | EE moved the 20% into `AIActionCastSpell` (any caster, verbal, not Silent); faithful: set the hearing bit for that call | S | EP1 covers NWN mode |

## Corrections to the recon (docs/EE-RECON.md)

- MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER removes the −5s but does not
  change how many attacks come (GetAttacksPerRound hard-codes /5).
- R1 is not fully data: the halfling sling +1 is hard-coded.
- FIRING_INTO_MELEE_MODIFIER is a dead key (not in nwmain.exe).
- B7 is `diffsettings.2da` NoCriticalOnPC, on at Easy and Normal.
- CanLevelUpInClass is exported, as `GetIsClassAvailable`.
- MIN_LEVEL_FOR_MAX_HP only affects the client's roll; the GetMaxHitPoints
  hook stays.
- EE ships no command-line script compiler (`util/win32` has DebugServer
  and GFFEditor only).

## Proposed order

Each step ends with an in-game test matrix, as on 1.69.

1. **E0 — foundation + crits.** Config / modes / log, hooks, ruleset hook,
   dice hook, attack-roll hook, difficulty hook; then the five crit rules
   (threat on 20, ×2, no confirmation, dice only, B7) — the smallest set that
   exercises every core piece.
2. **E1 — the rest of combat:** PB + sheet, AC by CR, Extra Attack,
   henchmen, retarget, advantage, R1, D1, E1, debug switches.
3. **E2 — opportunity attacks, conditions, weapon mastery** (Push last).
4. **E3 — saves and skills** (SK3 / SK5 last).
5. **E4 — character model:** served data + strings, XP, multiclass,
   alignment, HP, feat cadence, point buy + backgrounds, class features,
   the Fighter (first tested in game here — it never was on 1.69).

Served data and strings move to E0 if an earlier step needs rows or labels.

## Decisions (EP1–EP20)

| # | Question | Options | Recommendation |
|---|---|---|---|
| EP1 | What NWN mode means where EE changed 1.69's behaviour (deafness fails any caster's verbal spells; Search always rolls; Spot / Listen dice; no crits on PCs at Normal) | EE's vanilla / emulate 1.69 | **EE's vanilla** — it's the game players have |
| EP2 | Anchored patches (bytes found by pattern inside a named export at load; rule skipped if not found) | allow / never | **Allow, as a last resort** — today only the save natural 20 (+ the crit-dice call-site scan) |
| EP3 | Ruleset values | runtime hook / ship a ruleset.2da | **Hook** — per mode, per creature, data stays vanilla |
| EP4 | nwn5e's strings | FetchInternal hook / SetTlkOverride / module custom tlk | **Hook** — SetTlkOverride is saved into games; custom tlk clashes with modules |
| EP5 | Served folder priority | below module + haks (~15M, as 1.69) / above haks | **~15M** — a hak can still override nwn5e |
| EP6 | Script compiler (EE has none) | 1.69's clcompile (needs Diamond installed, old functions only) / neverwinter.nim's `nwn_script_comp` (open source, EE-aware; a pinned download into `tools/`, licence checked first) / compile at runtime from served source (`RunScriptChunk`) | **nwn_script_comp**, pinned; 1.69's compiler as the interim |
| EP7 | Crit confirmation | dice state machine / fix the result after / a full C++ ResolveAttackRoll | **Dice state machine** — keeps the engine's immunity handling |
| EP8 | B7 | own switch / fold into crit rules | **Own switch** (`crits_against_players`), as on 1.69 |
| EP9 | Sheet "Base Attack Bonus" line | stays 3e / hook GetBaseAttackBonus | **Stays 3e** — the hook would change feat prerequisites, polymorph, monster attacks |
| EP10 | NWN-mode second off-hand attack −5 (EE ties it to the iterative key) | always 0 / keep −5 via context | **Context-aware, low priority** |
| EP11 | Engine skill DCs (no DC getters; lock / trap dice are `rand`) | shift skill values while each action runs + fix the log line / ~25 anchored patches / skills ×2 vs 3e DCs | **Shift + log fix**; "take 20" out of combat stays NWN's for now |
| EP12 | Natural 20 on saves | one anchored patch / keep NWN's auto-success | **Anchored patch** |
| EP13 | Spell-save label in the combat log (client maps 1/2/3 only) | chat line ("Strength save") / swap TLK per message / client patch | **Chat line** |
| EP14 | Parry in faithful | refuse parry mode (SetCombatMode) / cosmetic toggle | **Refuse** — simplest, covers the AI |
| EP15 | The ability page every 4 levels (client-side) | leave it as a no-op + tell the player / 3 fixed-address patches / NUI later | **Leave it + tell** (CM4's sync already ignores it) |
| EP16 | Class count (MULTICLASS_LIMIT, now data: 3) | keep 3 / raise in faithful (2024 has no limit) | **Owner's call** — suggest testing 4 in the level-up panel first |
| EP17 | Chat commands vs NUI windows | chat now, NUI later / NUI now | **Chat now** for parity; NUI with Phase 7 |
| EP18 | How nwn5e creates effects | script bridge / engine constructors | **Script bridge** for Graze, Slow, Topple, icons; engine calls for Cleave and Push |
| EP19 | Spring Attack (its only engine use is the AoO skip) | faithful HasFeat hook / data change | **Hook** |
| EP20 | Resume after potion | test first / port now | **Test vanilla EE first** — it may already keep the attack |
