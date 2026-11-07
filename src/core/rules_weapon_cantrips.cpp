// #39 Part 1, build 3b: the weapon cantrips (docs/PLAN.md "Phase 4"; owner, 2026-11-07: "1 A, 2 A, 3 A, 4 A and
// while Produce Flame is active the character is also a light source, 5 A", then "build 3b"), `spellcasting`
// faithful. Both work through the game's own weapon attacks - the weapon's magic, its on-hit properties, Weapon
// Mastery and Sneak Attack stay - with nwn5e's attack, damage and round hooks asking here (weapon_cantrips.h).
// - Shillelagh 1099 (the Druid's; its radial menu Force 1100 or Bludgeoning 1101, the weapon's own): cast as a Bonus
//   Action (Q1 A: through feats 1549 / 1550, rules_cantrips.cpp's guard), with a Club or Quarterstaff (or NWN's magic
//   staff) in the hand - refused without one. For 1 minute (a marker effect with its icon, carrying the row cast;
//   0.92.1: applied through DelayCommand, after the feat's spell script - the 0.92.0 marker, applied inside it,
//   never showed): that weapon's melee attacks use the better of Wisdom and the term they had (Strength, or
//   Finesse's Dexterity) for attack and damage; its damage die is a d8 (d10 at 5, d12 at 11, 2d6 at 17; a Monk's
//   Martial Arts die when higher); Force turns the weapon's damage to Force. Letting go of the weapon (another in
//   the hand, or none) ends it, as casting it again does.
// - True Strike (NWN's 415 rewritten; Bard, Sorcerer, Wizard; Q2 A, Q3 A): cast at a creature with a weapon in the
//   hand, the cast becomes an attack order at it (no cast, no casting animation). Its round - the next one at that
//   creature, within about two rounds - has that one attack: no other main-hand, off-hand, Martial Arts or Flurry
//   attack (an Action Surge's still come). The attack uses the spellcasting ability of the class it was cast from
//   (an item's: the best of Intelligence, Wisdom and Charisma) in place of Strength or Dexterity, for attack and
//   damage; on a hit Radiant turns the weapon's damage to Radiant, and from level 5 it deals 1d6 Radiant more (2d6
//   at 11, 3d6 at 17), doubled on a Critical Hit. Melee or ranged.
// NWN mode: NWN's (Shillelagh isn't NWN's; its True Strike a level 1 buff).

#include "weapon_cantrips.h"

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "action_guard.h"
#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "resolve_damage.h"
#include "script_hooks.h"
#include "spell_impact.h"
#include "rules/cantrips.h"
#include "rules/weapons.h"

int FinesseDamageBonus(void* stats, int off_hand);  // rules_finesse.cpp: 2024 Finesse's Dexterity for Strength

namespace {

using WeaponFn = void* (*)(void* round, int attack_type);
using FinesseFn = int (*)(void* stats, void* item);
using CastingModFn = int8_t (*)(void* stats, uint8_t slot);
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int switched, int clear_first, int front);

WeaponFn g_weapon_of = nullptr;          // CNWSCombatRound::GetCurrentAttackWeapon
FinesseFn g_weapon_finesse = nullptr;    // CNWSCreatureStats::GetWeaponFinesse, through rules_finesse.cpp's hook
CastingModFn g_casting_mod = nullptr;    // CNWSCreatureStats::GetSpellcastingMod(class slot)
AddAttackActionsFn g_add_attack_actions = nullptr;

LogOnce g_once(1000);
int g_lines = 0;
bool LogSlot() {
  if (g_lines >= 400) return false;
  ++g_lines;
  return true;
}

constexpr int kAbilityStrength = 0;
constexpr int kAbilityIntelligence = 3;
constexpr int kAbilityWisdom = 4;
constexpr int kAbilityCharisma = 5;
constexpr uint64_t kMarkerGraceMs = 2000;  // game ms: Shillelagh's marker lands a moment after its script (DelayCommand)
constexpr uint64_t kTrueStrikeWaitMs = 15000;  // its attack's round starts within about two rounds (walking in)
constexpr size_t kRoundLastAttackCount = 0x2f64;  // InitializeAttackActions adds the counts +0x2F50..+0x2F64

