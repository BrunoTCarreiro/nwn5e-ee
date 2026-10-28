#include "rules/weapons.h"

#include <ctype.h>

#include <algorithm>

namespace rules {

Mastery MasteryForNwnBaseItem(int base_item) {
  switch (base_item) {
    case 0:    // shortsword
    case 11:   // shortbow
    case 31:   // dart
    case 38:   // handaxe
    case 51:   // rapier
    case 59:   // shuriken (NWN only: like the dart)
    case 63:   // throwing axe (NWN only: like the handaxe)
      return Mastery::kVex;
    case 1:    // longsword
    case 3:    // bastard sword (NWN only: like the longsword)
    case 4:    // light flail (flail)
    case 9:    // light mace (mace)
    case 41:   // katana (NWN only: like the longsword)
    case 47:   // morningstar
    case 58:   // short spear (spear)
      return Mastery::kSap;
    case 2:    // battleaxe
    case 35:   // heavy flail (NWN only: like the maul)
    case 45:   // magic staff (quarterstaff)
    case 50:   // quarterstaff
    case 92:   // lance
    case 95:   // trident
    case 108:  // dwarven waraxe (NWN only: like the battleaxe)
      return Mastery::kTopple;
    case 5:    // warhammer
    case 6:    // heavy crossbow
      return Mastery::kPush;
    case 7:    // light crossbow
    case 8:    // longbow
    case 28:   // club
    case 61:   // sling
    case 111:  // whip
      return Mastery::kSlow;
    case 10:   // halberd
    case 12:   // two-bladed sword (NWN only)
    case 18:   // greataxe
    case 32:   // dire mace (NWN only)
    case 33:   // double axe (NWN only)
      return Mastery::kCleave;
    case 13:   // greatsword
    case 55:   // scythe (NWN only: like the glaive)
      return Mastery::kGraze;
    case 22:   // dagger
    case 37:   // light hammer
    case 40:   // kama (NWN only: like the sickle)
    case 42:   // kukri (NWN only: like the dagger)
    case 53:   // scimitar
    case 60:   // sickle
      return Mastery::kNick;
    default:
      return Mastery::kNone;
  }
}

bool HasWeaponMastery(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian:
    case CharacterClass::kFighter:
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger:
    case CharacterClass::kRogue:
      return true;
    default:
      return false;
  }
}

int OffHandDamageAbilityModifier(int ability_modifier, bool two_weapon_fighting_style) {
  if (ability_modifier < 0 || two_weapon_fighting_style) return ability_modifier;
  return 0;
}

bool IsLightNwnBaseItem(int base_item) {
  switch (base_item) {
    case 0:   // shortsword
    case 22:  // dagger
    case 28:  // club
    case 37:  // light hammer
    case 38:  // handaxe
    case 40:  // kama
    case 42:  // kukri
    case 53:  // scimitar
    case 60:  // sickle
      return true;
    default:
      return false;
  }
}

bool IsMonkWeapon2024(int base_item) {
  switch (base_item) {
    case 0:   // shortsword
    case 9:   // light mace
    case 22:  // dagger
    case 28:  // club
    case 37:  // light hammer
    case 38:  // handaxe
    case 40:  // kama
    case 42:  // kukri
    case 45:  // magic staff
    case 50:  // quarterstaff
    case 53:  // scimitar
    case 58:  // short spear
    case 60:  // sickle
      return true;
    default:
      return false;
  }
}

int OffHandAttacks(int nwn_attacks, bool double_weapon, bool dual_wielder, bool main_hand_light,
                   bool off_hand_light) {
  if (nwn_attacks <= 0) return 0;
  if (double_weapon || dual_wielder) return 1;
  return main_hand_light && off_hand_light ? 1 : 0;
}

int NwnMainHandStrengthDamage(int strength_modifier, bool two_handed) {
  return two_handed && strength_modifier > 0 ? strength_modifier * 3 / 2 : strength_modifier;
}

int NwnOffHandStrengthDamage(int strength_modifier) {
  return strength_modifier > 0 ? strength_modifier / 2 : strength_modifier;
}

int WeaponMasterySlots(CharacterClass character_class, int levels) {
  if (levels < 1) return 0;
  switch (character_class) {
    case CharacterClass::kFighter:
      return levels >= 16 ? 6 : levels >= 10 ? 5 : levels >= 4 ? 4 : 3;
    case CharacterClass::kBarbarian:
      return levels >= 10 ? 4 : levels >= 4 ? 3 : 2;
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger:
    case CharacterClass::kRogue:
      return 2;
    default:
      return 0;
  }
}

int WeaponMasterySlots(const std::vector<ClassLevels>& classes) {
  int slots = 0;
  for (const ClassLevels& entry : classes) slots = std::max(slots, WeaponMasterySlots(entry.character_class, entry.levels));
  return slots;
}

