#include "rules/spells2024.h"

#include <algorithm>
#include <cmath>

#include "rules/cantrips.h"

namespace rules {

namespace {

constexpr uint32_t kDamageTypePiercing = 2;

Spell2024 Damage(int row, const char* name, const char* script, SpellEffect2024 effect, int dice, int die,
                 int upcast_dice, uint32_t type) {
  Spell2024 spell;
  spell.row = row;
  spell.name = name;
  spell.script = script;
  spell.effect = effect;
  spell.dice = dice;
  spell.die = die;
  spell.upcast_dice = upcast_dice;
  spell.damage_type = type;
  return spell;
}

Spell2024 Saved(Spell2024 spell, Ability save, bool half) {
  spell.save = save;
  spell.half_on_save = half;
  return spell;
}

Spell2024 Area(Spell2024 spell, SpellShape2024 shape, int feet) {
  spell.shape = shape;
  spell.feet = feet;
  return spell;
}

Spell2024 Touch(Spell2024 spell) {
  spell.melee = true;
  return spell;
}

std::vector<Spell2024> Build() {
  using E = SpellEffect2024;
  std::vector<Spell2024> spells;
  spells.push_back(Area(Saved(Damage(kBurningHands, "Burning Hands", "n5_s_burnhands", E::kSave, 3, 6, 1,
                                     kDamageTypeFire),
                              Ability::kDexterity, true),
                        SpellShape2024::kCone, 15));
  {
    Spell2024 cure = Touch(Damage(kCureWounds, "Cure Wounds", "n5_s_curewounds", E::kHealing, 2, 8, 2, 0));
    spells.push_back(cure);
  }
  {
    Spell2024 missile = Damage(kMagicMissile, "Magic Missile", "n5_s_magicmiss", E::kDarts, 1, 4, 0, kDamageTypeForce);
    missile.flat = 1;
    spells.push_back(missile);
  }
  spells.push_back(Touch(Saved(Damage(kInflictWounds, "Inflict Wounds", "n5_s_inflict", E::kSave, 2, 10, 1,
                                      kDamageTypeNecrotic),
                               Ability::kConstitution, true)));
  spells.push_back(Area(Saved(Damage(kThunderwave, "Thunderwave", "n5_s_thunderwave", E::kSave, 2, 8, 1,
                                     kDamageTypeThunder),
                              Ability::kConstitution, true),
                        SpellShape2024::kCubeFromCaster, 15));
  spells.push_back(Damage(kIceKnife, "Ice Knife", "n5_s_iceknife", E::kSpellAttack, 1, 10, 0, kDamageTypePiercing));
  spells.push_back(Damage(kGuidingBolt, "Guiding Bolt", "n5_s_guidbolt", E::kSpellAttack, 4, 6, 1, kDamageTypeRadiant));
  spells.push_back(Damage(kRayOfSickness, "Ray of Sickness", "n5_s_raysick", E::kSpellAttack, 2, 8, 1,
                          kDamageTypePoison));
  {
    Spell2024 life = Damage(kFalseLife, "False Life", "n5_s_falselife", E::kTemporaryHitPoints, 2, 4, 0, 0);
    life.flat = 4;
    life.upcast_flat = 5;
    spells.push_back(life);
  }
  spells.push_back(Saved(Damage(kDissonantWhispers, "Dissonant Whispers", "n5_s_dissonant", E::kSave, 3, 6, 1,
                                kDamageTypePsychic),
                         Ability::kWisdom, true));
  std::sort(spells.begin(), spells.end(), [](const Spell2024& a, const Spell2024& b) { return a.row < b.row; });
  return spells;
}

}  // namespace

const std::vector<Spell2024>& Spells2024() {
  static const std::vector<Spell2024> spells = Build();
  return spells;
}

const Spell2024* Spell2024Of(int row) {
  for (const Spell2024& spell : Spells2024()) {
    if (spell.row == row) return &spell;
  }
  return nullptr;
}

int CastSlotLevel(int spell_level, int chosen) {
  const int own = std::max(1, spell_level);
  if (chosen <= own) return own;
  return std::min(9, chosen);
}

int DiceAtSlot(int dice, int per_level, int spell_level, int slot_level) {
  return dice + per_level * std::max(0, slot_level - spell_level);
}

int SpellDiceAt(const Spell2024& spell, int slot_level) {
  return DiceAtSlot(spell.dice, spell.upcast_dice, spell.level, slot_level);
}

int CriticalDice(int dice, bool critical) { return critical ? 2 * dice : dice; }

int DamageAfterSave(int damage, bool saved, bool half_on_save) {
  if (damage <= 0) return 0;
  if (!saved) return damage;
  return half_on_save ? damage / 2 : 0;
}

int MagicMissileDarts(int slot_level) { return 3 + std::max(0, slot_level - 1); }

int FalseLifeTemporaryHitPoints(int roll, int slot_level) { return roll + 4 + 5 * std::max(0, slot_level - 1); }

bool TemporaryHitPointsReplace(int current, int offered) { return offered > current; }

int IceKnifeExplosionDice(int slot_level) { return DiceAtSlot(2, 1, 1, slot_level); }

bool InCone(float along, float side, float length, float slack) {
  if (along < -slack || along > length + slack) return false;
  // 2024's Cone: as wide as it is far from its point of origin - half that to each side.
  return std::fabs(side) <= std::max(0.0f, along) / 2.0f + slack;
}

bool InCubeFromCaster(float along, float side, float edge, float slack) {
  if (along < -slack || along > edge + slack) return false;
  return std::fabs(side) <= edge / 2.0f + slack;
}

}  // namespace rules