bool Faithful() { return g_config.spellcasting == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

const char* AbilityName(int ability) {
  switch (ability) {
    case kAbilityStrength: return "Strength";
    case 1: return "Dexterity";
    case kAbilityIntelligence: return "Intelligence";
    case kAbilityWisdom: return "Wisdom";
    case kAbilityCharisma: return "Charisma";
    default: return "Constitution";
  }
}

std::string Signed(int value) { return (value >= 0 ? "+" : "") + std::to_string(value); }

int CharacterLevel(void* stats) {
  const int level = stats ? ReadCreatureFacts(stats).level : 1;
  return level > 0 ? level : 1;
}

// The weapon making `creature`'s current attack (null: unarmed or none) and its attack data.
void* AttackWeapon(void* creature, const uint8_t** attack_out) {
  void* round = creature ? Field<void*>(creature, ee::kCreatureCombatRound) : nullptr;
  const uint8_t* attack = creature ? CurrentAttackData(creature) : nullptr;
  if (attack_out) *attack_out = attack;
  if (!round || !attack || !g_weapon_of) return nullptr;
  return g_weapon_of(round, attack[ee::kAttackType]);
}

bool AttackRanged(const uint8_t* attack) { return attack && Field<int32_t>(attack, ee::kAttackRanged) != 0; }

// The ability term the engine's attack modifier holds: ranged Dexterity; melee Strength, or Dexterity when higher
// with a finesse weapon (GetWeaponFinesse: 2024's property, a Monk's weapons).
int NormalAttackTerm(void* stats, void* weapon, bool ranged) {
  const int strength = engine::StrengthModifier(stats), dexterity = engine::DexterityModifier(stats);
  if (ranged) return dexterity;
  const bool finesse = weapon && g_weapon_finesse && g_weapon_finesse(stats, weapon) != 0;
  return finesse && dexterity > strength ? dexterity : strength;
}

bool ThrownBaseItem(int base_item) { return base_item == 31 || base_item == 59 || base_item == 63; }

// The ability term the damage bonus holds (rules_two_weapon.cpp's GetDamageBonus): Strength for melee and thrown
// weapons (NWN's two-handed multiplier on a positive one), 2024 Finesse's swap to Dexterity; other ranged weapons none.
int NormalDamageTerm(void* stats, int base_item, bool ranged, float multiplier) {
  int term = 0;
  if (!ranged || ThrownBaseItem(base_item)) {
    const int strength = engine::StrengthModifier(stats);
    term = strength > 0 && multiplier > 0.0f ? static_cast<int>(strength * multiplier) : strength;
  }
  return term + FinesseDamageBonus(stats, 0);
}

// --- Shillelagh -------------------------------------------------------------------------------------------------

struct Imbued {
  void* weapon = nullptr;  // the CNWSItem* in the hand at the cast (compared, never read)
  uint64_t cast = 0;       // game ms of the cast
  bool seen = false;       // its marker found once
};
std::unordered_map<uint32_t, Imbued> g_imbued;  // by caster: letting go of the weapon ends it

// The row of the Shillelagh on `creature` (-1: none): its marker's spell id, the row cast.
int ShillelaghRow(const void* creature) {
  if (!creature) return -1;
  void* const* list = Field<void* const*>(creature, ee::kObjectEffects);
  const int count = Field<int32_t>(creature, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return -1;
  for (int i = 0; i < count; ++i) {
    const int spell = list[i] ? Field<int32_t>(list[i], ee::kEffectSpellId) : -1;
    if (rules::IsShillelagh(spell)) return spell;
  }
  return -1;
}

// 0.92.1's diagnosis: the creature's effects, type and spell id ("t<type>/s<spell>").
std::string DescribeEffects(const void* creature) {
  void* const* list = Field<void* const*>(creature, ee::kObjectEffects);
  const int count = Field<int32_t>(creature, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return "no effects";
  std::string text = std::to_string(count) + " effects:";
  for (int i = 0; i < count && i < 24; ++i) {
    if (!list[i]) continue;
    text += " t" + std::to_string(Field<uint16_t>(list[i], ee::kEffectType)) + "/s" +
            std::to_string(Field<int32_t>(list[i], ee::kEffectSpellId));
  }
  return text;
}

// The Shillelagh row when `creature`'s current attack is a melee attack with the weapon it imbued, else -1.
int ShillelaghAttack(void* creature) {
  if (!creature || !rules::IsShillelaghWeapon(engine::BaseItemRowInSlot(creature, ee::kSlotRightHand))) return -1;
  const uint8_t* attack = nullptr;
  void* weapon = AttackWeapon(creature, &attack);
  if (!weapon || AttackRanged(attack) || weapon != engine::ItemInSlot(creature, ee::kSlotRightHand)) return -1;
  const auto imbued = g_imbued.find(IdOf(creature));
  if (imbued != g_imbued.end() && imbued->second.weapon != weapon) return -1;  // not the one it was cast on
  return ShillelaghRow(creature);
}

const char* const kShillelaghCast = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  if (GetEffectTag(e) == "n5c_shillelagh") RemoveEffect(OBJECT_SELF, e);
  e = GetNextEffect(OBJECT_SELF);
}
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_NATURE), OBJECT_SELF);
effect m = EffectLinkEffects(EffectIcon($ICON), EffectVisualEffect(VFX_DUR_CESSATE_POSITIVE));
m = TagEffect(SetEffectSpellId(m, $ROW), "n5c_shillelagh");
DelayCommand(0.0, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, m, OBJECT_SELF, TurnsToSeconds(1)));
SendMessageToPC(OBJECT_SELF, "nwn5e: Shillelagh - your " + GetName(GetItemInSlot(INVENTORY_SLOT_RIGHTHAND)) +
                " for 1 minute: $SAY");
)nss";

