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

## 6. Decided and built

**Decided (owner, 2026-10-21): fully native** — every 2024 choice on the
creation screens, so the character is complete when the game starts; the
background prototype first.

*Background prototype (0.5.5):* the level-1 general pick lists the four SRD
backgrounds as master feats (masterfeats rows 40–43), each opening its seven
ability splits (feat rows 1230–1257, `rules::BackgroundFromFeat`; general for
every class, level 1 only — MaxLevel 1, as NWN's own background feats). The
Fighter packages put Soldier first (Str +2 / Con +1; the finesse package Dex
+2 / Con +1), so Recommended takes it. nwn5e reads the background from that
feat (the record travels with the character, exports included); `!background`
shows it and changes it by swapping the feat; an older `!background` record in
local variables still counts. NWN's general feats stay in that pick for now
(trade-off 1 not settled); a Human's second level-1 pick could take a second
background (the first one counts). NWN mode hides the rows.

Owner's test of 0.5.5: two backgrounds can be picked (any character with two
general picks) — the screen has no "one of these" rule; fixed by the plan
below (C3).

## 7. The ideal 2024 creation (SRD 5.2)

What a complete level-1 character needs, in the order a good builder asks:

1. **Class** — and what it gives at level 1: hit points, saving throws,
   armour and weapon training.
2. **Background** — +2 / +1 or +1 / +1 / +1 among its three abilities, its
   Origin feat, 2 skills, a tool, its equipment.
3. **Species** — traits, size, speed; its choices (Human: Skillful = 1 skill,
   Versatile = 1 Origin feat; Elf: Keen Senses = Insight, Perception or
   Survival; lineages = spells, Phase 4).
4. **Ability scores** — standard array (15 14 13 12 10 8) or 27-point buy
   (8–15), then the background's increases (20 at most).
5. **Class choices** — skill proficiencies from the class list (Fighter 2,
   Rogue 4, Bard / Ranger 3, the rest 2; a skill already given by background
   or species is replaced by another), Fighting Style (Fighter), Weapon
   Mastery (Fighter 3, Barbarian / Paladin / Ranger / Rogue 2), Expertise
   (Rogue 2), spells (casters, Phase 4).
6. **Equipment** — the class's kit and the background's kit.
7. **Looks and name** — model, portrait, voice, name.

Flavour with no rule behind it in nwn5e, dropped: alignment, languages,
personality, deity, age.

## 8. The plan: NWN's screens as the 2024 creation

NWN's page order is fixed (§1), so the 2024 steps are placed on the pages
that can carry them. After the change:

| # | NWN page | Becomes | What it carries |
|---|---|---|---|
| 1 | Gender | Gender | the model (kept) |
| 2 | Race | **Species** | 2024 names and texts (Half-Orc plays as Orc, Half-Elf legacy); no ability changes (built) |
| 3 | Portrait | Portrait | kept |
| 4 | Class | Class | kept |
| — | Alignment | **removed** | skipped by a patch; everyone True Neutral |
| 5 | Abilities | **Ability Scores** | 27-point buy to 15, Recommended = standard array; primary floor 8 (fix) |
| 6 | Packages | **Background** | each class's list = the 4 backgrounds; a package = the background's skills, its default split, the class's default picks and both kits; OK takes the defaults, Customize goes on |
| 7 | Skills (Customize) | **Skills** | the class's picks only (1 rank = proficient); background and species skills already set and locked; 0 points at later levels |
| 8 | Feats (Customize) | **Feats** | the background's ability split (its 7, as a popup), a Human's Origin feat, the Fighter's Fighting Style, Weapon Masteries |
| — | Domains, School, Familiar | **removed** in faithful | 2024: the subclass comes at 3; familiars are a spell |
| 9 | Spells | Spells | casters, with Phase 4 |
| 10 | Appearance | Appearance | kept |
| 11 | Info | Name | kept (deity / age / description stay optional) |

When the game starts nwn5e applies what follows from those choices with no
window: the increases (ability sync), the Origin feat, the tool, the class
features (built). Everything is stored the engine's way — feats and skill
ranks — so it travels with saves and exports.

### Phases

| Phase | Work | Levers | Cost |
|---|---|---|---|
| C0 | Client hooks by address: found by anchors (unique strings, vtable slots), skipped and logged if a game update moves them; creation vs level-up detection (the exported CGuiModalPanel::Activate / CGuiPanel::Deactivate + the panels' vtables) | hooks | moderate |
| C1 | Alignment skipped; Domains / School / Familiar off in faithful (classes PickDomains, PickSchool, MinAssociateLevel in memory); primary floor 8; Species texts; page captions (Background, Ability Scores...) | patch, memory, strings, `.gui` | cheap |
| C2 | Background packages: 12 classes × 4 rows, generated with their feat, skill and equipment tables (2024 class kit + background kit as NWN items); NWN's player packages hidden (kept for henchmen) | data | moderate (data volume) |
| C3 | The Feats page's rules: at level 1 the general pick = the chosen background's 7 splits (the package read from the creation character), a Human's second = an Origin feat; class picks: Fighter 1 style + 3 masteries, the other masters their masteries; one of each — the double-background bug fixed | hook CanChooseFeat | moderate |
| C4 | Weapon Mastery as feats (one per weapon kind with a mastery property); nwn5e reads them; `!mastery` swaps one after a long rest | data + rules_mastery | moderate |
| C5 | Skills page as the 2024 pick: points = the class's picks (+ Human Skillful), max rank 1, background skills pre-set and locked, 0 points after level 1; nwn5e's proficiency read from the ranks (`!skills` stays for changes) | memory switched per screen + page-init hook + rules_skills | moderate |
| C6 | Game start: Origin feats granted, tool recorded, older characters' records (local variables) moved to feats / ranks, the chapter-export check | server hooks | cheap |
| C7 | Level-ups the same way: ASI as a feat with its splits (popup) instead of `!asi`, the Fighter's style at 7 and masteries at 4 / 10 / 16, 0 skill points | hook + data | moderate |

C1 and C2 are visible on the screens at once; C3 is the core (and fixes the
bug); C4–C6 complete level 1; C7 carries it into level-ups.

### Decisions for the owner

1. Alignment removed → every character True Neutral. Campaign dialogs that
   check good / evil see neutral (dialog shifts still move it).
2. Cleric domains, Wizard school, familiars and animal companions off in
   faithful.
3. The Packages page becomes Background; NWN's own packages stay for
   henchmen.
4. Weapon masteries chosen at creation (feats), instead of filled by use.
5. Equipment: the class's kit + the background's kit, mapped to NWN items
   (no gold alternative).
6. No rolled scores (point buy and standard array only).
