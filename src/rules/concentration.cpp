#include "rules/concentration.h"

#include <algorithm>

namespace rules {

namespace {

// spells.2da rows whose spell is an SRD 5.2 Concentration spell (the SRD 5.2.1's Duration lines, CC-BY-4.0): a
// row's label, then the SRD spell. A sub-radial variant (Protection from Evil of PROTECTION_FROM_ALIGNMENT, a
// Polymorph form) carries its own row id on its effects, so the variants are listed too. NWN-only spells have none
// until #39; Endurance and Bigby's Crushing Hand have no exact SRD spell - they're kept with their families (Enhance
// Ability, Arcane Hand).
const std::vector<int> kSpells = {
       4,  // Bestow_Curse - Bestow Curse
       5,  // Blade_Barrier - Blade Barrier
       6,  // Bless - Bless
       9,  // Bulls_Strength - Enhance Ability
      11,  // Call_Lightning - Call Lightning
      13,  // Cats_Grace - Enhance Ability
      20,  // Clairaudience_and_Clairvoyance - Clairvoyance
      22,  // Cloak_of_Chaos - Holy Aura (a variant)
      23,  // Cloudkill - Cloudkill
      26,  // Confusion - Confusion
      36,  // Darkness - Darkness
      39,  // Delayed_Blast_Fireball - Delayed Blast Fireball
      43,  // Dominate_Animal - Dominate Beast
      44,  // Dominate_Monster - Dominate Monster
      45,  // Dominate_Person - Dominate Person
      49,  // Endurance - Enhance Ability; nearest SRD spell
      53,  // Entangle - Entangle
      54,  // Fear - Fear
      63,  // Gate - Gate
      65,  // Globe_of_Invulnerability - Globe of Invulnerability
      74,  // Greater_Stoneskin - Stoneskin
      75,  // Gust_of_Wind - Gust of Wind
      78,  // Haste - Haste
      82,  // Hold_Monster - Hold Monster
      83,  // Hold_Person - Hold Person
      84,  // Holy_Aura - Holy Aura (a variant)
      88,  // Improved_Invisibility - Greater Invisibility
      89,  // Incendiary_Cloud - Incendiary Cloud
      90,  // Invisibility - Invisibility
     113,  // Mass_Haste - Haste
     119,  // Minor_Globe_of_Invulnerability - Globe of Invulnerability
     123,  // Mordenkainens_Sword - Arcane Sword
     127,  // Phantasmal_Killer - Phantasmal Killer
     130,  // Polymorph_Self - Polymorph
     136,  // Protection__from_Chaos - Protection from Evil and Good (a variant)
     137,  // Protection_from_Elements - Protection from Energy
     138,  // Protection_from_Evil - Protection from Evil and Good (a variant)
     139,  // Protection_from_Good - Protection from Evil and Good (a variant)
     140,  // Protection_from_Law - Protection from Evil and Good (a variant)
     143,  // Ray_of_Enfeeblement - Ray of Enfeeblement
     151,  // Resistance - Resistance
     161,  // Shapechange - Shapechange
     162,  // Shield_of_Law - Holy Aura (a variant)
     163,  // Silence - Silence
     165,  // Sleep - Sleep
     166,  // Slow - Slow
     171,  // Stinking_Cloud - Stinking Cloud
     172,  // Stoneskin - Stoneskin
     173,  // Storm_of_Vengeance - Storm of Vengeance
     183,  // Sunbeam - Sunbeam
     187,  // Unholy_Aura - Holy Aura (a variant)
     188,  // Vampiric_Touch - Vampiric Touch
     191,  // Wall_of_Fire - Wall of Fire
     192,  // Web - Web
     193,  // Weird - Weird
     321,  // PROTECTION_FROM_ALIGNMENT - Protection from Evil and Good
     323,  // Aura_versus_Alignment - Holy Aura
     354,  // Eagle_Splendor - Enhance Ability
     355,  // Owls_Wisdom - Enhance Ability
     356,  // Foxs_Cunning - Enhance Ability
     375,  // Evards_Black_Tentacles - Black Tentacles
     387,  // Polymorph_GIANT_SPIDER - Polymorph (a variant)
     388,  // Polymorph_TROLL - Polymorph (a variant)
     389,  // Polymorph_UMBER_HULK - Polymorph (a variant)
     390,  // Polymorph_PIXIE - Polymorph (a variant)
     391,  // Polymorph_ZOMBIE - Polymorph (a variant)
     392,  // Shapechange_RED_DRAGON - Shapechange (a variant)
     393,  // Shapechange_FIRE_GIANT - Shapechange (a variant)
     394,  // Shapechange_BALOR - Shapechange (a variant)
     395,  // Shapechange_DEATH_SLAAD - Shapechange (a variant)
     396,  // Shapechange_IRON_GOLEM - Shapechange (a variant)
     426,  // Earthquake - Earthquake
     430,  // Banishment - Banishment
     449,  // Bane - Bane
     450,  // Shield_of_Faith - Shield of Faith
     454,  // Spike_Growth - Spike Growth
     456,  // Expeditious_Retreat - Expeditious Retreat
     457,  // Tashas_Hideous_Laughter - Hideous Laughter
     459,  // Bigbys_Interposing_Hand - Arcane Hand
     460,  // Bigbys_Forceful_Hand - Arcane Hand
     461,  // Bigbys_Grasping_Hand - Arcane Hand
     462,  // Bigbys_Clenched_Fist - Arcane Hand
     463,  // Bigbys_Crushing_Hand - Arcane Hand; nearest SRD spell
     485,  // Flesh_to_stone - Flesh to Stone
    1057,  // nwn5e_Guidance - Guidance (#39 build 2; its variants below carry their own rows)
    1060,  // nwn5e_Resistance_Acid - Resistance
    1061,  // nwn5e_Resistance_Bludgeoning - Resistance
    1062,  // nwn5e_Resistance_Cold - Resistance
    1063,  // nwn5e_Resistance_Fire - Resistance
    1064,  // nwn5e_Resistance_Lightning - Resistance
    1065,  // nwn5e_Resistance_Necrotic - Resistance
    1066,  // nwn5e_Resistance_Piercing - Resistance
    1067,  // nwn5e_Resistance_Poison - Resistance
    1068,  // nwn5e_Resistance_Radiant - Resistance
    1069,  // nwn5e_Resistance_Slashing - Resistance
    1070,  // nwn5e_Resistance_Thunder - Resistance
    1071,  // nwn5e_Guidance_Acrobatics - Guidance
    1072,  // nwn5e_Guidance_Animal_Handling - Guidance
    1073,  // nwn5e_Guidance_Arcana - Guidance
    1074,  // nwn5e_Guidance_Athletics - Guidance
    1075,  // nwn5e_Guidance_Deception - Guidance
    1076,  // nwn5e_Guidance_History - Guidance
    1077,  // nwn5e_Guidance_Insight - Guidance
    1078,  // nwn5e_Guidance_Intimidation - Guidance
    1079,  // nwn5e_Guidance_Investigation - Guidance
    1080,  // nwn5e_Guidance_Medicine - Guidance
    1081,  // nwn5e_Guidance_Nature - Guidance
    1082,  // nwn5e_Guidance_Perception - Guidance
    1083,  // nwn5e_Guidance_Performance - Guidance
    1084,  // nwn5e_Guidance_Persuasion - Guidance
    1085,  // nwn5e_Guidance_Religion - Guidance
    1086,  // nwn5e_Guidance_Sleight_of_Hand - Guidance
    1087,  // nwn5e_Guidance_Stealth - Guidance
    1088,  // nwn5e_Guidance_Survival - Guidance
    1131,  // nwn5e_Dancing_Lights - Dancing Lights (#39 build 4b)
    1132,  // nwn5e_Dancing_Lights_Lights - Dancing Lights (its Move, 1133, is no cast)
    1160,  // nwn5e_Heroism - Heroism (#39 2a build 1b)
    1162,  // nwn5e_Hunters_Mark - Hunter's Mark (#39 2a build 2a)
    1163,  // nwn5e_Shield_of_Faith_Bonus_Action - Shield of Faith (its Bonus Action's row)
    1166,  // nwn5e_Expeditious_Retreat_Bonus_Action - Expeditious Retreat (its Bonus Action's row)
    1167,  // nwn5e_Hunters_Mark_Bonus_Action - Hunter's Mark (its Bonus Action's row)
};

}  // namespace

int ConcentrationSaveDc(int damage) {
  if (damage <= 0) return 0;
  return std::min(kConcentrationMaxDc, std::max(kConcentrationMinDc, damage / 2));
}

bool ItemCastNeedsConcentration(int base_item) {
  return base_item != kBaseItemPotions && base_item != kBaseItemCraftedPotion;
}

bool IsConcentrationSpell(int spell) { return std::binary_search(kSpells.begin(), kSpells.end(), spell); }

const std::vector<int>& ConcentrationSpells() { return kSpells; }

}  // namespace rules