const char* const kShillelaghEnd = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  if (GetEffectTag(e) == "n5c_shillelagh") RemoveEffect(OBJECT_SELF, e);
  e = GetNextEffect(OBJECT_SELF);
}
)nss";

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0 && LogSlot()) Log("cantrips: 0x%08X %s - its script returned %d", self, what, result);
}

void ShillelaghScript(uint32_t self) {
  void* caster = CreatureById(self);
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats) return;
  int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  if (!rules::IsShillelagh(row)) row = rules::kShillelagh;
  const int base_item = engine::BaseItemRowInSlot(caster, ee::kSlotRightHand);
  if (!rules::IsShillelaghWeapon(base_item)) {
    TellPlayer(self, "nwn5e: Shillelagh - no Club or Quarterstaff in your hand.");
    if (LogSlot()) Log("cantrips: 0x%08X Shillelagh - no club or quarterstaff in the hand (base item %d)", self, base_item);
    return;
  }
  const int wisdom = engine::AbilityModifier(stats, kAbilityWisdom);
  const rules::WeaponDie die = rules::ShillelaghDie(CharacterLevel(stats));
  const bool force = rules::WeaponCantripDamageType(row) == rules::kDamageTypeForce;
  std::string say = "Wisdom " + Signed(wisdom) + " for its attacks and damage when better than Strength, a " +
                    (die.count > 1 ? std::to_string(die.count) : std::string()) + "d" + std::to_string(die.sides) +
                    " damage die, " + (force ? "Force damage." : "its own Bludgeoning damage.");
  std::string code = kShillelaghCast;
  Replace(&code, "$ICON", std::to_string(rules::EffectIconOf(row)));
  Replace(&code, "$ROW", std::to_string(row));
  Replace(&code, "$SAY", say);
  Run(code, self, "n5_c_shillelagh");
  g_imbued[self] = Imbued{engine::ItemInSlot(caster, ee::kSlotRightHand), GameMs(), false};
  Log("cantrips: 0x%08X Shillelagh (row %d) - base item %d for 1 minute: Wisdom %+d, %dd%d, %s", self, row, base_item,
      wisdom, die.count, die.sides, force ? "Force" : "Bludgeoning");
}

