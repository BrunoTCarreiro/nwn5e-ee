#include "check.h"
#include "data/text_catalog.h"
#include "rules/radial_cleanup.h"
#include "rules/skills.h"

#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace rules;

namespace {

constexpr RuleVariant kFaithful = RuleVariant::kFaithful;
constexpr RuleVariant kNwn = RuleVariant::kNwn;
constexpr RuleVariant kOff = RuleVariant::kOff;

std::string Text(const char* text) { return text ? text : "<null>"; }

std::string ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream bytes;
  bytes << file.rdbuf();
  return bytes.str();
}

}  // namespace

// #126 / #182: the Special Attacks page leaves out its 3e feats in faithful - Called Shot, Power Attack (slot 2),
// Improved Power Attack (slot 5), Disarm, and every feat behind More Special Attacks (so slot 7 goes too).
TEST(RadialSpecialAttacksHideThe3eFeats) {
  const RadialPage page = RadialPage::kSpecialAttacks;
  for (int feat : {5, 28, 19, 9, 16, 389, 390, 31, 30, 425, 39, 867}) {
    CHECK_EQ(RadialPageHidesFeat(kFaithful, page, feat), true);
    CHECK_EQ(RadialPageHidesFeat(kNwn, page, feat), false);
    CHECK_EQ(RadialPageHidesFeat(kOff, page, feat), false);
  }
  // Knockdown / Improved Knockdown are nwn5e's Shove; nwn5e's own actions; the Cast Spell page's feats.
  for (int feat : {23, 17, 1321, 1322, 1323, 1312, 1313, 1563, 11, 1001, 0}) {
    CHECK_EQ(RadialPageHidesFeat(kFaithful, page, feat), false);
  }
}

// #126: the Cast Spell page without the 3e Metamagic page and Epic Spells in faithful.
TEST(RadialCastSpellHidesMetamagicAndEpicSpells) {
  const RadialPage page = RadialPage::kCastSpell;
  for (int feat : {11, 12, 25, 29, 33, 37, 1001}) {
    CHECK_EQ(RadialPageHidesFeat(kFaithful, page, feat), true);
    CHECK_EQ(RadialPageHidesFeat(kNwn, page, feat), false);
  }
  for (int feat : {28, 19, 5, 39, 1000, 1002, 10}) CHECK_EQ(RadialPageHidesFeat(kFaithful, page, feat), false);
  // The Special Abilities page hides no feat (its class sections: RadialHidesClassFeat).
  for (int feat : {11, 28, 413, 1089}) {
    CHECK_EQ(RadialPageHidesFeat(kFaithful, RadialPage::kSpecialAbilities, feat), false);
  }
}

// #126: the entries a page always adds - Parry, Crafting Skills, Taunt, Spell Special Abilities - left out in
// faithful, found by their icons; none in NWN mode.
TEST(RadialFixedEntriesLeftOut) {
  size_t count = 99;
  const RadialFixedEntry* entries = RadialFixedEntries(kFaithful, &count);
  CHECK_EQ(count, 5u);
  CHECK_EQ(entries != nullptr, true);
  if (!entries || count != 5) return;
  CHECK_EQ(static_cast<int>(entries[0].page), static_cast<int>(RadialPage::kSpecialAttacks));
  CHECK_EQ(entries[0].slot, 0);
  CHECK_EQ(Text(entries[0].icon) == "isk_parry", true);
  CHECK_EQ(entries[0].sub_page, false);
  CHECK_EQ(static_cast<int>(entries[1].page), static_cast<int>(RadialPage::kSpecialAbilities));
  CHECK_EQ(entries[1].slot, 0);
  CHECK_EQ(Text(entries[1].icon) == "ir_craft", true);
  CHECK_EQ(entries[1].sub_page, false);
  CHECK_EQ(static_cast<int>(entries[2].page), static_cast<int>(RadialPage::kSpecialAbilities));
  CHECK_EQ(entries[2].slot, 1);
  CHECK_EQ(Text(entries[2].icon) == "isk_taunt", true);
  CHECK_EQ(entries[2].sub_page, false);
  CHECK_EQ(static_cast<int>(entries[3].page), static_cast<int>(RadialPage::kCastSpell));
  CHECK_EQ(entries[3].slot, 6);
  CHECK_EQ(Text(entries[3].icon) == "IR_SPELL_ABS", true);
  CHECK_EQ(entries[3].sub_page, true);
  // The quickbar's Assign Barter (RadialBarterShows: no one to trade with).
  CHECK_EQ(static_cast<int>(entries[4].page), static_cast<int>(RadialPage::kAssignOtherActions));
  CHECK_EQ(entries[4].slot, 7);
  CHECK_EQ(Text(entries[4].icon) == "IR_BARTER", true);
  CHECK_EQ(entries[4].sub_page, false);
  CHECK_EQ(RadialPageHidesFeat(kFaithful, RadialPage::kAssignOtherActions, 28), false);
  count = 99;
  CHECK_EQ(RadialFixedEntries(kNwn, &count) == nullptr, true);
  CHECK_EQ(count, 0u);
  count = 99;
  CHECK_EQ(RadialFixedEntries(kOff, &count) == nullptr, true);
  CHECK_EQ(count, 0u);
}

