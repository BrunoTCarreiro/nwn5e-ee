# NWN: Enhanced Edition — recon for moving nwn5e (2026-10-20)

Read only (two research passes + checks). **D** = checked in the files or
official docs, **I** = inferred, **UNSURE** = needs a test. The owner's
install: `X:\SteamLibrary\steamapps\common\Neverwinter Nights` — EE
**89.8193.37-17** (build.txt commit 26c6e573, 6 Oct 2025; data r1284, Feb
2024). D.

## The key fact

**EE's 64-bit Windows `nwmain.exe` exports 18,245 named C++ functions**
(PE export table, decorated MSVC names), including the whole embedded
server: CNWSCreature, CNWSCreatureStats, CNWSCombatRound, CNWSMessage,
CNWSVirtualMachineCommands, CNWRules, CExoResMan, CTlkTable, CVirtualMachine.
Every function nwn5e hooks on 1.69 that was checked is there by name — e.g.
`?ResolveAttackRoll@CNWSCreature@@IEAAXPEAVCNWSObject@@@Z`,
`?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z`,
`?SavingThrowRoll@CNWSCreature@@QEAAEEGEIHGH@Z`, `GetDamageRoll`,
`GetSkillRank`, `GetMaxHitPoints`, `GetAttackModifierVersus`,
`GetArmorClassVersus`, `ResolveDamage`, `BroadcastAttackOfOpportunity`,
`LevelUp`, `ValidateLevelUp`, `CanLevelUp`, `AddExperience`,
`SetExperience`, `GetBonusFeats`, `HasFeat`, `AddFeat`, `GetSTRStat`,
`SetSTRBase`, `CNWSCreature::AIUpdate`, `CNWRules::LoadFeatInfo`,
`CExoResMan::AddResourceDirectory`, `CTlkTable::GetSimpleString`,
`CVirtualMachine::RunScript`. SavingThrowRoll has 21 direct call sites (the
exports are the real bodies). Not exported: `CanLevelUpInClass` (inlined or
renamed), the client's level-up / creation / sheet panel classes — the Linux
client `nwmain-linux` is not stripped and names them. D.

So nwn5e's design (loader + injected DLL + MinHook) can run in EE **single
player**, finding functions by name instead of by fixed address — which also
survives Beamdog patches far better. No public precedent found (UNSURE).

## What EE does as data (no code)

- **`ruleset.2da`** (retail copy: 423 keys; a user override beats it): about
  136 keys touch something nwn5e changed. Fully replaces nwn5e patches:
  two-handed Strength 1.5 → 1 (E1); racial attack / AC / Good Aim → 0 (R1);
  the two-weapon penalties and off-hand Strength (most of D1); Toughness,
  feat bonuses (Skill Focus, Great Fortitude …), Blooded / Thug / Epic
  initiative; point buy (`CHARGEN_*`). Partly: the iterative −5
  (`MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER` also sets when extra attacks
  come), `MIN_LEVEL_FOR_MAX_HP` (max or rolled; no fixed average),
  `IMPROVED_INITIATIVE_BONUS` (flat, not PB), the situational modifiers
  (zeroed, but no advantage roll), `MULTICLASS_LIMIT`. No key: level cap,
  the +1 ability every 4 levels, crit confirmation, automatic 1 / 20 on
  saves. D.
- **New 2da columns:** racialtypes (point-buy budget, general-feat cadence,
  extra feats at level 1, skill points), classes (caster setup, SpellcastingAbil
  …), skills (HideFromLevelUp), baseitems (crit threat / multiplier per
  weapon, weapon-feat links), spells (targeting shapes); new 2das ruleset,
  damagetypes, savingthrowtypes. D.