// Letting go of the weapon ends it; its minute's end (the marker gone) drops the watch.
void ShillelaghUpdate(void* creature) {
  if (g_imbued.empty() || !creature) return;
  const uint32_t id = IdOf(creature);
  const auto imbued = g_imbued.find(id);
  if (imbued == g_imbued.end()) return;
  const int row = ShillelaghRow(creature);
  if (row < 0) {
    if (!imbued->second.seen && GameMs() - imbued->second.cast < kMarkerGraceMs) return;  // not landed yet
    const bool seen = imbued->second.seen;
    g_imbued.erase(imbued);
    if (!LogSlot()) return;
    if (seen) {
      Log("cantrips: 0x%08X Shillelagh ended", id);
    } else {
      Log("cantrips: 0x%08X Shillelagh's marker never landed - %s", id, DescribeEffects(creature).c_str());
    }
    return;
  }
  if (!imbued->second.seen) {
    imbued->second.seen = true;
    if (LogSlot()) Log("cantrips: 0x%08X Shillelagh's marker is on (row %d)", id, row);
  }
  if (engine::ItemInSlot(creature, ee::kSlotRightHand) == imbued->second.weapon) return;
  g_imbued.erase(imbued);
  Run(kShillelaghEnd, id, "n5_c_shillelagh");
  TellPlayer(id, "nwn5e: Shillelagh ends - you let go of the weapon.");
  if (LogSlot()) Log("cantrips: 0x%08X Shillelagh ends - the weapon left the hand", id);
}

// --- True Strike ------------------------------------------------------------------------------------------------

struct Strike {
  uint32_t target = 0;
  int row = rules::kTrueStrike;    // the row cast (the cantrip itself: Radiant)
  int casting = 0;                 // the spellcasting ability modifier
  int ability = kAbilityCharisma;  // which
  int level = 1;                   // character level: its Radiant dice
  uint64_t ordered = 0;            // game ms
  bool armed = false;              // its round is running
  uint32_t round = 0;              // CombatRoundsEnded at its round's start
  const uint8_t* attack = nullptr;  // its attack's data, once rolled (compared)
  bool ranged = false;
};
std::unordered_map<uint32_t, Strike> g_strikes;  // by attacker

// The spellcasting ability of the class slot it was cast from (an item's or a script's: the best of Intelligence,
// Wisdom and Charisma).
void CastingAbility(void* stats, int class_slot, int* ability, int* modifier) {
  const int count = Field<uint8_t>(stats, ee::kStatsClassCount);
  if (class_slot >= 0 && class_slot < count && class_slot < ee::kMaxClassSlots && g_casting_mod) {
    const int row = Field<uint8_t>(stats, ee::kStatsClassInfo + class_slot * ee::kClassInfoSize + ee::kClassInfoClass);
    *ability = row == 10 ? kAbilityIntelligence : (row == 2 || row == 3 || row == 7) ? kAbilityWisdom : kAbilityCharisma;
    *modifier = g_casting_mod(stats, static_cast<uint8_t>(class_slot));
    return;
  }
  *ability = kAbilityIntelligence;
  *modifier = engine::AbilityModifier(stats, kAbilityIntelligence);
  for (int a : {kAbilityWisdom, kAbilityCharisma}) {
    const int m = engine::AbilityModifier(stats, a);
    if (m > *modifier) {
      *ability = a;
      *modifier = m;
    }
  }
}

