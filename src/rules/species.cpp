#include "rules/species.h"

namespace rules {

namespace {

struct Entry {
  uint32_t game;
  uint32_t nwn5e;
  StringRule rule;
};

// dialog.tlk strrefs (racialtypes.2da Name / NamePlural / ConverNameLower /
// Description; the Race page's title, help title and help; the hub's buttons)
// and nwn5e's strings for them.
constexpr Entry kSwaps[] = {
    {148, 16000260, StringRule::kSpecies},         // "Race" (the hub's button)
    {162, 16000261, StringRule::kSpecies},         // "Select a Race for your Character"
    {481, 16000262, StringRule::kSpecies},         // "Races" (help title)
    {485, 16000263, StringRule::kSpecies},         // the Race page's help
    {32, 16000264, StringRule::kSpecies},          // "Half-Orc" -> "Orc"
    {33, 16000265, StringRule::kSpecies},          // "Half-Orcs" -> "Orcs"
    {4922, 16000266, StringRule::kSpecies},        // "half-orc" -> "orc"
    {251, 16000270, StringRule::kSpecies},         // Dwarf
    {252, 16000271, StringRule::kSpecies},         // Elf
    {253, 16000272, StringRule::kSpecies},         // Gnome
    {254, 16000273, StringRule::kSpecies},         // Halfling
    {255, 16000274, StringRule::kSpecies},         // Half-Elf
    {256, 16000275, StringRule::kSpecies},         // Half-Orc (Orc)
    {257, 16000276, StringRule::kSpecies},         // Human
    {143, 16000280, StringRule::kAbilityScores},   // "Abilities" (the hub's button)
    {146, 16000434, StringRule::kStartingEquipment},  // "Packages" (the hub's button)
    {483, 16000434, StringRule::kStartingEquipment},  // "Description of Packages" (help title)
    {159, 16000435, StringRule::kStartingEquipment},  // "Configure Packages"
    {160, 16000436, StringRule::kStartingEquipment},  // "Select a Package for your Character"
    {487, 16000437, StringRule::kStartingEquipment},  // the Packages page's help
};

}  // namespace

std::optional<StringSwap> CreationStringSwap2024(uint32_t game_strref) {
  for (const Entry& entry : kSwaps) {
    if (entry.game == game_strref) return StringSwap{entry.nwn5e, entry.rule};
  }
  return std::nullopt;
}

}  // namespace rules