- **Resources:** a `development/` folder above everything (and
  reloadable), haks, custom tlk (`Mod_CustomTlk`), `SetTlkOverride` at
  runtime (replaces nwn5e's label hook). D.

## What EE adds in script

339 new NWScript functions (D): `EffectRunScript`, `TagEffect`, `EffectIcon`,
`UnyieldingEffect`, `EffectBonusFeat`, `SetCurrentHitPoints`,
`GetAttacksPerRound`, `ExecuteScriptChunk`, `SetEventScript`, json, SQLite
(module / campaign / per-PC databases), OnPlayerChat / OnPlayerGuiEvent /
OnPlayerLevelUp / OnNuiEvent. Still no setters for feats, abilities, skill
ranks, base saves, max HP, or an attack / damage-roll event (D). **NUI**
(scriptable windows) can replace `!asi`, `!background`, `!skills`,
`!mastery`, `!features` and the character sheet; it can't replace the native
level-up or creation wizards (D). No command-line compiler ships; the
open-source `nwn_script_comp` (neverwinter.nim) or `CompileScript()` (D).

## The campaigns in EE

`data/nwm/`: OC, SoU, HotU (14 modules) — 2019 rebuilds of the 1.69 modules
(a few script fixes, achievements), still no hak / custom tlk; spell, spawn
and shared level-up scripts are not embedded, so override / development
replace them (D).

## NWNX:EE (a local server)

Linux `nwserver` only (Docker / WSL2 on Windows), GPL-3; supports exactly
this build (8193.37.17, MD5 match); every EE patch needs an NWNX update; a C++
plugin SDK with 307 engine headers (D). Single player would mean the stock
client joining a local server: works for the campaigns in principle (co-op
plays them this way), but saving from the console / a command, no autosave,
a respawn flow instead of the load panel, a separate process — and the
client stays stock (I). Useful to nwn5e as a **reference** (class layouts, a
full C++ ResolveAttackRoll rewrite) — reference only: copying its code would
make nwn5e GPL-3.

## Routes (I)

| Route | Rough cost | |
|---|---|---|
| **B. EE single player, an x64 DLL hooking by export name** | ~15–25 sessions | true single player; nwn5e's design as is; layouts re-checked on the 64-bit build; some mid-function patches re-found (or replaced by whole-function hooks + ruleset.2da) |
| A. EE + a local NWNX server | ~15–25 | permanent friction (server process, saves, stock client) |
| C. Stay on 1.69 | 0 now | 32-bit, GOG only, no NUI; the cost of moving later grows with every hook |

What carries over in every route: `src/rules` (~2.7k lines, 115 tests), the
data rows and strings, the NWScript scripts, every decision and brief, the
RE knowledge (function names now match the community's). What doesn't: the
~10k lines of 1.69-specific engine code (addresses, byte patches, stubs) —
re-done against names.

## Per built feature (first guess, I)

> Superseded by **docs/EE-PORT.md** (2026-10-20, checked in EE's code; it
> lists the corrections to this section).

Data only: crits at 20 and ×2 (baseitems), E1, R1, point buy, alignment
limits, served data (native). Data + script: 2024 saves (cls_savthr = PB +
ruleset), feat cadence (racialtypes + cls_bfeat; ASI in NUI), skills
(effects + NUI), Shove (a spell feat), spell attacks (spell scripts), C1 save
abilities (spell scripts), mastery on-hit effects (Vex, Sap, Slow, Topple),
resume after a potion. Engine hooks (by name): attack roll (PB, advantage,
no confirmation), crit damage dice, the save roll (advantage, no 1 / 20,
species), opportunity attacks, retargeting, Extra Attack without −5,
perception / stealth, HP formula, the ability step every 4 levels (client
too), XP floor, multiclass 13s, Graze / Cleave, conditions on checks,
Fighter features (Champion crit range, Studied Attacks, Indomitable,
Fighting Styles), initiative PB / advantage.

## Recommendation

A half-session **spike for route B**: an x64 loader + DLL for EE that finds
`?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z` by name and makes every hit
crit (nwn5e's Phase 0 test), in EE single player. If it holds, move to EE by
route B, rebuilding feature by feature with EE's data levers first (ruleset /
2da / script / NUI) and name hooks for the rest; the 1.69 build stays in the
repo as the reference until EE matches it.
