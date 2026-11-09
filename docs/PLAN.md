# nwn5e — full D&D 2024 (5.5e) conversion of Neverwinter Nights Diamond

> **EE port (2026-10-20).** This plan is copied from nwn5e (1.69) at d6cf14a.
> This repo targets **NWN: Enhanced Edition 89.8193.37-17 (64-bit Windows,
> single player)** by route B (docs/EE-RECON.md): an x64 DLL that finds engine
> functions by their exported names. Every decision below stands. The engine
> addresses, byte patches and 1.69 file paths below are 1.69's; each rule is
> re-found in EE when it is ported, and its "built" note is updated then.
> Status: E1 built (0.2.0) — the rest of Phase 1 combat; E0's crit rules
> verified in game (0.1.1) (README). Before them,
> spike (0.0.2) verified in game 2026-10-20 — `GetCriticalHitRoll`
> hooked by name; every hit threatened (9 of 9 in The Prelude). EE calls it
> only on hits, from ResolveAttackRoll; UpdateCombatInformation calls it too
> (both hands, for the sheet). On Easy difficulty a creature's confirmed crit
> on the player stayed a hit — EE keeps the rule B7 removes.
> **Port brief: docs/EE-PORT.md** — every built feature's EE route, the
> shared pieces, the order (E0–E4) and decisions EP1–EP20 (decided
> 2026-10-20: all recommendations; faithful allows 8 classes — the engine's
> limit and nwn5e's one deviation from 2024 multiclassing; this replaces
> CM2's "3 classes" for EE).

Target: **NWN Diamond 1.69 (GOG), single-player**, `X:\GOG\NWN Diamond\nwmain.exe`
(PE32 x86, linked 2008-07-03, unpacked, fixed base 0x400000, no relocations).
Rules: **2024 rules as published in SRD 5.2** (CC-BY-4.0). Non-SRD 2024 material
(extra subclasses, backgrounds, feats) is personal-use only and must not ship in
anything shared.

**Public ruleset = SRD 5.2 only (owner, 2026-10-19).** Player's Handbook
material is added and tested reworded, in the **private layer**
(docs/PRIVATE-LAYER.md): a git-ignored `private/` folder with its own repo,
compiled in only on the owner's machine. This repo holds the mechanism, never
the content, until PHB material is released under an open license (OGL /
CC); then it may move here.

Goal: a **full conversion** — the game plays by 2024 rules end to end, not a
"5e-flavoured" 3e.

**Possible later move to NWN: Enhanced Edition (owner, 2026-10-19; not now).**
Diamond 1.69 stays the target. When routes cost about the same, prefer the
one that would carry over: the rules in `src/rules`, 2da / tlk / texture data
and NWScript port; `nwmain_1_69.h` addresses and hooks into 1.69 client panels
do not (EE has NUI for custom windows). Mark 1.69-only work as such.

## How it works

NWN keeps its rules in three places. Each 5e rule lands in the one that owns it:

| Layer | Owns | How we change it |
|---|---|---|
| Data (`.2da`, `dialog.tlk`) | numbers: hit dice, class tables, feats, spells, skills, base items | override folder / hak |
| NWScript (`.nss` → `.ncs`) | spell effects, rest, dying, module events | override folder / hak |
| Engine (`nwmain.exe`) | attack resolution, saves, crits, attacks per round, level-up, spell memorisation, GUI | **nwn5e.dll** hooks injected by **nwn5e.exe** |

- `nwn5e.exe` starts `nwmain.exe` suspended, injects `nwn5e.dll`, resumes it.
  The game files on disk are never modified; launching NWN normally is vanilla.
- `nwn5e.dll` refuses to patch anything unless `nwmain.exe`'s SHA-256 matches
  the build we mapped.
- Single-player runs an embedded server inside `nwmain.exe`; the server-side
  rule classes (`CNWS*`) are compiled into it at different addresses than in
  `nwserver.exe`. Function addresses are found with REA (Ghidra provider)
  starting from the community's 1.69 `nwserver.exe` names
  (`re/seeds/`, provenance in `docs/RE-NOTES.md`), and recorded in `re/maps/`.

## Phases

Each phase ends with an in-game test by the owner (the build machine cannot
play the game).

**Tracking (owner, 2026-10-29: "turn the current plan into gh issues so that
we can keep track of the work we're doing").** Open work is tracked in GitHub
issues on BrunoTCarreiro/nwn5e-ee and the nwn5e board (CLAUDE.md: labels,
milestones = these phases, the lifecycle). This file stays the design and
decision record; each issue names its section here. The first 76 issues
(#1-#76) were opened from this file on 2026-10-29: everything built and not
yet accepted, decided and not built, waiting for a decision, the known
follow-ups, and the phases and classes not started.

### Phase 0 — Foundation
- Toolchain in `X:\tools` (Ghidra 12.1.4, Node 24.21, REA 6.5.0). ✅
- Function map `nwserver.exe` → `nwmain.exe` for the rule functions.
- Loader + DLL + hook framework (MinHook, logging, hash check, ini config).
- Proof of concept: crits only on a natural 20, no confirmation roll.

### Phase 1 — Combat core

Split into sub-plans that each ship on their own: **1a** attack math
(proficiency bonus, Extra Attack, 2024 crit damage, creature numbers), **1b**
advantage/disadvantage and conditions, **1c** saving throws, **1d** weapon
mastery, two-weapon fighting, opportunity attacks. The rule math lives in
`src/rules/` (no engine code, unit-tested by `test.bat`); the hooks only read
engine state, call it, and write the result back.

Built so far (`src/rules/`, 31 tests): ability modifier, proficiency bonus by
level and by CR, save DC / spell attack, advantage/disadvantage d20 rolls,
Exhaustion penalty, attack ability and bonus, hit/miss/critical resolution,
damage with crit dice rolled twice, attacks per Attack action, expected
monster AC by CR.

- Attack roll: d20 + ability mod + proficiency bonus (+ magic) vs AC; no BAB.
- Advantage / disadvantage as a first-class roll modifier (sources: conditions,
  flanking replaced, hidden/unseen, prone, ranged in melee, etc.).
- Critical hits: natural 20 only, double damage dice, no confirmation.
- Attacks per round: 1, Extra Attack (2/3/4 for Fighter), no iterative −5s.
  NWN drops the rest of a round when its target dies (Cleave exists for
  that); 2024 Extra Attack lets each attack pick a target, so remaining
  attacks should move to the next enemy in reach.
- 2024 weapon properties and **Weapon Mastery** (Cleave, Graze, Nick, Push,
  Sap, Slow, Topple, Vex) on NWN base items.
- Two-weapon fighting per 2024 (Light property, Nick; bonus-action attack).
- Opportunity attacks as reactions.
- Conditions (2024 list) mapped onto NWN effects; exhaustion (−2 per level to
  d20 tests, −5 ft speed).

### Phase 2 — Saves, checks and skills
- Six saving throws (Str/Dex/Con/Int/Wis/Cha); class save proficiencies.
- Save DC = 8 + PB + ability mod; spell attack = PB + ability mod.
- Skills as proficiency / expertise (no skill points); 2024 skill list mapped
  onto NWN's skills (drop/merge the 3e-only ones).
- Ability checks and contested checks (grapple/shove → 2024 Unarmed Strike options).

### Phase 3 — Character model
- Levels 1–20, proficiency bonus +2…+6, 2024 XP table.
- Classes: the 12 2024 classes. NWN has 11 of them; **Warlock** is new.
  Subclass at level 3 (2024). NWN prestige classes become subclasses where a
  2024 equivalent exists, otherwise disabled.
- Species (2024): no ability increases from species.
- Backgrounds (2024): ability increases + Origin feat + skill/tool proficiencies.
- Feats: Origin, General (ASI at 4/8/12/16), Fighting Style, Epic Boon (19).
- Multiclassing prerequisites (13 in primary ability).
- Hit points: max at 1st level, fixed average or roll per level.
- Decided: "Phase 3 — character model: decided by the owner" below
  (CM0–CM13); facts in docs/PHASE3-CHARACTER-RECON.md.

### Phase 4 — Spellcasting
- Slot tables per class (full/half/pact casters); Pact Magic for Warlock.
- 2024 preparation: prepared list changes on a long rest; spells cast from any
  slot of sufficient level — **no per-slot memorisation**.
- Cantrips at will, scaling at 5/11/17.
- Upcasting; concentration (one at a time, Con save on damage: DC 10 or half
  damage, max 30); rituals (decide).
- Rewrite spell scripts to SRD 5.2; add missing SRD spells; retire 3e-only ones.
- **Vicious Mockery** (owner, 2026-10-19: option A — a real Bard cantrip, not
  a Taunt stand-in; Taunt stays off in faithful, SK8): new spells.2da row +
  script (ports to EE). Wisdom save against the caster's spell DC (the target
  must hear: deafened targets are immune); on a failure 1d6 psychic damage
  (2d6 / 3d6 / 4d6 at levels 5 / 11 / 17) and disadvantage on its next attack
  roll before the end of its next turn (through the advantage rules' sources,
  as the conditions add theirs). Decide then: NWN has no psychic damage type
  (magical or sonic).
- *#36, spell slots and 2024 preparation.* How it was (both modes): NWN's casting - Wizard, Cleric, Druid, Paladin,
  Ranger memorise a spell into each slot; Sorcerer and Bard know a list with uses per spell level; 3e counts with bonus
  spells for a high casting score; cantrips limited; Paladin and Ranger from level 4. The engine has both models, by
  data (classes.2da MemorizesSpells, SpellKnownTable, MinCastingLevel; cls_spgn / cls_spkn). SRD 5.2: slots by class
  level (full casters; Paladin and Ranger half casters from level 1; a multiclass character one combined table:
  full-caster levels + half the Paladin and Ranger levels, rounded up); a prepared list sized by class level, any
  prepared spell cast with any slot of its level or higher - Cleric, Druid and Wizard (from its spellbook) change the
  list after a Long Rest, Paladin and Ranger one spell, Bard and Sorcerer one when they gain a level; cantrips at will,
  stronger at 5 / 11 / 17; upcasting. Options: 1 the model - A NWN's known-spells model for every caster, B NWN's
  memorising with 2024 counts, C nwn5e's own counters; 2 upcasting - A automatic (no slot left at the level: the
  lowest higher slot, the spell told the slot's level), B A + by choice, C none; 3 changing prepared spells - A a
  "Prepare spells" window (after a Long Rest; at level-up for Bard and Sorcerer), B NWN's level-up picker only, C a
  chat command; 4 multiclass - A one combined pool, B a pool per class.
  - *Decided (owner, 2026-11-04: "go with your recommendations and note down the upcasting thing so you don't forget when doing 39"):* 1A, 2A,
    3A, 4A. **For #39:** 2B - upcasting on purpose (a higher slot chosen, e.g. the radial's metamagic entries as
    "cast with a level N slot"), the rewritten scripts scaling by the slot level 2A passes.
  - *The plan:* Part 1 the 2024 slots and spontaneous casting; Part 2 upcasting (2A) and the combined multiclass pool
    (4A); Part 3 the Prepare spells window (3A) - one window for every caster in faithful (at creation, at level-up,
    after a Long Rest; each class's limits; the prepared count a total over the levels, as 2024's - NWN's own picker
    counts per spell level), the Wizard's spellbook kept apart from its prepared list.
  - *Built 0.74.0 (Part 1; key `spellcasting`, faithful by default):* rules::SpellSlots2024, CantripsKnown2024,
    PreparedSpells2024, SpellGain2024, CastSpendsUse (SRD 5.2's tables; test SpellSlots2024; with the data test 10 of 10 mutations caught); data/files/cls_spgn_5e
    {bard,cler,dru,pal,rang,sorc,wiz}.2da (test SpellGainTablesAreSrd52 against the rules) and classes.if_spells2024
    (SpellGainTable; Cleric, Druid, Paladin, Ranger MemorizesSpells 0; MinCastingLevel 1; the Paladin on CHA; no
    spontaneous cures). rules_spell_slots.cpp hooks GetSpellGainWithBonus (rules::SpellGain2024 for a caster class's
    slot: no 3e bonus spells, domain slot or prestige caster levels), GetSpellMinAbilityMet (no 3e minimum score),
    DecrementSpellsPerDayLeft and SetMemorizedSpellInSlotReady (cantrips at will).
  - *0.74.1 (owner, 2026-11-04: "SL-3 cantrips run out / SL-4 paladin has no spell I can cast at an enemy"):* a cast
    spends in CNWSCreatureStats::UnReadySpell, which decrements a spontaneous caster's per-level byte inline (ClassInfo
    +0x168) and unreadies a memorised spell through the ClassInfo's SetMemorizedSpellInSlotReady - neither hooked in
    0.74.0. Now UnReadySpell is hooked (a cantrip's use put back) and the ClassInfo's SetMemorizedSpellInSlotReady
    (a memorised cantrip stays ready); the slot log is player characters only, once per value (NPCs at module load
    had used it up). SL-4: NWN's 1st-level Paladin spells are buffs and Cure Light Wounds (a touch attack on undead,
    no save), so the Paladin's Charisma DC shows with #39's spells; the row checks the slots instead.
  - *0.74.2 (owner, 2026-11-04: "SL-2 should not be allowed to queue spells a larger number of times than the slots
    available / SL-3 cantrips still show a number os uses even though they never go down / SL-5 wizard doesn't have
    cantrips"):* SL-2 - AddCastSpellActions checks only the uses left now and a use is spent when casting starts, so
    a cast order is refused (the engine's own "no uses" feedback) when the casts already queued from the same slots
    use up what is left (rules::CastQueueFull, DrawsOnSameSlots; test CastQueue, 7 of 7 mutations caught): a
    memorising class's ready copies of the spell, else the slots of its level; the running cast is the queue's first
    node between ticks and counts until it has spent its use (creature +0x81C); cantrips never. Through a new
    action_guard SpellOrderGuard (the whole order). SL-3 - the server sends plain counts (no "unlimited"), so the
    client's uses getter (a byte pattern) marks a 2024 caster's cantrip and its caller's next CExoString::ToString
    comes back empty: no number on the quickbar, the spell panels or the cure menu (a Wizard's memorised cantrip
    keeps its ready count on the quickbar). SL-5 - !test gives a Wizard every Wizard cantrip (NWN's character
    creation does; swap_class skipped it).
  - *Asked (2026-11-04), after the owner's SL-5 ("currently they are allowed to memorize one spell in multiple slots.
    I don't think that's how it's supposed to work? ... This just gives the caster less flexibility"):* a 2024 Wizard
    prepares a list from its spellbook after a Long Rest and casts any prepared spell with any slot of its level -
    Part 3's model. When: A the plan's order (Part 2, then Part 3; the Wizard memorises until then), B Part 3 next,
    then Part 2, C a stop-gap now (the Wizard casts any spellbook spell with any slot; NWN ties its level-up picks and
    scroll learning to memorising, so research and risk). Recommended B: Part 3 is the fix, and Part 2's upcasting is
    built once, on every caster casting from shared slots (a memorised NWN slot holds one spell).
  - *Decided (owner, 2026-11-04: "go with B"):* Part 3 (the Prepare spells window) next, then Part 2.
  - *Part 3 brief (2026-11-04; research: the engine and the window).* The engine's known list per class slot is
    saved with the character (SaveClassInfo "KnownList"; ReadSpellsFromGff) and is what the radial and spellbook
    show; CNWSCreatureStats::AddKnownSpell caps a player's list by CNWClass::GetSpellsKnownPerLevel (exported, server
    and client) unless the class has LearnScroll - also on load. The server checks no spells at level-up
    (ValidateLevelUp); ValidateCharacter counts them only with Enforce Legal Characters on. LearnScroll checks the
    class's LearnScroll, not MemorizesSpells. A player character's locals are saved in savegames only, not in an
    exported .bic (SaveServerCharacter -> SaveCreature); an item's locals are in both. NWN's spell-pick page shows
    for SpellbookRestricted classes unless SkipSpellSelection (client only). Questions:
    1 what the radial shows - A the prepared spells and cantrips only (the known list; every 2024 caster
    MemorizesSpells 0, SpellbookRestricted 1; GetSpellsKnownPerLevel answers 2024's counts; a Wizard's whole
    spellbook on an undroppable Spellbook item, scrolls learned into it), B every spell, the unprepared greyed and
    refused; 2 where spells are chosen at creation and level-up - A a Spells page in NWN's own screens (Route A,
    CHARGEN.md 9) changed to 2024's counts for every caster, the in-game window for the Long Rest changes only, B the
    in-game window for everything; 3 when a Long Rest change is allowed - A the window opens after a Long Rest and a
    radial entry reopens it until the first cast, B only when the rest ends. Recommended A, A, A.
  - *Decided (owner, 2026-11-04: "go with your recommendations. But for 3, can you make the window show up for wizard
    characters while they are resting? Here they can change their memorized spells but the button to accept the
    memorized spells only becomes enabled once the full rest completes?"):* 1A, 2A, 3A, and the window opens when
    the rest starts: the list can be changed while resting, its accept button enabled once the rest completes (an
    interrupted rest changes nothing); after it, the radial entry reopens it until the first cast. For every class
    that changes its list after a Long Rest (Wizard, Cleric, Druid; Paladin and Ranger one spell) - the owner
    named the Wizard; the rule is the same for the others (assumption noted to the owner).
  - *The plan:* 3a the engine's lists, the Wizard's Spellbook item and the Prepare spells window (rest-time,
    radial; until 3b it also fills the places a new character or a level-up opens); 3b the Spells page in the
    creation and level-up screens (the level-up's swaps: Bard and Sorcerer one spell, a cantrip). Research for 3a
    found that NWN keeps no known list for a Cleric, Druid, Paladin or Ranger, and a blueprint Wizard has only its
    memorised spells, so those blueprint casters would cast nothing: creatures that aren't player characters
    (henchmen too) know their class's spells.
  - *Built 0.75.0 (Part 3a):*
    - rules::LongRestChange, LongRestCantripChange, PreparesFromSpellbook, SpellbookSpells2024, KnownSpellsCap2024,
      ListChangeAllowed (SRD 5.2's Spell Preparation by Class and Spellcasting features; test SpellLists, 14 of 14
      mutations caught). A list over its count (NWN's, e.g. a Wizard's seven cantrips) may change freely down to it.
    - classes.if_spells2024: every 2024 caster MemorizesSpells 0, SpellbookRestricted 1, SkipSpellSelection 1 (NWN's
      spell page until 3b). rules_spell_slots.cpp hooks CNWClass::GetSpellsKnownPerLevel (the cap on a player's
      known list: the cantrips, the prepared total at a level with slots; none while ReadSpellsFromGff loads).
    - core/prepare_spells.cpp: the window (NUI n5prep: Cantrips / Prepared / Spellbook tabs, a list with icons and a
      prepared mark, the clicked spell's description, the counts, Accept / Cancel); opened as a rest begins
      (rest.h's new AddRestStartListener - CNWSCreature::Rest moved there from rules_rest.cpp), Accept enabled at
      the rest's end, an interrupted rest changing nothing; after a Long Rest the change stays open until the first
      cast (UnReadySpell); the radial's Prepare Spells (feat 1547, spell row 1049, n5_prepspl) and !prepare open
      it; it opens by itself when a list is empty or over its count, and after a level-up.
    - The Wizard's Spellbook: nw_it_book015 renamed, cursed (undroppable) and plot, the ids in its local (an item's
      locals are in the .bic); an existing Wizard's NWN spellbook moves in once; LearnScroll's new spell goes into it.
    - Creatures that aren't player characters: a caster class's empty list at a level it has slots for is filled
      with every class spell of that level (first seen, and after a rest) - a Cleric's, Druid's, Paladin's or
      Ranger's (NWN keeps none) and a blueprint Wizard's (only memorised spells, e.g. nw_skelmage).
    - !test spellcleric / spellpaladin / spellwizard now go through the window (0.74.2's "every Wizard cantrip"
      step is gone - the window chooses three).
  - *0.75.1 (owner, 2026-11-05: "attempting to do !test spellcleric crashes the game"):* CExoArrayList<uint>::Add
    from CNWSCreatureStats::LevelUpAutomatic (+0x17B0; the crash report's backtrace, via LevelUpHenchman). With the
    Cleric's list now restricted, the automatic level-up walks the class package's spells, asks
    GetSpellMinAbilityMet with each one's Cleric level - 0xFF off the list - and on a yes adds it to its per-level
    lists at that index. Part 1's hook said yes to every level; now only 0-9 (rules::SpellLevelMet2024, 2 of 2
    mutations caught). And !test empties a rebuilt caster's lists (the automatic level-ups pick the package's
    spells) and its Spellbook, so the window starts as a new character's (PrepareSpellsReset).
  - *0.75.2 (owner, 2026-11-05: "PR3 fails, interrupting rest allows the player to accept changes as if the rest had
    been completed. close the prepare spells window if the rest is interrupted"):* the change of an earlier rest
    (here !test's ForceRest) was still open - no spell cast since - so the interrupted rest's window accepted it. A
    rest's start now ends an earlier rest's change (only this rest's end opens one), and an interrupted rest closes
    the window ("the rest was interrupted - your spells stay as they were"); a rest interrupted before the window
    opened opens none.
  - *0.75.3 (owner, 2026-11-05: "PR-4 where's the prepare spells in the radial menu?"):* Prepare Spells (feat 1547;
    cls_feat List 3, OnMenu 1 - Turn Undead's place on the radial) is also one of nwn5e's quickbar actions now
    (rules/quickbar: record bit 39; an empty slot, keys 1-9 first, and a chat line), and its grant is logged.
  - *Asked (2026-11-05, owner: "is PR-4 what 2024 rules suggest?"):* not quite - SRD 5.2: "Whenever you finish a Long
    Rest, you can change your list of prepared spells"; nothing keeps the change open after it. PR-4's "until the
    first cast" was 3A's safety net. A keep it, B the change only in the window of that rest (Accept, Cancel or
    closing it ends it; reopening then only fills empty places), C as B with one reopen after a Cancel. Recommended B.
  - *Decided (owner, 2026-11-05: "go with B. The game is not strict at all about long rests so this grace period is
    unecessary"):* B.
  - *0.75.4:* the change opens as the rest finishes, in that rest's window (or, for a rest with no window at its
    start - a script's ForceRest, !test's - one opened at its end), and Accept, Cancel or the window's X ends it
    (the window's token tells a replaced window's close from this one's). Asked with it: do the 2024 rules limit Long
    Rests? SRD 5.2: "After you finish a Long Rest, you must wait at least 16 hours before starting another one" (1 HP
    to start; interrupted by Initiative, a non-cantrip spell, damage, an hour of exertion - an hour rested first gives
    a Short Rest's benefits, and the rest can resume at +1 hour per interruption). nwn5e has no limit (R1, owner
    2026-10-23: "no limit"); a follow-up issue holds the question.
  - *0.75.5 (owner, 2026-11-05: "PR6. spellbook tab should be the first one." / "PR7. cannot use a second prepared
    cantrip" / "PR8. grease is already in the spellbook"):* the Wizard's tabs are Spellbook, Cantrips, Prepared; a
    scroll of a spell already in the Spellbook is refused and kept (its Cast Spell property's iprp_spells SpellIndex
    read first - LearnScroll checks only the known, prepared list and would use it up). PR7 read as the 2024 limit
    (one cantrip per Long Rest for a Wizard), asked back. The profile's Wizard 3 prepares 6 (2024), not 5.
  - *0.75.6 (owner, 2026-11-05: "the prepare spells window should be made larger, right now the top line reads
    'Wizard 3 - cantrips O of 3, prepared O of 6, spellbook 1O of' the last 10 is cut off. Might happen with longer
    named classes."):* the header is two lines (the class and level; the counts), the window 980 x 680 (was 820 x
    620), the list 480 wide, the status a wrapping text box, the Spellbook tab's hint shorter (owner: "All other
    messages in the spellbook are also cut off"). Asked with it, "There's no way to change spells in the
    spell book, is this normal?" - yes: SRD 5.2's spellbook only grows (six level 1 spells, two per Wizard level
    after 1, copied spells); what a Long Rest changes is which of them are prepared.
  - *Tested (owner, 2026-11-05):* PR-6 and PR-8 pass ("6 had passed previously and 8 passed this time"); "PR-7 I can
    cast multiple cantrips"; "PR-9 and 11 pass"; PR-10 dropped ("will not test, forget those older characters" - the
    code that moves a Wizard's NWN spellbook into the item stays: a new Wizard's empty book is made by it).
  - *Part 3a accepted (owner, 2026-11-05: "part 3a accepted, start 3b").*
  - *Part 3b brief (2026-11-05; research: the client's spell page).* NWN's Spells page (CCharacterSpellsPanel,
    vtable 0xE7B580, .gui CG_SPELLS) follows the class pick at creation and level-up for a SpellbookRestricted class
    without SkipSpellSelection; it counts per spell level - the client's spells left (0x7EE050, not exported:
    GetSpellsKnownPerLevel minus the known at that level) - with the cantrips on their own tab, and its picks go to the
    client's known list. The level-up message already carries removals (HandlePlayerToServerLevelUpMessage reads an
    added and a removed list per level; LevelUp applies both), but the page has no swap. With 3a's counts it is right
    at creation (but the Wizard's would be its prepared list, not its book) and over-allows later (Cleric 2->3: 7
    picks for 1 new place) - a total-across-levels bound on 0x7EE050 (and the hub's quick level-up, 0x7EB630) fixes
    that. Plan: 3b-1 the page for Bard, Sorcerer, Cleric, Druid, Paladin and Ranger with 2024's counts; 3b-2 the
    Wizard; 3b-3 the level-up swaps. Questions: 1 the Wizard's spellbook - A on NWN's page (6 at creation, 2 a level;
    the columns the book), B the Prepare window's Spellbook tab; 2 the level-up swaps - A on the page (drop a known
    spell, pick its replacement), B the Prepare window after the level-up with one swap; 3 the page's counts - A NWN's
    "Remaining" per tab, B 2024's totals ("Cantrips 3 of 3, Prepared 5 of 6"). Recommended A, A (built last; back
    to the owner before any fallback to B), B.
  - *Decided (owner, 2026-11-05: "go with your recommendations"):* 1A, 2A, 3B.
  - *Built 0.76.0 (Part 3b-1):* NWN's Spells page at creation and level-up for Bard, Sorcerer, Cleric, Druid,
    Paladin and Ranger (classes.if_spells2024: SkipSpellSelection 0). core/spells_page.cpp hooks, by byte patterns,
    the client's spells left (bounded by the total left: rules::SpellsLeftOnPage - the Remaining number, a pick,
    Recommended), the page's ShowLevel (KnownTitle reads "Cantrips N of M" / "Prepared N of M", 3B) and the quick
    level-up (the hub's no-Customize path and Recommended: at its GetSpellsKnownPerLevel call the answer is bounded,
    rules::BoundedKnownCap; test SpellsPageTotal, 5 of 5 mutations caught). A tab's enable test and the other readers
    keep the per-level count (they ask whether a level has spells). After a level-up the Prepare window opens only
    for places left (the Wizard's, until 3b-2).
  - *Tested (owner, 2026-11-05):* "SP-4 there's no recommended button on the page" - the page's Recommended shows by
    the ruleset's CHARGEN_ENABLE_RECOMMENDED_BUTTON (1; set only at the page's setup), so at a level-up it may be NWN's
    own layout's; the level-up screen's Recommended (the quick level-up) is the bounded path to test instead. "SP-7
    Not a fan of the spellbook appearing AFTER the class configuration can it be integrated into the character
    creation somehow?" - 3b-2 (1A).
  - *Built 0.77.0 (Part 3b-2):* the Wizard on NWN's Spells page too (SkipSpellSelection 0): its levels 1+ are its
    spellbook - six at creation, two at each level-up (KnownSpellsCap2024 answers the book's size for a Wizard; a
    Wizard's LearnScroll keeps the engine from capping its list), the count bounded by the book and the picks
    together, the title "Spellbook N of M", a spell already in the book not offered again (CNWSpell::GetSpellLevel
    answers 0xFF at the page's Available pass). The picks land in the known list; prepare_spells.cpp takes them into
    the Spellbook item and keeps the prepared list to its count - the first four at creation (2024: "choose four
    spells from your spellbook"), at a level-up what was prepared and then the picks (rules::TakeWizardPicks, test
    WizardPicks; 4 of 4 mutations caught). Single player: the page reads the player character's book.
  - *0.77.1 (owner, 2026-11-05: "again initial prepare spells pop up out of nowhere would this make sense to integrate
    with the caster tutorial part?"):* the log: a new Wizard arrived with 7 cantrips (2024: 3) - NWN's page setup
    gives a LearnScroll class at level 1 every cantrip (the client's AddKnownSpell in a loop, 0x9CE2FD) - so the
    window opened to trim them. That add is skipped for a 2024 caster: the cantrips tab picks 3, the character arrives
    complete and no window opens (no tutorial tie-in needed: the window is a rest's, or the quickbar's).
  - *Tested (owner, 2026-11-05, 0.77.1's WZ-1 / WZ-2: a new Wizard's cantrips and spellbook on the page):* "It passes".
  - *Built 0.78.0 (Part 3b-3, 2A):* research found that stock NWN already lets a non-memorising class drop any known
    spell on the Spells page (its Remove, 0x9CA3E0; the level-up message sends what's gone as removed and the
    server's LevelUp applies it) - unlimited, and since 0.76.0 for all seven casters. core/spells_page.cpp hooks
    Remove: a pick of this level-up can always be dropped; a spell known before it (the page's own copy of the known
    lists at its setup, +0x1A20) only as SRD 5.2's level-up swap - Bard and Sorcerer one level 1+ spell, Bard,
    Cleric, Druid and Sorcerer one cantrip (rules::LevelUpSwapAllowed, test LevelUpSwaps, 5 of 5 mutations caught) -
    else refused, the title reading "No more swaps". Reset, Recommended and Cancel undo swaps as NWN does.
    !test spellpagesorc (a Sorcerer 2 with the XP for 3).
  - *0.78.1 (owner, 2026-11-05: "knowing that you can swap one spell and knowing which spells were previously there
    and the once there weren't can be a bit confusing ... maybe new selected spells that go on the right get a 'new'
    label and spells that vare swapped out to the left get a 'known' or 'old' label"):* at a level-up the
    known column's picks of this level-up read "<name> (new)" and the available column's dropped spells "<name>
    (old)" (CNWSpell::GetSpellNameText at ShowLevel's two calls; none at creation, where all would be new); a drop
    rebuilds the tab as a pick does.
  - *0.78.2 (owner, 2026-11-05: "instead of (old) make it '- Out' and add '- In' to the ones swapped in"):* the
    labels read "<name> - In" (every pick of this level-up - a pick isn't tied to the spell it replaces) and
    "<name> - Out".
  - *0.78.3 (owner, 2026-11-05: "the spells that were swapped in don't say '- IN'"):* a pick doesn't rebuild the
    tab, as 0.78.1 assumed: the page's Add (0x9C74C0, a click on an available row) builds the known row itself and
    names it at its own CNWSpell::GetSpellNameText call, and calls ShowLevel only to move on to the next level's
    tab. So a clicked pick showed no label until the tab was rebuilt. Add is hooked and its name call labelled too:
    "- In" unless the spell was known before (a dropped spell picked back).
  - *0.78.4 (owner, 2026-11-05: "bug: when removing a spell that's been added prepared spells go from '6 of 6' to
    '5 of 6'. re-adding it back doesn't increase the counter to 6 and the OK button remains greyed out. same happens
    in the cantrips"):* two causes. The title was refreshed only when the tab was rebuilt, and a pick doesn't
    rebuild it: Add now refreshes it. And the OK button needs all ten levels marked done (+0x19E0), which NWN sets
    only at the page's setup and for the tab picked on (0x9CE730). 2024's total is shared by the spell levels, so a
    pick on one tab left the others' marks stale: a Sorcerer 3's level 2 (2 left at the setup) stayed not done once
    level 1's picks filled the total, whatever tab was picked on, cantrips included. After a pick or a drop every
    level with nothing left is now marked done, then the OK button updated (0x9C6E70); a mark is only ever set.
  - *The labels' arrows (owner, 2026-11-06: "For 2 is it possible to have arrow symbols? If not go work your
    recommendation"):* not possible - the game's text is Windows-1252 (the language's code page), which has no
    arrow characters (the nearest are the guillemets « »). "- In" and "- Out" kept (owner, 2026-11-06: "2
    keep").
  - *Part 2 next (owner, 2026-11-06: "Go with your recommendations for 1 and 4."):* Part 2 - upcasting (2A) and
    the combined multiclass pool (4A) - while Part 3b waits for the owner's in-game tests.
  - *Built 0.79.0 (Part 2a, upcasting, 2A):* every 2024 caster casts spontaneously since 0.75 (a byte per spell level,
    ClassInfo +0x168). rules::SlotToSpend (its own level, else the lowest higher slot; never a lower one), CastsLeft
    (the slots at its level and above), CastFitsQueue (the queued casts spend in order, as they will; test Upcasting,
    10 of 10 mutations caught). rules_spell_slots.cpp:
    - GetSpellUsesLeft hooked (the cast's own check, UnReadySpell's, the AI's, GetHasSpell): the casts left when NWN
      would cast the spell with a slot of its level - asked with that level's byte set to 1 for the question if empty,
      so every other check (known, metamagic feat, polymorph) stays NWN's.
    - UnReadySpell checks GetSpellUsesLeft, then decrements the slot level's byte inline (the only cast spend: the
      binary's other writes to those bytes are the scripts' Decrement / IncrementSpellsPerDayLeft). For an upcast the
      byte is 1 while NWN spends it, and the lowest higher slot is decremented instead.
    - The slot level spent is kept on the caster, local int N5_SLOT_LEVEL, for #39's spells ("the spell assumes the
      higher level for that casting"); a player is told "no level N slot left - cast with a level M slot".
    - The client's uses getter answers the same total from its copy (class slot +0x288): the quickbar's number, and
      no greyed button while a higher slot is left.
    - SL-2's queue check spends the queued casts' slots in order (a fifth 1st-level cast with four 1st-level slots
      queues, into a 2nd-level slot).
    `!test spellupcast` (Sorcerer 3: 4 / 2 slots); spellcleric's note: nine 1st-level casts queue, not four.
  - *Revised (owner, 2026-11-06: "Upcasting should not be automatic, the player should have/get to choose at what
    level they cast a spell. I picture it as selecting the spell from their original level but there being an extra
    level (in the radial menu) where the user can go into and pick at which level they want to cast the spell at.
    For the quickbar, they can just set an upcast spell in a slot"):* 2A gives way to 2B, upcasting by choice
    (planned for #39, brought forward): a spell's radial entry gets one more level listing the slot levels to cast it
    with, and an upcast version can be put on the quickbar. A cast from the spell's own entry with no slot of its
    level left fails, as NWN's. The approach to look into: NWN's metamagic versions of a spell (the radial's
    submenu, the quickbar's assign menu, the cast's metamagic byte and its slot-level cost) - "cast at level N"
    versions through the same machinery. 0.79.0's automatic upcast stays until the choice is built, then goes.
  - *Researched (2026-11-06; the map in the session's notes):* NWN's metamagic versions aren't a submenu under a spell
    - they are a separate branch under each class (Metamagic -> type -> level -> spells, 0x9A24B0) - so the spell ->
    level submenu is new radial UI. The plan: the chosen slot level rides in the cast's **domain level** (radial param
    bits 24-27, a byte on the wire, action +0x48, creature +0x21C - GetLastSpellLevel() returns it - and the
    quickbar's QBDomainLevel, saved and reloaded); a spontaneous class never uses it today. Client: CRadialMenu::
    SetEntryx (0x1FA010) hooked - a 2024 caster's spell leaf (callback 0x9A4540) becomes a submenu
    (SetSubMenuCreatorx 0x1FA7F0) listing "Level N" for each level with slots, each casting with domain N;
    DisableEntry (0x1F8B30) kept off the parent while a slot of its level or above is left; the client's uses getter
    counts level N's slots for domain N, its own level's only for domain 0. Server: GetSpellUsesLeft and
    UnReadySpell read domain N as the slot level (the original asked with domain 0: ConfirmDomainSpell would refuse
    it); SL-2 likewise. The quickbar's assign mode is the same radial, so an upcast version goes on a slot as is.
    Open: the quickbar button shows no level mark (the count differs); a 1st-level spell with slots up to 9th needs
    a "more" entry (the radial holds 8).
  - *Decided (owner, 2026-11-06: "can we add extra text to an upcasted slot in the quick bar?", then "1 ok, 2 go with
    L3·4"):* the one extra radial click to cast at a spell's own level is fine; an upcast quickbar button's number
    reads "L3·4" (level 3, 4 slots left) - the text the game writes after the uses getter, rewritten as the
    cantrips' is.
  - *Built 0.84.0 (upcasting by choice; 0.79.0's automatic upcast gone):* rules::ChosenSlotLevel (the domain level
    names the slot, 0 the spell's own, never a lower), CastFitsQueue at exact levels, UpcastMenuLevels (its level and
    each higher one with slots; none when there's no higher one - the entry casts at once), UpcastCountText
    ("L3·4"), DecodeRadialCast / RadialCastWithDomain (the radial's param; tests Upcasting, RadialParam; 22 of 22
    mutations caught).
    - Server: GetSpellUsesLeft answers a domain level N with level N's slots when NWN would cast the spell with one of
      its own level (asked with domain 0 - ConfirmDomainSpell refuses a spontaneous class - and that byte 1 for the
      question); UnReadySpell spends level N the same way (NWN spends the own level's byte, set to 1, domain 0;
      level N's is decremented); domain 0 is NWN's own level. SL-2 counts exact levels, domain levels included.
    - Client: CRadialMenu::SetEntryx hooked - a 2024 caster's spell leaf (the cast callback, found by pattern; no
      metamagic, no domain, no other listing) with a higher level of slots becomes a SetSubMenuCreatorx entry (its
      icon, name and param). Its creator builds a CRadialMenu as the game does (its operator new from the spell
      list's own call, the constructor, +0x7F1) with "Level N" entries (the text SetEntryx: the spell's icon
      resref, the label) casting with domain N (0 for its own), each count the client's level-N uses
      (SetCountText), greyed with none unless the quickbar is assigning (handler +0x890); past 8 levels the last
      entry opens the rest. The client getter: domain N - level N's uses; domain 0 - its own level's, the radial's
      spell lists (the two getter calls in functions that call SetEntryx) the slots at its level and above, so the
      parent isn't greyed while a higher slot is left. An upcast quickbar button's number: the getter's caller's
      next ToString rewritten "L3·4". The quickbar keeps the param (QBDomainLevel), saved with the character.
    `!test spellupcast` and spellcleric's notes rewritten.
  - *The owner's 0.84.0 run (2026-11-06):* "everything goes through, but check the logs to figure out why can't I
    cast the negative energy ray, it says I have 3 slots" / "it sais I had no preparations left". The client log:
    nine "No uses or preparations left for that spell." (the server's GetSpellUsesLeft: the spell isn't in the
    Cleric's known list) - the quickbar button (key 8) was left from the Sorcerer test (`!test spellcleric` swapped
    the class); Negative Energy Ray is a Cleric 2nd-level spell, so the button counted the Cleric's three 2nd-level
    slots - NWN's client count for a spontaneous class reads the level's slots, never the known list. *0.84.1:* a
    2024 caster's spell its class doesn't know reads 0 on the client (spells_page.cpp's ClientKnowsSpell: the
    client's known count and known spell), so such a button greys out - as after a Long Rest's change too.
    UC-1 to UC-7 pass ("everything goes through"); UC-8 to retest with a prepared Cleric spell on the quickbar.
  - *The owner's 0.84.1 run (2026-11-06):* "SP-3. when no moore spells are prepareable grey out the ones that aren't
    prepared. if the user presses the prepared ones they unprepare and all other spells stop being greyed out." /
    "WZ-3 should not allow to accept without picking all the spells and cantrips." / "UC-9 if a spell is unprepared
    but in the quicjbar (and in the radial menu) instead of 0 it should have "NP" for "not prepared"". WZ-5/6
    ("cannot long rest due to the 16 hour limit"): the log shows a Long Rest just before (#135's wait, as built).
    *0.84.2:* the Prepare spells window greys the spells a full list doesn't hold (rules::PickRowEnabled; a picked
    one stays clickable, and dropping it frees the rest); Accept waits until each list holds its count, or every
    spell it could take (rules::ListComplete), with a status line saying so; a 2024 caster's spell its class
    doesn't know reads "NP" on the client (the getter's caller's next ToString, as "L3·4").
  - *The owner's 0.84.2 run (2026-11-06):* "the button didn't change after the spell being unprepared". The log: the
    rest's window accepted at 15:17:39 ("prepared 9 (+1 -1)"), after the rest's uses were back. The client's player
    update (HandleServerToPlayerUpdate_PlayerInfo) applies a known-list change (its remove and add known spell)
    without refreshing the quickbar - it does so for memorized spells and uses left only - so the button kept its
    count. *0.84.3:* after the update's remove and add (by return address), the quickbar's own refresh runs
    (spells_page.cpp).
  - *The owner (2026-11-06):* "why does the radial wheel show spells that have 0 slots? for example I can assign
    color spray although that's not an available spell to prepare, nor is it in the spells menu". The log: a Wizard
    3 loaded, then `!test spellcleric` swapped its class slot to Cleric and emptied the lists. The client's
    RemoveKnownSpell finds a spell's level through the slot's class (CNWSpell::GetSpellLevel) and returns when the
    class has none, so the Wizard's spells off the Cleric list stayed in the client's list; the radial (a
    spellbook-restricted class lists its known spells) showed them with no uses. The server's list was right (the
    cast refused). Only a class swap - a `!test` profile - does this; loading a save rebuilds the client's lists.
    The same caused 0.84.1's stale Negative Energy Ray. *0.84.4:* after the remove, the spell comes out of
    whichever level of the slot still holds it (spells_page.cpp).
  - *The level-up swaps (OK-1 to OK-4; owner, 2026-11-06: "So I can only replace a limited number of spells? I
    can't just remove Daze and Light for example, for Flare and Resistance?"):* explained - SRD 5.2 lets a Bard or
    Sorcerer replace one spell, and a Bard, Cleric, Druid or Sorcerer one cantrip, at each level gained; a house
    rule for more was offered. *Decided (owner, 2026-11-06: "do not add unlimited swaps, only those allowed by
    2024"):* the swaps stay SRD 5.2's (rules::LevelUpSwapAllowed); no house rule.
  - *Accepted (owner, 2026-11-06: "those open for testing are all tested and accepted").* #36 - Parts 1, 2, 2b, 3
    and 3b (0.74.0-0.84.4): the 2024 slots, spontaneous casting from the known list, upcasting by choice, the
    multiclass pool, the Prepare spells window, NWN's Spells page with 2024's counts and swaps, NP / L3·4 on the
    quickbar. Carried elsewhere: cantrip damage at levels 5 / 11 / 17, in each cantrip's script (#39, which reads
    the slot level a cast spent); Pact Magic with the Warlock (CM11); the tab icons' look (#137); "Resting" on
    Accept (#139).
  - *Built 0.80.0 (Part 2b, the combined multiclass pool, 4A):* SRD 5.2's multiclassing - with the Spellcasting
    feature from more than one class, the slots are the Multiclass Spellcaster table's (the full casters') at the
    Bard, Cleric, Druid, Sorcerer and Wizard levels plus half the Paladin and Ranger levels, rounded up; each class
    prepares by its own level. rules::MulticlassCasterLevel2024 (0 with fewer than two caster classes: one keeps its
    own table), SharesSlotPool, SlotsForClassSlot2024 (test MulticlassSlots, 14 of 14 mutations caught). NWN keeps
    uses per class slot, so every caster class's GetSpellGainWithBonus is the pool's table (cantrips stay each class's) and a
    rest fills them alike; a spend from one class is made in the others - the cast's (UnReadySpell, upcasts
    included) and a script's (DecrementSpellsPerDayLeft). Each class's counts stay the pool's, so the casts left and
    the client's numbers read them as they are, and no class shows 0 slots in NWN's own panels (designed first as
    one class holding the pool and the others none - dropped before building: a panel that hides a spell level with
    no slots would have hidden the second class's spells). SL-2's queue counts the casts of every class sharing
    the pool. After a level-up the next rest makes every class's counts the new pool's. `!test spellmulticlass`
    (Wizard 3 / Cleric 2: the table's level 5, 4 / 3 / 2).
  - *0.83.0 (owner, 2026-11-06: "instead of addin 'in' and 'out' is it possible to change the visibility of the
    icons used to add and remove spells? if so we can just prevent the user from attempting to remove unremovable
    spells. a spell that was added shoould keep the icon that allows the user to remove it while all the other ones
    that would result in a 'no more swaps' warning would get their icons hidden"):* the labels "- In" / "- Out"
    are gone (the GetSpellNameText hook with them). Each row on the page is a CGuiCharGenFeatButton (model
    CTL_CG_BTN_FEAT) with two CGuiMouseoverChildButton children: +0xC8 the move button (Add or Remove), +0x198 the
    info button (shown on hover). Once a tab is built, a known row whose spell can't be dropped now - known before
    this level-up, its list's swap spent or none for the class (rules::RemoveIconShown; test LevelUpSwaps, 3 of 3
    mutations caught) - has its move button taken out of the panel (CGuiObject::RemoveFromPanel, as the familiar page's row
    does); rows and spells matched by the known list's button and id arrays (+0x430, +0x420 = the page's +0x11C8). A
    drop rebuilds the tab, so the other rows lose theirs at once; a pick that undoes a swap rebuilds it, the buttons
    back. "No more swaps" stays as the fallback.
  - *0.83.1 (owner, 2026-11-06: "ALso remove the icon that adds spells to the prepared column once it is no longer
    possible to add new spells"):* once the tab has nothing left to pick (rules::AddIconShown: its spells left,
    LeftOnPage; test LevelUpSwaps, 2 of 2 mutations caught), every available row's move button is taken out too - after the tab is built and after a pick (the
    available list +0x890, laid out as the known one: its ids the page's +0xCB0, Add's). A drop rebuilds the tab,
    the buttons back. RemoveFromPanel only detaches the button's model (the hover's own toggle for the info button).
    And (owner, 2026-11-06: "And in cantrips, do not move automatically to level 1 spells once all the cantrips are
    seçected"): Add moves on to the next tab when the one picked on is done and the next has picks left - two looks
    at the next level's spells left (its calls into the client's spells left after "inc dl", one byte pattern, two
    matches in the binary). From the cantrips tab they're answered 0, so the page stays on it.
  - *The owner's 0.83.1 run (2026-11-06):* "IC is good" - IC-1 to IC-10 pass (the remove and add icons, the
    cantrips tab staying put). WZ-3: "labels on top of buttons don't follow the button order. on top of the buttons
    the texts go cantrips, prepared, spellbook, whiule the buttons go spellbook, cantrips, prepared" - *0.83.2:* the
    Prepare window's counts line reads Spellbook, Cantrips, Prepared for a Wizard, as its tabs. And "On level up it
    says 6 of 8 but only shows 4 spells" (the known column lists the prepared 4; the book's other 2 show nowhere):
    asked. The tab icons' greyed look: #137 (later). UP: "how does upcasting work? I can't seem to trigger it" - the
    log: four casts (Negative Energy Ray, Scare, Darkness and one more), at most three 1st-level slots spent of four;
    upcasting starts with the 1st-level slots all gone. Explained.
  - *Decided (owner, 2026-11-06: "go for A"):* WZ-3's count - a Wizard's level-up page shows its whole spellbook in
    the known column. *Built 0.83.3:* ShowLevel's known pass (its two calls of the client's known count, 0x7F66E0,
    and its one of the known spell, 0x7F5C50, by return address; the Available pass after it keeps its own) reads
    the book's spells at the tab's level, then the page's other known ones (rules::WizardPageRows, test
    LevelUpSwaps, 3 of 3 mutations caught). The page's OK (0x9C90D0) reads the client's own known lists, so only the picks
    go to the server. A book spell counts as known before (no remove icon; Remove refuses it).
- *#37, Concentration (owner, 2026-11-06: "let's start with 37 and 38").* SRD 5.2: a spell whose duration is
  "Concentration, up to ..." lasts while its caster keeps Concentration - one such spell at a time (casting another
  ends the first); damage calls for a Constitution save, DC 10 or half the damage (rounded down) if higher, at most
  30; Concentration ends when the caster is Incapacitated or dies, or when it chooses; Rage ends it (B2). How it
  was: NWN has none - every buff, area and summon runs its 3e duration, any number at once (SK7 only removed the
  Concentration skill's check while casting); spells.2da has no concentration column; a spell's effects carry its
  caster and spell id. Options: 1 which spells - A the NWN spells that are SRD 5.2 spells marked Concentration (a
  table from the SRD's descriptions; NWN-only spells with #39), B A + NWN-only ones by likeness; 2 who - A every
  caster (players, henchmen, enemies; NWN mode none), B player characters; 3 items - A scrolls and wands as a
  cast, potions not, B items never; 4 what's shown - A a resource panel line, combat-log messages for each save,
  an End Concentration action on the radial and the quickbar, B the messages only; 5 durations - A NWN's until
  #39 gives each spell its SRD duration, B the SRD's now.
  - *Decided (owner, 2026-11-06: "37, all recommendations except 4A, do not have a end concentration item in the
    quickbar, add a button for that in the resources panel. Maybe clicking the \"concentrating: <spell>\"
    element"):* 1A, 2A, 3A, 5A; 4: the resource panel's "Concentrating: <spell>" line, which a click ends, and
    the combat-log messages - no End Concentration action on the radial or the quickbar.
  - *Built 0.86.0 (key `concentration`, SK7's; faithful):* rules::IsConcentrationSpell - 84 spells.2da rows whose
    spell is an SRD 5.2 Concentration spell (from SRD 5.2.1's Duration lines; the sub-radial variants listed too;
    Endurance and Bigby's Crushing Hand kept with Enhance Ability and Arcane Hand), ConcentrationSaveDc,
    ItemCastNeedsConcentration (a potion's - base items 49, 104 - needs none); tests ConcentrationSaveDc,
    ConcentrationFromItems, ConcentrationSpellsAreSrd52. rules_concentration.cpp: a RunScript start listener
    (script_hooks.h's new AddScriptStartListener) sees a Concentration spell's own ImpactScript (spells.2da's,
    read once by the script bridge) begin for its caster (its spell +0x218, its cast item +0x818) - the caster's
    earlier one ends first. What holds it: the caster's effects of that spell (CGameEffect spell id +0x1C) on any
    object of its area and its areas of effect (creator +0x330, spell +0x314); ending removes them (a script per
    object) and destroys the areas; none left - over. Damage (OnApplyDamage's listener): the engine's save,
    forced to Constitution, DC on the 2024 scale; a failure, down, Incapacitated (IncapacitatedByCondition) or
    Rage end it at the caster's next update. The resource panel: rules::HudPool's `line` (no pips, no count) and
    `click`; "Concentrating: <spell>" in gold, a left click over it (the client) ends it on the server's next
    panel update (resource_panel.h's AddHudClick). The player's Concentration is a local variable too (a load).
  - *The owner's 0.86.0 run (2026-11-06):* CO-1 to CO-5 in the log (Bless, its end by Shield of Faith, a recast,
    the panel's click, the saves - six made, then one failed); CO-7 untested (no such potion: `!give
    nw_it_mpotion015` is Bull's Strength); CO-6, 8, 9 not in the log. The failed save's ending read "Bless ends - "
    (its reason read after its record was erased). "Why does the PC's concentration survive a load but enemies'
    don't?" - only the panel and a hit looked at a creature's saved one. *0.86.1:* the reason kept; a game's start
    (CNWSModule::LoadModuleFinish) clears the last game's Concentrations, and every creature's saved one is taken
    up at its first update. "Endurance and bigby's hand ... this doesn't really matter, right?" - right: #39's
    rewrite retires or folds such NWN-only variants, and the table follows it.
  - *The owner (2026-11-06):* "same for CO-6, 8 and 9 as they can't be tested normally" - left to a
    playthrough.
- *#38, rituals and material components (open decision 6).* SRD 5.2: a prepared spell with the Ritual tag can be
  cast as a Ritual - 10 minutes longer, no slot, so never upcast (a Wizard from its spellbook, unprepared:
  Ritual Adept); a focus or a component pouch covers ordinary material components, a component with a cost must
  be on the caster and one the spell consumes is used up each cast. How it was: NWN has neither; of its spells
  only a few carry the Ritual tag (Identify, Silence). Options: rituals - A left out for now (revisited if #39 adds
  Ritual spells that matter), B a "Ritual" radial entry out of combat, no slot, the clock 10 minutes on;
  components - A ignored, B only costly ones, paid in gold (a consumed one takes its cost, a kept one needs it on
  hand; a henchman pays from its master's gold; enemies exempt; NWN mode free), C costly ones as gem items.
  - *Decided (owner, 2026-11-06: "38, rituals A, components B for now, we'll need to check the meterial
    costs"):* no Rituals for now; costly material components paid in gold - the table of costs goes to the owner
    to check.
  - *The costs (SRD 5.2.1's, for the NWN spells that map to an SRD spell - 25 spells, their sub-radial variants
    too):* Bless 5 (kept), Protection from Evil and Good 25 (consumed), True Seeing 25, Continual Flame 50,
    Greater Restoration 100, Magic Circle 100, Stoneskin 100 (both NWN Stoneskins), Glyph of Warding 200, Raise
    Dead 500, Awaken 1,000, Planar Binding 1,000 (all three), Resurrection 1,000 (consumed); Clairvoyance 100,
    Identify 100, Create Undead 150 (and Create Greater Undead), Arcane Sword 250, Circle of Death 500, Holy Aura
    1,000, Shapechange 1,500, Gate 5,000 (kept); Legend Lore 450 (250 consumed). Asked: Restoration (between the
    SRD's Lesser and Greater), Protection from Alignment's 25 a cast, Greater Stoneskin's 100; NWN's economy
    (3e's guideline: about 49,000 GP of wealth at level 10; NWN's random chests give 2d20 GP at levels 9-10).
    *Decided (owner, 2026-11-06: "for now keep the gp cost. we'll see how it works on a playthrough"):* the
    SRD's costs as listed; Restoration free.
  - *Built 0.87.0 (key `spellcasting`, #36's; faithful):* rules::CostlyComponent - 42 spells.2da rows (the 25
    spells and their sub-radial variants), the GP on hand and the GP consumed - and ComponentAffordable (test
    MaterialComponentCosts, 8 of 8 mutations caught). rules_components.cpp: the spell order guard - a cast from a
    class slot of such a spell: the payer (the caster if a player character, else its master if one: a
    henchman, a familiar, a summon; enemies and NPCs pay nothing) needs the whole cost on hand, else the order
    is refused and told; the cast taking effect (spell_impact.h) takes the consumed part (TakeGoldFromCreature),
    told; an item's spell (a scroll, a wand, a potion) needs none. spell_impact.{h,cpp}: the cast's moment, the
    game's start, the cast item and the spell's name, moved out of rules_concentration.cpp and shared. "!gold
    [amount]" (test commands; rules::ParseGoldAmount, 5 of 5 mutations caught) shows or sets the gold.
  - *The owner's 0.87.0 run (2026-11-06):* GC-1 to GC-6 in the log (25 and 100 GP taken; refused at 20, 3 and 150
    GP); "can't test GC-7"; CO-7 (#37) "passes". *Decided (owner, 2026-11-06: "Let's have this system in place but
    for now let's turn it off under house rules"):* *0.87.1:* [house_rules] free_material_components (config,
    nwn5e.ini; announced on #123) - on for now, at the owner's word (house rules are otherwise off by default):
    no gold asked or taken; 0 brings SRD 5.2's costs back.
- *#39 Part 1, the cantrips (owner, 2026-11-06: "go for 39 focusing on the cantrips").* SRD 5.2 has 27 cantrips,
  NWN 10; 5 are both (Acid Splash, Light, Ray of Frost, Resistance; True Strike - NWN's a level 1 buff). NWN's play
  3e (Ray of Frost 1d4+1, Acid Splash and Electric Jolt 1d3, Resistance +1 to saves for 2 turns, Light for hours a
  level); the attack (SK10) and the DC (C5) are 2024's already. SRD 5.2's damage cantrips: 1d6-1d12, a die more at
  character levels 5, 11, 17, most with a rider. Options: 1 how - A served spells.2da rows and C++ scripts (as
  Disengage, Rage, the grenades; tested rules; NWN's visual effects), B NWScript compiled; 2 2024's damage types
  NWN lacks - A Lightning Electrical, Thunder Sonic, Necrotic Negative, Radiant Divine, Force and Psychic Magical,
  Poison Acid (none to a poison-immune creature), B all Magical; 3 which - A those that do something in NWN (Fire
  Bolt, Chill Touch, Shocking Grasp, Poison Spray, Sacred Flame, Starry Wisp, Sorcerous Burst, Vicious Mockery;
  Guidance, Spare the Dying; True Strike as 2024's, Shillelagh, Produce Flame; Eldritch Blast with the Warlock),
  B A + light versions of the utility ones; 4 NWN's own (Daze, Flare, Electric Jolt, Virtue, Cure / Inflict Minor
  Wounds) - A retired in faithful, B kept; 5 a choice inside a spell (Resistance's damage type, Guidance's skill) -
  A a menu (past 5 a "more" entry), B every type / every check; 6 three builds: the damage cantrips, support, the
  weapon cantrips.
  - *Decided (owner, 2026-11-06):* "1. A, but do not reuse icons yet. We'll need to see which icons become
    available after the existing abilities with no 2024 correspondence are removed." "2. A, but poison and acid
    are different things and force and psychic aren't magical" "3. B. Mage Hand can count as a summoned creature
    that allows the player to open/close doors, interact with chests, snuff lights... Prestidigitation can at the
    very least snuff and relight light sources. Minor illusion can create effects (sounds) and illusions (small
    creatures or humanoids) that will make guards or other creatures investigate (we have that functionality now)
    so they are not useless" "4. A" "5. recommnedations" "6. ok".
  - *What follows:* new spells show a placeholder icon of nwn5e's own (not a game icon) until the icons are
    reassigned. Poison, Force and Psychic become damage types of their own: NWN:EE has DAMAGE_TYPE_CUSTOM1-8
    (damagetypes.2da rows past Base, 12; the damage effect carries 32 amounts, OnApplyDamage reads them all,
    resistance and immunity take 32-bit flags) - served damagetypes / damagetypegroups / damagehitvisual rows give
    them their names, colours and hit visuals; a creature immune to poison takes no Poison damage. Lightning,
    Thunder, Necrotic and Radiant stay NWN's Electrical, Sonic, Negative and Divine. The utility cantrips (3B) are a
    fourth build, with the owner's ideas (Mage Hand a summoned hand for doors, chests and lights; Prestidigitation
    lights; Minor Illusion sounds and figures that draw creatures to investigate).
  - *Built 0.88.0 (build 1, the damage cantrips; `spellcasting` faithful):* rules::Cantrips (SRD 5.2.1's die, damage
    type and roll for each), CantripDice / CantripDiceRolled (Cantrip Upgrade at 5, 11, 17; doubled on a Critical
    Hit), RetiredCantrips (tests CantripUpgrade, CantripsAreSrd52, CantripsRetired). Served rows
    (spells.if_cantrips2024.new / .lists.txt): Fire Bolt 1050, Chill Touch 1051, Shocking Grasp 1052, Poison Spray 1053, Sacred
    Flame 1054, Starry Wisp 1055, Vicious Mockery 1056 - NWN's cantrips' animations and sounds, nwn5e's placeholder
    icon n5_is_new (its own art); Light 100, Ray of Frost 144, Acid Splash 424 (aimed at a point, a 1.5 m sphere) on
    nwn5e's scripts; Daze, Flare, Electric Jolt, Virtue, Cure / Inflict Minor Wounds off the class lists (the Prepare
    spells window drops a known one). damagetypes / damagetypegroups.if_cantrips2024.txt: Poison, Force, Psychic
    (custom 1-3, rows 13-15; names, the combat log's colours). rules_cantrips.cpp: each script in C++ - a chunk
    rolls (the engine's touch attack: SK10's spell attack; or the save against GetSpellSaveDC: C5's DC), C++ rolls
    the damage, a chunk applies it with NWN's visuals and the rider (Ray of Frost -33% speed a round; Chill Touch
    OnApplyHeal hooked - a heal heals 0 until the end of the caster's next turn; Shocking Grasp no Opportunity
    Attacks, rules_opportunity.cpp asks; Starry Wisp a light, its invisibility removed; Vicious Mockery
    Disadvantage on its next attack roll, rules_advantage.cpp asks; Poison Spray none to the poison-immune). !test
    profiles cantripbard and cantripwizard (level 5). Sorcerous Burst moves to build 2 (its seven damage types: the
    "more" menu).
    The launch check found spells.2da skipped ("line 16: expected a row number and 59 values": one rows file held two
    header lines) - split in two, and test RowsFilesParse now parses every rows file as the DLL does.
  - *Built 0.90.0 (build 2, owner, 2026-11-07: "go for it"; `spellcasting` faithful):* the cantrips cast through a
    choice (decision 5: a menu, past its slots a "More") and Spare the Dying. rules::ChoiceCantrips: Resistance
    (NWN's 151 rewritten - Cleric and Druid, SRD 5.2.1's text) with a variant row per damage type (1060-1070),
    Guidance 1057 per skill (1071-1088), Sorcerous Burst 1059 per type (1089-1095), each variant's Master its
    cantrip - NWN's own sub-radial mechanism: the engine maps a variant to its master in GetSpellUsesLeft,
    UnReadySpell and GetHasSpell (CNWSpell +0x178; a spell's variants a heap list at +0x168, count +0x170, the
    loader reading SubRadSpell1-8), so a variant needs no class level and a quickbar button can hold one
    (rules_spell_slots.cpp's client count treats it as its cantrip). The radial: #36's SetEntryx hook turns the
    master's leaf into a menu of its options (rules::RadialChoicePage: eight to a page, or seven and "More"; its
    page in param bits 44-48), each casting its variant through the radial's cast callback (#140's retargeting
    applies). Resistance and Guidance mark the creature touched (a tagged VFX_DUR_CESSATE_POSITIVE carrying the
    variant's id; Concentration ends it - the variant rows are in rules::ConcentrationSpells): Resistance takes
    1d4 off the chosen type, once per turn, before Resistance halves it (resistance.h's new reducer); Guidance adds
    1d4 to checks with its skill - the scripts' GetIsSkillSuccessful (its d20), the engine's converted checks
    (rules_skill_dcs.cpp's SkillAtCheck: Open Lock, Search, traps, Spellcraft) and !skill. Cast with no choice
    (an item's or a creature's master): the first type taken, the first skill checked; Sorcerous Burst Fire.
    Spare the Dying: a down player character Stable (rules_dying.cpp's StabilizeDownPlayer); its range 15 ft,
    doubling at 5, 11, 17 (CNWSCreature::ComputeSpellRange hooked). Sorcerous Burst: a ranged spell attack, d8s,
    each 8 another d8 up to the Charisma modifier (rules::SorcerousBurstDamage); off the Wizard's list
    (spells_page.cpp's GetSpellLevel: 0xFF for the Wizard, NWN's Wiz_Sorc column being shared). Tests
    ChoiceCantripsAreSrd52, RadialChoicePage, RadialChoiceParams, SorcerousBurstDice, ResistanceAndSpareTheDying;
    !test profiles cantripcleric, cantripsorcerer.
  - *0.90.1 (owner, 2026-11-07, testing 0.90.0: "While concentrating on spells that give bonuses (like resistance to
    piercing, for example) these bonuses should appear on the top right while the concentrating is happening";
    "Can't test CB-5"):* Resistance and Guidance carry an effect icon while they last (linked to their marker, so
    Concentration's end takes it too): effecticons.2da rows 130-160 (the game's run to 129, EffectIcon takes up to
    255), one per option, named for it - the variant's name ("Resistance: Piercing", "Guidance: Stealth") - with
    the game's Damage Resistance and Skill Increase icons (rules::EffectIconOf). The log of 0.90.0's test: Resistance
    (Piercing) stopped 3 of 8 and 1 of 1, once a turn; Guidance (Stealth) added +2, +1, +4 on !skill.
  - *0.90.2 (owner, 2026-11-07, CB-7: "Spare the Dying doesn't cast and if trying to cast via radial by pressing
    elsewhere to get the cursor, on click the cursor turns into a talk cursor and the henchman gets up and initiates
    a conversation"; "the others are good"):* the log - the cast at the down henchman queued, then dropped before
    its conjuring. A down henchman is ethereal (0.20.2, so enemies turn on the party): Sanctuary without a save, and
    CNWSEffectListHandler::OnApplySanctuary marks every creature in the area (visibility node +4: bit 2 can't see;
    a made save bit 3 sees through) - its master too. Now each creature friendly to it sees through, as after a made
    save (rules_dying.cpp FriendsSeeThrough, logged with each node's flags); its enemies stay blind. Only that apply
    sets the bits, so a creature arriving later isn't marked.
  - *Build 2 accepted (owner, 2026-11-07: "accepted, go for build 3"):* CB-1 to CB-4 and CB-6 to CB-10 pass (CB-7
    on 0.90.2: Spare the Dying and Cure Light Wounds reach a down henchman); CB-5 (a conversation's Persuade check)
    left to a playthrough.
  - *Build 3 brief (2026-11-07): the weapon cantrips - True Strike (NWN's 415 rewritten), Shillelagh, Produce
    Flame.* No spell can be cast as a Bonus Action yet (only feats - ReqAction 0, instant, one a round - and
    potions); weapon attacks have the hooks (the attack's ability: Finesse; its die: the Martial Arts die; its
    damage types per hit). Q1 Bonus Action casting: A a cast turned into a Bonus Action (a hidden feat, as the class
    abilities; reusable for 2024's Bonus Action spells), B a normal cast for now. Q2 True Strike: A the cast becomes
    one of the game's own weapon attacks (its magic, on-hit, Mastery, Sneak Attack; the round's other attacks
    dropped), B nwn5e rolls it inside a cast. Q3 the damage type: A a radial choice, B always Radiant / Force, C the
    weapon's unless picked. Q4 Produce Flame's hurl: A its menu (Flame a Bonus Action, Hurl an action while it
    lasts), B a separate ability. Q5: A two installs (3a Bonus Action casting and Produce Flame; 3b Shillelagh and
    True Strike), B one.
    - *Decided (owner, 2026-11-07: "1 A, 2 A, 3 A, 4 A and while Produce Flame is active the character is also a
      light source, 5 A"):* recommendations throughout; Produce Flame's flame makes its caster a light source (the
      stealth rules' carried light).
  - *Built 0.91.0 (build 3a):* Produce Flame 1096 (the Druid's; nwn5e's placeholder icon) - its radial menu Flame
    (1097) and Hurl (1098, Master 1096). Flame: NWN's 15 m orange light on the caster (VFX_DUR_LIGHT_ORANGE_15, a
    carried light for the stealth rules - visual ids 153-180), 10 minutes, the old one ending when cast again. Hurl:
    an action while it lasts - a ranged spell attack, d8 Fire, Cantrip Upgrade, the flame staying; with no flame
    the order is refused with a word (no round lost). Bonus Action casting (Q1 A): rules::BonusActionFeatOfSpell -
    the cantrip and its Flame carried by feat 1548 (feat.if_cantrips2024.txt, SPELLID 1097; TakesBonusAction makes
    it instant in faithful `actions`); an AddCastSpellActions guard refuses their cast and uses the feat instead
    (granted at the first cast) - instant, one a round, a second waiting for the next round (bonus_actions.cpp).
    Tests ProduceFlame, BonusActionFeats; !test cantripdruid.
  - *Built 0.92.0 (build 3b; owner, 2026-11-07: "build 3b"):* Shillelagh 1099 (the Druid's; nwn5e's placeholder
    icon) - its radial menu Force (1100) and Bludgeoning (1101, the weapon's own) - cast as a Bonus Action (feats
    1549 / 1550, as the Flame), with a Club or Quarterstaff (or NWN's magic staff 45) in the hand, refused without
    one. For 1 minute (a marker with effect icons 161-163, carrying the cantrip's row so the AI sees it): that
    weapon's melee attacks use the better of Wisdom and their own term (Strength, or Finesse's Dexterity) for attack
    and damage (GetAttackModifierVersus, GetDamageBonus); its die a d8, d10 at 5, d12 at 11, 2d6 at 17 (GetDamageRoll's
    weapon-dice filter, as the Martial Arts die - the better of the two); Force moves the weapon's damage to Force
    (ResolveDamage). Letting go of the weapon (another in the hand, or none) ends it (the creature update). True
    Strike (NWN's 415 rewritten: a Bard, Sorcerer and Wizard cantrip cast at a creature) - Radiant (1102) or the
    weapon's type (1103): an AddCastSpellActions order guard turns the cast into an attack order at it (no cast);
    the next round started at it (within 15 s) is cut to one attack (InitializeNumberOfAttacks: no off-hand,
    extra or Martial Arts attack; an Action Surge's still added). That attack uses the class slot's spellcasting
    ability in place of Strength or Dexterity (an item's: the best of Int, Wis, Cha); Radiant moves the weapon's
    damage to Radiant; 1d6 Radiant more at 5 (2d6 at 11, 3d6 at 17), doubled on a Critical Hit. Refused with a
    word: no weapon in the hand, or cast at no creature. Not checked: proficiency with the weapon. Tests
    Shillelagh, TrueStrike, WeaponCantripDamage; !test cantripbard (short sword and dagger, shortbow, Charisma
    18) and cantripdruid (a club in the pack). Next free ids: spells 1104, feats 1551, effect icons 164,
    strings 16000648.
  - *0.92.1 (the owner's 0.92.0 run, 2026-11-07: "I'm not sure shillelagh is work, no icon on top right and no
    notice that shillelagh ended on unequipping weapon"):* the log - each cast's marker missing 3 ms after its script
    (the watch ended at once, so no "let go" line) and the attacks without Wisdom (+3, not +6). The marker had been
    applied inside the feat's spell script, carrying the cantrip's row (1099); every feat ability that works applies
    its effects after it (the Monk's through DelayCommand, the Rage's from the creature update). Now the marker goes on
    through DelayCommand with the row cast (1100 / 1101), any Shillelagh row counts, and the watch gives it 2 s to land
    (a log line lists the creature's effects if it never does). Produce Flame's flame (the same path, untested yet)
    goes on the same way.
  - *0.92.2 (the owner's 0.92.1 run, 2026-11-07: "still nothing, no icon on the top right, no notice when changing
    weapons"):* the log - "marker never landed - 1 effects: t68/s-1" 2 s after each cast: through DelayCommand too,
    no part of the link (icon, visual) stayed on the caster. Cause not found yet (the engine's EffectIcon takes any id
    1-65535; ApplyEffectToObject leaves the spell id alone). Now the DLL keeps Shillelagh itself - its row, its
    weapon, its minute (game time) - so its attacks don't depend on the effect; its icon goes on as the Rage's does (a
    tagged supernatural EffectIcon from the creature update, 0.3 s after the cast). The first cast of a session logs a
    probe (1.5 s after it: each effect's tag, true type, spell id, duration type, first integer) of three test effects
    - C1 0.92.1's link applied in the cast's script, U2 the icon with the row's spell id, U3 the icon linked with the
    visual, both from the update - and takes them off.
  - *The owner's 0.92.2 run (2026-11-07):* Shillelagh works - its icon on (0.3 s after the cast), attacks +6 (Wisdom
    +3 for Strength +0), its 1d10 (Savage Attacker rolling it twice), the weapon's damage Force, and "ends - the
    weapon left the hand" on a swap. The probe: n5c_shillelagh (the icon alone, spell -1) and U2 (the icon with
    spell 1100) stayed; C1 and U3 - the icon linked with VFX_DUR_CESSATE_POSITIVE, in the cast's script or from the
    update - did not. So the link was what never stayed (why Resistance's same link stays isn't known); the icon
    alone is kept. True Strike: one attack in its round (Charisma +4 in place of +1), 8 of the weapon's damage made
    Radiant, 1d6 Radiant more; refused with no weapon in the hand. Produce Flame's Hurl found its flame. "Can't test
    TS-4, bard cannot equip short bow. or short sword for that matter": SRD 5.2.1 gives the Bard Simple weapons only
    (the short sword is Martial - the profile's mistake); the shortbow is Simple in 2024 but NWN's martial (its
    baseitems ReqFeat), and nwn5e doesn't give classes their 2024 weapon lists yet (#17, X7). The cantripbard
    profile now has two daggers and a light crossbow (Simple in both).
  - *Builds 3a and 3b accepted (owner, 2026-11-07: "accepted", after "TS-2,4 and 5 are confirmed" and "everything seems
    good"):* on 0.92.2 - Produce Flame (PF-1 to PF-8: the Flame a Bonus Action, Hurl refused with no flame, the
    second Flame in a round waiting, the carried light while hidden, the quickbar), Shillelagh (Wisdom, its die,
    Force, its icon, ending on a weapon swap), True Strike (its one-attack round with Charisma, Radiant and its d6,
    melee and ranged; refused with no weapon). The look of Produce Flame, Hurl and Fire Bolt: #143. Left: build 1's
    CA-1 to CA-10; build 4. The probe's code goes in the next install.
  - *Build 4 brief (2026-11-07): the utility cantrips, SRD 5.2.1's nine left (Eldritch Blast waits for a Warlock).*
    Q1 Mending and Message (no broken items, no one to whisper to in single player): A off the class lists until
    they have a use, B flavour. Q2 Mage Hand: A cast at a door, lever, light or container within 30 ft - the hand
    opens or closes it, uses it, lights or snuffs it, opens the container as if you stood by it; "Use" again
    from its menu while it lasts - B a hand steered like a familiar. Q3 Minor Illusion: A Sound (draws creatures
    that hear it, as heard_investigates) and Image (a crate, a humanoid or a small creature that draws creatures
    that see it; a Study action's Investigation check against the spell save DC sees through it), B the sound
    only. Q4 Dancing Lights: A four lights at a point, Dim Light 10 ft each (light for the stealth rules), moved
    with a Bonus Action, B one light following you. Q5 Druidcraft, Elementalism, Prestidigitation, Thaumaturgy: A a
    menu of each one's effects (the working ones - Fire Play, the doors, Booming Voice - and the rest a visual
    and a line), B the working ones only. Q6 sounds at a point (Phantom Sound, Druidcraft's animal sound): A they
    draw creatures, B a sound only.
    - *Decided (owner, 2026-11-07: "1A, 2A, 3A, 4 not too about the uses but A, 5 let's look into these deeper,
      6 A, depending on the creature they might also have an effect that would scare creatures away?"):* Q1-Q4
      A; Q6 A, and a sound may scare some creatures away instead, by the creature - to design; Q5 a closer look
      first.
    - *Q5 looked at (2026-11-07), each cantrip's effects - the working ones (Fire Play, the doors, Booming Voice,
      Magic Mark a little) and visuals - with Q7 how creatures react to a sound (A by its kind: voices or footsteps
      draw them, a roar or thunder makes animals, beasts and vermin flee for 3 rounds, a creature fighting ignores
      it; B one sound; C none flee) and Q8 Thaumaturgy's Fire Play (A dim puts a light out for a minute, B colour).*
    - *Decided (owner, 2026-11-07: "Q5.For elementalism beckon earch can be used on top of icy surfaces to negate
      sliperyness in that area, beckon water can create wet patches (that can be electrified for example) or snuff
      out lights. Sculpt elements, yes, don't use it. Prestidigitation is mostly for light sources. and marks I
      guess, but that's a bonus thing? thaumaturgy don't do the eyes and tremors Q7. A, but 2 turns Q8. A"):*
      Elementalism - Beckon Fire lights, Beckon Air closes doors, Beckon Water snuffs lights (and, with #142's
      surfaces, makes a wet patch that can be electrified), Beckon Earth (with #142) takes the slip out of ice in
      its area; no Sculpt Element. Prestidigitation - Fire Play, and Magic Mark as a bonus. Thaumaturgy - Booming
      Voice, Invisible Hand, Phantom Sound, Fire Play; no Altered Eyes or Tremors. Druidcraft as looked at. Q7 A, the
      fleeing 2 rounds. Q8 A.
  - *Build 4 started (owner, 2026-11-07: "Give me CA-1 to CA-10 and you start building the utility cantrips"), in two
    installs as build 3: 4a Fire Play and the doors (Druidcraft, Elementalism, Prestidigitation, Thaumaturgy less
    Phantom Sound, Mage Hand at doors and lights); 4b the sounds and illusions (Dancing Lights, Minor Illusion, Phantom
    Sound, Druidcraft's animal sound, the creatures' reactions - Q7, Mage Hand's levers and containers).*
  - *Built 0.93.0 (build 4a):* rows 1104-1122 (spells.if_cantrips2024.utility.txt; one impact script n5_c_utility,
    rules_utility_cantrips.cpp; no AI category, as NWN's Light): Druidcraft 1104 (Weather Sensor, Bloom, Sensory
    Effect, Fire Play), Elementalism 1109 (Beckon Air, Earth, Fire, Water - a 5-foot Cube, 1.5 m round the point or the
    object), Prestidigitation 1114 (Fire Play, Magic Mark), Thaumaturgy 1117 (Booming Voice, Fire Play, Invisible Hand;
    1121 Phantom Sound reserved for 4b), Mage Hand 1122 (no menu). Ranges by rules::CantripRangeFeet (30 ft;
    Prestidigitation 10) through ComputeSpellRange. Lights: light_sources.h's FirePlayAt (a lamp, brazier, torch
    bracket or tile flame's stand-in, as a click or the Light spell: its animation, the area's lighting, the stealth
    rule, guards who see one go out) - Fire Play and Mage Hand toggle, Beckon Fire lights, Beckon Water snuffs,
    Thaumaturgy's Fire Play dims one out for 1 minute (game time) and lights it again (any relight first cancels that).
    Doors (a script run as the door): Invisible Hand and Mage Hand open an unlocked shut one or shut an open one, Beckon
    Air shuts open ones. Booming Voice: Advantage on Intimidation checks for 1 minute (GetIsSkillSuccessful's d20 and
    !skill; it and Disadvantage cancel), its icon 164 (the icon alone, as Shillelagh's). Magic Mark: a blue glow
    (VFX_DUR_GLOW_LIGHT_BLUE, not a light the stealth rule counts) on an invisible placeable at the point for an hour of
    play (real time, not NWN's 2-minute hour), three per caster. Weather Sensor: GetWeather now (no forecast; indoors or
    underground no sky). Bloom, Sensory Effect, Beckon Earth: a visual and a line. Tests UtilityCantrips,
    UtilityCantripEffects; 16 of 16 mutations caught (a missed one - Produce Flame's modes taken as utility effects -
    fixed with a Hurl check). The Shillelagh probe's code out.
  - *The owner's 0.93.0 run (2026-11-08):* "UT-2. weather sensor, bloom and sensory effect don't seem to do much, they
    can be removed. Beackon water on the ground should leave a puddle. Actually, is there any "permanent" effect like
    that in NWN? Beckin earth on the ground should leave some sort of effect, nothing happens. UT-11 can't target a
    light source or a door with Thaumaturgy, the player has to target the PC and then use the cursor ... but
    prestidigitation seems to work fine"; then "we'll need to review the vfx for all these effects" (#143). The log:
    Fire Play, Beckon Fire / Water / Air, Invisible Hand, Booming Voice's advantage, the dim and its relight, Magic
    Mark's three - as built. *0.93.1:* Druidcraft is Fire Play alone, cast at a light (no menu; rows 1105-1108
    unused) - its Weather Sensor, Bloom and Sensory Effect out, the owner's call. The game offers a spell on an
    object's radial only when its row's TargetType takes the object: a menu's row now takes every target its effects
    take - Thaumaturgy 0x31 (yourself, a door, a light), Prestidigitation 0x34. Beckon Water's puddle and Beckon
    Earth's patch: a decision (NWN's lasting ground effects are areas of effect - Grease, Web, the fogs; no puddle or
    dust look among them).
    - *Brief (2026-11-08):* Q1 Beckon Water's puddle and Beckon Earth's dust: A lasting patches now, a look only, in 4b
      (a puddle 1 minute - SRD 5.2.1's water evaporates in 1 minute; dust 10 minutes - no end given), #142 adding
      their rules (electrified water, dust on ice); B with #142. Q2 their look until new visuals exist: A placeholders
      now (Grease's slick for the puddle, a dust puff a round), new ones asked for through #143; B the new visuals
      first. Q3 Druidcraft's animal sound (part of the Sensory Effect taken out; Q6 / Q7's sound that draws or scares):
      A kept in 4b (Druidcraft's menu again: Fire Play, Animal Sound); B dropped. Recommendations A, A, A.
    - *Decided (owner, 2026-11-08: "Go with your recommendations, but I'll design the looks, just tell me which VFX you
      need"):* A, A, A; the owner designs the new looks - the list of what each needs is on #143, and nwn5e serves
      the rows (visualeffects.2da, vfx_persistent.2da) for the owner's models when they're made.
  - *Build 1's tests (owner, 2026-11-08: "CA matrix passes, check the logs. go for 4b"):* the log on 0.93.1 - Fire Bolt
    2d10 (18), Ray of Frost 2d8 (11), Acid Splash 2d6 (10), Shocking Grasp 2d8 (5), Vicious Mockery 2d6 (8), Starry
    Wisp 2d8 (9), Sacred Flame 2d8 (8); no script errors. CA-10 (a save that knew Daze) not run.
  - *Built 0.94.0 (build 4b):* rows 1107-1108 (Druidcraft's Fire Play and Animal Sound - its menu again), 1121 and
    1134 (Thaumaturgy's Phantom Sound: Whispers, Thunder), Minor Illusion 1123 (1124-1130: Sound - Voices,
    Footsteps, Roar, Thunder; Image - a barrel, a humanoid, a small creature), Dancing Lights 1131 (1132 Lights, 1133
    Move; Concentration). Sounds (rules::SoundPlayOf; the game's own files on an invisible object at the point): those
    that draw - voices, footsteps, whispers - send the creatures hostile to the caster within 60 ft to look (searching,
    a look round, home; heard_investigates); those that scare - a roar, thunder, the animal's sound - make animals,
    beasts and vermin (racial types 8, 9, 25) flee 2 rounds, the others coming to look; none fighting, talking, plot
    or the caster's. Minor Illusion's image: plc_barrel / nw_commale / nw_cat for 1 minute - the creatures commoner
    faction, plot, ethereal (the AI doesn't see or attack it), a cutscene ghost (passed through), uncommandable; the
    hostile creatures that see the point come to look and, getting there, study it (d20 + Intelligence vs the spell
    save DC, rules::SeesThroughIllusion; the caster told); a new cast ends the earlier. Dancing Lights: four invisible
    placeables at the point (VFX_DUR_LIGHT_YELLOW_5 - the stealth rules' light - and a yellow glow, the orbs'
    placeholder), their effects the row cast (Concentration holds them; its end takes them off and the lights go), 1
    minute; out beyond 120 ft. Its Move: a spell order guard (no cast) with the order's point (action_guard.h's
    SpellOrder x, y, z), up to 60 ft and within 120 ft, a Bonus Action (bonus_actions.h's SpendBonusAction). Mage Hand
    at a placeable that's no light: a container opens (CNWSPlaceable::OpenInventory - its window closes once you move,
    as NWN's), a usable object is used (its OnUsed, GetLastUsedBy the caster: CNWSPlaceable +0x528); a locked one
    stays shut. Beckon Water at the ground: a puddle (vfx_persistent 103, Grease's slick) 1 minute; Beckon Earth: a
    dust patch (104, a dust puff a round) 10 minutes. Tests UtilitySounds, MinorIllusion, DancingLightsAndPatches; 16
    of 16 mutations caught.
  - *0.94.1 (owner, 2026-11-08: "there should be 5 new magic mark symbols, all tga with no alpha. can you wire them up
    for a test as well?"):* Magic Mark shows one of the owner's symbols (data/files/n5t_mark01-05.tga, 256 x 256)
    at random, flat on the ground and turned a random way: models n5v_mark01-05 (a 0.6 m quad just above the ground,
    self-lit, faded out at its end), drawn additively (n5t_markNN.txi "blending additive": black shows as nothing -
    the files' alpha, at most 88, is left aside), visualeffects.2da 1840-1844 (OrientWithGround, OrientWithObject),
    rules::MagicMarkVisual.
  - *0.94.2 (owner, 2026-11-08: "magic mark doesn't show anything on the place it was cast"):* the log - each mark made
    (its placeable and visual); the symbols' alpha channel was 0 on every pixel the lines included (the stray 88s
    aside), so the mark drew transparent - the same model path shows the light zones on placeables. The five TGAs are
    re-saved 24-bit (the empty alpha dropped, the colours as they were; the originals in b4e8e3f).
  - *0.94.3 (owner, 2026-11-08: "There's an issue with the last mark (05) where the square it's inscribed in can be
    seen"):* its background was dark grey (35, 31, 32), not black - drawn additively, a faint square. Levels: that
    grey to black, the rest stretched (white stays white).
  - *0.95.9 (owner, 2026-11-08: "CB-3 fails, concentration on resistance slashind disappears immediately"):* the
    log - "concentrates on Resistance: Slashing", then 3 s later "is over (none of its effects left here)", with
    or without damage. Build 2's marker for Resistance and Guidance was the icon linked with
    VFX_DUR_CESSATE_POSITIVE, the link 0.92.2's probe showed never stays. Concentration's check and Resistance's 1d4
    both look for that effect. The marker is now the icon alone, a plain (magical) one as the conditions' icons are,
    so Dispel Magic still ends it (owner: "why can't dispel magic remove resistance or guidance? wasn't it supposed
    to?" - a first build made it supernatural, never installed). Retests: CB-3b, CB-4 (Guidance) and CB-11 (Dispel
    Magic ends Resistance) on 0.95.9 - all three passed (owner: "they all pass now").
  - *0.95.10 (owner, 2026-11-08, UT-15: "the magic marks are not on the ground, they are floating"):* the mark's
    invisible object and its ground node are at the target point's height, so a point above the walkmesh (a click
    can give one) floats the mark. The point now goes down to GetGroundHeight, and the drop is logged ("the point
    dropped N m to the ground").
  - *Magic Mark on doors and placeables - decided (owner, 2026-11-08: "is it not possible to add magic marks on
    objects or walls?", then "Go with A"; SRD 5.2: "on an object or a surface").* Walls are out: NWN can't target
    one (a click there lands on the floor). 0.96.0:
    - A door: the mark stands on the slab's face on the caster's side (rules::MarkOnDoor; a closed door's
      walkmesh is 2.1 x 0.3 x 2.97 m, T_Door01's), flat on it, 1.4 m up.
    - A placeable: where its walkmesh starts on the line from the caster (the area's static-object line test,
      CNWArea::NoNonWalkPolysInDoors, halved 14 times), 3 cm out, facing the caster, 0.7 m above the ground; none
      found, 0.35 m from its centre (rules::MarkOnPlaceable).
    - The ground: as before (0.95.10's snap).
  - *The owner's 12 symbols (2026-11-08: "the new marks are in the x1 folder, you can delete the previous marks and
    add the new ones only and then you can delete the x1 folder"):* Illustrator's PNGs (RGBA, 256-257 px) put on
    black, cropped to 256 and saved as 24-bit TGAs, replacing the five. Rows 1860-1871 are flat (n5v_markNN) and
    1880-1891 upright (n5v_markNNu, two-sided, neither mirrored nor upside down from the front); 1840-1844 are no
    longer served. The 1x folder is removed.
  - *Magic Mark's colour - decided (owner, 2026-11-08: "would it also be possible to add a tint to the marks so that
    they're not always white?", then "Option b + random" - of B chosen when casting, A random, C one per symbol;
    SRD 5.2: "a color, a small mark, or a symbol").* 0.96.1:
    - Prestidigitation's menu is Fire Play and Magic Mark: White (1116), Gold, Red, Green, Blue, Violet
      (1135-1139) and Random (1140, one of the six): eight entries, one radial page.
    - Each colour is the material's colour (ambient, diffuse, self-illumination) on copies of the 12 marks, flat
      and upright (n5v_markNN<g r n b v>[u]); the texture stays the owner's white symbol.
    - Rows: White 1860-1871 / 1880-1891; colour t at 1900 + (t - 1) * 40 flat, + 20 upright
      (rules::MagicMarkRow). The layout puts White exactly where colour 0 would fall.
- **Visual effects that follow a body part (#144).** *Decided (owner, 2026-11-08: "I'd go for engine work as it
  allows for more flexibility with effects later by allowing nodes to follow body parts"),* over a held item (it
  takes the hand's slot) or an offset from the chest (it drifts as the character moves); asked for Produce Flame's
  flame in the hand (#141). The game pins effects only to the head, the chest ("impact") and the feet ("root");
  its "hand" casting glows sit at a fixed point in front of the chest (handconjure, linked to the model's base).
  - *Recon:* the client attaches every model with Gob::AttachToObject(target, node name, mode) (exported): the
    target's node found by name (GetPart, vtable +0x230), an attach behaviour keeping the model on it each frame -
    mode 0 CAurBehaviorAttach (position and turning), 3 CAurBehaviorAttachNoOrientation (position only), 1 / 2 / 4
    translate, rotate, GUI scale. Gob::GetModelName (vtable +0x18) names the attached model.
  - *0.95.0:* core/vfx_nodes.cpp hooks AttachToObject and AttachToPart: a model that a visualeffects row sends to
    a node (NWN5E_Node; NWN5E_NodeUpright 1 = position only, else turning with it; a plain attach's mode only)
    goes there when the target's model has that node, else where the game puts it. Rows files may add columns
    named NWN5E_... (data/twoda.cpp); data/effect_nodes.cpp reads the table: by model name, so a moved model needs
    its own name (one a row without a node also uses stays put, logged). Test rows 1845-1847 (right hand upright,
    left hand turning, head) with a marker (the owner's mark 05 on three crossed squares); `!vfxnode <row>`.
  - *The VFX bench's seam (the animation session, 2026-11-08, relaying the owner's "yes, go for it" to hand
    placement in the bench):* DevSeam::set_effect_node(model, node, upright), filled by core once the hooks are
    in: a model's node set at run time, before its row's (data::FindEffectNode); an empty node removes it.
  - *0.95.1:* installed with the bench's side (nwn5e-tools c55deea the page, 0b9a555 the dev layer, 40d27ee the
    chest and head through the same seam): "On the character's" right or left hand, chest or head, and Stays upright
    for Play in game (the bench's own notes: docs/TOOLS-PLAN.md). The game attaches its own effects with mode 3
    (position only) at head, impact and root.
  - *0.95.2:* nwn5e-tools 0d56019 - the bench clears a preview's node 10 s after the next Play or Stop (its end
    animation played at "root" when cleared at once); its preview keeps the world's directions for a position-only
    attach, as the game does (the owner keeps the hand flame centred, 0 0 z).
    Also d9a4fb3 / da78091 (owner: "build it"): the bench's Turns with the character - the feet, chest and head
    turn with it by default (a turning feet effect through set_effect_node(model, "root", false), mode 0), the hands
    stay upright by default.
  - *0.95.7 (the owner, D-1: "if I face south the flame still appears in front of the PC"):* a turning effect
    started on the world's axes, then turned with the character. Recon: after a position-only attach (a row without
    OrientWithObject; [effect +0x120] picks mode 0 or 3) the client turns the effect's root part to the world's axes,
    Gob::SetPartPosition("root", its position, identity). The hook now skips that call for an effect it turned to mode
    0 (data::SkipsWorldAlignment); the diagnosis log (fd4e81c) is removed.
  - *#145 accepted (owner, 2026-11-08, in the animation session's window: "#145 accepted").* The VFX bench's
    effect light (colour, radius, brightness, flicker, fades; a game light kept with its own flags), installed in
    0.95.3-0.95.8. Every row is passed or superseded: L-0, L-2, L-3b, L-5 to L-8, D-2 and P-1 passed; L-1, L-3,
    L-4 and D-1 superseded by their retests. The design notes are the animation session's (docs/TOOLS-PLAN.md).

### Phase 5 — Rest, healing, death
- Short rest (spend Hit Dice) and long rest (8 h, regain all HP and half max HD).
- Death saving throws at 0 HP; stabilise; massive damage.

### Phase 6 — Action economy
- Action / Bonus Action / Reaction / free object interaction inside NWN's
  6-second round. Highest design risk: NWN's action queue has no bonus action
  or reaction concept. Needs a design pass before code.
- *Recon (2026-11-02, #51's comment):* a feat with a spell (feat.2da ReqAction 1) clears the player's queue - the
  round's remaining attacks go - waits for the round's end and takes a 6 s cast round with no attacks: every nwn5e
  class ability costs the rest of the round and the next one today. A feat with ReqAction 0 runs its script at once
  (AI event 8) and leaves the round alone. One spell a round comes from WaitForEndOfRound and the cast round; a
  potion drinks inside the round (1 s) but the order clears the attacks; an equip pauses the round 1.5 s, no attack
  lost; the reaction count (+0x2F40) resets every round; attacks can be added mid-round (the AoO pattern).
- *Decided (#51; owner, 2026-11-02): "Main decision: mostly, but allow multiple actions to be queued up, one being
  executed each passing turn. At the end of the queue, if the PC ever targeted an enemy for normal attacks, a normal
  attack should be placed as a default action to be used if there are no other actions in the queue. For the
  other, (i) on all except the last one, I think maybe everyone should run on the same rules. We should also have a
  combat configuration panel where the player can decide the default bonus actions and triggered reactions that are
  active."* - A, per-round budgets on NWN's 6 s round (one Action, one Bonus Action, one Reaction), with a queue of
  Actions, one a round, and a normal attack on the last enemy targeted as the default when the queue is empty; 1
  (i) automatic bonus attacks count as the Bonus Action; 2 (i) reactions automatic, each switchable; 3 (i) Action
  Surge / Thief's Reflexes a second round of attacks or a second spell; 4 (i) potions a Bonus Action; 5 (i)
  equipping as NWN (1.5 s, no attack lost); 6 (ii) everyone on the same rules (monsters too). A combat configuration
  panel: the default Bonus Actions and the active reactions.
- *The design's points (owner, 2026-11-02: "yes to all, no automatic triggers for now"):*
  1. The queue holds Actions (spells, items, abilities that take the Action), one a round in order (the action
     queue strip shows them). A Bonus Action clicked runs at once when the round's is free, else it waits for the
     next round (in place of that round's default Bonus Action).
  2. The default attack: the last enemy ordered attacked, while alive and in sight; when it dies, the nearest enemy
     in sight (NWN's own retarget after a kill); out of combat, nothing.
  3. Everyone on the same rules: monsters and henchmen have the same budgets and queue (their AI fills it); their
     potions are Bonus Actions, their reactions automatic; the panel's settings are the player's.
  4. The Combat options window grows into "Combat configuration": the default Bonus Action (the automatic bonus
     attack - off-hand, Martial Arts' strike, Flurry in its mode - or none, keeping it for a clicked one) and a
     switch per reaction (opportunity attacks, Uncanny Dodge, Deflect Attacks and its redirect, Retaliation). No
     automatic triggers (Rage at a fight's start, Second Wind when Bloodied) for now.
  5. The build order: Bonus Actions (#52), the Action queue and the default attack (#120), the reaction switches
     (#54), Action Surge / Thief's Reflexes (#53), the movement extras (#55); then the staple actions (#56).
- *Built 0.61.0 (#52, part 1 - Bonus Actions; key `actions`, faithful by default):* src/core/bonus_actions.cpp,
  rules/action_economy.{h,cpp}.
  - The feats that take the Bonus Action (rules::TakesBonusAction: Second Wind, Rage and its successors,
    Intimidating Presence, Patient Defense, Step of the Wind, Wholeness of Body) and those that take no action
    (rules::TakesNoAction: Reckless Attack, Superior Defense, the Brutal / Cunning / Devious Strikes and Open Hand
    Technique pickers) get ReqAction 0 in memory after every rules load (CNWFeat +0x70): instant, the round's
    attacks go on.
  - One Bonus Action a round (rules::BonusActionFree: the creature's own round - CombatRoundsEnded - or 6 s out of
    combat): a UseFeat guard lets one through while free (a listener spends it once the engine took the use); one
    more is refused and waits (the latest click), and the next round's start spends its Bonus Action on it - a
    creature update runs it (out of combat, once free).
  - The round's start (rules_two_weapon.cpp's InitializeNumberOfAttacks hook, after the off-hand count, before the
    Monk's strike): rules::DecideRoundBonus - the automatic bonus attack (the off-hand attack, Martial Arts' strike,
    Flurry of Blows) is the round's Bonus Action unless it's taken (a waiting one) or kept free (the default "none");
    an off-hand attack through Nick (weapon mastery on either hand's weapon) is part of the Attack action - kept,
    free. Left out: the off-hand attack (+0x2F54 = 0), the Monk's strike and Flurry (the engine's Flurry strike off
    +0x2F60, no Focus point).
  - Combat configuration (the Combat options window, renamed; !options): "Bonus attack" - the default Bonus Action
    (record nwn5e_bonus_default: 0 the bonus attack, 1 none).
  - Tests BonusActionOnceARound, BonusActionFeats, BonusActionRoundStart (15 of 15 mutations caught).
  - Later: potions as Bonus Actions (with the Action queue, #120 - the item path), Cunning Action and Steady Aim
    (#53), Rage extended by a Bonus Action (it extends by attacking, as before), the reaction switches (#54).
- *The owner's 0.61.0 run (2026-11-02):* "new actions aren't being added to the actions queue at the top. They still
  execute, but I can't see them. In the options having bonus actions checked doesn't do anything or does it? I still
  have to manually trigger it. This option should allow the player to select what bonus action they'd want to be
  triggered automatically." (The log: Rage instant, Intimidating Presence and Patient Defense waited for the next
  round and went, Nick's off-hand attack free.) *Decided (owner, 2026-11-02: "yes, build both as described"):*
  - the action queue strip shows a waiting Bonus Action ("next round") and one just used (a moment) - they never
    enter NWN's queue;
  - the default Bonus Action is a selector: the bonus attack, a Bonus Action ability the character has, or none;
    the ability fires at the start of each of its rounds in combat while the Bonus Action is free, skipped when it
    isn't ready (no use left, Rage while raging) or would do nothing (Second Wind, Wholeness of Body at full hit
    points) - then the bonus attack; never out of combat.
- *Built 0.61.1:* rules::DefaultBonus (record: 0 the bonus attack, 1 none, the ability's family + 2;
  rules::BonusFeatFamily - Rage's successors and Second Wind's are one), DefaultBonusPointless, PlanDefaultBonus; the
  round's start fires the ready ability as a waiting one (the creature update runs it), its held feat the highest of
  its family, "ready" the engine's remaining uses (nwn5e's pools answer) and !BarbarianRaging. Combat configuration:
  a checkbox per choice the character has (one ticked, none ticked = none). The strip (action_queue.cpp) puts the
  waiting one ("next round"; a click cancels it) and the used one (1.5 s) first. Test DefaultBonusChoice (12 of 12
  mutations caught).
- *The owner's 0.61.1 run (2026-11-02):* BA-12 "after triggering the rage, lost the target. after clicking an enemy,
  it triggered the rage again" - the log: the default Rage at a fight's first round (19:26:29), then no orders and
  no attack target, the Rage lapsing a round later (no attack), so the next round's default raged again (by the
  rules: it had ended). Patient Defense as the default kept the Monk attacking round after round; Rage's flourish
  (a PlayAnimation - #121, the animation session's) drops the attack order, and the resume (rules_potion.cpp) only
  knew a target from an attack rolled in the last 30 s - none yet in a fight's first round. *Built 0.61.2:* the
  resume follows every Bonus Action feat; with no attack rolled, the target ordered (+kCreatureAttackTarget); an
  instant one's dropped attack is given back within 3 s (a later order of the player's stands).
- *The owner's 0.61.2 run (2026-11-02):* BA-12 - the default Rage at the fight's first round (19:40:10), once; the
  attacks followed (19:40:15, 19:40:18) and kept the Rage going; no second Rage.
*Accepted (owner, 2026-11-02: "all other BA items passed"; "BA-12 passes").* Bonus Actions (#52), 0.61.0-0.61.2 -
BA-1 to BA-11, BA-13 and BA-14 on 0.61.1, BA-12 on 0.61.2. Next in the build order: the Action queue and the default
attack (#120), then the reaction switches (#54).
- *#120's recon (2026-11-02, a static read of 89.8193.37):* a player's spell, item, feat or talent order
  (CNWSMessage::HandlePlayerToServerInputMessage, minors 0x12, 0x09, 0x06, 0x23) calls CNWSCreature::ActionManager(1),
  which - while the creature's AI action state (+0x8D6) is below 10: idle, moving, attacking - runs ClearAllActions;
  ClearAction of the AttackObject deletes the combat round's schedule (RemoveAllActions, ClearAllAttacks): the rest
  of the round's attacks go, the round runs out idle, then the cast. ClearAllActions keeps an action whose
  ClearAction says no. Once a cast is queued the state is 0x0E and a new order appends (NWN's own queue of spells,
  one a round through WaitForEndOfRound and the cast round). An attack never yields to what's queued behind it
  (AIActionAttackObject doesn't read the queue; at a round's end it starts the next). A potion's order queues a 1 s
  wait (no WaitForEndOfRound) and drinks inside the running round. Move orders are ActionManager(8), attack orders
  (4): they always clear. Nothing re-issues a player's attack when its queue runs dry.
- *Built 0.62.0 (#120, part 2 - the Action queue and the default attack; `actions` faithful):*
  src/core/action_order.cpp, src/core/default_attack.cpp, rules/action_economy.{h,cpp}.
  - An order given mid-round keeps the attack (a player character; the queue's head an AttackObject, the round
    running): the order's ClearAllActions runs with ClearAction refusing the AttackObject. Its new actions
    (rules::PlaceOrder): those that wait for the round's end (a spell, a scroll, a wand) stay behind the attack - the
    round's attacks go on and at the round's end the attack yields (rules::AttackYields: AIActionAttackObject with
    no round running and a spell waiting - its own clean-up, done), the cast takes the next round, one a round as
    NWN keeps them; the others (a potion) go to the queue's head - drunk now, the attack carrying on after it.
  - An attack order on the target it's attacking, with actions waiting (AddAttackActions, clearing): those go, the
    attack and its round stay (NWN's ClearAllActions would drop the round's attacks with them).
  - The default attack (a player character's): armed by an attack order on an enemy (an AttackObject added); when
    its queue runs dry in a fight - no orders, commandable (+0xEC), alive; tried once a second - it attacks as a
    click would: the last enemy while it stands (alive, an enemy, seen), else the nearest enemy it sees
    (rules::PickDefaultAttack; rules::DefaultAttackRange: 10 m with a melee weapon, ranged_retarget_range with a
    ranged one), which becomes the remembered one. Off when the fight is over, when it dies, on a move order of the
    player's (a click on the ground - HandlePlayerToServerInputWalkToWaypoint - or the keys, InputDriveControl; not
    a combat step, which keeps attacking), on a click on its tile; on again with the next attack order. It replaces
    the potion resume in faithful mode (rules_potion.cpp keeps it for NWN mode).
  - Potions are Bonus Actions (#51's decision 4 (i); everyone): a potion's use (baseitems.2da 49, 104 -
    rules::IsPotionBaseItem; 54, the 1.69 port's third row, is deleted in EE) spends the round's Bonus Action; one
    more waits for the next round, as a Bonus Action feat does (the strip's "next round" tile; the use's order kept -
    item_use.cpp's ItemUseOrder - and given again at the round's start, before a running attack).
  - The action queue strip: a "Default attack" tile last while actions are queued and the last isn't an attack (its
    click turns the default attack off); a waiting potion's tile.
  - Monsters and henchmen: their AI gives them one action a round itself; the default attack and the kept attack are
    the player's (the input's), the potions' Bonus Action everyone's.
  - Test profile `!test queue` (a Cleric 5, three attacks a round, potions). Tests ActionQueueOrders,
    DefaultAttackTarget, DefaultAttackRangeAndPotions (18 of 18 mutations caught).
- *The owner's 0.62.0 run (2026-11-02):* the log shows AQ-1 (the round's swings 2 and 3 after Doom was ordered, then
  the cast), AQ-2 (two spells, one a round), AQ-3, AQ-6 to AQ-9 working; AQ-4 and AQ-5 ran during a cast round and out
  of combat. The owner: "when issueing the second potion command it appears at the front of the queue instead of at
  it's place / it should be [Next action] [Next turn's action] [Default action], but right now it shows as [Next
  turn's action] [Next action] [Default action]". *Built 0.62.2 (with #121's 0.62.1 fix in):* a Bonus Action waiting
  for the next round shows just after the action in progress (rules::WaitingBonusSlot: after the special attacks
  riding on it too; at the end when none is in progress); the one just used still first. Test
  WaitingBonusAfterTheActionInProgress (5 of 5 mutations caught).
- *The owner's 0.62.2 run (2026-11-02):* "check the tile, they are confirmed" - the strip's order; the log: AQ-5b (a
  potion drunk before the attack mid-round, a second one waiting), AQ-4 (the round's swings 2 and 3 after the drink),
  AQ-5 (the waiting potion cancelled by its tile, nothing drunk at the next round).
*Accepted (owner, 2026-11-02: "check the tile, they are confirmed", after "there's one issue" - the waiting tile's
place, fixed in 0.62.2).* The Action queue and the default attack (#120), 0.62.0-0.62.2 - AQ-1 to AQ-3 and AQ-6 to
AQ-9 on 0.62.0, AQ-4, AQ-5 and AQ-5b on 0.62.2. Next in the build order: the reaction switches (#54).
- *#54, how it was (2026-11-02):* one reaction a round (the round's AoO count, +0x2F40; reactions.h's TakeReaction
  gives it back after 6 s to a creature not fighting) shared by four automatic users - opportunity attacks (D2: only
  moving away provokes), Uncanny Dodge (R7: the first hit seen each round halved), Deflect Attacks (M8: the first
  B/P/S hit each round reduced; the redirect not built) and Retaliation (BZ3's stand-in: the round's AoO at whoever
  damages the Berserker in reach); none could be switched off. Faithful still lets NWN's Tumble check (DC 15) avoid
  opportunity attacks; Disengage only through the Monk's Patient Defense and Step of the Wind (M5). Slow Fall (M6):
  no NWN use.
- *Decided (#54; owner, 2026-11-02: "go with your recommendations"):*
  - The reaction switches (#51's point 4): Combat configuration's "Reactions" - one checkbox per reaction the
    character has: Opportunity attacks, Uncanny Dodge, Deflect Attacks, Retaliation; on by default; one off is
    never taken, the round's reaction kept for the others. The player's own; monsters keep all theirs.
  - 1 A: Deflect Attacks' redirect - its own checkbox, off by default (it spends a Focus Point); on, it fires at the
    attacker when Deflect brings a hit's damage to 0 (within 5 ft for a melee attack, 60 ft and seen for a ranged
    one): a Dexterity save against the Focus DC or two Martial Arts dice + the Dexterity modifier, of the attack's
    damage type; 1 Focus Point.
  - 2 A: Slow Fall left out - NWN has no falls (M6's "no NWN use").
  - 3 B: a Disengage action for everyone (the radial and the quickbar): it takes the round's Action (no attacks that
    round) and moving provokes no opportunity attacks until the next round; Tumble's check against opportunity
    attacks retired with it in faithful (NWN mode keeps it). Only Disengage comes forward from #56; the rest of #56
    waits for its own decision.
- *Built 0.63.0 (#54, part 3 - reactions and Disengage; `actions` faithful):*
  - The switches (rules::Reaction, ReactionOn / SetReactionOn: a record bit per reaction that differs from its
    default; the record nwn5e_reactions, rules_opportunity.cpp; reactions.h's ReactionAllowed - a player's switches,
    every other creature all its reactions). Combat configuration's "Reactions": Opportunity attacks (everyone),
    Uncanny Dodge (Rogue 5), Deflect Attacks and "Deflect: redirect" (Monk 3), Retaliation (Barbarian 10). Each rule
    asks before it takes the reaction: the AoO broadcast's observer (GetAIStateReaction: not an enemy to the one
    leaving, for the broadcast only), Uncanny Dodge, Deflect Attacks, Retaliation. With two on for one hit, the first
    rule takes it (Uncanny Dodge before Deflect Attacks - their install order); a switch picks.
  - Deflect's redirect (rules::DeflectRedirects / DeflectRedirectReaches / DeflectRedirectDamage /
    DeflectRedirectDamageIndex): the hit brought to 0, the switch on, a Focus point - the attacker (melee: in reach;
    ranged: 18.3 m and seen) makes a Dexterity save (FocusSave, the Focus DC) or takes 2 Martial Arts dice + Dex, of
    the attack's largest damage share - its weapon's own damage (index 12) as the weapon's baseitems.2da
    WeaponType (GetCurrentAttackWeapon).
  - Disengage (src/core/disengage.cpp): feat 1321 (data/rows/feat.actions.txt; Mobility's icon), spell 1038 ->
    n5_disengage (spells.actions.txt); every player character's (rules::FeatsEveryoneHas2024 - granted by
    rules_features.cpp, record bit 19; taken back in NWN mode); the radial's Special Attacks page, slot 1 over Called
    Shot: Arm (radial_special_attacks.cpp - while the Shove is faithful, as that page's hook is), and the quickbar
    (bit 27). ReqAction 1: NWN's cast path - mid-round behind the round's attacks (#120), then its cast round, no
    attacks. Its script: 6 s in which the movement broadcast is skipped (rules_opportunity.cpp, as Tactical Shift
    is), and the default attack off (StopDefaultAttack). Tumble against opportunity attacks fails in faithful
    (rules_skill_dcs.cpp: the broadcast's Tumble check answered -1000 while the actions and opportunity attacks are
    faithful).
  - Deflect Attacks' description (16000532) tells of the switch and the redirect; Disengage's 16000533-16000534.
  - Test profile `!test reactions` (a Monk 5 / Rogue 5). Tests ReactionSwitches, DeflectRedirect, the everyone /
    grantable feats (21 of 21 mutations caught).
- *The owner's 0.63.0 run (2026-11-02):* "RE-6 is untestable right now, RE-7 looked like it didn't proc and maybe
  some of the monk as well". The log: the switches recorded; Deflect Attacks took a hit of 4 to 0 and the redirect
  spent a Focus point - the orc's Dexterity save failed, 14 slashing back (RE-3, RE-4); Disengage twice, moving
  provoked nothing, the default attack off (DE-1, DE-2). No Uncanny Dodge line - the orc spent most rounds prone
  (Topple) and attacked with Disadvantage; no hit of its is in the log to halve. RE-7: three hits on the Berserker
  "no Retaliation: its reaction is spent this round", then "not attacking anyone" - it stood still: BZ3's stand-in
  read the engine's AoO count, refilled only as one of its rounds starts, and needed an attack under way.
- *Built 0.63.1:* the shared reaction comes back to a creature none of whose rounds runs, whoever spent it
  (rules::ReactionBackWithoutRounds' round_running: one a rule took after 6 s as before; one spent otherwise - an
  opportunity attack, or no round ran yet - while no round runs; while they run, the engine's refill). Retaliation
  takes it through TakeReaction once its other checks pass; not attacking anyone, it's given an attack order at the
  creature first (AddAttackActions), as the engine's broadcast does for an idle observer, then the AoO.
  ReactionBackWhenNotFighting (4 of 4 mutations caught).
- *The owner's 0.63.1 run (2026-11-02):* the log: RE-7 - a Berserker standing still struck back twice ("it wasn't
  attacking: it turns on it"); RE-2 - Uncanny Dodge halved a hit (7 -> 3) and Deflect Attacks found the reaction
  spent. RE-7b and RE-3 "look right" (the owner). RE-6 (the opportunity attacks switch) untested: "untestable right
  now" - no enemy left the character's reach.
*Accepted (owner, 2026-11-02: "they look right, accept 54").* The reactions and Disengage (#54), 0.63.0-0.63.1 -
RE-1 to RE-5, RE-7 and DE-1 to DE-3 (RE-6 untested). Next in the build order: Action Surge / Thief's Reflexes (#53),
the movement extras (#55); then the staple actions (#56, a decision first).
- *#56, the staple actions (the owner, 2026-11-02: "let's do something normal, go for staple actions"; asked first
  "if these actions are mainly combat or mainly RP actions. if they are combat actions do they require specific
  equipment? If they do, is it available in NWN?"):* combat with no equipment - Dodge, Dash (both built for the
  Monk, M5), Disengage (#54), Help's attack part; Help's check part - role-play, no NWN hook; Ready, dropping prone,
  dragging a downed ally - combat, a mechanism NWN lacks; Jump - exploration, no jumping in NWN; the Net - combat,
  no net item in NWN; grenades - combat, NWN's (base item 81: Acid, Alchemist's Fire, Holy Water and Caltrops have
  SRD 5.2 counterparts; Tanglefoot Bag, Thunderstone, Choking Powder, the Fire and Acid Bombs don't). The owner
  asked "can the help action be used to assist a downed henchman?" - in the 2024 rules Help stabilizes a creature at
  0 Hit Points (DC 10 Wisdom (Medicine)); nwn5e's downed henchmen make no death saves (D2: only healing gets them
  up), so that does nothing for them; a house rule could get one up instead.
- *Decided (#56; owner, 2026-11-02: "see, this is where gameplay conflicts with rule faithfulness. Let's go with B
  and have it off by default, but we'll have the possibility there anyway in case the player wants to add that
  house rule. And dodge, dash, no help for attacks and grenades in their own issue"):*
  - Dodge and Dash, everyone's Actions (the radial's Special Attacks page and the quickbar, as Disengage): Dodge -
    until the next round attacks against you have Disadvantage (an attacker you see) and Dexterity saves Advantage;
    Dash - twice as fast for the round. The Monk's (M5) pieces, for everyone.
  - Help - only the house rule (B), off by default (nwn5e.ini): on, every player character has Help - on a downed
    henchman, a DC 10 Wisdom (Medicine) check; success, it gets up at 1 HP; failure, nothing (again next round). It
    takes the Action. Not SRD: labelled a house rule. No Help for attacks.
  - Grenades as 2024 gear: their own issue. Ready, dropping prone, dragging a downed ally, the Net: later, an issue
    for them. Jump dropped (no jumping in NWN).
  - Opened: #124 (grenades), #125 (Ready, dropping prone, dragging, the Net).
- *Built 0.64.0 (#56; `actions` faithful):* src/core/staple_actions.cpp - feats 1322 Dodge, 1323 Dash, 1324 Help
  (data/rows/feat.actions.txt; spells 1039-1041 -> n5_dodge, n5_dash, n5_help), every player character's
  (rules::FeatsEveryoneHas2024 - Help only with [rules] help_house_rule=1; record bits 20-22, quickbar bits 28-30).
  Each takes the Action (ReqAction 1, the cast path: mid-round behind the round's attacks).
  - Dodge: 6 s in which rules_monk.cpp's MonkDodging answers yes (the Patient Defense Dodge: Disadvantage on attacks
    against it, Advantage on Dexterity saves, not while Incapacitated); the radial's Special Attacks slot 0 (Parry's).
  - Dash: 6 s of a movement rate bonus, the factor times two (rules::DashMovementFactor - not an effect, so the
    engine's speed cap doesn't hold it back; the grapple's drag halves it); slot 2 (Improved Power Attack's).
  - Help (the house rule): its target by script (GetSpellTargetObject); a henchman rules_dying.cpp holds down
    (IsDownHenchmanUnderRules) - a d20 + the 2024 Medicine value (NWN's Heal through GetSkillRank) against DC 10
    (rules::HelpMedicineSucceeds); success: HelpDownHenchmanUp - the next pass stands it up at 1 HP ("helped up");
    the quickbar only (the radial page has no slot left).
  - Tests StapleActions, the everyone / grantable feats (11 of 11 mutations caught).
- *The owner's 0.64.0 run (2026-11-03):* the log: ST-3 - Disengage, Dodge, Dash placed on the quickbar; ST-1 - Dodge
  from the radial (slot 0, over Parry), then an attack on the character "disadvantage ... Dodge"; ST-2 - Dash. HE-1 and
  HE-2 (Help, the house rule) untested: "very hard to test, I'll assume they work". The owner also asked for the
  radial's reorganization, later: #126 (owner, 2026-11-04: "Same for 126 as we need to have all actions done and
  ready before we reorganize it" - near the end, with #125: "125 is non essential, something to review once we're
  almost done").
*Accepted (owner, 2026-11-03: "HE-1 and 2 are very hard to test, I'll assume they work").* The staple actions (#56),
0.64.0 - Dodge and Dash for everyone, Help as an off-by-default house rule. Phase 6 left: Action Surge / Thief's
Reflexes (#53), the movement extras (#55), grenades (#124), the later staple actions (#125).
- *#53, the extra actions (F3, T5, R4, R5 - all "Phase 6" in their briefs; none built).* *Decided (#53; owner,
  2026-11-03: "go with your recommendations. but see a lot of these automatic actions should be managed by the user,
  for now in the !options panel"):*
  - 1 A Action Surge (Fighter 2; two uses from 17; back on a Short or Long Rest): an instant feat (the Fighter's
    radial section, the quickbar). Used while attacking, the round gets the Attack action again - its attacks added
    to the round now; with an Action waiting in the queue (a spell, a scroll), that Action goes now instead of after
    the round; used outside a fight, the next round started.
  - Thief's Reflexes (Thief 17: two turns in a fight's first round): automatic - the first round of each fight gets
    the Attack action twice (or a queued Action too) - switchable in Combat configuration (the owner: automatic
    actions managed by the user).
  - 3 A Cunning Action (Rogue 2): Dash and Disengage as Bonus Actions (instant; a "Cunning Action" sub-menu in the
    Rogue's radial section, the quickbar; choosable as the default Bonus Action); Hide after a recon here - with
    this build if letting stealth through in combat is one engine check, else a follow-up.
  - 4 A Steady Aim (Rogue 3): a Bonus Action refused once the Rogue moved since its round began; the next attack
    with Advantage, no moving until the round ends; also a default Bonus Action choice.
- *Built 0.65.0 (#53; class features faithful):* src/core/extra_actions.cpp; feats 1325-1329 (data/rows/feat.fighter.txt,
  feat.rogue.txt; spells 1042-1046; the radial's class sections - cls_feat_fight.txt,
  cls_feat_rog.if_classfeat2024.txt - and the quickbar, whose placement record is now 64 bits: two records,
  rules::PlanQuickbar's uint64_t), granted by rules::GrantedFeats2024 (record bits 23-27).
  - Action Surge (rules::TakesNoAction: instant): its uses a pool (rules::ActionSurgeUses; a Short Rest gives them
    back, a Long one empties it). In a running round with a living target: rules::AttacksPerAttackAction's attacks
    added now - CNWSCombatRound::AddCleaveAttack each (its recon: a 0x28-byte round action, the attack type, at the
    schedule's head, +0x2F60 counted, the round shortened to fit, then a Cleave tag - the engine's own way, as the
    Cleave mastery uses it; the log may say Cleave). Otherwise the next round started: +0x2F60 at its start
    (rules_two_weapon.cpp's InitializeNumberOfAttacks hook - ExtraActionsRoundStart).
  - Not built: Action Surge's "an Action waiting goes now". NWN's spell takes its own cast round and a cast mid-round
    ends the attack round - the Surge would lose the round's attacks, not add an Action.
  - Thief's Reflexes: the initiative (rules_initiative.cpp) marks a Thief 17's fight; its first round, within 10 s,
    gets the Attack action's attacks again (+0x2F60). Combat configuration's "Automatic actions" switches it (record
    nwn5e_thief_reflexes_off; on by default).
  - Cunning Action (rules::TakesBonusAction): Dash, Disengage as everyone's (StartDash, StartDisengage); Hide -
    CNWSCreature::SetStealthMode(1) by the server (its recon: no combat check there; the 1.69 refusal was the
    client's or the input's), Stealth rolled as hiding starts (rules_perception.cpp).
  - Steady Aim: a UseFeat guard refuses it (the Bonus Action not spent) once the Rogue moved more than 0.5 m since
    its round began (rules::SteadyAimAllowed; the round's start recorded by ExtraActionsRoundStart; no round of its
    own: not moving now); its next attack with Advantage (rules_advantage.cpp: SteadyAimRollSources, used up) and
    EffectCutsceneImmobilize for 6 s (as the grapple holds).
  - Combat configuration: Cunning Action's three and Steady Aim as default Bonus Actions (FeatReady: Steady Aim
    when not moved, Hide when not in stealth mode).
  - Tests ExtraActions, ExtraActionsEconomy, the grants' counts, the quickbar's 64-bit record (15 of 15 mutations
    caught).
- *The owner's 0.66.1 run of AS-1 / AS-2 (2026-11-03):* "AS-1 and 2 look funky, especially 2. if AS gets activated
  before combat the PC looks like it half swings the sword before each attack but only attacks once". The log: the
  pre-fight Surge's round resolved its 6 attacks in pairs 8 ms apart at 0 / 2 / 4 s. CNWSCombatRound::
  InitializeAttackActions (0x376EE0) gives a round at most three swings (2 s apart from the round's start delay);
  past three attacks a swing carries n / 3 of them (the first ones one more), all played in one animation of
  min(750 n + 500, 3000) / (3 + (n - 3) / 2) ms (n = 6: 667 ms for two attacks) - so the first is cut off. NWN's own
  look for any round with 4+ attacks (Flurry from Monk 5, two weapons from Fighter 11, Fighter 20, Thief's
  Reflexes). The mid-round Surge (AddCleaveAttack) gave each attack a whole swing, but three back to back at once,
  before the round's own (and named Cleave).
  - *Decided (owner, 2026-11-03: "go with A"):* **one swing per attack.** A round with more than three attacks gives
    each its own whole swing, spread evenly between NWN's first and last swing times (6 attacks: one every 0.8 s);
    the round lasts as long as NWN's. Action Surge's mid-round attacks join that spacing (no Cleave tag). Options
    passed over: B - only Action Surge and Thief's Reflexes get their own swings; C - NWN's doubled swings kept.
  - *Built 0.66.2 (both modes - how a round shows its attacks, not how many):* src/core/round_swings.cpp,
    src/rules/swing_spacing.{h,cpp}. The recon: StartCombatRound runs InitializeNumberOfAttacks, then
    InitializeAttackActions, which takes its swings' total (min(750 n + 500, 3000)) off the round's 6000 ms timer
    (DecrementRoundLength scales the dues down with it); a due swing pauses the round - its timer standing still -
    for its animation ((k + 1) / 2 of it for k attacks) and resolves its k attacks (ResolveAttack(target, k,
    animation)). After InitializeAttackActions, a swing carrying k > 1 attacks becomes k swings (copies made by the
    engine's operator new - the call AddCleaveAttack makes - added with CNWSCombatRound::AddAction; the engine's
    animation length kept), rules::OneSwingPerAttack giving the dues (evenly from the first, the last gap at the
    round's end) and the timer (+ the engine's total, - n swings): the round's 6 s kept (6 attacks: a 666 ms swing
    every 1 s). Action Surge in a running round: AddRoundSwings - its attacks as swings of their own (+0x2F60 counted
    as AddCleaveAttack counted them; no Cleave tag), the pending and the new due evenly from now (rules::AddedSwings:
    at least 300 ms of timer apart, the round's end moved out when they don't fit - the round stretched by their
    swings); not in a cast round. The round bar's ticks take in swings added mid-round. Tests
    SwingSpacingEngineLengths, SwingSpacingOneSwingPerAttack, SwingSpacingAddedSwings (16 of 16 mutations
    caught). Installed with it: the animation session's #128 (a flourish pauses its round at once).
  - *The owner's 0.66.2 run of AS-1b (2026-11-03):* "something happened here where the round kept going but some
    attacks were not made on time and were skipped?" The log: none skipped - all six made, the next round on time.
    The engine held the 4th swing 1.2 s past its due with the round's timer running (AIActionAttackObject not taking
    a due swing - its cause not found), so the 5th and 6th were due by then and followed back to back; the
    pre-fight Surge's 6th was held 1 s the same way. 0.66.1's 3-swing rounds show the same holds (gaps of 3.1 and
    2.9 s, then 1.6 s), hidden in NWN's 2 s gaps.
  - *Built 0.66.3:* as a swing starts (CNWSCombatRound::GetAction - the list's RemoveHead, called only by
    AIActionAttackObject), the swings behind it keep at least 300 ms of round time between them
    (rules::KeepSwingGaps: moved back, never earlier; the round's end moved out past the last when needed) - a hold
    stretches the round instead of bunching it; a swing made more than 300 ms late is logged. The round bar's ticks
    follow the swings: a made one where the round was when it was made, the ones to come where they're due now.
    Test SwingSpacingKeepGaps (7 of 7 mutations caught).
  - *The owner's 0.66.3 run of CA-3 (2026-11-03):* "after cunning action hide mid round I cannot exit stealth mode
    even after combat is done". The log: a find ("out of stealth mode (found_ends_hiding)") and the next Hide's
    "already in stealth mode" - the find's SetActionMode had done nothing. The recon: SetActionMode -> SetMode, which
    toggles only when the mode differs (stealth: byte +0x6ED) -> ToggleMode, which refuses a stealth change either
    way while the creature has an attack target (+0x724 / +0x728: feedback 0x3C), else SetActivity(stealth, on /
    off) - the stealth activity bit (+0x8E0 bit 0) and SetStealthMode. 0.65.0's Hide called SetStealthMode alone: the
    bit stayed off, and SetActivity finds nothing to turn off without it (0.39.3's snuff found the same), in a fight
    or after it.
  - *Built 0.66.4:* Cunning Action: Hide goes in by SetActivity(stealth, 1) - the engine's own way. A ToggleMode hook
    (the stealth button, a find's SetActionMode) takes a hider out even with an attack target
    (rules::StealthToggleAllowed: into stealth mode not with an attack target - NWN's, kept - out of it always): the
    activity bit set first (a hide from 0.65.0 - 0.66.3), the attack target set aside around the engine's toggle, and
    SetStealthMode(0) if the engine still leaves it on. Test ExtraActions' StealthToggleAllowed rows (3 of 3 mutations
    caught).
  - *The owner's 0.66.8 run (2026-11-03):* AS-3, CA-1, CA-2, CA-4, SA-1, SA-2, OP-1 - "everything passes" (the
    log: Action Surge mid-round, a Short Rest, Disengage and Dash as Bonus Actions, a second one waiting for the next
    round, Steady Aim); the swing rows on 0.66.3, CA-3 through #129.
  - *Accepted (owner, 2026-11-03: "everything passes").*
- *#129, attacking from hiding (a follow-up of #53's CA-3).* The owner's 0.66.4 run: the Hide worked (Stealth 15,
  then 22), but the attack in progress took the character out of stealth mode at its next swing (0.1 s and 0.5 s
  later). The recon: each swing's CNWSCreature::ResolveAttack calls ActivityManager(4) before its roll, which turns
  the stealth activity off (SetActivity) - NWN ends stealth mode as a swing starts; the 2024 Hide ends after the
  attack roll, made hidden (Advantage while the target can't see you). (0.65.0 - 0.66.3's Hide never set the
  activity bit, so this never fired.) Options: A, Hide stops your attacks (you stay hidden until you order one);
  B, the first attack from hiding rolled hidden, the hide ending right after it.
  - *Decided (owner, 2026-11-03):* "do A and B now before I continue with the matrix".
  - *Built 0.66.5:* rules::WhenAttackEndsHide (faithful: after the roll; NWN: before it). B (rules_perception.cpp,
    perception faithful): an ActivityManager hook leaves the stealth activity for an attack's call (marked left
    alone for the call, +0x8E4 bit 0, as ActivityManager reads it), and a ResolveAttack hook ends the hide by
    SetActivity(stealth, 0) once the swing's roll and damage are done. A (extra_actions.cpp): the Hide turns the
    default attack off and cancels the attack order as the action queue's click does (CNWSCreature::CancelAction:
    its round's swings go with it) - a moment later (DelayCommand -> n5_ca_hidestop), since a waiting Hide goes off
    at a round's start from inside the attack step; until then the round's swings are held (round_swings.cpp's
    GetAction puts a held swing back). Test ExtraActions' WhenAttackEndsHide rows (3 of 3 mutations caught).
  - *The owner's 0.66.5 run (2026-11-03):* "check the logs, the enemy never lost sight of me". The log: A and B
    worked (the attack stopped, the next attack "from hiding - the roll made hidden, out of stealth mode after it"),
    but no perception check came while hiding: a creature checks the hiders it sees at its perception update, every
    6 s of game time, and both hides were attacked from after about 4.5 and 5.4 s - the orc kept the sight it had.
    The recon: SetStealthMode runs DoPerceptionUpdateOnCreature(creature, hider, 1) for every creature in the area at
    once only for Hide in Plain Sight (feat 433).
  - *Built 0.66.6:* a hide (perception faithful) is checked for at once - rules_perception.cpp's SetStealthMode hook
    runs that update for every creature in the hider's area as hiding starts (after its Stealth roll), as the 2024
    Hide takes effect at once. Who still finds it (up close, a look in its cone, a search) keeps seeing it, and a
    hostile takes it out of stealth mode (F) - hiding in front of an enemy within 2 m fails at once.
  - *The owner's 0.66.6 run (2026-11-03):* "no matter where I am, even if I'm behind them and outside the passive
    perception range (blue dashed line) the enemy will still have me as seen" (HA-1, 3, 4 good). The log: the
    at-once check ran ("2 creature(s) ... check for it now") and decided nothing - no detection logged until the
    6 s update ("does not see"). The recon: DoPerceptionUpdateOnCreature skips the stealth checks for a target its
    observer both sees and hears (visibility node +4, bits 0 and 1) unless its flag holds, and keeps the flag only
    for a target with Hide in Plain Sight (HasFeat(433) at its start).
  - *Built 0.66.7:* a HasFeat filter (feats.cpp) answers Hide in Plain Sight yes for the hider, at that call site
    (DoPerceptionUpdateOnCreature's code) and only while the at-once checks run - the engine's own re-check: its
    detection (the stealth rules), its visible list and its perception events.
  - *The owner's 0.66.7 run (2026-11-03, "check the logs"):* the at-once check worked (both creatures "does not
    see / hear" as the hide began), but the attack from hiding rolled normally ("normal roll"). The recon: the
    engine's "invisible attacker" term (INVISIBILITY_ATTACK_BONUS, in GetAttackModifierVersus) asks
    CNWSCreature::GetInvisible(defender, attacker), which walks invisibility effects only - a hider never counted.
  - *Built 0.66.8:* rules_advantage.cpp adds the Hide action's Invisible condition (rules::HiddenAttackAdvantage):
    an attacker hiding (stealth mode - the hide lasts through the roll) whose target's visibility list doesn't see
    it rolls with Advantage ("hidden: attacking from hiding, unseen" in the advantage line), perception faithful.
    Test ExtraActions' HiddenAttackAdvantage rows (4 of 4 mutations caught).
  - *The owner's 0.66.8 run (2026-11-03):* HA-2/3 - the Cunning Action Hide (Stealth 16) unseen at once, the attack
    "hidden: attacking from hiding, unseen; d20 15 / 1 -> 15", out of stealth mode after it; HA-4 - the stealth
    button (Stealth 29), the first attack with Advantage (12 / 19 -> 19), then out. HA-1 on 0.66.6.
  - *Accepted (owner, 2026-11-03: "accept 129").*
- *#55, the movement extras (B9, C2, B10, OH3; Phase 6's last planned part).* NWN movement has no per-turn budget, so
  "move up to half your Speed" can't be counted; the pattern already used (Tactical Shift, Withdraw, Patient Defense):
  moving provokes no opportunity attacks for the rest of the round, no speed change. Options given: 1. Instinctive
  Pounce (built in #25 as +50% speed for the Rage's first round): A leave it, B an automatic charge; 2. Remarkable
  Athlete's move after a Critical Hit: A no opportunity attacks from moving for the rest of the round, B A plus +50%
  speed, C leave it out; 3. Forceful Blow's move toward its target: A the same as 2A, B an automatic step after it,
  C leave it out; 4. Fleet Step (Open Hand 11): A once a round, Step of the Wind right after another Bonus Action goes
  at once (its Focus point, not the round's Bonus Action), B leave it out; 5. the free Patient Defense / Step of the
  Wind (Disengage / Dash as a Bonus Action, no Focus point; Phase 6's Monk leftover): A fold it into #55, B later.
  - *Decided (owner, 2026-11-03: "go with your recommendations"):* 1A, 2A, 3A, 4A, 5A.
  - *Built 0.67.0:* disengage.cpp's StartFreeMove - Disengage's no-opportunity-attack window (6 s, the movement
    broadcast skipped: rules_opportunity's Disengaged), the attack going on (no default-attack stop). Remarkable
    Athlete: rules_fighter.cpp's after-attack-roll listener on a Critical Hit (Champion 3). Forceful Blow:
    rules_barbarian_strikes.cpp after the push. Fleet Step: bonus_actions.cpp's guard lets Step of the Wind (either
    use) through when the round's Bonus Action went to something else - the bonus attack, Flurry, a potion count -
    once a round in a slot of its own (rules::FleetStepAllows; Monk 11); the use isn't the round's Bonus Action. The
    free uses: feats 1330 "Patient Defense: Disengage" and 1331 "Step of the Wind: Dash" (data/rows/feat.monk.txt,
    spells 1047 / 1048, cls_feat_monk "+" rows, strings 16000551-16000554; n5_pd_free / n5_sotw_free run
    StartDisengage / StartDash), Bonus Actions, granted at Monk 2 (GrantableFeats2024 entries 28, 29; the quickbar's
    bits 37, 38). Instinctive Pounce as built (#25). Tests ExtraActions (Fleet Step, the grants), ExtraActionsEconomy,
    QuickbarHighBits, the grant counts (12 of 12 mutations caught).
  - *The owner's 0.67.0 run (2026-11-03):* PD-1 (Patient Defense: Disengage - no Focus point, moving provoked
    nothing), SW-1 / FS-1 (Step of the Wind: Dash and the 1-point use through Fleet Step), FS-2 (a second one waited),
    RA-1 (a crit, then no opportunity attacks), FB-1 (two Forceful Blows, the follow without opportunity attacks).
  - *Accepted (owner, 2026-11-03: "accept 55").*
- *#125, the later staple actions (Ready, dropping prone, dragging a downed ally, the Net):* deferred (owner,
  2026-11-03: "I think 125 will be deferred to way later, I don't see much value in those").
- *#124, grenades as 2024 gear.* The recon (the game's data, EE 89.8193.37): base item 81 (`grenade`) carries one
  single-use Cast Spell (iprp_spells 336-343, 528-529); spells.2da 464-471, 744-745 (Innate 3, a projectile, range M =
  20 m, Caltrops S = 8 m). DoGrenade (x0_i0_spells): a ranged touch attack for the direct hit (Acid Flask 1d6,
  Alchemist's Fire 1d6, Holy Water 2d4 vs undead only, the bombs 10d6), doubled on a crit, and always 1 point of
  splash to the hostiles within 6.67 m; no save. Tanglefoot Bag (x0_s3_tangle): one target, Reflex DC 15 or entangled
  2 rounds. Thunderstone (x0_s3_thunder): 3.33 m, Fort DC 15 or deafened 5 rounds. Choking Powder (x0_s3_choke): a
  5-round cloud (6.7 m), Fort vs the spell DC or dazed 2 rounds. Caltrops (x0_s3_caltrop): a permanent 5 m circle,
  1 piercing on entering and each round, no save, gone after 25 damage. The bombs (x2_s3_bomb) add a 5-round cloud:
  fire 4d6 a round (Reflex half) or acid 4d6 on entering and a permanent slow. Blueprints x1_wmgrenade001-007,
  x2_it_acidbomb / firebomb; Drow mephits carry Tanglefoot Bags; traps and Craft Trap use them. The 2024 gear
  (SRD 5.2): Acid, Alchemist's Fire, Holy Water - replace one attack of the Attack action; one creature within
  20 ft, a Dexterity save (DC 8 + Dex + PB): 2d6 Acid; 1d4 Fire and burning (1d4 Fire at the start of each of its
  turns until it puts it out); 2d8 Radiant to a Fiend or Undead. Caltrops - a Utilize action, a 5-ft square within
  5 ft; entering: a DC 15 Dexterity save or 1 Piercing and Speed 0 until its next turn.
  Options: 1 the throw's cost - A 2024 (mid-round it goes at once and takes one of the round's remaining swings),
  B the Action as now; 2 the flasks' hit - A 2024 (the Dex save, one target, 2024's damage; Holy Water on Undead and
  evil Outsiders; NWN's 20 m range kept), B A plus NWN's 1-point splash, C NWN's; 3 burning - A 3 rounds for
  everyone (1d4 Fire at the start of each of its rounds - the action that puts it out, which NWN's AI never takes),
  B until put out (players' Extinguish action, monsters 3 rounds at most), C 1 minute; 4 Caltrops - A NWN's throw
  and 5 m circle, 2024's effect (the save on entering, 1 Piercing and stopped until its next round), NWN's 25-damage
  life, B 2024 exactly (a 1.5 m patch), C NWN's; 5 the five without a 2024 counterpart - A kept in 2024 terms (the
  flasks' save in place of the touch attack and DC 15; Tanglefoot Restrained, Thunderstone Deafened, Choking Powder
  Incapacitated; the bombs 10d6, Dex half), B NWN's exactly, C NWN mode only.
  - *Decided (owner, 2026-11-04: "good recs, go with them"):* 1A, 2A, 3A, 4A, 5A.
  - *Built 0.68.0 (key `grenades`, faithful by default):* src/core/grenade_gear.cpp, src/rules/grenades.{h,cpp}.
    - The flasks and the bombs run in C++ in place of NWN's scripts (script_hooks.cpp by name: x0_s3_acid,
      x0_s3_alchem, x0_s3_holy, x2_s3_bomb): the spell target's Dexterity save on the thrower's DC
      (rules::GrenadeSaveDc), 2024's damage (rules::GrenadeHitDice), no splash; Holy Water on undead and evil
      outsiders only; Alchemist's Fire's burning (VFX_DUR_INFERNO_CHEST, 1d4 Fire at 1, 2 and 3 rounds) and NWN's
      coating of a melee weapon; the bombs GetReflexAdjustedDamage (Evasion) then NWN's 5-round clouds.
    - Caltrops: NWN's throw and field (x0_s3_caltrop), x0_s3_calen in C++ - a hostile entering, a DC 15 Dexterity save
      or 1 Piercing and EffectCutsceneImmobilize 6 s; x0_s3_calhb nothing; NW_L_TOTAL_DAMAGE to 25 as NWN's.
    - rules_save_roll.cpp: a grenade's save (rules::GrenadeSaveOnThrowerDc: all but Caltrops) against its thrower is
      on the thrower's DC - Tanglefoot Bag's and Thunderstone's fixed 15 replaced; SpellDc2024 answers the clouds'
      GetSpellSaveDC (Choking Powder, the bombs') the same way.
    - 1A: the #120 ordering (action_order.cpp's PlaceNewOrder) puts a player's grenade cast (an ItemCastSpell of a
      base item 81) before its attack mid-round - the engine's AIActionItemCastSpell starts no cast round and no pause
      while a round runs (its StartCombatRoundCast only without one) - its WaitForEndOfRound left behind the attack
      and not counted as an Action waiting; round_swings.cpp's TakeRoundSwing frees the round's last pending swing
      (CExoLinkedListInternal::Remove; the engine's operator delete, StartCombatRound's) or owes one to the next round
      (taken after InitializeAttackActions).
    - The descriptions in faithful mode: nwn5e's strings 16000555-16000563 for dialog.tlk's 2989-2997 and 84353-84354
      (rules::GrenadeDescription2024, rules_labels.cpp's swap).
    - Tests GrenadeSpells, GrenadeRules, GrenadeDescriptions (17 of 17 mutations caught).
  - *The owner's 0.69.0 run (2026-11-04):* "the preview area for the bombs and flasks are the same and they are huge" -
    spells.2da's aiming circle (EE's TargetShape / TargetSizeX / TargetFlags) is NWN's splash, 6.67 m for every flask
    and both bombs. *0.69.1:* data/rows/spells.if_grenades2024.txt, served with the grenades faithful
    (ServeConditionalRows "grenades2024"): Acid, Alchemist's Fire, Holy Water no circle on a creature (TargetFlags 35:
    EE's 32, "suppress with target", as Dispel Magic's) and one creature wide at the ground (0.83 m, #130's patch);
    Tanglefoot Bag none; the bombs 5 m (their cloud); Choking Powder, Thunderstone and Caltrops the game's.
  - *The owner's 0.69.1 run (2026-11-04):* "I'm having a bit of an issue with the hazards not affecting the PC ... The
    acid flask doesn't show an area of effect preview but it clearly has a pretty big splashzone ... The firebomb area
    is huge and bright but it doesn't emit any light ... Why does throwing a flask while hidden count as entering
    combat?" The log: no "grenades: 0x..." line in the 0.69.0 or the 0.69.1 run - none of #124's handlers ever ran; the
    game's chat shows NWN's ranged touch attack and 1-point splash (a desk, an invisible object). Cause:
    script_hooks.cpp's lookup took only names starting "n5" (every handler before #124 was nwn5e's own), so x0_s3_acid,
    x0_s3_alchem, x0_s3_holy, x2_s3_bomb, x0_s3_calen and x0_s3_calhb ran as NWN's: #124 and #130 untested so far.
    *0.69.2:* the lookup is rules::FindScriptName (any registered name, case-blind; test ScriptNames, 4 of 4 mutations
    caught), and AddScriptHandler refuses a name its hook's lookup wouldn't find (logged "SKIP nwn5e's script ...").
    Hiding: it ended as each throw started (09:23:20, 09:23:49) - the engine's, no nwn5e line.
  - *Decided (owner, 2026-11-04: "go with your recommendations"), after the 0.69.1 run:* every grenade hazard hurts
    anyone in it, the thrower and its henchmen too - the flask patches (#130's 4B replaced), Caltrops' field, the bombs'
    clouds, Choking Powder's cloud (henchmen keeping out of them: #131, later); an orange light where the fire burns,
    the Fire Bomb's cloud and an Alchemist's Fire patch; a grenade thrown from hiding keeps the hide (#132 A).
  - *Built 0.70.0 (with 0.69.2's fix):*
    - grenade_gear.cpp: the patches and Caltrops (x0_s3_calen) hurt any creature. The bombs' clouds run nwn5e's own
      scripts (EffectAreaOfEffect(AOE_PER_FOGFIRE / FOGACID, n5_bfire_in / _rd, n5_bacid_in / _rd / _out)): NWN's
      Incendiary Cloud (4d6 Fire entering and each round - creatures, doors, placeables - Dexterity half on the
      thrower's DC) and Acid Fog (entering, 4d6 Acid and a Constitution save or slowed while in it, tagged
      n5_bacid_slow and removed on leaving; 2d6 Acid each round), to anyone, no spell resistance; the Acid Fog and
      Incendiary Cloud spells keep NWN's scripts. Choking Powder's own x0_s3_chokeen / x0_s3_chokehb in C++: anyone, a
      Constitution save or dazed 2 rounds (poison immunity as NWN's).
    - Light: an invisible object (plc_invisobj, tag nwn5e_fire_light) with VFX_DUR_LIGHT_ORANGE_20 at the Fire Bomb's
      cloud for 5 rounds, _10 at an Alchemist's Fire patch for 2. light_sources.cpp's LoadPlaceable hook (now also
      with the grenades faithful) loads it as a normal placeable while CreatingFireLight is on - not static (a static
      one a script creates never reaches the client), not clickable, plot. The hiding-in-light rule (stealth_watch.cpp)
      counts VFX_DUR_LIGHT_* on placeables: bright within half its radius, dim within it.
    - #132: rules::ThrowKeepsHiding; rules_perception.cpp hooks CNWSCreature::AIActionItemCastSpell (a grenade's throw
      marked while it runs: IsGrenadeCast), and #129's ActivityManager hook leaves the stealth activity alone for its
      ActivityManager(1) (+0x8E4 bit 0; the combat modes still turned off) - logged "throws a grenade from hiding -
      still hidden".
    - The descriptions: Caltrops, Choking Powder, the bombs and the patches say "anyone in it, you too"; the fire ones'
      light.
    - Test ThrowFromHiding (3 of 3 mutations caught).
  - *Asked (owner, 2026-11-04, after 0.70.0's install):* "one more thing, bombs (poison and fire) along with
    thunderstone should tunr off torches and other such light sources in range of the explosion as they blow up".
    *Built 0.71.0* (the defaults stated to the owner: each blast's own radius; what nwn5e can already put out; carried
    torches stay lit): rules::GrenadeBlastRadius - the bombs 5 m (their cloud), the Thunderstone 3.33 m (its bang);
    light_sources.cpp's BlastPutsOutLights - the lit placeables (lamps, braziers, torch brackets) and the tiles'
    torches (their stand-ins) round the spell's target put out as a hit does, the fill lights recomputed, "nwn5e: the
    blast puts out N lights" to the thrower; with light_sources on only. The Thunderstone's x0_s3_thunder in C++ for
    it (NWN's: non-friends within 3.33 m, a Constitution save on the thrower's DC or deafened 5 rounds). Test
    GrenadeBlasts (4 of 4 mutations caught).
  - *The owner's 0.71.0 run (2026-11-04: "alchemist's fire on a weapon should also make it a light source. check the
    logs"):* the Acid Bomb's and the Thunderstone's blasts put out 2 candelabras each (relit by Use); the Acid Bomb's
    fog on the thrower walking in - Constitution save 16 vs DC 14, 19 Acid entering, 4 its round; the Fire Bomb thrown
    from hiding ("still hidden"), its light read by the hiding rule (darkness -> dim -> in light walking up, darkness
    again as the cloud ended), its cloud Dexterity 18 vs 14, 7 Fire; an Acid Flask at the ground left its patch; at a
    spawned orc its Dexterity save (19 vs 14), the thrower still hidden while the orc searched; Alchemist's Fire on
    the longsword: 5 and 2 Fire on hits. Found: #132's log line came every tick of the cast (ActivityManager(1) each
    AI update - 300+ lines, the stealth log's budget); the thrower's own cloud signalled it as attacked (an Initiative
    roll). *0.71.1:* the coating adds ItemPropertyLight (low, orange: 10 m) for its 4 rounds - the wielder's light
    effect, counted as carried by the hiding rule; #132's line once a throw (3 s per thrower); a hazard (patch, cloud,
    Caltrops) signals only creatures not friendly to its thrower - the thrower and its friends still take its damage.
  - *The owner's 0.72.0 / 0.72.1 runs:* GR-2 (1d4 Fire, then a burning tick of 1), GR-4 (Holy Water: the skeleton 8
    Divine, the orc unharmed), GR-16 ("you carry a light (10 m) - enemies spot you at +4"), GR-9 in part (the thrower
    made its own Caltrops' DC 15 save twice), GR-1 (a failed save, 9 Acid, no splash); #132's HD-1 and HD-2.
*Accepted (owner, 2026-11-04: "you can accept all of those").* #124 (grenades as 2024 gear, 0.68.0 - 0.72.1) and #132 (a
throw from hiding keeps the hide). Left untested: GR-5 (a throw mid-round takes a swing), GR-6 - GR-8 (Tanglefoot Bag,
the Thunderstone's deafening, Choking Powder), a failed Caltrops save, the Fire Bomb's hit on a creature (GR-10),
GR-12 (the aiming circles), GR-13 (the descriptions).
- *#130, house rules; flasks thrown at the ground leave a patch (the 2024 Oil flask's rule).* Asked (owner, 2026-11-04,
  after #124's brief): "Ok, I want to add a house rules section. For example we'll keep the faithful rules, the ones
  you added. But I also want acid flask, alchemist fire and holy water to work with oil flask rules where they create a
  hazard area when thrown in the floor". Oil (SRD 5.2): poured on the ground it covers a 5-ft square; lit, it burns 2
  rounds - 5 Fire damage to a creature that enters it or ends its turn there, at most once a turn. The section (the
  plan): nwn5e.ini's `[house_rules]`, each rule off by default, Help's key moved there ("House rules", below).
  Options: 1 the size - A one creature's space (a 5-ft square), B NWN's small radius (1.67 m); 2 the damage - A Oil's
  flat 5 of the flask's type, B the flask's dice each time; 3 a save - A none (Oil's), B a Dexterity save for none;
  4 whom it hurts - A anyone (Oil's), B NWN's area-spell rule (allies only above Normal difficulty, as the Bombs'
  clouds).
  - *Decided (owner, 2026-11-04: "1A (later we'll check if it's too small), 2B, 3A, 4B"; then "go ahead with the code
    we'll test them all together" - with #124's 0.68.0):* 1A, 2B, 3A, 4B.
  - *Built 0.69.0 ([house_rules] flask_patches, off by default; with the grenades faithful):*
    - rules::FlaskMakesPatch (Acid, Alchemist's Fire, Holy Water; the house rule on and the grenades faithful),
      kFlaskPatchRounds 2, kFlaskPatchLockSeconds 5 (once a round, under the 6-s tick), FlaskPatchAreaEffect -
      vfx_persistent rows 100-102 (data/rows/vfx_persistent.flasks.txt): a 0.83 m radius; one actor of Acid Fog's fog,
      of Wall of Fire's flames; Holy Water the Light spell's glow (VFX_DUR_LIGHT).
    - grenade_gear.cpp: a flask at the ground (no creature, door or placeable at the target) - its splash visual, then
      EffectAreaOfEffect(row, n5_fp<flask>_in, n5_fp<flask>_rd) for 2 rounds. The patch's scripts in C++ (kPatch):
      entering, the creature; its round tick (6 s), everyone in it - the flask's dice (2B), no save (3A), a local on
      the creature for 5 s (once a round). Whom: x0_i0_spells' spellsIsTarget (SPELL_TARGET_STANDARDHOSTILE) written
      out (4B) - hostiles always; the thrower, its henchmen and neutrals only above Normal difficulty; the module's
      switches X0_G_ALLOWSPELLSTOHURT, X2_SWITCH_ENABLE_NPC_AOE_HURT_ALLIES, X2_SWITCH_MULTI_HENCH_AOE_MADNESS. Holy
      Water's only undead and evil outsiders.
    - The three flasks' descriptions with the house rule on: 16000564-16000566 (rules::GrenadeDescription2024's
      flask_patches). The grenades' 2024 descriptions (16000555-16000563) lose dialog.tlk's opening sentence (the
      game's own text, not ours to copy).
    - config: [house_rules] help (Help's, #56 - [rules] help_house_rule still read) and flask_patches; "config: house
      rules" in the log. The test switch flask_patches (0..1): `!test grenades` turns it on, ten of each flask.
    - Test FlaskPatches (11 of 11 mutations caught).
  - *0.70.0 (owner, 2026-11-04: "go with your recommendations"; #124's 0.70.0 note):* the patches hurt anyone in them,
    the thrower and its henchmen too (4B replaced); an Alchemist's Fire patch gives light while it burns.
*Accepted (owner, 2026-11-04: "you can accept all of those").* #130 (the house-rules section; flask patches). Seen: an Acid
Flask at the ground left its patch (HR-1's patch, 0.71.0). Left untested: HR-1's creature walking in, HR-2 - HR-8.
- *#133, a creature that only hears a hider (owner, 2026-11-04, the 0.71.0 run: "the enemy, after the throw had a
  "heard" icon over them but they were tracking them as if they had seen me").* The log: the orc hit by an Acid Flask
  from hiding attacked the thrower a second later, before it had perceived it at all (NWN's AI answers a spell cast at
  it by attacking the caster); then, hearing it (Stealth 6), NWN's AI attacked its exact spot, each roll at
  Disadvantage ("target unseen") - the 2024 rule for a creature you hear but can't see, but NWN's hearing never
  guesses wrong. Options: A as now; B a creature that only hears a hider goes to where it heard it and searches,
  attacking once it sees it (a grenade's target likewise); C B for the throw only.
  - *Decided (owner, 2026-11-04: "go with B"):* B, faithful.
  - *Built 0.72.0:* rules::UnseenHiderSearched; rules_perception.cpp's Detection keeps where each creature last heard
    each hider (its listen check heard a hiding target) and UnseenHiderSearch - an AI creature's AIActionAttackObject
    on a hiding target it doesn't see (engine::Sees: its visibility list's seen bit) is dropped (ClearAction, done) and
    a search queued by DelayCommand (search mode, run to the last heard spot - else the hider's spot as it attacks, the
    throw's origin - look left and right, wait 3 s); once a second at most, logged "would attack ... unseen - it
    searches". action_order.cpp's AIActionAttackObject hook asks it first, installed with actions or perception
    faithful. NWN's AI asks again each combat round, so it keeps searching until it sees the hider (found ends the
    hide, #129, and the attack goes ahead). Spells cast at a hider aren't changed. Test ExtraActions' stealth checks (4 of 4 mutations caught).
  - *The owner's 0.72.1 run:* HS-1 - three Acid Flasks from hiding, the orc's attacks dropped for searches ("where it
    was hit from", then "where it last heard it", 0.9 m from the hider), no attack on the hider while it hid; HS-4 - not
    hidden, the orc attacked as before. 0.72.1 also: the test hit point floor ([debug] creature_min_hp) before a save
    against a character and at !spawn (the 0.72.0 run's orc died of one flask at its own 4 hit points).
*Accepted (owner, 2026-11-04: "you can accept all of those").* #133. Left untested: HS-2 (a longer move away), HS-3 (found up
close, then attacked).
- *#136, a creature harmed by a hider it can't see (owner, 2026-11-05: "if an enemy is harmed by a player action
  (thrown grenade for example), but they don't have any player or henchmnen in LoS, does combat start?").* NWN's
  OnDamaged (nw_c2_default6) sends a creature that can't see its damager to the damager's exact location
  (ActionMoveToLocation(GetLocation(oDamager))) - #133's leak, by the damage path. Options: A route that move
  through #133's rule (where the hider was heard, a search near it); B A, and the creatures unaware of the hider
  Surprised when its attack starts combat (SRD 5.2: Disadvantage on their Initiative roll).
  - *Decided (owner, 2026-11-06: "Go with your recommendations for 1 and 4."):* B, faithful.
  - *Built 0.81.0:* rules::MoveOntoUnseenHider (#133's rule and the move ending within kUnseenHiderMoveReach, 1 m,
    of the hider) and SurprisedAtInitiative (test ExtraActions' stealth checks, 8 of 8 mutations caught).
    - rules_perception.cpp hooks CNWSCreature::AddMoveToPointAction for a script's ActionMoveToLocation only
      (ExecuteCommandMoveToPoint's one call, by its return address; the Vector by pointer): an AI creature's move
      onto a hiding enemy it doesn't see (the nearest within 1 m of the destination) is dropped for #133's search,
      now shared (SearchForUnseenHider: where it last heard the hider, else where the hider is - the hit's origin;
      once a second at most; logged "would run to ... unseen - it searches"). The search's own move (its spot, the
      last 3 s) goes through.
    - rules_initiative.cpp: Surprise - an Initiative d20 with Disadvantage when the creature sees no enemy (alive,
      in its area) while one is hidden from it (HiddenByStealth); logged for any creature ("Surprised - sees no
      enemy, one hidden from it"), with the player's roll line's reasons.
  *Accepted (owner, 2026-11-06: "HD and LR seem fine and accepted").* #136.
- *#131, creatures keep out of hazards (owner, 2026-11-04, on #130's 0.69.1 run: "We might need to eventually work on
  the henchman AI to prevent them from standing in hazard areas").* How it was: the grenade hazards hurt anyone (0.70.0);
  NWN's henchman AI (nw_ch_acb) answers only an enemy's area spell (it may flee or dispel) and ignores its master's on
  purpose ("if we singe some eyebrow hair, oh well"). Options: 1 who - A a player's associates, B every AI creature;
  2 which - A the grenade hazards, B also the harmful spell areas where they'd hurt it; 3 what - A step out, B step out
  and keep out (a melee attack on a target in one waits), C B and no following into one.
  - *Decided (owner, 2026-11-04: "1. hmmmmmm, I'm not sure if we're entering some sort of deeper game mechanics but I
    feel that most living enemies would avoid hazards, while undead and enemies that are charmed or somehow dominated
    would not / 2 B / 3 B"):* every living AI creature - monsters and a player's associates; not undead or constructs,
    not the charmed, dominated or confused; never a player's character; 2B; 3B.
  - *Built 0.73.0 (with the grenades faithful):* rules::HazardOfScript / HazardStepOutMetres (the hazards by their
    scripts: #130's patches, the Bombs' clouds, Caltrops, Choking Powder - anyone, Holy Water's undead and evil
    outsiders; Acid Fog, Incendiary Cloud, Stinking Cloud, Cloudkill, Wall of Fire, Blade Barrier, Web, Entangle,
    Grease, Creeping Doom, Evard's Black Tentacles, Storm of Vengeance, Cloud of Bewilderment, Stonehold, Ghoul Touch -
    whom NWN's area-spell rule picks; the radius + 1.5 m), AvoidsHazards, HazardHoldsAttack (test HazardAvoidance,
    14 of 14 mutations caught). script_hooks.cpp: AddScriptObserver - run after a script, the game's or a handler.
    hazard_watch.cpp: after each hazard script, the creatures in it listed by a script run as it
    (GetFirstInPersistentObject; spellsIsTarget written out for the spells' areas), kept by hazard; an avoiding
    creature it hurts steps out (ClearAllActions, ActionMoveAwayFromLocation(its centre, running, radius + 1.5 m)),
    once in 4 s; action_order.cpp's AIActionAttackObject hook drops an avoiding creature's melee attack on a target
    standing in a hazard that would hurt it while it stands outside (a spell's area: its caster's enemy; HoldsRangedWeapon:
    a ranged attack goes on). Charmed (state 1), confused (2), dominated (7, or a dominated associate) are not in control.
  - *The owner's 0.73.0 run (2026-11-04: "well, the henchmen will need to be made stronger, they can't deal with a
    firebomb"):* the `!spawn nw_orcb ally` henchman, a level 1 orc, kept its 4 hit points (the test floor skipped it: a
    henchman counts as a character), failed its save in the Fire Bomb's cloud (18 Fire) and went down; the step-out
    was ordered each round ("steps out of 0x...") but a downed henchman can't move. *0.73.1:* the floor for a !spawn
    ally too; a downed henchman isn't told to step out. Henchmen's toughness against the party's area damage: its own
    issue (later).
  - *The owner's 0.73.1 run:* HZ-1 (the ally orc, 4 -> 50 hit points, stepped out of the Fire Bomb's cloud), HZ-2 (a
    hostile orc stepped out, twice), HZ-3 (a skeleton stayed), HZ-4 (a greataxe orc held its attack on the player
    character standing in Caltrops three times, and stepped out of the cloud and the field), HZ-6 (the ally stepped out
    of Caltrops); another creature in the room held its attack on the orc in the cloud. Henchmen's toughness: #134, dropped (owner,
    2026-11-04: "134 was just for testing" - the test henchman's 4 hit points; 0.73.1's floor).
*Accepted (owner, 2026-11-04: "I think the HZ matrix is good").* #131.

### Phase 7 — Interface
- Character creation and level-up screens for the 2024 flow (background,
  species, subclass at 3, feat choices, no skill points).
- Character sheet: six saves, proficiency bonus, spell DC/attack.
- Spellbook: prepared list + slots instead of memorised slots.
- Combat log strings (custom `.tlk`): advantage/disadvantage, saves, mastery.
- Recon 2026-10-19: docs/UI-RECON.md — panels are models + `.gui` layouts +
  hardcoded client code; every sheet number is a server value; routes A–F
  (2da, tlk, art, NWScript, server hooks, client hooks) with EE portability,
  and the cheapest route per item above.
- *#140, radial casting (owner, 2026-11-06: "I have an issue with spells picked from the radial menu not showing
  the area of effect of a spell ... we're targeting blind without kn owing how big the area is, how many friends
  and/or foes it might hit ...").* NWN casts a radial spell at once at what the radial was opened on (a creature,
  an object, a spot); the quickbar enters EE's targeting mode, which shows a spell's area (spells.2da's
  TargetShape: 114 of 362 spells have one; HostileSetting marks 150 harmful). The owner's rules: on an enemy a
  single-target spell is cast at once, an area spell retargets with its area shown; on the ground or scenery
  every spell retargets; on yourself a helpful spell is yours, a harmful or neutral one retargets. Edge cases put:
  allies, neutral creatures (as enemies), self-only and caster-centred spells (at once), cones and lines (they
  retarget), objects (as creatures), variants and the upcast menu (after the last pick), items (the same), feat
  abilities, a cancelled cursor (nothing spent), range (NWN walks), quickbar assignment (unchanged), spells with no
  shape (single until #39), a controlled creature (the same), the AI (unaffected).
  - *Decided (owner, 2026-11-06: "Q1. right Q2. b Q3. a Q4. before"):* allies - a helpful single-target spell is
    cast on the ally, a harmful one or an area spell retargets; a helpful area spell opened on yourself
    retargets too (every area spell shows its area); feat abilities with an area follow the same rules; built
    before #39's build 2.
  - *Built (0.89.0, 2026-11-06):* `radial_retarget.cpp` hooks the radial's four leaves - a spell (the cast
    callback #36 maps, so the upcast menu's "Level N" too), a spell-like ability, the spell menu's feat and the
    creature menu's RadialMenuUseFeat (class abilities, special attacks). When `rules::RadialRetargets` says so it
    starts the quickbar's own cursor instead of the cast: the spell cursor with the same param (class, upcast
    level, metamagic carry over) or the feat cursor, both showing TargetShape's area; Esc frees it, nothing spent.
    What the menu was opened on: yourself, an ally (GetAIStateReaction friend), another creature, a door, a
    placeable or an item, or the ground (anything else). Two refinements of the table: opened on yourself, a spell
    that can't target its caster (TargetType without the self bit, e.g. the Help house rule's) retargets; a class
    ability that isn't self-only, helpful and single-target, opened on yourself is used on you (the game gave it
    the cursor). A feat's spell is feat.2da SPELLID; TARGETSELF feats are used at once. Items aren't cast from the
    radial (the inventory and the quickbar already show the cursor). `radial_retarget` (nwn5e.ini). Tests:
    RadialSpellFromRow, RadialRetargets, CreatureMenuFeatParams.
  - *Fixed (0.89.1, 2026-11-07; owner's test of 0.89.0: "The radial menu behaviour doesn't seem to have changed at
    all", RT-4 "is greyed out in the radial menu", RT-6 "cannot select hold person if radial menu is on PC"):*
    the leaves read the controlled creature through the global once, not twice as the game does - no creature, so
    every pick fell back to the game's cast; and the spell menu greys a spell its target can't take (0x9A4910,
    TargetType against the target), so one the rules hand the cursor is kept pickable now. The Cleric's
    spontaneous cure / inflict leaf follows the rules too; each pick writes a log line.
  - *Fixed (0.89.2, 2026-11-07; owner's test of 0.89.1: "RT-1 doesn't cast at once, asks for retarget with cursor
    / RT-7 cure doesn't cast at once / RT-9 fire bolt doesn't cast at once"):* the radial's target is the client's
    id, the server's with bit 31 set (the log: Hold Person "on the ground (0x8000031a)", the orc being 0x31A);
    looked up on the server as is it found no object, so every creature counted as the ground. rules::ServerObjectId.
  - *Accepted (owner, 2026-11-07: "accepted, 2 b, 3 it already does"):* 0.89.2's rows pass (the log agreed:
    single-target spells on a creature at once, area spells, harmful spells on an ally and ground picks the cursor).
    The two picks: a self-only spell (Shield) on another creature's radial stays greyed, as the game has it (2 b);
    Esc already cancels the cursor (3).

### Phase 8 — Creatures and campaign balance
- Bounded accuracy breaks against 3e stat blocks (HotU creatures with AC 30+).
  Convert creatures at spawn from their 3e blueprint to a 5e-style stat block
  (CR-based AC/HP/attack/DC) with hand-tuned overrides for named creatures.
- Campaign XP and the level cap: OC/SoU/HotU expect up to level 40.

### Phase 9 — Balance and polish
- Play-through of OC chapter 1, SoU, HotU openers; tuning tables.

## Open decisions (owner)

Still open (2026-10-29) - 2, 6 and 7, tracked as issues #22, #38 and #51. 1, 3,
4, 5 and 12 were answered by "Phase 3 — character model" (2026-10-19; CM1,
CM3, CM6, CM10).

1. *(Answered, CM6.)* Half-elf and half-orc are not 2024 species — keep as legacy, or replace?
2. New species (dragonborn, tiefling, goliath, aasimar, orc) — appearance work needed. (#22)
3. *(Answered, CM1 / CM7.)* Epic levels: cap at 20 with Epic Boons, or a custom 21+ track for HotU?
4. *(Answered, CM3.)* Hit points per level: fixed average or rolled?
5. *(Answered, CM10.)* Prestige classes with no 2024 subclass equivalent: drop or adapt?
6. *(Answered 2026-11-06, #38: no Rituals for now; costly material components paid in gold.)* Rituals and
   material components: model or ignore?
7. Action economy design (Phase 6). (#51)
12. *(Answered, CM10.)* Subclass selection UI: prestige classes become 2024 subclasses (decision
    10). Proposed: pick the subclass at level 3 in the existing level-up feat
    step (one "subclass" feat per option, custom names/descriptions/icons),
    grant subclass features at their levels from the DLL, and hide prestige
    classes from the class list. A dedicated subclass screen would need the
    client's level-up UI code reworked — not planned.

## Phase 1b — advantage/disadvantage (decided by the owner, 2026-10-18)

Proposed the same day; the owner took every recommendation except B5, where
the 2024 rule was chosen over NWN's check.

**How.** NWN adds situational bonuses as flat 3e numbers inside
GetAttackModifierVersus (all of them decoded, RE-NOTES "situational terms").
ResolveAttackRoll rolls its d20 (call at 0x744401, `RollDice(1, 20)`
0x47E120) *before* it asks for the attack modifier, so:
1. ResolveAttackRoll hook (already hooked for the trace), at entry: work out
   which situational terms apply to this attacker and target (the engine's
   own checks), keep their 3e total (`rules::NwnSituationalTotal`) and the
   roll mode (`rules::SituationalRollMode`, with the B5 close-combat check)
   for this attack.
2. The d20 call at 0x744401 goes to a function that rolls two dice and keeps
   the higher or lower (`rules::RollD20`, the engine's own RollDice for each
   die). The kept die is the natural roll for natural 1 / 20.
3. The GetAttackModifierVersus hook takes the 3e total back out for that
   attack.
The parry roll (0x74465E) and the threat roll (0x744707) are untouched.

**B1. Mapping** (decided as proposed; `src/rules/nwn_situational.cpp`):

| NWN term (3e) | 2024 |
|---|---|
| Target blind (+2), attacker invisible to the target (+2) | Advantage |
| Target stunned (+2) | Advantage |
| Target prone: melee +4 / ranged −4 | Advantage in melee, Disadvantage at range |
| Can't see the target (−4) | Disadvantage |
| Ranged attack within the target's melee reach (−4) | removed; see B5 |
| Moving target (−2, ranged) | nothing (no 2024 rule) |
| Target flanked (+2) | see B2 |

**B2. Flanking: off.** Core 2024 has no flanking; the DMG's optional rule
(Advantage) is not used — with a henchman almost every melee attack would
flank, making Advantage the norm. NWN's +2 is removed. (Sneak Attack's own
flanking check is separate and unchanged.)

**B3. Point Blank Shot.** The NWN feat removes the −4 for shooting in melee
and adds +1 within 5 m; 2024 has no such feat. Until feats are reworked
(Phase 3): it removes the close-combat Disadvantage (as 2024 Sharpshooter
and Crossbow Expert do) and keeps its +1.

**B4. Every attacker** — party, henchmen, creatures, summons — as in 2024.

**B5. Close combat: the 2024 rule.** A ranged attack has Disadvantage when
any enemy of the attacker that is within its own melee reach of the
attacker can see the attacker and is not incapacitated (dead, dying,
helpless) — not only the target. NWN's target-only −4 is still taken out of
the engine's number.

**B6. Combat log.** It shows one d20 ("17 + 2 = 19"): the kept die;
nwn5e.log records both dice and the mode. An "Advantage" label in the log
text is Phase 7 (interface).

**B7. Crits against players** (taken with "your recommendation for all"):
NWN turns a creature's critical hit on a player-controlled character into a
plain hit (ResolveAttackRoll's crit path, 0x600100 — likely a difficulty
rule); 2024 has no such rule, so it is removed, behind its own switch.

Later sources, as each NWN effect is converted: conditions (Restrained,
Paralyzed, Poisoned, Frightened…), hidden attackers, Reckless Attack, Help,
spells (Faerie Fire, Bless's d4 is not adv/disadv).

## Modes: faithful and NWN (owner, 2026-10-18)

Every rule decided from here on has two versions: **faithful** (D&D 2024 as
written) and **NWN mode** (2024, with NWN's own choice kept where it exists
for gameplay — NWN is a video game, and long playtests aren't planned). One
line in nwn5e.ini picks the default for all of them — `mode=faithful`
(default) or `mode=nwn` — and each rule's own key can still override it.
"NWN mode" is not the original 3e game: that stays available by switching a
rule off (its key = 0). Earlier rules (crits, attack bonus, Extra Attack,
retargeting, Phase 1b) are the same in both modes unless noted.

## House rules (owner, 2026-11-04, #130)

The owner: "I want to add a house rules section. For example we'll keep the faithful rules, the ones you added." Rules
that aren't SRD 5.2: each off by default, on top of the faithful rules, in nwn5e.ini's `[house_rules]`; each one's
in-game text says "a house rule". A house rule works only with the faithful mode it names.

| Key | Issue | The rule | With |
|---|---|---|---|
| `help` (before 0.69.0 `[rules] help_house_rule`, still read) | #56 | Help on a downed henchman: a DC 10 Wisdom (Medicine) check gets it up at 1 HP | `actions` faithful |
| `flask_patches` | #130 | Acid, Alchemist's Fire, Holy Water thrown at the ground leave a patch one creature wide for 2 rounds: the flask's dice to anyone entering it or ending its round there (you too), no save, once a round; a fire one gives light | `grenades` faithful |

## Phase 3 — character model: decided by the owner (2026-10-19)

**Decision (owner, 2026-10-19): the recommendations below (CM0–CM13).**
PHB material goes to the private layer (docs/PRIVATE-LAYER.md).

Engine and data facts: docs/PHASE3-CHARACTER-RECON.md. "2024" = SRD 5.2;
Player's Handbook / DMG-only material is named as such and, **since the repo
is public, can't go in it at all** (personal use only). Answers open
decisions 1, 3, 4, 5 and 12 above.

**The frame (recommendation).** Faithful = the 2024 character model; NWN
mode = NWN's character model (3e progression, feats, prestige classes), with
the 2024 rules already built on top (proficiency bonus, attacks, saves,
checks) — as Phase 2 kept NWN's ranks in NWN mode. Faithful is computed **at
read time from the character's class levels** wherever the engine allows it
(hit points, features, ability scores): nothing is written into the
character that the other mode can't undo, a saved character works in both
modes, and henchmen follow without their own code. Key engine facts: XP only
gates level-ups; one server validator checks every level-up (PCs and
henchmen) and accepts *fewer* feats, no ability bump and unspent points;
the bonus-feat slots per class level are an in-memory table both the screen
and the validator read; maximum HP is re-summed from the level history on
every read; one function decides which classes a character may take. Until
Phase 7 builds screens, choices the screens can't offer are chat commands
(as `!skills`, `!mastery`).

**CM0. Per-mode data (foundation).** Hit dice, XP table, feat slots, class
lists and racial adjustments are 2da data, loaded once into the rules object;
2da / tlk files are shared by both modes, and an override-folder file would
change vanilla launches too. Recommendation: (A) **an in-memory overlay**:
after the 2das load, the DLL rewrites the loaded values per rule key (hit
die, XP table, XP penalty, bonus-feat slots, "player class" flag — all at
decoded offsets); (B) **served rows**: new rows (Warlock, SRD feats,
species) come from nwn5e's own 2da files that the DLL hands the resource
manager (vanilla launches stay vanilla), present in both modes and hidden by
the overlay in the mode that lacks them; names and descriptions through the
label hook (private string ids, as the saves' labels); the private layer
adds its own rows the same way. RE needed for B (the
resource manager) and for how the level-up screen filters its feat lists.
1.69-only: the offsets and the serving hook; the rows and rules port.
*Built 0.9.50 (B; key `served_data`):* rows live in `data/rows/<table>.txt`
(and `private/data/rows`), strings in `data/strings.txt` (ids 16000000+,
served by the label hook); at start-up the DLL rebuilds each table from the
game's own copy (override folder, then the key / BIF files in the engine's
order — read directly, so no game file is ever in the repo) + the rows, into
`build\served`, registered with the resource manager from the rules
constructor's entry. Rows are shared by both modes; each rule hides its rows
in the mode without them (A, through the rules-loaded hook). Checked with
`[debug] served_data_probe` (a hidden feat row read back from the engine
with its name). Format: data/README.md.

**CM1. XP table and level cap (open decision 3).** NWN: 500·L·(L−1), cap 40
with HotU. 2024: 300 / 900 / 2,700 / 6,500 / 14,000 … 355,000 at 20, cap 20
(the Epic Boon feat at 19; SRD 5.2 has no advancement past 20 — the DMG's
post-20 boons are DMG-only, to check). With the campaigns' XP awards
unchanged, the 2024 table gives 2024's own pacing: levels 1–3 about 3 times
faster, equal around level 6, slower after (partly offset: NWN gives lower
characters more kill XP). OC (NWN ~16 at 120,000) ends near 13; SoU a little
lower than NWN; **HotU climbs from 15 to 20 over the campaign** (its end, NWN
~27–30, is past 355,000) instead of hitting 20 in its first chapter and
staying there — the main argument for the table. Recommendation, faithful:
the 2024 table in memory, cap 20 (CanLevelUp hook; MaxCharLevel left alone —
lowering it makes the character validator refuse), and one invariant: **a
character's XP is never below its level's threshold** — when a script or an
automatic level-up sets the level (HotU sets 105,000 = NWN 15, then levels
the PC to 15), XP is raised to the 2024 threshold (165,000); without it the
HotU PC would wait until 195,000 for level 16. XP past 20 does nothing. NWN
mode: NWN's table and cap 40 (epic levels; proficiency bonus stays +6).
Consequences: switching a saved character between modes changes how many
levels its XP is worth (its levels never drop).
*Built 0.9.46 (key `progression`):* the XP table is written once the rules
object has loaded exptable.2da; the cap is CanLevelUp's own byte (20, raised
to 40 with HotU — now 20). The XP floor is done where scripts set XP (the
SetXP command; engine awards only add — positive plot awards go through
GiveXPToCreature): HotU sets XP 105,000 and then the player levels up (or
bk_auto_level does), so the *value* must become 165,000 — and the respawn
scripts clamp with NWN's formula (500·L·(L−1)), which on the 2024 table
would give XP at low levels and take levels at high ones. Rule
(rules::ScriptSetXp2024): NWN's threshold for another level sets that level
(HotU, Prelude's 1,000 / 3,000, x2_dm_leveldown); NWN's threshold for the
current level never raises XP nor drops the level (respawn, q2e_ud_throncr);
anything else (crafting and Harper costs, the Deck's −10,000, a negative plot
award) is kept but never below the current level. An XP cost landing exactly
on an NWN threshold would be read as a level-set (rare: exact multiples of
500·L·(L−1)).
*0.9.47:* ReloadAll (module load / unload with haks, 11 callers) re-reads
exptable.2da; the 2024 table is now written after it too.
*Verified in game 2026-10-19 (0.9.48):* the 2024 table on the sheet. Not seen
yet: a script SetXP (x2_dm_leveldown on a level 3+ character, a respawn).

**CM2. Multiclassing.** 2024: 13 in the primary ability of every current
class and the new one (Barbarian Str; Bard Cha; Cleric Wis; Druid Wis;
Fighter Str or Dex; Monk Dex and Wis; Paladin Str and Cha; Ranger Dex and
Wis; Rogue Dex; Sorcerer Cha; Warlock Cha; Wizard Int); no XP penalty; save
proficiencies from the first class only (built, C2); limited proficiencies
from later classes. NWN: −20 % XP per class 2+ levels behind (favored class
excepted), no ability prerequisite, at most 3 classes. Recommendation,
faithful: the 13 rule in CanLevelUpInClass (drives both the class list and
the validator, so the screen greys the class out), XP penalty off (class
penalty flag 0 in memory), later-class armour / weapon proficiencies limited
to 2024's multiclass lists, 3 classes kept (the client's limit). NWN mode:
NWN's (the penalty is NWN's multiclass brake; no 13 rule).
*EE (2026-10-20, EP16 in docs/EE-PORT.md):* the limit is EE's ruleset value
MULTICLASS_LIMIT (3) and EE stores up to 8 classes — faithful allows **8**
(NWN mode 3); 8 is the engine's storage, nwn5e's one deviation from 2024's
unlimited multiclassing. Confirmed by an interface test when ported (E4).
*Built 0.9.46 (key `multiclass`):* no XP penalty (AddExperience always takes
its one-class path); the 13s in CanLevelUpInClass, with the engine's ability
scores (base + racial adjustment + effects — CM4 will change what they hold),
prestige classes counted as their parent class (decision 10: a Fighter taking
Weapon Master is not a new class). It applies to henchmen's automatic
level-ups too (decision 11). Later-class proficiencies wait for the feature
mask (CM9).
*Verified in game 2026-10-19 (0.9.48):* a Fighter with Str 16 Dex 15 Con 14
Int 10 Wis 10 Cha 9 was refused Bard, Cleric, Druid, Monk, Paladin, Ranger,
Sorcerer and Wizard, each with its reason; Barbarian and Rogue stayed open.

**CM3. Hit points (open decision 4).** 2024: level 1 = the die's maximum +
Con; each later level = roll or fixed average (d6 4, d8 5, d10 6, d12 7) +
Con; dice Barbarian d12; Fighter, Paladin, Ranger d10; Bard, Cleric, Druid,
Monk, Rogue, Warlock d8; Sorcerer, Wizard d6 (NWN: Bard, Rogue d6; Sorcerer,
Wizard d4). NWN: the client rolls each level (max at levels 1–3, then half +
random) and stores it. Recommendation, faithful: **fixed average**, computed
in GetMaxHitPoints from class levels with the 2024 dice (stored rolls
ignored, so it is reversible and the same for henchmen); Con retroactive
(already how NWN sums). Rolled would need 2024-die rolls stored per level
(the client rolls with the in-memory die, so only new levels). NWN mode:
NWN's dice and rolls. The 2024 die per class also feeds Phase 5's Hit Dice.
*Built 0.9.49 (key `hit_points`):* GetMaxHitPoints hooked; for player
characters and henchmen the dice come from the level history (the class of
each level, in order) or, without one (henchmen built in the toolset), from
the class slots in order; prestige classes use their parent's die (decision
10). Toughness, Epic Toughness and the Pale Master's bonus no longer count
(CM8). The level-up screen still shows NWN's HP roll for the new level (client
code, Phase 7); the sheet shows the 2024 maximum. Current HP above a lower new
maximum is left to the engine (watch in the test).
*Verified in game 2026-10-20 (0.9.49):* a Fighter with Con +2 had 12 at level 1
(NWN 12) and 20 after levelling to 2 (+8; NWN 24). Not seen yet: a henchman,
current HP above a lower new maximum (a character above level 1 made under
NWN).

**CM4. Ability scores.** 2024: standard array, 27-point buy (8–15) or roll;
**species give nothing**; the background gives +2 / +1 or +1 / +1 / +1 among
its three abilities; Ability Score Improvement is a feat (CM7); 20 maximum
(Epic Boons to 30). NWN: 30-point buy up to 18, racial adjustments (applied
when read; where is not decoded yet), +1 every 4 total levels. Recommendation,
faithful: racial adjustments → 0 and the background's increases added at the
same read point; NWN's per-level +1s (stored in the level history) not
counted; point buy 27 / max 15 if the creation screen's budget is a patchable
constant (client RE, 1.69-only), otherwise Phase 7. NWN mode: NWN's.
Consequences: campaign Int / Cha dialog checks (142 dialogs) see the 2024
scores.
*Built 0.9.52 (key `ability_scores`):* racial adjustments zeroed in the loaded
race table after every rules load (only Dwarf, Elf, Gnome, Halfling and
Half-Orc have any; NPCs of those species lose them too); the creation screen's
point buy patched at 39 client sites (27 points, at most 15, 2 points from
13; the "Recommended" button can still set package scores above 15). Read-time
wasn't possible: every cached modifier is recomputed inline from the stored
base (9 writers per ability), so a character's base scores are *set* through
the engine's setters — minus NWN's level +1s (history +0x102), plus the
background and each ASI, none past 20 (rules::FaithfulAbilityDelta) — and
each change is kept in a local variable (`nwn5e_delta_*`): applied once,
undone in NWN mode. Checked every 2 s per character (AIUpdate) and right
after !asi / !background. Not undone without nwn5e (a vanilla launch sees
the 2024 bases).
*Verified in game 2026-10-20 (0.9.52):* `!background soldier str con` → Str 16
→ 18, Con 16 → 17; `!asi dex` after taking ASI 1 → Dex 13 → 15; switching to
Criminal removed the Soldier increases (Str 16) and gave Dex 17 (+2 +2) and
Alert (initiative +5 = Dex +3, PB +2); a new character's point buy: 27 points,
at most 15, the second point cost from 13 (owner).
Not seen yet: a henchman's ASIs, the NWN-mode undo, legacy ASIs.
*Accepted (#19, the character model rows not seen in game; owner, 2026-11-01: "EP16b is good"; "I won't test NWN
mode as faithful characters should switch modes and vice versa. I'll assum the other rows from #19 matrix also
pass").* Seen in game: S4, O6, CM1's script SetXP path (2026-10-30), EP16a and EP16b (the 4th class on the sheet,
kept through save and reload). Taken as passing, not seen: CM1b (respawn), the henchman rows (CM3a HP, CM4a
ASIs). Not tested, by the owner's call - a character stays in the mode it was made in: the mode-switch rows
(CM3b, CM4b, CM4c) and NWN mode's 3-class limit (EP16c). So current HP above a lower maximum after a switch
(CM3b) stays undecided, and the NWN-mode undo and legacy ASIs stay as built, unseen.

**CM5. Backgrounds.** SRD 5.2 has 4: Acolyte, Criminal, Sage, Soldier
(abilities, Origin feat, 2 skills, a tool). NWN has no background step; its
"background" feats (Blooded, Luck of Heroes, Silver Palm…) are 3e/Forgotten
Realms feats. Recommendation, faithful: `!background` (once per character,
existing ones too): picks one of the 4 and the ability split, grants the
Origin feat, and its 2 skills count in `!skills`; tools map to SK1's rows
(Thieves' Tools = Open Lock / traps; Calligrapher's / gaming set have no use
and are only recorded). NWN's background feats do nothing and are hidden.
NWN mode: NWN's background feats, no 2024 background. Note: Magic Initiate
(Acolyte, Sage) needs Phase 4's spells; until then it grants nothing.
*Built 0.9.52:* `!background <name> <+2> <+1>` or `<+1> <+1> <+1>` (the SRD's four;
changeable); its skills count as proficient (Insight, Religion, History and
Athletics have no NWN use yet); Alert (Criminal) gives initiative +PB now;
Savage Attacker and Magic Initiate are recorded. NWN's background feats still
work until CM8 hides them. `!asi <ability>` (+2) or `<a> <b>` (+1 each), `!asi
undo`, `!asi` to see the scores and what is unspent.

**CM6. Species (open decisions 1, 2).** SRD 5.2: Dragonborn, Dwarf, Elf,
Gnome, Goliath, Halfling, Human, Orc, Tiefling. Built already: the save
advantages (C3) and Halfling Luck. Recommendation, faithful: the 5 shared
species get their 2024 traits (Dwarven Toughness +1 HP per level through
CM3, Darkvision ranges, Elf Keen Senses and Human Skillful as `!skills`
picks, Human Versatile = an Origin feat in NWN's human bonus slot, Trance and
the lineage spells with Phases 4 / 5); NWN racial features with no 2024
counterpart do nothing. **Half-Orc plays as the SRD Orc** and is named "Orc"
in faithful (Relentless Endurance, Adrenaline Rush, Darkvision 120) — the
missing Orc at no art cost, and the 23 campaign scripts checking Half-Orc
still work. **Half-Elf stays as a legacy species** with its SRD 5.1 traits
(Darkvision, Fey Ancestry, two skills; no ability increases; SRD 5.1 is
CC-BY 4.0 as well). Tiefling, Goliath, Dragonborn: need models — a separate
appearance recon after the core (Tiefling / Goliath may reuse NWN models;
Dragonborn needs a new one). Aasimar is PHB-only. NWN mode: NWN's races and
racial features.

**CM7. When feats come (feat cadence).** 2024: Origin feat at level 1 (from
the background; humans one more), a General feat at class levels 4 / 8 / 12
/ 16 (Fighter also 6 / 14, Rogue 10), Fighting Style (Fighter 1, Paladin 2,
Ranger 2), Epic Boon at 19 — Ability Score Improvement is one of the General
feats. NWN: a general feat at total level 1 and every 3rd (+ human), class
bonus feats, +1 ability every 4 total levels. Recommendation, faithful:
NWN's general feat only at level 1 (the Origin slot; patched on client and
server together); 2024's feat levels as class bonus-feat slots in memory
(no protocol change — both sides read that table); NWN's ability step off
on both sides (the level-up message carries the byte on both or neither);
ASI as feat rows (one per possible pick, the next one shown) whose +2 / +1+1
is assigned with `!asi`. NWN mode: NWN's cadence.
*Built 0.9.51 (key `feat_cadence`):* six 2-byte patches — the general feat
only at level 1 on the server, in henchmen's automatic level-up and on the
level-up screen; no ability byte in the level-up message on either side and
no ability page on the screen — and the bonus-feat slots of the 11 base
classes rewritten after every rules load (rules::IsFeatLevel2024). Ability
Score Improvement = feat.2da rows 1200–1206 (data/rows/feat.txt), each
needing the one before so the next one is offered, in every base class's
bonus list (List 2); hidden in NWN mode (MinLevel 0xFF). Henchmen still level
(LevelUpAutomatic fills free slots from all feats; the validator accepts
fewer). Not yet: Fighting Style slots and Epic Boons (with those feats, CM8 /
CM9), the level-1 slot limited to Origin feats (CM8), prestige classes' slots
(CM10 turns them off).
*Verified in game 2026-10-20 (0.9.52):* a Fighter leveled 1 → 4: no general
feat at 3, no ability page at 4, Ability Score Improvement 1 offered in the
bonus slot and taken.

**CM8. Which feats exist.** SRD 5.2 has 17: Origin — Alert, Magic Initiate,
Savage Attacker, Skilled; General — Ability Score Improvement, Grappler;
Fighting Style — Archery, Defense, Great Weapon Fighting, Two-Weapon
Fighting; Epic Boons — Combat Prowess, Dimensional Travel, Fate, Irresistible
Offense, Spell Recall, the Night Spirit, Truesight. NWN offers ~210 non-epic
feats to everyone. Recommendation, faithful: **the 17 only**; an NWN feat
that is one of them is reused under the 2024 name (Improved Initiative →
Alert, built in SK9), the rest do nothing and are hidden. The PHB
counterparts (Tough, Mobile, Great Weapon Master, War Caster, Dual Wielder,
Sharpshooter, Mounted Combatant) are PHB-only: not in the public repo — the
private layer can add them reworded (owner, 2026-10-19).
Build-able now: the Fighting Styles, Savage Attacker, Alert, Skilled;
Grappler waits for grappling, Magic Initiate / Spell Recall for Phase 4.
NWN mode: NWN's feats. Consequences: faithful builds choose through class
features (Fighting Style, Weapon Mastery, Invocations, Metamagic) and spells
rather than feats — that is SRD 5.2's shape.

**CM9. Class features.** Recommendation, faithful: a **feature mask** at
read time (the engine's has-feat check, RE: address not taken yet): a
character has exactly the 2024 features of its class levels — NWN features
with a 2024 counterpart kept under the 2024 name and level (Rage, Sneak
Attack, Evasion, Wild Shape, Lay on Hands…), NWN-only ones absent (Barbarian
Uncanny Dodge and damage reduction, Scribe Scroll, NWN's level-1 Cleric
domains), new ones as served rows with DLL mechanics. Many need bonus
actions / reactions (Phase 6), rests (Phase 5) or spells (Phase 4), so the
classes come **one at a time, each with its own short brief** (keep / rename
/ new / drop / later per level): Fighter, Rogue, Barbarian, Monk first;
Paladin, Ranger, Bard and the full casters with Phase 4. Campaign
`GetHasFeat` checks (83 scripts) read the mask. NWN mode: NWN's features.
*Built 0.9.53 (the frame, key `class_features`):* rules::ClassFeatures2024
lists each class's 2024 features by level with what nwn5e does with each (the
Fighter's from its brief; the others empty until theirs); `!features` shows a
character's. The feature mask (has-feat hook) waits for the first class
that has NWN features to remove (the Fighter has none: its bonus feats went
with CM7) or for CM8.

**CM10. Subclasses and prestige classes (open decisions 5, 12).** SRD 5.2
has one subclass per class: Berserker, Lore, Life, Land, Champion, Open Hand,
Devotion, Hunter, Thief, Draconic Sorcery, Fiend Patron, Evoker (text checked
per class before building). With one each, **faithful grants it at class
level 3 automatically** — no selection screen (decision 12's feat-step
design stays the fallback if more are ever added). Prestige classes in
faithful: off for PCs and henchmen (CanLevelUpInClass); levels already taken
count as the parent class (decision 10; e.g. Deekin's Red Dragon Disciple
levels are Sorcerer levels with Draconic Sorcery at 3) and their own
features read as absent. A henchman's package level-up into a prestige class
falls back to its other class (NWN's own fallback). Dialog branches that
check a prestige class simply never show. NWN mode: NWN's prestige classes
(with decision 10's level counting), no 2024 subclasses.
*Built 0.9.53:* CanLevelUpInClass (CM2's hook, now installed for either rule)
refuses every class past the 11 base classes in faithful — the level-up
list greys the prestige classes out, henchmen's package level-ups fall back.
The subclass is the SRD one at class level 3 (rules::Subclass2024), shown by
`!features`; its features come with each class's brief.

**CM11. Warlock.** New class in a blank classes.2da row (39) through CM0's
served data: d8, Cha, Pact Magic, Eldritch Invocations, Magical Cunning, the
Fiend Patron, Eldritch Blast. No campaign script can name it (1.69 has no
CLASS_TYPE for it): no class-specific dialog, no henchman Warlocks; no art
needed. Recommendation: faithful only (hidden in NWN mode); reserve the row
in Phase 3, build it after Phase 4's slot system (a Warlock without Pact
Magic is empty).

**CM12. Henchmen and automatic level-ups.** Decision 11 (henchmen use the
PC rules) holds. With read-time computation henchmen need no own code: HP,
features and abilities follow their class levels; their packages keep
picking NWN feats, which do nothing in faithful. Recommendation, faithful:
their ASIs are assigned automatically (+2 to the class's primary ability,
then the next); OC's pre-built blueprints read the same way. HotU's
automatic level-up of the PC to 15: the player assigns its ASIs with `!asi`.
*Built 0.9.52 (with CM4):* a henchman's ASIs = its 2024 feat levels (classes
grouped by decision 10), each +2 to its first class's main ability below 20,
then the next (rules::AsiPriority2024).

**CM13. Existing characters.** Everything read-time switches cleanly with
the mode. What a faithful player does once: `!background`, `!asi` for the
ASIs already earned, `!skills` (built). Characters above 20 (HotU saves) are
read as their first 20 levels in faithful. The XP floor (CM1) raises stored
XP — the one write, and the reason a character moved back to NWN mode may
have levels to take.
*Built 0.9.52 (part):* the first time faithful sees a player character, its
2024 feat levels so far minus the ASI feats it owns become "legacy" ASIs to
assign with `!asi` (`nwn5e_asi_legacy`); its NWN level +1s stop counting. The
ability changes are the second write (undone in NWN mode).

**CM14. Alignment (decided by the owner, 2026-10-19: the recommendation,
option A).** 2024 keeps alignment as a description only (character, monster
stat blocks); no SRD 5.2 rule reads it — no class requires one, and the
"alignment" spells and items (Protection from Evil and Good, Holy Avenger)
work by creature type. NWN uses it for class limits (Paladin lawful good,
Monk lawful, Barbarian / Bard non-lawful, Druid neutral, Blackguard /
Assassin evil; a paladin who changes alignment can't take paladin levels —
the creation screen limits alignments by class too), item "Use Limitation:
Alignment", spells and weapon properties (Smite Evil, Holy / Unholy /
Axiomatic / Anarchic, Protection from Alignment, Holy Word, Blasphemy), and
the campaigns shift it in many dialogs and check it in some.
Recommendation, faithful: alignment is descriptive — class limits cleared in
memory (the CM0 overlay; the creation screen and CanLevelUpInClass both read
them), item alignment limits ignored, alignment spells and weapon properties
converted to creature types where they are rewritten (Phase 4 spells, the
Paladin brief's smite, Phase 8 items); the campaigns keep shifting and
checking it as story. NWN mode: NWN's.
*Built 0.9.48 (key `alignment`):* the three class fields cleared once the
rules load and after every reload (a shared "rules loaded" hook, also used
by CM1's XP table and later by CM0's overlay); the creation screen's own
hard-coded limits skipped (it never read the class fields); the item check
treats every item as having no alignment limit. Not changed: alignment
shifts and dialog checks; spells and weapon properties (with their phases).
*Verified in game 2026-10-19 (owner):* every alignment at creation and the
limited classes at level-up.

**Build order (proposed).** 1. RE: resource serving, feat-list filtering,
the ability read point, the has-feat check, the creation budget. 2. CM1 +
CM2 (known hooks, small). 3. CM3. 4. CM7 + CM4 + CM5 (`!background`,
`!asi`). 5. CM9 / CM10 frame (mask, automatic subclass, prestige off), then
Fighter, Rogue, Barbarian, Monk. 6. CM8's feats as their hooks exist. 7. CM6
(Orc, Half-Elf, traits; new species after the appearance recon). 8. CM11 and
the casters with Phase 4. Phase 3 is the largest phase so far; each class
brief is its own decision.

## Phase 3 — Fighter (CM9): decided by the owner (2026-10-20)

**Decision (owner, 2026-10-20): the recommendations below (F0–F7, C1–C5).**
Build order: C1, C2, F6 (existing hooks); F1 (AC and damage-dice hooks);
F0 (script compiling, served scripts, granting features), then F2 + F7
Shift, F5, C5.

The first class brief. SRD 5.2 Fighter, one subclass: **Champion** (Battle
Master and the others are PHB-only — private layer). NWN's Fighter has no
features of its own beyond proficiencies (the same in 2024: simple and
martial weapons, all armour, shields) and its bonus feats, which CM7 already
removed in faithful. Status per level, then the decisions (F0–F7, C1–C5).

| Lvl | 2024 feature | Status |
|---|---|---|
| 1 | Fighting Style | F1 |
| 1 | Second Wind | F0, F2 |
| 1 | Weapon Mastery (3 / 4 at 4 / 5 at 10 / 6 at 16) | built (D3) |
| 2 | Action Surge (2 uses at 17) | F3 |
| 2 | Tactical Mind | F4 |
| 3 | Champion | automatic (CM10) |
| 4, 6, 8, 12, 14, 16 | Ability Score Improvement | built (CM7) |
| 5 | Extra Attack (2 / 3 at 11 / 4 at 20) | built (1a) |
| 5 | Tactical Shift | F7 |
| 9 | Indomitable (2 uses at 13, 3 at 17) | F5 |
| 9 | Tactical Master | F7 |
| 13 | Studied Attacks | F6 |
| 19 | Epic Boon | with CM8 |
| Champion 3 | Improved Critical (19–20) | C1 |
| Champion 3 | Remarkable Athlete | C2 |
| Champion 7 | Additional Fighting Style | C3 (with F1) |
| Champion 10 | Heroic Warrior | C4 |
| Champion 15 | Superior Critical (18–20) | C1 |
| Champion 18 | Survivor | C5 |

**F0. How usable features run (applies to every class).** Second Wind, Action
Surge, Rage, Channel Divinity … are things the player *uses*. NWN does this
with a feat that has uses per day and a spell row whose script does the work
(Lay on Hands, Turn Undead); the engine counts the uses and restores them on
rest. Options: (a) **NWScript** — the feat's spell row runs an nwn5e script
(.nss in the repo, compiled at build time with the open-source nwnsc against
the game's own nwscript.nss, served by CM0); ports to EE as is; (b) the DLL
catches the spell and does the work in C++ (1.69-only). Recommendation: (a),
with the DLL only where a script can't reach (rolls inside the engine).
Short rests (Phase 5) later restore the short-rest uses.

**F1. Fighting Style (and C3).** The four SRD Fighting Style feats as served
rows in the Fighter's bonus list, a bonus slot at Fighter 1 (and Champion 7;
Paladin / Ranger 2 with their briefs): Archery (+2 to ranged attack rolls),
Defense (+1 AC in armour), Great Weapon Fighting (1s and 2s on the damage
dice of a two-handed / versatile melee weapon count as 3s), Two-Weapon
Fighting (the ability modifier on the off-hand attack — NWN's Two-Weapon
Fighting feat already does this in faithful, D1; it becomes this style). The
ASI rows get "Level 4+" (MinLevel 4, the SRD feat's prerequisite) so they
aren't offered at level 1. Recommendation: build now (all hooks exist).

**F2. Second Wind.** Bonus action: regain 1d10 + Fighter level HP; 2 uses (3
at 4, 4 at 10), one back on a short rest, all on a long rest. Options: (a) now,
through F0, as a normal action until Phase 6 has bonus actions; (b) wait for
Phase 6. Recommendation: (a) — it is the Fighter's level-1 identity.

**F3. Action Surge.** One extra action (two uses at 17). NWN has no actions to
add to; the closest is an extra round of attacks. Recommendation: Phase 6
(action economy) — a stand-in now would have to be redone.

**F4. Tactical Mind.** Spend a Second Wind use to add 1d10 to a failed ability
check. Needs a choice the engine can't ask for. Options: (a) automatic — when
a check fails by 10 or less and a use is left, it is spent; (b) later with
Phase 7's interface. Recommendation: (b) — spending heals silently is worse
than not having it.

**F5. Indomitable.** Reroll a failed saving throw with a bonus of your Fighter
level; 1 use per long rest (2 at 13, 3 at 17). The save roll is ours (C4).
Options: (a) automatic: the first failed save each time a use is left is
rerolled (no prompt in NWN); (b) automatic only for saves against spells and
effects the player marks with a command. Recommendation: (a), uses through
F0's feat counting.

**F6. Studied Attacks.** When you miss with an attack, advantage on your next
attack against that creature before the end of your next turn. Built like
Vex (D4). Recommendation: build now.

**F7. Tactical Shift (move half speed without opportunity attacks after
Second Wind) and Tactical Master (swap a weapon's mastery for Push, Sap or
Slow on one attack).** Shift needs Second Wind; Master needs a per-attack
choice. Recommendation: Shift with F2; Master later (a `!mastery` style
toggle, or Phase 7).

**C1. Improved Critical (3) and Superior Critical (15).** Crits on 19–20, then
18–20 with weapon and unarmed attacks. Every crit is held to 20 today
(decision 8 already says an expanded range only upgrades a hit).
Recommendation: build now (class check on the threat range). NWN's Improved
Critical feat no longer widens the range in faithful (CM8).

**C2. Remarkable Athlete.** Advantage on initiative and on Strength
(Athletics) checks; after a critical hit, move half speed without
opportunity attacks. Recommendation: the initiative advantage now (SK9 has
the hook); Athletics has no NWN use; the move with Phase 6.

**C4. Heroic Warrior (10).** In combat, gain Heroic Inspiration at the start
of each turn if you don't have it (spend it to reroll a d20). Heroic
Inspiration is also the Human's Resourceful (CM6). Options: (a) automatic —
it rerolls your first failed attack roll or save each round; (b) later with a
shared Heroic Inspiration (CM6). Recommendation: (b), built once for both.

**C5. Survivor (18).** Heroic Rally: at the start of each turn, if you're
Bloodied (half HP or less) and above 0, regain 5 + Con modifier HP. Defy
Death: advantage on death saves (Phase 5). Recommendation: Heroic Rally now
(the creature update), Defy Death with Phase 5.

**NWN mode:** NWN's Fighter (bonus feats; prestige classes such as Weapon
Master).
*Built 0.9.54:* C1 (GetCriticalHitRoll: the Champion's floor 19 / 18 under the
2024 crit rule), C2 (initiative advantage, with invisible / hidden), F6
(Studied Attacks: a miss marks the target like Vex; the next attack at it
within the Fighter's next turn has advantage). Fighter levels include
prestige-class levels (decision 10).
*Built 0.9.55 (F1 part 1):* the four style feats served (feat.2da 1210–1213,
in the Fighter's bonus list only), bonus slots at Fighter 1 and 7, ASI rows
Level 4+; Archery (+2 on the ranged attack roll and the sheet) and
Two-Weapon Fighting (the off-hand ability modifier, as NWN's feat) work;
Defense and Great Weapon Fighting can be taken and work once their hooks are
built (AC in armour, the weapon's damage dice). At Fighter 7 the slot also
offers ASI (NWN's screen can't limit one slot to styles).
*Built 0.9.56 (F0, F2, F7 Shift):* scripts/*.nss are compiled at build time
with the game's own compiler (utils\clcompile.exe — no download; it reads
nwscript.nss from the install) into build\data\files, which CM0 serves as
is. Second Wind = feat.2da 1220–1222 (2 / 3 / 4 uses per day, restored by a
rest; data/rows/feat.fighter.txt) → spells.2da 1000–1002 (a self feat
ability like Lay on Hands; data/rows/spells.fighter.txt) → nwn5e_secwind:
1d10 + Fighter level (prestige levels included); from Fighter 5 it sets
`nwn5e_tactical_shift` for 6 s, which the opportunity-attack rule reads (no
AoO). nwn5e grants the right tier to every Fighter (character made earlier
included) with full uses, records it (`nwn5e_granted_feats`), and removes it
in NWN mode. Uses an action (Phase 6 makes it a bonus action).
*Built 0.9.57 (F1 part 2):* Defense +1 AC in armour (chest item base AC > 0,
the engine's own test) on attacks and on the sheet (CNWSCreature::
GetArmorClass hooked); Great Weapon Fighting on the weapon's base dice only
(GetDamageRoll 0x69B0E9 / 0x69B2D1 redirected): melee, main hand, a
Two-Handed or Versatile weapon (rules::IsTwoHandedWeapon2024 /
IsVersatileWeapon2024) held in two hands — NWN's two-handed wield, or the
off hand empty (a versatile weapon of the wielder's size). Sneak attack and
effect dice are not raised (the conservative reading).
*Built 0.9.58 (F5, C5):* Indomitable in the saving-throw wrapper — a failed
save (not an automatic failure) is rolled once more against the DC lowered by
the Fighter level, spending a use (nwn5e_indomitable_used; cleared when the
engine restores feat uses after a rest — the mastery module's rest hook now
takes listeners). Heroic Rally once per combat round (the round counter of
D4) when Bloodied and above 0 HP: + 5 + Con modifier, current HP written
directly. The Fighter brief is done but for Action Surge (Phase 6), Tactical
Mind / Master (later) and Heroic Warrior (with CM6).

*Accepted (owner, 2026-10-30: "28, 57 and 23 accepted").* The Fighter on EE (#23): the Champion's
crits (19-20, 18-20 from 15), Defense, Indomitable, Studied Attacks and Remarkable
Athlete seen in the owner's `!test fighter` runs; Tactical Shift and Heroic Rally
accepted without a log line of their own.

## Phase 3 — the non-caster classes (CM9): briefs for the owner's decision (2026-10-24)

Owner, 2026-10-24: "start work on non caster classes" — the Barbarian, the
Monk and the Rogue (the Ranger and the Paladin are half-casters: with Phase 4).
Three briefs below, each checked against the SRD 5.2.1 PDF and EE's engine
(read-only recon; the engine facts, with RVAs, close each brief; the session
re-checked their exports). Shared between them — one piece of code each:
- **X1. The feature mask** (B0 = M0 = R0): one faithful HasFeat filter (feats.cpp
  already filters CNWSCreatureStats::HasFeat) answering NWN's class feats by
  their 2024 counterparts — NWN-only ones 0, kept ones by the 2024 level — for
  the engine, the scripts' GetHasFeat and the AI alike; the character keeps its
  feats (one save, both modes). Masked feats stay on the sheet, labelled "no
  effect in 2024 rules", until Phase 7.
- **X2. Evasion** (M11 = R8): NWN's Improved Evasion means 2024 Evasion (Rogue 7,
  Monk 7): Dexterity saves only, not while Incapacitated; NWN's Evasion absent.
- **X3. Flat-footed** (R1 e; B0's Uncanny Dodge 1): faithful hooks GetFlatFooted
  to "never" — 2024 has no flat-footed; helpless targets give advantage
  instead (the conditions decision left it "until Sneak Attack is converted").
- **X4. Resistance** (B2, M14): one DoDamageImmunity hook — a 2024 Resistance
  halves (rounded down) and doesn't stack; the same route for Petrified (CN8),
  species and spells later.
- **X5. Unarmored Defense** (B4, M2): Barbarian (Con) or Monk (Wis), the higher,
  not both.
- **X6. Feature uses and rests** (B1, M3, OH2): hooks on GetFeatTotalUses /
  GetFeatRemainingUses / DecrementFeatRemainingUses (exported) for Rage's 2024
  counts and the Monk's Focus pool; a short-rest listener list (the Short Rest
  hard-codes Second Wind today).
- **X7. Weapon proficiencies** (M17, R3, B14's mastery list, CM2's multiclass
  lists): one CheckProficiencies pass for all classes — its own item, after
  these briefs.

**Decisions (owner, 2026-10-24, taken ten at a time):**
- X1: **removed completely** in faithful (owner: "remove them completely, we
  don't need to keep NWN compatibility") - the NWN-only class feats are taken
  off the character (gone from the sheet and the radial; existing characters
  and henchmen at load) and the class feat tables served in faithful no
  longer grant them; the HasFeat filter stays for the counterparts answered
  by 2024 level. A character played in faithful doesn't get them back in NWN
  mode (accepted); characters made in NWN mode keep them.
- X2-X7 A. B1 A. B2 A, potions allowed while raging.
- B3 A with the owner's caveat, as defined: the Rage also goes on while the
  Barbarian has an attack order on a living enemy it can see (the engine's
  attack target, set while it runs at the enemy) - 2024's Bonus Action
  extension until Phase 6; a move order, a round with no attack order,
  Incapacitated, Heavy armour or the 10 minutes end it.
- B4-B13 A (B6 henchmen reckless whenever they rage). B10: the owner wants
  the effects picked in the interface or a custom element ("we need to figure
  out a way for the player to pick effects like this via the UI or a custom
  UI element somewhere") - X8.
- B9 Instinctive Pounce, A (a charge): from Barbarian 7, entering Rage gives
  +50% speed for one round (half its speed again; the move still provokes
  opportunity attacks, as in 2024).
- **X8. Picking class options** (Brutal Strike's effects, later Cunning
  Strike, Open Hand Technique ...), C - both: **radial sub-options** (the
  engine's sub-radial, as Wild Shape's: spells.2da SubRadSpell1-5, five per
  menu) set the standing choice - on / off, quickbar-able, an icon on the
  character and a chat line show it - first with Brutal Strike; and a
  **"Combat options" NUI window** (every class choice with descriptions and
  checkboxes, no five-option limit) when Cunning Strike comes.
- BZ1-BZ4 A; M1 A with (c)(i) (the bonus unarmed strike every round), M2-M5
  A.
- B14 A: the Barbarian's mastery list (built from proficiency, so it offered
  ranged weapons) drops Longbow, Shortbow, Light / Heavy Crossbow, Sling, Dart
  and Shuriken; Throwing Axe stays (2024's thrown handaxe, a melee weapon). A
  Barbarian who took one of them re-picks (`!mastery`).
- M6-M10, M12-M15, OH1 A (M7 automatic at the first fight with half the
  points spent or Bloodied; M9 NWN's Stunning Fist attack at 1 point, refunded
  on a miss; M13's reroll automatic; OH1 picked through X8, Topple by
  default).
- OH2-OH4 A (OH4 one step: the save at once, 10d12, no instant death). R1 a-c
  A (the 2024 trigger, once per round, the dice doubled on a crit); R1 d A
  (faithful drops sneak-attack immunity only; crit immunity unchanged until
  Phase 8). R2 A, and onto the RP list. R4 A (Phase 6; stealth in combat for
  Rogue 2+ now if it is one server-side check). R5 A (Phase 6).

**RP list (owner, 2026-10-24: "Note down 48 in a RP list, we can look into
these RP skills and maybe try to find a way to make them useful").** Features
with no NWN use today, to look at for a role-play use (dialog checks,
scripted moments, ambient effects):
- Thieves' Cant (Rogue 1, R2).
- Second-Story Work (Thief 3, T2: climbing, jumping).
- Slow Fall and Acrobatic Movement (Monk 4 / 9, M6).
- The Monk's artisan's tool or musical instrument (M17).
- Survival (Primal Knowledge, B7).
- Languages.
(All added by the owner, 2026-10-24: "add 58 to the RP list along with all
the other recommendations you listed".)

- R6, R7, R9-R12, T1-T4 A (R6 picked through X8, the Poisoner's Kit waived;
  R7 automatic on the first seen hit each round, using the round's
  opportunity attack if it can be tracked; R9 at nwn5e's own d20s first; R12
  automatic on the first failed save - to be revisited with X9; T3 with an
  "attacking ends hiding" rule; T4 scrolls first).
- **X9. Choosing to spend a feature on a roll (owner, 2026-10-24):** "We'll
  need to come up with an interface for rolls such as this. The player might
  not want to use Stroke of Luck on an inconsequential roll. Some sort of
  interface that would pause the game and pop up a window asking if you'd
  like to use this applicable skill on this failed roll. This could also
  work for other rolls. For example to add guidance bonus before a roll
  where it can apply." - options in chat, to decide. Then: "take a look at
  how baldur's gate 3 handles rolls. it's a bit hamfisted for use in a combat
  situation in an active battle type game such as NWN, but it works for
  story interactions." Engine limit found: a roll and its consequence are one
  uninterruptible step (a script's `if (!MySavingThrow(...)) ApplyEffect...`),
  so a prompt after a failed engine / script roll can't wait for an answer.
  **Decided (owner, 2026-10-24: "X9 as recommended"):** combat - each
  roll-spending feature (Stroke of Luck, Indomitable, Disciplined Survivor,
  ...) gets Always / Smart (only against deadly or disabling effects) / Never
  in the X8 options window, plus arming before a dangerous moment; story
  checks - a BG3-style roll window, step 1 (the d20, modifiers, DC and
  outcome shown; bonuses set up beforehand applied) with the class work or
  Phase 7, step 2 (rolling in the window and rerolling a failure, the
  conversation held meanwhile) only if a spike shows the engine can hold the
  conversation step. No damage-per-round numbers needed (owner: "no need to
  run the damage numbers, go for barbarian").
- T5 A (Phase 6, with Action Surge). NWN mode's sneak dice stay undoubled on a
  crit (A). Sneak Attack on an opportunity attack: B - one more Sneak Attack
  on an opportunity attack made outside the Rogue's round (2024's once per
  turn), after a short RE to tell opportunity attacks apart in the attack
  data.

### Phase 3 — Barbarian (CM9): recommendations (for the owner's decision)

SRD 5.2 Barbarian, every feature below checked in the SRD 5.2.1 PDF (pp. 28–30, read 2026-10-24); its one subclass is **Path of the Berserker** (Wild Heart, World Tree, Zealot are PHB-only — private layer). NWN's Barbarian (EE `cls_feat_barb`, List 3, automatic): **Rage** 293 at 1 with successors 326/327/328/329/330/331 at 4/8/12/15/16/20 (uses 1→6/day, +1 per 4 levels past 20; spell 307 nw_s1_barbrage: +4 Str/Con, +2 Will, −2 AC for 3 + Con rounds, +6/+3 with Greater Rage at 15), **Barbarian Fast Movement** 194 at 1 (+10% speed), **Uncanny Dodge 1–6** (195, 251–255) at 2/5/10/13/16/19, **Damage Reduction 1–4** (196, 332–334) at 11/14/17/20 (+ epic 1067 at 23); proficiencies as 2024 (simple + martial weapons, light + medium armour, shields); epic Mighty / Thundering / Terrifying Rage. **Faithful today masks nothing**: a faithful Barbarian plays NWN's Rage, keeps Uncanny Dodge, DR and +10% speed, plus what is built — d12 fixed-average HP (CM3), Str/Con saves, 2024 skills + Primal Knowledge's skill at 3 (C7b), Weapon Mastery 2/3/4 (D3), Extra Attack at 5 (1a), ASIs (CM7), the 2024 kit (C2), no alignment limit (CM14); `!features` names the Berserker at 3, no features. Henchmen: OC/HotU Daelan and SoU's Xanos are Barbarians (decision 11); their AI and dialogs use feat 293.

| Lvl | 2024 feature | Status |
|---|---|---|
| 1 | Rage (2 uses / 3 at 3 / 4 at 6 / 5 at 12 / 6 at 17; damage +2 / +3 at 9 / +4 at 16) | B1, B2, B3 |
| 1 | Unarmored Defense | B4 |
| 1 | Weapon Mastery (2 / 3 at 4 / 4 at 10) | built (D3); B14 |
| 2 | Danger Sense | B5 |
| 2 | Reckless Attack | B6 |
| 3 | Path of the Berserker | automatic (CM10) |
| 3 | Primal Knowledge | skill built (C7b); Strength while raging B7 |
| 4, 8, 12, 16 | Ability Score Improvement | built (CM7) |
| 5 | Extra Attack | built (1a) |
| 5 | Fast Movement | B8 |
| 7 | Feral Instinct | B9 |
| 7 | Instinctive Pounce | B9 |
| 9 | Brutal Strike | B10 |
| 11 | Relentless Rage | B11 |
| 13 | Improved Brutal Strike (Staggering, Sundering) | B10 |
| 15 | Persistent Rage | B12 |
| 17 | Improved Brutal Strike (2d10, two effects) | B10 |
| 18 | Indomitable Might | B13 |
| 19 | Epic Boon | with CM8 |
| 20 | Primal Champion | B13 |
| Berserker 3 | Frenzy | BZ1 |
| Berserker 6 | Mindless Rage | BZ2 |
| Berserker 10 | Retaliation | BZ3 |
| Berserker 14 | Intimidating Presence | BZ4 |
| — | NWN only: Uncanny Dodge 1–6, Damage Reduction 1–4 + epic, Fast Movement +10% | drop (B0) |

**B0. NWN's Barbarian features in faithful (the feature mask — the Barbarian is the first class that needs it).** CM9: NWN-only features absent. They are engine rules, not effects: Fast Movement 194 is one HasFeat in CNWSCreature::GetMovementRateFactor (+0x4A, adds ruleset BARBARIAN_ENDURANCE_BONUS 0.1); Damage Reduction is five HasFeat calls in CNWSObject::DoDamageReduction (1067/334/333/332/196; amounts ruleset BARBARIAN_DAMAGE_REDUCTION_LEVEL_1–4, EPIC_BARBARIAN_DAMAGE_REDUCTION); Uncanny Dodge 2–6's trap Reflex bonus is HasFeat calls in SavingThrowRoll before its d20 (C7 already drops it with `saves` faithful), and Uncanny Dodge 2 (251) also guards sneak / death attacks (HasFeat in ResolveSneakAttack, ResolveDeathAttack). Uncanny Dodge 1 (195, Dex when flat-footed) is **not** HasFeat: GetArmorClassVersus (+0x988/+0x999) looks it up directly in the two feat lists, as Defensive Awareness 949. Options: (a) **a HasFeat filter** (AddFeatFilter, key `class_features`) answering 0 for these feats in faithful — read time, the character keeps the feats (one save, both modes), script GetHasFeat goes through HasFeat too, so campaign checks read the mask (none of the 4,144 base-game scripts tests these feats); (b) RemoveFeat with a record — the sheet follows, but it writes into the character and NWN's level-ups grant them again. **Recommendation:** (a), the general CM9 mask for the next classes. Uncanny Dodge 1's direct lookup is moot if faithful drops flat-footed (2024 has none — the Rogue brief's call, with Sneak Attack); if it stays, patch those two lookups. The sheet still lists the feats: their descriptions can say "no effect in 2024 rules" (conditional served feat rows → private strings); removing them from the list is Phase 7.

**B1. Rage: the feat, its uses, rests.** 2024: 2 uses (3 at 3, 4 at 6, 5 at 12, 6 at 17); one back on a Short Rest, all on a Long Rest. NWN: UseFeat / the script commands map 293 to the highest Rage feat the character has (GetHighestLevelOfFeat); uses are per feat id (a "spent" byte on the creature's list entry; remaining = total − byte); the AI rages with ActionUseFeat(FEAT_BARBARIAN_RAGE) (nw_i0_generic: Xanos, HotU Daelan; dialog x0_d1_userage) and stops casting while GetHasFeatEffect(FEAT_BARBARIAN_RAGE). Options: (a) **keep NWN's Rage feats as the 2024 Rage** — hook GetFeatRemainingUses (behind UseFeat, the radial update, AI talents, scripts) and GetFeatTotalUses (SetFeatRemainingUses, loading) for the Rage chain to answer the 2024 count by Barbarian level; in faithful, spell 307's ImpactScript becomes `nwn5e_rage` (a conditional served spells row, as the kits), NWN mode keeps nw_s1_barbrage; named "Rage" by served strings; (b) new nwn5e feat rows per uses tier (as Second Wind) with NWN's Rage masked — breaks the henchman AI and dialogs. **Recommendation:** (a). Short Rest: one use back (IncrementFeatRemainingUses on GetHighestLevelOfFeat(293), next to Second Wind in rules_rest); Long Rest: the engine's reset, as now. Entering Rage is NWN's feat use (an action, as Second Wind) until Phase 6's Bonus Action.

**B2. Rage: what it gives while active.** 2024: Resistance to Bludgeoning, Piercing and Slashing; Rage Damage on attacks using Strength (weapon or Unarmed Strike); Advantage on Strength checks and saves; no spells, no Concentration. NWN's +4/+4/+2 Will/−2 AC are dropped (2024 has none of them). *Resistance:* OnApplyDamage asks, per damage type, DoDamageImmunity (a %), then DoDamageResistance (flat), then DoDamageReduction (physical only). EffectDamageImmunityIncrease(50) removes max(1, damage × 50 / 100) — the larger half stays (7 → 4, 2024: 3) and it stacks with items. Options: (a) the script links 50% immunity (one point off on odd damage, stacks); (b) **hook DoDamageImmunity**: a raging creature's B/P/S damage halved, rounded down, once (2024 resistances don't stack) — the shared 2024 Resistance that Petrified (CN8), species and spells can use later. **Recommendation:** (b). *Rage Damage:* in the existing GetDamageBonus hook (rules_two_weapon.cpp) when the attack's ability is Strength; flat, so not doubled on a crit (decision 13). *Strength advantage:* Str saves (C1's Str-mapped spell saves, SK6's Shove save) get an advantage source in the save d20 (SaveD20); Str checks have no NWN use yet — GetIsSkillSuccessful's d20 (SK11's place) takes it for Athletics / Smith's Tools. *No spells:* AIActionCastSpell refuses spells (spells.2da UserType 1) while raging — feat abilities (UserType 3: Rage itself, Second Wind) pass, feat uses also run through AIActionCastSpell; AIActionItemCastSpell refuses scrolls and wands but allows potions (drinking isn't casting — a design call for you); Concentration comes with Phase 4 (Rage ends it). **Recommendation:** all four, with B1.

**B3. Rage: start, duration, end.** 2024: lasts until the end of your next turn; extended each turn by an attack roll against an enemy, forcing an enemy to save, or a Bonus Action, up to 10 minutes; ends early on donning Heavy armour or being Incapacitated; can't be entered in Heavy armour. NWN: 3 + Con rounds. **Recommendation:** `nwn5e_rage` applies one extraordinary linked effect (icon, visual) for 10 minutes; the DLL ends it (script bridge) when 6 s of unpaused game time pass without an attack roll at an enemy (the attack-roll listener) or a save it forced on an enemy (the save wrapper's `versus`), when it is Incapacitated (IncapacitatedByCondition, dead or dying), or when it wears Heavy armour (a chest item with base AC 6+, NWN's heavy range; entering Rage in it ends at once, with a message). Extending by a Bonus Action waits for Phase 6; a rage started before a fight lapses after a round without attacking, as in 2024.

**B4. Unarmored Defense.** 2024: no armour → base AC 10 + Dex + Con; a shield is allowed. **Recommendation:** build now on the Defense style's two hooks (GetArmorClassVersus for attacks, CNWSCreature::GetArmorClass for the sheet): no armour (chest base AC 0, the engine's own test) → + Con modifier, never below 0 (a negative Con keeps the normal AC; 2024 lets you take the better). A Barbarian/Monk gets the better of the two Unarmored Defenses, not both (Monk brief).

**B5. Danger Sense.** 2024: Advantage on Dex saves unless Incapacitated. Replaces NWN's Uncanny Dodge (B0). **Recommendation:** build now in the save d20 next to the species advantages, when the save's ability is Dex (NWN's Reflex, or a C1 spell mapped to Dex) and the creature isn't Incapacitated.

**B6. Reckless Attack.** 2024: at your first attack roll on your turn, you may take Advantage on Strength attack rolls until your next turn; attacks against you have Advantage then. NWN can't ask. Options: (a) **a radial toggle** — a served feat with unlimited uses (F0's route: feat → spell row → script) switching `nwn5e_reckless` and an icon; while on and in combat, each round from its first attack, its Strength attacks and attacks against it get Advantage (advantage sources in rules_advantage.cpp, as Studied Attacks); (b) automatic while raging; (c) Phase 7. **Recommendation:** (a), off by default. Design call for you: henchmen have no AI for it — suggested: they are reckless whenever they rage (Frenzy needs it).

**B7. Primal Knowledge.** The skill is built (C7b). 2024 also: while raging, Acrobatics, Intimidation, Perception, Stealth and Survival checks can be made as Strength checks. **Recommendation:** with Rage — the 2024 skill bonus (SK2, rules_skills.cpp) takes the better of Strength and the skill's ability while raging (NWN: Tumble, Intimidate, Spot / Listen, Hide / Move Silently; Survival has no NWN use).

**B8. Fast Movement.** 2024: from Barbarian 5, +10 ft while not in Heavy armour. NWN: +10% from level 1, any armour (EE's GetMovementRateFactor has no armour test). **Recommendation:** build now — the HasFeat filter at that one call answers "Barbarian 5+ and not in Heavy armour"; the ruleset float BARBARIAN_ENDURANCE_BONUS answers 0.333 instead of 0.1 (10 ft of a 30 ft speed; the convention of Exhaustion's 5 ft = 1/6, death_saves.h), under the engine's 1.5 cap.

**B9. Feral Instinct (Advantage on Initiative) and Instinctive Pounce (move up to half your speed as part of entering Rage).** Feral Instinct goes in the initiative d20 (rules_initiative.cpp, next to Remarkable Athlete). NWN movement has no per-turn budget, so Pounce gives nothing. **Recommendation:** Feral Instinct now; Pounce recorded, back with Phase 6 if moves get a budget.

**B10. Brutal Strike (9; Improved at 13 and 17).** 2024: using Reckless Attack, give up Advantage on one Strength attack that has no Disadvantage; on a hit +1d10 of the weapon's type (2d10 at 17) and one effect — Forceful Blow (push 15 ft, then move half speed toward it without opportunity attacks), Hamstring Blow (−15 ft speed until your next turn); from 13 Staggering Blow (Disadvantage on its next save, no opportunity attacks until your next turn) and Sundering Blow (+5 on the next attack roll by another creature against it); from 17 two different effects. Two choices every turn. Options: (a) **automatic**: the first Strength attack each round that has Advantage and no Disadvantage while reckless gives up the Advantage; the effect chosen beforehand (`!brutal <effect> [<effect>]`, default Hamstring; `!brutal off`). Every piece exists: Forceful = Push (D4, 4.6 m); Hamstring = Slow's speed decrease at 50%; the dice through GetDamageRoll (rolled twice on a crit); Staggering = the AoO broadcast drops a staggered candidate + the save d20; Sundering = +5 once on the attack modifier. (b) Phase 7 (a per-attack choice). **Recommendation:** (a), after B6.

**B11. Relentless Rage.** 2024: dropping to 0 HP while raging without dying outright → DC 10 Con save; success sets HP to 2 × Barbarian level; DC +5 per use, back to 10 on a Short or Long Rest. **Recommendation:** with Rage, in the existing DoDamage hook (rules_dying.cpp, before the death saves — it reads HP before and after and already decides massive damage): the save through the engine's own roll (Fortitude = Con, shown in the combat log); HP set as Heroic Rally does; the DC a record both rests clear. The Orc's Relentless Endurance (CM6) can share the hook.

**B12. Persistent Rage.** 2024: on rolling Initiative, regain all Rage uses (once per Long Rest); Rage lasts 10 minutes without extending and ends only on Unconscious or Heavy armour. **Recommendation:** with Rage — in the ResolveInitiative hook, clear the spent byte (SetFeatRemainingUses) with a record the rest listener clears; from Barbarian 15, B3's watcher skips the extension and Incapacitated checks (Unconscious = asleep or dying).

**B13. Indomitable Might (18) and Primal Champion (20).** Might: a Strength check or save whose total is below the Strength score uses the score — in the save wrapper, a failed Str save becomes a success when the score ≥ DC (the player told; the engine's line still shows a failure, as Indomitable). Champion: +4 Strength and Constitution, maximum 25, through CM4's ability sync (rules::FaithfulAbilityDelta: recorded, undone in NWN mode). **Recommendation:** both (Champion is existing code; Might in the save wrapper with B2's Str advantage). Only HotU reaches these levels.

**B14. Weapon Mastery (built; one gap).** D3's 2 / 3 / 4 at 1 / 4 / 10 is right, but SRD 5.2.1 limits the Barbarian to **Simple or Martial Melee** weapons and D3 offers any proficient weapon (cls_feat_barb.mastery lists bows, crossbows, sling, dart, shuriken). **Recommendation:** the Barbarian's list becomes melee only (a multiclass character: its classes' lists combined).

**BZ1. Frenzy (Berserker 3).** 2024: raging and using Reckless Attack, the first target you hit each turn with a Strength attack takes extra d6s equal to your Rage Damage, of the weapon's type. **Recommendation:** with B6 — the dice in the GetDamageRoll hook (rules_fighter.cpp, where Great Weapon Fighting and Savage Attacker roll), doubled on a crit; once per turn by the combat-round counter.

**BZ2. Mindless Rage (6).** 2024: immune to Charmed and Frightened while raging; entering Rage ends both. **Recommendation:** with Rage, script only — `nwn5e_rage` links immunity to charm, domination and fear and removes those effects on starting (CN5's frightened respects NWN immunities, as decided; the DLL also drops its fear record for the creature).

**BZ3. Retaliation (10).** 2024: a Reaction when a creature within 5 ft damages you — one melee attack against it. Options: (a) **a stand-in now**: when a creature within 5 ft damages the Berserker, queue the engine's attack of opportunity at it (CNWSCombatRound::AddAttackOfOpportunity) if the round's AoO is unused — the one reaction per round nwn5e already models with the AoO (D2); (b) Phase 6. **Recommendation:** (a) after a short RE of whether the AoO can be queued that way and spends the round's AoO; else Phase 6.

**BZ4. Intimidating Presence (14).** 2024: Bonus Action; creatures of your choice in a 30 ft emanation make a Wisdom save (DC 8 + Str + PB) or are Frightened for 1 minute (repeat the save at the end of each of their turns); once per Long Rest, or spend a Rage use to restore it. **Recommendation:** after Rage, through F0 — a served feat (unlimited uses) runs `nwn5e_intimpres`: every hostile within 9.1 m ("of your choice" = enemies) makes a Will (= Wisdom) save; failure → EffectFrightened 10 rounds (CN5 turns it into the 2024 Frightened); the save is one the Berserker forced (extends Rage, B3); the first use per Long Rest free (a record), later uses spend a Rage use (refused with a message when none left); the DLL rolls the repeat save every 6 s (creature update) and ends the fear on a success; an action until Phase 6.

**NWN mode:** NWN's Barbarian — its Rage (nw_s1_barbrage, NWN's uses), Uncanny Dodge, Damage Reduction, +10% Fast Movement, the epic rage feats; none of the 2024 features. The rest of the 2024 frame stays (Extra Attack, proficiency bonus, Weapon Mastery as D3's NWN mode).

**Build order (proposed).** 1. Existing hooks, no new RE: B0 (the HasFeat filter; DR could also be its ruleset values at 0), B8, B4, B5, B9 Feral Instinct, B13 Primal Champion, B14. 2. Rage: B1 (hook GetFeatRemainingUses / GetFeatTotalUses — exports known; the conditional spells row and `nwn5e_rage.nss`; the Short Rest use back), B2 (the DoDamageImmunity hook — export known; GetDamageBonus; the save d20; the cast refusals), B3 (the watcher), then B7, B11, B12, B13 Might, BZ2. 3. B6 (the F0 toggle), then BZ1, then B10. 4. BZ4. 5. BZ3 after its RE, or with Phase 6 (with Rage, Reckless and Intimidating Presence as Bonus Actions, extending Rage by a Bonus Action, Instinctive Pounce). The Epic Boon with CM8.
*Built 0.21.0 (part 1, key `class_features`):* rules_barbarian.cpp - X1: NWN's
Uncanny Dodge 1-6 and Damage Reduction 1-4 / epic taken off characters
(rules_features' sync, rules::NwnOnlyClassFeats, a chat line) and no longer
granted (cls_feat_barb.if_classfeat2024.txt, flag `classfeat2024`, which also
moves Fast Movement 194 to level 5); B4 Unarmored Defense (+ Con with no
armour, a shield allowed, not stacked with a Monk's Wisdom - X5) on
GetArmorClassVersus and the sheet; B5 Danger Sense in the save d20; B8 Fast
Movement - feat 194 answered for characters (Barbarian 5+, chest base AC < 6),
GetMovementRateFactor hooked to know whose rate, BARBARIAN_ENDURANCE_BONUS a
third for characters (monsters keep NWN's 0.1); B9 Feral Instinct in the
initiative d20; B13 Primal Champion (rules::ClassFeatureAbilityIncreases, in
the 2024 ability sync); B14 the Barbarian's mastery rows melee only
(tools/data/mastery_feats.py CLASS_EXCLUDED) and rules::MasteryAllowed in
`!mastery`, its swap and the fill-by-use - an earlier ranged pick given up
with a word. `!features` lists the Barbarian. Unit tests: the levels, Unarmored
Defense with a Monk, X1's list, Primal Champion's cap, MasteryAllowed.
Launch-checked (hooks in, the rows served). *Tested in game (owner,
2026-10-24, a Fighter 5 made a Barbarian by `!test`'s swap_class, levelled to
7):* B1-B5 pass - the log shows Fast Movement's third, Unarmored Defense +2
(Con), Feral Instinct's advantage on initiative, three melee mastery slots;
Danger Sense's save advantage wasn't rolled that session.
*Built 0.22.0 (part 2, Rage; key `class_features`):* the feat stays NWN's (293
+ successors - the AI's and the dialogs'); spell 307's served row
(spells.if_classfeat2024.txt) runs `nwn5e_rage` - a marker effect of the spell
(GetHasFeatEffect still reads it) and a local the DLL picks up - and
rules_barbarian.cpp keeps the 2024 Rage. Shared pieces built for the next
classes: X6 feat_uses.cpp (a provider takes feats over: total by its rule, one
spent pool in a record; GetFeatRemainingUses / GetFeatTotalUses / Decrement /
Increment hooked; a Long Rest empties it) and the short-rest listener list
(rest.h); X4 resistance.cpp (DoDamageImmunity hooked: a resisted type halved,
rounded down, once); feat_use.cpp (one UseFeat hook: guards + listeners - the
potion resume moved onto it and now follows a Rage too); game_clock.cpp
(unpaused game time). B1 uses 2/3/4/5/6, one back on a Short Rest, a
player's Rage refused in Heavy armour or while raging (nothing spent; an AI's
Rage in Heavy armour gives nothing and keeps its marker, so the AI doesn't
retry). B2 resistance to B/P/S; Rage Damage in GetDamageBonus (melee or a
thrown Throwing Axe, not a finesse weapon used with a better Dex); advantage
on Strength saves; AddCastSpellActions refuses UserType 1 spells,
AddItemCastSpellActions scrolls / wands / rods / staffs (potions allowed). B3
the end checked every 250 ms: a round (6 s game time) without an attack roll
at an enemy, a save forced on one (the save wrapper's `versus`) or a chase
(the attack target alive, an enemy, seen); incapacitated, unconscious (0 HP,
down, asleep), Heavy armour, 10 minutes; a rest ends it. B7 the better of
Strength for Acrobatics / Intimidation / Perception / Stealth / Survival while
raging (SkillValue2024). BZ2 Mindless Rage: charm / domination / fear removed
(CN5's fear icon by CNWSObject::RemoveEffect) and immunity linked. B9
Instinctive Pounce: +50% speed for 6 s. B11 Relentless Rage in rules_dying's
DoDamage (dying faithful), d20 + the hooked GetFortSavingThrow vs DC 10 + 5 per
use, a record both rests clear. B12 Persistent Rage after ResolveInitiative
(initiative faithful), once per Long Rest; lasts 10 minutes. B13 Indomitable
Might after Indomitable in the save wrapper. Rage icon: effecticons row 220
(ife_rage, string 16000444). Any creature that rages gets the 2024 Rage at its
Barbarian level; the uses pool is characters' only. `!rage` (status; `!rage
reset`). Unit tests: uses, damage, the chain, the level gates, Relentless's DC
and HP, Might. Open: whether the engine clears the attack target on a move
order (the chase), AddCastSpellActions' first argument being the spell id (the
log line names it), Rage across a saved game (the DLL's state isn't saved: the
marker stays, the benefits don't).

*Built 0.41.0 (part 3: B6, BZ1, B10, X8; owner, 2026-10-26: "let's go with barbarian part 3, set the
!test command to alter the PC in a way that makes it able to run the matrix"):*
rules_barbarian_strikes.cpp. B6 Reckless Attack - nwn5e's feat 1310 (granted from Barbarian 2,
rules::GrantedFeats2024; on the radial through cls_feat_barb) -> spell 1010 (instant, self) ->
n5_reckless, run in C++ by the new script_hooks.cpp (CVirtualMachine::RunScript, moved out of
light_sources.cpp, which now registers its n5_lamp / n5_flame names there): a toggle kept on the
character (nwn5e_reckless), icon 221, a chat line. While on, its Strength attacks (melee, a thrown
Throwing Axe; not a finesse weapon with a better Dex) get Advantage and from its first attack on,
attacks at it get Advantage until a round after its last (two sources in rules_advantage.cpp's roll).
Other creatures (henchmen) are reckless while raging. BZ1 Frenzy (Barbarian 3 = Berserker 3):
raging + reckless, the first creature it hits each round with a Strength attack takes Rage-Damage d6
(2 / 3 / 4), added in rules_fighter's GetDamageRoll (the weapon's type), doubled on a crit. B10
Brutal Strike (9) - feat 1311 -> master spell 1011 with a sub-radial (X8: rows 1012-1016, Master
1011, FeatID = 16000-16004 * 65536 + 1311, as the game's Wild Shape / horse menu encode theirs; the
feat USESPERDAY **** = unlimited, as the horse menu): Forceful, Hamstring, Staggering, Sundering,
Off -> n5_bs_*; a pick again removes it; one effect below 17, the newer two from 17 (rules::
PickBrutalEffect, kept as nwn5e_brutal; never chosen = Hamstring Blow, every other creature's too);
icon 222. Reckless, the first Strength attack each round whose roll mode is Advantage gives it up
(rules_advantage after the mode); on a hit +1d10 (2d10 at 17; crit doubled) in GetDamageRoll and the
effects: Forceful = PushOnHit at 4.572 m (rules_push.cpp's distance now a parameter; needs weapon
mastery's Push), Hamstring = EffectMovementSpeedDecrease(50) for 6 s, Staggering (13) = Disadvantage
on its next save (rules_save_roll's SaveD20) and no opportunity attacks for a round (rules_opportunity's
GetAIStateReaction during the movement broadcast), Sundering (13) = +5 on another creature's next
attack roll at it within a round (rules_attack's GetAttackModifierVersus, inside an attack roll).
`!reckless`, `!brutal [effect | off]`. Rows: feat.barbarian.txt, spells.barbarian.txt,
cls_feat_barb.if_classfeat2024.txt (+2), effecticons 221 / 222, strings 16000460-16000471. !test:
Berserker 9 (then 13, 17), Str 18, a greataxe, battleaxe, large shield, throwing axes, shortbow.
Unit tests: the levels, Frenzy's dice, the picks (locked, removed, two at 17, the oldest dropped),
the stored choice, the granted feats. Launch-checked: rows served, RunScript hooked.
Open: whether a feat use with ConjTime 0 interrupts the attack round; the sub-radial's FeatID
high word only assumed arbitrary-unique (the game's own are 1004-1093 / 5001-5030).
*0.41.1 (owner: "rage always ends before the PC can attack an enemy, even if they have an enemy
targeted"):* B3's chase read only the engine's attack target (+0x724), which isn't set while the
creature runs at the enemy; it now also counts an ATTACK_OBJECT action at the head of its action
queue (outside RunActions the running action is back at the head). A lapse logs the attack target,
the head action and its target.
*0.50.1 (owner: "I think my henchman is raging twice in a row"):* the log showed Daelan's Rage
lapsing while its AI walked to the next kobold - a plain MoveToPoint (action 1), no attack order and
no attack target - and the AI (and nwn5e's rage-on-initiative) raging again at once: a second use,
then a third. B3's chase now also counts any of the first 8 queued actions naming a living enemy it
sees (an attack order, a move to the enemy), and moving in a fight with such an enemy ahead (within
45 degrees of its facing, 40 m: rules::HeadingToward); a lapse logs the queue's action ids, the
combat state and the facing. The same log showed a henchman Frenzy with no attack behind it: the
engine's AI estimates a weapon's damage (EquipMostDamagingMeleeWeapon / Ammunition,
GetIsWeaponEffective - GetDamageRoll at maximum) and nwn5e's damage additions ran on those, spending
the turn's Frenzy before the real hit. A damage roll outside the attacker's ResolveDamage
(resolve_damage's DamageRollIsEstimate) is now the engine's alone - no Savage Attacker, Frenzy,
Brutal Strike, sneak or Cunning Strike dice. Tests: RageChaseHeading.
*0.50.2 (owner, testing 0.50.1: "the henchman seemed to rage twice in a row before engaging
enemies"; "check if any other barbarian skill uses the same vfx as the rage"):* the log had one Rage
(one start, no lapse, no second use) - the "twice" was the look: spell 307's own conjure ("head")
and cast ("out") animations, half a second each, then the victorymg flourish its start plays. No
other Barbarian feature shares it (Intimidating Presence: the war cry and taunt; Reckless Attack,
Brutal Strike, Frenzy, Retaliation: icons only). Faithful's row 307 now has no conjure / cast
animation and no cast time (spells.if_classfeat2024.txt): the flourish is the Rage's one gesture,
and the Rage is instant, as nwn5e's other class abilities. A henchman's refused second Rage (already
raging) is now logged.
*0.50.3 (owner, testing 0.50.2: "the henchman seemed to rage twice in a row before engaging"):*
this time the log had two Rages: raged at initiative (13:29:19), then no orders at all for 6 s
(`actions []` - the request's ClearAllActions had emptied its queue and its AI hadn't decided again),
a lapse, the next initiative (13:29:27) and a second Rage; its first attack at 13:29:34.
Initiative now only marks the henchman (a minute, the fight); its creature update asks for the Rage
once it closes in on an enemy (B3's chase target) and queues the attack on that enemy after it.
Also: Cunning Strike's Withdraw held only when the target survived (13:31:27: Daze + Withdraw, the
kobold died, nothing) - it is the Rogue's own, so it now applies either way. Test commands for the
rows the owner couldn't set up ("can't create a situation where EL1 and SL1 are testable"):
`!prone [s]`, `!save <ability> <dc>`, `!reflex <dc> <damage>` (test_commands.cpp); later `!skill <skill> <dc>`
(#96).
*0.50.4 (owner: "a weird spot where daelan kept triggering new lines of audio dialog, I believe
when I spawned the first hill giant and he had to rage again"):* 0.50.3's request looped - its
attack order rolls initiative again at once, initiative marked the henchman again, and the next
update asked again, clearing the queued Rage: ~30 asks in 0.6 s at each of the first two fights,
~200 in 4 s at the second giant, a combat voice line each, until one Rage got through (one use
spent). A Rage asked for now isn't asked again for 6 s. The game clock logs pauses ("game clock:
paused / running"): Rage, Withdraw and the repeat saves run on it, and a Withdraw still holding 80
real seconds later (14:13:27 -> 14:14:48) needs reading against them. `!reflex` logs the damage taken.
*0.50.5 (owner: "add a !level command"):* `!level <n> [class]` (test commands): the engine's own
level-ups up to level n in the class - the first class when none is named, another one multiclasses -
as !test's `level` makes them (now one function, AddLevels), without touching abilities or items.
*0.41.2 (owner: "why do enemies have in their radial menu, a barbarian section that is filled with
greyed out options?"):* radial_class_feats.cpp (`radial_hide_self_feats`, client). The game puts every
class feat with OnMenu 1 (cls_feat_*) that the character has on any creature's radial and greys a
self-only one (TargetSelf) when the menu isn't the player's - the game's own Rage did that too. For
another creature's menu the player's self-only class feats are now switched off (OnMenu 0, put back
after) while the client collects the class slots and while a class section's page is built: no greyed
entries, a class left with nothing gets no slot. RE (RVA): 0x98F550 collects one 8-byte record per
player class (count: client stats +0x69): a section (2+ feats), the feat itself (1) or none; called
from the main creature menu (0x994E79: up to 3 laid out inline, else the "more abilities" page
0x991520, callback) with rcx = the menu, which it ignores. 0x98FBF0 CreateClassAbilityMenu (lazy
callback; rdx & 7 = class slot, >> 3 = page). Class feat entry getter 0x2997F0 (16-byte entries at
CNWClass +0x2B0, word count +0x2B8; OnMenu dword +0xC). Entry builder 0x9957A0 greys a TargetSelf
feat when [menu +0x7F8] isn't the player's id. Launch-checked: both hooks at those RVAs.
*0.41.3 (owner: "make it clear the fighting style"):* !test takes the Fighting Style feats (1210-1213)
away when no class has the Fighting Style feature (rules::HasFightingStyleFeature2024: Fighter 1,
Paladin / Ranger 2) - the swapped Fighter's Great Weapon Fighting was still adding to the Barbarian's
damage. A style named under `feats` is still given. The old Fighter's weapon mastery picks stay.
*0.45.4 (owner's BS10 test at level 13: the Topple saves never showed the Disadvantage - "yes, go
ahead with staggering first"):* on one hit Topple's save was rolled (rules_mastery's after-roll
listener) before Brutal Strike set Staggering Blow (its own, later), so only the target's next
save could be the staggered one - and weak enemies died first. 2024 lets the attacker order its
own effects of one hit; Staggering, then the Topple save it gives Disadvantage, is the pick. Topple
now asks rules_barbarian_strikes (BrutalStaggerBeforeSave) to set this hit's Staggering Blow just
before its save; Brutal Strike's step after the roll then leaves the mark alone (that save spent
the Disadvantage; no opportunity attacks for the round still hold). Only Staggering moves: the
listeners' order stays, so Forceful Blow's 15 ft still wins over the Push mastery's 10 ft on one
hit. Testing aid (owner: "increase the enemy's hp to 50"): `creature_min_hp=50` in build
wn5e.ini
([debug], existing); the !test profile is level 13.
*0.46.0 (owner's R6 test: a Rage started in chain mail - "make sure the NWN armor AC matches 2024
and go with A"; key `armor`):* heavy armour was NWN's AC 6+ (the 3e line), so chain mail (NWN's AC 5)
counted as medium and half plate (AC 7) as heavy. NWN's tiers are named by the game's own
descriptions (dialog.tlk 1727-1736: 1 padded, 2 leather, 3 studded leather or hide, 4 scale mail,
5 chainmail, 6 banded or splint, 7 half-plate, 8 full plate) and each is now read as that 2024
armour (rules/armor.h, SRD p. 92): Padded 11 + Dex, Leather 11 + Dex, Studded 12 + Dex (light);
Scale Mail 14 + Dex (max 2), Half Plate 15 + Dex (max 2) (medium); Chain Mail 16, Splint 17,
Plate 18 (heavy, no Dex). For players and henchmen (a monster's AC is its CR's): the AC's
Dexterity - CNWSCreatureStats::GetDEXMod(1) (0x3fd280: armor.2da DEXBONUS by tier, a cap of 0
meaning none, so no table can make heavy armour Dex-free; its callers with 1 are the AC ones:
GetArmorClass, GetArmorClassVersus, the sheet) - answers the 2024 Dexterity; the base difference
(leather / studded -1, chain mail / splint +1, half plate -2) is added where nwn5e adds to the AC
(GetArmorClassVersus, the sheet's GetArmorClass - hooked for it in NWN class mode too). Heavy =
chain mail, splint, plate for Rage and Fast Movement. Tests ArmorTiersAre2024 / ArmorBaseAdjustment
/ ArmorDexterity (mutations: chain mail medium, half plate at 17, heavy's Dex penalty kept - caught).
Not yet: the Strength an armour asks for (Speed -10 ft) and its Stealth Disadvantage; shields (NWN
+1 / +2 / +3, 2024 one Shield +2).
*0.46.1 (owner's BS13 test - the OC's Daelan never raged: "go with B"):* the game's AI
(nw_i0_generic, DetermineClassToUse) lets only Xanos and HotU's Daelan (tags x0_hen_xan,
x2_hen_daelan) rage on their own - "only Xanos gets to do this" - so the OC's Daelan
(nw_hen_dae_01) never did. Now any player's henchman with Rage rages as the fight starts, as that
AI does it (ClearAllActions, ActionUseFeat; GetHasFeat = a use left), when it rolls initiative
(rules_initiative's ResolveInitiative hook, beside Persistent Rage) - not in Heavy armour, not while
raging; faithful only. Enemy Barbarians (option C) wait for the monsters' 2024 stat blocks.
*Built 0.47.0 (part 4: BZ4; owner, 2026-10-27: "let's do barbarian 4" - BZ4 A):* Intimidating
Presence is nwn5e's feat 1314 on the Barbarian's radial (granted at 14; spell row 1017, n5_intimpres
run in C++; icon ife_x3_pdkfear): every enemy within 30 ft (edge distance 9.1 m; "of your choice" =
enemies) makes a Wisdom save - NWN's Will save of the fear type (a fear immunity, a halfling's Brave
count) - against 8 + Str + PB; a failure gets EffectFrightened for a minute, which CN5 turns into
the 2024 Frightened (the fear icon credited to the Berserker). Each round it saves again (the
creature update, every 6 s); a success removes that Berserker's fear (icon and effect). The saves
are ones the Berserker forced, so a Rage goes on (B3's NoteSaveForced). The first use after a Long
Rest is free (record nwn5e_intimidating_presence, cleared by the rest); later ones spend a Rage use,
refused with a word when none is left. A war cry (VFX_FNF_HOWL_WAR_CRY) and the taunt animation. An
action until Phase 6 (2024: a Bonus Action). Test GrantedFeatsFollowClassLevels (14, the DC).
*Built 0.47.1 (part 4: BZ3, the stand-in - decision A):* Retaliation - when a creature in reach
damages a Berserker 10+ (any Barbarian 10+; the SRD's one subclass), the Berserker takes the round's
attack of opportunity at it, which nwn5e already treats as its one Reaction (D2). RE (0.47.1):
CNWSCombatRound::AddAttackOfOpportunity (0x374320) checks nothing - it puts an AoO attack at the
front of a running round and counts it; the checks are the broadcast's
(BroadcastAttackOfOpportunity 0x382eb0, per candidate): an AoO left (round +0x2f40 > 0), round
states +0x2f28 / +0x2f08 clear, in combat (+0x700), +0x6ed / +0x6ef / +0xfc clear, not dead, the
same area, within MaxAttackRange, no ranged weapon, no spell or heal queued, hostile; after it,
round +0x2f40 - 1 and +0x750 = 1. Retaliation does the same, only while it is already attacking
someone (the broadcast adds attack actions first). Every refusal is logged with why (40 lines).
The damage comes from a shared OnApplyDamage hook (damage_listeners.cpp - the weapon mastery's
Vex / Slow / Push moved onto it, unchanged), so Retaliation doesn't depend on weapon mastery.
Also (owner: "use victorymg animation for the rage"): a Rage starts with the mage's victory
animation (ANIMATION_FIREFORGET_VICTORY2, victorymg). Test: HasRetaliation (10).
*The owner's run, 2026-10-31 (#25; 0.58.9, `!test barbarian` at 17 and `!test barbarian14`):* B2 passes
("B2 has been tested"). From the log: B1, B3 (at 14), B6, B9's initiative, B10, B12, BZ1 and BZ4 pass; BZ3
struck back 5 times and was refused 9 (the engine's round mid-swing: the stand-in's limit). B7: "Can't test
b7". For B3 at 17 ("it takes a long time for the rage to end" - Persistent Rage, 10 minutes) a Berserker 14
profile, `!test barbarian14`. A level-17 Rage outlasted that profile's rebuild until a rest (381 s).
*Decided (owner, 2026-10-31: "add !skill and end rage on !test"; #96).* *Built 0.59.1:* `!skill
<skill> <dc>` (test commands) rolls a 2024 skill check for the speaker - d20 + the skill's 2024 total, the
one every check reads (GetSkillRank's) - against the DC, and says which ability counted: Strength, by Primal
Knowledge, while raging. The skills log line says so too ("Str (Primal Knowledge)"; it named the skill's own
ability). A `!test` profile that rebuilds the character ends an active Rage first. Test SkillCheckArguments.
*Accepted (owner, 2026-10-31: "all good"; "96 accepted").* K1-K6 in the log: Strength by Primal Knowledge
for Intimidation, Perception and Stealth while raging (B7 passes), Intelligence for Arcana, the usage line for a
tool and a DC out of range, and "Rage ends after 6 s - the character is rebuilt (!test)". The quickbar K6 left
("missing a lot of the normal icons") was its second row on screen - "it was showing me the quickbar as if
shift were pressed"; nothing was lost. Two leftovers, both fixed on the owner's word ("fix both", #102): Rage's
button moved to the next empty slot at every rebuild (F12, Shift+F5, Shift+F6), and the Rage uses spent went
through it (0 left after K6).
*Built 0.59.3 (#102).* A `!test` rebuild reads the quickbar first; at its cleanup (about 4 s
later, after the features update gave back what the reset took) the feat buttons it took off go back to their
slots - a slot still empty, a feat the character holds again, an action not on the bar elsewhere
(rules::RebuildRestoredSlots) - before anything is placed; then the new character's other actions go on empty
slots. Then the character rests (ForceRest): every use pool, hit points and spells back, as after a Long Rest,
the rest clock held still. Test QuickbarRebuildPutsButtonsBack (5 mutations caught).
*Owner, 2026-10-31 (#103):* "The inventory for each of the test commands should work the same way leveling does,
it should be emptied and only filled up with what's necessary, otherwise the characters will end up with
multiple duplicates of the same items and very encumbered" (K6's log: "walk-limited - can't run (encumbrance
2)"); and on Q5: "For Q5, where's the clock?". *Built 0.59.4:* a profile that builds a character or gives items
(rules::TestProfileEmptiesInventory) empties the inventory and the equipped slots before its items - plot items
(a campaign's quest items), the creature slots (a module's PC skin, claws) and gold kept. `!time` says the
game's hour and date. Test TestProfilesEmptyTheInventory (3 mutations caught).
*0.59.5 (#102; the log of the owner's 0.59.4 run, 2026-10-31):* Rage still went on a new slot at each Barbarian
rebuild (slots 2, 12, 13). NWN's Rage is 293 and its successors 326-331, and the engine moves a Rage button
along that chain as the levels go and come back - the old slot held the engine's Rage 5 (328), not empty and
not "Rage" to the quickbar code, so nothing went back and another Rage (293) was placed (the owner saw Rage in
its old slot, with a second one further on). The chain is one action now (rules::SameQuickbarAction, as Second
Wind's three feats): a button on any of its feats is the Rage, left as the engine set it, and one a rebuild
took off goes back as 293. Test QuickbarRageChainIsOneAction (4 mutations caught).
*Accepted (owner, 2026-10-31: "these ones all pass"; "Q1b, B11, B13 and B1b pass"; "102 and 103 accepted").*
#102: a rebuild's buttons keep their slots (Rage through its chain, 0.59.5) and the character starts rested;
#103: the inventory emptied before a profile's items, and `!time`.
*Decided (owner, 2026-10-31: "add !fear"; #104) - for BZ2 (Mindless Rage), which needs the Barbarian charmed or
frightened.* *Built 0.59.6:* `!fear [charm] [seconds]` (test commands) - the speaker Frightened, or Charmed, by
the nearest enemy (by itself when none is near) for 30 s, at most 120: the engine's own effect, as a spell's,
which CN5 makes the 2024 Frightened (no flight). A Rage entered under it ends it; one under way turns it away
(Mindless Rage's immunity). Test FearArguments (7 mutations caught).
*Accepted (owner, 2026-11-01: "F2, the fear icon disappeared"; "104 and 106 accepted").* F1-F3 and F5 on 0.59.6,
F4 through #106's C4 (Rage while charmed ends the charm). BZ2 passes with it.
*Accepted (#25, Barbarian parts 2-4; owner, 2026-11-01: "B9 is goo[d]"; "HR is good").* The last rows on 0.59.17:
B9 (Instinctive Pounce's speed in the Rage's first round, by eye) and HR (a Barbarian henchman rages as a fight
starts); the rest passed on 0.59.4-0.59.6 (Q1b, B1b, B7 through #96, B10b, B11, B13, BZ2; BZ3 within the
stand-in's limit, decision A).
*#109 (the tools session, adding `!test anims` for #108: a profile's `equip` knew only armour, shields and the off
hand - a cloak went to the right hand). Built 0.59.14:* each item goes to its own slot - cloak, helmet, boots,
gloves or bracers, belt, amulet, a ring to the right hand's ring (the next to the left), arrows / bolts / bullets
- weapons as before; `anims.txt` equips its cloak.
*Accepted (owner, 2026-11-02: "accept 109").* The cloak worn in the owner's `!test anims` runs (the animation
session's report); the other slots go the same way, unused by any profile yet.
*Recon facts (EE 89.8193.37-17; RVA):* GetMovementRateFactor 0x391FE0 — HasFeat(194) at 0x39202A (checked again by the session: `mov edx,0C2h; call HasFeat 0x40B900`), ruleset BARBARIAN_ENDURANCE_BONUS 0.1 (row 307), caps MOVEMENT_SPEED_PENALTY_CAP 0.125, BONUS_DEFAULT_CAP 1.5, MONK_CAP 3.0. DoDamageReduction 0x491950 — HasFeat for 1067/334/333/332/196 at +0x177/+0x219/+0x291/+0x309/+0x37E. SavingThrowRoll 0x3EC600 — HasFeat 255…251 at 0x3ED046/0x3ED115/0x3ED150/0x3ED188/0x3ED1C0, its only d20 at 0x3ED260 (ruleset UNCANNY_DODGE_LEVEL_2–6 = 1–5). Uncanny Dodge 2 (251) in ResolveSneakAttack +0x536 and ResolveDeathAttack +0x32A. GetArmorClassVersus 0x3F47F0 — 195 at +0x988/+0x999 and 949 at +0x9AB/+0x9BC via CExoArrayList::Contains (0x296910) on stats+0 / +0x20. GetFlatFooted 0x3E3910 — HasFeat(226 Uncanny Reflex) = never flat-footed. Script GetHasFeat (0x525410): GetHighestLevelOfFeat +0xBD, HasFeat +0xD3, GetFeatRemainingUses +0xF5. GetHighestLevelOfFeat 0x4029B0 (UseFeat +0x3C, the script feat commands). GetFeatRemainingUses 0x4012F0 (total inline, minus the entry's byte +2, clamped 0–100; callers UseFeat, AIActionCastSpell, PlayerUpdate, talents, ResolveDamage, ResolveMeleeSpecialAttack, SetCombatMode, SaveClassInfo, scripts). GetFeatTotalUses 0x401C70 (ReadStatsFromGff, SetFeatRemainingUses 0x415960; feat 331: + (level − 20) / 4). Increment / DecrementFeatRemainingUses 0x40B9D0 / 0x3F2750 change exactly that feat id's byte. Feat 293: SPELLID 307, USESPERDAY 1, TARGETSELF 1; spell 307 NW_S1_BarbRage, UserType 3. DoDamageImmunity 0x491690 (virtual +0x160; % from +0x170; removes max(1, dmg × % / 100)). OnApplyDamage 0x428740: immunity +0x160, DoDamageResistance +0x150 (0x492620), DoDamageReduction +0x148 for flags & 0x1007. AIActionCastSpell 0x3ADA70, AIActionItemCastSpell 0x3C07B0, AddAttackOfOpportunity 0x374320, GetHasFeatEffectApplied 0x495480. Scripts with feat 293: nw_i0_generic, x0_d1_userage / x0_d2_hasrage, x2_i0_spells.
*Open / unverified:* a feat use and the Short Rest's +1 landing on the same entry (the highest Rage feat); ResetFeatRemainingUses just zeroing the byte; GetHasFeatEffectApplied = the script's GetHasFeatEffect; BZ3's AoO queueing; OnApplyDamage's second loop (+0x1238) not examined; flat-footed in faithful (Rogue brief); Rage Damage on thrown weapons (ranged = Dex in rules::AttackAbilityModifier); served feats with USESPERDAY `****` unlimited; design calls — Heavy = chest base AC 6+ (2024's Half Plate is Medium), potions allowed while raging, henchmen reckless whenever raging, "of your choice" = all enemies; renaming tlk strings (1062 …) by conditional served rows → private ids (assumed).

### Phase 3 — Monk (CM9): recommendations (for the owner's decision)

SRD 5.2.1 Monk (pp. 49–52, checked 2026-10-24); every 2024 feature below and the one subclass, **Warrior of the Open Hand**, are in the SRD (Mercy, Shadow, the Elements are PHB-only — private layer). Built already: d8 (CM3), Str/Dex saves (C2), 2024 skills, Dex and Wis 13 to multiclass (CM2), the kit (spear, 5 daggers; C2), no Weapon Mastery (D3), Extra Attack at 5 (1a), ASIs (CM7), Open Hand at 3 (CM10). NWN's Monk (EE `cls_feat_monk`): 1 Monk weapon proficiency (club, crossbows, dagger, handaxe, kama, quarterstaff, shuriken, sling), Improved Unarmed Strike, Flurry of Blows (a combat mode), Evasion, Stunning Fist, Monk AC Bonus, Cleave; 2 Deflect Arrows; 3 Fast Movement ("MonkEndurance"), Still Mind; 5 Purity of Body; 6 Knockdown, Improved Knockdown; 7 Wholeness of Body; 9 Improved Evasion; 10/13/16 Ki Strike +1/+2/+3; 11 Diamond Body; 12 Diamond Soul (spell resistance); 15 Quivering Palm; 18 Empty Body; 20 Perfect Self. The engine computes: unarmed die d6/d8/d10/d12/d20 from Monk 1/4/8/12/16; AC + Wis + Monk level / 5 (no armour, no shield); speed +10% per 3 Monk levels; Stunning Fist uses = Monk level per day. **Faithful today:** changed by built rules — attacks use PB, no iterative −3, 1 attack (2 from Monk 5) instead of NWN's monk table, unarmed attacks use Strength (Weapon Finesse isn't offered, CM8), Small monks roll Medium dice (`size`), Still Mind's +2 dropped (C7); still NWN's — Flurry (+1 attack, −2 to all), Stunning Fist's DC (C6 rescaling it: unverified), Monk AC Wis + level / 5, the die progression, Fast Movement, Ki Strike, spell resistance, the immunities, Empty Body, Quivering Palm's instant death.

| Lvl | 2024 feature | Status |
|---|---|---|
| 1 | Martial Arts (die d6 / d8 at 5 / d10 at 11 / d12 at 17; Dexterous Attacks; Bonus Unarmed Strike) | M1 |
| 1 | Unarmored Defense | M2 |
| 2 | Monk's Focus (Focus Points = Monk level) | M3 |
| 2 | Flurry of Blows | M4 |
| 2 | Patient Defense, Step of the Wind | M5 |
| 2 | Unarmored Movement (+10 ft; +15 at 6, +20 at 10, +25 at 14, +30 at 18) | M6 |
| 2 | Uncanny Metabolism | M7 |
| 3 | Deflect Attacks | M8 |
| 3 | Warrior of the Open Hand | automatic (CM10) |
| 4, 8, 12, 16 | Ability Score Improvement | built (CM7) |
| 4 | Slow Fall | M6 (no NWN use) |
| 5 | Extra Attack | built (1a) |
| 5 | Stunning Strike | M9 |
| 6 | Empowered Strikes | M10 |
| 7 | Evasion | M11 (with the Rogue's) |
| 9 | Acrobatic Movement | M6 (no NWN use) |
| 10 | Heightened Focus | M4, M5 |
| 10 | Self-Restoration | M12 |
| 13 | Deflect Energy | M8 |
| 14 | Disciplined Survivor | M13 |
| 15 | Perfect Focus | M7 |
| 18 | Superior Defense | M14 |
| 19 | Epic Boon | with CM8 |
| 20 | Body and Mind | M15 |
| Open Hand 3 | Open Hand Technique | OH1 |
| Open Hand 6 | Wholeness of Body | OH2 |
| Open Hand 11 | Fleet Step | OH3 (Phase 6) |
| Open Hand 17 | Quivering Palm | OH4 |
| — | NWN-only Monk feats | M16 |
| — | Proficiencies | M17 |

**M0. The feature mask (with the Barbarian's B0: the same HasFeat filter).** CNWSCreatureStats::HasFeat (exported, 674 call sites) answers the Monk's NWN feats as their 2024 counterparts' levels: Monk AC Bonus (260) → Unarmored Defense from 1; Fast Movement (207) → Unarmored Movement from 2; Stunning Fist (39) → Stunning Strike from 5; Ki Strike (213) → Empowered Strikes from 6 (343, 344 false); Improved Evasion (212) → Evasion from 7 (Evasion 206 false); Wholeness of Body (211) at Open Hand 6; Quivering Palm (296) at Open Hand 17; Deflect Arrows (8) and the M16 feats false. Passive features answer true before NWN would grant them (Unarmored Movement at 2, Empowered Strikes at 6) even without the feat; usable ones come through rules_features' granted feats (as Second Wind: recorded, removed in NWN mode), e.g. Wholeness at Open Hand 6. Names and descriptions by the label hook (2024 name, SRD text); masked feats stay on the sheet described "not used (2024 rules)" until Phase 7 hides them. Engine reads that bypass HasFeat get their own hook (GetACNaturalBase reads the list directly, M2). The hook must be cheap (a small feat-id set first). Campaign GetHasFeat checks read the mask (e.g. x0_i0_match's FEAT_STUNNING_FIST, used by the AI). **Recommendation:** build first, with the Monk's table; the other classes add theirs.

**M1. Martial Arts.** 2024, while unarmed or wielding only Monk weapons (Simple Melee + Martial Melee with Light: scimitar, shortsword) with no armour and no shield: (a) Dexterous Attacks — Dex for attack and damage of Unarmed Strikes and Monk weapons, and for the Grapple / Shove DC; (b) the Martial Arts die (1d6 / 1d8 / 1d10 / 1d12 at 1 / 5 / 11 / 17) in place of the normal damage; (c) Bonus Unarmed Strike (Bonus Action). NWN: the gate is GetUseMonkAbilities (Monk levels, chest AC 0, no small / large / tower shield; it ignores the weapon); "Monk weapon" is the base item's IsMonkWeapon (kama, torch); unarmed dice from GetUnarmedDamageDie / Dice (ruleset MONK_DAMAGE_TIER_* under GetUseMonkAbilities, else 1d3). Route: (a) faithful's GetWeaponFinesse hook (`finesse`) answers 1 for unarmed attacks and Monk weapons while Martial Arts holds; FinesseDamageBonus extended to them; SK6's Shove DC takes the better of Str and Dex for a Monk. (b) Unarmed: hook GetUnarmedDamageDie / Dice for characters only (as Extra Attack; monsters keep NWN's), or globally by ruleset answers (thresholds 4/8/12/16 → 5/11/17/21, dies → 6/8/10/12); Monk weapons: the weapon-dice filter (GetDamageRoll's two weapon RollDice calls, as Great Weapon Fighting) rolls the Martial Arts die when it is larger; rules::IsMonkWeapon2024 (club, dagger, handaxe, light hammer, light mace, quarterstaff / magic staff, short spear, sickle, scimitar, short sword; kama as sickle, kukri as dagger), also written into the base items' IsMonkWeapon after every rules load (CM0 overlay) so NWN's Flurry and monk tables accept them. (c) A stand-in until Phase 6: one more attack each round while Martial Arts holds, on the round's additional-attack count after InitializeNumberOfAttacks (Haste and Flurry use it) — not in a round with an off-hand attack (2024: the Light extra attack takes the same Bonus Action); it uses the weapon in hand, not strictly an unarmed strike. Options for (c): (i) always; (ii) only in Flurry mode, as NWN. **Recommendation:** (a), (b), (c)(i), now.

**M2. Unarmored Defense.** 2024: no armour, no shield → base AC 10 + Dex + Wis. NWN: GetACNaturalBase adds Wis (when positive, feat 260 in the list) + Monk level / 5 under GetUseMonkAbilities. **Recommendation:** build now — hook GetACNaturalBase, drop the level / 5 in faithful (Wis as it is: a negative Wis gives plain 10 + Dex either way); with the Barbarian's (Con) the higher one counts, not both.

**M3. Monk's Focus (Focus Points).** 2024: points = Monk level from 2; all back on a Short or Long Rest; Focus DC = 8 + Wis + PB. Route: one pool per character — the spent count in a local (`nwn5e_focus_spent`) cleared by the rest listener and the Short Rest (rules_rest hard-codes Second Wind today: a short-rest listener list); each Focus feat — Stunning Strike (1), Patient Defense (1), Step of the Wind (1), Superior Defense (3), Quivering Palm (4) — shows the pool on the radial and quickbar: GetFeatTotalUses / GetFeatRemainingUses answer pool ÷ cost, DecrementFeatRemainingUses spends from the pool (all three exported; the first two already special-case Stunning Fist); `!focus` shows it. (One use-row per feat, Second Wind's way, can't share a pool.) **Recommendation:** the pool, now — M4, M5, M7, M9, M13, M14, OH4 need it.

**M4. Flurry of Blows (2; three strikes at 10, Heightened Focus).** 2024: 1 FP for two Unarmed Strikes as a Bonus Action (replacing M1(c)'s one). NWN: combat mode 5 — InitializeNumberOfAttacks gives +1 additional attack when unarmed or with a monk weapon (else switches the mode off); every attack at −2 (FLURRY_OF_BLOWS_TO_HIT_MODIFIER). Route: the mode is the 2024 switch — each round with Martial Arts and a point left spends 1 FP: M1(c)'s strike + 1 more (+ 2 more from Monk 10), no off-hand attack; out of points, M1(c) alone; the −2 answered 0. Options: (a) spend per round (a Monk 5: 5 Flurry rounds per rest — 2024's economy); (b) once per fight (cheaper than 2024). **Recommendation:** (a).

**M5. Patient Defense and Step of the Wind (2; Heightened Focus at 10).** 2024: Bonus Action, free: Disengage (PD) or Dash (SotW); for 1 FP: Disengage + Dodge (PD) or Disengage + Dash, jumps doubled (SotW); from 10, PD's FP use also gives temp HP = 2 Martial Arts dice and SotW can carry a willing ally. NWN: nothing (Tumble is D2's Disengage stand-in). Route: F0 (Second Wind's pattern) — served feats (1 FP) → spells.2da rows → nwn5e scripts. PD: no opportunity attacks this round (the Tactical Shift flag, F7), Dodge until the Monk's next round (disadvantage on attacks against it — a source in rules_advantage; advantage on Dex saves; lost when Incapacitated), the temp HP from 10. SotW: no opportunity attacks this round, EffectMovementSpeedIncrease(100) for the round (Monk cap 3.0). Both an action until Phase 6 (as Second Wind); the free versions with Phase 6; the ally carry and jumps have no NWN use. **Recommendation:** the FP versions now.

**M6. Unarmored Movement (2), Slow Fall (4), Acrobatic Movement (9).** 2024: speed +10 / 15 / 20 / 25 / 30 ft at 2 / 6 / 10 / 14 / 18 with no armour and no shield (of 30 ft, Exhaustion's convention: +33 / 50 / 67 / 83 / 100%). NWN: Fast Movement (207, Monk 3) adds 10% per 3 Monk levels in GetMovementRateFactor (0.1 a literal, not a ruleset value; caps ruleset 1.5, monk 3.0). Route: hook GetMovementRateFactor, the 2024 monk part instead of NWN's. Slow Fall (a Reaction against falling damage) and Acrobatic Movement (walls, liquids): NWN has no falls or climbing. **Recommendation:** Unarmored Movement now; the other two "no NWN use".

**M7. Uncanny Metabolism (2) and Perfect Focus (15).** 2024: on rolling Initiative you may regain all FP and heal Martial Arts die + Monk level, once per Long Rest; from 15, if you don't use it, FP rise to 4 when at 3 or fewer. Route: the ResolveInitiative hook (SK9's), with a "new fight" guard (D4's round counter; how often ResolveInitiative runs is unverified). Options for Uncanny Metabolism: (a) automatic at the first initiative where at least half the points are spent, or the Monk is Bloodied; (b) whenever any point is spent; (c) Phase 7, as a choice. **Recommendation:** (a); Perfect Focus automatic.

**M8. Deflect Attacks (3) and Deflect Energy (13).** 2024: a Reaction when hit by an attack whose damage includes B/P/S (any type from 13): reduce its damage by 1d10 + Dex + Monk level; if that brings it to 0, 1 FP redirects it (the creature saves Dex against the Focus DC or takes 2 Martial Arts dice + Dex). NWN: Deflect Arrows (8, Monk 2) in ResolveDefensiveEffects — once per round, a ranged hit, a free hand, not flat-footed, Reflex DC 20 → the shot misses. Route: mask Deflect Arrows; the reduction automatic on the first qualifying hit each round (the round's Reaction) in the OnApplyDamage hook, lowering the damage effect's per-type amounts, physical first (RE: telling an attack's damage effect from other damage); Phase 6 makes it share the Reaction with opportunity attacks; the redirect (a choice to spend) Phase 6 / 7. **Recommendation:** the reduction now, the redirect later.

**M9. Stunning Strike (5).** 2024: once per turn on a hit with a Monk weapon or an Unarmed Strike, 1 FP: the target saves Con against the Focus DC — failure: Stunned until the start of the Monk's next turn; success: speed halved and the next attack roll against it before then has advantage. NWN: Stunning Fist (39, Monk 1), a special attack chosen before the roll: −4 to hit, the use spent before the hit test (a miss wastes it); on a hit dealing damage, if not stun-immune: Fort DC 10 + level / 2 + Wis (+ Epic Improved Stunning Fist) or stunned 6 s; Monk level uses per day. Route: Stunning Fist becomes Stunning Strike from 5, unarmed attacks and Monk weapons only, 1 FP from the pool, refunded on a miss (2024 spends only on a hit), the −4 answered 0; the save wrapper rolls Con against 8 + Wis + PB (identified by its call site in ResolveMeleeSpecialAttack — the feat id isn't passed); the 6 s stun stays (one NWN round = until the Monk's next round); on a success EffectMovementSpeedDecrease(50) for the round and a mark giving the next attack against it advantage (as Vex). Options: (a) the special attack, declared before the hit, refunded on a miss; (b) automatic on the first hit each round while a `!stun` toggle is on. **Recommendation:** (a).

**M10. Empowered Strikes (6).** 2024: Unarmed Strikes can deal Force damage. NWN: Ki Strike (10 / 13 / 16) makes unarmed attacks +1 / +2 / +3 against DR (CalculateDamagePower reads KI_STRIKE_LEVEL_1..5 by feat 213, 343, 344, epic 697 / 698 for unarmed attacks under monk tables); NWN's DR X/+N plays 2024's "resistance to nonmagical B/P/S", which Force ignores. Route: mask Ki Strike into Empowered Strikes from 6 (213 true, 343 / 344 false), KI_STRIKE_LEVEL_1 answered 20 (the top damage power): unarmed strikes pierce every DR X/+N; per-type resistances (X/-) stay. (A real Force damage type would mean rewriting the damage split — not proposed.) **Recommendation:** build now.

**M11. Evasion (7).** 2024 Evasion = 3e's Improved Evasion (nothing on a success, half on a failure; Dex saves only; not while Incapacitated). NWN: Evasion at 1, Improved Evasion at 9; the reflex-damage handlers ask HasFeat 212, then 206. Route: the mask (212 true from 7, 206 false) plus Incapacitated and Dex-only tests in the handlers (the C1 follow-up: Cone of Cold and the like now roll Con but still apply Evasion). **Recommendation:** with the Rogue's Evasion (Rogue 7) — one piece of code for both.

**M12. Self-Restoration (10).** 2024: at the end of each turn, remove one of Charmed, Frightened, Poisoned; no Exhaustion from going without food. Route: once per round (the round counter) one of charm / domination, nwn5e's fear (CN5) or a poison effect (CN4) is removed through the script bridge; hunger has no NWN use. **Recommendation:** build now.

**M13. Disciplined Survivor (14).** 2024: proficiency in all saving throws; a failed save can be rerolled for 1 FP. NWN: Diamond Soul (12) is spell resistance (M16). Route: rules_saves gives all six from Monk 14 (a class feature, multiclass too); the reroll in the save wrapper as Indomitable — automatic while a point is left, after Indomitable. **Recommendation:** with M3.

**M14. Superior Defense (18).** 2024: at the start of the turn, 3 FP: Resistance to all damage except Force for 1 minute or until Incapacitated. Route: a Focus feat (3) whose script applies EffectDamageImmunityIncrease(type, 50) for every type but Magical (50% immunity = 2024 Resistance; or the Barbarian's B2 DoDamageImmunity route), 60 s, removed early when Incapacitated (creature update); from the radial like Second Wind, its action cost Phase 6's. **Recommendation:** with M3.

**M15. Body and Mind (20).** 2024: Dex and Wis +4, maximum 25. Route: CM4's ability delta (rules::FaithfulAbilityDelta), the cap 25 for these two. **Recommendation:** build now.

**M16. NWN-only Monk features: absent in faithful (the mask).** Still Mind (already dropped by C7), Purity of Body (disease immunity), Diamond Body (poison immunity), Diamond Soul (SR 10 + level), Empty Body (50% concealment), Perfect Self, Improved Unarmed Strike (nothing left for it: unarmed attacks no longer provoke, D2), Cleave, the epic monk feats. Knockdown / Improved Knockdown (6) are SK6's Shove (every 2024 character has it): they stay as NWN grants them — Shove for everyone is SK6 / CM8's. **Recommendation:** as listed.

**M17. Proficiencies.** 2024: Simple weapons + Martial weapons with Light (scimitar, short sword); no armour; one artisan's tool or musical instrument. NWN: Monk proficiency (feat 49: club, crossbows, dagger, handaxe, kama, quarterstaff, shuriken, sling). Route: one hook on CNWSCreature::CheckProficiencies (exported) driven by each class's 2024 list — shared with every brief and CM2's limited multiclass lists; kama, kukri, shuriken count as sickle, dagger, dart (simple); the tool only recorded. **Recommendation:** with the all-class proficiency item (the Monk's list first if that lands now).

**OH1. Open Hand Technique (Open Hand 3).** 2024: each hit from a Flurry of Blows attack imposes one of Addle (no opportunity attacks until its next turn), Push (Str save against the Focus DC or pushed 15 ft), Topple (Dex save or Prone). Route: D4's Push (rules_push, 4.5 m) and the engine's knockdown with the Focus DC and the right abilities; Addle a "no opportunity attacks" mark (rules_opportunity); needs which attacks are M4's Flurry attacks (RE: their index in the round); the choice a setting, `!technique topple|push|addle` (default Topple), like `!mastery`. **Recommendation:** after M4.

**OH2. Wholeness of Body (Open Hand 6).** 2024: Bonus Action, heal Martial Arts die + Wis (at least 1); uses = Wis modifier (at least 1) per Long Rest. NWN: feat 211 (Monk 7, once a day) → spell 309 nw_s2_wholeness, heals 2 × Monk level. Route: feat 211 granted at Open Hand 6 (rules_features), spell row 309's script a served nwn5e script, uses max(1, Wis) through GetFeatTotalUses, not restored by a Short Rest, an action until Phase 6. **Recommendation:** build now.

**OH3. Fleet Step (Open Hand 11).** Step of the Wind right after another Bonus Action. **Recommendation:** Phase 6.

**OH4. Quivering Palm (Open Hand 17).** 2024: on an Unarmed Strike hit, 4 FP start vibrations lasting Monk-level days (one creature at a time); ending them takes an action or forgoes one attack of the Attack action: Con save against the Focus DC, 10d12 Force, half on a success. NWN: feat 296 (Monk 15, once a day), a special attack: on a hit Fort DC 10 + level / 2 + Wis or death (QUIVERING_PALM_BASE_DC, ResolvePostMeleeDamage). Options: (a) one step — the special attack (unarmed, 4 FP); on a hit the Con save at once for 10d12 Magical (NWN's Force), half on a success (the save wrapper tells the engine "success", so its death never happens; nwn5e applies the damage); (b) two steps — the hit marks the target, `!palm` or a second use ends it later (closer to 2024, more interface). **Recommendation:** (a) now; (b) with Phase 6 / 7 if wanted.

**NWN mode:** NWN's Monk as it is (its feats and engine numbers, Flurry's −2, Stunning Fist, Quivering Palm's death, prestige classes); the both-mode rules stay (PB, Extra Attack's count, no iterative penalty, 2024 saves). No Focus Points, no Open Hand.

**Build order (proposed).** 1. M0 (the mask, the Monk's table and labels), then M2, M6, M10, M15, M16 (known hooks: HasFeat, GetACNaturalBase, GetMovementRateFactor, the ruleset). 2. M1 (the finesse hook, GetUnarmedDamageDie / Dice, the weapon-dice filter, the base items' IsMonkWeapon, the extra attack after InitializeNumberOfAttacks). 3. M3 (the pool, the feat-use hooks, the short-rest listener), then M4, M9, M7, M13, M14, M5. 4. OH2, OH4, OH1. 5. M8 (RE: an attack's damage effect at OnApplyDamage), M12, M11 with the Rogue's. 6. M17 with the all-class proficiency item. Later: Phase 6 (the free Patient Defense / Step of the Wind, Fleet Step, Deflect's redirect, Bonus Unarmed Strike as a Bonus Action), CM8 (Epic Boon).

*Recon facts (EE 89.8193.37-17; RVA; read by disassembly):* ruleset label sites (the call sites reference the label minus its first character — the compiler folds it into the FNV seed): MONK_DAMAGE_TIER_* in GetUnarmedDamageDie / Dice; FLURRY_OF_BLOWS_TO_HIT_MODIFIER GetAttackModifierVersus 0x3F6693; STUNNING_FIST_BASE_SAVE_DC 0x3E6D1D and _EFFECT_DURATION 0x3E7049 (ResolveMeleeSpecialAttack); STUNNING_FIST_TO_HIT_MODIFIER 0x412F1E; QUIVERING_PALM_BASE_DC 0x3E938A; DEFLECT_ARROWS_DC 0x3E5EAF; DIAMOND_SOUL_SPELL_RESISTANCE_BASE 0x408E0F; KI_STRIKE_LEVEL_* 0x385825; STILL_MIND_COMPETANCE_BONUS 0x3EC9FC; FEAT_EXTRA_STUNNING_ATTACK_USES in GetFeatTotalUses / GetFeatRemainingUses; MOVEMENT_SPEED_BONUS_MONK_CAP in GetMovementRateFactor. GetUseMonkAbilities 0x396370 (class 5 or 40 levels, chest ComputeArmorClass 0 — class 40 also 1–3 — no left-hand base item 14 / 56 / 57; ignores the weapon; callers GetACNaturalBase, GetArmorClassVersus, GetUnarmedDamageDie / Dice, GetUseMonkAttackTables, movement, EquipMostDamagingMeleeWeapon, UpdateCombatInformation). GetUnarmedDamageDie / Dice 0x40AD60 / 0x40AA70 (tier by Monk level < 4 / 8 / 12 / 16; Medium dies 6 / 8 / 10 / 12 / 20, dice 1; Small *_SDAMAGE_*; else UNARMED_DAMAGE_DIE 3; callers GetDamageRoll +0x67D / +0x68B / +0xA08 / +0xA16, GetUnarmedDamageRoll 0x40B3E0, UpdateCombatInformation). GetACNaturalBase 0x3F3E90 (+ Wis at stats +0xD41 when > 0 if the list Contains 260; + Monk level / 5; argument 1 returns that part only). GetMovementRateFactor 0x391FE0 (Monk 207 under GetUseMonkAbilities: + (level / 3) × 0.1 literal at .rdata 0x140D83B74; caps 1.5, 3.0 for Monks). InitializeNumberOfAttacks 0x3773A0 (combat mode byte creature +0x6EF: 5 Flurry, 6 Rapid Shot; Flurry: round +0x2F60 = 1 when unarmed or the right-hand base item's +0xC0 IsMonkWeapon; else Shou Disciple feats 866 / 899 then SetCombatMode(0, 1); HASTED_BONUS_ATTACKS into +0x2F60; total = +0x2F50 + 0x2F54 + 0x2F60 + 0x2F64). GetIsDefaultMonkWeapon 0x29DD40 (base items 15, 40). GetUseMonkAttackTables 0x40B510. Stunning Fist in ResolveMeleeSpecialAttack 0x3E6660 (special attack id at attack data +0x7A; DecrementFeatRemainingUses(39) at 0x3E6CAC before the hit test; needs a hit with damage, no stun immunity; DC 10 + level / 2 + Wis + EPIC_IMPROVED_STUNNING_FIST_DC_BONUS; Fort save 0x3E6FDD; 6.0 s). GetFeatTotalUses 0x401C70 (Stunning Fist = Monk level, else (level + 3) / 4; +3 with feat 410; cap 100). Deflect Arrows in ResolveDefensiveEffects 0x3E5630 (ranged attack data +0x5C; round flag +0x2F30, SetDeflectArrow 0x379C20; not flat-footed; feat 8; free hand or non-ranged main weapon of relative size ≤ 0; Reflex DC 20). Ki Strike in CalculateDamagePower 0x385610 (698 / 697 / 344 / 343 / 213 → KI_STRIKE_LEVEL_5..1). Evasion in ExecuteCommandGetReflexAdjustedDamage 0x52F790 (HasFeat 212 then 206). Exports: HasFeat 0x40B900, DecrementFeatRemainingUses 0x3F2750, IncrementFeatRemainingUses 0x40B9D0, SetFeatRemainingUses 0x415960, CheckProficiencies 0x3D2D90, ResolveInitiative 0x39F100 (callers AddAttackActions, EventHandler ×3, AIActionCounterSpell, OnApplyDamage ×2), SetCombatMode 0x3D8C50, GetSpellResistance 0x408DD0, DoDamageResistance 0x492620. (Session spot-check: the exports' RVAs match.)
*Open / unverified:* how often ResolveInitiative runs (M7's guard); base item +0xC0 = the loaded IsMonkWeapon and whether writing it suffices for Flurry / monk tables, and whether SetCombatMode(5) checks feat 204 or armour; whether +0x2F60 attacks are main-hand or unarmed, their place in the round (OH1), the round's attack limit; SavingThrowRoll's feat argument at the Stunning Fist / Quivering Palm sites (0 — identified by call site); whether C6 rescales Stunning Fist's DC today; whether NWN limits Stunning Fist to unarmed; telling an attack's damage effect at OnApplyDamage and editing its amounts (M8); whether the radial / quickbar refresh pool-backed uses; whether using a feat costs the round's attacks; henchman / monster AI use of Flurry and Stunning Fist (melee talents, category 22; OC's Grimgnaw as the monk henchman is from memory); whether damage power 20 is the ceiling (M10); the class 39 constant in GetACNaturalBase; masked feats on the sheet until Phase 7; whether +100% speed at Monk 18 feels right.

**Built - part 1 (0.52.0; owner, 2026-10-28: "Sure, monk").** src/core/rules_monk.cpp:
- *M0 / M16 (X1):* rules_features takes off characters Still Mind, Purity of Body, Diamond
  Body, Diamond Soul, Perfect Self, Empty Body, Ki Strike 2-3 and the epic Ki Strikes (any
  character - only Monks have them) and, from characters with Monk levels only, Cleave,
  Improved Unarmed Strike and the epic Improved Stunning Fist (anyone may have picked those
  in NWN). The faithful Monk table (data/rows/cls_feat_monk.if_classfeat2024.txt) no longer
  grants them, nor Evasion; Fast Movement comes at 2, Ki Strike at 6, and Improved Evasion
  from nwn5e at 7. Monk AC Bonus, Fast Movement and Ki Strike carry the 2024 names
  (Unarmored Defense, Unarmored Movement, Empowered Strikes). Stunning Fist, Flurry of Blows,
  Deflect Arrows, Wholeness of Body and Quivering Palm stay NWN's until their parts.
- *M2:* GetACNaturalBase hooked - a character's Monk level / 5 is taken off (10 + Dex + Wis).
- *M6:* GetMovementRateFactor (read: the effects' rate, + Barbarian Fast Movement, then -
  with HasFeat(207) and the Monk abilities - the rate again + (Monk level / 3) x 0.1, so NWN's
  Monk part *replaced* a Barbarian's; then the caps). Its HasFeat(207) is answered no for
  characters, and the 2024 bonus (+1/3 at 2 ... +1 at 18, no armour, no shield, at most the
  3.0 Monk cap) is added by a new "bonus" stage of the movement hook, before the adjusters
  (the grapple's drag halves it). Barbarian / Monks keep both, as 2024 does.
- *M10:* CalculateDamagePower hooked for whose strike it is; HasFeat(213) is Monk 6+ for
  characters and ruleset KI_STRIKE_LEVEL_1 answers 21 for them - DAMAGE_POWER_PLUS_TWENTY
  (the brief's "20" was the +20 tier: 6 is DAMAGE_POWER_ENERGY, so +20 is 21). Unarmed only
  (GetUseMonkAttackTables(1)), as NWN's Ki Strike.
- *M11 (moved up from part 5):* the Rogue's Evasion filter only read Rogue levels, so a Monk
  had no Evasion at all in faithful (206 removed by R0, 212 answered no); it takes Monk 7 now,
  and rules_features grants 212 at Monk 7 (GrantedFeats2024 now lists each feat once).
- *M15:* rules::ClassFeatureAbilityIncreases - Monk 20: Dexterity and Wisdom +4, at most 25.
- Test aid: `!spawn <blueprint> dr` gives the creature damage reduction 10/+5.
*0.52.1 (owner's MK2 / MK3 log, 2026-10-28):* right after `!test` the Monk was briefly overloaded
(encumbrance 2) and Unarmored Movement lifted its 0.12 rate to 0.62 - past 2024's "Speed at most
5 ft". With `carrying` faithful an overloaded creature (+0x970 = 2) now gets no class speed bonus:
not Unarmored Movement, and not the Barbarian's Fast Movement (its ruleset answer 0 then - the
same hole since B8). The Monk test profile carries a small shield and leather armour.
*Accepted (owner, 2026-10-28: "I tested MK1, 4,5 and 6, they pass"; "MK2b, MK2c and MK3b pass, go with
part 2").* (Equipping the shield and the armour needed `!feat add 32` / `!feat add 3`: NWN refuses gear
without its proficiency feat; 2024 lets anyone wear it with penalties - X7.)

**Built - part 2 (0.53.0; owner, 2026-10-28: "go with part 2").** M1 Martial Arts, as decided
(a), (b), (c)(i) - while a character Monk is unarmed or holds only Monk weapons (a torch may be
in the off hand) and GetUseMonkAbilities holds (no armour, no shield):
- *(a) Dexterous Attacks:* rules_finesse's GetWeaponFinesse answers 1 for its unarmed strikes and
  Monk weapons (installed with class_features faithful too, whatever `finesse` is); its damage
  bonus takes the better of Dex and Str (FinesseDamageBonus); rules_shove's RollUnarmedSave uses
  the better for the Shove / Grapple DC (a grapple's escape DC is the same number).
- *(b) The Martial Arts die:* GetUnarmedDamageDie / Dice hooked - d6 / d8 / d10 / d12 at 1 / 5 /
  11 / 17, one die, any size (NWN's tiers were < 4 / 8 / 12 / 16 up to d20, Small creatures
  their own rows). A Monk weapon rolls the die when its own dice average lower - inside
  rules_fighter's weapon-dice filter, so Great Weapon Fighting (a quarterstaff in two hands)
  and Savage Attacker apply to the die. rules::IsMonkWeapon2024: club, dagger, handaxe, light
  hammer, light mace, quarterstaff / magic staff, short spear, sickle, scimitar, shortsword;
  kama, kukri. The served baseitems.2da (data/rows/baseitems.if_classfeat2024.txt) marks them
  IsMonkWeapon: the column EE's CNWBaseItemArray::Load reads into base item +0xc0 (0x2a0d65;
  without it only the torch and kama are), which InitializeNumberOfAttacks' Flurry and
  GetUseMonkAttackTables read - verified by disassembly, so writing memory wasn't needed.
- *(c) The bonus strike:* after InitializeNumberOfAttacks (rules_two_weapon's hook, now installed
  with class_features faithful), one more on the round's extra attacks (+0x2f60) - none in a
  round with an off-hand attack, none in NWN's Flurry mode (+0x6ef = 5: it adds its own until
  part 3). It uses what is in hand.
*0.53.1 (owner's MA log, 2026-10-28: MA2-MA8 as expected; MA1 the sheet's own number):* a weapon
equipped mid-fight gave that round "extra 2" - 4 attacks. The engine counts a round's attacks again on
an equip, and InitializeNumberOfAttacks only *sets* the extra attacks in Flurry / Rapid Shot (else it adds
Haste's to what is there); EndCombatRound clears them (0x375c43). The bonus strike is now added once per
counted round (rules_mastery's EndCombatRound counter), and taken back when a mid-round recount no longer
allows it (a second weapon: an off-hand attack). Savage Attacker's log line names the die it rolled (a
Monk's d8, not the weapon's d6).
*Accepted (owner, 2026-10-28: "MA1 is good. check the logs"; the 0.53.1 log: a quarterstaff equipped
mid-round "the bonus strike kept once" - extra 1; a second dagger "taken back" - off-hand 1, extra 0).*

**Built - part 3 (0.56.0; owner, 2026-10-29: "ok, it's good, monk part 3").** src/core/rules_monk.cpp, as
decided (M4 (a), M7 (a), M9 (a), M13 automatic):
- *M3 Monk's Focus:* X6's pool (feat_uses.cpp, record `nwn5e_focus_spent`), which now takes a cost per feat
  and a spend filter: a Focus feat's uses are the points left / its cost (Stunning Strike, Patient Defense,
  Step of the Wind 1; Superior Defense 3; Quivering Palm's 4 for part 4), so the radial and the quickbar count
  them. A point per Monk level from 2; all back on a Short Rest (its line says so) and a Long Rest; Focus DC
  8 + Wis + PB. `!focus` (test aids: `!focus left <n>`, `!focus reset`).
- *M4 Flurry of Blows:* feat 204 from Monk 2 (HasFeat no below it; the served table grants it at 2). Each
  round in its mode with Martial Arts spends a point (once per counted round - a recount doesn't pay again):
  two strikes (three from 10) on the round's extra attacks in place of the bonus strike, no off-hand attack;
  out of points, the engine's one (the bonus strike). Leaving the mode mid-round keeps the round's strikes,
  no bonus strike on top. FLURRY_OF_BLOWS_TO_HIT_MODIFIER answered 0.
- *M9 Stunning Strike:* Stunning Fist (39) from Monk 5, named Stunning Strike; a UseFeat guard: Martial Arts
  holds, once a turn. ResolveMeleeSpecialAttack's own DecrementFeatRemainingUses(39) (0x3E6CAC - the attack
  result at +0x78 is already set: the engine tests it right after) spends a point only on a hit (the spend
  filter, told by its caller); the Fortitude save that follows (0x3E6FDD, told by its call site) takes the
  Focus DC unconverted (a Con save in faithful saves). A failure: NWN's stun, 6 s (STUNNING_FIST_EFFECT_DURATION
  6.0 - one round, as 2024); a success: EffectMovementSpeedDecrease(50) for 6 s and a mark - the next attack
  roll on it within the round has Advantage (rules_advantage). STUNNING_FIST_TO_HIT_MODIFIER and
  _DAMAGE_MODIFIER answered 0.
- *M7:* at ResolveInitiative (one a 30 s - the "new fight" guard): Uncanny Metabolism with half the points
  spent or Bloodied, once per Long Rest (record `nwn5e_metabolism_used`) - every point back, Martial Arts die
  + Monk level healed; else Perfect Focus (15): 3 or fewer points left rise to 4.
- *M13 Disciplined Survivor:* every save proficient from Monk 14 (rules::ClassFeatureSaveProficiency); a failed
  save rolled again for a point in the save wrapper - after Indomitable and Indomitable Might, before Stroke of
  Luck - while a point is left.
- *M5:* feats 1317 / 1318 (granted at Monk 2; on the radial through cls_feat_monk's `+` rows) -> spells 1029 /
  1030 -> n5_patient / n5_stepwind, 1 point each. Patient Defense: no opportunity attacks this round
  (rules_opportunity's Tactical Shift test) and Dodge until the next round (attack rolls on it Disadvantage,
  Dexterity saves Advantage, not while Incapacitated); from 10, two Martial Arts dice of temporary hit points
  (600 s). Step of the Wind: no opportunity attacks and EffectMovementSpeedIncrease(100) for the round. The
  free versions: Phase 6.
- *M14 Superior Defense:* feat 1319 (Monk 18) -> spell 1031 -> n5_superior, 3 points: X4's resistance provider
  (every damage type but Magical - NWN's Force) for a minute; Incapacitated ends it (checked as damage comes).
- The quickbar places Flurry of Blows, Patient Defense, Step of the Wind, Stunning Strike and Superior Defense
  when gained (record bits 20-24).
- *Caveats:* the -4 / -2 answers are global (the ruleset answer has no creature: a monster's Stunning Fist and
  Flurry lose their penalties too); a Monk 1-4 who took Stunning Fist as a general feat has it only from 5;
  Uncanny Metabolism and Perfect Focus wait on ResolveInitiative at a fight's start (how often it runs is
  still unverified - the 30 s guard).

*Fixes from the owner's tests (2026-10-30):*
- *FO4 (owner: "FO4 is untestable, monk can't equit longsword"):* NWN won't equip a weapon its wielder isn't
  proficient with. The `monk` test profile carries a light crossbow instead - in NWN's Monk proficiency (feat
  49: crossbows), not a 2024 Monk weapon - so Martial Arts can be seen off (data only).
- *0.57.8 (owner, FO6: Stunning Strike "greyed out with a 0. my focus is full"):* the client is told a feat's
  remaining uses only when the feat list or a feat entry's own uses byte changes (ComputeLastUpdate_GuiFeats
  0x4C98C0 compares both - the entries at stats +0x10, count +0x18, the byte at +2 - and sets bit 4 of the
  player update; WriteGameObjUpdate_PlayerUpdate +0x112C then sends GetFeatRemainingUses). X6's pools live in a
  record, so a spend, a rest or a `!focus` changed nothing the engine compares and the quickbar and the radial
  kept the count last sent. For a feat a provider claims, a player character's byte now mirrors its remaining
  uses, set at its AIUpdate when it differs (feat_uses.cpp) - Focus and Rage alike. Every engine accessor of a
  claimed feat is hooked, and a save writes GetFeatRemainingUses, so the byte is free for this.
- *0.57.10 (owner, FO6: "cannot use stunning strike while armed"):* UseFeat's case for Stunning Fist and
  Quivering Palm (0x3A9590; its jump table sends 39 and 296 there) refuses any item in the right hand - "You
  cannot use this feat while armed" (feedback 0xC6), NWN's unarmed-only rule. When the Stunning Strike guard
  finds Martial Arts holding, it marks the creature's inventory, and that one GetItemInSlot call (the one the
  0xC6 feedback follows, found by that pattern) answers an empty right hand, once. A non-Monk weapon is
  refused by the guard first. Quivering Palm (part 4) will need the same.

*Accepted (owner, 2026-10-30: "28, 57 and 23 accepted").* Monk part 3 (#28), 0.56.0 with 0.57.8 and 0.57.10: every row of
FO1-FO14 seen in the owner's runs (the log evidence on #28).

**Built - parts 4 and 5 (0.60.0; owner, 2026-11-02: "Monk parts 4 and 5"; #29, #30).** src/core/rules_monk.cpp, as
decided (OH1, OH2, OH4 (a), M8, M12 A). Warrior of the Open Hand is the SRD's one Monk subclass, from 3, so its
levels are Monk levels.
- *OH2 Wholeness of Body (6):* NWN's feat 211 moves from Monk 7 to 6 in the served table (cls_feat_monk row 9;
  characters made earlier are granted it - only when missing, so NWN mode keeps NWN's own); spell 309 runs
  n5_wholeness instead of nw_s2_wholeness (spells.if_classfeat2024.txt, instant): the Martial Arts die + Wisdom
  modifier healed, at least 1. Uses = the Wisdom modifier, at least 1, a Long Rest (X6 pool
  `nwn5e_wholeness_spent`; the Short Rest leaves it). An action until Phase 6.
- *OH4 Quivering Palm (17), (a) one step.* Read by disassembly (89.8193.37): ResolveMeleeSpecialAttack (0x3E6660)
  runs the special attack's roll, ResolveDamage and ResolvePostMeleeDamage (0x3E8F00), whose Quivering Palm part
  needs a hit with damage, a target not immune to critical hits and a lower level than the attacker's, then rolls
  Fortitude against QUIVERING_PALM_BASE_DC + level / 2 + Wis (SavingThrowRoll at 0x3E93F5) or death; after that
  its switch spends the use (DecrementFeatRemainingUses(296) at 0x3E76D2) and adds NWN's feedback for an immune
  (0x7E) or higher-level (0x49) target. nwn5e: feat 296 at 17 (row 17), HasFeat no below it; the save wrapper
  answers NWN's death save "saved" with no roll for a faithful Monk character (no instant death); the spend
  filter takes 4 Focus points only on a hit and then rolls the 2024 Constitution save against the Focus DC -
  10d12 magical (NWN's Force), half on a success - a half second later (with the swing). Unarmed only: UseFeat's
  own rule, as 2024's Unarmed Strike. No vibrations to end later, no "one creature at a time" ((b): Phase 6 / 7).
- *OH1 Open Hand Technique (3).* CNWSCombatRound::GetExtraAttack (0x376270): an attack is extra when the round has
  extra attacks (+0x2F60 / +0x2F64) and its index + 1 passes the main hand's count (+0x2F50); GetOffHandAttack
  (0x376400): past the main hand's and the extra ones. So a round runs the main hand, then the extra attacks -
  where M4 put the Flurry's strikes - then the off hand. After an attack roll that hits, in a round whose Flurry
  point was paid, attacks main..main + FlurryStrikes - 1 are the Flurry's (rules::IsFlurryStrike); each imposes
  the standing choice (X8): nwn5e's feat 1320 at 3 whose radial (spells 1032-1037, FeatIDs 16014-16018) picks
  Topple / Push / Addle / off or opens the Combat options window; `!technique`; `!options` (one of three
  checkboxes); record `nwn5e_oh_technique`, Topple until changed. Topple: a Dexterity save against the Focus DC
  or NWN's knockdown for a round; Push: a Strength save (rules_shove's forced Strength) or rules_push's 15 ft as
  the hit lands (the Monk installs rules_push and lands its pushes itself, weapon mastery or not); Addle: no
  opportunity attacks until its next turn (rules_opportunity, as Staggering Blow's).
- *M8 Deflect Attacks (3) / Deflect Energy (13):* NWN's Deflect Arrows (8) moves from 2 to 3 (row 6) and carries
  the 2024 name; its own effect is masked (HasFeat(8) in ResolveDefensiveEffects answers no for characters). After
  ResolveDamage (resolve_damage.h, the Rogue's Uncanny Dodge's way), the first hit each round on a Monk 3+
  character whose damage includes bludgeoning, piercing or slashing (the weapon's base damage counts; any type
  from 13) loses 1d10 + Dex + Monk level, physical damage first (rules::ReduceAttackDamage) - the round's
  reaction (+0x2F40), shared with Uncanny Dodge and opportunity attacks; not while Incapacitated. Automatic; the
  redirect at 0 damage (1 point): Phase 6 / 7.
- *M12 Self-Restoration (10):* a creature update listener: a round (6 s of game time) after Charmed, Frightened
  or Poisoned is seen on a Monk 10+ character, one goes - Charmed, else Frightened, else Poisoned
  (rules::SelfRestorationPick) - and one more each round after. nwn5e's 2024 Charmed (a player) and Frightened are
  icons (rules_conditions.cpp); NWN's charm and domination (a henchman), NWN's poison effect and the poison icon
  (Cunning Strike's Poisoned) count too; removed by a script (RemoveEffect takes linked effects). Going without
  food has no NWN use.
- The quickbar places Wholeness of Body and Quivering Palm when gained (record bits 25, 26 - six bits left);
  Open Hand Technique's pick stays on its radial, `!technique` and `!options`. The Focus tooltip lists Quivering
  Palm's 4; `!features` describes all six.
- Tests MonkOpenHand, MonkDeflectAndRestore and QuickbarMonkActions' new rows (31 of 31 mutations caught).
- *Caveats:* NWN's own feedback line for a Quivering Palm on a crit-immune or higher-level target still shows
  (nwn5e's save and damage follow anyway); Deflect Attacks takes the round's first qualifying hit even when a
  bigger one comes later; Topple's knockdown is NWN's (its own immunities); Self-Restoration acts a round after a
  condition is seen, not at the exact end of the Monk's turn.
*The owner's 0.60.5 run (2026-11-02):* "OH2-2 shows "Trap Kit" instead of "Wholeness of Body" in the action
queue" - NWN's spells.2da row 309 carries an item's name strref, which the strip shows (the radial names the
feat); "M8-1 isn't triggering" - the log: Deflect Attacks took hits to 0, then "reaction spent this round" four
rounds later: the engine refills the round's reaction only when the creature's own round starts, and a Monk
standing still while shot at has none running. *Built 0.60.7:* data/rows/spells.names.txt serves row 309 the
feat's name (1945), both modes; reactions.h's TakeReaction - a reaction a rule took is back after 6 s when none of
the creature's rounds ended since (rules::ReactionBackWithoutRounds; test ReactionBackWhenNotFighting, 4 mutations
caught) - for Deflect Attacks and the Rogue's Uncanny Dodge (the same gap). The owner: "M12-1,2 and 3 are
confirmed"; "I didn't see the deflection trigger probably because there was no animation to show it".
*Decided (owner, 2026-11-02): "go with A for the deflection, the thing is that I'm pretty sure I've seen deflect
projectile animation. but yeah, I'll add C later."* (A: a visual effect on the Monk; B, NWN's parry - the hit then a
miss to the engine - not taken; C: a custom deflect animation, with the animation session, later.) *Built 0.60.8:*
NWN's parry sparks (VFX_COM_SPARKS_PARRY) on the Monk with each deflection.
*Accepted (owner, 2026-11-02: "M12-1,2 and 3 are confirmed"; "everything passes").* Monk parts 4 and 5 (#29, #30),
0.60.0 with 0.60.7 and 0.60.8: OH1, OH2, OH4 and M8, M12 - every row of the matrices (the 10:29 log: Wholeness of
Body, Quivering Palm on a hit and a miss, Topple, Push, Addle, Deflect Attacks; the 13:27 log on 0.60.10: Deflect
Attacks and Wholeness of Body again). Left for later: a custom deflect animation (C, with the animation session),
Deflect's redirect and Quivering Palm's two steps (Phase 6 / 7).
*Monk follow-ups (#31; owner, 2026-11-02: "go with your recommendations"):* 1 B - Stunning Strike's -4 and
Flurry's -2 none for characters, NWN's for monsters (NWN's Monk); 2 A - a Monk 1-4's general-feat Stunning Fist
(NWN mode's only) waits for Monk 5, kept; 3 B - Uncanny Metabolism and Perfect Focus once a fight, a new one only
after the Monk was out of combat (the owner's 10:29 log: Uncanny Metabolism at 10:40:42, a minute into a fight -
the 30 s guard); 4 A - henchman AI with Flurry / Stunning Strike checked in the playthroughs (#72); 5 A - +100%
speed at Monk 18 kept; the radial / quickbar refresh of pool uses was 0.57.8's. *Built 0.60.11:* the three ruleset
answers ask who attacks - the roll being resolved (CurrentAttackRoll: GetAttackModifierVersus and
ResolveSpecialAttackAttackBonus read them inside ResolveAttackRoll; the damage one inside ResolveDamage) -
rules::MonkAttackPenalty: a character 0, a monster NWN's, none known (the sheet) 0; MonkOnInitiative keeps the
fight until a creature update finds the Monk out of combat (NWN's in-combat state, +0x700) at least 10 s after it
began (rules::MonkFightOver). Test MonkFollowUps (6 mutations caught).
*Accepted (owner, 2026-11-02: "F1, F3-1 and 2 and 110 all pass").* The Monk follow-ups (#31), 0.60.11 - with parts
1-5 accepted, the Monk's conversion (#27) is done; left for later: Phase 6 / 7 (Fleet Step, the free Patient
Defense / Step of the Wind, the bonus strike as a Bonus Action, Deflect's redirect, Quivering Palm's two steps),
the proficiencies (#17), the Epic Boon (#16), a custom deflect animation (with the animation session).

### Phase 3 — Rogue (CM9): recommendations (for the owner's decision)

**Decided (owner, 2026-10-28: "go with the recommendations").** Every R and T
below as recommended, with two updates made since the brief: **R6** Cunning
Strike is picked like Brutal Strike - its six effects and Off are more than a
sub-radial's five slots, so it brings the "Combat options" NUI window (X8's
second half) plus a quickbar button per effect, `!cunning` as a fallback;
**R7** Uncanny Dodge spends the round's reaction - the round's AoO count found
for Retaliation (CNWSCombatRound +0x2f40, 0.47.1) - so (b). **R0** follows X1
(owner, 2026-10-24: "remove them completely"): the NWN-only rogue feats are
taken off characters in faithful, as the Barbarian's were, and the served
feat table no longer grants them; Improved Evasion stays as 2024 Evasion (R8),
the Sneak Attack feats stay (the AI's flanking talent asks for them; R1
decides). Build order as proposed.


SRD 5.2 Rogue, one subclass: **Thief** (Arcane
Trickster, Assassin and Soulknife are PHB-only — private layer). Every feature
below was checked in the SRD 5.2.1 PDF (pp. 61–64) on 2026-10-24; none is
PHB-only (Cunning Strike's Poisoner's Kit is an SRD tool). The core traits are
built: d8 (CM3), Dexterity and Intelligence saves (C2), 4 skills, Thieves'
Tools and Expertise (SK2), Light armour (NWN's too); weapons are R3.

NWN's Rogue (EE's cls_feat_rog): Sneak Attack 1d6 at 1, +1d6 at every odd
level (10d6 at 19; feats 221, 345–353); Evasion at 2; Uncanny Dodge 1 at 3
(keeps Dexterity to AC when flat-footed), 2 at 6 (can't be flanked by a rogue
less than 4 levels higher; +1 Reflex against traps), 3–6 at 11 / 14 / 17 / 20
(+2 … +5 against traps); bonus picks at 10 / 13 / 16 / 19 among Crippling
Strike, Defensive Roll, Opportunist, Skill Mastery, Improved Evasion and
Slippery Mind; Epic Rogue at 21. **Faithful today:** the built rules replace
the hit die, saves, skills (Expertise at 1 and 6), the feat levels (ASI at 4 /
8 / 10 / 12 / 16 instead of NWN's picks, which the screens no longer offer —
CM7, CM8), Weapon Mastery (2), the Rogue-only traps (SK3) and Uncanny Dodge's
trap bonuses (C7 drops them: they come before the save's d20). Everything else
is still NWN's: sneak attack by NWN's rules on every hit, Evasion at 2,
Uncanny Dodge 1–2, and any NWN rogue feat a character took before. Status per
level, then the decisions (R0–R12, T1–T5).

| Lvl | 2024 feature | Status |
|---|---|---|
| 1 | Expertise (2) | built (SK2: `!skills`, the Skills pages) |
| 1 | Sneak Attack (1d6 … 10d6 at 19) | R0, R1 (today NWN's) |
| 1 | Thieves' Cant | R2 |
| 1 | Weapon Mastery (2) | built (D3, C4) |
| 2 | Cunning Action | R4 (Phase 6) |
| 3 | Thief | automatic (CM10) |
| 3 | Steady Aim | R5 (Phase 6) |
| 4, 8, 10, 12, 16 | Ability Score Improvement | built (CM7) |
| 5 | Cunning Strike | R6 |
| 5 | Uncanny Dodge | R7 |
| 6 | Expertise (2 more) | built (SK2, level-up Skills page) |
| 7 | Evasion | R8 |
| 7 | Reliable Talent | R9 |
| 11 | Improved Cunning Strike | R6 |
| 14 | Devious Strikes | R6 |
| 15 | Slippery Mind | R10 |
| 18 | Elusive | R11 |
| 19 | Epic Boon | with CM8 |
| 20 | Stroke of Luck | R12 |
| Thief 3 | Fast Hands | T1 |
| Thief 3 | Second-Story Work | T2 |
| Thief 9 | Supreme Sneak | T3 |
| Thief 13 | Use Magic Device | T4 |
| Thief 17 | Thief's Reflexes | T5 (Phase 6) |

**R0. NWN's Rogue feats in faithful (the feature mask).** The Rogue is the
first class with NWN features to take away (the Fighter had none, so CM9's
mask waited for it). EE needs no new RE for the mechanism: feats.cpp already
filters CNWSCreatureStats::HasFeat. Recommendation: one faithful filter that
answers for these feats at **every** caller — the engine's rules, the scripts'
GetHasFeat (the stock dragon breaths, fire traps, nw_i0_spells … test Evasion
themselves), henchmen's automatic level-ups — so each NWN rule falls silent
and the 2024 feature decides in its own hook:

| NWN feat | what EE does with it | faithful |
|---|---|---|
| Sneak Attack 1–10 (221, 345–353) | ResolveSneakAttack, GetDamageRoll's dice | kept (the AI's flanking talent asks for it), decides nothing: R1 |
| Evasion (206, at 2) | the GetReflexAdjustedDamage command; scripts | absent |
| Improved Evasion (212, a pick) | the same | means 2024 Evasion: R8 |
| Uncanny Dodge 1 (195) | Dexterity to AC when flat-footed | absent (flat-footed goes: R1 e) |
| Uncanny Dodge 2–6 (251–255) | no flanking for sneak / death attacks; +1 … +5 vs traps | absent (2024's is R7) |
| Crippling Strike (222) | 2 Strength damage on a sneak hit | absent |
| Defensive Roll (223) | Reflex save halves a lethal hit, once a day | absent |
| Opportunist (224) | +4 on opportunity attacks | absent |
| Skill Mastery (225) | take 20 on traps and locks, in combat too | absent (Reliable Talent is R9) |
| Slippery Mind (259) | a failed Will save rolled again (after the d20, so C7 misses it) | absent (2024's is R10) |
| Epic Improved Sneak Attack (834–843), Epic Rogue (974) | more sneak dice; level 21 | absent (cap 20) |

The feats stay on the sheet; in faithful the label hook names them for what
they now are (Improved Evasion "Evasion", Uncanny Dodge "Uncanny Dodge", the
rest "NWN feature — no effect in 2024 rules"); `!features` lists the 2024
truth. NWN mode: all NWN's.

**R1. Sneak Attack.** 2024: once per turn, extra damage (1d6 at 1, +1d6 at
every odd level, 10d6 at 19, the weapon's damage type) to one creature hit by
an attack with a Finesse or Ranged weapon that has Advantage — or, without
Advantage, when an ally of the attacker that isn't Incapacitated is within 5
ft of the target and the attack doesn't have Disadvantage. A critical hit
rolls its dice twice (2024 crits double every damage die of the attack). No
range limit, no immunity by creature type. NWN (decoded: Engine facts):
ResolveSneakAttack decides before the hit, for every attack, with any weapon:
the target can't see the attacker, or it is flat-footed, or it is "flanked"
(busy with someone else — no distance, no ally test); ranged only within 10 m;
Uncanny Dodge 2 blocks flanking; no sneak attack on a sneak- or crit-immune
target. It sets a flag in the attack's data that ResolveDamage hands to
GetDamageRoll, which rolls the dice once — their count from the feats —
outside its critical loop: never doubled. So NWN's dice match 2024 level for
level; trigger, frequency, weapons, range, immunity and crits don't. Route:
hook ResolveSneakAttack (exported, one caller: inside ResolveAttackRoll, after
rules_advantage's before-listener has the roll mode); in faithful it writes
the flag by the 2024 test. The dice: a filter at GetDamageRoll's sneak
RollDice (the 10th of its 12, +0x12df, found by shape at load) inside the
GetDamageRoll hook rules_fighter already has; "used this turn" is marked when
that roll happens (only on a hit) and read by the next ResolveSneakAttack.
Sub-decisions:
- (a) The test: Advantage = rules_advantage's final roll mode (an unseen
  attacker — hiding included —, blinded / stunned / restrained / paralysed /
  asleep targets, prone in melee, Vex, Studied Attacks: the Rogue's Vex
  weapons — rapier, shortsword, shortbow — set up the next Sneak Attack); the
  ally = a creature friendly to the attacker (party, henchmen, summons) within
  5 ft edge to edge of the target, not dead, dying or Incapacitated
  (IncapacitatedByCondition), the mode not disadvantage; the weapon a Finesse
  one (rules::IsFinesseNwnBaseItem) or a ranged attack (bows, crossbows,
  slings, darts, shuriken, throwing axes) — not unarmed strikes or creature
  weapons; no 10 m limit. Recommendation: as written.
- (b) Once per turn: an NWN round is a 2024 turn (D4). Recommendation: once
  per the attacker's combat round (an opportunity attack on another creature's
  turn could add one in 2024: open question).
- (c) Dice: ⌈Rogue level / 2⌉ d6, Rogue levels as ClassLevel2024 (Assassin and
  Shadowdancer count, decision 10), twice as many on a critical hit; the
  engine's epic and Blackguard dice ignored. Creatures with sneak feats and no
  Rogue levels (NWN blueprints) keep their NWN dice under the 2024 test until
  Phase 8. Recommendation: as written.
- (d) Immunity: NWN's sneak-immune creatures (immunity type 30; every
  crit-immune one, type 31, too — undead, constructs, oozes …). Options: (a)
  keep it, with crit immunity, until Phase 8 converts creatures; (b) faithful
  drops it (2024 has none). Recommendation: (b) — Sneak Attack is the Rogue's
  damage, and much of the campaigns' fighting is undead and constructs; crit
  immunity is the same question (kept today by E0's design).
- (e) Flat-footed: the conditions decision left NWN's flat-footed Dexterity
  loss (stunned, paralysed, asleep, petrified, blind) "until Sneak Attack is
  converted". With (a) helpless targets give advantage instead.
  Recommendation: faithful hooks GetFlatFooted to "never" (no Dexterity loss
  to AC; its other users — Defensive Roll, Death Attack,
  ResolveDefensiveEffects — are masked or minor); NWN mode NWN's.
The combat log's "Sneak Attack" follows the flag (the engine's own text).

**R2. Thieves' Cant.** A language; NWN has no language rules. Recommendation:
listed, no effect.

**R3. Weapon proficiencies.** 2024: Simple weapons, and Martial weapons with
Finesse or Light (rapier, scimitar, shortsword, whip). NWN's rogue proficiency
(feat 50): club, dagger, dart, light and heavy crossbow, light mace,
morningstar, quarterstaff, sling, shortbow, short sword, rapier, handaxe — it
lacks spear, sickle, light hammer, scimitar, whip, kama and kukri, and has the
heavy crossbow (Martial, neither Finesse nor Light). Every class has the same
question (CM2's later-class lists wait for it too). Recommendation: one
proficiency pass for all classes (CheckProficiencies is exported; the mastery
picks follow proficiency), not in this brief.

**R4. Cunning Action (2).** Bonus action: Dash, Disengage or Hide. NWN:
stealth mode is a free toggle but refused in combat (1.69 test, SK5); Tumble
stands in for Disengage (D2); no Dash. Options: (a) Phase 6; (b) the Hide part
now — a Rogue 2+ may enter stealth mode in combat (the DC 15 Stealth and the
perception rules still decide whether it hides). Recommendation: (a), with (b)
if the in-combat refusal turns out to be one server-side check (not found in
SetStealthMode / SetActivity: RE).

**R5. Steady Aim (3).** Bonus action, only if you haven't moved this turn:
Advantage on your next attack this turn, then Speed 0 until the turn ends.
Options: (a) Phase 6; (b) a toggle now (`!steadyaim`): each round in which the
Rogue hasn't moved since the round began, its first attack has advantage and
it can't move until the round ends. Recommendation: (a) — a henchman beside
the target already gives most Sneak Attacks (R1 a), hidden attacks have
advantage, and (b) would be redone with bonus actions.

**R6. Cunning Strike (5), Improved Cunning Strike (11), Devious Strikes
(14).** When dealing Sneak Attack damage, give up dice (removed before
rolling) for an effect after the damage; the save DC is 8 + PB + Dexterity
modifier. Poison (1d6, needs a Poisoner's Kit): Constitution save or Poisoned
1 minute, repeating the save each turn; Trip (1d6): Large or smaller,
Dexterity save or Prone; Withdraw (1d6): move half Speed without opportunity
attacks; from 11 two effects per Sneak Attack; from 14 Daze (2d6):
Constitution save or next turn only a move, an action or a bonus action; Knock
Out (6d6): Constitution save or Unconscious for 1 minute or until damaged,
repeating the save each turn; Obscure (3d6): Dexterity save or Blinded until
the end of its next turn. A per-attack choice the engine can't ask for.
Recommendation: build after R1 as a standing choice — `!cunning <effect>
[<effect>]`, `!cunning off` (NUI with Phase 7) — applied to every Sneak Attack
with dice enough; the target's save through the save wrapper's forced ability;
henchmen none (full dice). In NWN terms: Trip = the knockdown Topple uses
(D4); Poison = nwn5e's own Poisoned (CN4's disadvantage, an icon, no ability
damage), a save each round; the kit waived (NWN has no Poisoner's Kit item —
Phase 8 may serve one); Withdraw = no movement opportunity attacks for the
rest of the round (Tactical Shift's local, F7); Obscure = blindness for a
round; Knock Out = sleep (Unconscious, CN2: advantage, crits from 5 ft) ended
by any damage (the DoDamage hook) or a Constitution save each round, 1 minute
at most; Daze = one round of NWN's Slow (NWN's Dazed is 2024's Incapacitated:
too strong).

**R7. Uncanny Dodge (5).** Reaction: when an attacker you can see hits you
with an attack roll, halve that attack's damage (rounded down). NWN's Uncanny
Dodge is unrelated (R0). Options: (a) automatic: the first hit each round from
an attacker the Rogue sees (engine::Sees), unless it is Incapacitated; (b) as
(a), spending the round's opportunity attack — NWN's one per round, refilled
each round, as 2024's one reaction — and none when it already made one. Route:
a post-hook on CNWSCreature::ResolveDamage halves the attack's damage entries
(int16 per damage type in the attack data) before
ResolvePost{Melee,Ranged}Damage applies them — before resistances, as 2024
orders it. Recommendation: (b) if the round's AoO count is found (RE), else
(a); Phase 6 makes it a real reaction.

**R8. Evasion (7).** 2024's Evasion is NWN's Improved Evasion — no damage on a
successful save, half on a failed one — for Dexterity saves only, and not
while Incapacitated. Route: R0's filter: Improved Evasion (212) answers "Rogue
7+ (Monk 7+ with the Monk brief), not Incapacitated, and the save rolls
Dexterity" — 0 when C1's table moved the spell's save (Cone of Cold, Sunbeam,
Sunburst roll Constitution: the 1.69 follow-up) — and Evasion (206) answers 0,
for the engine's command and the scripts alike. An item's Improved Evasion
property stays (Phase 8). Recommendation: build now.

**R9. Reliable Talent (7).** An ability check with a skill or tool proficiency
treats a d20 of 9 or lower as 10. Route: the dice filter at the d20s nwn5e
already owns — the scripts' GetIsSkillSuccessful (SK11), the Stealth roll when
hiding (SK5: the DC 15 Hide), Search and a searching observer's Perception;
not the engine's own lock / trap / heal-kit d20s (CRT rand: SK11's gap).
Recommendation: build now at those sites; locks and traps with that gap.

**R10. Slippery Mind (15).** Proficiency in Wisdom and Charisma saves (NWN's
reroll: R0). Route: rules::HasSaveProficiency gains class-feature
proficiencies (Rogue 15; Monk 14 with its brief), read by the three save
getters and C1's substituted abilities (Charisma saves: Banishment, Bane …).
Recommendation: build now.

**R11. Elusive (18).** No attack roll against you has Advantage unless you're
Incapacitated. Route: rules_advantage (weapon attacks) and SK10's spell-attack
roll ignore advantage sources against such a target; disadvantage stays.
Recommendation: build now.

**R12. Stroke of Luck (20).** Turn a failed D20 Test into a 20; once per Short
or Long Rest. Options: (a) automatic: the first failed saving throw (like
Indomitable, after it); (b) the first failed save or attack roll; (c) a prompt
with Phase 7. Recommendation: (a), restored by the rest listener (the Short
Rest's too, 5a).

**T1. Fast Hands (3).** Bonus action: a Dexterity (Sleight of Hand) check to
pick a lock, disarm a trap with Thieves' Tools or pick a pocket, or Utilize,
or a magic item's Magic action. Recommendation: the speed with Phase 6 (in
2024 a potion is already a bonus action); now the Thief's lock and trap checks
use the better of Thieves' Tools and Sleight of Hand (so Expertise in Sleight
of Hand counts) — a skill adjuster while the six lock / trap actions run
(rules_skill_dcs knows them).

**T2. Second-Story Work (3).** Climb Speed; jump distance by Dexterity. NWN
has no climbing or jumping. Recommendation: listed, no effect.

**T3. Supreme Sneak (9).** The Stealth Attack Cunning Strike (1d6): an attack
doesn't end the Hide action's Invisible condition if you end the turn behind
three-quarters or total cover. nwn5e doesn't end hiding on an attack (2024's
Hide does), and NWN has no cover data. Recommendation: later, with an
"attacking ends hiding" rule (a perception decision of its own); until then it
has nothing to keep.

**T4. Use Magic Device (13).** Attune up to four items (NWN has no
attunement); a property that spends charges spends none on a 6 on 1d6; any
Spell Scroll, with Intelligence — cantrips and level 1 surely, higher an
Intelligence (Arcana) check against 10 + the spell's level or the scroll
crumbles. NWN: class limits on scrolls (279 of 287 stock scrolls) and in
faithful no skill lifts them (SK8, which waited for this). Route: the Use
Magic Device skill answer inside CheckUseMagicDeviceSkill (rules_nwn_skills'
adjuster) lifts a scroll's class limit for a Thief 13 after nwn5e's Arcana
roll for level 2+ (Arcana = Lore and Spellcraft, SK1; the scroll destroyed on
a failure); charges by a pre-hook on CNWSItem::SetNumCharges when an item use
spends them. Recommendation: scrolls now, charges once their path is read
(RE).

**T5. Thief's Reflexes (17).** Two turns in the first round of a combat (the
second at Initiative − 10). Recommendation: Phase 6, with Action Surge (one
machinery: an extra action).

**NWN mode:** NWN's Rogue (sneak attack by flanking, flat-footed or unseen on
every hit, NWN's dice and immunities, Evasion, Uncanny Dodge, the level-10+
picks; Assassin and Shadowdancer).

**Build order (proposed).** 1. R0 + R1 (the mask filter; ResolveSneakAttack,
the sneak dice, once per round, crits, immunity, GetFlatFooted) — the class's
identity. 2. R8, R10, R11 (a filter, the save getters, the advantage rule). 3.
R7 (ResolveDamage post-hook). 4. R9, R12. 5. R6 (needs R1; effects through the
script bridge). 6. T4, T1. Later: R4, R5, T5 (Phase 6), T3 (an
attacking-ends-hiding rule), R3 (the proficiency pass for all classes); R2 and
T2 have nothing to build.

*Built 0.50.0 (part 1: R0, R1; rules_rogue.cpp):* R0 - X1's list gains Evasion
(206), Crippling Strike, Defensive Roll, Opportunist, Skill Mastery (222-225),
Slippery Mind (259) and Epic Improved Sneak Attack 1-10 (834-843), taken off
characters in faithful with a chat line naming them (rules_features.cpp,
NwnOnlyClassFeatName); data/rows/cls_feat_rog.if_classfeat2024.txt stops
granting Evasion (row 3) and Uncanny Dodge 1-6 (rows 4-9). R1 - CNWSCreature::
ResolveSneakAttack (0x3ebe00) is hooked: in faithful it sets the attack's
sneak flag (+0x60) by the 2024 test - a Finesse (IsFinesseNwnBaseItem) or
ranged attack; the attack's roll mode Advantage (rules_advantage's pending
mode, CurrentAttackRollMode) or, not Disadvantage, a friendly creature
(GetAIStateReaction 1 - engine::IsFriend) within 5 ft of the target, not dead,
dying or Incapacitated; not twice in the attacker's round (marked when the
dice roll, CombatRoundsEnded); no immunity, no 10 m limit (the engine's
special attacks 0x363 / 0x364 still never sneak). The dice: a filter on
GetDamageRoll's 10th RollDice (+0x12df, found as the 10th of its 12 at load)
rolls ceil(Rogue level / 2) d6 (ClassLevel2024: Assassin and Shadowdancer
count), doubled on a critical hit - the roll's context from rules_fighter's
GetDamageRoll hook (CurrentDamageRoll); a creature with sneak feats and no
Rogue levels keeps the engine's count, doubled on a crit. R1 (e) -
GetFlatFooted answers 0 in faithful. `!features` lists the Rogue (28 rows).
Tests: RogueSneakAttack2024, NwnOnlyClassFeatsAreTheOnesWithoutCounterparts.

*Built 0.50.0 (part 2: R8, R10, R11):* R8 - nwn5e grants Improved Evasion (212)
at Rogue 7 (GrantedFeats2024; GrantedOnlyWhenMissing: an old NWN pick stays the
character's), served as "Evasion" in faithful (data/rows/feat.if_classfeat2024.txt,
strings 16000489/490). A HasFeat filter answers 212 for characters, for the
game's own calls only (nwmain.exe's code - nwn5e's sync must see the real list
to grant it): Rogue 7+, not Incapacitated, and, when a save was rolled in the
last 200 ms, only if it rolled Dexterity (rules_save_roll's RecentSaveAbility:
C1's spell substitution and forced abilities included). The engine's Reflex-
damage command (ExecuteCommandGetReflexAdjustedDamage 0x52f790) asks for 212 /
206 before its save (the feedback's feat) and right after it: none on a
success, half on a failure. Monsters keep NWN's. R10 - rules::
ClassFeatureSaveProficiency (Rogue 15: Wisdom and Charisma), read by both save
getters in rules_saves.cpp with class_features faithful. R11 - Elusive (Rogue
18, not Incapacitated): SituationalRollMode gained `no_advantage` (every
Advantage source dropped, Disadvantage kept), passed for weapon and spell
attack rolls (rules_advantage.cpp; the log says "Elusive: no Advantage").
Tests: RogueSneakAttack2024 (part 2's rows), ElusiveTargetsGiveNoAdvantage.

*Built 0.50.0 (parts 3-4: R7, R9, R12):* R7 - CNWSCreature::ResolveDamage is now
one shared hook (resolve_damage.cpp: skips - the Shove's no-damage moved there -
and after-listeners). After it fills the attack data's damage (int16 per type at
+0x16, 32 of them; GetDamage / SetDamage read them through
GetDamageIndexFromFlags), a hit or critical on a Rogue 5+ character that sees the
attacker and isn't dead, dying or Incapacitated is halved (each entry rounded
down: rules::HalveAttackDamage) before ResolvePost*Damage builds the damage
effect - so before resistances - spending its reaction: the round's AoO count
(CNWSCombatRound +0x2f40, now ee::kRoundOpportunitiesLeft) decremented; none
left, no Uncanny Dodge (and after it no Retaliation or AoO that round). R9 -
rules_skills' ReliableTalentRoll (Rogue 7+, proficient in the skill's 2024
counterpart): the scripts' GetIsSkillSuccessful d20 (rules_skill_dcs - now also
without `conditions` faithful), trap searching's d20 (SearchDie) and a searching
observer's Perception d20 (StealthDice) - with hiding's Stealth (0.35.0) the d20s
nwn5e rolls; the engine's own lock / trap / heal-kit d20s stay (SK11's gap). R12 -
rules_save_roll: a failed save of a Rogue 20 character whose Stroke of Luck is
ready (record nwn5e_stroke_used) is rolled again with SaveD20 answering 20 (the
engine's own total against the DC, its feedback line); used, it waits for a Short
or Long Rest (rest listeners). Tests: RogueSneakAttack2024's part 3-4 rows.

*Built 0.50.0 (part 5: R6 + X8's window):* Cunning Strike is two radial feats
nwn5e grants - 1315 at Rogue 5 (sub-radial: Poison, Trip, Withdraw, Off,
Combat options) and 1316 Devious Strikes at 14 (Daze, Knock Out, Obscure,
Combat options); spells.rogue.txt rows 1018-1028 (FeatIDs 16005-16013 * 65536 +
the feat), each option a quickbar button too (record bits 12-19). The choice is
a record (nwn5e_cunning: never chosen = off - henchmen keep full dice), shown by
an icon (effecticons 224); one effect, two from 11 (a third drops the older).
rules_rogue's sneak dice filter (inside ResolveDamage, which now tells who it
resolves - resolve_damage's ResolvingDamage) asks rules_cunning_strike for the
cost: the chosen effects in order while their dice fit, Trip skipped (free)
against a Huge or larger target; the rest is rolled, doubled on a crit (the
cost comes off before the doubling). When that hit's damage lands
(OnApplyDamage listener, 5 s at most; nothing if it dropped the target) the
target saves against 8 + PB + Dex (SavingThrowRoll, nwn5e's DC unconverted):
Poison = the game's poison icon for a minute (rules_conditions now reads that
icon as Poisoned: no ability damage), a poison-type Constitution save each 6 s
ends it; Trip = knockdown for a round; Withdraw = the Rogue's moves provoke no
AoO for the round (rules_opportunity, beside Tactical Shift); Daze = Slow for a
round; Knock Out = EffectSleep for a minute (tagged), ended by any damage (at
the creature's next update) or a Constitution save each 6 s - NWN's sleep
immunity still blocks it (the Rogue is told); Obscure = Blindness for a round.
The **Combat options** window (combat_options.cpp: `!options`, or the menus'
last entry) lists every standing choice the character has - Reckless Attack,
Brutal Strike's effects, Cunning Strike's - as checkboxes with what each does;
a click is the same pick as the radial / quickbar / `!brutal` / `!cunning`
(their chat line), and the window follows picks made anywhere (watch events:
only a change the player made picks). Tests: RogueCunningStrike,
QuickbarCunningStrikeOptions, the grant rows.

*Built 0.50.0 (part 6: T1, T4):* rules_thief.cpp. T1 Fast Hands (Thief 3) - a
skill adjuster after the 2024 skills': Open Lock and Disable Trap (Thieves'
Tools) answer the better of their value and Sleight of Hand's (Pick Pocket),
wherever they are asked (the lock / trap actions, the character sheet). T4 Use
Magic Device (Thief 13), scrolls - CNWSCreature::CheckUseMagicDeviceSkill (the
engine's "does UMD overcome this item's class / race / alignment limit", SK8's
"never") answers yes for a spell scroll (baseitems 75); CNWSCreature::UseItem is
now one shared hook (item_use.cpp: guards and listeners - the potion resume
moved onto it), whose guard reads the scroll by script (a class limit none of
the Thief's classes meets? the spell's spells.2da Innate level) and for level 2+
rolls Arcana: d20 (Reliable Talent) + the 2024 Arcana value (Spellcraft,
skill_rank's new SkillRankNow) against 10 + the level - a failure crumbles one
scroll of the stack and refuses the use. The scroll keeps its own caster level
and DC (2024: the Thief's Intelligence); attunement and charges later. Tests:
RogueThief. data/test.txt: a human Thief 17 with a rapier, a shortbow and
wizard / cleric scrolls of levels 1-3.

*Accepted (owner, 2026-10-28: "I assume CS4b and CS6 work, CS1 is good and EL1 and SL1 are
confirmed"), 0.50.0-0.50.5.* Confirmed in the owner's logs: Sneak Attack with an ally next to the
target (melee and ranged), with Advantage (a prone or knocked-out target) and none without either;
crits double the dice left after Cunning Strike's; Uncanny Dodge (the first seen hit a round
halved, then "reaction spent"); Slippery Mind (!saves); Evasion (!reflex: 0 on a made save, half on
a failed one); Elusive (!prone at Rogue 20: "Elusive: no Advantage"); Stroke of Luck (a failed save
rolled as a 20, then spent); Cunning Strike's Trip, Poison, Withdraw (also after a kill, 0.50.3),
Knock Out (woken by damage), Obscure, Trip skipped on a Huge target, two effects at 11+ with the
older dropped, Off and !cunning; the Combat options window; Use Magic Device (a level 1 scroll read,
a Fireball crumbling on Arcana 3 and read on 16); Fast Hands (Open Lock +10 -> Sleight of Hand +16);
the potion resume on the shared item-use hook. Taken on the owner's word: Poison's repeat save ending
it, Daze's Slow, the radial menus and quickbar buttons. Stroke of Luck back after a Short Rest:
confirmed by the owner after ("just did the short rest and stroke of luck triggered"). Henchman fixes found while testing: 0.50.1-0.50.4 (Barbarian B3
notes).

## Phase 5 — rests, healing, death: decided by the owner (2026-10-23)

Recon (EE 89.8193.37-17, disassembly; facts the decisions rest on):
- **Rest**: every rest but ForceRest is CNWSCreature::Rest (0x39F360,
  `Rest(bAssociate, bEnemyLOS)`): refused when dead or dying, in a no-rest
  area, with an enemy within ruleset REST_ENEMY_CHECK_DISTANCE (30 m, seen or
  not) or exerted (+0x574: damage or a spell cast in the last 10 s); then
  AIActionRest (0x3CA890) over restduration.2da's 10 s + 0.5 s per level of
  real time, cancelled by exertion, a seen enemy within 30 m or any new
  order (CancelRest 0x386940). HP and spell levels come back gradually during
  it (a cancelled rest keeps part); at the end: RemoveBadEffects (every
  temporary effect), HP to max, spells, item properties,
  ResetFeatRemainingUses (nwn5e's rest listener). The clock doesn't move, and
  nothing limits how often one rests. Module OnPlayerRest: STARTED, FINISHED,
  CANCELLED.
- **Dying**: the official campaigns' OnPlayerDying (nw_o0_dying, all 13
  modules) applies EffectDeath at once - a PC dies at 0 HP today. The engine
  has a PC dying state (GetIsPCDying 0x495620: -10 < HP < 1; lying down, not
  commandable); CNWSCreature::DoDamage (0x38A930) applies the damage,
  OnApplyDamage signals OnPlayerDying on entering it, OnApplyDeath (0x42AA40)
  signals OnPlayerDeath (the respawn panel). Healing a dying PC above 0
  revives it (OnApplyHeal -> resurrection). Henchmen die at 0; each
  campaign's OnDeath decides: OC raised at the temple (nw_ch_ac7), SoU lying
  at 1 HP until healed (x0_ch_hen_death), HotU a corpse to raise
  (x2_hen_death).
- **Clock**: scheduled events (DelayCommand, heartbeats) are keyed to the
  world clock that SetTime advances (CWorldTimer at server +0x10098); after a
  jump every event due runs in one update, oldest first - a burst, in order,
  not lost. 87 of 19,064 official scripts read the clock: ~55 ambient
  (lights, NPC routines), ~25 conversation lines, ~6 gameplay (a night-only
  encounter in Chapter 2, HotU's hour-gated doors and once-a-day throne
  search, HotU's rest system's wandering-monster odds); SoU sets it itself.
- **SRD 5.2** (D&D Beyond free rules, checked 2026-10-23): Long Rest (8 h)
  regains all HP and **all** Hit Point Dice, removes 1 Exhaustion, 16 h before
  another; Short Rest (1 h): spend Hit Dice (die + Con, minimum 1); death
  saves as below; massive damage; Stable regains 1 HP after 1d4 h; monsters
  die at 0.

Decisions (faithful; NWN mode keeps NWN's rest and the module's dying):
- **R1 Long Rest** = NWN's Rest as it is (no enemies within 30 m, no limit),
  and it restores every Hit Die. **R3** an interrupted one gives nothing (no
  partial HP or spells).
- **R1/R4 Short Rest**: a **dedicated button in the HUD next to Rest**
  (owner). Instant, anywhere out of combat - no enemy that can see you within
  30 m, no damage taken or spell cast in the last 10 s; it **spends at least 1
  Hit Die** (as many as needed to full HP, the biggest first), so short rests
  between long rests are at most the character's level; short-rest features
  recharge (Second Wind's one use, Action Surge). The party's henchmen take
  it too.
- **R2 Clock**: stays still for now; an ini switch moves it (Long Rest +8 h,
  Short Rest +1 h) to test later (owner: "we'll have to test with the clock
  moving").
- **R5 Hit Dice**: one per level, the class's die (rules::HitDie2024), all
  back on a Long Rest; shown by `!rest`.
- **D1 Death saves**, for a player character with a henchman (in the party,
  in the area, standing): at 0 HP it lies unconscious (the engine's dying
  state, HP never below 0) and rolls every 6 s: d20 10+ a success, a natural 1
  two failures, a natural 20 wakes it at 1 HP; damage at 0 HP a failure (a
  critical hit two); 3 successes Stable, 3 failures dead (the module's death
  panel and respawn); damage left after 0 at least the HP maximum kills
  outright. When the fight ends (no living hostile within 30 m) it wakes at
  1 HP, saves done or Stable; when every henchman with it is down or dead, it
  dies. **Without a henchman**: NWN's (the campaigns' instant death).
  *Confirmed (owner, 2026-10-23), asked whether a standing henchman should
  hold off massive damage or the death panel: "option 1, there have to be
  some failure states" - massive damage and a third failure kill at once,
  the panel straight away; only "no henchman left standing" waits for the
  fight.*
- **D2 Henchmen**: SoU's handling in every campaign - at 0 HP a henchman lies
  at 1 HP, uncommandable, and gets up when healed or when the fight ends.
  *Revised (owner, 2026-10-23, 0.19.4): "do not use that for henchmen, the
  player needs to get them up manually after the fight" - only healing gets
  them up (a spell, a healer's kit, a Short Rest's Hit Dice).*
- **Temple**: on respawn the henchmen join the player at the temple at full
  health; and a Long Rest in the temple's area (the campaign's respawn
  waypoint's) summons every henchman who is down or left behind, at full
  health.
- **D3 Waking**: any healing wakes (NWN's potions, spells, healer's kits);
  no 1d4-hour rule (the fight's end wakes).
- **D4 Defy Death** (Champion 18): advantage on death saves; 18-20 counts as
  a 20.
- **D5 Exhaustion**: 1 level on coming back from 0 HP; each level -2 to d20
  tests (attacks, saves, checks) and -5 ft speed; level 6 is death; a Long
  Rest removes every level, a Short Rest none (owner, 2026-10-23).
- Build: **5a** rests, Hit Dice, the Short Rest button, short-rest
  recharges; **5b** death saves, henchmen, the temple, exhaustion, Defy
  Death.
*5a built (0.18.0, key `rests`, `rest_clock`):* rules/hit_dice.{h,cpp}
(unit-tested: the pool, the record's packing, a Short Rest's dice);
rules_rest.cpp - CNWSCreature::Rest hooked (all or nothing: +0x854 / +0x85C
past any rest), the rest listener gives every Hit Die back (record
nwn5e_hit_dice_spent), `!shortrest` and `!rest`, the Short Rest (out of
combat: no enemy that sees you within 30 m, not exerted; the party's
henchmen; Second Wind a use back through IncrementFeatRemainingUses);
hud_rest.cpp - in single player the party bar's Spellbook place becomes
Short Rest (the Rest model and art for now, tooltip 16000438) and Spellbook
moves to the dead Player List place (its art via the model's ReplaceTexture,
tooltip 7038, click), converted in CPanelPartyBar::Update (slot 11) once
per bar, greyed with Rest. A click is taken at the player's next server
update. The icon: the owner's own n5_but_srest.tga (private layer, 0.18.1); the
visual: short_rest_vfx (0.18.2; 221 Lesser Restoration's by default, 0.18.4).
*5a verified (owner, 2026-10-23: "5a is all checked and correct, just wasn't
able to check H8 and H9"): the button, Short Rests (refused mid-fight, Hit
Dice running out), Second Wind back, the Long Rest (every Hit Die back, all
or nothing). Not checked: a henchman resting along (H8), the button greyed
with Rest (H9).*
*R2 accepted (#49; owner, 2026-11-01, 0.59.17: "Rest passes"):* with `!test rest`'s rest_clock on (the log: a
Short Rest at 22:37:23, a Long Rest at 22:37:59), the R2 rows pass - a Long Rest +8 h, a Short Rest +1 h, an
interrupted or refused one nothing (the owner's eye, on #105's clock; the log doesn't record the jump). Still off
by default (rest_clock=0, as R2 decided before the game had a clock); whether faithful rests move it by default
is a decision of its own (#110).
*Decided (#110; owner, 2026-11-01: "go with B for 110"):* faithful rests move the clock by default - a Long Rest
+8 h, a Short Rest +1 h, as SRD 5.2's rests take; NWN mode keeps NWN's still clock (its rests don't install the
rest rules); `rest_clock=0` keeps it still. *Built 0.59.18:* rest_clock's built-in value and nwn5e.ini's are 1
(build\nwn5e.ini, never overwritten, gets it from the built-in value where it has no line); `!test rest` still
turns it on where an ini turns it off.
*Accepted (owner, 2026-11-02: "F1, F3-1 and 2 and 110 all pass").* #110: a Long Rest moves the clock 8 hours and a
Short Rest 1 by default, faithful.
*Long Rest limits (#135; owner, 2026-11-05: "do the 2024 rules restrict number of long rests at all per period of
time?").* SRD 5.2: "After you finish a Long Rest, you must wait at least 16 hours before starting another one";
"To start a Long Rest, you must have at least 1 Hit Point"; an interrupted rest of 1 hour or more gives a Short
Rest's benefits, and it can be resumed for 1 more hour per interruption. R1 has no limit.
*Decided (owner, 2026-11-06: "Also impose the 16 hour limit but also make it so the user can remove this limit as
part of the house rules we talked about earlier"):* the 16-hour wait between Long Rests, faithful (on by default),
on the game clock; a [house_rules] switch removes it (off by default, as every house rule). The 1 HP minimum
(explained: the resting character already can't rest while down, so it only changes a downed henchman) and the
interrupted rest's Short Rest benefits: asked.
*Built 0.82.0 (#135, the 16-hour wait):* rules::GameHourOf (NWN's calendar: 12 months of 28 days) and
LongRestWaitLeft (0 with no Long Rest yet, NWN mode, the house rule or a clock behind the last end; test
LongRestWait, 10 of 10 mutations caught). rules_rest.cpp records each Long Rest's end as a game hour (nwn5e_long_rest_end, a
character record; + the 8 hours rest_clock moves) and gates a rest's start (rest.h's new AddRestGate, asked before
NWN's CNWSCreature::Rest): refused while the wait lasts, the player told "no Long Rest yet - N hours more to wait";
!rest says when the next may start. [house_rules] long_rest_any_time (config, nwn5e.ini; announced on #123) lifts
it. With rest_clock on it comes to one Long Rest a game day. "!time <hours>" (test commands; rules::ParseClockHours)
moves the clock 1-48 hours to test it without waiting (7 of 7 mutations caught). A !test profile's ForceRest
clears the wait (its own Rest may follow); a module script's ForceRest (an inn) counts as a Long Rest.
*Decided (owner, 2026-11-06: "1 yes, 2 keep, 3 a+b, 4 B"):* the 1 HP minimum (b) yes; the interrupted rest's
Short Rest benefits (c) not taken - an interrupted rest still gives nothing (R3).
*Built 0.82.1 (#135 b):* rules::DownBlocksLongRest (test LongRestWait, 3 of 3 mutations caught); the rest gate
refuses a Long Rest's start to a creature that is down - dying, or nwn5e_down / a henchman these rules hold down
(rules_dying.cpp's IsDownForRest) - and tells a downed henchman's master "<name> is down and doesn't rest - heal
them first (a spell, a healer's kit or a Short Rest), then rest"; the player's own rest goes ahead.
*Accepted (owner, 2026-11-06: "HD and LR seem fine and accepted").* #135.
*A grace rest (#138, follow-up of #135; owner, 2026-11-06: "WZ-5/6 cannot long rest due to the 16 hour limit (maybe
there can be an extra grace long rest at the beginning of the game?)").* Explained: the log showed a Long Rest
four minutes before the refused one (the wait as built); a new character has no wait; a !test profile that
builds a character already cleared it once. Options: keep it, a grace rest per game start, test-only.
*Decided (owner, 2026-11-06: "give one grace rest at the start of the game, \"!test\" allows for a grace rest
too"):* each game start's first Long Rest skips the 16-hour wait, and any !test profile grants it again.
*Built 0.85.0 (#138):* rules::LongRestGraceLeft (test LongRestWait). rules_rest.cpp hooks CNWSModule::
LoadModuleFinish (a new game or a saved one loaded; the server's MainLoop is its one caller) and stamps the game
start; the local nwn5e_rest_grace holds the stamp of the game whose grace a creature used (a finished Long Rest
uses it), so the rest gate lets the first one through and logs "this game's grace rest"; !rest says so. Every
!test profile resets it ("rest: the next Long Rest skips the 16-hour wait").
*Corrected (owner, 2026-11-06: "GR-1 no, the grace rest would be just at the start of the game, after character
creation, not everytime a game loads").* Taken as: one grace Long Rest per character (a new character's first
rest has no wait anyway, so the grace is the one extra rest the wait would refuse), not renewed by a load; a
!test profile still grants it again. *Built 0.85.1:* rules::LongRestGraceApplies (a rest the wait would refuse,
the grace unused; test LongRestWait) replaces the game-start stamp, and the LoadModuleFinish hook is gone. The
rest gate lets such a rest through and marks it (nwn5e_rest_grace_now); its end uses the grace (the record
nwn5e_rest_grace_used, saved with the character). A !test profile resets the record; 0.82.0's clearing of the
wait after a profile's own rest is gone (the grace does it: one rest either way).
*Removed (owner, 2026-11-06: "wait, so there's already a rest available at the start of the game? never mind
then. I just wanted the player to have access to one rest. remove it and we can accept this").* 0.85.2 takes the
grace rest out: the rest code is 0.84.4's again (#135's wait as accepted - a character with no Long Rest yet may
rest at once, and a !test profile that builds a character clears the wait after its own rest, so one may
follow). *Accepted (owner, 2026-11-06: "remove it and we can accept this").* #138 closed.
*Resting a lot (#110; owner, 2026-11-01: "are there any consequences in-game for resting too many times, like
events tied to calendar days that can be missed?"). Researched 2026-11-01* (the campaigns' own .nss, which every
official module ships, and the NWN Lexicon's SetTime / SetCalendar notes): nothing in the official campaigns can be
missed by the date - no quest, deadline or leaving NPC reads it. The Prelude reads no clock at all (interiors, fixed
lighting). Three of the "~6 gameplay" uses above are never wired up: Chapter 2's night-only encounter (no placed
encounter uses it), HotU's hour-gated doors (TryToCloseAllDoorsInArea, never called) and its once-a-day throne
search (no dialog uses it). What a jump does: timed effects end sooner (their expiry is on the same clock - a Short
Rest takes 20 rounds off every buff, summons' 24 h end after 3 Long Rests; the resting PC's own buffs already go with
NWN's Rest); pending DelayCommands fire at once; outdoor day and night, lamps and NPCs' night posts follow on their
next heartbeat (some NPCs stand elsewhere at night); stores never restock (no time in them); an hour past 23 wraps
into the next day (the Lexicon). The only stored-time checks: the Blacklake bartender's weekly earnings (works once
either way) and the EE paladin mount's 24 game hours. SoU moves the calendar itself, always forward. Not seen in game:
whether the lighting snaps or fades; heartbeats and encounter resets after a jump (inferred from the engine's fields).
*A visible clock (#105; owner, 2026-11-01: "If time is an important part of the game, is affected by resting and
has effects on in-game npc routines, shouldn't there be a visible clock?"). Decided (owner, 2026-11-01): "I see
the time is already displayed on hover on the compass, so there should be a clock on top of the compass on the
bottom right, we'll have to look into how to make it fit the UI but for now try to come up with something
appropriate, the sun and moon icons are a nice touch".* A game day is 24 hours, an hour the module's "minutes per
hour" of real time (2 in the owner's module: a day in 48 real minutes).
*Built 0.59.8 (#105):* clock_panel.cpp - a client NUI window as the resource panel's (input passes through): a dark
card, the sun (a disc in a ring of dots, orange at dawn and dusk) or the moon (a pale disc, a shadow over part of
it), and the game time "HH:MM" - the game minute worked out from NWN's (GetTimeMinute counts the module's real
minutes within the hour: rules::ClockGameMinute with HoursToSeconds(1)). Read once a second through the script
bridge (GetTimeHour / Minute / Second, GetIsDawn / Day / Dusk), the GUI size every 2 s; placed by rules::PlaceClock,
its centre 64 GUI units from the right edge and its bottom 132 over the bottom (x the GUI scale) - a first guess at
the compass's top; the compass's place isn't read from the engine yet, so the spot is two test switches (clock_x,
clock_y; `!test clock`, data/tests/clock.txt) to fit it. `!clock` turns it off / on. Tests HudClockTime,
HudClockPlacedOverTheCompass and the switch ranges (10 of 11 mutations run, all caught - the check was stopped for low memory before clock_y's lower bound, whose test row is in).
*The owner's 0.59.8 run (2026-11-01):* the clock "seems fine it's just not pushed all the way to the right edge of
the screen nor against ther compass at the bottom"; and #106's C5: "charming an attacking PC doesn't stop the
attack" (the log: two attack rolls on the charmer after the charm - GetAttackTarget reads none between swings).
*Built 0.59.10:* the clock's card flush with the right edge (rules::PlaceClock lets the window's
transparent pad of 4 hang past the edges; centre 37 from the right), its bottom 120 over the bottom edge, and
`!clock right / bottom <n>` / `!clock where` to fit it live. The charm reads the charmed player's orders directly
(the queued attack, the one carried out, the attack targets, the round's target) and clears them with the combat
state (ClearAllActions(TRUE)); an attack roll on the charmer is no roll but a miss, the attacker's orders
cleared (an attack-roll skipper, the Shove's way). Tests HudClockPlacedOverTheCompass (3 mutations caught).
*The owner's 0.59.10 run (2026-11-01):* C5 and the Rage upright (0.59.9, #108) pass ("the other items in the
matrix pass"); the clock: "the default clock sits at about 41 units right, any value under 41 changes nothing
and it's still not completely against the right edge of the window. clock bottom 125 is correct". *Built
0.59.12:* NUI keeps a window on screen, and a group clips its children's draw lists at its pad, so the card
stayed 4 in from the edge: the clock's root is a column now (no group) and the card is drawn to the window's
edges (the window's own clip is its whole rect); the window is 74 x 26, flush right (centre 37), its bottom 129
over the bottom edge - the card where the owner fitted it (125 for the old window, whose card ended 4 over its
bottom). Test HudClockPlacedOverTheCompass (2 mutations caught).
*0.59.13 (owner: "the clock is unvisible now as the frame is dominated by the scroll bar elements"):* a column's
row takes its spacing under it (4), so the 26-high window's 18 inside its pad overflowed and grew scroll bars
(the group had kept them off). The window is 8 taller than the card now (74 x 34), the card at its top - flush
right still, its bottom 121 over the bottom edge (the card's 129). Test HudClockPlacedOverTheCompass (2 mutations caught).
*0.59.15 (owner: "clock still has scrollbar elements which half covers the numbers"):* with no group, NUI's window
shows scroll bars, taller or not. The group root (scrollbars 0) is back - 0.59.10's layout, which showed none - so
the card is the group, 4 in from the window's edge; the window flush right (centre 41), its bottom 125 (the
owner's fit with this layout). A card fully flush would need the window past the screen's edge, which NUI
doesn't allow as built (its edge_constraint is untried). Test HudClockPlacedOverTheCompass (2 mutations caught, the flush one after a check that the built-in spot is flush by itself, not by the on-screen clamp).
*0.59.16 (owner, 2026-11-01: "try edge_constraint"):* the clock's window gets an edge_constraint of -8 all round
(NUI's window rect, untried) and may hang its pad of 4 past the right edge (rules::PlaceClock's overhang; centre
37), so its card can end on the edge where NUI lets it - where it doesn't, the window stays flush as in 0.59.15.
`!clock edge <n>` / `!clock edge off` change it live (the window reopens; off = null, as before), and the log
says how each window opened. Test HudClockPlacedOverTheCompass (2 mutations caught).
*The owner's 0.59.16 run:* "clock is missing" - the window opened (the log), out of sight; then "edge 0 is almost
good, I think it would need to be edge -5 to be perfect, but this is close enough". So edge_constraint is the
window's margins from the screen's edges: 0 lets it reach them (null keeps it further in), -8 put it out of
sight. *Built 0.59.17:* edge_constraint 0 built in (`!clock edge <n>` still tries others).
*Accepted (owner, 2026-11-01: "it's still the same, let's leave it there and continue testing")* - the clock as
0.59.17 places it, a few units short of flush right (the owner's "-5 to be perfect" left as is: below 0 NUI can
put the window out of sight).
*5b built (0.19.0, key `dying`):* rules/death_saves.{h,cpp} (unit-tested: the
saves, the Exhaustion penalty on them, Defy Death, damage at 0 HP, massive
damage, the speed penalty); rules_dying.cpp -
CNWSCreature::DoDamage hooked (HP read before and after: a player character
with a henchman standing is held at 0 HP in the engine's dying state, a
player's henchman at 1 HP; damage at 0 HP adds failures, a critical hit -
seen by the attack-roll listener - two; a death is HP -10, so OnApplyDamage
kills through the engine's own path), CNWSModule::RunEventScript hooked
(OnPlayerDying skipped for a player in death saves, or dropped into them in
the last 5 s - the event is queued; OnPlayerRespawn: the henchmen summoned
4 s later, once the campaign's own jump is done), the creature update (every
100 ms: a death save every 6 s of unpaused time, waking, dying; down
henchmen laid down - plot, ANIMATION_LOOPING_DEAD_FRONT, uncommandable, their
own plot flag kept in a local - and got up), the rest listener (a Long Rest
removes Exhaustion; in a temple's area - NW_DEATH_TEMPLE, NW_DEATH_PLACE,
X1_TPORT_LOC, SoU's X0_RESPAWN_LOC, the last respawn's area - it summons the
henchmen down, dead or left behind). Exhaustion is one linked supernatural
effect (attack, save and skill decreases - kept by the 2024 rolls - and
movement speed), tagged by level and checked every 6 s. The down states are
locals too (nwn5e_down), so a saved game picks them up again. `!dying` shows
the state; with test_commands, `!dying roll <d20>`, `!dying hurt <HP>
[henchman]`, `!dying exhaustion <level>`. Calls made while building:
- "The fight is over" = no living enemy within 30 m for a round (6 s), as
  decided; enemies near that never touch the party (none sees one of it or
  is seen) end it after 12 s, so a caged or sleeping monster can't keep
  anyone down for good. *0.19.2:* 0.19.0 asked for an enemy that could see
  the party, at once - it woke a player 36 ms after the drop while the enemy
  coming had not spotted anyone yet (owner, 2026-10-23: "a PC dying during
  combat gets up immediately"). *0.19.3:* it also goes on while someone of the
  party still up attacks a living creature, however far (its attack target,
  GetAttackTarget's field; owner: "a good check is 'if the henchman is
  actively fighting' as well").
- *0.19.6, replacing the above:* the fight is the engine's own "in combat":
  it goes on while a henchman still up is in combat (GetIsInCombat, +0x700,
  kept 8 s past the last blow) or a creature in combat has someone of the
  party as its attack target, and is over 2 s after neither. Distance is no
  test - in the Prelude 10 hostiles within 30 m that weren't in the fight
  kept a Stable player down for good (owner, 2026-10-23: "fight over trigger
  doesn't happen"). *0.19.7:* a henchman counts as fighting only with a
  living attack target (the engine keeps "in combat" 8 s past the last
  blow, so 0.19.6 woke the player ~10 s after the last enemy fell); over 3 s
  after no one fights.
- The death saves, the fight watch and their retries run on game time: the
  passes' steps while CServerExoApp::GetActivePauseState is 0. *0.19.5:* the
  engine keeps updating creatures while paused, and 0.19.4 (wall clock) went
  on rolling death saves through a pause (owner, 2026-10-23).
- A down player keeps lying: its posture (+0x6B4: 3 / 4 lying) and animation
  (+0xD8) are logged when they change and the posture is put back if
  something stands it up (0.19.5; owner: the player "got up after the 15
  roll" at 0 HP - the engine's own writes keep a dying creature lying, so
  the logs are to show what did).
- A respawn removes Exhaustion like a Long Rest (else a death at level 6
  would come back at 6).
- Henchmen don't gain Exhaustion (the player character only).
- Death magic (EffectDeath) still kills a henchman for real: the campaign's
  OnDeath, as before.
*0.20.0 - what the player sees (owner, 2026-10-23: "some on screen indicator
of how many failures and successes have happened, a vfx while the PC is doing
the rolls and a status icon indicating how many levels of exhaustion"; then
"Build the prototype and your recommendations for 2 and 3 are good"):*
- **Death-save marker** (a prototype, to judge in game): a sprite over the
  down player, always facing the camera. *0.20.1, the owner's layout:* an
  icon with three thin bars each side, successes left (green, all blue when
  Stable), failures right (red), filled from the icon out; the icon is the
  Pale Master's Bone Skin (ife_x2bonesk1, half face, half skull - successes by
  the face). *0.21.1:* the icon is the game's own texture by name - a second,
  smaller sprite with `texture ife_x2bonesk1`, loaded from the player's
  install as it is (its dark square, not faded) - so nothing of the game's is
  in either repo and the public build shows it too; the bars are nwn5e's
  texture with a clear middle (before: the icon baked into private textures,
  `--center`, a plain disc in public). Ten ASCII effect models of nwn5e's own
  (two emitters, update Single / render Normal, like the game's vdr_seen
  marker; the icon's renderorder after the bars), 1.4 m over
  the creature's root node (its head lies on the ground), and their art -
  generated by tools/data/death_save_marker.py; visualeffects rows 1560-1569,
  one effect per state, swapped as the saves change. `death_save_marker`.
- **Visuals**: an aura while rolling (512, red pulse) or Stable (513, blue),
  a flash per roll (199 success, 203 failure, 217 natural 1, 68 natural 20);
  `death_save_*_vfx`, 0 = none.
- **Exhaustion's icon**: EffectIcon of effecticons rows 200-204 (the game's
  Fatigue icon) or 210-214 (the owner's numbered art, n5_ief_exh1..5.tga in
  the private layer, used per level once served), tooltips 16000439-443; the
  penalties' own icons hidden (HideEffectIcon). EffectIcon takes rows 1-255.
- **The exhausted look** (0.22.2; owner, 2026-10-24: "exhausted levels 1 and 2
  the PC should have the pausetrd animation when idle. exhaustion levels 3,4
  and 5 should add the injured walk"): EE's ReplaceObjectAnimation on the
  player character - pause1 / pause2 -> pausetrd from Exhaustion 1, walk ->
  walkinj from 3 (run unchanged); sent on a change (the player's upkeep, and
  at once on a level change), the whole state after a load so a kept swap is
  undone; `!anim`'s Stop has it sent again. `exhaustion_look`.
- **Bleeding at low hit points** (0.22.3; owner, 2026-10-24: vfx 491, the
  critical head spray, "too severe" - "just some mild dripping, then dripping
  with a small puddle, then more bleeding"; 0.22.4: "just do 1 bleeding
  effect, mild, with a small puddle when the PC is under 10%"): one duration
  effect of nwn5e's own (tools/data/bleed_vfx.py; visualeffects 1570
  NWN5E_BLEED, root node, the game's fxpa_sphere and blood textures by name) -
  drops falling from waist height and blood splats on the ground that spread
  and fade where they fell (a walking character leaves a short trail). 0.22.5
  (owner: "there isn't a puddle on the floor like in 491"): the puddle is made
  as the game's VFX_COM_BLOOD_CRT_RED_HEAD makes its own (its M_Red_Sticky
  emitter, read from vwp_l_blood_red: Fountain, texture `blood` on a 2x2 sheet,
  random frame, flags bounce + random + splat, size 0.018 -> 1 over 5 s) -
  sticky drops that flatten where they land and spread (ours to 0.4 m over
  6 s, under one a second); the flat Aligned_to_World_z quad never showed.
  0.22.6 (owner: "still no puddle"): two a second, 491's red, spreading to
  0.8 m, thrown a little outward so they land around the feet instead of
  hidden under them; and a check row, 1571 NWN5E_SPLAT_TEST - 491's own
  sticky settings (150 a second for 0.15 s, velocity 2, to 1 m over 5 s) in an
  ASCII model of ours: a puddle from it and not from 1570 = tuning; none from
  either = an ASCII model's splat doesn't work, another way needed. Both left
  puddles (owner: "they both have puddles, the low health effect is good
  now"); 0.22.7: the falling drips move with the character (the emitter's
  inherit flag - "can the dripping blood follow the character model"), the
  splats stay where they land; the check row 1571 removed. 0.22.8 (owner: "the
  dripping effect is no good, remove it just leave the puddle"): the drips are
  gone - the effect is the puddle alone.
  low_hp_look.cpp puts it on the player character under 10% of its hit points
  (checked each second, put back every 6 s if stripped), none at 0 HP.
  `low_hp_bleed`, `bleed_percent`.
- `!vfx` has a search field (owner: "can you add a search bar to the vfx
  window?"): the list narrows as you type, by name or row number.
- `!anim` (0.22.1; owner, 2026-10-24: "is there a way to preview walking
  animations?" - "build the !anim window"): a window of the humanoid model's
  84 animations (a_ba / a_fa), movement first, with the same search. A click
  plays one in place, looping (EE's ReplaceObjectAnimation swaps the look-far
  loop for it, then ActionPlayAnimation); "Walk / Run a loop" walks 5 m out
  and back twice with the selection as the walk / run (none: the game's own);
  speed 0.25x-2x; "Play typed" plays the search text as a name; Stop or
  closing restores everything. Engine facts (nwmain's animation switch, table
  RVA 0x963E88, ids 0-140): 1 ready, 2 walk, 3 walk backwards, 4 run, 5-8
  knockdown / dead, 14 damage, 15-20 conjure / cast, 24-27 combat steps
  (cwalkf/b/r/l), 28 taunt, 32 meditate, 33 worship, 35 bow, 36 sit, 37 steal,
  44-46 victory, 53-58 turns and idles, 70 drink, 71 read, 86-87 runfl/runfr,
  97-140 Custom1-40 (a_ba has none of them); walkinj (the limp) and walkdead
  have no id - the walk case picks them by state.
- `!vfxedit` (0.23.0; owner, 2026-10-25: "a small vfx editor where I can make
  new or edit existing VFX to make new ones" - "Something that can be used in
  game would be ideal"): a window with one effect (name, stays / plays once,
  seconds, on root / head / impact) and its emitters, each with the ASCII
  emitter's parameters (update, render, blend, texture, position, rate, on-for
  seconds, life, speed, random speed, spread, gravity, spawn area, size,
  colour, alpha, sheet grid and last frame, blur, bounce; follows the creature,
  splats, bounces, random frame); Preview on me plays it with the game's own
  renderer. How: data/vfx_model.cpp writes an ASCII effect model under a new
  name each time (n5vx0001...) into build\vfxedit, registered with the
  resource manager watching for changes (RemoveResourceDirectory +
  AddResourceDirectory when the new file isn't found); visualeffects row 1600
  (visualeffects.editor.txt, empty) is pointed at it in the loaded table -
  the client reads a visual effect's model columns from g_pRules +0x130
  (CTwoDimArrays) +0x280 (the visualeffects C2DA) each time it shows one
  (0x96D37F, 0x8729D0: the column names inline), so C2DA::SetCExoStringEntry
  changes what row 1600 shows; a new name each time, so no cached model. Save
  writes build\vfxedit\saved\<name>.vfx (the draft) and <name>.mdl; Load
  lists the drafts and nwn5e's own n5_*.mdl. 0.23.1: Load takes a game
  effect by its visualeffects row - its compiled model (root, impact or head
  node) read where the player's game has it (ReadGameResource) and
  data/vfx_model.cpp's binary reader: emitter nodes by their header (node
  header 0x60, update +0x18, render +0x38, blend +0x58, texture +0x78, x grid
  +0x0C, flags +0xD4: bounce 0x10, random 0x20, inherit 0x40, splat 0x200),
  controllers from node +0x44 / +0x50 (offsets from byte 12), the emission
  time from the animations' copies of the node - the game keys birthrate down
  to a negative value (491's sticky 150 -> -150 over 0.033-0.3 s: off at
  0.17 s, its spray off at 0.167 s) - and a one-shot effect's length from its
  impact animation (header length at +0x70: 491, 4.97 s). Checked locally on
  491 (vwp_l_blood_red: sticky, spray, cloud) and vwp_s_low_red; nothing of the
  game's in the repo - a unit test builds its own compiled model. Written
  models now carry the impact's detonate event (Explosion emitters burst on
  it).
- **Animation recipes** (0.24.0; owner, 2026-10-25: "After that maybe look into
  it is possible to edit animations" - "Let's try recipe" - "Confirm the
  recipes"): the body skeletons (p + m/f + a/d/e/g/h/o + 0/2, ASCII) take their
  animations from libraries - a_ba (human, elf, halfling male), a_fa (their
  females), a_da (dwarf, gnome, half-orc male), a_dfa (dwarf, half-orc female)
  and the large-build '2's, a_ba itself -> a_ba_non_combat -> a_ba_med_weap.
  At startup core/anim_packs.cpp builds each recipe (data\anims\*.recipe;
  animedit\recipes\*.recipe, the owner's) into an "animation pack" per
  library (n5_ba ... n5_dfa2: the library's bones as dummies, the recipes'
  animations; supermodel -> the library) and serves copies of the skeletons
  pointing at the packs (pmh0 -> n5_ba -> a_ba) - all from the player's own
  game files, nothing of the game's in nwn5e. A recipe: a base animation mixed
  toward another by an amount (0 the base, 1 the mix, below 0 exaggerated),
  a speed, the part kept (data/anim_recipe.cpp: the compiled library reader -
  model root +0x48, animations +0x78, supermodel +0xA8; animation length +0x70,
  animroot +0x78; node 0x70 bytes, name +0x20, children +0x48, keys +0x54,
  values +0x60; position 8, orientation 20 as quaternions - quaternion
  blending, ASCII axis-angle out). Tested first from a script through the
  game's development folder (owner: "n5test works"); the C++ output matches
  it. `!animedit`: name, base, mix toward, amount, speed, kept part; Save
  (animedit\recipes), Load, Play base / mix / recipe (a new or changed recipe
  plays after a restart - a character's model stays loaded). First recipe:
  n5_psnmild (the poisoned idle, half way toward the normal one).
  `animation_recipes`. Not yet: a live preview (a mannequin with a freshly
  named skeleton is the idea to try); custom content that overrides the
  skeletons in a hak would be overridden by the served copies.
  0.24.1 (owner: "make it so the base and mix toward animations are
  selectable from a dropdown" - "same for the vfxedit, I can't remember all the
  vfx and particle names"): `!animedit`'s Base and Mix toward are dropdowns of
  the human library chain's animations (read at startup, !anim's descriptions
  as labels; a_ba_custom, an ASCII library at the chain's end, isn't read);
  `!vfxedit`'s texture is a dropdown of the particle textures the game's
  effects use and its Load takes a game effect from a dropdown of the rows
  whose model has emitters - read once at the first open (checked locally on
  all 459 effect models the table names: 380 with emitters, 1652 emitters, no
  failure, 3.6 s from disk).
  0.24.2 (owner: "the saving closing and openning the game just to get a
  preview is not great. could we get a webapp working" - "don't make it an
  artifact, create an actual webapp I can run locally"): the live preview is
  the Animation Recipe Bench, a local web app in `tools/recipe_bench/`
  (`recipe_bench.bat`: a Node server on 127.0.0.1:8765, no dependencies, that
  opens the page). At startup nwn5e copies the 10 animation libraries it reads
  (a_ba and its chain, a_fa, a_da, a_dfa and the large-build '2's) to
  `build\animedit\libraries\`; the bench reads them from there, and three
  stick figures on a canvas play the base, the recipe and the mix with the
  same maths as `anim_recipe.cpp` (`public/core.js`, a line-for-line port,
  checked against the C++ on a_ba). Save writes `<name>.recipe` straight into
  `build\animedit\recipes\`; Open lists those and nwn5e's own. One restart
  when the recipe is done. nwn5e also reads `*.recipe*.txt` there (what Notepad
  or a browser download makes of a recipe), the newest file of a name winning,
  and `!animedit`'s Load lists them.
  0.25.0 (owner: "I want to make a crouch walk animation" - "go, use stealth
  mode as the trigger"): recipes get a crouch step after the mix, speed and
  trim - `crouch` (metres the hips drop, 0-0.6) and `lean` (degrees forward,
  -45-60). The legs are solved again on the library's bones (thigh and shin,
  the knee in the plane it bent in, measured off the old hip-ankle line; a
  nearly straight leg bends the way its thigh faces) so each ankle stays where
  the animation had it: same footfalls and stride, deeper knees; the head tilts
  70% of the lean back. Why not a mix toward a crouched pose: measured, walk ->
  getlowlp 0.3 sinks the planted foot 14 cm into the floor, never lifts the
  other and shortens the stride 25% (0.5: stride halved). Checked on the real
  a_ba walk: ankles within 2.6 mm between keys (exact at keys), lowest ankle
  0.128 (the walk's 0.13); C++ and the bench's core.js agree to 4 decimals.
  The skeletons are uniform scales of their library (animationscale) except the
  halflings' shins (~10% shorter: the game's own walk has the same mismatch on
  them). nwn5e's `n5_crwalk` (walk) and `n5_cridle` (pause1), crouch 0.25 m,
  lean 15, are swapped in while a creature is in stealth mode - any creature
  with a part-based body (appearance.2da MODELTYPE P), both rule modes
  (`stealth_crouch`). `stance_look.cpp` is now the one place that swaps idle and
  walk: the crouch wins over the exhausted look (rules_dying.cpp passes the
  level in). The bench has Crouch and Lean sliders. Not yet checked in game:
  whether stealth mode moves at a speed the walk's stride matches (foot slide).
  0.25.1 (owner: "it's an improvement, the main issue is that the pose still
  keeps a straight back, instead of leaning forward"): measured, lean 15 from
  the waist only gave a spine 12-16 degrees forward - too little to read from
  the game's camera. Lean is now a hip hinge (the pelvis tips 35% of it, the
  legs solved again so the feet stay put - lean works without a crouch too),
  the arms are turned back to hang as the animation swings them (a side profile
  showed them trailing back with the torso, a skier's tuck), and n5_crwalk /
  n5_cridle lean 30 (spine 31 / 27 degrees forward, plus the hip tip).
  0.25.2 (owner: "increase the lean range"): lean goes to 90 degrees (the torso
  level; the game's own pick-up squat leans ~70), in the recipe files and the
  bench's slider (-45 - 90, was -20 - 45 on the slider, -45 - 60 in files).
  The owner then tuned the crouch in the bench (n5_lowcrwalk / n5_lowcridle:
  crouch 0.4, lean 70 - "as lowcrwalk and idle not normal crwalk and idle"):
  nwn5e's n5_crwalk / n5_cridle take those values (checked on a_ba, a_fa,
  a_da, a_dfa: feet within 3 mm of the footfalls, never below the floor; the
  torso near level, the back knee ~17 cm off the floor at heel strike).
  0.25.3 (owner: "the crouch walking isn't correct though, the PC doesn't lean
  forward" - the idle did): holding a weapon, the engine layers walk_swordr /
  walk_swordl / walk_shieldl / walk_bowl (literal names in nwmain; animroot
  torso_g, keying the torso, neck, head and arms) over the walk, standing the
  torso back up - measured, the crouch walk under the game's layer: spine 1
  degree forward. A recipe on an animation with those layers now also builds
  <name>_<hold> from <base>_<hold> with the same settings, and the crouch step
  keys only what hangs from a clip's animroot (the layer: torso, head, arms);
  stealth mode swaps walk_<hold> -> n5_crwalk_<hold> with the walk (spine 71
  degrees armed). Unverified: that ReplaceObjectAnimation reaches the layers
  (the engine plays them by name; the walk swap works). The bench previews the
  layer (Holding).
- **Stealth marker** (0.26.0; owner: "how possible is it to show enemys' sight
  ranges?" - recommended an eye/ear marker first, rings after - "build your
  recommendation"): while a player character hides, an eye over each hostile
  within 40 m that sees it, an ear over one that only hears it (the game's
  isk_spot / isk_listen by name; tools/data/stealth_marker.py; visualeffects
  1571 / 1572 on the head; stealth_marker.cpp, once a second, from
  GetObjectSeen / GetObjectHeard). Cleared when it stops hiding, and an area's
  leftovers when it is entered. `stealth_marker`, both modes.
  Light (owner: "are light sources and general light level taken into
  account"): read from nwmain - DoSpotDetection adds the area's Spot modifier
  ([area+0x21c]) and, only at night (module+0x3d4 == 2, what GetIsNight reads):
  a watcher with no light visual on itself (visualeffects 153-180,
  VFX_DUR_LIGHT_*) and no darkvision visual (182) spots at -5
  (SPOT_NO_LIGHT_MODIFIER), a hider with a light visual is spotted at +10
  (HIDING_LIGHT_MODIFIER). Not taken into account: lamps and braziers placed in
  the area, the area's own lighting, dark interiors by day, racial darkvision
  (a feat, not the 182 visual). SK5 leaves these terms as they are. The 2024
  way (darkness heavily obscured: hide anywhere; dim light: disadvantage on
  sight Perception, -5 passive) would need a light level per spot - a
  decision for later.
- **Range rings and investigating noises** (0.27.0; owner: "next up we'll add
  the range indicators ... for the hearing detection, do option 2"):
  stealth_marker.cpp became stealth_watch.cpp, one pass a second over the
  hostiles near a hiding player character. Rings: the nearest stealth_rings (6)
  within 45 m get an amber ring at how far they see and a dashed blue one at how
  far they hear - each creature's own distances, read from the engine
  (DoSpotDetection uses +0x7b0, +0x7b4 in combat; DoListenDetection +0x7b8 /
  +0x7bc; DoPerceptionUpdateOnCreature cuts off at the larger), written on it as
  locals by the creature update, shown by the nearest of 10 radii
  (tools/data/stealth_rings.py: flat self-lit bands, nwn5e's own textures,
  visualeffects 1573-1592, tilted to the ground under the creature). Heard, not
  seen (2024, perception=faithful): NWN's AI never acts on hearing
  (nw_c2_default2: "DO NOT RELY ON HEARING FOR ENEMY DETECTION"; x0_i0_enemy's
  heard-enemy search is commented out); now such a hostile walks to where the
  sound came from in detect mode (a rolled Perception under SK5), looks about,
  waits 4 s and walks back - not one fighting, talking, plot, an associate or
  uncommandable; at most every 25 s. WalkWayPoints leaves a moving / waiting
  creature alone, so the heartbeat doesn't cut it short. heard_investigates.
- **Light in detection - exploring** (0.27.1; owner: "explore if we can make it
  so that environmental light sources can increase detection"). What nwn5e can
  read: the area (interior, above ground, natural; day/night; sun and moon
  ambient/diffuse colours - GetAreaLightColor), each 10 m tile's main lights
  (its coloured ambient) and source lights (its torches / braziers; 0 = off -
  GetTileSourceLight1/2Color), placeables lit (GetPlaceableIllumination: lamps,
  braziers - position known, light radius not), light visual effects on objects
  (153-180, the radius in the name: 5/10/15/20), and a creature's own visual
  list (+0x160, what EE's spot check reads: a carried torch or light item). Not
  readable: the rendered light itself (a model light's real radius, shadows).
  So a light level per spot (bright / dim / dark) is an estimate, to calibrate:
  !light (light_probe.cpp) prints all of it where the character stands, and
  logs it. Proposed, not built: NWN mode - standing in light at night counts as
  carrying one (+10 to spot, EE's own term); 2024 mode - bright normal, dim
  disadvantage on sight Perception (-5 passive), dark heavily obscured (only
  darkvision sees, as dim); and a light gem on the character.
- **Hiding succeeds; searches test it** (0.28.0; owner: "we need to fix the
  hiding, hiding should succeed by default, enemy investigation rolls should
  test it"). Was: a Stealth total below 15 didn't hide (hiding_needs_15) and
  every enemy's passive Perception (10 + mod) beat a low total every 6 s. Now
  (SK5 D, hiders_need_search): hiding always works (hiding_needs_15 off by
  default); an observer that isn't searching can't see a hider - it can hear
  one (passive Perception still), which sends it to investigate; searching
  observers (investigating = detect mode, detect mode, alert in combat) roll a
  d20 Perception against the kept Stealth total, and a searcher's roll against
  a hiding player character is told to it (once, again when it changes). Up
  close (B) still finds a hider. The investigation waits 6 s at the noise so a
  detection pass falls there. Note: a calm enemy watching you hide loses you
  (2024 says you can't hide from a creature that sees you clearly) - the light
  level, when built, can give passive sight back in bright light.
- **Vision cones** (0.29.0; owner: "how hard would it be to develop a system
  where facing direction would matter? So we can implement vision cones" -
  "Go with option 2"). Read from nwmain first: EE's DoSpotDetection already
  has a facing term - a computer observer looking for a player hider gets -5
  when the hider is 135 degrees or more off its facing (dot < -0.707), plus -5
  for a still hider and +5 for a still observer. SK5 E (vision_cone, 120): with
  hiders_need_search, a calm observer sees a hider inside its cone
  (rules::WithinVisionCone, tested) - its passive Perception against the
  Stealth total - and not outside; searching observers see all round; a calm
  look's result is told to the player too. Shown while hiding
  (stealth_watch.cpp): a calm hostile's cone outline turning with its facing
  (OrientWithObject; tools/data/stealth_rings.py, rows 1610-1649: 90 / 120 /
  150 / 180 degrees x 10 radii), a searching one's full ring. The drawings are
  flat and ignore walls (the rule doesn't: sight needs a clear line); a stepped
  cone cut at walls (slices, each its length from line-of-sight tests once a
  second) is the possible next step. Not done: idle guards turning to look
  about.
  0.30.0 (owner: "once discovered the PC leaves stealth mode, this means all
  vision cones, circles, icons, etc, disappear. Vision cones should be 90
  degrees, and they should be filled, not outlined"): SK5 F
  (found_ends_hiding) - a hostile that sees a hiding player character (a
  search, a look in its cone, or up close) takes it out of stealth mode
  (DelayCommand 0: the next script tick, not inside the engine's detection),
  told "<name> ...: you are found - you are no longer hidden"; the watch then
  clears everything. vision_cone 90. Cones filled and translucent (n5_cone_f:
  lighter near the creature, soft sides and end), drawn for 60 / 90 / 120 /
  150 degrees (rows 1610-1649 reused; the 180 outlines dropped). Searching
  enemies keep the ring outline.
  0.31.0 (owner: "would it be possible for the wedge to have different colors
  depending on how easy it would be for the enemy to spot the PC? from 50%
  alpha red for very likely to 20% alpha yellow for very unlikely"): a calm
  look is passive Perception, not a roll, so the colour is its margin over the
  hider's Stealth as DoSpotDetection adds it up (rules_perception.cpp
  SpotMargin, weighed once a second): passive base (10 / 5) + Perception, the
  area's Spot modifier (+0x21c), +5 observer still / -5 hider still
  (AnimationStationary), at night -5 without a light or darkvision visual and
  +10 for a hider's light, minus the kept Stealth total and the size term.
  Colours 0-4 (yellow 20% ... red 50%): 6+ red, 1-5 orange-red, -4-0 orange,
  -9 to -5 yellow-orange, -10 or less yellow. 200 cone models (5 colours x
  60/90/120/150 x 10 radii, rows 1610-1809).
  0.31.1 (owner: "on being discovered the stance should return to standard,
  right now the PC remains crouched. There's also some z-fighting on the cones
  ... weird interference patterns when they overlap"): the log showed the
  restore sent 50 ms after the exit - ReplaceObjectAnimation doesn't restart
  the idle already playing; stance_look now restarts it (pause2 for 0.1 s) when
  the idle changes on a creature doing nothing. Each creature's cone and rings
  get their own lift (id % 11 x 2 cm, EffectVisualEffect vTranslate; hearing
  ring +1 cm) so overlapping ones never share a height. The found log now says
  whether the finder was hostile (a non-hostile's sight doesn't end hiding;
  the 15:59:33 "out of stealth" line was one that isn't).
  0.31.2 (owner: "when discovered the PC still doesn't return to the normal
  stance"): read from nwmain - SetAnimationReplace removes an entry for an
  empty new name; ComputeUpdateRequired flags any change to the list (bit
  0x18) and WriteGameObjUpdate_UpdateObject sends the whole list (count +
  CResRef pairs). The crouch still didn't go on screen, so a swap is now undone
  by mapping the animation to itself (pause1 -> pause1: a changed entry, which
  is how the crouch got there) - in stance_look, !anim and !animedit - and the
  idle restart is 0.5 s; the look log says whether it ran or the creature was
  busy (its action).
  Known, accepted for now (owner, 2026-10-25: "they interfere with some
  environment objects, but I guess that's fine for now"): the wedges are flat
  models, so placeables, walls and uneven ground cut through them or hide
  parts; a stepped cone cut at walls (line-of-sight slices) stays the option.
- **Light where you hide** (0.32.0; owner: "do the sensible distances, I'll add
  light queries down the line"). The !light readings (Training Halls, night,
  interior): moon ambient 58 (dim), braziers / torch brackets found lit by
  GetPlaceableIllumination at 3.9-7.8 m, no light effects; the tile light
  getters return -1 there (not readable; the probe had counted that as lit -
  fixed, and tile lights aren't used). SK5 G (stealth_light): stealth_watch.cpp
  rates the hider's spot once a second - bright: within 4 m of a lit placeable,
  the inner half of a light effect's radius (VFX_DUR_LIGHT_*: 5/10/15/20 m),
  carrying a light (the engine's visual list), or an area lit 110+ (ambient +
  half diffuse, sun or moon by the time, 0-255); dim: within 9 m of a lit
  placeable or inside a light effect's radius, or an area lit 30+; else dark -
  and tells the character when it changes. rules_perception.cpp applies 2024's:
  dim -5 to an observer's sight (its Spot skill call while that hider is the
  target), dark unseen unless darkvision (feat 228 or visual 182) within 18 m
  (then dim); up close still finds; the cone colours include it. EE's night
  terms zeroed in 2024 mode (ruleset.if_light2024.txt: rows 89 / 90 = 0, served
  only when stealth_light). Outdoors not measured yet; thresholds to tune with
  the owner's light queries.
  0.32.1 (owner: "if I leave stealth mode while moving the PC doesn't return
  to normal vertical stance. Light sources should have a circle indicator so
  the player can see where they take effect" - and "a small room with nothing
  in it ... still showed up as dim"): a walking creature whose walk swap
  changes gets CNWSCreature::SetAnimation(0) for one tick, after the engine's
  update, so the next update's movement restarts the walk through the new swap
  (no interruption). Light zones while hiding: each lit placeable within 30 m a
  pale solid ring at 4 m and a dashed one at 9 m, a light effect r/2 and r
  (rows 1820-1835), none when the area is bright everywhere. Indoors /
  underground the area's own light no longer counts (the Training Halls' moon
  ambient 58 made every room dim): dark unless a source is near. Tile-built
  torches stay unreadable (-1), so a room lit only by them reads dark.
  0.33.0 (owner: "make the wedges more transparent and lets change the stealth
  vfx, make it dependent on the light level, 90% visible on light, 70% in dim
  and 50% in dark" - "do not make the ghostly effect pulse, have it maintain
  the transparency level"): cones 12-30% opaque (were 20-50%). The see-through
  look: no ruleset entry or exported client stealth function; an object's
  alpha is its Gob's +0x108, written by Gob::SetColorShifting(colour, alpha,
  propagate) (the other writers: the sky, the area loader). stealth_ghost.cpp
  hooks it: while a player character hides, see-through requests (alpha < 1)
  get its light's opacity (0.9 / 0.7 / 0.5); stealth_watch puts EE's steady
  ghost (VFX_DUR_GHOST_TRANSPARENT 424, progfx 420; not the pulsing 401/404) on
  it and re-applies it when the light changes so the new value is set. The
  first 40 requests are logged (caller, asked, set) - unverified in play: what
  alpha 420 and the game's own stealth look ask for, and whether anything else
  see-through nearby is caught while hiding. stealth_ghost.
  0.33.1: it was - the 0.33.0 log shows creatures fading in (alpha 0) and
  other models (0.8, 0.26) all asking through the same model-level call, so a
  hider's opacity would have reached them. Now the client routine that picks
  an object's see-through alpha from its visual effects (rva 0x763420 in
  89.8193.37: the effect's +0x100, -1 when none; found by its entry bytes and
  checked) is hooked instead, and only an object registered as hiding (by id,
  client object +0x8) with a see-through effect on gets its light's opacity;
  see-through objects left alone are logged while someone hides (12 at most).
  Per id, so hiding henchmen can have their own. Stealth toggled while moving
  (owner: "there's still an issue with the animations and the lean the
  character has when entering or leaving stealth mode while walking"): the
  idle restart (0.31.1) now needs a still creature (AnimationStationary) -
  keyboard movement has no action, so it played pause2 for half a second
  under a moving character; the walk restart (0.32.1) waits up to 1.5 s for
  the walk (stealth turns a run into a walk on a later update). The look log
  names the animation playing at each swap.
  Outline research (owner, 2026-10-25: "there might be an issue with the
  transparency levels where the player might not see their own character in
  high levels of stealth. Is it possible to add any kind of outline to
  characters?"), not built: EE's mouse-over / Tab highlight is a glow drawn
  round a model's silhouette. The scene keeps a list of highlighted models and
  their colours (Scene +0x80 models, count +0x88; +0x90 colours, count +0x98;
  exported virtuals Scene::AddHighlightGob(model, colour) slot 35 - updates the
  colour or appends - RemoveHighlightGob slot 36, DoHighlightGob slot 54; global
  look: AurSetHighlightGobSize / Intensity / Frequency / Amplitude). A client
  object's animation object (+0x78) has 5 colour slots (flags +0x9c, colours
  +0x60 + 12 x slot, highlight flags +0xb0): 0x965cf0 sets one (colour, slot,
  highlight; the highlight is kept only when a global - likely
  graphics.hilite.glow - is on), 0x968590 clears one, and 0x966170 applies the
  first active slot: SetColorShifting with its colour (white = no tint) and, if
  it highlights, scene (client app -> area +0xf8 -> +0x248) AddHighlightGob.
  The game uses all 5 slots (4: a creature routine, 3: no highlight, others
  through CNWCObject vtable slot 43 / 0x801e00), so an outline for a hider
  would add the model to the scene list directly and put it back when the
  mouse-over code removes it.
  0.34.0 (owner: "go with A"): built so. stealth_ghost.cpp hooks the client
  creature's per-frame update (CNWCCreature vtable slot 29, rva 0x7fbd10, entry
  bytes checked); after it, a creature registered as hiding has its model
  (+0x78 -> +0xc8) put on the scene's list (client app +0x8 -> +0x30 -> area
  +0xf8 -> scene +0x248) in pale white-cyan (0.80, 0.95, 1.0) whenever it isn't
  on it - so a hover's own colour shows while the pointer is on it, then ours
  comes back - and taken off when it stops hiding; models and scenes seen
  before are only compared (a new area forgets them). The scene's slots 35 / 36
  are checked against the exports first. Unverified in play: how the glow
  looks on a see-through model, whether it pulses, and whether the client
  object id matches the server's (0x7FFFFFFF) - "stealth outline: 0x... outlined
  while it hides" in the log says it did. `stealth_outline`.
  0.35.0 (owner: "go with all your recommendations for henchman and rogue
  questions" - and "right now my character, in light, is very translucent when
  it should be at 90% visible"): the log had no "drawn" line at all - 0.33.1's
  routine (0x763420) isn't on the ghost effect's path, so the PC showed 424's
  own alpha - and no "outlined" line: the client knows an object by the
  server's id | 0x80000000 (WriteOBJECTIDServer; 0x7FFFFFFF is 0xFFFFFFFF
  there), so 0.34.0 never matched the PC. Now: client ids have bit 31 cleared
  before the match; Gob::SetColorShifting is hooked again, but only a hider's
  own model gets its opacity; and each frame a hider's model alpha (+0x108) is
  set through its animation object's slot 29 (0x966a40, entry bytes checked)
  when it isn't the opacity - back to solid when it stops, if nothing else
  changed it. Logged: the client object matched, the first caught request
  (caller), "drawn N% visible", and on stopping how often the alpha was set.
  The decisions, as recommended:
  HS1 henchmen follow into stealth mode (not Stand your ground - bit
  0x08000000 of NW_ASSOCIATE_MASTER; not down, dead, fighting, elsewhere or
  found since), out with the player; a stand-ground one in stealth mode is
  taken out in the first 2 s (the SoU / HotU scripts copy the master's mode).
  `henchmen_follow_stealth`. HS2 A: a henchman found leaves stealth on its own,
  the player told, and stays out until the player hides again
  (nwn5e_found_out). HS3: a hiding henchman gets its own light level, ghost
  opacity and outline. HS4: the cones stay coloured for the player.
  RB1: opacity = the light's step less 2% per point of Stealth bonus beyond
  ability + proficiency (Expertise's second PB, item and spell bonuses), 30% at
  least (rules::StealthOpacity). RB2: Reliable Talent on the Stealth d20 (rogue
  7+, proficient: 9 or lower is 10; class_features faithful;
  rules::ReliableTalentD20) - the rest of R9 (other skill checks) still with
  the Rogue work. RB3: SpotMargin has no size term (the check cancels the
  engine's; 0.34.0 was 4-8 off against smaller / larger watchers).
  0.35.1 (owner: "the cyan glow is too strong and should only appear when
  opacity is over 45%" - "when visibility is under 55% ... so in darkness,
  visibility is set to 50%, the glow should appear"): the outline only while a
  hider is drawn under 55% visible, at a third of the colour (0.27, 0.32,
  0.34); off again (logged) when it is drawn 55%+. In play 0.35.0 drew 90% in
  light and outlined as built (client object FFFFFFFF matched), but no
  henchman followed and nothing said why: the follow pass now logs, when it
  changes, each henchman's reason (stands its ground, hiding, found, dead,
  down, fighting, elsewhere, follows, or the game refused).
  0.36.0 lights (owner: "putting off a light source is a good one" - L1:
  attacking, interacting directly, "or via another spell like mage hand or
  prestidigitation"; L2: "would it be possible eventually to add an
  interactible element where wall torches exist?"; L3 / M1-M3 as recommended;
  then light-giving gear: "the brighter the weapon & equipment the greater
  penalty"). light_sources.cpp: a hook on CVirtualMachine::RunScript runs
  nwn5e's code for the event script names n5_lamp_use / n5_lamp_hit /
  n5_lamp_spl (every event script goes through it: RunEventScript ->
  RunScript(name, self, 1, type x 1000 + event)). On entering an area each lit,
  non-static, inventory-less light placeable gets them where its own are empty
  (on used - also replacing the stock switches nw_02_onoff, which never turned
  the light off, and x2_am_lampuse - and made usable; on damaged = a hit; on
  spell cast at). Static placeables (stock blueprints without a Static field
  load so) are scenery the client can't click and SetUseableFlag refuses:
  skipped. Used toggles; a hit (non-fire) or Ray of Frost / Ice Dagger / Gust
  of Wind / Cone of Cold / Ice Storm puts it out (damage mended); the Light
  spell relights. Out = deactivate animation, illumination off,
  RecomputeStaticLighting. L3: hostiles within 20 m with a line to it come to
  look (2024, heard_investigates). Mage Hand / Prestidigitation: with the
  spells (not NWN spells). L2: feasible later - tile source lights are dummy
  nodes <tile>sl1/sl2 in the tile models, and the client spawns fx_flame01
  there only while the colour isn't 0, so SetTileSourceLightColor 0 removes
  flame and light; a clickable stand-in can sit at the node (GetTileID -> .set
  model -> node position, rotated by GetTileOrientation).
  Darker maps: each area once a session - sun / moon ambient and diffuse at
  map_light % (70) of the originals kept on the area, tiles' main lights one
  step darker (lightcolor.2da steps are ~x0.5; never to black), recompute;
  `!maplight <10-100>` re-applies here and saves it. M3: tile torches in the
  light rule by grid coordinate (its tile bright, the 8 round dim) - 0.32.0
  read them by metres and got -1; `!light` likewise. Carried light: by reach
  (5/10/15/20 m) a watcher spots the hider at +2/+4/+6/+8 and it stands in dim
  (5 m) or bright (10 m+) light (rules::CarriedLightLevel / SpotBonus), told
  when it hides with one; also in the wedge colour.
  0.36.1: in play every light was static scenery ("0 light sources here can
  be put out (12 static)"): LoadPlaceable hooked - a lit static placeable
  without inventory has Static (+0x5a0) cleared before the area is sent; stays
  plot (OnApplyDamage posts OnDamaged before its plot test). Then 12 lights
  clickable, one used 3 times.
  0.37.0 (owner: "there are smaller torches that should also be interactive" -
  a room lit only by small candles still read "in light" - "the radial menu
  on light sources should also have an option beyond 'bash' ... 'snuff' ...
  once that's done they should stop attacking"): tile flames - per lit tile
  source light, its node <model>sl1/sl2 read from the tile model (.set
  [TILE<id>] Model=; data::ReadModelNodePosition / TilesetTileModel /
  TilesetTransition / TilePointInArea, tested), placed in the area, and a
  plc_invisobj stand-in (tag nwn5e_flame_x_y_slot, name "Flame") put there,
  made a normal usable plot placeable as it loads (LoadPlaceable hook while
  nwn5e creates them). Use / hit / cold-wind spell / Light toggle that slot
  (colour 0, the other kept) + RecomputeStaticLighting; doused ones put out
  again on entry. fx_flame01's light is 7 m: the light rule reads the stand-ins
  - a bright colour (odd) bright within 4 m and dim within 7.5 m, a pale dark
  one (even: half as bright) dim within 4 m (replaces the whole-tile rule);
  their zone rings drawn on the floor below. Bash is snuff: a hit that puts a
  light out stops the attacker; an attack on one already out stops it (on
  melee attacked). A real "Snuff" radial entry needs the client radial builder
  (0x9b19d0; fixed entries) patched - not done.
  0.37.1: in play "0 tile flames ... 200 lit slots with no flame node": the
  new tile getters (GetTileID / Orientation / Height) go through
  CNWSArea::GetTile(Vector), by metres, while the light getters take the grid
  coordinate - asked at the tile's centre in metres now (37 flames placed, 2
  used). Skipped lights logged with why. 0.37.2 (owner: a light circle from the
  next room showed through the wall and standing on it read dim): a light
  counts, and shows its zone ring, only with LineOfSightVector from half a
  metre out of it to the hider's chest. 0.37.3 (owner: a shadow stayed in a
  corridor with every light out - the tile's main light 4/0 over the door;
  "let's try B"): a tile's main lights go off when every light on it and the 8
  round it is out (data::TileFillLightsOff), back when one is relit; one
  recompute per change; kept dark through the map darkening, re-applied on
  entry (area locals nwn5e_fill_x_y). fill_follows_lights. 0.37.4 (owner: a
  torch held sideways spews flames sideways): it_torch_000 / 001's Flame02 -
  velocity 0.15 along the torch, inherit, no mass - patched at start to the
  smoke's way (velocity 0, mass -0.1, inherit off; data::PatchEmitter) and
  served (BuildServedTables, before registration): built on the player's
  machine from the game's own models. torch_flame_up.
  0.37.5: the torch flame's particles live 0.5 s (game 2 s), rise at mass -0.14
  (torch_flame_life_ms / torch_flame_rise, read at start). 0.37.6 (owner: more
  invisible lights casting shadows in corridors - "Let's try A"): indoors a
  tile's fill lights only while a lit light is on it or the 8 round it. 0.37.7
  (owner: "I still see the shadow from the invisible tile lights" - those next
  to lit torches - "let's try A+"): indoors and underground no fill lights at
  all; light and shadows only from visible sources. Accepted (owner,
  2026-10-25: "I think this is good actually, let's keep it for now").
  Torch flame accepted (owner, 2026-10-25: "the torch flame looks fine").
  Reliable Talent (RB2, 0.35.0) to be tested later, maybe during the rogue pass
  (owner, 2026-10-25). 0.38.0 (owner: "we can also try the light reach by
  brightness"): a light placeable's reach scales with its light colour's
  strongest channel (placeables.2da LightColor -> lightcolor.2da; brazier 1.9 ->
  4 / 9 m, candle 0.94 -> 2 / 4.5 m; rules::LightReachByIntensity, tested),
  kept on it at load (LoadPlaceable hook: nwn5e_light_bright / _dim / _rings;
  -1 for a "lit" one whose appearance gives no light); the light rule and its
  zone rings read them.
  Radial menu recon (owner, 2026-10-25: "send an agent to recon the radial
  menu for 'snuff' and 'light', this might also open future avenues for other
  interactions using the radial menu"), static analysis of 89.8193.37, not built:
  CRadialMenu is exported (guimenu.cpp): 8 slots a ring; SetEntryx(slot, icon
  resref, strref, handler, callback, int64 param) 0x1fa3a0 (label through
  CTlkTable::FetchInternal - nwn5e's 16,000,000+ strings work), a text variant
  0x1fa250, DisableEntry 0x1f8b30, SetSubMenuCreatorx 0x1faa40, Activate
  0x1f7e80; HandleMenuAction 0x1f9350 calls [m+0x140+s*16](handler
  [+0x138+s*16], param [+0x1b8+s*8]); a free slot's callback is
  HandleUninitializedCallback, its icon (+0x6e0+s*8) null. One builder class a
  kind (RTTI): CGuiPlaceableMenu (CNWCPlaceable vtable 41 0x8703d0 -> ctor
  0x9b1950, target client id at +0x7f8 -> builder 0x9b19d0), CGuiDoorMenu,
  CGuiCreatureMenu (also self; ~20 submenus), CGuiItemMenu,
  CGuiQuickbarMenu, CClientSpellMenu / CClientTrapMenu. A placeable's ring
  (clockwise from N): 0 Examine, 1 Cast submenu, 2 Use, 3 Trap submenu, 4 FREE,
  5 Bash (player creature [+0x37C] & 0x80: "may attack"), 6 Lock / Unlock, 7 DM
  lock (DM only). Choices send Input_* messages (Bash Input_Attack 6/2, Use
  Input_UseObject 6/0xB ...). A custom choice's transport: GuiEvent_Notify
  (0x35/1; client sender 0x7bdf30, found by the bytes 41 B0 01 4C 8B 4C 24 38
  B2 35 at 0x7be026; pass a zero Vector) -> CNWSMessage::
  HandlePlayerToServerGuiEvent (exported 0x451b30: type, int, object stored on
  the module, RunEventScript(19) OnPlayerGuiEvent, the type not checked) -> a
  shared CNWSModule::RunEventScript dispatcher (rules_dying.cpp hooks it only
  for dying faithful today). Per object: menu+0x7f8 & 0x7FFFFFFF = the server
  id (single player: read the server placeable - +0x594 lit, nwn5e's locals);
  on a remote server CNWCPlaceable+0x1f0 non-null while lit. Icons: 32x32 TGAs
  (aurora_gui.bif), loaded by resref - a served ir_snuff.tga would do; stock
  is_darkness / is_light. Plan: hook CRadialMenu::Activate; when the menu is
  a CGuiPlaceableMenu on a light, slot 4 = Snuff (lit; a copy of slot 5's
  Bash callback, only if slot 5 is set) or Light (unlit; a copy of slot 2's
  Use) - no server change, light_sources.cpp does the rest; later a GuiEvent
  transport + registry (AddRadialAction) for other interactions - also what
  Barbarian 3's X8 radial options would use. Dumps: the job's
  tmp/recon_radial (NOTES.txt).
  0.39.0-0.39.6 (owner, 2026-10-25/26): R1 built (radial Snuff / Light, R2
  saved for Barbarian 3). Snuff while hidden keeps the hide (a 30 s mark;
  the SetStealthMode hook refuses only engine calls, never ToggleMode, and
  sets the stealth activity bit +0x8e0 back - SetActivity clears it first,
  which left the stealth button stuck in 0.39.2). Snuff with a ranged weapon
  shoots from where the hider stands, 20 m at most, d20 + Dex + PB vs DC 13
  (18 beyond 10 m); a miss is heard ("the clatter"). Stance after toggling
  stealth while moving with a weapon: the client stops a walk's arm layer
  (walk_swordr ...) by its name through the swap list, so a layer swapped
  while it played kept playing. 0.39.5 changed the arm layers only once the
  creature stood still (fixed the stop pose); 0.39.6 (owner: "during the
  original move, after toggling stealth on or off the torso lean is wrong")
  holds a moving creature at the stand for 150 ms with the old layers, then
  swaps them and lets the walk go (the log counts the hold's updates).
  Backward-walk stutter while crouched (S key; click-to-move backwards is
  smooth): ours, not the game's - 0.39.6's note had misread the owner. Checked
  2026-10-26 (owner: "Check if our alterations really didn't touch the
  original code"): an unmodded launch gets nothing of nwn5e (no NWN5E key
  table, override / development / hak empty, the localvault characters and
  the newest save hold no animation swaps - only save 000023 does), but with
  nwn5e a hider's walk swap (walk -> n5_crwalk) covers walking backwards too
  (the game has no walk-back clip: animation 3 plays "walk"). 0.39.7 adds
  `!animtrace [seconds]` (anim_trace.cpp): what the character's models are
  asked to play (Gob::PlayAnimation) and its server animation, logged at the
  end - to see what the stutter is before fixing it. First traces (hidden,
  S held): the client plays its own guess - n5_crwalk at speed -1 (walking
  back) - while the server's animation flips between run (4) and stand (0)
  about every 20 ms (never walk-back, 3), and each flip reaches the client:
  run (upright, not swapped) and n5_cridle between the steps - the stutter.
  Each keyboard-movement message makes the server SetCombatMode(0),
  ClearAllSpecialAttacks, ClearAllActions, ActionManager(8) and
  AddDriveAction (CNWSMessage::HandlePlayerToServerInputDriveControl);
  AIActionDrive moves by the keys' bits (1 forward, 2 back, 3 run) and
  AddDriveAction drops bit 2 while +0x76c (limited movement speed) is set.
  0.39.8: the trace also records each SetAnimation on the character with
  its caller and each drive action's bits and +0x76c; the timeline goes to
  animtrace.txt next to nwn5e.log (no line cap), a summary per hiding
  state to nwn5e.log. 0.39.8's trace (hidden S, hidden click, upright S,
  hidden W): the keyboard movement's AIActionDrive sets run (4, +0x15b2) for
  a backward step - hidden or not - and stand (0, +0x1664) after it; forward
  while hidden it sets walk (2). The client plays each: upright the churn
  between walk-back, run and pause1 doesn't show (one upright pose), but a
  hider's run wasn't swapped - its root model spent 20% of the time in the
  upright run (+10% in cwalkf, the turn while moving), 18% in n5_cridle,
  49% in n5_crwalk backwards. Click-to-move backwards: 99% n5_crwalk. The
  limited-speed flag (+0x76c) was 1 throughout, hidden or not - not it.
  0.39.9: while crouching, run and run_<hold> are swapped to the crouch
  walk too (a hider never runs). 0.39.10 (owner: "walking back while
  crouched or standing still has issues ... the character doesn't seem to be
  able to move much it just stutters the position"; and the wrong lean when
  hiding / unhiding "while walking backwards"): the run (4) came from
  AIActionDrive finding no key - AddDriveAction clears the keys' 2 (backward)
  for a walk-limited creature (+0x76c, OnApplyLimitMovementSpeed - applied by
  ComputeModifiedMovementRate for any ruleset MOVEMENT_STAGE_PENALTY: stealth
  mode, detect mode, heavy encumbrance), so the server never stepped it back
  while its client did: position pulled back, run animation - the unmodded
  game's stealth too (AIActionDrive itself only turns a limited run into a
  walk). drive_fix.cpp: backward-only keys keep the key (drive_backward_fix);
  each change of a player's walk limit is logged with detect / stealth /
  encumbrance (the trace had it on while upright too - cause to read in the
  log). stance_look: moving = a moving animation or a position change in
  the last 500 ms; the toggle's stand hold starts on the first walking update.
  0.39.10's log: the walk limit was on while upright because the character
  is heavily encumbered (+0x970 = 1 throughout, Str 16) - by NWN's
  encumbrance.2da it can never run; 2024 carrying capacity is Str x 15 (240
  lb). Open decision for the owner (faithful: the 2024 capacity; NWN mode:
  NWN's table). 0.39.11 (owner: "walking backwards while turning still
  triggers the torso lean and walking motion when stopped issue
  sometimes"): the client starts a weapon's walk / run arm layer with
  Gob::PlayAnimation(layer, 1, 8, 0) and stops it (layer, 1, 128, x) when the
  walk ends; the turn clips (pauseturn, cwalkf) between the walk and the idle
  can lose the stop. anim_layers.cpp (stuck_layer_fix): a model whose base
  has been an idle for 300 ms with such a layer still started gets the stop;
  logged. Gob::PlayAnimation is hooked there now (!animtrace records
  through it).
  0.40.0 (owner, 2026-10-26: "use the 2024 encumberance"): `carrying`
  faithful serves SRD 5.2's carrying capacity (Rules Glossary): NWN's
  encumbrance.2da (Strength rows, tenths of a pound; over Normal heavily
  encumbered - walks, can't run; over Heavy overloaded) gets Normal = Heavy
  = Str x 150 (x 15 lb.; tools/data/carrying_capacity.py), so anything over
  the capacity is overloaded, and ruleset MOVEMENT_STAGE_PENALTY_ENCUMBRANCE_
  OVERLOADED 2 -> 4: ComputeModifiedMovementRate slows by 100 - 100 /
  2^(stages - 1)% at most to x 0.125 - about 4 ft of 30 (2024: at most 5
  ft). No Goliath in nwn5e (Powerful Build: one size larger). NWN mode:
  NWN's table. Open: the torso stutter while crouch-walking (owner: forward
  mostly the arms, backward the whole torso) - the crouch arm layers key
  the torso with a lean computed against a pelvis at rest, while the walk
  under them bends and sways the pelvis, and the engine plays the layer
  forward at speed 1 even while the walk runs backward at -0.96; a trace of
  the current build asked for before changing the layers.
  0.40.2 (owner: "the arm stuttering is very noticeable when walking ...
  encumbered and in stealth mode"; CA1-3 approved): traces with call
  stacks - an overloaded crawl (walk played at +-0.24) makes the client
  restart the walk every frame (vanilla too: the upright crawl did ~50 / s,
  unseen), and each walk start runs 0x961080 twice: the walk arm layers on,
  then the run arm layers off - by name through the swap list; 0.39.9 had
  swapped run_<hold> to the crouch walk's layer too, so turning the run
  layers off stopped the walk layer just started (once per walk at normal
  speed - no weapon arms on the crouch walk; ~60 / s at the crawl). The run
  layers are never swapped now (removed, also from saves); the base run
  still is. Likely the earlier torso / arm stutter while crouch-walking too.
  0.40.3 / 0.40.4 (owner: "when turning off stealth mode it takes a while
  for the light markers, vision cones and hearing markers to disappear"):
  0.40.3 handled a hiding change at once instead of at the once-a-second
  check ("behaviour is unchanged" - the server cleared them 15-50 ms after
  the toggle already). The wait was the client's: a removed duration
  effect plays its model's cessation and is kept for its length + 3 s
  (0x97162b: + 0xbb8 ms) on the cessation's last frame - the game's own
  effects fade there; nwn5e's rings, cones and light zone rings keyed
  nothing in cessation, so they stayed drawn ~3 s (and a ring or cone that
  changed size or colour overlapped its old self for 3 s). 0.40.4:
  tools/data/stealth_rings.py's cessation fades the band out over 0.1 s
  (alphakey) and shrinks it away at the end (scalekey); the eye / ear
  markers already shrank to nothing in theirs.
  *Tested in game (owner, 2026-10-26: "everything seems good, untested: RB1,
  GL3 and a few others but I think most are good"):* the 0.33-0.40
  backlog - visibility (GH1 dim / dark, GH2, GL1, GL2, LS8), enemies (RB3,
  LS5), henchmen (HS1-4, HS2-1, HS2-2, HS3-1, HS3-2, HS4), lights (LR1, LR2,
  FL3-FL7, LS4, LW1-LW3, RS3, RS4), MK1-MK4 and the earlier CA1-CA3, ST,
  SR, animation sets (WT / PA / LN / BW / DB / SL / AS). Left for the rogue
  pass: RB1 (Expertise more see-through), GL3 (the glow for such a rogue
  in the dark) and RB2 (Reliable Talent). CA4 / LN9 optional, not reported.
  0.42.0-0.44.0: the VFX Bench (a tools web app, then tools/vfx_bench, now in nwn5e-tools) - its
  notes moved to docs/TOOLS-PLAN.md, "VFX Bench" (2026-10-30).
*Accepted (owner, 2026-10-24: "Accept 5b recommendations"), 0.20.2:*
1. Henchmen don't gain Exhaustion (as built). 2. A respawn clears it (as
built). 3. Death magic from an enemy (EffectDeath with an enemy creator:
Finger of Death, Wail of the Banshee, Power Word Kill...) lays a player's
henchman down at 1 HP like damage - OnApplyDeath hooked, taking the engine's
own immortal path; any other death (a script's, a trap's) still kills, so
story deaths stay. 4. A down henchman is ethereal (enemies don't see it and
turn on the party), removed when it gets up or is summoned. 5. A down player
character is Unconscious for the conditions (ReadConditions: asleep):
advantage against it, and a hit from within 5 ft is a critical hit - two
failed death saves. 6. No floating text or tracker window: the marker and the
chat lines are the indicator.
*0.22.9 (owner's test: "the henchman keeps getting up and taking a potion
when he's supposed to be dead"):* the henchman AI scripts make it commandable
again (nw_ch_ac5 when attacked, nw_ch_acb on a harmful spell, nw_ch_ac4 at its
master's shouts) and its combat round drank a potion - healed above 1 HP, up.
Now CNWSObject::AddAction / AddActionToFront (every action's way into the
queue) are hooked: a down henchman's actions are refused, except while
nwn5e's own scripts run for it (down, held down, up, summoned); and each pass
a down henchman not in a lying pose (animation 5-8: its AI cleared the dead
animation) is laid down again, quietly, at most once a second.
*0.22.10 (owner: "healing a henchman who goes down during a fight doesn't
cause him to get up"):* the heal was seen (the log's "up at 23 HP") but he
stayed lying - OnApplyDamage set the lying posture (+0x6B4 = 3 / 4) when the
blow took him below 1 HP, and only ClearAction / RecomputeAmbientAnimationState
write standing (1). Getting up (healed, or summoned) now sets the standing
posture and the stand animation (SetAnimation 0).
*Tested in game (owner, 2026-10-24):* the death-save visuals (V1-V6), the
Exhaustion icon (E5, E6), the `!vfx` search (S2), death magic, the unseen and
the healed downed henchman (D1-D3), the henchmen's Short Rest and its button
(H8, H9) all pass; the low-HP bleeding settled as the puddle alone (0.22.8).

## Phase 2 — skills and checks: decided by the owner (2026-10-19)

**Decision (owner, 2026-10-19): the recommendations below, as revised.**
SK3: option (a) (script skill values scaled to the 3e range in faithful; (b)
if the project moves to EE). SK6: option A (Disarm does nothing in faithful).
SK5's open choice (no recommendation was given) is taken as the 2024 rule
in faithful: the hider rolls Stealth once per hide, not every 6 s. Deferred:
Craft Trap (Phase 3), item use limitations without Use Magic Device (Phase 8).
*Built 0.9.23 (step 1: SK1, SK2, SK3 a):* GetSkillRank hook, `!skills`,
proficiency kept in local variables (taken from ranks until chosen), 2024
names through the label hook, script skill ranks scaled (2 x). Next: SK3
fixed-DC conversion, then SK10, SK6 / SK7, SK5, SK9, SK8 / SK11.

Saves (C1–C8) and spell DCs (C5) are done; what is left of Phase 2 is skills,
ability checks and spell attacks. Engine facts: docs/PHASE2-SKILLS-RECON.md.
Key fact: one function, **GetSkillRank (0x6A2C00)**, builds every skill total
the engine and the scripts use — one hook converts them all; each use then
rolls its own d20 (no shared roll), and **no skill check has a natural 1/20
rule** (already 2024). Each item: faithful (2024) and NWN mode;
recommendation first. "2024" means SRD 5.2 (the project's rule source);
Player's Handbook-only material is named as such.

*Revised 2026-10-19 after an owner-requested consistency review (marked
"rev.")*: one point applies to every item — **data files (2da, dialog.tlk)
can't be switched per mode**, so anything faithful-only is done in hooks (as
C8 relabels the saves), never by editing a shared table.

**SK1. The skill list.** NWN 1.69 has 28 skills (27 + Ride); 2024 has 18 plus
tool proficiencies. Recommendation (faithful): keep NWN's rows (the engine
and the official campaigns use them) but make them the 2024 skills —
Hide + Move Silently = **Stealth**; Spot + Listen + Search = **Perception**
(rev.: 2024 finds hidden things, traps included, with the Search action —
Wisdom (Perception); Investigation is the Study action); one proficiency, all
its rows get it; Lore and Spellcraft → Arcana; Heal → Medicine; Pick Pocket →
Sleight of Hand; Tumble → Acrobatics; Persuade → Persuasion; Bluff →
Deception; Intimidate → Intimidation; Perform → Performance; Animal Empathy
and Ride → Animal Handling; Open Lock, Disable Trap, Set Trap → **Thieves'
Tools**; Craft Trap → (2024 has no trap crafting; Tinker's Tools at most —
decide with crafting in Phase 3); Craft Armor and Craft Weapon → **Smith's
Tools**. Names changed in faithful through the label hook (rev.: not the tlk,
which would rename them in NWN mode too). Concentration, Discipline, Parry,
Taunt, Appraise and Use Magic Device have no 2024 counterpart (SK6–SK8).
2024's Athletics, History, Insight, Investigation, Nature, Religion and
Survival have no NWN use (nothing in the engine or campaign scripts checks
them); add them only when content needs them (adding skills.2da rows needs RE
first: the per-creature rank arrays may be fixed-size). NWN mode: NWN's 28
skills. Consequences: faithful's sheet lists fewer, merged skills; nothing in
the campaign loses a check.

**SK2. Proficiency instead of ranks.** NWN: ranks bought with skill points
each level (up to level + 3), plus Skill Focus (+3), Epic Skill Focus (+10),
racial affinities (+2), Alertness (+2), armour check penalty, −4 to Str / Dex
skills when blind, item and spell bonuses — totals reach +40. 2024: ability
modifier + proficiency bonus if proficient (+PB again with Expertise); a few
items. Recommendation (faithful): the GetSkillRank hook returns ability
modifier + PB (proficient) + PB (Expertise) + item and spell effects (kept, as
for saves in C7); feat and racial bonuses dropped; ranks ignored (rev.: the
skill points stay in classes.2da for NWN mode; Phase 7 hides the step in
faithful); armour check penalty becomes 2024's **disadvantage on Stealth** in
the armours that have it (2024 armour table); blindness (rev.): NWN's −4 to
Str / Dex skills dropped, checks that need sight (Spot, Search) fail. Proficiency
comes from class choices at level 1 (2024 counts: most classes 2, Bard /
Ranger 3, Rogue 4), background (2), species (e.g. Elf: Insight, Perception or
Survival; Human: one skill) and the Skilled feat; Expertise from Rogue 1 / 6,
Bard 2 / 9, Ranger 2 / 9 and **Wizard 2 (Scholar)** (rev.: was missing). Until
Phase 7 builds the level-up screens they are chosen with a chat command
(`!skills`, like `!mastery`) and kept as local variables; existing characters
pick theirs with `!skills`, **up to their 2024 count** (rev.: not "every skill
they have ranks in", which could give a rogue 15). NWN mode: NWN's ranks.
Consequences: faithful skill totals drop from up to +40 to at most +17
(Expertise) — which is why SK3 is needed; Skill Focus and similar feats stop
doing anything in faithful (Phase 3 retires them).

**SK3. DCs on the 2024 scale.** NWN's fixed DCs (locks, traps, Search,
scripted checks) are set for 3e totals; 2024 DCs run 5–30. Recommendation
(faithful): convert fixed DCs where the engine and GetIsSkillSuccessful compare
them, as C6 converts save DCs — **DC 10 + (3e DC − 10) / 2**, rounded (3e 20 →
15, 30 → 20, 40 → 25, 50 → 30); opposed checks (Hide / Move Silently against
Spot / Listen, Pick Pocket's notice roll) need no conversion (both sides
change) (rev.: Concentration and Taunt checks are removed in faithful by SK7 /
SK8, so they are not converted). Identification (Lore → Arcana) compares the
2024 value with SkillVsItemCost.2da's rank thresholds; in faithful the hook
converts at that comparison (rev.: the table is shared, so it isn't edited; Use
Magic Device's use of it goes with SK8). **The campaign's AutoDC (rev.):** 59
compiled scripts — including the Persuade checks in about 300 conversations —
do their own `GetSkillRank + d20() >= DC` with DCs from the NPC's level; our
DC conversion can't reach them, and with 2024 totals most would fail. Options:
(a) when a *script* asks for a skill rank (command 315), faithful returns the
2024 total scaled to the 3e range (about 2 × the bonus), so conversation
checks stay as passable as in NWN — 1.69-only hook; (b) recompile those
scripts with 2024 DCs — most faithful, ports to EE, heavy; (c) accept harder
conversation checks. Recommendation: (a) now, (b) if the project moves to EE.
NWN mode: NWN's DCs. Consequences: locks and traps stay openable by trained
characters; untrained ones fail much more often (2024's intent).
*Built 0.9.30 (with `skills`):* the engine's fixed DCs converted where each
check loads them (before the combat log reads them): GetIsSkillSuccessful,
unlock / lock and the scripts' "can it be picked", Flag / Recover / Disarm /
Examine / Set Trap, Search, Heal kits (poison / disease DC), Pick Pocket (30
/ 20 -> 20 / 15), Animal Empathy, Tumble against attacks of opportunity (15 ->
13), Spellcraft's identify-spell checks and the Examine Trap message. A DC of
1 or less stays (NWN's "always works" floor) and nothing is clamped (a DC 100
lock stays out of reach) — unlike saves, whose C6 conversion keeps 10..30.
Lore identification maps the 2024 total to the SkillVsItemCost.2da row as
scripts do (2 x). A trap a character sets stores base + 2 x the setter's 2024
skill, so the later conversion lands right. Open (not changed; NWN's for
now): "fail by 10 sets the trap off" / "fail by 5" margins (Recover, Disarm,
Set Trap, Animal Empathy) are still 3e-sized; Disable Trap and Set Trap still
give each other NWN's +2 synergy (both are Thieves' Tools in 2024); traps of
DC > 35 still need a Rogue (3e Trapfinding); NWN's take 20 out of combat
(Open Lock, traps, Animal Empathy, Heal) stays — with 2024 totals it opens
most locks for anyone given time; and Search's d10 when not searching could
become passive Perception 10 as SK5 did for creatures.
*Decided (owner, 2026-10-19), the recommendations:* faithful drops take 20
(2024 has none; clicking again is the retry, traps keep their risk), the +2
Disable Trap / Set Trap synergy (both are Thieves' Tools) and the Rogue
requirement for traps above DC 35 (3e Trapfinding); Search uses passive
Perception 10 when not searching (as SK5); "fail by 10 sets the trap off"
stays. SK5 addition: hiding needs a Stealth total of 15 (the 2024 Hide action)
behind its own switch — below it the creature is not hidden.
*Built 0.9.31:* take 20 removed (both branches into the "20" path NOPed at
the seven trap / lock actions; Animal Empathy and Heal kits' combat test),
the synergy +2 skipped (5 sites), the DC > 35 Rogue test always passed (11
sites); Search's six dice redirected (d20 in detect mode, else 10);
`hiding_needs_15` (default on): the Stealth total (d20 + Hide) is told to the
player when hiding starts, and below 15 Spot / Listen read the hider as not
in stealth mode (noticed once in range) until it hides again.
*0.9.32 (owner test 2026-10-19):* a loaded game restores stealth mode while
the character is being placed (no area yet), when the skills hook still gives
NWN's value — the first roll used +2 instead of +4 and said "not hidden". No
roll then; the first 6 s update rolls. The perception log now shows each
check's score (> 0 = noticed) and whether the observer stands still (+5);
four goblins saw a Stealth 16 at melee range — to be read from the score.
*Decided (owner, 2026-10-19), after a Stealth 24 walked around goblins
unnoticed (scores -5 to -19: passive 10-13 never beats one high roll):* B —
2024 hides you only out of sight; NWN has no cover data, so an enemy that has
the hider in front of it within about 2 m (and can see it) finds it; behind
still works. C — alert enemies search: a creature in combat uses an active
d20 Perception each 6 s update instead of passive 10. Each behind its own
switch. *Built 0.9.33:* B (`found_up_close`) — when a Spot check gets as far
as its score (the observer can see: not blind, in range) but misses, a hider
within 2 m in front of the observer (ahead of its shoulders) with a clear line
(no wall faces or placeables, PlanPush's tests) is found; C
(`alert_creatures_search`) — +0x4BC set counts as searching for the 6 s roll.
*Owner test 2026-10-19 (0.9.35):* B verified (found at 1.8 m in front; 1.2 m
behind and 2.2 m in front not). NWN refuses stealth mode in combat, so C only
matters for a creature already hidden when others fight (a henchman engaging,
Hide in Plain Sight); not tested. SK3 (locks, traps) waits for an occasion.

**SK4. Natural 20 / 1 on checks.** NWN has none on skill checks; 2024 has
none on ability checks. Nothing to change (both modes).

**SK5. Stealth against Perception.** NWN: every 6 s each creature caches a
Hide d20, a Move Silently d20, and a Spot / Listen roll — d20 in detect mode,
**d10** otherwise — compared with distance, area, size (−8 tiny … +8 huge) and
"in combat −10" modifiers. 2024: the hider rolls Stealth once when it hides
(the Hide action) and stays hidden until found or it gives itself away;
others notice it with **passive Perception (10 + modifier)** or an active
Perception check (the Search action). Recommendation (faithful): observers
not in detect mode use 10 instead of a d10 roll (passive Perception), detect
mode keeps the d20 (searching); drop the size modifier (not 2024); keep
distance and area modifiers (needed for NWN's real-time perception; 2024 uses
light and cover instead); the hider's roll (rev.): keep NWN's re-roll every
6 s (simpler) or roll once per hide (2024) — decide; "in combat −10" (rev.):
not 2024 (you can hide in combat if obscured) — drop. NWN mode: NWN's.
Consequences: watchful guards become more consistent (no lucky d10 of 1);
small creatures lose their hiding bonus; hiding in combat gets easier.
*Built 0.9.29 (key `perception`):* Stealth rolled when hiding starts
(SetStealthMode hook; a creature loaded already hidden rolls at its first
6 s update) and kept until it stops; passive 10 unless searching. Found in
the decode and done in the same spirit: one d20 serves both NWN rows of a
2024 skill (Hide and Move Silently = one Stealth roll; Spot and Listen = one
Perception die); NWN's half skill for an observer moving without searching
goes with its d10 (passive Perception uses the full modifier); the Ranger's
Favored Enemy bonus to Spot / Listen is dropped (SK2 drops feat bonuses to
skills); a hider whose Stealth total is exactly 0 is no longer noticed at once
(a 3e "no ranks" rule that +0 2024 totals would hit). The detection log lines
(`stealth:` / `perception:`) are for testing.

**SK6. Knockdown and Disarm (Discipline).** NWN: after a Knockdown / Improved
Knockdown hit the defender (Large or smaller; one size more with Improved)
rolls d20 + Discipline against the attacker's attack total, and is knocked
down on a loss; Disarm works the same way. 2024: **Shove** (an Unarmed Strike
option) — **no attack roll** (rev.); the target makes a Strength or Dexterity
saving throw (its choice) against DC 8 + the attacker's Strength modifier +
PB, or is pushed 5 ft or knocked prone; the target can be at most one size
larger than the shover (rev.). **SRD 5.2 has no Disarm at all** (the only
disarming is the Battle Master's Disarming Attack, a Player's Handbook
subclass outside the SRD). Recommendation (faithful): using a Knockdown feat
is a Shove — no attack roll, the target's better of Strength or Dexterity
save against 8 + Strength modifier + PB, target at most one size larger
(rev.: the first draft kept the attack roll, which made it weaker than 2024);
**Disarm does nothing in faithful** (option A; B = keep it as a house rule
with a Strength save against the same DC; C, NWN's Discipline check, is not
possible once SK1 removes the skill) — a PC who took it gets the choice back
in Phase 3. NWN mode: Discipline for both. Consequences: knockdown lands more
often (no attack roll), but strong or nimble targets resist by their saves;
both Topple (Constitution save) and Shove (Strength or Dexterity save) are
2024 saves (rev.: not "one rule"); NPCs with Disarm stop disarming in
faithful.
*Built 0.9.40 (key `knockdown`):* a Knockdown / Improved Knockdown attack
becomes an "automatic hit" before ResolveAttackRoll (no roll), the special
path skips its two damage calls for it (no damage), and the handler's size
check + Discipline roll are replaced by the 2024 size limit (one larger, both
feats; NWN's Improved allowed two) and the target's better Strength /
Dexterity save (the engine's save roll, a forced Strength ability when that
is better, so the log names it) against 8 + Str + PB; the engine's own
knockdown effect (4.5 s) on a failure. Disarm is never added as a special
attack: an ordinary attack (no -6 / -4, no opportunity attack, no disarm).
Recon correction: NWN's Knockdown already limited size to one larger.
*Owner test 2026-10-19 (0.9.40):* the save worked (Strength save -3 vs DC
13, labelled; a failure knocked down, a success did not), but the attack roll
and damage stayed: the current-attack lookup passed the index as a byte, so
GetAttack read garbage and the attack never looked like a Knockdown. 0.9.41
passes it as an int (as rules_advantage does).

**SK7. Concentration (the skill).** NWN: a Concentration check on damage
while casting (DC from the damage), for defensive casting, when entangled (DC
15) and against Taunt. 2024: casting is not interrupted by damage; only
maintaining a concentration spell needs a Constitution save (Phase 4); no
check to cast while restrained; no defensive casting (casting draws no
opportunity attacks, D2). Recommendation (faithful): remove the damage and
entangled casting checks; defensive casting has nothing left to do; the
concentration save comes with Phase 4. NWN mode: NWN's. Consequences: casters
in melee are no longer interrupted — a big buff to casters in melee, as in
2024 (balanced in 2024 by concentration and by spells being actions).
*Built 0.9.39 (key `concentration`):* the OnApplyDamage check is skipped (the
path a non-casting creature takes), the defensive-casting and entangled checks
always pass; the d20s still roll. Taunt's opposed Concentration goes with SK8.

**SK8. The 3e-only skills.** Parry (parry mode), Taunt, Appraise, Use Magic
Device. 2024 has none of them as skills. Recommendation (faithful): **parry
mode does nothing** (rev.: the first draft pointed to the Defensive Duelist
feat, which is Player's Handbook-only, not SRD); Taunt is left out of faithful
(it stays in NWN mode); Appraise dropped (stores use list prices); Use Magic
Device dropped — in faithful, NWN's item use limitations (class, race,
alignment properties) simply apply, with no skill to get around them (rev.:
2024 items are mostly unrestricted except some attunements — decide with
items in Phase 8); the SRD Thief's **Use Magic Device at level 13** brings
the bypass back in Phase 3 (rev.: was missing); scrolls are Phase 4. NWN
mode: NWN's. These touch few campaign scripts (Appraise 4, UMD 1, Taunt 3,
Parry 4).
*Built 0.9.45 (key `nwn_only_skills`):* parry mode is cosmetic (no parry
rolls / ripostes, the creature schedules its attacks and makes AoOs — with
only the first, a parrying NPC would just stand there); Taunt plays its
animation and stops (no roll, effect, hostility or log line); stores record
every OpenStore markup / markdown as 0 (Appraise is script-only — nw_i0_plot's
opposed d10 roll — so this is the one place; quest discounts passed the same
way go too, and the script's "merchant's reaction" text still shows);
CanUseItemViaUMD returns 0 (class / race / alignment limits hold — 279 of 287
stock scrolls carry a class limit, so non-casters can't read them, as in
2024) and Holy Avenger gives a non-paladin +2. The scripts' AutoDC dialog
checks on these skills (x0_d2_skparr*, x0_d2_sktaun*, x2_appraise_*) still
read NWN's ranks.

**SK9. Initiative and ability checks.** NWN: initiative = d20 + Dex + 4
(Improved Initiative) + 8 (Epic Superior Initiative); no other raw ability
checks in the engine; dialogs gate on ability *scores* (about 1,000
Int-gated lines), not rolls. 2024: initiative is a Dexterity check (+PB with
the Alert feat, SRD); advantage / disadvantage apply. Recommendation
(faithful): initiative = d20 + Dex modifier, Improved Initiative → Alert
(+PB), Epic Superior Initiative → dropped until Phase 3 sorts epic feats;
leave the score-gated dialog lines alone. NWN mode: NWN's.
*Built 0.9.42 (key `initiative`):* GetDexMod in 0x655AF0 returns Dex (+ PB
with Improved Initiative) and the feat bonuses after it are skipped — which,
following the formula, also drops NWN's +2 for Blooded and Thug (background
feats with no 2024 counterpart); the d20 has advantage while the creature is
invisible (GetInvisible) or hidden (stealth mode with a hide of 15+, 2024's
Invisible condition), and Halfling Luck rerolls a natural 1.

**SK10. Spell attacks.** NWN: TouchAttackMelee / Ranged (in 59 spell
scripts) roll their own d20 with the melee / ranged attack bonus (today PB +
Str / Dex after attack_proficiency) against touch AC, with NWN's threat and
confirmation. 2024: spell attack = d20 + PB + spellcasting modifier against
AC; natural-20 crit, no confirmation; advantage and disadvantage apply.
Recommendation (faithful): the touch attack uses PB + the casting ability
(the same ability C5 uses for DCs), our roll mode (advantage / disadvantage,
conditions) and our crit determination (natural 20, no confirmation); the
crit's damage stays the spell script's (rev.: the scripts compute it, mostly
doubling everything — "dice only" comes with the Phase 4 spell rewrite);
against full AC (2024 has no touch AC). NWN mode: NWN's (with the global
attack conversion). Consequences: rays and touch spells hit by the caster's
casting ability; armoured targets become harder to hit with rays (full AC).
*Built 0.9.37 (key `spell_attacks`):* the handler 0x6F1170's bonus calls
return PB + the casting modifier of the slot / item the spell is cast from (as
C5 reads it; Charisma for innate abilities; a scroll's attack bonus, DC - 8,
for items) + the touch-attack effect bonuses (type 6, as the engine asks);
full AC; the d20 with the attack roll mode (situational terms, close combat,
conditions, Halfling Luck); a natural 20 crits without confirmation. Only a
creature caster on a creature (traps / placeables keep NWN's bare d20). In
1.69 the wizard rays (Ray of Frost, ...) make no touch attack at all; the
touch attacks are Ghoul / Vampiric Touch, Harm, Slay Living, Heal / Mass Heal
and Lay on Hands on undead, monster Bolt abilities, mephit breath, beholder
rays and Arcane Archer arrows (59 scripts).

**SK11. Conditions on checks** (completes CN4 / CN5 / CN8): poisoned and
frightened give disadvantage on ability checks; **deafened fails checks that
need hearing (Listen)** and blinded those that need sight (SK2) (rev.: was
missing); exhaustion later on d20 tests. Recommendation: all, once the check
rolls are hooked (rev.: the d20 sites of SK3 / SK5 / SK9, not SK2's rank
hook).
*Built 0.9.43 (with `conditions` faithful):* initiative (disadvantage cancels
the hidden / invisible advantage), perception (a searching observer's d20
with disadvantage, a passive one 10 - 5 = 5; a deafened one hears nothing, a
blinded one sees nothing), Search for traps (the same; blinded: the lowest
die) and the scripts' GetIsSkillSuccessful (disadvantage; Listen while
deafened, Spot / Search while blinded roll the lowest die — the d20 can't
force a failure, the rank still counts). Poisoned = an NWN poison effect;
frightened = nwn5e's fear with its source in sight. Not yet: the engine's own
actions (locks, traps, Heal kits, Animal Empathy, Tumble, Pick Pocket) — their
d20s are CRT rand calls without the creature at hand; a later pass.

Build order if accepted: SK2 + SK1 (GetSkillRank hook, `!skills`, proficiency
storage, labels) → SK3 (DC conversion at GetIsSkillSuccessful and the engine
sites, AutoDC option) → SK10 (touch attacks) → SK6 / SK7 → SK5 → SK9 → SK8 /
SK11. Rough size: SK2+SK1 two sessions, SK3 one and a half, SK10 one, SK6+SK7
one, SK5 one, SK9+SK8+SK11 one. Ports to EE: the rules (src/rules); the hooks
are 1.69-only.

## Conditions — decided by the owner (2026-10-19)

**Decision (owner, 2026-10-19): the recommendations below, as written**, with
CN8 revised after decoding the engine (RE-NOTES "Condition effects"):
Deafened leaves CN8 — faithful drops NWN's 20% spell failure (it only ever hit
Bard / Sorcerer / Wizard spells, and Still Spell avoided it), keeps "can't
hear"; NWN mode unchanged. Petrified stays NWN for now; resistance to all
damage is a later decision. CN7 is built with Phase 5. Build order: CN8
Deafened, then CN3/CN2 saves, CN2 crits, CN4, CN6, CN1, CN5 (by size and
risk, adjusted after recon).


The last open Phase 1 item. Phase 1b (B1) already turned NWN's attack terms
for a blind, stunned or prone target, an invisible attacker and an unseen
target into advantage/disadvantage. Everything else about conditions is still
NWN's. "NWN today" is from NWN's own rules; where marked *(confirm)* the
engine code is not decoded yet and is checked before building. Each item has
a faithful and an NWN-mode version; recommendation first.

*Built 0.9.11–0.9.12 (2026-10-19):* CN8 Deafened, CN1, CN2 (advantage both
modes — NWN gives paralysed / sleeping targets no attack bonus, only
flat-footed — saves, 5 ft crits), CN3, CN4, CN6. Recon corrections: Evard's
Black Tentacles applies paralysis (CN2), not entangle; NWN mode's CN2
"advantage only" is new to NWN mode for the same reason. Still open (owner):
CN5 (the generic monster AI refuses to fight while frightened — options in
RE-NOTES "Conditions: hook points"), NWN's coup de grace against sleeping /
paralysed creatures of level 4 or less (automatic hit and kill; 2024 has
none), and whether paralysed / sleeping targets keep NWN's flat-footed
Dexterity loss on top of advantage (they do for now).

*Decided (owner, 2026-10-19), after the recon:* **CN5** faithful replaces
NWN's fear with nwn5e's own: the frightened state never takes hold (so no
fleeing, no loss of control, no -2 saves, and NWN's monster AI keeps
fighting); the creature gets the fear icon for the effect's duration, from
its source, and attacks with disadvantage while it can see that source.
Remove Fear and similar cures don't end it early (known gap). NWN mode keeps
NWN's fear. **Coup de grace:** faithful drops it (the 5 ft critical hit
replaces it); NWN mode keeps it. **Flat-footed Dexterity loss** on paralysed
and sleeping targets stays in both modes until Sneak Attack is converted
(Phase 3). **Push:** built as option A of the recon (RE-NOTES "Push").
*Built 0.9.13:* CN5 (OnApplySetState hook), no coup de grace, Push.

**CN1. Restrained** (Entangle, Web, Evard's Black Tentacles). NWN today: held
in place, plus 3e's −2 to its attacks and −4 Dex *(confirm)*. 2024: speed 0;
attacks against it have advantage, its attacks disadvantage, disadvantage on
Dex saves. Recommendation: faithful = the 2024 effects, NWN's flat penalties
removed; NWN mode = NWN's. Consequences: faithful makes webs and tentacles
much more dangerous — everyone hits restrained creatures easily (both ways:
an entangled PC is easy prey) — NWN mode unchanged.

**CN2. Paralyzed, asleep, held (helpless).** NWN today: can't act; attackers
get a bonus *(confirm: +2 "stunned" term or more)*. 2024 (Paralyzed /
Unconscious): incapacitated; auto-fail Str and Dex saves; attacks against it
have advantage; a hit from within 5 ft is a critical hit. Recommendation:
faithful = advantage (already, if the engine marks it as stunned), plus
auto-fail Str/Dex saves and melee hits become crits; NWN mode = advantage
only. Consequences: faithful makes Hold Person / Sleep on a fighter close to
a death sentence (every melee hit doubles its dice) and the same for the
party's paralyzed foes — the 2024 power level of hold spells.

**CN3. Stunned.** NWN today: can't act, +2 for attackers (now advantage).
2024 adds: auto-fail Str and Dex saves. Recommendation: both modes add the
auto-fail (it is part of being stunned in either). Consequences: stunned
targets can't dodge fireballs; small change.

**CN4. Poisoned.** NWN today: poison is ability damage (poison.2da), with no
ongoing penalty. 2024: Poisoned = disadvantage on attack rolls and ability
checks (poisons also deal damage). Recommendation: faithful = while a poison
effect is on a creature, disadvantage on its attack rolls (ability checks
when Phase 2 adds them); NWN's ability damage stays until Phase 8 converts
the poisons; NWN mode = NWN's. Consequences: poison bites (spiders, wyverns)
matter during the fight, not only after; poisoned PCs miss more.

**CN5. Frightened.** NWN today: the creature flees and can't act — a
frightened PC loses control for the duration. 2024: disadvantage on attack
rolls and ability checks while the source is in sight, and it can't move
closer to the source; it still acts. Recommendation: faithful = no fleeing;
disadvantage on attacks while it can see the source (moving closer is not
enforced — NWN's AI and pathing can't easily express it); NWN mode = NWN's
fleeing. Consequences: faithful keeps you in control under fear but fear
spells (Scare, Fear, dragon fright) become weaker against monsters, which keep
attacking at disadvantage instead of running.

**CN6. Concealment and miss chances** (Invisibility, Blur, Displacement,
Darkness, blindness). NWN today: a percentage miss chance (20–50%) after the
attack hits. 2024 has no miss chance: an invisible or unseen target already
gives disadvantage (B1); Blur and Displacement give disadvantage on attacks
against you. Recommendation: faithful = no miss chance; concealment from
effects becomes disadvantage on attacks against that creature (one source,
not stacking with the unseen one); NWN mode = NWN's miss chance. Consequences:
faithful removes the "hit… but missed (50%)" double roll; invisibility gets a
little weaker (disadvantage ≈ −4 instead of halving the hits), Blur
Displacement about the same.

**CN7. Exhaustion.** NWN today: none. 2024: levels 1–6, −2 per level to every
d20 test, −5 ft speed per level, death at 6; a long rest removes one.
Recommendation: build it with the Phase 5 rest rework (2024 sources are
mostly forced marches, no rest, a few spells); nothing in the campaign applies
it yet. Both modes the same.

**CN8. Keep as NWN** (no change, both modes): Charmed and Dominated (NWN's
"becomes friendly / controlled" is close to 2024; on the player both are a
daze, by NWN's design), Petrified (same as paralysis plus the stone look; no
damage resistance — later decision), Dazed (= 2024 Incapacitated), Confused
(random AI), Grappled (no grapple in NWN; Phase 6). *Revised 2026-10-19:*
Deafened — faithful drops the 20% arcane spell failure; NWN mode keeps it.
*Revised (owner, 2026-11-01: "change the charm to 2024, not being able to attack the caster"; #106)* - after #104's
F4 ("Cannot activate rage in F4": a charmed player was NWN's daze). Faithful: a charmed player keeps control - no
NWN charm state, the charm icon credited to the charmer for the effect's time - and can't attack its charmer or
aim a harmful spell at it (the order refused with a word); immunities stay NWN's; Mindless Rage ends it. NPCs keep
NWN's charm (friendly to the charmer - close to 2024, and what their AI expects). The charmer's social Advantage
has no NWN use against a player.
*Built 0.59.7 (#106):* EffectCharmed is SetState 1 (CNWSCreature::RemoveCharmEffectsByOBJECTID checks 1 and 7,
Dominated). On a player in faithful, OnApplySetState keeps no NWN state: the charm icon (EFFECT_ICON_CHARMED)
credited to the charmer for the effect's time, an attack on it under way stopped, a word - unless NWN refuses
the charm (immune to charm or mind spells: its own feedback). The orders: the dying rules' and the Rage's hooks
on CNWSObject::AddAction and CNWSCreature::AddCastSpellActions moved into one place (action_guard.cpp, order
guards); the charm's guards refuse an attack order on the charmer (action 0xC, its one parameter the target -
read from CNWSCreature::AddAttackActions) and a spell aimed at it with spells.2da HostileSetting 1, with a word.
Mindless Rage drops the charm icon with the fear's. Test ConditionsCharmed2024 (6 mutations caught).
*0.59.10:* a charm stops an attack under way (the orders read directly; an attack roll on the charmer a miss) -
see the clock's notes under rests (#105). *Accepted (owner, 2026-11-01: "the other items in the matrix pass";
"104 and 106 accepted").* C1-C6.

## Grapple and Throw — decided by the owner (2026-10-27)

Owner: "can you think about ways to implement some dnd staple actions such as
grapple or throw (enemies especially)", then, of the shortlist (Dodge, Dash,
Disengage, Help, Ready, Net, NWN's grenades as 2024 gear, dragging a downed
ally, dropping prone, Jump - all left for later): "let's go just with the
grapple and throw for now". It replaces CN8's "Grappled (no grapple in NWN;
Phase 6)".

*SRD 5.2.1 (checked 2026-10-27):* Unarmed Strike (p. 190) - Damage, Grapple or
Shove, each in place of one attack; Grapple and Shove: the target, at most
one size larger (Grapple: and you have a hand free), makes a Strength or
Dexterity save (its choice) against 8 + Str + PB. Grappled (p. 182): Speed 0;
Disadvantage on attacks against anyone but the grappler; the grappler can drag
or carry it, each foot costing 1 extra unless it is Tiny or two or more sizes
smaller. Grappling (p. 182): one grapple per hand; escape: its action, a
Strength (Athletics) or Dexterity (Acrobatics) check against the escape DC;
it also ends if the grappler is Incapacitated or the two end up farther apart
than the grapple's reach, and the grappler can let go at any time. Grappler
feat (p. 87; level 4+, Str or Dex 13+): Punch and Grab, Advantage against the one you grapple, no
extra cost to drag your size or smaller. **No rule lets a character throw a
creature**: the only throw is the Kraken's Fling (p. 303) - a Large or smaller
creature it grapples, to a space it can see within 60 ft not in the air; the
thrown creature and each creature in that space make a Dexterity save (DC 25):
failure 4d8 Bludgeoning and Prone, success half. Falling (p. 182): 1d6 per
10 ft, Prone on landing.

*Engine pieces already there:* Shove (SK6: NWN's Knockdown, knock prone only;
rules_shove.cpp), the push (rules_push.cpp: PlanPush - a straight line that
stops before unwalkable ground and placeables, not creatures; a fall and a
0.1 s slide; used by the Push mastery and Forceful Blow), saves against a 2024
DC (rules_save_roll), the advantage hook (rules_advantage BeforeAttackRoll),
the movement-rate hook (GetMovementRateFactor), feat -> spell -> C++ handler
rows with sub-radials (Brutal Strike), creature updates (AddCreatureUpdateListener).
Not there: a Grappled condition, speed 0 other than CutsceneImmobilize,
dragging, a size-comparison helper, action economy (Phase 6).

**GT1. Modes.** Grapple and Throw are faithful only; NWN mode has neither
(owner: "we're not adding new features to base NWN"). Throw is a house rule
(the SRD has none for characters), built on the Kraken's Fling.

**GT2. Throw's numbers** (owner: "numbers seem fine"). Only a creature you
grapple, at least one size smaller than you; in place of one attack; it ends
the grapple. Distance: 5 ft x your Strength modifier, at least 5 ft (Str 18:
20 ft). The thrown creature and any creature it lands on make a Dexterity
save against your grapple DC (8 + Str + PB): failure 1d6 Bludgeoning per
10 ft thrown (at least 1d6; the Falling rule) and Prone; success half damage,
no Prone.

**GT3. A grappled NPC** (owner: "sure"): one fighting its grappler keeps
attacking it (no Disadvantage there); one that was after someone else tries to
escape each round (the better of Athletics / Acrobatics against the escape DC,
in place of its round's attacks).

**GT4. Order** (owner: "yeah"): G1 Shove's other choice, push 5 ft (rules_push)
-> G2 Grapple (the condition; the grappler drags it at half speed unless it is
Tiny or two sizes smaller; Release; Escape; ends on Incapacitated or out of
reach) -> G3 monsters that grab (the SRD stat blocks' "the target has the
Grappled condition (escape DC X)" hits, on the matching NWN creatures) -> G4
Throw (a flight arc - SetObjectVisualTransform lerps over a longer slide - and
the landing: the knockdown fall, dust, a camera bump) -> G5 animations (a pose
step for the animation recipes: grip, held, drag, lift-and-throw).

*Built 0.45.0 (G1; owner: "might as well keep going with the shove push"):*
Shove's push is its own radial action, nwn5e's feat 1313 "Shove: push"
(Bigby's Forceful Hand icon), next to Knockdown, which faithful now names
"Shove" (feat.if_shove.txt) - one click on a creature, no mode to set. Engine
facts (RE of UseFeat 0x3a91d0): for Knockdown / Disarm / Called Shot etc. it
calls AddAttackActions(target) then CombatRound::AddSpecialAttack(feat) (none
with a ranged weapon); the id lands in an attack's +0x7a and
ResolveMeleeSpecialAttack resolves it; ids past the game's rows (>= 1116)
skip its uses check (+0x6d) and resolve as an ordinary attack. So feat_use.cpp
gained handlers (a feat nwn5e carries out itself): 1313 is queued exactly so,
and rules_shove.cpp resolves it - no attack roll, no damage (the Shove's own
hooks), the size limit (one larger), the Shove's save (the better of Str /
Dex vs 8 + Str + PB, shared as unarmed.h's RollUnarmedSave for the Grapple),
and on a failure rules_push.cpp's ShovePush: the push line (walls,
placeables, unwalkable ground stop it), no fall - the hit flinch, 0.5 s held,
the 0.1 s slide. Every player character has both Shoves while the Shove is
faithful (rules_features.cpp; feat 23 granted only when missing, so one a
character took stays in NWN mode; players only - other creatures' AI would
use Knockdown far more). `!shove [push]` shoves the creature you are
attacking (the radial's fallback). Tests: EveryoneCanShove (mutations: the
own Knockdown taken back, granted in NWN mode - both caught).

*0.45.1 (owner's test of 0.45.0: "shove with a ranged weapon targets and
fires a ranged attack" - "push and shove should be in the same common action
section of the radial menu, right? right now push is in the class actions"):*
(1) A player's Shove (Knockdown) with a ranged weapon in hand is refused with
a word (a UseFeat guard) - UseFeat queues the attack and adds the special only
without one, so the engine shot instead; other creatures keep the engine's
way (their AI might stand idle if refused). (2) The radial's Special Attacks
page is a fixed list in the client (RE: its builder 995090 adds Parry, Called
Shot, Power Attack, Knockdown slot 4, Improved Power Attack, Disarm slot 6,
More Special Attacks through one entry helper 9957a0; no feat.2da column
decides - only a cls_feat OnMenu 1 entry puts a feat anywhere, in the class
section, which 0.45.0's rows did). radial_special_attacks.cpp hooks the
builder and puts Shove: push in slot 6, Disarm's (Disarm does nothing in
faithful, SK6): the setter reuses the label and deletes the old icon; the
slot's disabled bit is cleared first and the label count put back. The cls_feat
rows are gone. The quickbar's assign menu uses the same builder.

*0.45.2 (owner's test of 0.45.1, Barbarian 3 with a halfling: after the bow,
the greataxe "wouldn't even let me grab the axe" - not red):* an engine bug,
both modes, not the halfling's size. In combat a player's equip / unequip is
queued on the combat round (AddEquipAction: type-6 action, item +0x14, slot
+0x1c; AddUnequipAction type 7) and replayed at the round's end
(ProcessPendingCombatActions); CNWSCombatRound::RemoveAllActions - the combat
ending (SetCombatState 0), the combat timer, a new attack target,
PacifyCreature - frees the list without a reply, and the client keeps the item
marked as waiting (CNWCItem +0x240), refusing to pick it up for the session
(RE: the grab gate CGuiInvRepository::HandleLMouseDown 0x881dc5; cleared only
by HandleServerToPlayerInventory). equip_cancel.cpp hooks RemoveAllActions
and answers each queued equip / unequip of a player's item with EquipCancel /
UnequipCancel first, as AddEquipAction answers one it replaces: the item is
free again, drag it once more. Also: the push's chat line says how far when the
line is cut short ("pushed only 0.5 m (something close behind it)"; 0.45.1
said "pushed 5 ft" for a 0.5 m slide).

*0.45.3 (owner: "can enemy corpses still be pushed back if the killing blow
has a push effect?" - option B, "go with B, 0.1 s"):* a killing blow's push
(the Push mastery, Forceful Blow) moves the body: the same planned line (walls,
placeables, unwalkable ground stop it), the same 0.1 s slide, no fall and no
hold (its death animation plays), no retarget (the kill's own picks the next
enemy). Before, the planner refused a dead target ("not pushed (dead)") and a
slide stopped when its creature died - now one that dies mid-slide goes on.
Cosmetic: a corpse is no creature to push by the rules. A dead creature may get
no AIUpdate, so body slides are moved from any creature's update.

*0.48.5 (owner, Brutal Strike with two effects: "one of the attacks that moved
the enemy was sometimes just blinking it away instead of even sliding it"):* a
slide took 0.1 s whatever its length - Forceful Blow's 15 ft at 45 m/s, half
again the speed the 10 ft Push was tuned at - and moved only in the pushed
creature's own updates, which a busy area may not give it every frame: a long
push could reach the client as one or two jumps. Now a slide keeps the tuned
speed (10 ft in 0.1 s, 30 m/s: 15 ft in 0.15 s, never under 0.1 s) and every
slide moves from any creature's update, by time, as the bodies' do. The log's
push line says how long the slide takes. Unverified: that the client never
snaps a 0.15 s slide (its position smoothing wasn't read).

*0.49.2 (owner's test of 0.48.5: "forceful blow still looks weird"; the log:
every Forceful Blow came with the battleaxe's Topple, "falls without a round
pause (its round is paused by" itself):* Topple's knockdown (EffectKnockdown, 0.5
s after the roll) and the push's own fall started together - two falls, the push
holding its fall 1.2 s and then getting up while the knockdown wants a round on
the ground. A push on a creature whose Topple save just failed
(ToppledRecently, rules_mastery.cpp: 2 s) now only slides it - the knockdown's
fall and round stand ("no fall of its own: Topple's knockdown has it" in the
log). Unverified that this was what looked weird - asked.

*0.45.5 (owner: "for shove the PC should use the nwstab animation for now"):* a player's Shove
(Knockdown) and Shove: push swing "nwstab", the unarmed thrust, whatever is in hand. The server
notes the Shove as rules_shove resolves it (ShoveSwing: 1.5 s); anim_layers.cpp's client creature
update ties that to the creature's model, and Gob::PlayAnimation (hooked there) plays nwstab for
the model's next melee swing (<hold> slash / stab / close / reach / cleave; not a parry, a walk, a
layer). Unverified: that the client starts the swing after the server resolves it - if it starts
before, the swap would land on the next swing (!animtrace shows the order). Bespoke shove /
grapple animations: G5.

**G2 decisions** (owner, 2026-10-27: "go with the recommendations"):
D1 a free hand is required (an empty main hand, or a one-handed weapon and an
empty off hand; refused with a word otherwise); D2 the radial's Special Attacks
page next to the Shoves (its eight slots are full: Called Shot: Leg's slot 3 -
Called Shot has no 2024 counterpart), the quickbar (auto-placed), `!grapple`;
D3 one creature at a time; D4 Grappled: can't move, Disadvantage on attacks
against anyone but its grappler, an icon, GT3's AI; D5 the drag: it comes along,
at half speed unless Tiny or two sizes smaller - a slide for now; D6 it ends on
letting go (Grapple on it again, `!grapple release`), an escape, the grappler
Incapacitated, out of reach, or the free hand filled; D7 the Shove's nwstab swing
for the grab until G5; D8 a player's own Escape comes with G3 (only monsters
grapple players).

*Built 0.49.0 (G2):* nwn5e's feat 1312 "Grapple" (Bigby's Grasping Hand's
icon; data/rows/feat.unarmed.txt), every player character's while the Shove is
faithful (FeatsEveryoneHas2024, GrantableFeats2024's record bit 8), on the
Special Attacks page's slot 3 (radial_special_attacks.cpp, now a list of
entries) and the quickbar's actions (bit 11). rules_grapple.cpp: queued and
resolved as the Shove's push (unarmed.h gained an option's own start step:
Grapple used on the creature it holds lets go, a full hand or a second grapple
is refused - before anything is queued); the resolver re-checks the hand, the
size (one larger at most), that neither is already in a grapple, and rolls the
Shove's save; on a failure the target is Grappled, the escape DC the same.
Grappled: EffectCutsceneImmobilize + the icon (effecticons 223), temporary
(3 s) and refreshed every 2 s from the grappler's update, so a loaded game
loses them in seconds (the grapples live in memory); its attacks on anyone but
its grappler have Disadvantage (rules_advantage.cpp). The drag: held at the
grab distance (1-2 m); past it (+0.2 m) it is placed back at that distance on
the line between them (rules_push.cpp's new DragCreatureTo: walkable and clear
of placeables, else it stays); past it + 1.5 m it is out of reach and the
grapple ends. The grappler's movement rate is halved while it drags something
bigger than Tiny / two sizes smaller - GetMovementRateFactor now lives in
movement_rate.cpp (Fast Movement's hook moved there, the drag an adjuster).
GT3: a grappled creature the game moves that isn't fighting its grappler tries
once a round (6 s): d20 + the better of Athletics (skills.2da row 28) and
Acrobatics (Tumble, 21) - the engine's skill ranks, 2024 values with `skills`
faithful - against the DC; a failed try pauses its round 3 s with the flinch
(rules_push.cpp's new PauseCombatRound: in place of its attacks). It ends on
letting go, an escape, the grappler down or Incapacitated (IncapacitatedByCondition),
the grappler's hand filled, out of reach, death, another area; the players
among the two are told (names through the script bridge: StringToObject).
`!grapple` grapples the creature you attack, says whom you hold, `!grapple
release` lets go. Tests: GrappleNeedsAFreeHand, GrappleHoldAndEscape,
EveryoneCanShove and the quickbar's (mutations: the off hand ignored, a shield
alone refused, no hold clamp, no slack, beats-not-meets, no grant, no quickbar
entry - all caught).
*0.58.6 (the owner's G2 run, 2026-10-30, G2-9: "the grappled enemy keeps attempting to escape the grapple while
the game is pause, and not so much while the game is going"):* the escape try's and the held icon's timers ran on
GetTickCount, and the creature updates run during a pause - four of five tries in the log fell in one. They run
on the game clock now (GameMs, still while paused). "Not so much while running" is GT3 as decided: a grappled
creature fighting its grappler keeps attacking. The messages point at Grapple again, not `!grapple release`
(owner: "I'm not a big fan of using !commands for combat stuff, I rather use quickbar or radial menu actions").

*The owner's G2 run, 2026-10-31:* "G2 results I think they all pass" - G2-1 to G2-10, G2-12, G2-13; GT3 kept
("keep gt3"). G2-11 doesn't: "the enemy sometimes teleports to the final location, instead of sliding" (the log:
Forceful Blow "slides 4.6 m away in 152 ms" - 0.48.5's open question answered: the client does snap a 0.15 s
slide; it gets the server's positions at its network rate and smooths a creature's own walking, not a move the
server makes for it). The grapple's look, the same way: "the PC continues to move normally, albeit at half speed,
and the grappled npc eventually slides after. is there a better way to do this? ... Is it possible to have the PC
extend the empty handed arm towards the enemy in a way that suggests they are grabbing the enemy? and when they
walk this animation should not be broken, they would need to walk backwards or sideways to keep contact. Also,
right now the enemy follows very far ... is it possible to have a tighter coupling?"
Offered: (1) smooth movement on screen - slower slides / nwn5e smoothing the moved creature's model on the
client / the engine's own walking for the held creature; (2) tight coupling - the held creature kept at arm's
reach (both sizes) every update, no slack; (3) the reaching arm - an arm layer over idle and walk, the pose built
in the bench (its IK pins the hand); (4) walking while holding - A: it stays in front, the grappler walks forward
as usual; B: the grappler faces it and walks backwards / sideways (a facing hook, a directional walk set).
*Decided (owner, 2026-10-31: "go with your recommendation, keep 11 open"):* in that order - (1) by client-side
smoothing first (G2-11 with it), then (2), (3), and (4) A - B if A doesn't look right in game. #11 stays open
until the slide looks right; (3) and (4) go with G5 (#14), their animations through the bench (the tools stream).

*Built 0.58.7 - steps (1) and (2).* Read 89.8193.37: the client smooths a move the server makes for a creature
that isn't walking only under 4 m - HandleServerToPlayerCreatureUpdate_Update (0x781E80) snaps a jump of 4 m or
more (Forceful Blow's 4.6 m) - and chases a shorter one over 512 ms from the last network position (+0x2C0),
which is where the drag's lag came from. Every client move goes through CNWCCreature's vtable slot 37 (0x80AD90:
it copies the vector to +0x38, moves the light, checks the area, and puts the model there). client_smoothing.cpp
hooks it: for a creature nwn5e moves, the vector is copied and its x / y replaced by nwn5e's point (the height
stays the engine's) - a push's slide drawn along the server's line by the client's own clock (rules::SlidePoint),
then held at its end until the client's network target catches up (within 5 cm, or 1 s: rules::SmoothHandBack);
a held creature at the server's position each frame. rules_push.cpp and rules_grapple.cpp tell it what they move
(SmoothSlide / SmoothFollow / SmoothStop). Single player only: another player's client keeps the engine's
smoothing. (2): the hold is arm's reach - 1 m up to a Medium target, half a metre more per size above
(rules::GrappleHoldMetres) - pulled in at once and every update, at most 8 cm an update, with 5 cm of slack
against jitter (rules::GrappleDragDistance); it was where the creature was grabbed (1-2 m), dragged only once 0.2 m
past it. Tests SmoothSlideAlongTheServersLine, SmoothHandsBackOnceTheClientCaughtUp, GrappleHoldAndEscape (10
mutations caught).
*The owner's 0.58.7 run, 2026-10-31:* S2-S6 pass ("the rest are good": the grab pulled to arm's reach, the
drag, the Large hold, the release, the pause). G2-11: "the animation order should be falls prone first and then,
while the falling prone animation is playing, sliding back. that way it looks like they are flying back. But the
speed is too great at the moment." S1 (a Shove's push): "The slide isn't smooth, it's very fast." The log: the
client drew every slide each frame (Forceful Blow's 4.6 m in 152 ms, 7 frames; handed back after 235 ms), but at
the speed tuned while the client still snapped slides (30 m/s; a Shove's 1.5 m in 0.1 s, 5 frames), and starting
as the damage landed - with Topple's knockdown, which a script applies 0.5 s after the roll - so a slide was over
before the fall showed.
*Built 0.58.8.* A push that knocks the creature down (the Push mastery's own fall, Topple's knockdown, a killing
blow's body) waits for its fall to show on the server (at most 0.6 s), then slides 100 ms into it over 500 ms,
whatever the distance (rules/push_slide.h: PushSlideStartMs). The tools session read the knockdown clips: kdbck
(falling backwards) has the body down at about 0.4 s, kdfnt (forwards) at about 0.6 s, both clips 1.0 s, so the
slide ends just after the body lands. A Shove's push is a stumble back over 350 ms. The client is told to draw a
slide when it starts, so both begin together; the hold (Cutscene Immobilize) outlasts the slide. The three times
are !test switches - push_slide_ms 100..1500, push_slide_delay_ms 0..500, shove_slide_ms 100..1500 - with a
switches-only profile, `!test slides` (data/tests/slides.txt), to tune them in game without a rebuild. Tests
PushSlidesWhileItFalls and the switch ranges in TestProfilesLevelsSwitchesNotes (12 mutations caught).
*Accepted (owner, 2026-10-31: "grapple and slide tests are good"; "11 accepted").* G2 (the grab, the
drag at arm's reach, the hold by size, letting go, the escape tries on game time, GT3) and the push slides'
look on 0.58.8/0.58.9: a falling push slides into its fall, a Shove's push stumbles back, both drawn by the
client. The reaching arm and walking while holding (steps 3 and 4 of the 2026-10-31 decision) go on with G5
(#14).

**GT-L1. Later: throwing an enemy into another** (owner: "How possible would it
be to implement throwing an enemy into another as a way to damage both? We're
not implementing this now, just note it down for later"). Feasible, and GT2
already has the rule (the Kraken's "each creature in the destination space"
saves and takes the damage too). In NWN terms: Throw aims at a creature (or a
spot) as a targeted ability rather than only "away from you"; the flight is
planned as PlanPush plans a push, but also stops at the first creature on the
line (creatures don't overlap) - that one is struck; both save and take the
damage, both fall Prone on a failure (a struck one might also be pushed back
5 ft - to decide then). Any creature in the way is struck, ally or enemy, as
the rule reads.

## Quickbar — decided by the owner (2026-10-27)

Owner: "I was only asking to figure out if having abilities and actions the PC
can perform assigned to number keys (1-9) would be feasible. at least for the
most common actions. so the player wouldn't have to dig through 4 layers of
radial menus to set the strike they want to use" - "do the 1-9 hook plus
auto-placing new actions". `quickbar` (faithful; nwn / off = NWN's quickbar - a
new feature, so faithful only). rules/quickbar.cpp (the plan, tested),
core/rules_quickbar.cpp (the engine).

*0.48.0:*
- **Keys 1-9 press quickbar slots 1-9**, as F1-F9 do. The client turns each
  key into an input event (fixed ids set up at 0x6df630, not keymap.2da's) and
  CClientExoAppInternal::HandleInputEvent (0x6ed720, not exported) acts on it:
  F1-F12 are events 5-16 - CPanelQuickBar::ClickButton (0x85c3b0) on the row on
  screen (Shift / Ctrl held show the second / third row, SetBank 0x859a60);
  keys 1-9 are 0x19-0x21 - a conversation's replies, else 1-4 select EE's Quick
  Groups (Ctrl+1-4, events 0x4b-0x4e, set them) and 5-9 nothing. The hook
  hands 1-9 on as F1-F9, so the engine's checks and rows apply (Shift+1 =
  Shift+F1); a conversation, Ctrl held or app +0x630 (a restricted-input state
  of unknown meaning) leave them alone. Typing never reaches it (CExoInput
  drops keys while a text box has the keyboard). Trade: plain 1-4 no longer
  select a Quick Group.
- **nwn5e's class actions go on empty slots** the first time a player
  character has them, in this order: Second Wind; Rage, Reckless Attack, Brutal
  Strike's options (Forceful and Hamstring Blow from 9, Staggering and
  Sundering from 13, Off), Intimidating Presence; Shove, Shove: push. The first
  empty slot (keys 1-9 first, then F10-F12, the Shift row, the Ctrl row);
  never over a button; an action already on the bar is only recorded; one the
  player took off stays off (nwn5e_quickbar_placed, a bit per action); a full
  bar waits for a free slot; Second Wind's button follows its feat (1220 ->
  1221 -> 1222) in place. A chat line names each new button and its keys. A
  Brutal Strike option's button is a feat button holding the option's
  spells.2da FeatID (option * 65536 + 1311) - what the quickbar's assign menu
  stores (RadialMenuUseFeat 0x998520 -> CGuiQuickButton::SetButtonFeat 0x867070)
  and clicks with (0x85cb60, the radial's call). Engine: CNWSCreature +0x4e0 ->
  36 CNWSQuickbarButton of 0x60 bytes (+0x08 type, 4 = feat; +0x50 the number),
  SetQuickbarButton_GeneralINTParam, SendServerToPlayerGuiQuickbar_SetButton
  (one slot); saved with the character. `!quickbar` lists the actions and
  where they are; `!quickbar reset` forgets the record and puts the missing
  ones back on empty slots.
- Unverified (the test): that the client takes a server-sent option button as
  its assign menu does (HandleServerToPlayerGuiQuickbar_SetButton wasn't read)
  - its icon and label, and that clicking it picks the option.

*0.48.1 (owner: "the quickbar needs to have the numbers always visible
otherwise it-s not that useful"):* the game names each slot's key - "F1"..,
"Shift-F1"..,  "Ctrl-F1".., white at 60% (CPanelQuickBar's constructor
0x8584a0, the only code using "F%d"; the label is the button's text, a
CAurString at CGuiQuickButton +0xe0) - but hides it as soon as the slot is
filled: every setter (SetButtonFeat 0x867070 and some 40 more) sets the
label's colour to 0,0,0,0 and only clearing the slot (0x85c240) brings it
back. So only empty slots showed their key. Now a quickbar key label stays
shown, white and opaque (a hook on the label's SetColor, CAurString vtable slot
15, 0x4a3a0 - the RGBA at *(label +0x58); only the current panel's 36
labels), and slots 1-9 of the first two rows read "1".."9" and
"Shift-1".."Shift-9" (the constructor, then the label's SetText, slot 26;
only while the number keys are hooked). The Ctrl row keeps "Ctrl-F1"..
Unverified (the test): readable over every icon (white text, no outline).

*0.48.2 (owner's test of 0.48.1: "the only issue is F10,F11 and F12's labels
only show up as \"F1\""):* a label draws only what fits its box - the width
at label +0x74 (a float set when the text is attached to the button model's
text0 / text1 hookpoints; SetText's CAurFont::
CalculateVisibleStringLengthAndWidth cuts the rest) - about two characters:
"F10" showed "F1", and the rows' "Shift-" / "Ctrl-" labels were stubs (the
game's own, on empty slots, too). Each quickbar label is widened 1.6x
(CAurString::SetWidth, vtable slot 24, 0x4a8c0) before its text is set again,
and the labels lose the row prefixes - a row shows only while its key is held:
"1".."9", "F10".."F12" on the first two rows, "F1".."F12" on the Ctrl row
(rules::QuickbarSlotLabel). Unverified: a label re-attached later (a GUI
rescale) might go back to the narrow box.

*0.48.3 (owner: "oh, the label is being hidden by the icon"):* the GUI draws
by depth, and the quickbar button model (qb_but67) puts its hookpoints at text
z 0.40, icon 0.55, the counts (count0 / count1) 0.75 - so the icon covered the
key label (the game hid it on filled slots anyway). Each quickbar label's node
moves 0.25 toward the viewer (0.65: over the icon, under the counts), the way
the label's own SetPosition (CAurString vtable slot 4, 0x4a900) reaches it:
StringGob = label - 0x4c0, its node through Gob vtable +0x228 (index -1), a
string node (type 0x3ea), position at node +0x70. SetPosition's bytes are
checked at install.

*0.48.4 (owner, again: "still nothing, the labels are still being covered by
the button icon"; the log: "widened 0.00 -> 0.00, 0 moved over the icon"):*
both 0.48.2 and 0.48.3 missed. CGuiTextButton::InitializeModel (0x1ea890, run
by CGuiObject::SetModel inside the panel's constructor) attaches the label to
the "text" hookpoint with Gob::AttachToObject mode 0 - a behaviour that only
follows the hookpoint; SetPosition moves only an offset behaviour (type 0x3ea,
what Gob::GetBehavior(-1) - the "node" of 0.48.3 - must be), so nothing moved.
And a label on one hookpoint has width 0 - "no limit" to
CAurFont::CalculateVisibleStringLengthAndWidth - so the 1.6x widening did
nothing (removed; why "F10" read "F1" is still open: slot 10's laid-out text is
logged when a panel is made). The counts on icons (CGuiIcon::AttachCountLabel
0x1e3310) attach with mode 1 - the offset behaviour (0x53d70, offset at +0x70)
- then SetPosition(x, y, 0.1). Now InitializeModel is hooked: on a
CGuiQuickButton (its vtable) the label is attached again, mode 1
(Gob::SetBehavior frees the old behaviour; Gob::SetScene is a no-op for the
same scene), and SetPosition(0, 0, 0.25). The log says how many labels went
over the icon.
Accepted (owner's test of 0.48.4: "ok, it-s good"): the key labels show over
the icons; the log: 36 attached over the icon, slot 10's text "F10" whole.

## Mocap mode — moved to docs/TOOLS-PLAN.md

The Mocap mode notes (#74-#95: the captures, the bench's mocap tools, the bench redesign and #95) are in docs/TOOLS-PLAN.md since 2026-10-30 (CLAUDE.md, "Two sessions in one checkout").

## Action queue strip — decided by the owner (2026-10-28)

Owner: "One thing I think is sorely needed is an action queue display" - then "a sort of action
queue at the middle top of the screen going left to right from the next action to the most
recently added to the queue. Each item would consist of an action and target"; "go with the
design, do it before monk part 3".

*Found first:* EE has a small one - PNL_ACTIONQUEUE under the portrait (CPanelActionQueue: the
action in progress and the queue, click to cancel), fed by CNWSCreature::UpdateActionQueue
(0x3a6270: up to 10 entries of 0x1c at creature +0x59c - group id, display id, parameter,
point, target) and CNWSMessage::ComputeLastUpdate_ActionQueue. Display ids are NWScript's
ACTION_* values (GetAQActionIDByID; GetCurrentAction returns them); special attacks have their
own by feat (Knockdown, Disarm, Stunning Fist ...) - nwn5e's Shove: push and Grapple none.

*Design taken:* a strip centred at the top, no title bar or border; left to right from the
action in progress (gold) to the newest; up to 8 items, then "+N"; each the action's icon (the
spell's, the feat's or the radial menu's ir_*), its name and its target ("a spot" for a point,
"yourself"); a click cancels it; the player's own character; `!queue` on / off (per character,
on by default); hidden when nothing is queued; the portrait panel stays.

*Built 0.54.0* (src/core/action_queue.cpp, src/rules/queue_strip.cpp): the queue read as the
engine reads it - action groups in order (GetPositionByGroupIndex, GetActionByGroupId), the
combat round's special attacks before their attack (feat 0, 6 and the internal 0xfdea / 0xfdeb
skipped, as the engine does), each action's parameters where UpdateActionQueue reads them
(ee_layout.h); bookkeeping actions without an ACTION_* value left off. Checked every 150 ms per
player; one script per change creates the NUI window if needed (its eight slots built in a
loop from one template - a short script) and sets names (spells.2da, feat.2da, iprp_spells for
an item's power, the target's name), icons, colours and the centred geometry (the player's GUI
width). A click calls CNWSCreature::CancelAction(display id, group or special-attack id) - the
portrait panel's own call (a matching special attack comes off the round, else the group) -
and RemoveSpecialAttack for nwn5e's own special attacks.
Later, if wanted: henchmen's queues; the round's attack count; a fully clear background.
*0.54.1 (owner: with two actions queued "the scrollbars ... rapidly appear and disappear alternatingly
horizontal and vertical"):* the hidden slots still take room in NUI's row, so it overflowed the window
sized to the shown ones and each scrollbar took room for the other. The row sits in a group without
scrollbars ("scrollbars": 0 - clipped, never scrolled); the window 124 px tall, 28 px of side room.
*0.54.2 (owner: "clicking an item in the queue doesn't cancel it, I can queue the same item (a potion
for example) multiple times while not having enough, this will pollute the queue. with the queue full of
potion actions, if I move somewhere that clears the queue completely"):* nui.cpp's event hook knows a
window by the token kept on the character (nwn5e_nui_<id>, set by NuiOpen) - the strip's own script
never set it, so its clicks were dropped; it sets it now. A player's use of an item's power is refused
("... all 3 uses are already queued") when the ItemCastSpell actions of that item and power in the queue
take every use it has left (rules::ItemPowerUsesLeft: single use - the stack; N charges a use - charges /
N; uses per day - those left today; others no limit), read by script from its Cast Spell property
(item_use.cpp now tells guards which active property is used). A move clearing the queue is NWN's own
(a new order replaces it) - kept (owner, 2026-10-28: "sure, let's keep the same NWN behaviour for the
move").
*0.55.0, animated (owner: "thinking that actions slide down and fade out as they exit the queue and all
queued elements slide left"):* NUI has no animation of its own (one built-in effect: "encouraged", a
glow), but every property can be a bind - a window's geometry (slide), a colour's alpha (fade), draw-list
shapes. Server-side updates (a script and a message each) are too heavy for 60 a second, so the strip moved
to nwn5e's client windows (client_ui.cpp: the game's own NUI, from its every frame; ClientNuiSetJson added
for geometry / colours / bools). The server's side still reads the queue and names it by one script
(locals), then hands the items over (a lock); clicks come back the same way. Each item is its own small
window (a tile: icon, name, target over a dark card - a draw-list rectangle with a colour bind): the
strip's left edge fixed (a full strip centred), a tile eases to its slot every frame (one leaving: those
after it slide left), a new one drops in and fades in, one leaving slides down 36 px and fades over 0.4 s
(text and card; the icon greys - it turns disabled: NUI can't fade images) and closes. Without client
windows (a server with no game client) the server's strip as before.
*0.55.1 (owner: "a few frames where a new action shows up in the middle of the screen ... it can just
appear in place"; "not as centered as it was before"):* a client window opens where NUI puts one with no
geometry (the centre) and takes its bound geometry a frame or two later - a new tile stays empty for three
frames, then appears in place (no drop-in). The strip is centred on the items it shows again (the fixed
left edge sat left of the centre); one leaving, it closes up on its centre - the items after it slide left.
*0.55.3 (owner: "we need to make sure the whole text is readable. a Potion of cure wounds right now reads
as 'Potion of Cur'"; then "is it possible to split the text into new lines? Potion of / Cure Wounds?"):*
three text lines under the icon - the name split at its spaces into two balanced lines
(rules::WrapQueueText: the split whose longer line is shortest - "Potion of / Cure Wounds") and the
target, each line its own colour; each tile as wide as its longest line (about 7.5 px a character -
13 had filled the 92 px the old tile left its text; 96-300 px), the strip centred on them, those that
don't fit on the screen to the "+N". (0.55.2 capped the client windows' open / close log lines.)
*Accepted (owner, 2026-10-29: "ok, it's good, monk part 3").*

## Round bar — decided by the owner (2026-11-03)

Owner: "sometimes it's hard to figure out when a round ends and the next one begins, is there something that could be
done here? An onscreen element, like a small loading bar that would show the state of the current round?" (#127).
The engine keeps it per creature: CNWSCombatRound's RoundStarted (+0x2F04), SpellCastRound (+0x2F08), Timer
(+0x2F0C), RoundLength (+0x2F10), Paused (+0x2F1C - each swing pauses it), the reaction count (+0x2F40) and the
schedule (+0x2F70: entries of 0x28 bytes, the due time at +0, the type at +0x10 - 1 an attack). Options: A the bar,
the swings' ticks and three pips (the Action, the Bonus Action, the Reaction) under the action queue strip; B the bar
only; C A's content in the resource panel.

*Decided (owner, 2026-11-03: "go with A").*

*Built 0.66.0* (src/core/round_bar_window.cpp, src/rules/round_bar.cpp - one obj folder: the names differ): a client window under the strip
(rules::RoundBarTop: the strip's tiles' bottom, or the screen's top with `!queue` off), centred at the GUI scale, in a
fight only - a card, the three pips with their letters (A, B, R: green left, grey spent, amber a Bonus Action waiting
for the next round - bonus_actions.cpp's BonusActionStatus, reactions.h's ReactionReady; shown while the actions are
faithful), the bar (rules::RoundFill; gold an attack round, blue a cast round, dimmed while paused, empty between
rounds) and up to ten ticks (rules::RoundSwingMarks: the schedule's attack entries as the round starts; dark once made
- fewer left). The server's side samples the single-player character every update (a lock); the client's draws every
frame, the fill moving between samples while the round runs. `!roundbar` or Combat configuration's "Display: Round
bar" turns it off (the record nwn5e_roundbar_off). The resource panel, when it sits under the strip (a narrow window),
goes under the bar (rules::PlaceHud's round_bar_on). Tests RoundBarFillAndSwings, RoundBarKindsAndPips, the panel's
place (13 of 13 mutations caught).
*The owner's 0.66.0 run (2026-11-03):* "the bar pauses for a split second, then the vfx plays, then the animation
plays" - the log: the Rage flourish waited for the next round ("no room for 3542 ms in its round"), then played paused
3542 ms at the new round's start; no swing during it. The bar then sat empty, dimmed and still - no visible pause.
*Built 0.66.1:* while the round is paused the whole track turns violet. The gap between the Rage's VFX and its
animation (the flourish waiting for room) is the animation session's: #128 ("yes, do both").
*0.66.3 (#53):* the ticks follow the swings - a made one where the round was when it was made, the ones to come where
they're due now (an Action Surge's added swings, a held swing's followers).
*The owner's 0.66.8 run (2026-11-03):* RB-1 to RB-7 - the log: the bar in each fight; Patient Defense, a second one
waiting, then used; Uncanny Dodge as the reaction (Deflect refused, the reaction spent); `!roundbar` off and on; the
Rage flourish after the swing in progress, the round paused 3542 ms. *Accepted (owner, 2026-11-03: "everything
looked correct, accept 127").*

## Resource panel — decided by the owner (2026-10-29)

The owner, after Monk part 3: "We might need some sort of custom hud element to display focus points".
Offered (info first): placement (a) above the quickbar's left end, (b) under the queue strip, (c) by the
portrait; look (a) pips, rows of 10, with "14/18", a spent pip fading out, (b) a bar, (c) an icon and a
number; scope (a) Focus only, (b) a general class-resources panel, Focus first, later Rage, Second Wind
and once-per-rest features (Uncanny Metabolism as a dot lit while ready); shown (a) always while the
character has a pool, (b) in combat or when not full; plus a hover tooltip with the costs and `!hud` to
turn it off. **Decided:** "go with the recommendation, do it before testing" - 1a, 2a, 3b, 4a.

**Built (0.57.0).** src/core/resource_panel.cpp, src/rules/hud_layout.cpp:
- *What it shows:* one block per pool the class rules give (AddHudPool; the Monk's Focus from rules_monk.cpp,
  Uncanny Metabolism its dot from Monk 2): the name, a pip per point - a blue circle under a grey ring, the
  fill fading out over 0.45 s when spent, back in over 0.25 s - in rows of 10 (more than 40 points: the count
  alone), "left/total", then each flag's name and dot (gold while ready, an empty ring once used), on a dark
  card. Hovering it shows the tooltip (rules::FocusTooltip: the points, the DC, the costs at this level,
  Uncanny Metabolism ready or used, Perfect Focus). `!hud` turns it off and on (record `nwn5e_hud_off`).
- *How:* the server's side reads the pools for the single-player character every 150 ms and the window's
  size and GUI scale (GetPlayerDeviceProperty) every 2 s, and hands them over when they change; the client's
  side keeps one NUI window, rebuilt only when the pools' shape changes (names, totals, flags) - a spend or a
  rest only recolours pips. A new window shows nothing for three frames (NUI opens it at the screen's centre
  until its geometry applies), then appears in place. Closed when there is no in-game GUI (the main menu).
- *Where (read from 89.8193.37):* the quickbar is 800 x 36 GUI units, centred on the bottom edge, scaled by the
  GUI scale s about its bottom centre (CPanelQuickBar::Reposition 0x860010: SetPositionScreen((gui width -
  800) / 2, 0); Camera::UpscaleViewport's sceneGUI_PNL_QUICK_BAR branch 0x5c131; CGuiInGame::GetGuiPosition
  0x6f7b00 keeps 36 for it); nothing moves it, one row shows. A NUI window's x / y are window pixels, its w / h
  GUI units (x s). The main chat window (EDIT_CHAT_MAIN, 720 GUI units wide from the left edge, its bottom on
  the bar) sits right above the bar's left part at nearly every width, so the panel starts just right of it -
  above the bar's left part, not over the newest chat lines (an adjustment to 1a; owner to confirm).
- *Not yet:* hiding it when the quickbar hides (cutscenes): the bar's +0xa0 (non-null while it shows,
  GetGuiPosition's test) is logged, not used, until a log confirms it. Rage, Second Wind and the Fighter's
  and Rogue's once-per-rest features come as their own pools.
- *Noted:* the action queue strip places its tiles as if x were in GUI units - right at the owner's GUI scale
  1.0, off-centre at other scales (to fix with the strip's next change).

*0.57.1 (owner, after a look: "have the resource bar be aligned with the right edge of the quickbar and use
paragraphs to structure the text inside the hover tooltip"):* the card's right edge on the quickbar's right
edge (NUI is Nuklear: the window's padding 4 and the group's 4 - the strip's 108 tile left its text 92 - and
the group clips its children's draw lists, so the card fills the group, 4 in from the window's edge, and the
window is placed by that). The tooltip in paragraphs: "Focus: 14 of 18 points" and what they are; "Costs", one
a line; "Uncanny Metabolism: ready"; "Perfect Focus". (The owner's 0.57.0 log: the quickbar's +0xa0 set while
it showed; window 1078 then 1638 x 904, GUI scale 1.0.)

*0.57.2 (owner: "either reduce the resource box padding, remove the semiopaque background or move the box
slightly up as it's just a bit over the Talk input box"; and "if the game window resolution is something that
would be low enough that the resource panel would overlap the chat window it should be repositioned and placed
underneath the action queue panel"):* moved up. Read from the game: the Talk line (CGuiInGameChatPrompt,
PNL_CHAT_PROMPT, placed at (0, 36), always shown in game - its HideChatPrompt has no callers) is a band the
window's full width, 33 to 56 GUI units over the bottom edge, so the card's bottom now sits 4 over 56 (0.57.1's
was at 40, 16 into it). The chat log (CGuiInGameChatWindow, PNL_CHAT; EDIT_CHAT_MAIN, or EDIT_CHAT_LEFT /
EDIT_CHAT_RIGHT split) is 720 GUI units wide from the left edge at any window size, its bottom on the Talk
line, only its height draggable (ui.chat.pane.primary.height; the owner's 2.98 - about 298). Where the card's
left edge would come within 8 of the chat log's right edge (the window narrower than about 1460 px at scale
1.0 for a Monk's Focus card), the card goes centred under the action queue strip instead - below the tiles
(4 + 122 x s) and the 36 px a leaving tile slides (rules::PlaceHud; the strip's sizes now in
rules/queue_strip.h, shared). Checked again every 2 s with the window's size, so a resize moves it.

*0.57.3 (owner: "the resource panel should be right up against the talk input, right now there's a bit of a
gap. When it gets transferred to the top, if the action queue is hidden, the resources should be up against the
top of the window, if it's showing the resources should be right up against it, no space between them"):* no
gaps - the card's bottom on the Talk line's top (56 x s over the bottom edge), under the strip its top on the
tiles' bottom (4 + 122 x s: a tile's card, -4 inside its column, ends at the window's 122), and with the strip
turned off (`!queue`) on the window's top edge. The panel still doesn't follow the strip filling and emptying
(it sits under the strip's place either way); the owner asked whether the strip should be a permanent fixture
so nothing moves.
*Decided (owner, 2026-10-29: "go with c, and fade in place under the strip"):* (c) - the panel keeps the strip's
place whether the strip shows or not (nothing moves as the queue fills and empties; no empty strip drawn); the
strip's own `!queue` off puts it on the top edge. 0.57.4: while the panel sits right under the strip, a leaving
tile fades in place (rules::QueueExitDrop) instead of sliding 36 px down over it; elsewhere it still slides.

*0.57.11 (owner, 2026-10-30, after the Monk's test run: "all pass except for the hover tooltip on the resource
panel, it doesn't show paragraphs"):* NUI's tooltip draws its text as one line - the paragraphs' line breaks
reach it (nui::Str escapes them, the text arrives with them) but aren't drawn. The panel's own hover box
replaces it: a second client window (n5hudtip; input passes through), a dark card and a draw-list line per
line of text - rules::HudTipLines wraps the tooltip at word breaks to 330 GUI units (kHudCharWidth a character)
and keeps the paragraphs' gaps - right-aligned above the panel, below it at the screen's top
(rules::PlaceHudTip). NUI has no hover event (no mouseenter / mouseleave among the binary's event names), so
the cursor is read each frame: GetCursorPos on the game's window (this process's largest visible top-level
window), scaled to the window size the game reports; off the panel, the box waits off screen. Rebuilt when
its text changes, shown three frames after (the screen's-centre lesson).
*0.57.12 (owner: "the tooltip panel is permanently onscreen, positioned on the top left. when the resource panel
is hovered the tooltip repositions to the correct places, but when the cursor leaves ... returns to the top
left"):* NUI keeps a window on screen, so the box parked at -10000 showed at the top-left. It now stays at its
spot by the panel, its draw list switched off ("on" false) unless the mouse is on the panel - nothing drawn,
input passing through.

*Accepted (owner, 2026-10-30: "28, 57 and 23 accepted").* The resource panel (#57), 0.57.0-0.57.12: HU1-HU12 and the hover
box (HU5) seen in the owner's runs.

## Test profiles — the owner's request (2026-10-29)

The owner, after the test matrices for the issues waiting on a test: "set different "!test" commands for
each of the different requirements" (#82). `!test` applied one profile, data\test.txt, so each test meant
rewriting it, and the switches a test needs meant an ini edit and a restart.

**Built (0.57.5).** src/core/test_commands.cpp, src/rules/test_profile.cpp, data/tests:
- `!test` lists the profiles (data\tests\*.txt, each with its `name`); `!test <profile>` applies one (a name
  of letters, digits, '-' and '_' - no path). data\test.txt is gone (its Monk 18 is `monk`).
- One per setup in the matrices: `monk` (#28, #57), `barbarian` (#25), `grapple` (#11), `fighter` (#23),
  `combat` (#2), `rest` (#49), `halfling` (#19 O6), `smallmonk` (#19 S4), `fourclass` (#19 EP16).
- New keys: `levels` (more classes, each up to its level - EP16's three); `switches` (test switches for the
  session only - attack_trace, attacks_per_round, creature_hp_multiplier, creature_min_hp, rest_clock, the
  ones read each time they are used; every `!test <profile>` puts the ini's values back first, so one test's
  switches don't stay on for the next; the ini is never written); `note` (lines shown after it is applied:
  what the profile can't do itself - "start from a new level 1 character", an ini-only switch).
- For those switches to work mid-game: with test_commands, the creature hit point floor and the attack trace
  listen from the start (each checks its switch on every roll), and the attacks-per-round hook is in even with
  extra_attack off (its sheet fix stays off there unless a count is set).
- *Not settable from a profile* (installed at start-up: the ini only): every_hit_crits, combat_breakdown, mode
  and the rule switches (dying, rests ...); a profile that needs one says so in a note.
- *Not covered:* #19's CM1 / CM3 / CM4 rows (a character made in NWN mode, henchmen) - no profile makes those.
- A unit test parses every profile in data\tests, so a typo fails test.bat instead of `!test` in game.

*0.57.6 - decided by the owner (2026-10-29: "go with a").* Asked for new characters for some rows (levels were
only added), the owner: "wait, can't the test commands also set levels? why are you asking me for ne
characters?" Offered: (a) a profile that sets a class or levels first takes the character down to level 1,
then builds it exactly; (b) levels only added, as built - a new character for some rows; (c) a separate
`!reset`. Built (a): a profile with swap_class, class or levels (rules::TestProfileBuildsCharacter) starts with
a script SetXP 0 - CM1 reads 0 as "level 1", and the engine's level loss takes the levels away, the last taken
first (the DM level-down script's path, not yet seen in game: the first `!test` on a higher character is its
test too). If no level is lost, the character's XP goes back and the profile only adds, as before; chat says
which ("level: 18 -> 1 (a fresh start)" or "couldn't go down"). The profiles' "start from a new character"
notes are gone; one character runs every profile, in any order. A feat a profile gives through `feats` isn't
tied to a level and may stay.

*Fixes from the owner's tests (2026-10-30):*
- *0.57.7 (the owner's log: `!test fourclass` after `smallmonk` gave Fighter 1 / Rogue 1 - "Wizard needs
  Intelligence 13" on the previous profile's Int 10):* a profile's scores are set before the levels too (a new
  class's 13 test, CM2, sees them) and again after.
- *0.57.9 (owner: after `!test monk` "the quickbar's slots all became empty"):* the reset and a swapped first
  class took the old actions' feats, and the engine their buttons with them; the placement record
  (nwn5e_quickbar_placed) still had them as placed, so they stayed off. A profile that builds a character now
  forgets that record (as `!quickbar reset`): every action goes back on an empty slot at the next update.
- *0.57.13 (owner: after `!test fighter` "the quickbar still has old monk feats and no fighter feats"):* the
  swap from Monk to Fighter took the Monk's feats, but the engine left their buttons (unlike the earlier
  swap), so they kept the slots. A profile that builds a character also empties the buttons of nwn5e's actions
  the character no longer has (rules::LostQuickbarActions: any class's actions, against the character's now;
  items, spells and other feats untouched). A Fighter's own action is Second Wind (and everyone's Shove /
  Grapple); its other features are passive.
- *0.58.1 (owner: "quick bar is absolutely empty now"; the log: the cleanup ran at `!test`, before the
  features update took the old class's granted feats - so those stayed - and Shove was emptied because the
  reset's level loss took Knockdown):* the cleanup waits 3 s (after the features update, every 2 s), then sends
  the whole bar again (the engine's own full update, so the client shows what the server holds). The features
  update gives back a feat nwn5e granted that a level loss or a swapped class took (its record still said
  granted, so it never came back) - Shove, Shove: push and Grapple after a reset.
- *0.58.2 (the owner's log: "script SetXP 120000 -> 195000" at `!test fighter`):* `!test` / `!level` raised the
  XP with the SetXP script command, which CM1 reads in NWN's table - and 2024's level 13 threshold, 120,000, is
  NWN's level 16 (the only one of the twenty that is), so a Fighter 13 had level 16's XP. They now raise it
  through the engine's SetExperience (only the script command's call is converted); the reset to level 1 keeps
  the script's SetXP 0 (its level loss is the point).

*Accepted (owner, 2026-10-30: "2 and 82 accepted").* The test profiles (#82), 0.57.5-0.58.2, with the `cleric` profile
for combat C2: TP1-TP18 seen in the owner's runs.

## Test register — the owner's request (2026-11-08, #147)

The owner, 2026-11-08: "instead of having ephemeral test matrices let's have an excel file where we keep all the
tests, passed and open so that I can just look at them any time and filter to only show the open ones", then "make
it so the status cells are dropdowns where I can select the status". The process is CLAUDE.md's "The test register".

- docs/tests.csv holds every test: 632 backfilled from the test tables on 63 issues. An accepted issue's rows are
  Passed and an open issue's are Open, with the results from the owner's messages filled in.
- scripts/tests_sheet.py (standard library only) writes TESTS.xlsx (git-ignored): newest issue first, a filter on
  every column (it opens on the Open tests), and Status dropdowns coloured by value. Every run first reads the saved
  sheet back, so a Status or Note the owner changes there goes into the register. `set` and `add` edit it.

*Accepted (owner, 2026-11-08: "#147 accepted").* TR-1 to TR-4 passed; the owner marked results in the sheet's
dropdowns, and they were read back.

*Moved to a GitHub Project (#148; owner, 2026-11-08: "Argh, so I guess an excel file to track issues isn't the best
as it cannot be read and edited by agents and myself at the same time, what else can we use?", then "go with A" - of
a GitHub Project, Google Sheets, a claude.ai page with a database, or keeping Excel).*

- The tests are on the "nwn5e tests" board (users/BrunoTCarreiro/projects/3), one item per test, edited by the owner
  and the sessions at once. Its columns are Status (a dropdown), ID, Issue (a number), Version, Stream, Setup, Do,
  Expect, Note, Added, Result date and Source; text columns hold 1024 characters, so each item's description holds
  the whole test.
- The 243 open and failed tests were moved. The passed, superseded and dropped ones stay in docs/tests.csv, the
  archive, and `export` writes the board back into it.
- scripts/tests_board.py (list, add, set, export, move) replaces tests_sheet.py, and TESTS.xlsx is retired. The
  owner's last Excel results were carried over.

## Phase 1c/1d and racial bonuses — decided by the owner, 2026-10-18

Walked through one by one (issue, what NWN does, what 2024 does, what to
expect). Engine facts: RE-NOTES "Saving throws" and "Two-weapon fighting and
attacks of opportunity". Owner: "both versions, faithful as default" for
each, except where noted.

**R1. Racial combat bonuses.** NWN: Battle Training (+1 to hit vs orcs and
goblinoids for dwarves, vs reptilians for gnomes), halfling Good Aim (+1
thrown) and +1 with slings, Defensive Training (+4 AC vs giants). Faithful:
all removed (the attack then always matches the sheet); the 2024 species
traits replace them (the save ones with C3, the rest in Phase 3). NWN mode:
kept.

**C1. Which save.** Faithful: Fortitude → Con, Reflex → Dex, Will → Wis by
default, plus a per-spell table switching to the 2024 spell's ability
(Banishment → Cha, Feeblemind → Int, Entangle → Str…), filled as spells are
converted (Phase 4); needs the spell behind a save (recon). NWN mode: the
three saves with the default mapping only (computed the 2024 way). The
original 3e saves stay available by switching the save rule off.

**C2. Save proficiency.** Faithful: characters and henchmen — the two saves
of their first class (2024; later class features add more: Monk 14, Rogue
15); creatures — ability modifier only (most 2024 monsters have none); NPCs
built with real classes count as characters. Bosses get Legendary
Resistance with the monster conversion (Phase 8). NWN mode: characters the
same; creatures proficient in every save their NWN tables make "good".

**C3. 3e save bonuses.** Faithful: flat feat and racial bonuses dropped;
Great Fortitude / Lightning Reflexes / Iron Will → proficiency in Con / Dex
/ Wis saves (like 2024 Resilient); Divine Grace and Dark Blessing → Aura of
Protection (Charisma modifier, min +1, to saves of the paladin and allies
within 3 m, from paladin level 6; the Blackguard counts as a paladin);
Luck of Heroes and the creature blueprint save bonus dropped; species get
2024 advantages by save type — dwarf vs poison, halfling vs fear plus
rerolling natural 1s on d20 tests (Luck), elf vs charm (and sleep immunity
as Trance), gnome on Wis (later Int/Cha) saves. Needs advantage on the save
roll's d20. NWN mode: the same base and advantages, plus NWN's flat feat and
racial bonuses and the blueprint bonus; Divine Grace as NWN (self, from
level 2).

**C4. Natural 1 / 20 on saves.** Faithful: no automatic success or failure.
NWN mode: NWN's — a natural 20 always saves, a natural 1 fails when the game
option "Saving Throw Automatic Failure On 1" is on.

**C5. Spell DC.** Faithful: class casters 8 + proficiency bonus + casting
modifier (spell level and Spell Focus no longer count); spells from items
use the 2024 spell-scroll DCs by spell level (13 for 0–2, 15 for 3–4, 17 for
5–6, 18 for 7–8, 19 for 9); monster abilities 8 + their CR proficiency bonus
+ their Charisma modifier (instead of NWN's fixed 3). NWN mode: the same,
with Spell Focus / Greater / Epic as +1 / +2 / +3.

**C6. Other DCs** (traps, poisons, diseases, item powers, scripted monster
abilities, module scripts) — both modes: converted when the save is rolled,
new DC = 10 + (old DC − 10) / 2, rounded, kept within 10–30 (14 → 12, 20 →
15, 26 → 18, 30 → 20, 40 → 25); spell DCs (C5) excluded. Phase 8 replaces
the formula with exact 2024 values for the main sources.

**C7. Bonuses inside the save roll.** Faithful: item save bonuses kept
(balance pass in Phase 8); spell effects kept until each spell is converted
(Phase 4: Bless → +1d4, Protection from Evil → advantage…); feat and racial
"+2 vs X" dropped (C3's advantages replace them); immunities kept. NWN mode:
all kept.

**C8. Character sheet.** Both modes: the three numbers show the 2024 saves.
Faithful: the labels are swapped in memory ("Fortitude / Reflex / Will" →
"Constitution / Dexterity / Wisdom") wherever the game shows them, sheet and
combat log included — no game files changed; needs the text lookup (recon).
NWN mode: NWN's labels. All six saves: a `!saves` chat command prints them
with the proficient ones marked; three more rows on the sheet itself are
looked into in Phase 7.

**D1. Two-weapon fighting.** Faithful: no attack penalty on either hand; one
off-hand attack per round; off-hand damage without the ability modifier
unless negative (NWN's Two-Weapon Fighting feat = the 2024 fighting style,
adds it back); both weapons must be Light (club, dagger, handaxe, light
hammer, sickle, scimitar, shortsword, plus kukri and kama); Improved
Two-Weapon Fighting → Dual Wielder (any one-handed weapon, still one extra
attack); Ambidexterity does nothing; double weapons give the one extra
attack. NWN mode: NWN's rules unchanged (penalties, feats, Improved TWF's
second attack, half Strength).

*Built 0.8.5 (2026-10-18).* Same as decided. Found on the way, for a later
decision: NWN also adds 1.5 × Strength to damage with two-handed weapons and
+1 damage within 5 m with Point Blank Shot (both 3e; 2024 has neither).

**D2. Opportunity attacks.** Faithful: casting no longer provokes; moving
away still does, once per round per creature; Combat Reflexes gives no extra
ones and Spring Attack no longer protects (feat rework); Tumble stays as the
way out until Phase 6 adds Disengage. NWN mode: NWN's rules unchanged.

*Built 0.8.3 (2026-10-18), with what the recon found:* NWN already refills
each creature's one AoO at the end of every round, and 1.69 has no Combat
Reflexes, so "once per round" and "no extra ones" hold in both modes without
code. NWN also provokes on four things the decision didn't list — drinking
a potion and every ranged shot (the party is exempt from both at the easier
difficulties), unarmed attacks without Improved Unarmed Strike, and Disarm.
None provoke in 2024 (a potion is a Bonus Action; a ranged attack next to an
enemy has disadvantage instead, B5), so faithful removes
them too, in keeping with "only moving away provokes"; NWN mode keeps them.

**D3. Weapon mastery — who.** Faithful: the 2024 counts (Fighter 3/4/5/6 at
1/4/10/16, Barbarian 2/3/4 at 1/4/10, Paladin, Ranger, Rogue 2); chosen with
chat commands (`!mastery`, `!mastery <weapon>`, swap one after a long rest);
empty slots fill with the weapon kinds actually used (announced in chat);
Weapon Focus also gives mastery of its weapon for any class (2024 Weapon
Master stand-in); later the choice may move into level-up (Phase 3/7). NWN
mode: the mastery classes have mastery with every weapon they are
proficient with; Weapon Focus the same.

*Built 0.8.7 (2026-10-18).* Choices made on the way: several mastery classes
give the larger count, not the sum (like Extra Attack; prestige levels add to
the parent class); a swap is `!mastery <old> for <new>` (either order since
0.8.8: the weapon already in a slot is the one given up) and becomes available
when a rest finishes (also the ForceRest script command); filling an empty
slot by `!mastery <weapon>` is free; only weapons the character is proficient
with (baseitems.2da ReqFeat) can be chosen or auto-filled, unarmed never;
a magic staff counts as a quarterstaff; henchmen fill their slots by use only
(the player is told). The choices are local variables on the creature
(`nwn5e_mastery_1`…`6`, `nwn5e_mastery_swap`), so they live in the save game.

**D4. Weapon mastery — what.** Each NWN weapon gets its 2024 property
(`src/rules/weapons.cpp`; NWN-only weapons take the closest). Faithful: all
eight, automatic whenever their condition holds — Vex and Sap (advantage /
disadvantage), Graze (damage on a miss), Cleave (NWN's cleave attack on a
hit), Topple (Con save or knockdown, after C1–C4), Slow (−33% speed for a
round), Push (3 m knockback; last, possibly approximated); Nick waits for
the action economy (Phase 6); monsters don't use mastery. NWN mode: the
same without Push.

*Built 0.9.0–0.9.7 (2026-10-18).* Vex, Sap, Graze, Slow, Topple and Cleave
work and are verified in game (README has the evidence). Choices made on the
way: a "hit" is attack result 1/3/7/10 and Vex/Sap last in NWN combat rounds
(an NWN round is a 2024 turn); Graze, Slow and Topple effects land when the
swing does (queued like the engine's own hit damage); Topple's knockdown lasts
a round; Cleave's extra attack is the engine's own cleave attack, aimed
through the round's target as Great Cleave does. Known gap: Vex and Slow also
trigger on hits that deal 0 damage (immune targets; 2024 needs damage).

*Push parked (owner, 2026-10-18), with this design for later:* on a Push hit
(target Large or smaller, faithful only) the target plays NWN's knockdown
(falling) animation as a visual only — no knockdown effect, no prone, it gets
straight back up — and during the fall is moved up to 3 m straight away from
the attacker, through the engine's own jump (the path scripts'
JumpToLocation uses), onto walkable ground only and stopping short at walls.
Recon needed: playing the fall animation without the knockdown state, and the
jump / safe-location call.
*Built 0.9.13 (2026-10-19), option A of the recon:* on the hit's landing
(OnApplyDamage), the target moves up to 3 m (0.5 m steps) straight away onto
walkable ground with 0.5 m clear behind it, then plays the knockdown fall as a
visual through a 1.2 s round pause (shortened by as much, as the hit flinch
does); warhammer and heavy crossbow. To check in game: whether NPCs slide or
snap over the move, the fall's length and direction.
*0.9.14 after the owner's test (2026-10-19):* the one-step move made the
client slide the model after the PC had already reacted, losing the fall;
pushed goblins could end inside a fountain (placeables aren't in the tile
walkmesh). Now the fall starts at once and the target slides along the line
during it (AIUpdate steps), the line must also clear the area's static
objects (placeables), and — owner request — when another enemy is then
closer than the pushed one, the attacker switches to the nearest (in place
for the round, made to stick at its end). Known gap: closed doors are not
checked.
*0.9.15 after the second test (2026-10-19):* the fall sometimes did not
play (the round could not pause), the slide was cut short (the target walked
back toward the attacker) and retargeting never fired. Now the target cannot
move for the fall (an AI-state effect, as entangle uses), the fall plays even
without a round pause, and the retarget is a logged scan of the enemies (each
decision and the round-end check are in nwn5e.log).
*0.9.16 (owner's third test, 2026-10-19):* retargeting works (logged
"attacker now attacks the closer", kept at the round end); the slide is
0.25 s. The "AoO from the push" was the pushed goblin running back: NWN
provokes from every enemy a runner passes, also when running toward them;
D2's faithful rule ("only moving away provokes") now holds for that too.
*0.9.17-0.9.18:* slide 0.15 s (owner). The remaining "swing at the pushed one"
was not an AoO (none logged, no AoO text) but the round's next swing, aimed
before the hit landed; the retarget now happens at the attack roll (the
landing still moves the target).
*0.9.19:* at the roll the engine's attack-order fields are still empty
(round target and queued attack both 0x7F000000), so 0.9.18 never switched
and the PC walked after the pushed one (drawing AoOs from the other goblin).
Now the switch is decided at the roll from the attack's own target (round
target set, as after a kill), the order is rewritten again when the hit lands
(by then it names the pushed one), and the round-end check stays.
*0.9.20:* the 0.9.19 logs show the switches held, but twice the PC stood idle
after a push (once following with no switch, once after a switch) while the
goblin attacked it: its attack order was dropped. For 8 s after a push the
attacker is watched: its order changes are logged, and if it is left with no
order at all while its intended target stands, the attack is reissued (as a
click). Corpses are never chosen (the scan skips the dead).
*0.9.21:* 0.9.20 test: retargeting held on all 8 pushes, no reissue needed;
the PC lost its target once, after drinking a potion (outside the watch;
probably NWN clearing the attack order on item use, unverified). The fall
sometimes did not show though the server set it: now the target stops
moving at once (AI-state bit cleared directly as well as by the effect) and
the fall animation is put back after each of its AI updates for the fall
s length (the first override is logged with the animation that replaced it).
*0.9.22 (owner request, 2026-10-19):* after a potion mid-fight the PC goes back
to attacking (`resume_attack_after_potion`, both modes). The creature-update
hook (AIUpdate) moved to creature_update.cpp, shared by Push and this.
*Hazard test (owner, 2026-10-19, 0.9.22):* a Caltrops field (permanent area
of effect, on-enter 1 damage after 1.0-2.2 s, the same every round) — two
pushes into it gave 1 damage 1.4 s and 1.8 s later; per-round ticks 6 s
apart cannot explain both, so a pushed creature counts as entering an area
of effect (the trigger update after SetPosition). Web from the Cloak of
Arachnida lasts 1 round (caster level 3 / 2), too short to test with.

**E1. Two-handed Strength (owner, 2026-10-18).** NWN adds 1.5 x the Strength
modifier (truncated) to two-handed melee damage; 2024 adds it once. Faithful:
once; NWN mode: 1.5 x. Point Blank Shot's +1 damage stays with B3 (Phase 3).
*Built 0.9.8; verified in game 2026-10-19 (warhammer, Strength modifier +3: 1.5 x gave
+4 in 3e, now +3).*

**C1 table (owner, 2026-10-18): option A.** Faithful: the NWN spells that exist
in SRD 5.2 with another save ability use it — Strength (Entangle 53, Gust of
Wind 75, Evard's Black Tentacles 375), Intelligence (Feeblemind 55), Charisma
(Bane 449, Banishment 430, Lesser/Planar/Greater Planar Binding 96/128/69),
Constitution (Cone of Cold 25, Sunbeam 183, Sunburst 427, Inflict Minor to
Critical Wounds 431-435 and the Blackguard's 611/612). NWN-only spells
(Dismissal and the like) keep the default mapping; NWN mode has no table.
The spell is the running script's (OBJECT_SELF +0x188, an area of effect's
own +0x1CC), so lingering clouds use their own spell. Effects on the NWN save
(items, spells) still apply to the overridden one. *Built 0.9.8.* *0.9.27:* the
first in-game test (Cone of Cold, 2026-10-19) rolled Reflex: spells that save
through GetReflexAdjustedDamage (Cone of Cold, Sunbeam, Sunburst) go through
their own command handler (299), which 0.9.8 did not hook; now hooked too.
Follow-up: that handler still applies Evasion to the damage when the save is no
longer Dexterity (2024 Evasion covers Dexterity saves only) — class features.
*Verified in game 2026-10-19 (0.9.27: Cone of Cold, goblins rolled
Constitution). 0.9.28:* the combat log still read "Dexterity Save vs. Cold" —
the client names the save from the kind (1/2/3) the server sends, so a save on
a spell's own ability is now sent as its own kind and shown as "Constitution
Save" (and Strength / Intelligence / Charisma Save for the others). *Verified
in game 2026-10-19 ("Weak Goblin : Constitution Save vs. Cold").*

### Build order

1. The mode switch (`mode=faithful|nwn`, per-rule keys override).
2. R1 — remove the racial combat bonuses (small).
3. Saves (0.8.x): save values (C1 default, C2, C3 numbers) → save roll (C4,
   C3 advantages, C6 conversion) → spell DC (C5) → `!saves` and labels (C8)
   → the per-spell save table scaffold (C1).
4. D2 — opportunity attacks (small).
5. D1 — two-weapon fighting.
6. D3 then D4 — mastery choice (`!mastery`), then Vex, Sap, Graze, Cleave,
   Topple, Slow, Push.
7. Follow-ups noted for later phases: Legendary Resistance (8), Disengage
   (6), sheet rows (7), species traits (3), spells (4).

*Accepted (owner, 2026-10-30: "2 and 82 accepted").* The combat rows not seen in game until then (#2), on 0.58.x: a
bow's remaining attacks to the next enemy in the fight (decision 14); a crit with a flat effect bonus (Divine
Favor's +1 once, the dice doubled - crit_dice_only); back to the same target after a potion; Push - a Huge
creature not pushed, a Large one slid 10 ft (D4). C1b / C1c (shots lost with no enemy in the fight, or past
40 m) accepted on the owner's word, without a log line.

## Decided (owner, 2026-10-17)

8. Expanded critical range (Champion's Improved Critical, 19–20): the expanded
   range only upgrades a hit; a natural 19 that does not reach the AC misses.
   Only a natural 20 always hits (`rules::ResolveAttackRoll`).
9. Creature AC conversion target: keep the 2014 DMG "Monster Statistics by
   Challenge Rating" AC column as a personal-use guideline
   (`rules::ExpectedMonsterArmorClass`); not shipped in anything shared.
10. Prestige classes count as levels of the class they would be a 2024
    subclass of (`rules::ClassFromNwnId`): Weapon Master, Arcane Archer,
    Purple Dragon Knight, Dwarven Defender → Fighter; Champion of Torm,
    Blackguard → Paladin; Assassin, Shadowdancer → Rogue; Harper Scout → Bard;
    Red Dragon Disciple → Sorcerer; Pale Master → Wizard; Shifter → Druid.
    Levels add to the parent class (Fighter 6 / Weapon Master 6 = Fighter 12).
11. Henchmen use the player-character rules (gear AC, proficiency by level,
    Extra Attack from their classes). Summons, familiars and animal companions
    keep the creature rules (CR-based AC and proficiency, engine attack count)
    — they have stat blocks, not gear.
13. Critical hits double only the damage dice (2024), not modifiers.
14. When the target dies mid-round, the remaining attacks move to the next
    enemy in reach (2024 Extra Attack lets each attack choose a target).
    Ranged (owner, 2026-10-17): only to an enemy already in the fight — one
    charging or attacking the attacker or the party — so a bow never picks an
    idle, caged or sleeping enemy. With none, the remaining shots are lost,
    as in NWN. Range: 20 m at first; 40 m (the weapon's range) once the
    filter was in (owner, 2026-10-18), set by `ranged_retarget_range`.
