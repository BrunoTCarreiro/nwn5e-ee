# Character creation for 2024 — spike (2026-10-21)

Read-only research (no launches) after the owner's ask: "we need to figure
out how to update the character creation for 5e — an essential part of the
journey". **D** = read in EE's code or data, **I** = inferred. Full evidence
(addresses, dumps, scripts) was kept in the session's scratch notes; the
addresses below are EE 89.8193.37-17 Windows rvas unless marked Linux.

## 1. How EE's creation works

All of creation runs in the **client**, in 16 C++ panel classes that Windows
does not export (D). Their layouts are `.gui` files (`cg_main` + ~18 `cg_*`,
GFF "GUI V3.28", type 2047): positions, control models, caption strrefs only
— no page order, no logic (D). EE ships no `.jui`; NUI can't be used before
the character exists in a running module (D: `NuiCreate` resolves the player
through `GetClientObjectByObjectId`).

**Flow (D)** — the hub `CCharacterGenerationPanel::OnPopupPanelsExit`
(0x8e9be0) is a fixed switch on page exit codes:

Gender → Race → Portrait → Class → Alignment → Abilities → Packages →
(Recommended applies the package) or (Customize: Skills → Feats → [Domain]
→ [School] → [Spells] → [Familiar]) → Appearance → Info → Play.

- Alignment is skipped only for a Paladin with its NWN restriction (CM14
  cleared those fields, so every class gets the page now).
- The bracketed pages come from classes.2da (PickDomains, PickSchool,
  SpellbookRestricted, SkipSpellSelection, MinAssociateLevel).
- Play saves the character to LOCALVAULT and logs it in; the server's
  `LoadCharacterStart` → `CNWSPlayer::ValidateCharacter` (0x4bf210) →
  `LoadCharacterFinish` are exported. With Enforce Legal Characters off —
  always in single player — ValidateCharacter checks only the name, level
  range, MULTICLASS_LIMIT, item level limits and the level history (D): **no
  creation change can get a character rejected.**

**Reachable on Windows (D):** every panel's vtable through its RTTI name
(e.g. hub 0xe64678, Feats 0xe7b040, Skills 0xe7b430, Alignment 0xe64018);
each page's init is vtable slot 12 (PostAttachmentInitialize), which also
stores every button handler next to its tag ("OkButton", "RaceButton"...),
so all handlers are mapped (hub Play = 0x8e3f90). The exported
`CGuiPanel::Deactivate` (0x1fc470) and `CGuiModalPanel::Activate` (0x1fbe40)
see every page open and close. Pinned non-exported functions:
`CNWCLevelUpStats::CanChooseFeat` 0x7ed040 (unique debug string),
BuildFeatList 0x9c6540, SetNormalBonusFlags 0x7fa370. Hooking them needs a
hook-by-address helper next to HookExport.

## 2. What drives each page (and the lever)

| Page | Data that drives it | Lever |
|---|---|---|
| Race | racialtypes PlayerRace, name / description strrefs | data + the strings hook (cheap) |
| Class | classes PlayerClass, MULTICLASS_LIMIT | data |
| Alignment | 9 fixed buttons | skip = a small anchored patch (every class True Neutral, still changeable); not by data |
| Abilities | CHARGEN_* ruleset keys, race point budget | ruleset (done: 27 / 15, Recommended = the standard array) |
| Packages | packages.2da + PackFT / PackSK / PackEQ tables, gold | data: 2024 starting kits, a style first |
| Skills | points = f(class base, race SkillPointModifierAbility / ExtraSkillPointsPerLevel / FirstLevelSkillPointsMultiplier), max rank = level + SKILL_MAX_LEVEL_1_BONUS | 0 points = data (a no-op page); 2024 class picks at level 1 only = the same fields switched while the creation screen is open (moderate) |
| Feats | race +0x5c / +0x58 / +0x48, class GetBonusFeats(level), class feat table flags (bit 0 normal, bit 1 bonus), MinLevel | in-memory table flags (cheap): e.g. the Fighter's bonus pick offers only the Fighting Styles |
| Appearance, Portrait, Gender, Info | 2das, TLK | data |

