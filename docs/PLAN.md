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

### Phase 5 — Rest, healing, death
- Short rest (spend Hit Dice) and long rest (8 h, regain all HP and half max HD).
- Death saving throws at 0 HP; stabilise; massive damage.

### Phase 6 — Action economy
- Action / Bonus Action / Reaction / free object interaction inside NWN's
  6-second round. Highest design risk: NWN's action queue has no bonus action
  or reaction concept. Needs a design pass before code.

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

### Phase 8 — Creatures and campaign balance
- Bounded accuracy breaks against 3e stat blocks (HotU creatures with AC 30+).
  Convert creatures at spawn from their 3e blueprint to a 5e-style stat block
  (CR-based AC/HP/attack/DC) with hand-tuned overrides for named creatures.
- Campaign XP and the level cap: OC/SoU/HotU expect up to level 40.

### Phase 9 — Balance and polish
- Play-through of OC chapter 1, SoU, HotU openers; tuning tables.

## Open decisions (owner)

1. Half-elf and half-orc are not 2024 species — keep as legacy, or replace?
2. New species (dragonborn, tiefling, goliath, aasimar, orc) — appearance work needed.
3. Epic levels: cap at 20 with Epic Boons, or a custom 21+ track for HotU?
4. Hit points per level: fixed average or rolled?
5. Prestige classes with no 2024 subclass equivalent: drop or adapt?
6. Rituals and material components: model or ignore?
7. Action economy design (Phase 6).
12. Subclass selection UI: prestige classes become 2024 subclasses (decision
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

### Phase 3 — Rogue (CM9): recommendations (for the owner's decision)

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
  the face), the game's art, so in the private layer (`--center`); the public
  build has a plain disc. Ten ASCII effect models of nwn5e's own (one emitter,
  update Single / render Normal, like the game's vdr_seen marker), 1.4 m over
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
- `!vfx` has a search field (owner: "can you add a search bar to the vfx
  window?"): the list narrows as you type, by name or row number.
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
