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

## 9. Every choice the SRD asks for, and where it goes

Owner (2026-10-21): "make sure the plan accounts for edge cases where extra
screens are required (maybe warlock picking a patron?)". In 2024 the patron
is the Warlock's **level-3 subclass**, and SRD 5.2 has one per class (the
Fiend for the Warlock), so subclasses are granted, not chosen — a choice
appears only if the private layer adds more (then: a choice group at level
3, below). The Warlock's real level-1 extra is its first Eldritch Invocation.

### 9.1 The mechanisms (no new screens)

The edge cases fall into five mechanisms on pages the plan already uses:

- **Choice groups on the Feats page.** Any "pick one (or N) of a list" is a
  master feat (the group) with its options as sub-feats (the popup, as
  Weapon Focus). A table in nwn5e (`rules::`, unit-tested) says who gets how
  many picks of which group at which level (class level, species, an Origin
  feat); the Feats page's pick counts come from it and the CanChooseFeat hook
  (C3) lets each pick take only its group, one option each, with the group's
  own conditions (e.g. Expertise only in a proficient skill). The Background
  page's packages list a default for every group, so "OK" fills them too.
- **Skill picks on the Skills page**, counted by the same table per class,
  level and species (creation and level-up), 1 rank = proficient.
- **The Spells page** for spells and cantrips (Phase 4).
- **The Familiar page**, kept only for a Warlock with Pact of the Chain.
- **"Skills again"**: a choice made on the Feats page that adds skill picks
  (Skilled) re-opens the Skills page for those picks — the Configure chain's
  next-page helper (0x9c69a0) is a small state machine nwn5e can step back
  into (C8).

### 9.2 Creation (level 1)

| Who | Choice | Mechanism |
|---|---|---|
| every class | class skills (Fighter 2, Rogue 4, Bard any 3, Ranger 3, the rest 2) | Skills page |
| every class | background, its ability split | Background page + the split group |
| Fighter | Fighting Style (1), Weapon Mastery (3) | groups |
| Barbarian, Paladin, Ranger, Rogue | Weapon Mastery (2) | group |
| Rogue | Expertise (2) | group (proficient skills only) |
| Cleric | Divine Order: Protector / Thaumaturge | group |
| Druid | Primal Order: Magician / Warden | group |
| Warlock | Eldritch Invocation (1; the Pacts are invocations) | group |
| Warlock + Pact of the Chain | the familiar's form | Familiar page (2024 forms mapped to NWN familiars) |
| Warlock + Pact of the Tome; every caster; Magic Initiate | cantrips and spells | Spells page (Phase 4) |
| Human | Skillful (1 skill), Versatile (1 Origin feat) | Skills page (+1) and a group |
| Elf | lineage (Drow / High / Wood) × spellcasting ability; Keen Senses (Insight / Perception / Survival) | groups (one pick each) |
| Gnome | lineage (Forest / Rock) × ability | group |
| Dragonborn | ancestry (10 dragons) | group (with the new-species work) |
| Goliath | giant ancestry (6) | group (new species) |
| Tiefling | legacy (Abyssal / Chthonic / Infernal) × ability | group (new species) |
| Dwarf, Halfling, Orc | — | — |
| Magic Initiate (Origin feat) | spell list (Acolyte: Cleric, Sage: Wizard; a Human's: any of 3) × ability | group; its spells Phase 4 |
| Skilled (Origin feat) | 3 skills | "Skills again" |

### 9.3 Level-ups

| When | Choice | Mechanism |
|---|---|---|
| 3 (every class) | subclass: granted (one in the SRD); a group if the private layer adds more | — / group |
| 4, 8, 12, 16, 19 (+ Fighter 6 / 14, Rogue 10) | Ability Score Improvement (split popup) or another feat (CM8); Epic Boon at 19 | groups |
| Fighter 4 / 10 / 16; Barbarian 4 / 10 | one more Weapon Mastery | group |
| Champion 7 | a second Fighting Style | group (styles only) |
| Paladin 2, Ranger 2 | Fighting Style, or Blessed / Druidic Warrior (2 cantrips: Phase 4) | group |
| Bard 2 / 9, Rogue 6, Ranger 2 / 9, Wizard 2 (Scholar: its 6 skills) | Expertise | group |
| Barbarian 3 (Primal Knowledge), Bard 3 (Lore: 3) | skills | Skills page (counted by the table) |
| multiclassing into Bard, Ranger or Rogue | 1 skill | Skills page |
| Sorcerer 2 / 10 / 17 | Metamagic (2 each) | group (effects Phase 4) |
| Warlock 2, 5, 7, 9, 12, 15, 18 | more invocations (and one swap per level) | group (+ swap) |
| Hunter 3 / 7 | Hunter's Prey, Defensive Tactics | groups |
| Land Druid 3 | the land (arid / polar / temperate / tropical) | group |
| Cleric 7, Druid 7 | Blessed Strikes, Elemental Fury | groups |
| Draconic Sorcerer 6 | Elemental Affinity (damage type) | group |
| every caster | new spells | Spells page (Phase 4) |

### 9.4 Changed outside a level-up

2024 lets some choices change on a long rest (a Weapon Mastery, the Land
Druid's land, the Hunter's choices) or swap at a level-up (a Fighting Style,
an invocation): the level-up's Feats page offers the swap where it falls on a
level-up; long-rest changes use chat commands (`!mastery` exists), later a
window.

### 9.5 Order edge cases

- Background before Skills: its skills are pre-set, so a class pick can't
  repeat them.
- Expertise after Skills: only skills just made proficient count.
- Skilled after Skills: "Skills again" (C8).
- An invocation needing a cantrip, chosen before the Spells page: allowed,
  checked when the game starts and re-asked if unmet.
- Pact of the Chain → the Familiar page follows the Feats page (as NWN's
  chain already does).

### 9.6 Dropped as flavour

Tools and instruments (beyond what NWN skills use: Thieves' Tools), languages
(Thieves' Cant, Druidic, Deft Explorer's), size choices (the model decides),
deity, age, alignment.

### 9.7 Phases, revised

C3 becomes the **choice-group table** + the CanChooseFeat hook (+ client
CalcNumberFeats for the counts) — the Fighter's groups first, then the other
classes as their briefs are written; **C3b** species groups (Elf, Gnome;
Dragonborn / Goliath / Tiefling with their models); **C8** "Skills again";
**C9** the Familiar page for Pact of the Chain (with the Warlock, CM11);
spell choices stay Phase 4's (the Spells page).

## 10. Rethink: nwn5e's UI system (owner, 2026-10-21)

"This basically requires a rethink of how to customize the character creation
and level up screens ... we should build a UI system on top of what exists.
This is also an experiment to see how far one can customize an existing game
using these tools, so I want to do things right — as if the game had been
designed for this, not merely an adaptation."

### Design principles

1. **One visual language.** New steps look and behave like the game's own
   (fonts, frames, buttons, the hub's step list with its done marks, the
   description pane, Recommended), not like a bolted-on window.
2. **New steps are real steps.** They sit in the creation hub and the
   level-up sequence as their own pages (Back / Next / Cancel, Recommended,
   done state), not squeezed into an existing page under the wrong name.
3. **Data-driven.** Steps, choice groups, options, texts and layouts are data
   (2da / JSON / .gui / strings); code provides mechanisms, not content.
4. **The engine's own storage.** Results are feats, skill ranks and
   character fields — saved, exported and validated like NWN's own.
5. **One system everywhere.** The same pages and choice groups serve
   creation, level-up and in-game changes (rest swaps).
6. **Localisable.** Every visible text is a string reference.
7. **Robust.** Every hook is found by anchors and checked; if a game update
   breaks one, that step falls back (chat command) and says so — never a
   crash or a half-made character.
8. **Limits are findings.** Where the engine can't be extended cleanly, the
   limit is documented with its evidence, not hidden by a hack.

### The two candidate technologies (to settle by a spike)

- **A. Classic panels extended:** a new hub step ("Background") as a real
  chargen page — its layout a served `.gui` (data), its panel built from the
  engine's own GUI classes (CGuiPanel / CGuiModalPanel and the button and
  list controls are exported; the chargen-styled controls are not), wired into
  the hub's step list and exit switch. Matches the look exactly; the hardest.
- **B. EE's client NUI:** EE's own windows (Options, dialogs) are client-side
  `Nui::Window` / `Nui::JSON::JsonWindow` objects (RTTI present on Windows:
  `.?AVJsonWindow@JSON@Nui@@`, `.?AVWindow@Nui@@`); nwn5e would create one
  from its JSON at any time (menu, creation, level-up) and read its events
  before they would go to a server. Flexible and data-driven; its look is
  EE's NUI skin, not the classic chargen frames.
- Either way the flow layer is the same: the hub's exit switch and the
  Customize chain's next-page helper (§1) decide when a step opens.

Spike plan: B1 a client NUI window on the main menu (no game), its clicks
and binds in the DLL; B2 the same inside the creation screen, the native
chain waiting for it; A1 a served `cg_*.gui` layout as a new panel opened
from the hub; A2 a new step button in the hub (cg_main) with a done mark.
Screenshots of each, compared with the native pages; then the choice.

### Spike results

*C0 (f285b7f):* anchors for code nothing exports — a function by the one
`lea` of its own debug string, a vtable by its RTTI name, a function by a
unique byte pattern, hooks by address. Checked at launch against the
research: CanChooseFeat 0x7ed040, the creation hub 0xe64678, the feats page
0xe7b040.

*B1 (0.5.6, 2026-10-21):* nwn5e opens EE's own client NUI windows from its
JSON with no game loaded. The client's NUI message handler (anchored as the
one function with a `lea` of the `enable_make<JsonWindow>` vtable, checked by
its three ReadJSON calls: rva 0x7a5f20) builds the window from a message
nwn5e writes with the engine's own CNWMessage writers (layouts and binds as
UBJSON, `data/ubjson.h`, unit-tested); events are taken at
JsonWindow::QueueEvent (pattern, checked against OnOpened: 0x746130) and
handled a frame later from a CGuiMan::UpdateAndRender pre-hook. No engine
object is built or freed by nwn5e. Seen at launch (screenshots): the window
on the main menu, its text changed by a bind 3 s later, its "open" event
back in the DLL. **The look:** EE's NUI skin draws with the game's own font,
gold frames and button style — nwn5e's window reads as part of the main
menu, not as an overlay. Clicks verified by the owner (13:46, 0.5.6): Background,
Species, Class, Background, Close — each click (with its mousedown / mouseup)
reached nwn5e, the window's text followed, Close closed it. **B1 passed.**