// #126: the class sections' NWN-only abilities hidden in faithful; the ones with 2024 versions coming stay.
TEST(RadialClassSectionsHideNwnOnlyFeats) {
  for (int feat : {413, 414, 302, 304, 199, 303, 869, 871, 873}) {
    CHECK_EQ(RadialHidesClassFeat(kFaithful, feat), true);
    CHECK_EQ(RadialHidesClassFeat(kNwn, feat), false);
    CHECK_EQ(RadialHidesClassFeat(kOff, feat), false);
  }
  // Smite Evil, Lay on Hands, Summon Mount, Bard Song, Wild Shape, Turn Undead, Rage, Flurry, Stunning Fist (the
  // Monk's Stunning Strike), Wholeness of Body, Quivering Palm, the horse menu, a player tool, nwn5e's rows.
  for (int feat : {301, 299, 1095, 257, 305, 294, 293, 204, 39, 211, 296, 1089, 1106, 1325, 1312}) {
    CHECK_EQ(RadialHidesClassFeat(kFaithful, feat), false);
  }
}

// #126: NWN's labels renamed in faithful - "Disengage" (stop attacking), Detect and Stealth Mode, Cast Spell and
// Assign Spell (the owner's "Spells", "Assign Spells").
TEST(RadialLabelsRenamed) {
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 1057)) == "radial.stop_attacking", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4848)) == "radial.search", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4849)) == "radial.stop_searching", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4850)) == "radial.hide", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4851)) == "radial.stop_hiding", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 557)) == "radial.spells", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 1158)) == "radial.assign_spells", true);
  // Attack, the pages, Parry Mode ...; the spell list's "Spells" (2295), the quickbar's other assign entries.
  for (uint32_t strref : {1056u, 1055u, 949u, 4852u, 4847u, 1058u, 0u, 2295u, 556u, 558u, 1157u, 1159u, 1155u}) {
    CHECK_EQ(RadialLabelKey(kFaithful, strref) == nullptr, true);
  }
  for (uint32_t strref : {1057u, 4848u, 4849u, 4850u, 4851u, 557u, 1158u}) {
    CHECK_EQ(RadialLabelKey(kNwn, strref) == nullptr, true);
    CHECK_EQ(RadialLabelKey(kOff, strref) == nullptr, true);
  }
  size_t count = 99;
  const RadialLabel* labels = RadialLabels(kFaithful, &count);
  CHECK_EQ(count, 7u);
  for (size_t i = 0; labels && i < count; ++i) {
    CHECK_EQ(Text(RadialLabelKey(kFaithful, labels[i].strref)) == Text(labels[i].key), true);
  }
  count = 99;
  CHECK_EQ(RadialLabels(kNwn, &count) == nullptr, true);
  CHECK_EQ(count, 0u);
}

// #126: each renamed label's text is in data/text/en.txt - the SRD's action names, nwn5e's others (generated).
TEST(RadialLabelTextsInTheCatalogue) {
  data::TextCatalog catalog;
  std::string error;
  CHECK_EQ(data::ParseTextCatalog(ReadFile("data/text/en.txt"), &catalog, &error), true);
  const auto text = [&](const char* key) {
    const auto it = catalog.find(key);
    return it == catalog.end() ? std::string("<missing>") : it->second.text;
  };
  const auto source = [&](const char* key) {
    const auto it = catalog.find(key);
    return it == catalog.end() ? std::string("<missing>") : it->second.source;
  };
  CHECK_EQ(text("radial.stop_attacking") == "Stop Attacking", true);
  CHECK_EQ(text("radial.search") == "Search", true);
  CHECK_EQ(source("radial.search") == "srd:Search", true);
  CHECK_EQ(text("radial.hide") == "Hide", true);
  CHECK_EQ(source("radial.hide") == "srd:Hide", true);
  CHECK_EQ(text("radial.stop_searching") == "Stop Searching", true);
  CHECK_EQ(text("radial.stop_hiding") == "Stop Hiding", true);
  CHECK_EQ(text("radial.spells") == "Spells", true);
  CHECK_EQ(source("radial.spells") == "owner", true);
  CHECK_EQ(text("radial.assign_spells") == "Assign Spells", true);
  CHECK_EQ(source("radial.assign_spells") == "owner", true);
  CHECK_EQ(catalog.count("radial.magic"), 0u);
  size_t count = 0;
  const RadialLabel* labels = RadialLabels(kFaithful, &count);
  for (size_t i = 0; labels && i < count; ++i) CHECK_EQ(catalog.count(labels[i].key), 1u);
}

