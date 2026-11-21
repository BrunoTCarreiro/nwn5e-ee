#include "rules/radial_cleanup.h"

namespace rules {

namespace {

bool Faithful(RuleVariant radial) { return radial == RuleVariant::kFaithful; }

// The Special Attacks page's 3e feats (its builder, 995090 in 89.8193.37, checks each on the character).
constexpr int kSpecialAttacksFeats[] = {
    5,    // Called Shot: Arm (slot 1) and Called Shot: Leg (slot 3)
    28,   // Power Attack Mode (slot 2)
    19,   // Improved Power Attack Mode (slot 5, #182)
    9,    // Disarm (slot 6)
    16,   // Improved Disarm (slot 6)
    389,  // More Special Attacks (slot 7): Expertise Mode
    390,  //   Improved Expertise Mode
    31,   //   Sap
    30,   //   Rapid Shot Mode
    425,  //   Dirty Fighting Mode
    39,   //   Stunning Fist (the Monk's class section keeps it: Stunning Strike)
    867,  //   Whirlwind Attack
};

// The Cast Spell page's (the spell menu's constructor, 9a10e0).
constexpr int kCastSpellFeats[] = {
    11, 12, 25, 29, 33, 37,  // the 3e Metamagic page: Empower, Extend, Maximize, Quicken, Silent, Still Spell
    1001,                    // Epic Spells: Epic Character
};

constexpr RadialFixedEntry kFixedEntries[] = {
    {RadialPage::kSpecialAttacks, 0, "isk_parry", false, "Parry Mode"},
    {RadialPage::kSpecialAbilities, 0, "ir_craft", false, "Crafting Skills"},
    {RadialPage::kSpecialAbilities, 1, "isk_taunt", false, "Taunt"},
    {RadialPage::kCastSpell, 6, "IR_SPELL_ABS", true, "Spell Special Abilities (Counterspell, Defensive Casting Mode)"},
};

constexpr int kClassFeats[] = {
    413,  // Divine Might (Cleric, Paladin)
    414,  // Divine Shield (Cleric, Paladin)
    302,  // Remove Disease (Paladin)
    304,  // Elemental Shape (Druid)
    199,  // Animal Companion (Druid, Ranger)
    303,  // Summon Familiar (Sorcerer, Wizard: the Find Familiar spell)
    869,  // Mighty Rage (Barbarian, epic)
    871,  // Curse Song (Bard, epic)
    873,  // Epic Dragon Shape (Druid, epic)
};

constexpr RadialLabel kLabels[] = {
    {1057, "radial.stop_attacking", "Disengage"},
    {4848, "radial.search", "Detect Mode"},
    {4849, "radial.stop_searching", "Cancel Detect Mode"},
    {4850, "radial.hide", "Stealth Mode"},
    {4851, "radial.stop_hiding", "Cancel Stealth Mode"},
};

template <size_t N>
bool Listed(const int (&list)[N], int feat) {
  for (int listed : list) {
    if (listed == feat) return true;
  }
  return false;
}

}  // namespace

bool RadialPageHidesFeat(RuleVariant radial, RadialPage page, int feat) {
  if (!Faithful(radial)) return false;
  switch (page) {
    case RadialPage::kSpecialAttacks:
      return Listed(kSpecialAttacksFeats, feat);
    case RadialPage::kCastSpell:
      return Listed(kCastSpellFeats, feat);
    case RadialPage::kSpecialAbilities:
      return false;  // its feat entries are the class sections' (RadialHidesClassFeat) and the horse menu (kept)
  }
  return false;
}

const RadialFixedEntry* RadialFixedEntries(RuleVariant radial, size_t* count) {
  const bool faithful = Faithful(radial);
  if (count) *count = faithful ? sizeof(kFixedEntries) / sizeof(kFixedEntries[0]) : 0;
  return faithful ? kFixedEntries : nullptr;
}

bool RadialHidesClassFeat(RuleVariant radial, int feat) { return Faithful(radial) && Listed(kClassFeats, feat); }

const RadialLabel* RadialLabels(RuleVariant radial, size_t* count) {
  const bool faithful = Faithful(radial);
  if (count) *count = faithful ? sizeof(kLabels) / sizeof(kLabels[0]) : 0;
  return faithful ? kLabels : nullptr;
}

const char* RadialLabelKey(RuleVariant radial, uint32_t strref) {
  if (!Faithful(radial)) return nullptr;
  for (const RadialLabel& label : kLabels) {
    if (label.strref == strref) return label.key;
  }
  return nullptr;
}

}  // namespace rules