// The cast becomes an attack order at the target (true: the cast itself refused).
bool OrderTrueStrike(void* creature, int row, uint32_t target, int class_slot) {
  const uint32_t id = IdOf(creature);
  void* stats = StatsOf(creature);
  if (!stats || !g_add_attack_actions) return false;
  if (target == ee::kObjectInvalid || target == id || !CreatureById(target)) {
    TellPlayer(id, "nwn5e: True Strike - cast it at a creature: it becomes your attack at it.");
    return true;
  }
  const int base_item = engine::BaseItemRowInSlot(creature, ee::kSlotRightHand);
  if (base_item < 0 || !rules::NwnWeaponName(rules::MasteryKindOfNwnBaseItem(base_item))) {
    TellPlayer(id, "nwn5e: True Strike - no weapon in your hand: it is an attack with one.");
    if (LogSlot()) Log("cantrips: 0x%08X True Strike refused - no weapon in the hand (base item %d)", id, base_item);
    return true;
  }
  Strike s;
  s.target = target;
  s.row = rules::IsTrueStrike(row) ? row : rules::kTrueStrike;
  CastingAbility(stats, class_slot, &s.ability, &s.casting);
  s.level = CharacterLevel(stats);
  s.ordered = GameMs();
  g_strikes[id] = s;
  const int ordered = g_add_attack_actions(creature, target, 0, 1, 0);  // as a click on it
  const bool radiant = rules::WeaponCantripDamageType(s.row) == rules::kDamageTypeRadiant;
  const int dice = rules::TrueStrikeExtraDice(s.level, false);
  TellPlayer(id, std::string("nwn5e: True Strike - your next round is one attack at it: ") + AbilityName(s.ability) +
                     " " + Signed(s.casting) + " for its attack and damage, " +
                     (radiant ? "Radiant damage" : "the weapon's damage") +
                     (dice > 0 ? " and " + std::to_string(dice) + "d6 Radiant more." : "."));
  Log("cantrips: 0x%08X True Strike (row %d, class slot %d) at 0x%08X - %s %+d, level %d (%d d6 Radiant): attack "
      "ordered %d", id, s.row, class_slot, target, AbilityName(s.ability), s.casting, s.level, dice, ordered);
  return true;
}

bool TrueStrikeOrderGuard(void* creature, const SpellOrder& order) {
  if (!Faithful() || !creature || order.fake || !rules::IsTrueStrike(static_cast<int>(order.spell))) return false;
  return OrderTrueStrike(creature, static_cast<int>(order.spell), order.target, order.class_slot);
}

// A cast that reached its script (an item's, a script's ActionCastSpell...): the attack ordered now.
const char* const kStrikeTarget = R"nss(
SetLocalString(OBJECT_SELF, "n5c_target", ObjectToString(GetSpellTargetObject()));
)nss";

void TrueStrikeScript(uint32_t self) {
  void* caster = CreatureById(self);
  if (!caster) return;
  Run(kStrikeTarget, self, "n5_c_truestrike");
  const std::string target = engine::GetLocalString(caster, "n5c_target");
  const uint32_t id = target.empty() ? ee::kObjectInvalid : static_cast<uint32_t>(strtoul(target.c_str(), nullptr, 16));
  OrderTrueStrike(caster, Field<int32_t>(caster, ee::kObjectLastSpellId), id, 0xFF);
}

Strike* StrikeOf(void* creature) {
  if (g_strikes.empty() || !creature) return nullptr;
  const auto found = g_strikes.find(IdOf(creature));
  return found == g_strikes.end() ? nullptr : &found->second;
}

// The True Strike whose attack `creature` is making now (null: none).
Strike* StrikeAttack(void* creature) {
  Strike* s = StrikeOf(creature);
  return s && s->attack && s->attack == CurrentAttackData(creature) ? s : nullptr;
}

// Its attack: the first one its round rolls at its target.
void StrikeBeforeRoll(AttackRoll& roll) {
  Strike* s = StrikeOf(roll.attacker);
  if (!s || !s->armed || s->attack || !roll.data || !roll.target) return;
  if (s->round != CombatRoundsEnded(IdOf(roll.attacker)) || IdOf(roll.target) != s->target) return;
  s->attack = roll.data;
  s->ranged = AttackRanged(roll.data);
}

void StrikeAfterRoll(AttackRoll& roll) {
  Strike* s = StrikeOf(roll.attacker);
  if (!s || !s->attack || s->attack != roll.data) return;
  const uint8_t result = roll.data[ee::kAttackResult];
  if (rules::MasteryAttackHit(result)) return;  // its damage: below
  const uint32_t id = IdOf(roll.attacker);
  g_strikes.erase(id);
  if (LogSlot()) Log("cantrips: 0x%08X True Strike's attack misses (result %d)", id, result);
}