NUI (scriptable windows): rich enough for a wizard (labels, text, buttons,
toggles/tabs, combo boxes, lists with templates, images, sliders, group
layout swaps, binds, watches); **not modal**, hidden by cutscene mode and the
H key, not restored after a save/load; a script string literal is capped at
8 KB (bigger layouts: per page, or `.jui` files served from `NWN5E:`).

## 3. The 2024 steps and where they can live

| 2024 step | Where | Status |
|---|---|---|
| Species | native Race page (names, texts; no ability scores) | built: adjustments 0; to do: 2024 names / texts (Half-Orc as Orc, CM6); new species need models |
| Class | native | — |
| Ability scores | native: 27-point buy to 15; Recommended = standard array | to fix: the class's primary ability can't go below 11 (CHARGEN_BASE_ABILITY_MIN_PRIMARY isn't answered) |
| Background (+2/+1, 2 skills, tool, Origin feat, kit) | **no native page** (adding one: costly; repurposing Schools / Domain / subrace: hacky) → nwn5e window after arrival | `!background` today |
| Class skills (Fighter 2 of 8) | native Skills page as 2024 picks (moderate), or the window | `!skills` today |
| Fighting Style | native: the Fighter's level-1 bonus pick limited to the 4 styles (cheap) | decided earlier as a window (A) — native now possible |
| Weapon masteries | window (or `!mastery` / by use) | — |
| Origin feat | from the background (window); a Human's extra one = the native level-1 general pick (CM8) | — |
| Equipment | class kit = package data (native); background kit = window | — |
| Alignment | native page as a description, or skipped | limits lifted (CM14) |
| Looks, name, portrait | native | — |

A window can't hold the game by itself: the character is held with
EffectCutsceneParalyze / SetCommandable and SetGuiPanelDisabled (cutscene
mode would hide the window). Its progress belongs in the per-character
SQLite database, which savegames and exported characters keep.

## 4. Recommended journey

1. **Native creation, tuned by data and small hooks:** 2024 species texts;
   the primary-ability floor fixed; Recommended = standard array; the
   Fighter's level-1 pick = a Fighting Style; 2024 starting kits in the
   packages; the Skills page as the 2024 class-skill pick (or a no-op).
2. **On arrival, nwn5e's "Finish your character" window** for what has no
   native page: background (ability split, skills, tool, Origin feat, kit),
   masteries, species choices (Human Skillful, Elf Keen Senses), a summary.
   The character is held until it's done; a new character is recognised at
   the server's character load (XP 0, no nwn5e record).
3. **Level-ups:** native level-up + a short window for 2024 choices (how to
   spend an ASI; swapping a style).

Not recommended: replacing creation with a template character
(Mod_DefaultBic) and a full window-based creation — no 3D preview, and race,
class and appearance would be written by hand.

## 5. Found on the way

- **Point buy:** the class's primary ability starts at 11 and can't drop
  below it — answer CHARGEN_BASE_ABILITY_MIN_PRIMARY 8 in faithful.
- **Records on the character:** nwn5e keeps its per-character choices in
  local variables (background, ASIs, ability deltas, skills, masteries,
  granted feats). An exported character (.bic) keeps no local variables (I:
  absent from SaveCreature), and campaign chapter changes carry the character
  by export: the choices would be lost, and the ability sync would take NWN's
  level +1s back a second time. To verify, then move the records to the
  per-character database.
- **Fighter 7:** with the bonus-flag lever, the level-7 slot would offer
  Ability Score Improvement as well as the styles (one list for every bonus
  slot) — accept, or switch the flags per level.
- **CM8 coupling:** with only the styles and ASI flagged as Fighter bonus
  feats, NWN's fighter feats (Power Attack, Weapon Focus...) leave the
  Fighter's feat slots — consistent with 2024, but it is CM8's decision.
