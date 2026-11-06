#include "rules/material_components.h"

#include <algorithm>

namespace rules {

namespace {

struct Entry {
  int spell;
  int needed;
  int consumed;
};

// {spells.2da row, GP on hand, GP consumed}: SRD 5.2.1's costed material components (CC-BY-4.0) - the row's label,
// then the SRD spell. A sub-radial variant (Protection from Evil of PROTECTION_FROM_ALIGNMENT, a Shapechange form)
// is listed with its master's cost. Legend Lore: incense worth 250 GP, consumed, and four ivory strips worth 50 GP
// each, kept. Create Undead's 150 GP is per corpse; NWN's raises one.
const std::vector<Entry> kCosts = {
    {  6,    5,    0},  // Bless - Bless
    { 18,  500,    0},  // Circle_of_Death - Circle of Death
    { 20,  100,    0},  // Clairaudience_and_Clairvoyance - Clairvoyance
    { 22, 1000,    0},  // Cloak_of_Chaos - Holy Aura (a variant)
    { 29,  150,    0},  // Create_Greater_Undead - Create Undead
    { 30,  150,    0},  // Create_Undead - Create Undead
    { 63, 5000,    0},  // Gate - Gate
    { 69, 1000, 1000},  // Greater_Planar_Binding - Planar Binding
    { 70,  100,  100},  // Greater_Restoration - Greater Restoration
    { 74,  100,  100},  // Greater_Stoneskin - Stoneskin
    { 84, 1000,    0},  // Holy_Aura - Holy Aura (a variant)
    { 86,  100,    0},  // Identify - Identify
    { 96, 1000, 1000},  // Lesser_Planar_Binding - Planar Binding
    {103,  100,  100},  // Magic_Circle_against_Chaos - Magic Circle (a variant)
    {104,  100,  100},  // Magic_Circle_against_Evil - Magic Circle (a variant)
    {105,  100,  100},  // Magic_Circle_against_Good - Magic Circle (a variant)
    {106,  100,  100},  // Magic_Circle_against_Law - Magic Circle (a variant)
    {123,  250,    0},  // Mordenkainens_Sword - Arcane Sword
    {128, 1000, 1000},  // Planar_Binding - Planar Binding
    {136,   25,   25},  // Protection__from_Chaos - Protection from Evil and Good (a variant)
    {138,   25,   25},  // Protection_from_Evil - Protection from Evil and Good (a variant)
    {139,   25,   25},  // Protection_from_Good - Protection from Evil and Good (a variant)
    {140,   25,   25},  // Protection_from_Law - Protection from Evil and Good (a variant)
    {142,  500,  500},  // Raise_Dead - Raise Dead
    {153, 1000, 1000},  // Resurrection - Resurrection
    {161, 1500,    0},  // Shapechange - Shapechange
    {162, 1000,    0},  // Shield_of_Law - Holy Aura (a variant)
    {172,  100,  100},  // Stoneskin - Stoneskin
    {186,   25,   25},  // True_Seeing - True Seeing
    {187, 1000,    0},  // Unholy_Aura - Holy Aura (a variant)
    {321,   25,   25},  // PROTECTION_FROM_ALIGNMENT - Protection from Evil and Good
    {322,  100,  100},  // Magic_Circle_against_Alignment - Magic Circle
    {323, 1000,    0},  // Aura_versus_Alignment - Holy Aura
    {363, 1000, 1000},  // Awaken - Awaken
    {376,  450,  250},  // Legend_Lore - Legend Lore
    {392, 1500,    0},  // Shapechange_RED_DRAGON - Shapechange (a variant)
    {393, 1500,    0},  // Shapechange_FIRE_GIANT - Shapechange (a variant)
    {394, 1500,    0},  // Shapechange_BALOR - Shapechange (a variant)
    {395, 1500,    0},  // Shapechange_DEATH_SLAAD - Shapechange (a variant)
    {396, 1500,    0},  // Shapechange_IRON_GOLEM - Shapechange (a variant)
    {419,   50,   50},  // Continual_Flame - Continual Flame
    {549,  200,  200},  // Glyph_of_Warding - Glyph of Warding
};

const Entry* Find(int spell) {
  const auto at = std::lower_bound(kCosts.begin(), kCosts.end(), spell,
                                   [](const Entry& entry, int id) { return entry.spell < id; });
  return at != kCosts.end() && at->spell == spell ? &*at : nullptr;
}

}  // namespace

ComponentCost CostlyComponent(int spell) {
  const Entry* entry = Find(spell);
  return entry ? ComponentCost{entry->needed, entry->consumed} : ComponentCost{};
}

bool ComponentAffordable(int gold, const ComponentCost& cost) { return gold >= cost.needed; }

const std::vector<int>& CostlySpells() {
  static const std::vector<int> spells = [] {
    std::vector<int> out;
    for (const Entry& entry : kCosts) out.push_back(entry.spell);
    return out;
  }();
  return spells;
}

}  // namespace rules