// #126: another creature's ring - Pick Pocket (282) and Animal Empathy (269) already show their SRD skill names with
// the skills faithful (rules_labels.cpp swaps the skill names wherever they show), so the radial adds nothing.
TEST(RadialOtherRingSkillNames) {
  CHECK_EQ(Text(Faithful2024SkillLabelForStrref(282)) == "Sleight of Hand", true);
  CHECK_EQ(Text(Faithful2024SkillLabelForStrref(269)) == "Animal Handling", true);
  CHECK_EQ(RadialLabelKey(kFaithful, 282) == nullptr, true);
  CHECK_EQ(RadialLabelKey(kFaithful, 269) == nullptr, true);
}

// 0.99.111's first test: whose ring opened - the ground's has no Special Attacks (the log now says which).
TEST(RadialRingOf) {
  const uint32_t player = 0x7fffffff;
  CHECK_EQ(static_cast<int>(RadialRingOf(player, kRadialNoObject, player)), static_cast<int>(RadialRing::kYours));
  CHECK_EQ(static_cast<int>(RadialRingOf(0x2f2, player, player)), static_cast<int>(RadialRing::kYourAssociate));
  CHECK_EQ(static_cast<int>(RadialRingOf(0x2f2, 0x300, player)), static_cast<int>(RadialRing::kAnotherCreature));
  CHECK_EQ(static_cast<int>(RadialRingOf(0x2f2, kRadialNoObject, player)),
           static_cast<int>(RadialRing::kAnotherCreature));
  CHECK_EQ(static_cast<int>(RadialRingOf(kRadialNoObject, kRadialNoObject, player)),
           static_cast<int>(RadialRing::kGround));
  // No player creature (its id OBJECT_INVALID): a creature with no master isn't its associate; the ground stays.
  CHECK_EQ(static_cast<int>(RadialRingOf(0x2f2, kRadialNoObject, kRadialNoObject)),
           static_cast<int>(RadialRing::kAnotherCreature));
  CHECK_EQ(static_cast<int>(RadialRingOf(kRadialNoObject, kRadialNoObject, kRadialNoObject)),
           static_cast<int>(RadialRing::kGround));
  CHECK_EQ(Text(RadialRingName(RadialRing::kYours)) == "your", true);
  CHECK_EQ(Text(RadialRingName(RadialRing::kYourAssociate)) == "your associate's", true);
  CHECK_EQ(Text(RadialRingName(RadialRing::kAnotherCreature)) == "another creature's", true);
  CHECK_EQ(Text(RadialRingName(RadialRing::kGround)) == "the ground's", true);
}

TEST(RadialRingSlots) {
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0xef, kNwn) ==
               "0 Character Sheet, 1 Cast Spell, 2 Attack, 3 Special Attacks, 5 Issue Command, 6 Special Abilities, "
               "7 Emote",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0x10, kNwn) == "4 party / polymorph", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x4e, kNwn) ==
               "1 Cast Spell, 2 Move To, 3 More Actions, 6 Special Abilities",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x91, kNwn) == "0, 4 Jump All Players, 7 Jump to Point", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kAnotherCreature, 0xcf, kNwn) == "0, 1, 2, 3, 6, 7", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYourAssociate, 0x01, kNwn) == "0", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0, kNwn) == "none", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x100, kNwn) == "none", true);  // only slots 0-7
  // The radial faithful: slot 1 is the owner's "Spells" (Cast Spell renamed).
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0x0f, kFaithful) ==
               "0 Character Sheet, 1 Spells, 2 Attack, 3 Special Attacks",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x4e, kFaithful) ==
               "1 Spells, 2 Move To, 3 More Actions, 6 Special Abilities",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0x0f, kOff) ==
               "0 Character Sheet, 1 Cast Spell, 2 Attack, 3 Special Attacks",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kAnotherCreature, 0x02, kFaithful) == "1", true);
}

