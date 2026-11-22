#pragma once

// #126 / #182, the radial menu's cleanup (docs/PLAN.md "Phase 6", #126; owner, 2026-11-21: "we need to look for
// nwn/3e specific menu items to remove them"; then "the cleanup (the NWN / 3e removals, the renames, #182) built
// soon"): what the faithful radial leaves out and what it renames - the entries NWN's 3e rules put there with no 2024
// meaning, and NWN's names that clash with the 2024 actions. core/radial_pages.cpp, radial_special_attacks.cpp,
// radial_class_feats.cpp and rules_labels.cpp do it in the client. NWN mode (and off): the game's radial, nothing
// left out or renamed.
//
// The client builds its pages in code (the #126 brief): an entry tied to a feat is added only when the character has
// the feat - so a page leaves it out when it doesn't see the feat while it builds (RadialPageHidesFeat); an entry it
// always adds is found by its icon and the call that adds it removed (RadialFixedEntries); a class section lists the
// class's cls_feat OnMenu 1 feats (RadialHidesClassFeat); a label is a dialog.tlk string (RadialLabelKey: nwn5e's
// text, data/text/en.txt, in its place).

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "rules/modes.h"

namespace rules {

// The radial pages the client builds in code.
enum class RadialPage {
  kSpecialAttacks,    // the self ring's slot 3 (IR_XATTACK) and the quickbar's "Assign Special Attacks"
  kSpecialAbilities,  // the self ring's slot 6 (IR_XABILITY) and the quickbar's "Assign Special Abilities"
  kCastSpell,         // the self ring's slot 1, another creature's slot 1, the quickbar's "Assign Spell"
};

// A feat whose entry `page` adds only when the character has it, left out in faithful: the page doesn't see the feat
// while it builds. Special Attacks: Called Shot (5: slots 1 and 3), Power Attack (28: slot 2), Improved Power Attack
// (19: slot 5, #182), Disarm / Improved Disarm (9 / 16: slot 6) and the feats behind slot 7's More Special Attacks -
// Expertise (389), Improved Expertise (390), Sap (31), Rapid Shot (30), Dirty Fighting (425), Stunning Fist (39: the
// Monk's class section keeps it, as Stunning Strike) and Whirlwind Attack (867) - so that page and its entry go.
// Cast Spell: the 3e Metamagic page (Empower 11, Extend 12, Maximize 25, Quicken 29, Silent 33, Still 37: the
// IR_METAMAGIC entry - where the 2024 Sorcerer's Metamagic goes later) and Epic Spells (Epic Character, 1001).
// Knockdown / Improved Knockdown (23 / 17) stay: nwn5e's Shove.
bool RadialPageHidesFeat(RuleVariant radial, RadialPage page, int feat);

// An entry a page always adds (no feat needed), left out in faithful.
struct RadialFixedEntry {
  RadialPage page;
  int slot;
  const char* icon;  // its icon resref, which finds the call that adds it
  bool sub_page;     // a page of its own (SetSubMenuCreatorx), not a leaf (SetEntryx)
  const char* name;  // for the log
};
// Special Attacks 0 Parry Mode (NWN's parry skill; SK8: no parry mode); Special Abilities 0 Crafting Skills (NWN's
// Craft Trap; owner, 2026-11-21, #183: "A with c for later"), 1 Taunt (SK8: Taunt does nothing); Cast Spell 6 Spell
// Special Abilities - NWN's Counterspell (2024's is a spell) and Defensive Casting Mode (SK7: no defensive casting),
// its only two entries. None in NWN mode (*count 0, nullptr).
const RadialFixedEntry* RadialFixedEntries(RuleVariant radial, size_t* count);

// A class section feat (cls_feat OnMenu 1) the faithful radial hides - NWN-only, or replaced by a spell: Divine Might
// (413), Divine Shield (414), Remove Disease (302), Elemental Shape (304), Animal Companion (199), Summon Familiar
// (303: the Find Familiar spell), Mighty Rage (869), Curse Song (871), Epic Dragon Shape (873). Smite Evil, Lay on
// Hands, Summon Mount, Bard Song, Wild Shape and Turn Undead stay (their 2024 versions come with the classes), as do
// nwn5e's own rows.
bool RadialHidesClassFeat(RuleVariant radial, int feat);

// A label the faithful radial renames: the game's dialog.tlk string and nwn5e's text in its place.
struct RadialLabel {
  uint32_t strref;
  const char* key;   // data/text/en.txt
  const char* game;  // the game's text, for the log
};
// NWN's "Disengage" (1057: the Attack entry while attacking - it stops the attack, nothing like 2024's Disengage)
// as "Stop Attacking" (owner, 2026-11-21); Detect Mode / Cancel Detect Mode (4848 / 4849) as the Search action,
// Stealth Mode / Cancel Stealth Mode (4850 / 4851) as the Hide action (nwn5e's detect and stealth modes are them:
// SK5). The game shows each of these strings only on its radial and quickbar buttons and the action queue's icons,
// all meaning that same thing, so the string is renamed wherever it shows. "Cast Spell" (557) as Magic, the SRD 5.2.1
// action's name (owner, 2026-11-21: "do the Magic rename and Prepare Spells now"): the client uses 557 only as the
// spell section's entry on a radial - your ring, a DM's, your associate's, another creature's, the ground's, an
// item's, a placeable's, a door's, and the spell menu's own "Cast Spell" entry (a DM's); the quickbar's assign ring
// says "Assign Spell" (1158), its own string. None in NWN mode.
const RadialLabel* RadialLabels(RuleVariant radial, size_t* count);
// The key of the text that replaces `strref`'s, nullptr when it stays.
const char* RadialLabelKey(RuleVariant radial, uint32_t strref);

// Whose ring a radial menu is (0.99.111's first test: the owner's "nothing much on the radial" was the ring the
// ground opens - Move To, Cast Spell, More Actions, Special Abilities - which never had Special Attacks; the log
// didn't say which ring opened). A creature menu's target (+0x7f8) and its target's master (+0x804) against the
// player's id; the ground's ring builds its pages on a creature menu with no creature (the target OBJECT_INVALID).
enum class RadialRing { kYours, kYourAssociate, kAnotherCreature, kGround };
constexpr uint32_t kRadialNoObject = 0x7f000000;  // OBJECT_INVALID
RadialRing RadialRingOf(uint32_t target, uint32_t master, uint32_t player);
const char* RadialRingName(RadialRing ring);  // "your", "your associate's", "another creature's", "the ground's"
// The log's list of a ring's entries: the slots set (bit s of `slots`), named for your ring and the ground's
// ("0 Character Sheet, 1 Cast Spell, ..."), numbered for the others ("0, 1, 2, 3, 6, 7"); "none" when empty.
std::string RadialRingSlots(RadialRing ring, unsigned slots);

// #126, Magic's own entries (owner, 2026-11-21, after RC-7: "Prepare spells should also be in the Cast Spell section
// (wasn't it supposed to be called Magic)?", then "do the Magic rename and Prepare Spells now"): a feat the faithful
// Magic page shows in a slot of its own - Prepare Spells (1547, every 2024 caster's: #36) in slot 6, Spell Special
// Abilities' before the cleanup took it out - for a character with it; the class sections leave it out
// (RadialMovesClassFeat), so it is in one place. One entry for a multiclass: the Prepare spells window has its "Next
// class" button. None in NWN mode.
struct RadialMagicEntry {
  int feat;
  int slot;
  const char* name;  // for the log
};
const RadialMagicEntry* RadialMagicEntries(RuleVariant radial, size_t* count);
bool RadialMovesClassFeat(RuleVariant radial, int feat);
// Whether a Magic entry goes on a spell menu the game just built: one built as a page (`built` - the innate
// abilities' page builds none of its own entries), on your own ring (`on_you`) or the quickbar's Assign Spell
// (`assigning`), for a character with the feat, in a slot still empty.
bool RadialMagicEntryShows(bool built, bool on_you, bool assigning, bool has_feat, bool slot_free);

}  // namespace rules