// A hit: True Strike's Radiant and its extra dice, Shillelagh's Force (the weapon's damage moved).
void WeaponCantripHit(void* attacker, void* /*target*/) {
  if (!attacker) return;
  uint8_t* data = CurrentAttackData(attacker);
  if (!data || !rules::MasteryAttackHit(data[ee::kAttackResult])) return;
  auto* entries = reinterpret_cast<int16_t*>(data + ee::kAttackDamageEntries);
  const uint32_t id = IdOf(attacker);
  if (Strike* s = StrikeAttack(attacker)) {
    const Strike strike = *s;
    g_strikes.erase(id);
    const int moved = rules::MoveWeaponDamage(entries, ee::kAttackDamageEntryCount,
                                              rules::WeaponCantripDamageType(strike.row));
    const bool critical = data[ee::kAttackResult] == ee::kAttackResultCritical;
    const int dice = rules::TrueStrikeExtraDice(strike.level, critical);
    const int extra = dice > 0 ? RollEngineDice(dice, 6) : 0;
    rules::AddDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeRadiant, extra);
    Log("cantrips: 0x%08X True Strike hits 0x%08X%s - %s %+d, %d of the weapon's damage made Radiant, %dd6 Radiant "
        "more = %d", id, strike.target, critical ? " (a Critical Hit)" : "", AbilityName(strike.ability),
        strike.casting, moved, dice, extra);
    return;
  }
  const int row = ShillelaghAttack(attacker);
  if (row < 0 || rules::WeaponCantripDamageType(row) != rules::kDamageTypeForce) return;
  const int moved = rules::MoveWeaponDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeForce);
  if (moved > 0 && g_once.First(LogOnce::Key(id, row, 1))) {
    Log("cantrips: 0x%08X Shillelagh's hit: the weapon's %d damage is Force (logged once)", id, moved);
  }
}

}  // namespace

bool WeaponCantripsOn() { return Faithful(); }

int WeaponCantripAttackBonus(void* stats) {
  if (!Faithful() || !stats) return 0;
  void* creature = CreatureOf(stats);
  if (!creature) return 0;
  if (Strike* s = StrikeAttack(creature)) {
    void* weapon = AttackWeapon(creature, nullptr);
    const int normal = NormalAttackTerm(stats, weapon, s->ranged);
    const int change = rules::WeaponCantripAbility(true, normal, s->casting) - normal;
    if (g_once.First(LogOnce::Key(IdOf(creature), s->casting, 100 + normal))) {
      Log("cantrips: 0x%08X True Strike's attack: %s %+d in place of %+d (logged once)", IdOf(creature),
          AbilityName(s->ability), s->casting, normal);
    }
    return change;
  }
  const int row = ShillelaghAttack(creature);
  if (row < 0) return 0;
  const int normal = NormalAttackTerm(stats, AttackWeapon(creature, nullptr), false);
  const int wisdom = engine::AbilityModifier(stats, kAbilityWisdom);
  const int change = rules::WeaponCantripAbility(false, normal, wisdom) - normal;
  if (g_once.First(LogOnce::Key(IdOf(creature), wisdom, 200 + normal))) {
    Log("cantrips: 0x%08X Shillelagh's attack: Wisdom %+d, its term %+d -> %+d (logged once)", IdOf(creature), wisdom,
        normal, normal + change);
  }
  return change;
}

int WeaponCantripDamageBonus(void* stats, int off_hand, float strength_multiplier) {
  if (!Faithful() || !stats || off_hand) return 0;
  void* creature = CreatureOf(stats);
  if (!creature) return 0;
  const int base_item = engine::BaseItemRowInSlot(creature, ee::kSlotRightHand);
  if (Strike* s = StrikeAttack(creature)) {
    const int normal = NormalDamageTerm(stats, base_item, s->ranged, strength_multiplier);
    return rules::WeaponCantripAbility(true, normal, s->casting) - normal;
  }
  if (ShillelaghAttack(creature) < 0) return 0;
  const int normal = NormalDamageTerm(stats, base_item, false, strength_multiplier);
  const int wisdom = engine::AbilityModifier(stats, kAbilityWisdom);
  return rules::WeaponCantripAbility(false, normal, wisdom) - normal;
}