*Decided (owner, 2026-10-21, after B2):* "the issue here is extending and
changing the actual windows the game uses for character creation, not
creating new ones on the side that just look like add-ons." **Route A is the
target:** new steps are real pages of the classic creation UI (the hub's step
list, the page frames, lists, description panes and buttons), and existing
pages change from the inside. The client NUI windows (B1 / B2) stay as a
tool, off by default (`client_nui_test`), for in-game windows if wanted;
they are not used for creation or level-up. Next: map the classic GUI system
(page construction through the engine's own allocator, layouts, control
binding, the hub's steps) — then A1 (an existing page changed from the
inside) and A2 (a new page + hub step).

*A2 (0.6.0, 2026-10-21): the Background step, a real creation page.* The
classic GUI mapped (scratch notes "classic_gui"): a page is a CGuiPanel whose
controls are members; its init (vtable slot 12) binds them to the layout with
ImportLayoutData and writes their {object, function} callbacks; OK / Cancel
end with Deactivate(code) + the page deleting itself. nwn5e (`chargen_ui.cpp`,
ability_scores faithful): the hub's Alignment step becomes **Background** —
served `cg_main.gui` (the button's caption) and `cg_background.gui` (the page:
the game's frame, list, help box, Recommended / OK / Cancel); when the hub
opens the Alignment page it is rebuilt in place as a Class page (the engine's
own constructor, through the engine's own `operator new` for its list items)
with nwn5e's vtable copy whose init binds nwn5e's handlers. The page lists the
four SRD backgrounds with their descriptions; OK asks for the ability split
in the same page (Recommended: the class's background and its two best
abilities, `rules::RecommendedBackground2024`, unit-tested); OK sets neutral
alignment (the Alignment page's own write), keeps the background feat for the
new character (given when it enters; the feat is the record) and exits with
the Alignment page's code — the hub's own logic marks the step done and moves
on to Ability Scores. The background feats left the feat picks
(ALLCLASSESCANUSE 0); the Fighter package defaults went. All six anchors
resolved at launch to the research's addresses (new 0xC10324, Class ctor
0x8DBE70, list item 0x8DCBC0, GetCreature 0x800C90, Class Cancel 0x8E10C0,
Class OK 0x8E36D0); if one is missing nothing is hooked and the layouts
aren't served (the game's Alignment step stays). Layouts are built from JSON
(`data/gui/src`) with `tools/gff/gffjson.py`.

*A2b (0.6.1, 2026-10-21): the split on the game's Ability Scores page.* The
split list was clunky (owner, A2 test). Background's OK now opens a second
page: the game's own Ability Scores page (`CCharacterAbilitiesPanel`, built
as the hub builds it — the engine's `operator new` + its constructor, found
as the call after `new` in the hub's AbilitiesButton handler and checked by
the vtable it writes) with nwn5e's vtable copy (init `IncreasesInit`) and
layout `cg_bgincrease.gui` (the game's `cg_attributes.gui`, the title and
two captions changed: "Increases Left", "Highest Increase"). Each row keeps
its arrows (CGuiTimerButton, callback at +0xD0): the background's three
abilities take +0 / +1 / +2, the other three are greyed and show "-"; the
arrows allow +2 / +1 or +1 / +1 / +1 (three increases, none above +2), OK
lights when all three are spent; Recommended fills the class's split. The
Background page exits with 0x8100 (unknown to the hub: it only refreshes
the sheet), the increases page inherits its exit delegate and exits with the
Alignment page's 0x8001; its Cancel is the game's own (back to the hub,
the step not done). Anchors at launch: Abilities ctor 0x8DB730, Abilities
Cancel 0x8E1000.
0.6.2 (owner's A2b test): the title sat off its place and OK looked lit
while dead. The game's own inits set the title's flag 4 before importing it
(the label then takes its text box from the layout's Obj_Label_Width /
Height; without it the box has no size), and grey a disabled text button's
caption themselves (SetColor 0.5 on button + 0xD0 next to SetEnabled(0)).
Both pages now do the same (`ImportTitle`, `EnableTextButton`). **Accepted by the owner
(2026-10-21, 0.6.2): the Background step (A2 + A2b) is done.**

*C1 (0.7.0, 2026-10-21): the small 2024 changes.* (1) Point buy: the
class's main ability can go down to 8 like the others
(CHARGEN_BASE_ABILITY_MIN_PRIMARY answered 8; NWN's is 11), ability_scores
faithful. (2) The hub's Abilities button reads Ability Scores (string 143
answered by the label hook, ability_scores). (3) Species, a new key
`species` (CM6's frame): Race reads Species (148, the Race page's title
162 and help 481 / 485), the Half-Orc is named Orc (32 / 33 / 4922), and
each species' description (racialtypes Description 251-257) lists its SRD
5.2 traits and what nwn5e does with them so far (the Half-Elf: legacy,
SRD 5.1) — `rules::CreationStringSwap2024`, strings 16000260-16000280.
(4) No School step (class_features faithful): the Customize chain's
next-page function (0x9C69A0: anchored as the call the Feats and School
pages' OK handlers share, checked by its PickSchool test) runs with every
class's PickSchool cleared for that one call — the flag stays set
everywhere else, because the server reads it too (SaveClassInfo writes a
character's School only when it is set), so existing specialists keep
their school and NWN mode is untouched. A wizard is a generalist (school
0), at creation and when multiclassing in. The packages path still applies
a specialist package's school (C2 replaces the packages). Also fixed: six
strings in data/strings.txt had raw line breaks (the four background
descriptions and the Background page's two helps showed their first line
only); a unit test now reads the file (one string per line, ids in range,
no repeats, every swap's string present). **Verified in game (2026-10-21,
0.7.0):** the owner checked the hub captions, the background descriptions
and the character sheet; the logs show the species texts, the main-ability
floor answered (11 -> 8), a Wizard through Customize with no School step,
and that Wizard entering with its background (Acolyte, Int 17 = 15 + 2, Wis
13 = 12 + 1) and saving cleanly.

*C3 (0.8.0, 2026-10-21): the Feats page's rules.* Since A2 the background
and its split are chosen on their own page, so C3 is what the Feats page
offers. (1) Which feats exist (CM8, new key `feats`): the client's
CanChooseFeat (0x7ED040, anchored by its own "Invalid Feat" message) says
no to every feat but the 2024 ones nwn5e has built
(`rules::IsChoosableFeat2024`: Alert = NWN's Improved Initiative, Ability
Score Improvement, the four Fighting Styles) — the Feats page and the
packages' default picks both ask it, so NWN's ~210 feats leave the
screens; characters keep the ones they have. (2) When feats come
(feat_cadence): no general feat at level 1 — every race's
NumberNormalFeatsEveryNthLevel (+0x5C) is 0 in memory — because the
Origin feat comes with the background; a human keeps
ExtraFeatsAtFirstLevel (+0x48), 2024's Versatile (an Origin feat: Alert
until the others are built). A Fighter's level-1 bonus pick is its style
(F1's slot; ASI waits for 4). (3) Most level-1 characters now have no feat
to pick, so Skills ends the Customize chain itself — and the hub ignores
Skills' exit code (0x8010), marking Packages done only on the Feats
page's (0x800E): the next-page hook hands the hub 0x800E then. The race
edit waits for that hook (no hook, no edit). (4) The packages: their
default picks take a table's first choosable feats and stop at its end,
so the 64 base-class tables get ASI, the Fighter's styles (Defense first
for Strength packages, Archery for Dexterity ones) and Alert appended
(data/rows/packft*.txt, `tools/data/packft_rows.py`); NWN mode reaches
them only after its own. Anchors at launch: CanChooseFeat 0x7ED040.
*Not yet:* the other Origin feats (Savage Attacker, Skilled; Magic
Initiate with Phase 4) — a human's Versatile offers Alert alone, and a
human Criminal (Alert from the background) gets nothing more from it;
Origin feats in the level-4+ slots (2024 allows them: Alert isn't in the
bonus lists yet); Weapon Mastery picks (C4); henchmen's automatic
level-ups (the server's own CanChooseFeat) still take NWN feats. *(Fixed in 0.15.1.)*
**Verified in game (owner, 2026-10-21, 0.8.0):** "everything seems good";
the logs show non-human Wizards and a Cleric finishing Customize with no
Feats page (the Skills page closing the chain as the Feats page), the
filter in use, and a Soldier Fighter entering (Str 17, Con 15) and
levelling to 4 (Champion at 3, Second Wind 3 uses at 4).

*C2 (0.9.0, 2026-10-21): the Packages step as the 2024 starting kits.*
Since the background has its own page, a package is the class's kit.
Decided (owner, 2026-10-21: "go with the recommendations"): one 2024
package per class — the Fighter two, SRD option A (Chainmail, Greatsword,
Light Flail, Throwing Axes) and B (Studded Leather, Scimitar, Short Sword,
Longbow, Arrows) — NWN's themed packages hidden in 2024 mode (kept for
henchmen and NWN mode); kit only, no gold option; items with no NWN
counterpart left out (packs become a Torch; instruments, holy symbols,
foci, spellbook, books dropped; javelins become Throwing Axes); no NWN
starting healing potions (the 2024 kits have none); the background's kit
(rules::BackgroundKit2024) and gold given when the character enters, with
the background. New key `starting_equipment`. The data (tools/data/kits.py):
12 equipment tables served whole (data/files/packeq5e_*.2da), packages.2da
rows 140-151 (their feat preferences: the class's, the greatsword Fighter's
with Great Weapon Fighting first, the archer's with Archery), NWN's 69
base-class player packages set PlayerClass 0, and classes.2da's Package
(Recommended) pointing at the 2024 one — all as `.if_kits` rows, which the
loader serves only when the rule asks (new: conditional rows), so NWN mode's
tables stay NWN's. Skill preferences wait for C5.
**To revisit (owner, 2026-10-21): the starting items, for game balance** —
a package item comes as its blueprint's stack (50 Throwing Axes for 8
javelins, 99 Arrows for 20), the 2024 gold is far below NWN's (4-28 gold
against 20-100), and NWN's starting healing potions are gone.
0.9.1 (owner's C2 test: a Sage Wizard had two robes, two quarterstaves and
two daggers): the background's kit skips an item the character already
carries or wears (by blueprint), and the kits give one dagger or handaxe
where the SRD gives several (spares to throw; the game can't throw them) —
the Barbarian's 4 handaxes, the Monk's 5 daggers, everyone's 2, the
Criminal's 2. **Verified in game (owner, 2026-10-21, 0.9.1):** "everything
seems good"; the log shows a Sage Wizard entering with the Sage's kit given
by the script (quarterstaff and robe skipped as already had, 8 gold).

*C4 (0.10.0, 2026-10-21): Weapon Mastery as feats.* A "Weapon Mastery"
master feat (masterfeats row 44) with one sub-feat per weapon kind (feat
rows 1260-1299, the order of rules::NwnMasteryWeapons(), append only),
each needing one of its weapon's proficiency feats (OrReqFeat0-4, checked
by the engine), in the bonus lists of the five classes with Weapon
Mastery; generated by tools/data/mastery_feats.py, checked against the C++
weapon table by a unit test. The class bonus slots (feat_cadence) are
rules::BonusPicks2024: the feat levels + the Fighting Style levels + the
masteries a class level adds (Fighter 3 / +1 at 4, 10, 16; Barbarian 2 /
+1 at 4, 10; Paladin, Ranger, Rogue 2). NWN's bonus picks are one pool, so
the CanChooseFeat hook keeps each group to its count for the class level
being taken (the client stats' class slot +0x6A, its class +0x295 and new
level +0x296, stride 0x150; the chosen list): a level-1 Fighter takes 1
style + 3 masteries, a Fighter 4 an ASI (or a style) + 1 mastery. The
feats are the record: rules_mastery reads them (older characters' local
variables still count), and !mastery, its swap and the fill-by-use (kept
for henchmen and empty slots) write them. Recommended picks the kit's
weapons (data/rows/packft*.txt). The feats are hidden unless masteries are
picked on the screens (weapon_mastery and feats faithful). Calls made:
only weapons you're proficient with; multiclassing into a second mastery
class gives that class's picks too, and every mastery feat counts — more
than D3's slots (the most of any one class) for such a character; rare,
accepted for now (the slot table is per class).
0.11.0 (owner's C4 test): a halfling couldn't equip the spear or greatsword
its kit and masteries offered — NWN won't let a creature hold a weapon two
sizes larger. 2024 has no size limit (SRD 5.2: Heavy is Disadvantage below
Str 13 melee / Dex 13 ranged; 2014's Small rule is gone). Decided (owner,
2026-10-21): faithful — new key `size` (0.11.0 named it weapon_size): a weapon two or more sizes
larger counts as one size larger (two-handed) in the server's
GetRelativeWeaponSize and the client's copy (0x8012D0, anchored by its
whip check), and the Heavy rule is a Disadvantage source in the attack's
roll mode. Still NWN: a Small creature holds a Medium weapon (longsword)
two-handed; weapon models aren't scaled to the wielder. Also: `!mastery A
for B` with both already mastered now says so (the test's swap named two
weapons the Fighter already had — Longsword came from its level-4 pick).
0.12.0 (owner: "same issue with gnomes, check for other species /
equipment incompatibilities"): the equip refusal was CanEquipWeapon's own
inline size check (the hooked function is asked elsewhere). A sweep of every
engine function reading the creature's size (+0x718) found, besides it:
CanUseItem's (the inventory's unusable mark); the Tower Shield refused to
Tiny / Small in CanEquipItem, CanEquipShield and CanUseItem; the 3e size
modifiers to AC and attack (CREATURE_SIZE_*: Small +1 / +1, nwn5e had left
them); a Small creature's smaller unarmed dice (UNARMED_SDAMAGE_DIE, the
monk's *_SDAMAGE_*). All fixed in faithful (`size`: five one-byte patches,
all or none; the ruleset answered 0 / with the Medium values). Already
2024 elsewhere: Hide / Spot size terms (SK5), Knockdown / Disarm (SK6's
Shove), the two-weapon Light test (D1's 2024 list), two-handed Strength
(E1: once either way). Found on the way, not a size rule: NWN's Weapon
Finesse feat — which CM8 no longer offers — was the only way to attack with
Dexterity; new key `finesse`: the 2024 Finesse property (GetWeaponFinesse
hooked; damage with the better of Str and Dex, rules::FinesseDamageAdjustment,
unit-tested). Left: a Small creature holds a Medium weapon two-handed (NWN),
weapon models aren't scaled, the sheet's damage line shows Strength.
**Verified in game (owner, 2026-10-21, 0.12.0):** "everything looks good" —
gnomes and halflings equip Large weapons, the Small attack bonus answered 0,
Finesse damage on the owner's short sword (Dex +3 for Str +1) and on
creatures' daggers, the Small unarmed dice answered with the Medium ones.
Not yet seen in game: the Tower Shield on a Small character (nw_ashto001)
and a Small monk's unarmed hit (their patches / answers are in the log).

*0.13.0 (owner, 2026-10-21: "next go for missing origin feats and medium
weapons for small characters").* (1) The Origin feats Savage Attacker and
Skilled (feat rows 1300 / 1301, data/rows/feat.origin.txt,
rules_origin_feats.cpp): Savage Attacker — once per turn (the creature's
combat-round count), a weapon hit's damage dice are rolled twice and the
higher roll kept, through the weapon-dice filter Great Weapon Fighting
uses (rules_fighter.cpp: its GetDamageRoll hook now installs in either
mode of class_features); the Soldier background counts as having it.
Skilled — rules::SkillAllowance::feat_skills, three picks of any skill
for !skills (unit-tested). Both are general feats (a human's Versatile
pick); Recommended takes Savage Attacker for the martial classes, Skilled
for Rogues and Bards, Alert otherwise (packft rows). Magic Initiate waits
for spells. (2) A Small creature wields weapons as a Medium one (`size`):
the server's and the client's GetRelativeWeaponSize answer with Medium for
it, and where the engine computes the difference inline the creature's
size reads Medium for the length of the call (CanEquipWeapon, wrapped;
InitializeNumberOfAttacks, in rules_two_weapon.cpp's hook) — a halfling's
longsword is one-handed (a shield or an off-hand weapon with it), its
greatsword two-handed, its dagger light; Great Weapon Fighting's "two
hands" test uses the same size. Shove, size modifiers and everything else
keep the creature's own size.

*Seen in game (owner, 2026-10-21, with 0.13.1's `!test`: a halfling
Fighter 5 made from a level-1 character).* `!test` applied the profile
twice (species, 4 levels through the engine's level-ups, scores, feats,
items equipped). S2: the Tower Shield equips on a Small character. O6: the
halfling's longsword is "equipped as a one-handed weapon" (the off-hand
dagger not tried). O7: the greatsword fights two-handed (Great Weapon
Fighting applies). O3: Savage Attacker once per turn (the second hit of a
round without it). O1, O2, O4, O5 not yet seen.

*0.13.2 (owner, 2026-10-21: "make it faithful").* Savage Attacker on a
critical hit: the hit's dice — the weapon's dice once per multiplier,
GetDamageRoll's critical loop — are one roll, rolled twice whole and the
higher total kept (rules::KeepSecondSavageRoll, unit-tested); 0.13.0 rolled
each set twice and kept the better of each, a little more than the rule
gives. The filter rolls every set at the hit's first weapon-dice ask
(GetCriticalHitMultiplier through its export, so rules_crit's answer
counts) and hands them out in order.

*Suspected, not confirmed (2026-10-21): EE's Easy difficulty maximises a
character's weapon dice.* GetDamageRoll passes each weapon-dice roll through
CNWRules::GetWeightedDamageAmount with diffsettings.2da's
MinPCDamagePercent (DMEasy 100, Easy 50, Normal and up none) when a PC hits
a non-PC: the roll becomes min(maximum, roll + maximum x (p..99)%), so on
Easy a greatsword deals 12 nearly every hit (the test fight: 15, 15 and 27
= 12 + 12 + 3). Its twin, MaxNPCDamagePercent, scales what monsters deal to
characters (Easy 50). Dice rules (Great Weapon Fighting, Savage Attacker)
only show at Normal or harder. Not answered by nwn5e (crits_against_players
already answers NoCriticalOnPC from the same table); a rule key for it is
an open question. *2026-10-22, against it:* still at Easy, the same
character's longsword hits on Prelude's mages dealt exactly the kept dice
+ Strength (Savage Attacker 5 -> 8, 8 -> 11, 2 -> 5; a 2d8 critical 12 ->
15) — no weighting, though the code path reads the same for any non-PC
target. Not yet known whether the combat dummy or the greatsword (with
Great Weapon Fighting) made the difference; next: the greatsword against
an ordinary enemy, ideally with combat_breakdown=1. *Then, at Normal
(difficulty 2) with combat_breakdown=1:* the greatsword with Great Weapon
Fighting and Savage Attacker dealt exactly nwn5e's roll + Strength — the
engine's breakdown "9 + 3", "8 + 3", "7 + 3"; a critical hit 13 (4d6) + 3 =
16, its breakdown text printing "6 (Strength Bonus) (Critical x2)" though
crit_dice_only applied Strength once (the debug text asks the multiplier
from its own call site). So nwn5e's weapon dice reach the damage unchanged,
and the dummy fight's raise happened in the engine after nwn5e's roll
(GetWeightedDamageAmount is the only step between; its percent defaults to
100 and is replaced by MinPCDamagePercent only for a PC hitting a non-PC
creature). Why the dummy got it at Easy and the mages didn't is not known;
left as an engine quirk — test at Normal.

*0.13.3 (owner, 2026-10-22: O1 "I think Alert is being shown as Improved
Initiative"; O2, O4 correct).* Feat 377 kept NWN's name and +4 description
though nwn5e plays it as Alert (rules_initiative.cpp: the proficiency
bonus instead). With `initiative` faithful it is now served as Alert
(data/rows/feat.if_alert.txt, strings 16000394-16000395: the SRD 5.2 text,
Initiative Swap noted as not built); NWN mode keeps Improved Initiative.
*Verified in game (owner, 2026-10-22: "all good"):* O1 (Alert by its name)
and O5 (Skilled's three picks in !skills, set up with !test) — with O2, O3,
O4, O6, O7 and S2 before, all of 0.13's rows. Still unseen: the off-hand
dagger with a halfling's longsword (O6) and a Small monk's unarmed hit (S4).

*C5 (0.14.0, owner 2026-10-22: "go with the recommendations").* The Skills
page as the 2024 pick, with `skills` faithful. Decided: (1) the seven 2024
skills NWN has no row for become skills.2da rows 28-34 (Athletics, History,
Insight, Investigation, Nature, Religion, Survival; EE keeps a creature's
ranks in an array sized by the table — GetSkillRank bounds-checks against
CNWRules +0x107 — so rows can be added); they exist in both modes so saves
load either way, unusable and off the page in NWN mode (on the character
sheet there with rank 0). (2) One row per 2024 skill on the page
(rules::SkillsPageRow: Stealth = Hide, Perception = Spot, Arcana = Lore,
Animal Handling = Ride — Animal Empathy is only a Druid's / Ranger's), the
skill's other rows hidden from it (HideFromLevelUp, read only by the
page's list builder 0x9c4fd0, not by the sheet). (3) Rows without a 2024
skill and the tool rows hidden from the page. (4) The 2024 rules decide each
pick: points = the class's count + 1 for a Human (Skillful, any) or an Elf
(Keen Senses: Insight, Perception, Survival); every pick 1 point, rank 1;
rules::CanPickOnSkillsPage (CheckSkillChoice on a page-only allowance)
accepts a pick only if the picks still fit; the background's skills set to
rank 1 and locked. (5) No skill points after level 1; Skilled (its Feats
page comes after this one), Expertise, multiclass and level-3 picks stay
with !skills until C7. (6) The page's ranks are the record: rules_skills
reads a new character's proficiencies from them.
How (src/core/chargen_skills.cpp; the client's CNWCLevelUpStats functions,
each found by one instruction unique in .text): the level's skill points
(0x7ec0c0: (class base + ability, at least 1) x race multiplier + extra at
level 1, else base + ability + extra — called by the level-up reset 0x7edcc0,
the page's constructor 0x9c3040 before it copies the starting ranks into the
page's floor array +0xf88, and Recommended 0x9c9a30); "+" (0x7f7a00: class
skill 1 point, cross-class 2; max rank level + CHARGEN_SKILL_MAX_LEVEL_1_BONUS,
halved cross-class); "-" (0x7ee3e0: down to the page's floor, 1 or 2 points
back); the package's picks (0x7eb300, Recommended and the hub's OK without
Customize). Client stats: race uint16 +0x50, ranks int8* +0xd8 (set through
0x7fa8b0, which refreshes the totals), points uint16 +0x10e. Recommended =
rules::RecommendedSkillsPagePicks (each class's usual skills, then common
ones for the species pick; unit-tested over every class x species x
background). Data (tools/data/class_skills.py): skills.added.txt (the rows),
skills.if_skills2024*.txt (usable, page rows shown), cls_sk5e_*.2da (the
2024 class lists as class skills, NWN's usability and its flags for rows
without a 2024 skill), classes.if_skills2024.txt; the `skills2024` rows are
served only when all four hooks are in. tests/data/class_skills_test.cpp
checks the tables against the rules. Not built: Performance on the page for a
non-Bard (Perform is a Bard's only, AllClassesCanUse 0 — !skills for it).

*0.15.1 (owner 2026-10-22: "Go for the henchman issue").* Henchmen's
automatic level-ups take the 2024 feats: the server's CanChooseFeat
(0x3F1830), whose one caller is CNWSCreatureStats::LevelUpAutomatic
(henchmen, the script's LevelUpHenchman, !test), says no to the others with
`feats` faithful (rules_feats.cpp; it was !test's alone). A package whose
feat table has no 2024 row leaves that pick empty; a henchman's ability
increases come from its feat levels anyway (CM12). Not checked server-side:
the kinds of bonus pick per level (C3's client filter) — a package's order
(Ability Score Improvement, styles, masteries) decides.

*C7 (0.16.0, owner 2026-10-22: "Go for it", after "Are your recommendations
the closest to faithful?").* Level-ups, as SRD 5.2 has them.
**C7a — the feat levels.** Ability Score Improvement (General feat, Level 4+,
Repeatable: +2 to one score or +1 to two, none above 20) is taken on the
Feats page with its choice: the k-th ASI is a master feat (masterfeats rows
45-51) whose 21 sub-feats are its choices (feat rows 1400 + 21 (k - 1) +
split; rules::AsiSplit / AsiFeatFromId; tools/data/asi_feats.py). nwn5e's
feat filter offers only the next ASI's choices (the client stats' feats,
+0x118 / count +0x120) and none that changes nothing (its abilities already
20; the client stats' scores +0x70); the server's check (henchmen) keeps the
same order. The ability sync reads the choices from the feats — exports keep
them. Older characters' ASIs (rows 1200-1206, abilities from !asi) still
count; !asi lists every ASI and `!asi change str [con]` swaps the last one
(the feat, or the older record). "Another feat for which you qualify": the
Origin feats (Alert, Savage Attacker, Skilled — Repeatable, rows 1302-1307,
each needing the one before) in the classes' bonus lists
(cls_feat_*.if_feats2024.txt, served with feats faithful) and the Fighting
Style feats for a Fighter (already allowed in the general pick); an Origin
feat counts against the general pick (rules::FitsBonusPicks; a Human's
Versatile pick at level 1 is its own). Recommended takes the package's main
ability first (+2 main, +1 main / +1 second, +2 second, +1 second / +1
third; tools/data/packft_rows.py). `!style <old> for <new>`: "whenever you
gain a Fighter level, you can replace the feat you chose with a different
Fighting Style feat" — once per Fighter level gained (record
nwn5e_style_swap_level).
**C7b — the Skills page at level-ups** (chargen_skills.cpp, C5's hooks): a
level's floor is the ranks it starts from (taken when the character, level,
class, race or background changes — the game asks for the points more than
once per level, Recommended before its own reset); rank 0 -> 1 above it is
a new skill, 1 -> 2 Expertise. Points = rules::LevelSkillPicks2024:
multiclassing into a Bard (any), Ranger or Rogue (one of its list),
Barbarian 3 (Primal Knowledge: one of its list), Bard 3 (College of Lore:
three of any), plus Expertise — Rogue 1 (at creation too) and 6 (2), Bard 2
and 9 (2), Ranger 2 (1) and 9 (2), Wizard 2 (1 of its six: Scholar).
rules::SkillAllowanceFor now holds Primal Knowledge and College of Lore,
so the profile keeps them. A !skills profile takes what the page adds when
the ranks are 2024's (rules::RanksAre2024: only page rows, 1 or 2); a
character with NWN ranks gets no points after level 1 (!skills). Not built:
Grappler (the SRD's other General feat), Epic Boons at 19, Magic Initiate,
Paladin and Ranger Fighting Styles (their briefs), Skilled's picks on a page
(C8: its Feats page comes after the Skills page).

**C8 — "Skills again" (0.17.0, 2026-10-23; owner: "go for C8", then "Go with
recommendations").** Skilled is an Origin feat (rows 1301, repeats 1302-1307):
a Human's Versatile pick at level 1, any feat level after. Its three picks
now come on the Skills page, opened again after the Feats page:
- **The chain** (recon: the Skills / Feats pages' OK handlers and NextPage
  0x9c69a0, read in the disassembly). The Feats page's OK appends its feats,
  then NextPage(…, 0x800E), then deletes its page. nwn5e's NextPage hook
  (chargen_ui.cpp) asks chargen_skills.cpp first: when the Feats page added
  Skilled (the client stats' feats against a count taken when the level's
  first Skills page opened), a Skills page is built as the Skills page's OK
  builds a Feats page - operator new(0xF98), the page's constructor (the one
  function referencing its vtable), the Feats page deactivated with the
  Skills page's code (0x8010: the hub only refreshes its sheet, the level-up
  ignores it), the exit delegate copied, CGuiModalPanel::Activate, the
  level-up's page (+0x18 of GetLevelUp, the call the Skills OK makes) set to
  it - and NextPage returns. That page's OK is the game's: CalcNumberFeats
  answers 0 for it (else a Feats page again: a loop), and its
  NextPage(0x8010) goes on as 0x800E - Domain, Spells, the end - as the
  Feats page's would have; the hub marks the Builds step done on it.
- **The page**: one step, "Skilled: choose 3 skills of any kind" (6 when
  taken twice...), every skill the character didn't have listed; the level's
  floor is the ranks as the page opens, so "-", Reset and Recommended (the
  class's usual skills) touch only Skilled's picks. No points from the
  constructor (it adds the level's at a level-up); the level as before the
  page comes back when it closes.
- **Cancel** is the game's (owner's pick): creation - back to the hub, the
  Builds step undone; a level-up - abandoned.
- **Expertise** at the same level comes before Skilled (a level-1 Human
  Rogue): it can't go to a Skilled pick there; !skills can move it.
- **The server**: ValidateLevelUp prices each new rank in NWN skill points
  against NWN's budget for the level (banked +0xDC6 + class base + Int; a
  cross-class rank twice) and refuses more (66155, "Character applied
  invalid number of skillpoints") - Skilled's three often are (a Fighter
  with Int 8 at 4: 3 against 1). For a player character whose ranks are the
  2024 record the budget is topped up for the call (rules_skills.cpp); the
  page keeps the 2024 count, the other checks run. Creation isn't checked in
  single player (Enforce Legal Characters off).
- **Performance** is usable by every class with skills faithful
  (skills.if_skills2024.perform.txt: AllClassesCanUse 1) - 2024 has no
  Bard-only skill, and Skilled and Skillful take any skill (a non-Bard's is
  cross-class to the server: the top-up covers it).
- **Skilled without the page** (a build taken without Configure, whose
  Recommended feat is Skilled; the level-up's Recommended): a creature
  update fills its picks on the server as Recommended would - the class's
  usual skills, rank 1 (rules::FillSkillChoices, unit-tested) - and tells the
  player; skills chosen with !skills or NWN ranks are left alone. Henchmen
  need nothing: their 2024 skills are read from their ranks within their
  choices (Skilled counted).

*0.17.1 (owner's C8 tests, 2026-10-23: "S6 / Feats page has an empty list to
choose from", "S7 / No chat message").* The Origin feats (Alert, Savage
Attacker, Skilled and its repeats) sat in every class's feat list as List 2
(bonus only), and CNWClass::IsNormalFeat answers from a class's list when the
feat is in it (ALLCLASSESCANUSE only otherwise): a Human's Versatile pick -
the game's general (normal) pick at level 1 - could take none of them. A
Human Wizard (no bonus picks at 1) had an empty list (S6); a build taken
without Configure gave the Human no Origin feat at all, so no Skilled and
nothing to fill (S7). A Human Fighter's Origin feat took one of its four
bonus picks instead, leaving room for two of its three Weapon Masteries: the
game's CanChooseFeat (client 0x7ed040) fits a level's feats in two passes -
those one kind of pick only first (bonus ones while bonus picks last), then
the ones either may take. The Origin feats are List 1 now (a bonus or a
general pick; tools/data/asi_feats.py), so the general pick takes them and
the bonus picks stay the style's and the masteries'.

*The 2026-10-22 test matrix done (owner: "the !style works, i've tested it
before, and loading an !exported character also works"): !style swapped
Archery for Great Weapon Fighting at Fighter 4 (the new help); E3 - a
character exported with !export and taken into a new game had its 3 records
restored from its own database. C5-C7 and 0.16.x verified.*

*Part 4 passed (owner, 2026-10-22, 0.16.10: "Part 4 seems like it passes
fine"): an Elf Wizard (Sage) - Wizard skills then Keen Senses (3 picks, Arcana
and History the background's), Reset and OK between the steps; at Wizard 2
the Scholar Expertise step (Recommended: Arcana); !skills: proficient in
Arcana, History, Insight, Perception, Religion, Expertise in Arcana.*

*0.16.10 (owner's retest, 2026-10-22: "the style manual switch it always
says "You already have archery"").* The Fighter had Archery and Defense both
(a Fighting Style feat at 4), and !style's help always named "!style defense
for archery" - a swap between two styles held, refused each way. The help
now lists the styles a swap can go to and names one that works ("!style
defense for great weapon"); the refusal says a swap goes to a style you
don't have. The same retest passed: a Halfling Fighter's Skills page has no
species step (2 picks), the empty level-up Skills pages are skipped, the
refused classes say why, and a greyed skill shows its description.

*0.16.9 (owner's Part 3, 2026-10-22: "add the reason to the class
descriptions, also if there are no skills to pick, you can skip that
screen. if a player presses a skill that's greyed out due to having reached
the max increase they could the text should not say that they cannot learn
that skill").* (1) A class the level-up's Class page refuses says why in its
description: "Multiclassing (2024): you can't take a level in Monk now - it
needs Dexterity and Wisdom 13, and you have Dexterity 13, Wisdom 12" (or the
current class that fails; prestige classes: their 2024 place, the subclass
at 3). The GetIsClassAvailable hook keeps each refused class's reason (the
player's characters only, each round of checks afresh), rules_labels.cpp
appends it as classes.2da's Description string is looked up, and the Class
page's deleting destructor drops them. (2) A level-up whose Skills page has
nothing to pick (no step; an older character's level-up with no points) is
skipped: the page's OK pressed by a thread timer once the page is up. (3) A
greyed row's click (the list's select callback, 0x9cb730) showed
"Unavailable Skill / You may not learn this skill." for any disabled row;
on the stepped page every listed skill is one the step can take, so the
row's disabled bit is off for the call and the skill's description shows.

*Accepted (owner, 2026-10-22, after testing 0.16.8: "ok, I think it's
good"): the Skills page in steps - OK to move on, Recommended and Reset,
each step's texts (0.16.4-0.16.7) - and the Builds step (0.16.8).*

*0.16.8 (owner, 2026-10-22: "the name "Packages" it's weird" ... "go with
"Builds"").* With starting_equipment faithful the Packages step reads
Builds: the hub's button and the page's help title (146, 483) "Builds",
"Configure Packages" (159) "Configure Build", the title (160) "Select a
Build for your Character", and the page's help (487) explains a build as
the class's ready choices - its 2024 starting equipment and the skills and
feats Recommended picks, now and at level-ups - no special abilities.
Swapped as the strings are looked up (rules::CreationStringSwap2024, rule
kStartingEquipment), as Race reads Species. NWN mode keeps "Packages": its
own packages' descriptions all say "These packages ...".

*0.16.7 (owner, 2026-10-22: "For each step, have the text change to explain
what the user is doing. where the skill description is have the default
text change with each step").* The help box, until a skill is clicked,
explains the step: "Step 1 of 3: Rogue skills" with how many and from which
list (at a level-up why: multiclassing in, Primal Knowledge, College of
Lore's Bonus Proficiencies), what proficiency does, the background's skills
already had; Human Skillful / Elf Keen Senses; Expertise (the Wizard's
Scholar list); then what OK, Recommended, Reset and - do.

*0.16.6 (owner, 2026-10-22: "can we make it so the user needs to press "OK"
to move to the next step and the recommended button only applies to the
current choice and doesn't pick all skills at once?").* The page keeps its
own step: the first when it opens (any choices already there taken back),
the next only by OK, the first again by Reset. A step's choices are counted
by the step each was made in. OK is greyed until the step's choices are
made (or it can take no more); on a step before the last it shows the next
step's list, on the last it is the game's OK. The header ends "- OK for the
next step" / "- OK to finish" once the step is done. Recommended takes back
the step's choices and makes them again (the class's usual skills in order,
then any the step can take) - the game's own handler, every step at once,
now runs only on a page nwn5e leaves to the game. "-" takes back only a
choice of the step shown. Recommended, Reset and OK build the list again,
so every row's number is the rank now.

*0.16.5 (owner, 2026-10-22: "a "reset" button between the recommended and
OK buttons that resets the point allocation and returns to user to the
first step. Also, pressing the "recommended" button crashes the game").*
Recommended: the game's handler updates the list row of every skill not
hidden (HideFromLevelUp), finding it through the list's ids — with only a
step's rows listed, a skill outside the list has no row (index -1) and the
handler read past the list (crash at 0x9c9d67). The skills the list doesn't
hold are now hidden from it for the length of the call, as the list builder
already does. Reset: a fourth button between Recommended and OK, from a
served copy of the game's cg_skills.gui (data/gui/src/cg_skills.json ->
data/gui/skills/cg_skills.gui, served only when the Skills page's hooks are
in: ServeSkillsPageLayout). nwn5e makes the CGuiTextButton inside the
page's init (the list builder is reached only from there, layout open),
binds it to the layout's "ResetButton", and destroys it in a pre-hook of the
page's deleting destructor. A click takes back this level's picks (the
background's skills and earlier levels' ranks stay), returns their points
and shows the first step. On a page nwn5e leaves to the game (an older
character's level-up) the button is there but greyed.

*0.16.4 (owner, 2026-10-22: "skills chosen in the first step should not
appear in the second step. On the third step, no skills should be removed
they should only be greyed out if they cannot be added to ... Skill should
only be added or removed to the list between steps").* Each step's list is
fixed for the whole step (chargen_skills.cpp Candidate): the class step —
the class's skills the character didn't have; the species step — the skills
it can take, not the class step's picks (each pick remembers the step it was
taken in: a Human may take a class skill as its any-skill pick); the
Expertise step — every skill the character has that Expertise can go to.
Within a step a row only turns live or greyed; once every pick is made the
last step's list stays, all greyed. "-" works only within the step — one
that would take the page back a step (a skill pick while choosing
Expertise) is refused; Cancel starts the page over.

*0.16.3 (owner, 2026-10-22: "Unpickable feats just clutter the selection
process, pickable feats at their max level that aren't greyed out don't make
sense ... In each step only pickable feats are displayed and feats that
reach their maximum level get greyed out. There should also be text in the
window that explains what kind of points are being used").* The Skills page
lists only the current step's rows: the ones it can take (live) and this
level's picks (greyed — at their maximum for the step — and still there for
"-"). The game's own builder makes the list, the other rows hidden from it
(HideFromLevelUp) for the length of the call; when a pick changes the step's
rows the list is emptied (CGuiButtonListbox::DeleteButtons — the page's row
arrays are the list box's) and built again. The header line (once "Allocate
skill points.") names the step and what is left ("Rogue skills: choose 2
more of 4", "Human (Skillful): 1 skill of any kind: choose it", "Expertise:
choose 2 more of 2"); the points box shows the step's picks left; the help
box explains the steps and the background's skills when the list is new (a
click leaves the clicked skill's description).

*0.16.2 (owner's first C5-C7 test, 2026-10-22).* Fixes:
- **The background's skills at 0** on the Skills page (K1): the level-up
  reset (0x7edcc0) clears the ranks after the points call that set them, and
  the page's constructor skips its own points call; they are set again just
  before the page builds its rows (BuildSkillList 0x9c4fd0, hooked).
- **The page in steps** (owner: "Ideally these would be done each in their
  step (Rogue skills, Species skills, Expertise). Right now how points can be
  distributed is seemingly unknowable."): the class's skills, then a Human's
  or Elf's pick, then Expertise; "+" takes a pick only in its step, and every
  row the step can't take is greyed as the game greys one (CGuiCheckbox::
  SetEnabled 0, its name SetColor 0.5) — after the list is built and after
  every "+", "-" and Recommended (their handlers hooked: the builder stores
  "+" / "-", the page's init registers RecommendButton). This level's picks
  stay live for "-".
- **A skill the class can't use** (Performance for a Rogue: greyed, "You may
  not learn this skill") is never taken (CNWClass::IsSkillUseable).
- **The background's Origin feat** (Criminal: Alert, Soldier: Savage
  Attacker) isn't offered on the Feats page at creation — it comes with the
  background.
- **"Ability Score Improvement" listed 21 times** (L9): the Feats page groups
  a master's sub-feats only while they follow one another in the feat list's
  order (CNWRules +0xf8, by name); the seven ASIs' choices share names, so
  they interleaved. After every rules load their block is reordered ASI by
  ASI.
- Recommended's tries no longer log each refusal.

*C6 (0.15.0, owner 2026-10-22: "Go for C6").* Game start and the
chapter-export check. Verified in the code: EE saves each object's own
SQLite database with it — CNWSCreature::SaveCreature calls
CNWSObject::SaveSqliteDatabase unconditionally (beside its material,
texture and animation overrides and its UUID), and a vault character's load
(CNWSPlayer::LoadCreatureData, LoadCharacterFromIFO) calls
LoadSqliteDatabase — while local variables aren't in SaveCreature (§5). So
(src/core/records.cpp) nwn5e's per-character records stay in local
variables (fast to read) and a player character's are also written to its
database (table nwn5e_records, through the script bridge: SqlPrepareQueryObject
on the character). Once per character object (marker nwn5e_records) the
first reader restores them from the database — an exported character
entering a new chapter, a vault character in a new game — or, the database
being empty, copies an older character's local records into it. Records:
the ability sync's applied deltas (without them an exported character's
background increases and level +1 removals would be applied twice), ASI
choices and the legacy count, an older !background, the !skills profile,
the class features nwn5e granted, mastery swaps, the background's Origin
feat. Moved to engine data on the way: an older !background record to its
background feat, older local masteries (before C4) to their feats. Origin
feats: the background's, as a real feat (Soldier: Savage Attacker, Criminal:
Alert; Acolyte and Sage's Magic Initiate waits for spells) — on the sheet,
kept by exports — taken back if the background changes or outside faithful,
never one the character chose itself. Tools: Thieves' Tools (Criminal) is a
proficiency already (the background's skills); the others (Calligrapher's
Supplies, a Gaming Set) have no NWN use — nothing to record. The check:
`!export` ([debug] test_commands) exports the character as a chapter change
does; a new game with it should keep its scores, skills and choices. Not
verified offline: EE ships no script compiler and the script bridge only
works with a module loaded, so the database scripts are first compiled in
game — a failure is logged once ("records: the restore / write / copy
script failed"), and the records then stay in local variables as before.

*Decided (owner, 2026-10-21): the Domain step stays until the Cleric
conversion* — NWN's two domains are picked at creation until the Cleric's
brief (Phase 4) drops what domains grant (powers, domain spells, the bonus
slots) and gives the Life Domain at 3. The Familiar step follows the same
reasoning and stays until the Wizard, Sorcerer and Druid briefs. Why both
were held: the Domain and Familiar steps. Skipped the same
way, a Cleric would keep the engine's default domains — Air and Animal
(CNWSCreatureStats_ClassInfo's constructor: domain bytes 0 / 1) — with
neither domain's powers, and their domain spells still castable; a Wizard,
Sorcerer or Druid would keep its Summon Familiar / Animal Companion feat
with a creature nobody chose. Both are class features (CM9): proposed to
go with the Cleric's and the arcane casters' / Druid's briefs (Phase 4),
where the feature mask removes what the steps grant.