// #126 (owner, 2026-11-21: "do the Magic rename and Prepare Spells now"): Prepare Spells on the Spells page (Cast
// Spell, renamed), slot 6, out of the class sections; nothing in NWN mode.
TEST(RadialSpellsPrepareSpells) {
  size_t count = 99;
  const RadialSpellsEntry* entries = RadialSpellsEntries(kFaithful, &count);
  CHECK_EQ(count, 1u);
  CHECK_EQ(entries != nullptr, true);
  if (entries && count == 1) {
    CHECK_EQ(entries[0].feat, 1547);
    CHECK_EQ(entries[0].slot, 6);
    CHECK_EQ(Text(entries[0].name) == "Prepare Spells", true);
  }
  count = 99;
  CHECK_EQ(RadialSpellsEntries(kNwn, &count) == nullptr, true);
  CHECK_EQ(count, 0u);
  count = 99;
  CHECK_EQ(RadialSpellsEntries(kOff, &count) == nullptr, true);
  CHECK_EQ(count, 0u);
  CHECK_EQ(RadialMovesClassFeat(kFaithful, 1547), true);
  CHECK_EQ(RadialMovesClassFeat(kNwn, 1547), false);
  CHECK_EQ(RadialMovesClassFeat(kOff, 1547), false);
  for (int feat : {294, 1546, 1548, 413, 0}) CHECK_EQ(RadialMovesClassFeat(kFaithful, feat), false);
  // Moved, not hidden as NWN-only: the two lists stay apart.
  CHECK_EQ(RadialHidesClassFeat(kFaithful, 1547), false);
}

TEST(RadialSpellsEntryShows) {
  CHECK_EQ(RadialSpellsEntryShows(true, true, false, true, true), true);   // your Spells page
  CHECK_EQ(RadialSpellsEntryShows(true, false, true, true, true), true);   // the quickbar's Assign Spells
  CHECK_EQ(RadialSpellsEntryShows(true, true, true, true, true), true);
  CHECK_EQ(RadialSpellsEntryShows(true, false, false, true, true), false);  // another creature's, the ground's
  CHECK_EQ(RadialSpellsEntryShows(false, true, false, true, true), false);  // the innate abilities' page
  CHECK_EQ(RadialSpellsEntryShows(true, true, false, false, true), false);  // no Prepare Spells
  CHECK_EQ(RadialSpellsEntryShows(true, true, false, true, false), false);  // slot 6 taken
  CHECK_EQ(RadialSpellsEntryShows(true, false, true, false, true), false);
}

// #126 (owner, 2026-11-22: "Leave the barter then but it probably should only appear when showing the radial menu on
// another player?"): Barter on another player character's ring alone in faithful; the game's in NWN mode.
TEST(RadialBarterShows) {
  CHECK_EQ(RadialBarterShows(kFaithful, BarterTarget::kAnotherPlayer), true);
  CHECK_EQ(RadialBarterShows(kFaithful, BarterTarget::kNonPlayer), false);
  CHECK_EQ(RadialBarterShows(kFaithful, BarterTarget::kNoTarget), false);
  CHECK_EQ(RadialBarterShows(kNwn, BarterTarget::kAnotherPlayer), true);
  CHECK_EQ(RadialBarterShows(kNwn, BarterTarget::kNonPlayer), false);  // the game's ring: Talk To / Loot there
  CHECK_EQ(RadialBarterShows(kNwn, BarterTarget::kNoTarget), true);    // the game's quickbar
  CHECK_EQ(RadialBarterShows(kOff, BarterTarget::kNoTarget), true);
  // The table and the rule agree: the quickbar's Assign Barter is left out exactly when Barter isn't shown there.
  for (RuleVariant mode : {kFaithful, kNwn, kOff}) {
    size_t count = 0;
    const RadialFixedEntry* entries = RadialFixedEntries(mode, &count);
    bool listed = false;
    for (size_t i = 0; entries && i < count; ++i) {
      if (entries[i].page == RadialPage::kAssignOtherActions && Text(entries[i].icon) == "IR_BARTER") listed = true;
    }
    CHECK_EQ(listed, !RadialBarterShows(mode, BarterTarget::kNoTarget));
  }
}