bool MasteryAllowed(const std::vector<ClassLevels>& classes, int base_item) {
  for (const ClassLevels& entry : classes) {
    if (WeaponMasterySlots(entry.character_class, entry.levels) == 0) continue;
    if (entry.character_class != CharacterClass::kBarbarian) return true;
    switch (base_item) {
      case 6: case 7:    // heavy, light crossbow
      case 8: case 11:   // longbow, shortbow
      case 31: case 59:  // dart, shuriken
      case 61:           // sling
        break;
      default:
        return true;
    }
  }
  return false;
}

namespace {

struct NwnWeapon {
  int base_item;
  const char* name;
  int weapon_focus_feat;  // GetWeaponFocus 0x6A5C20; -1 none
  std::vector<int> proficiency_feats;  // baseitems.2da ReqFeat0-4
};

// Proficiency feats: 44 exotic, 45 martial, 46 simple, 48 druid, 49 monk,
// 50 rogue, 51 wizard, 256 elf.
const std::vector<NwnWeapon>& Weapons() {
  static const std::vector<NwnWeapon> kWeapons = {
      {22, "Dagger", 90, {46, 48, 49, 50, 51}},     {28, "Club", 43, {46, 48, 49, 50, 51}},
      {9, "Light Mace", 94, {46, 50}},              {47, "Morningstar", 95, {46, 50}},
      {50, "Quarterstaff", 96, {46, 48, 49, 50, 51}}, {58, "Spear", 97, {46, 48}},
      {60, "Sickle", 98, {46, 48}},                 {7, "Light Crossbow", 93, {46, 49, 50, 51}},
      {6, "Heavy Crossbow", 92, {46, 49, 50, 51}},  {31, "Dart", 91, {46, 48, 50}},
      {61, "Sling", 99, {46, 48, 49, 50}},          {0, "Short Sword", 103, {45, 50}},
      {1, "Longsword", 106, {45, 256}},             {2, "Battleaxe", 110, {45}},
      {4, "Light Flail", 114, {45}},                {5, "Warhammer", 115, {45}},
      {10, "Halberd", 112, {45}},                   {13, "Greatsword", 107, {45}},
      {18, "Greataxe", 111, {45}},                  {35, "Heavy Flail", 116, {45}},
      {37, "Light Hammer", 113, {45}},              {38, "Handaxe", 108, {45, 49, 50}},
      {51, "Rapier", 104, {45, 50, 256}},           {53, "Scimitar", 105, {45, 48}},
      {63, "Throwing Axe", 109, {45}},              {95, "Trident", -1, {45}},
      {8, "Longbow", 101, {45, 256}},               {11, "Shortbow", 102, {45, 50, 256}},
      {92, "Lance", -1, {}},                        {3, "Bastard Sword", 123, {44}},
      {12, "Two-Bladed Sword", 127, {44}},          {32, "Dire Mace", 125, {44}},
      {33, "Double Axe", 126, {44}},                {40, "Kama", 117, {44, 49}},
      {41, "Katana", 122, {44}},                    {42, "Kukri", 118, {44}},
      {55, "Scythe", 121, {44}},                    {59, "Shuriken", 120, {44, 49}},
      {108, "Dwarven Waraxe", 952, {44}},           {111, "Whip", 993, {44}},
  };
  return kWeapons;
}

const NwnWeapon* FindWeapon(int base_item) {
  const int kind = MasteryKindOfNwnBaseItem(base_item);
  for (const NwnWeapon& weapon : Weapons()) {
    if (weapon.base_item == kind) return &weapon;
  }
  return nullptr;
}

std::string Squeeze(const std::string& text) {
  std::string out;
  for (char c : text) {
    if (c == ' ' || c == '-' || c == '_' || c == '\'') continue;
    out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

const std::vector<int>& NwnMasteryWeapons() {
  static const std::vector<int> kIds = [] {
    std::vector<int> ids;
    for (const NwnWeapon& weapon : Weapons()) ids.push_back(weapon.base_item);
    return ids;
  }();
  return kIds;
}

int MasteryKindOfNwnBaseItem(int base_item) { return base_item == 45 ? 50 : base_item; }

bool IsHeavyWeapon2024(int base_item) {
  switch (base_item) {
    case 6:    // heavy crossbow
    case 8:    // longbow
    case 10:   // halberd
    case 12:   // two-bladed sword (NWN only)
    case 13:   // greatsword
    case 18:   // greataxe
    case 32:   // dire mace (NWN only)
    case 33:   // double axe (NWN only)
    case 35:   // heavy flail (the maul)
    case 55:   // scythe (the glaive)
    case 92:   // lance
      return true;
    default:
      return false;
  }
}

int FinesseDamageAdjustment(int strength_modifier, int dexterity_modifier, bool off_hand,
                            bool off_hand_keeps_ability) {
  auto term = [&](int modifier) {
    if (!off_hand) return modifier;
    return modifier < 0 || off_hand_keeps_ability ? modifier : 0;
  };
  const int better = dexterity_modifier > strength_modifier ? dexterity_modifier : strength_modifier;
  return term(better) - term(strength_modifier);
}

int WeaponMasteryFeat(int base_item) {
  const int kind = MasteryKindOfNwnBaseItem(base_item);
  const std::vector<int>& kinds = NwnMasteryWeapons();
  for (size_t i = 0; i < kinds.size(); ++i) {
    if (kinds[i] == kind) return kWeaponMasteryFeatFirst + static_cast<int>(i);
  }
  return -1;
}

int WeaponKindOfMasteryFeat(int feat) {
  const std::vector<int>& kinds = NwnMasteryWeapons();
  const int index = feat - kWeaponMasteryFeatFirst;
  return index >= 0 && index < static_cast<int>(kinds.size()) ? kinds[index] : -1;
}

const char* NwnWeaponName(int base_item) {
  const NwnWeapon* weapon = FindWeapon(base_item);
  return weapon ? weapon->name : nullptr;
}

int NwnWeaponFromName(const std::string& text) {
  const std::string key = Squeeze(text);
  if (key.empty()) return -1;
  for (const NwnWeapon& weapon : Weapons()) {
    if (Squeeze(weapon.name) == key) return weapon.base_item;
  }
  static const struct {
    const char* alias;
    int base_item;
  } kAliases[] = {{"shortspear", 58}, {"staff", 50}, {"waraxe", 108}, {"twobladed", 12}, {"flail", 4},
                  {"mace", 9}, {"hammer", 5}, {"lightxbow", 7}, {"heavyxbow", 6}, {"crossbow", 7}};
  for (const auto& alias : kAliases) {
    if (key == alias.alias) return alias.base_item;
  }
  return -1;
}

const char* MasteryName(Mastery mastery) {
  switch (mastery) {
    case Mastery::kCleave: return "Cleave";
    case Mastery::kGraze: return "Graze";
    case Mastery::kNick: return "Nick";
    case Mastery::kPush: return "Push";
    case Mastery::kSap: return "Sap";
    case Mastery::kSlow: return "Slow";
    case Mastery::kTopple: return "Topple";
    case Mastery::kVex: return "Vex";
    default: return "none";
  }
}

int WeaponFocusFeatForNwnBaseItem(int base_item) {
  const NwnWeapon* weapon = FindWeapon(base_item);
  return weapon ? weapon->weapon_focus_feat : -1;
}

std::vector<int> ProficiencyFeatsForNwnBaseItem(int base_item) {
  if (base_item == 45) return {};  // magic staff: anyone
  const NwnWeapon* weapon = FindWeapon(base_item);
  return weapon ? weapon->proficiency_feats : std::vector<int>{};
}

int FreeMasterySlot(const std::vector<int>& chosen, int slots) {
  for (int i = 0; i < slots && i < static_cast<int>(chosen.size()); ++i) {
    if (chosen[i] < 0) return i;
  }
  return -1;
}

bool MasteryAttackHit(int nwn_attack_result) {
  return nwn_attack_result == 1 || nwn_attack_result == 3 || nwn_attack_result == 7 || nwn_attack_result == 10;
}

bool MasteryAttackMiss(int nwn_attack_result) {
  return nwn_attack_result == 2 || nwn_attack_result == 4 || nwn_attack_result == 8 || nwn_attack_result == 9;
}

bool IsFinesseNwnBaseItem(int base_item) {
  switch (base_item) {
    case 0:    // shortsword
    case 22:   // dagger
    case 31:   // dart
    case 40:   // kama
    case 42:   // kukri
    case 51:   // rapier
    case 53:   // scimitar
    case 111:  // whip
      return true;
    default:
      return false;
  }
}

int AttackAbilityModifier(bool ranged, bool weapon_finesse, bool finesse_weapon, int strength, int dexterity) {
  if (ranged) return dexterity;
  if (weapon_finesse && finesse_weapon && dexterity > strength) return dexterity;
  return strength;
}

int ToppleDc(int ability_modifier, int proficiency_bonus) { return 8 + ability_modifier + proficiency_bonus; }

int GrazeDamage(int ability_modifier) { return ability_modifier > 0 ? ability_modifier : 0; }

bool VexActive(uint32_t attacker_rounds_now, uint32_t attacker_rounds_at_hit) {
  return attacker_rounds_now <= attacker_rounds_at_hit + 1;
}

bool SapActive(uint32_t source_rounds_now, uint32_t source_rounds_at_hit) {
  return source_rounds_now == source_rounds_at_hit;
}

bool ResolveMasterySwap(const std::vector<int>& chosen, int slots, int a, int b, MasterySwap* swap) {
  const bool a_in = MasterySlotOf(chosen, slots, a) >= 0;
  const bool b_in = MasterySlotOf(chosen, slots, b) >= 0;
  if (a_in == b_in) return false;
  *swap = a_in ? MasterySwap{a, b} : MasterySwap{b, a};
  return true;
}

int MasterySlotOf(const std::vector<int>& chosen, int slots, int base_item) {
  for (int i = 0; i < slots && i < static_cast<int>(chosen.size()); ++i) {
    if (chosen[i] >= 0 && chosen[i] == MasteryKindOfNwnBaseItem(base_item)) return i;
  }
  return -1;
}

}  // namespace rules