bool WeaponCantripDice(void* stats, int off_hand, int* count, int* sides) {
  if (!Faithful() || !stats || off_hand || !count || !sides) return false;
  void* creature = CreatureOf(stats);
  if (!creature || ShillelaghAttack(creature) < 0) return false;
  const rules::WeaponDie die = rules::BetterDice({*count, *sides}, rules::ShillelaghDie(CharacterLevel(stats)));
  if (g_once.First(LogOnce::Key(IdOf(creature), die.count * 100 + die.sides, 300 + *count * 100 + *sides))) {
    Log("cantrips: 0x%08X Shillelagh's damage die: %dd%d in place of %dd%d (logged once)", IdOf(creature), die.count,
        die.sides, *count, *sides);
  }
  *count = die.count;
  *sides = die.sides;
  return true;
}

bool TrueStrikeRoundStart(void* round) {
  if (!Faithful() || g_strikes.empty() || !round) return false;
  void* creature = Field<void*>(round, ee::kRoundOwner);
  Strike* s = StrikeOf(creature);
  if (!s) return false;
  const uint32_t id = IdOf(creature);
  const uint32_t now = CombatRoundsEnded(id);
  const bool recount = s->armed && s->round == now;  // a recount mid-round keeps it
  if (!recount) {
    if (s->attack) {  // rolled in an earlier round and never resolved (no damage step): spent
      g_strikes.erase(id);
      return false;
    }
    if (GameMs() - s->ordered > kTrueStrikeWaitMs) {
      if (LogSlot()) Log("cantrips: 0x%08X True Strike lapsed - no round at its target in time", id);
      g_strikes.erase(id);
      return false;
    }
    // At its target: the order's (AddAttackActions sets it at once) or the attack's own.
    if (Field<uint32_t>(creature, ee::kCreatureAttemptedAttackTarget) != s->target &&
        Field<uint32_t>(creature, ee::kCreatureAttackTarget) != s->target) {
      return false;  // not at it (yet)
    }
    s->armed = true;
    s->round = now;
    s->attack = nullptr;
  }
  SetField<int32_t>(round, ee::kRoundMainHandAttacks, 1);
  for (size_t at = ee::kRoundOffHandAttacks; at <= kRoundLastAttackCount; at += sizeof(int32_t)) {
    SetField<int32_t>(round, at, 0);
  }
  if (!recount && LogSlot()) Log("cantrips: 0x%08X True Strike's round (%u): one attack at 0x%08X", id, now, s->target);
  return true;
}

void InstallWeaponCantrips() {
  g_weapon_of = reinterpret_cast<WeaponFn>(EngineExport(ee::kGetCurrentAttackWeapon));
  g_weapon_finesse = reinterpret_cast<FinesseFn>(EngineExport(ee::kGetWeaponFinesse));
  g_casting_mod = reinterpret_cast<CastingModFn>(EngineExport(ee::kGetSpellcastingMod));
  g_add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));
  const bool scripts = AddScriptHandler("n5_c_shillelagh", &ShillelaghScript) &&
                       AddScriptHandler("n5_c_truestrike", &TrueStrikeScript);
  const bool hooks = AddSpellOrderGuard(&TrueStrikeOrderGuard) &&
                     AddAttackRollListeners(&StrikeBeforeRoll, &StrikeAfterRoll) &&
                     AddResolveDamageListener(&WeaponCantripHit) && AddCreatureUpdateListener(&ShillelaghUpdate) &&
                     CountCombatRounds();
  const bool engine = g_weapon_of && g_weapon_finesse && g_casting_mod && g_add_attack_actions;
  Log("cantrips: build 3b - Shillelagh (a Bonus Action; Wisdom, its die, Force) and True Strike (its cast an attack) "
      "(#39 Part 1)%s",
      scripts && hooks && engine ? "" : " - SOME hooks missing");
}
