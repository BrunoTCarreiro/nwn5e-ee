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
  CHECK_EQ(count, 4u);
  CHECK_EQ(entries != nullptr, true);
  if (!entries || count != 4) return;
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

// #126: NWN's labels renamed in faithful - "Disengage" (stop attacking), Detect and Stealth Mode.
TEST(RadialLabelsRenamed) {
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 1057)) == "radial.stop_attacking", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4848)) == "radial.search", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4849)) == "radial.stop_searching", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4850)) == "radial.hide", true);
  CHECK_EQ(Text(RadialLabelKey(kFaithful, 4851)) == "radial.stop_hiding", true);
  for (uint32_t strref : {1056u, 1055u, 949u, 4852u, 4847u, 1058u, 0u}) {  // Attack, the pages, Parry Mode ...
    CHECK_EQ(RadialLabelKey(kFaithful, strref) == nullptr, true);
  }
  for (uint32_t strref : {1057u, 4848u, 4849u, 4850u, 4851u}) {
    CHECK_EQ(RadialLabelKey(kNwn, strref) == nullptr, true);
    CHECK_EQ(RadialLabelKey(kOff, strref) == nullptr, true);
  }
  size_t count = 99;
  const RadialLabel* labels = RadialLabels(kFaithful, &count);
  CHECK_EQ(count, 5u);
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
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0xef) ==
               "0 Character Sheet, 1 Cast Spell, 2 Attack, 3 Special Attacks, 5 Issue Command, 6 Special Abilities, "
               "7 Emote",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0x10) == "4 party / polymorph", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x4e) == "1 Cast Spell, 2 Move To, 3 More Actions, 6 Special Abilities",
           true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x91) == "0, 4 Jump All Players, 7 Jump to Point", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kAnotherCreature, 0xcf) == "0, 1, 2, 3, 6, 7", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYourAssociate, 0x01) == "0", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kYours, 0) == "none", true);
  CHECK_EQ(RadialRingSlots(RadialRing::kGround, 0x100) == "none", true);  // only slots 0-7
}
