# nwn5e — full D&D 2024 (5.5e) conversion of Neverwinter Nights Diamond

> **EE port (2026-10-20).** This plan is copied from nwn5e (1.69) at d6cf14a.
> This repo targets **NWN: Enhanced Edition 89.8193.37-17 (64-bit Windows,
> single player)** by route B (docs/EE-RECON.md): an x64 DLL that finds engine
> functions by their exported names. Every decision below stands. The engine
> addresses, byte patches and 1.69 file paths below are 1.69's; each rule is
> re-found in EE when it is ported, and its "built" note is updated then.
> Status: spike (0.0.1) — `GetCriticalHitRoll` hooked by name.

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
