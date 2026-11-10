// #39 Part 2a (owner, 2026-11-09: "go A, level by level", then "A on all, but mark featherfall for later removal";
// docs/PLAN.md "Phase 4"), `spellcasting` faithful: SRD 5.2.1's levelled spells, their impact scripts run in C++
// (script_hooks.h) as the caster, as the cantrips' are (rules_cantrips.cpp). Build 1a, the level 1 damage and healing
// spells (rules/spells2024.h: their numbers, areas and upcasting):
// - NWN's rows rewritten (data/rows/spells.if_spells2024.level1.txt): Burning Hands 10, Cure Wounds 32 (NWN's Cure
//   Light Wounds), Magic Missile 107, Inflict Wounds 432 (NWN's Inflict Light Wounds), Thunderwave 436 (NWN's
//   Balagarn's Iron Horn), Ice Knife 543 (NWN's Ice Dagger); new: Guiding Bolt 1147, Ray of Sickness 1148, False Life
//   1149, Dissonant Whispers 1150 (nwn5e's placeholder icon). Each on its 2024 classes' lists as it's built.
// - The slot level: the cast's chosen one (#36's upcasting: the caster's +0x21C, GetLastSpellLevel(); 0 its own) - an
//   item's cast (a scroll, a wand) at the spell's own level.
// - Rolls: a spell attack (TouchAttackRanged / Melee - SK10's 2024 spell attack - twice the dice on a Critical Hit),
//   or each target's save against GetSpellSaveDC() (C5's 2024 DC; Reflex, Fortitude and Will answer as Dexterity,
//   Constitution and Wisdom). An area's damage is rolled once for every creature in it; a Dexterity save for half
//   goes through GetReflexAdjustedDamage (Evasion).
// - Areas from the caster (Burning Hands' 15-foot Cone, Thunderwave's 15-foot Cube) point at the spot aimed at; a
//   creature is in when part of it is (half a Medium creature's width of slack). Every creature in them, friend or
//   foe, the caster's own allies too (2024).
// - Riders: Guiding Bolt - the next attack roll against the target before the end of your next turn (two rounds)
//   has Advantage (rules_advantage.cpp asks; the target glows until then); Ray of Sickness - Poisoned (nwn5e's
//   poison icon) until the end of your next turn, nothing to a creature immune to poison; Dissonant Whispers - a
//   failed save spends the target's Reaction running 30 feet farther away (its actions locked while it runs);
//   Thunderwave - a failed save pushes it 10 feet away (the Shove's push, any size); Ice Knife - hit or miss, the
//   shard explodes: the target and each creature within 5 feet, a Dexterity save or the Cold damage; Magic Missile -
//   every dart at the creature targeted, each its own damage, none through NWN's Shield (417, until build 3).
// - Healing: Cure Wounds 2d8 + the spellcasting ability modifier (the 2024 DC's: DC - 8 - PB); False Life's
//   Temporary Hit Points replace those you have only when more (they don't stack), for 8 hours at most.
// - Build 1b (owner, 2026-11-09: "go for 1b"):
//   - Healing Word 1151, a Bonus Action spell: its order (the radial's, with its class and Level N) becomes its feat's
//     use (1555, casting 1152: instant, the round's attacks go on - bonus_actions.cpp's one a round); the script
//     spends the order's slot (rules_spell_slots.cpp: as the cast would) and heals 2d4 a level + the class's
//     casting modifier (the Life Domain's Disciple of Life sees the slot's level). Refused, nothing spent, when the
//     round's Bonus Action is spent, past 60 feet, or (SRD 5.2.1: One Spell with a Spell Slot per Turn) when a slot
//     was spent this turn - and a slot spell is refused after it the same turn. With `actions` in NWN mode it's an
//     action, as NWN casts it.
//   - Chromatic Orb 1153: its type from the menu (1154-1159; cantrips.h's choices, each opening the slot levels);
//     a spell attack, 3d8 a level; two of its d8s alike and the orb leaps - another attack and damage roll on the
//     nearest enemy within 30 feet of the target not yet hit, as many leaps as the slot's level. A diamond worth
//     50 GP (#38).
//   - Heroism 1160 (Concentration, 1 minute): immune to Frightened (any fear on it ends) and, at the start of each
//     of its turns (its round; out of combat every 6 s), Temporary Hit Points equal to the caster's spellcasting
//     modifier (replacing fewer, as they don't stack); an upcast's extra creatures the nearest friends within 10
//     feet of you (you first). Not on an enemy.
// - Build 2a (owner, 2026-11-10: "go for build 2"), the buffs - each a mark on its creatures (an icon, tagged, the
//   row cast: Concentration finds it), its rule in the hooks:
//   - Bless and Bane: up to 3 creatures (1 more a level), the nearest friends (Bane: enemies, a Charisma save)
//     to the spot aimed at within 30 feet; 1d4 on (off) attack rolls - rules_attack.cpp's modifier, a spell
//     attack's (rules_spell_attack.cpp) - and saves (rules_save_roll.cpp: the roll's DC moved by it, told).
//   - Shield of Faith (a Bonus Action): +2 AC; Mage Armor: a base AC of 13 + Dexterity while no armour is worn,
//     for characters (one base AC: Unarmored Defense's counted against it) - rules_attack.cpp's AC and the sheet's.
//   - Divine Favor (a Bonus Action): a weapon hit's 1d4 Radiant; Hunter's Mark (a Bonus Action): an attack roll's
//     hit on the quarry, 1d6 Force (weapon hits through ResolveDamage; spell attacks from their scripts), its
//     mark moved with a Bonus Action (feat 1562) once the quarry drops; twice the dice on a Critical Hit.
//   - Protection from Evil and Good: Aberrations, Elementals, Fey, Outsiders (Celestials, Fiends) and Undead have
//     Disadvantage attacking it (rules_advantage.cpp) and can't Frighten or Charm it (versus immunities).
//   - Sanctuary (a Bonus Action): an attack roll or a spell at the warded creature - the attacker's Wisdom save
//     against the caster's DC, or it's lost; it ends when the warded creature attacks, casts or deals damage.
//   - Expeditious Retreat (a Bonus Action): a Dash now, and a Bonus Action's Dash while it lasts (feat 1561).
//   - Longstrider: +10 feet of Speed (a third of 30 ft) for an hour; an upcast's extra creatures as Heroism's.
//   The Bonus Action spells all cast as Healing Word does (rules::BonusActionSpells); their feats used on their own
//   are refused before the Bonus Action is spent (a feat-use guard).
// NWN mode: NWN's spells.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "action_guard.h"
#include "attack_roll.h"
#include "bonus_actions.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "damage_listeners.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "movement_rate.h"
#include "reactions.h"
#include "resolve_damage.h"
#include "saves.h"
#include "script_hooks.h"
#include "spell_cast_listeners.h"
#include "spell_impact.h"
#include "staple_actions.h"
#include "rules/cantrips.h"
#include "rules/spell_slots.h"
#include "rules/spells2024.h"
#include "rules/weapons.h"

// rules_spell_slots.cpp (#39 2a build 1b): the slot a Bonus Action spell's order names - left, spent.
int OrderSlotsLeft(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic);
bool SpendOrderSlot(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic);
// rules_barbarian.cpp: the chest item's base AC, a shield in hand; B4's Unarmored Defense (Mage Armor's base AC).
int ChestArmorClassOf(void* creature);
bool HoldsShieldOf(void* creature);
int UnarmoredDefenseBonus(void* stats);
// Below (build 2a): Hunter's Mark on a spell attack's hit.
void HuntersMarkSpellHit(uint32_t caster, uint32_t target, bool critical);

namespace {

constexpr size_t kCreatureLastSpellLevel = 0x21C;  // GetLastSpellLevel(): the cast's slot level (0: its own)
constexpr uint64_t kRoundMs = 6000;
constexpr uint64_t kUntilEndOfNextTurnMs = 2 * kRoundMs;  // "until the end of your next turn" (cantrips' reading)
constexpr float kSlack = 0.5f;      // half a Medium creature's width: in an area when part of it is
constexpr float kPushMetres = 3.048f;  // 10 feet
constexpr const char* kVarHits = "n5s_hits";
constexpr const char* kVarSave = "n5s_save";
constexpr int kLogMax = 400;

std::unordered_map<uint32_t, uint64_t> g_guided;  // Guiding Bolt: Advantage on the next attack roll against it, until
std::unordered_set<uint32_t> g_glow_off;          // Guiding Bolt's glow to take off (the mark used)
// Build 1b.
using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          float* location);
UseFeatFn g_use_feat = nullptr;  // CNWSCreature::UseFeat through its hook (bonus_actions.cpp: one a round)
using SpellRowFn = void* (*)(const void* spells, int id);
SpellRowFn g_spell_row = nullptr;
void** g_rules_for_rows = nullptr;
thread_local bool t_bonus_cast = false;   // Healing Word's feat being used
thread_local bool t_bonus_spend = false;  // its slot being spent
struct BonusCast {
  uint32_t spell = 0;
  int class_slot = 0;
  int domain_level = 0;
  int metamagic = 0;
  uint64_t at = 0;
};
std::unordered_map<uint32_t, BonusCast> g_bonus_casts;  // Healing Word's order, for its feat's script
struct SlotTurn {
  uint32_t rounds = 0;
  uint64_t at = 0;
  bool bonus = false;  // spent by a Bonus Action spell
};
std::unordered_map<uint32_t, SlotTurn> g_slot_turns;  // a creature's last spell slot spent
struct Hero {
  uint32_t caster = 0;
  int temporary = 0;
  uint32_t rounds = 0;
  uint64_t last = 0;
};
std::unordered_map<uint32_t, Hero> g_heroes;  // Heroism's creatures
int g_hero_logs = 0;
int g_logged = 0;

bool Faithful() { return g_config.spellcasting == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_logged < kLogMax && ++g_logged; }

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

std::string Float(float value) {
  char text[32];
  sprintf_s(text, "%.3f", value);
  return text;
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0 && LogSlot()) Log("spells2024: 0x%08X %s - its script returned %d", self, what, result);
}

uint32_t ParseId(const std::string& text) { return static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 16)); }

// "id:a:b;" entries (a script's n5s_hits).
struct Entry {
  std::string target;
  int value = 0;
  float along = 0.0f;
  float side = 0.0f;
};

std::vector<Entry> Entries(void* caster) {
  std::vector<Entry> entries;
  const std::string text = caster ? engine::GetLocalString(caster, kVarHits) : std::string();
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find(';', at);
    if (end == std::string::npos) break;
    const std::string item = text.substr(at, end - at);
    at = end + 1;
    Entry entry;
    const size_t first = item.find(':');
    entry.target = item.substr(0, first);
    if (first != std::string::npos) {
      const size_t second = item.find(':', first + 1);
      entry.value = atoi(item.substr(first + 1).c_str());
      if (second != std::string::npos) {
        entry.along = static_cast<float>(atof(item.substr(first + 1).c_str()));
        entry.side = static_cast<float>(atof(item.substr(second + 1).c_str()));
      }
    }
    if (!entry.target.empty()) entries.push_back(entry);
  }
  return entries;
}

int SlotLevelAt(void* caster, int level) {
  if (!caster) return level;
  if (Field<uint32_t>(caster, ee::kCreatureLastSpellCastItem) != ee::kObjectInvalid) return level;  // an item's
  return rules::CastSlotLevel(level, Field<uint8_t>(caster, kCreatureLastSpellLevel));
}

int SlotLevelOf(void* caster, const rules::Spell2024& spell) { return SlotLevelAt(caster, spell.level); }

const char* SaveCall(rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kConstitution: return "FortitudeSave";
    case rules::Ability::kWisdom: return "WillSave";
    case rules::Ability::kCharisma: return "WillSave";  // C1 makes it Charisma for its spell (Bane)
    default: return "ReflexSave";
  }
}

const char* AbilityName(rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kConstitution: return "Constitution";
    case rules::Ability::kWisdom: return "Wisdom";
    case rules::Ability::kCharisma: return "Charisma";
    default: return "Dexterity";
  }
}

const char* TypeName(uint32_t type) {
  switch (type) {
    case 2: return "Piercing";
    case rules::kDamageTypeCold: return "Cold";
    case rules::kDamageTypeRadiant: return "Radiant";
    case rules::kDamageTypeFire: return "Fire";
    case rules::kDamageTypeNecrotic: return "Necrotic";
    case rules::kDamageTypeThunder: return "Thunder";
    case rules::kDamageTypePoison: return "Poison";
    case rules::kDamageTypeForce: return "Force";
    case rules::kDamageTypePsychic: return "Psychic";
  }
  return "damage";
}

std::string Dice(int count, int die) { return std::to_string(count) + "d" + std::to_string(die); }

// --- The scripts -----------------------------------------------------------------------------------------------

// One creature: the spell's event, its beam, and $TEST's answer ("id:n;").
const char* const kTarget = R"nss(
object t = GetSpellTargetObject(); string r = "";
if (GetIsObjectValid(t)) {
  $SIGNAL
  $BEAM
  r = ObjectToString(t) + ":" + IntToString($TEST) + ";";
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

// The creatures near the caster, with where they stand from it toward the point aimed at ("id:along:side;").
const char* const kGather = R"nss(
vector o = GetPosition(OBJECT_SELF);
vector d = GetPositionFromLocation(GetSpellTargetLocation()) - o;
d.z = 0.0;
if (VectorMagnitude(d) < 0.1) d = AngleToVector(GetFacing(OBJECT_SELF)); else d = VectorNormalize(d);
SetFacingPoint(o + d);
location here = GetLocation(OBJECT_SELF);
$LOOK
string r = "";
object t = GetFirstObjectInShape(SHAPE_SPHERE, $REACH, here, TRUE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (t != OBJECT_SELF && !GetIsDead(t)) {
    vector v = GetPosition(t) - o;
    r += ObjectToString(t) + ":" + FloatToString(v.x * d.x + v.y * d.y, 0, 3) + ":" +
         FloatToString(fabs(v.x * d.y - v.y * d.x), 0, 3) + ";";
  }
  t = GetNextObjectInShape(SHAPE_SPHERE, $REACH, here, TRUE, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

// Ice Knife's explosion: the creatures within 5 feet of the target.
const char* const kExplosion = R"nss(
object c = StringToObject("$TARGET");
location l = GetSpellTargetLocation();
if (GetIsObjectValid(c)) l = GetLocation(c);
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_FROST_L), l);
string r = "";
object t = GetFirstObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (!GetIsDead(t)) r += ObjectToString(t) + ":0;";
  t = GetNextObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

// One creature's save (-1: gone): 0 failed, 1 succeeded, 2 immune.
const char* const kSave = R"nss(
object t = StringToObject("$TARGET"); int s = -1;
if (GetIsObjectValid(t) && !GetIsDead(t)) {
  if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  s = $SAVE(t, GetSpellSaveDC(), SAVING_THROW_TYPE_NONE, OBJECT_SELF);
}
SetLocalInt(OBJECT_SELF, "n5s_save", s);
)nss";

// A Dexterity save for half: the damage it leaves (Evasion's too; -1: gone).
const char* const kReflexHalf = R"nss(
object t = StringToObject("$TARGET"); int n = -1;
if (GetIsObjectValid(t) && !GetIsDead(t)) {
  if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  n = GetReflexAdjustedDamage($AMOUNT, t, GetSpellSaveDC(), SAVING_THROW_TYPE_NONE, OBJECT_SELF);
}
SetLocalInt(OBJECT_SELF, "n5s_save", n);
)nss";

// The damage on one creature, its visual and the spell's rider.
const char* const kDamage = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
$IMMUNE
ApplyEffectToObject(DURATION_TYPE_INSTANT, SetEffectSpellId(EffectLinkEffects(EffectDamage($AMOUNT, $TYPE),
                    EffectVisualEffect($VIS)), $ROW), t);
$RIDER
)nss";

const char* const kHeal = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
ApplyEffectToObject(DURATION_TYPE_INSTANT, SetEffectSpellId(EffectHeal($AMOUNT), $ROW), t);
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEALING_S), t);
)nss";

// False Life: the Temporary Hit Points you have give way to more.
const char* const kTemporary = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  if (GetEffectType(e) == EFFECT_TYPE_TEMPORARY_HITPOINTS) RemoveEffect(OBJECT_SELF, e);
  e = GetNextEffect(OBJECT_SELF);
}
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, SupernaturalEffect(SetEffectSpellId(EffectTemporaryHitpoints($AMOUNT),
                    $ROW)), OBJECT_SELF, HoursToSeconds(8));
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_EVIL_HELP), OBJECT_SELF);
)nss";

// Magic Missile: the darts, each its own damage, as NWN's own fly (VFX_IMP_MIRV, the travel time by distance).
const char* const kDarts = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
float dist = GetDistanceBetween(OBJECT_SELF, t);
float travel = 0.1;
if (dist > 1.0) travel = dist / (3.0 * log(dist) + 2.0);
$DARTS
)nss";

const char* const kShielded = R"nss(
object t = StringToObject("$TARGET");
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGIC_PROTECTION), t);
SendMessageToPC(OBJECT_SELF, "nwn5e: " + GetName(t) + "'s Shield stops your Magic Missile.");
)nss";

// Dissonant Whispers: its Reaction spent running away - its actions locked until it's done (or 4 s).
const char* const kFlee = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t) || GetIsDead(t)) return;
float away = GetDistanceBetween(t, OBJECT_SELF) + $FEET;
AssignCommand(t, ClearAllActions(TRUE));
AssignCommand(t, ActionMoveAwayFromObject(OBJECT_SELF, TRUE, away));
AssignCommand(t, ActionDoCommand(SetCommandable(TRUE, t)));
AssignCommand(t, DelayCommand(4.0, SetCommandable(TRUE, t)));
AssignCommand(t, SetCommandable(FALSE, t));
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_CONFUSION_S), t);
SendMessageToPC(OBJECT_SELF, "nwn5e: " + GetName(t) + " spends its Reaction running from you (Dissonant Whispers).");
)nss";

const char* const kGlowOff = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "n5s_guided") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }
)nss";

struct Look {
  std::string beam;    // before the roll
  std::string vis;     // on the damage
  std::string rider;   // after it
  std::string immune;  // before it
};

Look LookOf(int row) {
  Look look;
  look.vis = "VFX_IMP_MAGBLUE";
  switch (row) {
    case rules::kBurningHands:
      look.vis = "VFX_IMP_FLAME_S";
      break;
    case rules::kInflictWounds:
      look.vis = "VFX_IMP_NEGATIVE_ENERGY";
      break;
    case rules::kThunderwave:
      look.vis = "VFX_IMP_SONIC";
      break;
    case rules::kIceKnife:
      look.vis = "VFX_IMP_FROST_S";
      break;
    case rules::kGuidingBolt:
      look.beam = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(VFX_BEAM_HOLY, OBJECT_SELF, BODY_NODE_HAND), "
                  "t, 1.0);";
      look.vis = "VFX_IMP_SUNSTRIKE";
      look.rider = "effect g = EffectVisualEffect(VFX_DUR_LIGHT_WHITE_5); ApplyEffectToObject(DURATION_TYPE_TEMPORARY, "
                   "TagEffect(SupernaturalEffect(g), \"n5s_guided\"), t, RoundsToSeconds(2));";
      break;
    case rules::kRayOfSickness:
      look.beam = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(VFX_BEAM_DISINTEGRATE, OBJECT_SELF, "
                  "BODY_NODE_HAND), t, 1.0);";
      look.vis = "VFX_IMP_POISON_S";
      look.immune = "if (GetIsImmune(t, IMMUNITY_TYPE_POISON, OBJECT_SELF)) { ApplyEffectToObject(DURATION_TYPE_INSTANT, "
                    "EffectVisualEffect(VFX_IMP_MAGIC_PROTECTION), t); SendMessageToPC(OBJECT_SELF, GetName(t) + "
                    "\" is immune to Poison.\"); SetLocalInt(OBJECT_SELF, \"n5s_save\", 2); return; }";
      look.rider = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect(SetEffectSpellId("
                   "EffectIcon(EFFECT_ICON_POISON), $ROW)), \"n5s_poisoned\"), t, RoundsToSeconds(2));";
      break;
    case rules::kDissonantWhispers:
      look.vis = "VFX_IMP_HEAD_SONIC";
      break;
  }
  return look;
}

// Applies `amount` of the spell's damage to `target` (nothing when 0); false when the target turned out immune.
bool Damage(uint32_t self, const rules::Spell2024& spell, const std::string& target, int amount, uint32_t type,
            const std::string& vis, const Look& look) {
  if (amount <= 0) return true;
  void* caster = CreatureById(self);
  if (caster) engine::SetLocalInt(caster, kVarSave, 0);
  std::string code = kDamage;
  Replace(&code, "$IMMUNE", look.immune);
  Replace(&code, "$RIDER", look.rider);
  Replace(&code, "$TARGET", target);
  Replace(&code, "$AMOUNT", std::to_string(amount));
  Replace(&code, "$TYPE", std::to_string(type));
  Replace(&code, "$VIS", vis);
  Replace(&code, "$ROW", std::to_string(spell.row));
  Run(code, self, spell.script);
  return !(caster && engine::GetLocalInt(caster, kVarSave) == 2);
}

int SaveOf(uint32_t self, const rules::Spell2024& spell, rules::Ability ability, const std::string& target) {
  void* caster = CreatureById(self);
  if (!caster) return -1;
  std::string code = kSave;
  Replace(&code, "$TARGET", target);
  Replace(&code, "$SAVE", SaveCall(ability));
  Run(code, self, spell.script);
  return engine::GetLocalInt(caster, kVarSave);
}

int ReflexHalf(uint32_t self, const rules::Spell2024& spell, const std::string& target, int amount) {
  void* caster = CreatureById(self);
  if (!caster) return -1;
  std::string code = kReflexHalf;
  Replace(&code, "$TARGET", target);
  Replace(&code, "$AMOUNT", std::to_string(amount));
  Run(code, self, spell.script);
  return engine::GetLocalInt(caster, kVarSave);
}

// `signal`: 1 a hostile spell's event, 0 a friendly one's, -1 none (its save sends it).
std::vector<Entry> OneTarget(uint32_t self, const rules::Spell2024& spell, const std::string& test, int signal,
                             const Look& look) {
  std::string code = kTarget;
  Replace(&code, "$TEST", test);
  Replace(&code, "$SIGNAL", signal < 0 ? std::string()
                                       : std::string("if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, "
                                                     "GetSpellId(), ") + (signal ? "TRUE" : "FALSE") + "));");
  Replace(&code, "$BEAM", look.beam);
  Run(code, self, spell.script);
  return Entries(CreatureById(self));
}

std::string SaveWord(int save) { return save == 0 ? "failed" : save == 1 ? "succeeded" : "immune"; }

// --- The spells ----------------------------------------------------------------------------------------------------

// Burning Hands, Thunderwave: every creature in the area from the caster; one damage roll.
void FromCaster(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const float edge = rules::FeetToMetres(spell.feet);
  std::string code = kGather;
  Replace(&code, "$REACH", Float(edge * 1.2f + kSlack));
  Replace(&code, "$LOOK",
          spell.row == rules::kThunderwave
              ? "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_SOUND_BURST), "
                "Location(GetArea(OBJECT_SELF), o + d * " + Float(edge / 2.0f) + ", 0.0));"
              : "");
  Run(code, self, spell.script);
  const Look look = LookOf(spell.row);
  const int dice = rules::SpellDiceAt(spell, slot);
  const int full = RollEngineDice(dice, spell.die);
  std::string log;
  int inside = 0;
  for (const Entry& entry : Entries(caster)) {
    const bool in = spell.shape == rules::SpellShape2024::kCone
                        ? rules::InCone(entry.along, entry.side, edge, kSlack)
                        : rules::InCubeFromCaster(entry.along, entry.side, edge, kSlack);
    if (!in) continue;
    ++inside;
    int dealt = 0;
    int save = 0;
    if (spell.save == rules::Ability::kDexterity && spell.half_on_save) {
      dealt = ReflexHalf(self, spell, entry.target, full);
      if (dealt < 0) continue;
      save = dealt == full ? 0 : 1;
    } else {
      save = SaveOf(self, spell, spell.save, entry.target);
      if (save < 0) continue;
      dealt = rules::DamageAfterSave(full, save != 0, spell.half_on_save && save == 1);
    }
    Damage(self, spell, entry.target, dealt, spell.damage_type, look.vis, look);
    log += "; 0x" + entry.target + " " + std::to_string(dealt) + (save == 0 ? "" : " (saved)");
    if (spell.row == rules::kThunderwave && save == 0) {
      void* target = CreatureById(ParseId(entry.target));
      float pushed = 0.0f;
      const std::string why = target ? ShovePush(caster, target, kPushMetres, &pushed, true) : "gone";
      log += why.empty() ? ", pushed " + Float(pushed) + " m" : ", not pushed (" + why + ")";
    }
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s (a level %d slot): %s = %d %s, %s save%s - %d in the %d-foot %s%s", self, spell.name,
        slot, Dice(dice, spell.die).c_str(), full, TypeName(spell.damage_type), AbilityName(spell.save),
        spell.half_on_save ? " for half" : "", inside, spell.feet,
        spell.shape == rules::SpellShape2024::kCone ? "Cone" : "Cube", log.c_str());
  }
}

// Guiding Bolt, Ray of Sickness, Ice Knife: a spell attack.
void Attack(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const Look look = LookOf(spell.row);
  const std::vector<Entry> hits =
      OneTarget(self, spell, spell.melee ? "TouchAttackMelee(t)" : "TouchAttackRanged(t)", 1, look);
  for (const Entry& hit : hits) {
    const bool critical = hit.value == 2;
    std::string log = hit.value > 0 ? (critical ? "a Critical Hit" : "a hit") : "a miss";
    if (hit.value > 0) {
      const int dice = rules::CriticalDice(rules::SpellDiceAt(spell, slot), critical);
      const int amount = RollEngineDice(dice, spell.die);
      std::string rider = look.rider;
      Replace(&rider, "$ROW", std::to_string(spell.row));
      Look applied = look;
      applied.rider = rider;
      const bool landed = Damage(self, spell, hit.target, amount, spell.damage_type, look.vis, applied);
      if (landed) HuntersMarkSpellHit(self, ParseId(hit.target), critical);  // build 2a
      log += ": " + Dice(dice, spell.die) + " = " + std::to_string(amount) + " " + TypeName(spell.damage_type);
      if (!landed) log += " - immune";
      const uint32_t id = ParseId(hit.target);
      if (spell.row == rules::kGuidingBolt) {
        const uint64_t now = GameMs();
        if (g_guided.size() > 64) {
          for (auto it = g_guided.begin(); it != g_guided.end();) {
            it = it->second <= now ? g_guided.erase(it) : std::next(it);
          }
        }
        g_guided[id] = now + kUntilEndOfNextTurnMs;
        g_glow_off.erase(id);
        TellPlayer(self, "nwn5e: Guiding Bolt - the next attack roll against it before the end of your next turn "
                         "has Advantage.");
        log += "; the next attack roll against it has Advantage";
      }
      if (spell.row == rules::kRayOfSickness && landed) log += "; Poisoned until the end of your next turn";
    }
    if (spell.row == rules::kIceKnife) {
      std::string code = kExplosion;
      Replace(&code, "$TARGET", hit.target);
      Replace(&code, "$RADIUS", Float(rules::FeetToMetres(rules::kIceKnifeExplosionFeet) + 2.0f * kSlack));
      Run(code, self, spell.script);
      const int dice = rules::IceKnifeExplosionDice(slot);
      const int cold = RollEngineDice(dice, rules::kIceKnifeExplosionDie);
      log += "; the explosion " + Dice(dice, rules::kIceKnifeExplosionDie) + " = " + std::to_string(cold) + " Cold:";
      for (const Entry& other : Entries(caster)) {
        const int save = SaveOf(self, spell, rules::Ability::kDexterity, other.target);
        if (save < 0) continue;
        const int dealt = rules::DamageAfterSave(cold, save != 0, false);
        Damage(self, spell, other.target, dealt, rules::kDamageTypeCold, "VFX_IMP_FROST_S", Look());
        log += " 0x" + other.target + " " + std::to_string(dealt);
      }
    }
    if (LogSlot()) {
      Log("spells2024: 0x%08X %s (a level %d slot) at 0x%s: %s", self, spell.name, slot, hit.target.c_str(),
          log.c_str());
    }
  }
}

// Inflict Wounds, Dissonant Whispers: one creature's save.
void SaveOne(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const Look look = LookOf(spell.row);
  for (const Entry& one : OneTarget(self, spell, "0", -1, look)) {
    const int dice = rules::SpellDiceAt(spell, slot);
    const int full = RollEngineDice(dice, spell.die);
    const int save = SaveOf(self, spell, spell.save, one.target);
    if (save < 0) continue;
    const int dealt = rules::DamageAfterSave(full, save != 0, spell.half_on_save && save == 1);
    Damage(self, spell, one.target, dealt, spell.damage_type, look.vis, look);
    std::string log = std::string(AbilityName(spell.save)) + " save " + SaveWord(save) + ": " + Dice(dice, spell.die) +
                      " = " + std::to_string(full) + ", " + std::to_string(dealt) + " " + TypeName(spell.damage_type);
    if (spell.row == rules::kDissonantWhispers && save == 0) {
      void* target = CreatureById(ParseId(one.target));
      if (target && !engine::IsDeadOrDying(target) && TakeReaction(target)) {
        std::string code = kFlee;
        Replace(&code, "$TARGET", one.target);
        Replace(&code, "$FEET", Float(rules::FeetToMetres(rules::kDissonantWhispersFleeFeet)));
        Run(code, self, spell.script);
        log += "; its Reaction spent running away";
      } else {
        log += "; no Reaction left to run with";
      }
    }
    (void)caster;
    if (LogSlot()) {
      Log("spells2024: 0x%08X %s (a level %d slot) at 0x%s: %s", self, spell.name, slot, one.target.c_str(),
          log.c_str());
    }
  }
}

void Darts(uint32_t self, const rules::Spell2024& spell, int slot) {
  const std::vector<Entry> targets = OneTarget(self, spell, "GetHasSpellEffect(" + std::to_string(rules::kNwnShield) +
                                                              ", t)", 1, Look());
  for (const Entry& one : targets) {
    if (one.value != 0) {
      std::string code = kShielded;
      Replace(&code, "$TARGET", one.target);
      Run(code, self, spell.script);
      if (LogSlot()) Log("spells2024: 0x%08X Magic Missile at 0x%s: its Shield stops the darts", self, one.target.c_str());
      continue;
    }
    const int darts = rules::MagicMissileDarts(slot);
    std::string lines, log;
    for (int i = 0; i < darts; ++i) {
      const int amount = RollEngineDice(spell.dice, spell.die) + spell.flat;
      const std::string after = Float(0.1f * static_cast<float>(i));
      lines += "DelayCommand(" + after + ", ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MIRV), "
               "t));\nDelayCommand(travel + " + after + ", ApplyEffectToObject(DURATION_TYPE_INSTANT, SetEffectSpellId("
               "EffectLinkEffects(EffectDamage(" + std::to_string(amount) + ", " + std::to_string(spell.damage_type) +
               "), EffectVisualEffect(VFX_IMP_MAGBLUE)), " + std::to_string(spell.row) + "), t));\n";
      log += (i ? ", " : "") + std::to_string(amount);
    }
    std::string code = kDarts;
    Replace(&code, "$TARGET", one.target);
    Replace(&code, "$DARTS", lines);
    Run(code, self, spell.script);
    if (LogSlot()) {
      Log("spells2024: 0x%08X Magic Missile (a level %d slot) at 0x%s: %d darts, 1d4 + 1 Force each - %s", self, slot,
          one.target.c_str(), darts, log.c_str());
    }
  }
}

void CureWounds(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  for (const Entry& one : OneTarget(self, spell, "0", 0, Look())) {
    const int dice = rules::SpellDiceAt(spell, slot);
    const int roll = RollEngineDice(dice, spell.die);
    const int modifier = SpellcastingModifier2024(caster);
    const int amount = roll + modifier > 0 ? roll + modifier : 0;
    std::string code = kHeal;
    Replace(&code, "$TARGET", one.target);
    Replace(&code, "$AMOUNT", std::to_string(amount));
    Replace(&code, "$ROW", std::to_string(spell.row));
    Run(code, self, spell.script);
    if (LogSlot()) {
      Log("spells2024: 0x%08X Cure Wounds (a level %d slot) on 0x%s: %s = %d + %d (spellcasting ability) = %d Hit "
          "Points", self, slot, one.target.c_str(), Dice(dice, spell.die).c_str(), roll, modifier, amount);
    }
  }
}

void FalseLife(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int roll = RollEngineDice(spell.dice, spell.die);
  const int amount = rules::FalseLifeTemporaryHitPoints(roll, slot);
  const int current = Field<int16_t>(caster, ee::kObjectTemporaryHitPoints);
  if (!rules::TemporaryHitPointsReplace(current, amount)) {
    TellPlayer(self, "nwn5e: False Life - " + std::to_string(amount) + " Temporary Hit Points, but you keep your " +
                         std::to_string(current) + " (they don't stack).");
  } else {
    std::string code = kTemporary;
    Replace(&code, "$AMOUNT", std::to_string(amount));
    Replace(&code, "$ROW", std::to_string(spell.row));
    Run(code, self, spell.script);
    TellPlayer(self, "nwn5e: False Life - " + std::to_string(amount) + " Temporary Hit Points.");
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X False Life (a level %d slot): 2d4 = %d + 4 + %d = %d Temporary Hit Points (had %d)", self,
        slot, roll, 5 * (slot - 1), amount, current);
  }
}

// --- Build 1b ------------------------------------------------------------------------------------------------------

int InnateLevel(uint32_t spell) {
  if (!g_spell_row || !g_rules_for_rows || !*g_rules_for_rows) return -1;
  const void* row = g_spell_row(Field<void*>(*g_rules_for_rows, ee::kRulesSpells), static_cast<int>(spell));
  return row ? Field<uint8_t>(row, ee::kSpellInnateLevel) : -1;
}

// SRD 5.2.1, One Spell with a Spell Slot per Turn: a slot spent this turn (`bonus`: by a Bonus Action spell).
bool SpentThisTurn(uint32_t id, bool* bonus) {
  const auto at = g_slot_turns.find(id);
  if (at == g_slot_turns.end()) return false;
  const long long since = static_cast<long long>(GameMs()) - static_cast<long long>(at->second.at);
  if (!rules::SameTurn(at->second.rounds, CombatRoundsEnded(id), since)) return false;
  if (bonus) *bonus = at->second.bonus;
  return true;
}

// The willing-creature spells: an enemy is no target (0.99.3, Heroism's; build 2a: Mage Armor, Protection from Evil
// and Good).
bool WillingSpell(int spell) {
  return spell == rules::kHeroism || spell == rules::kMageArmor || spell == rules::kProtectionFromEvilAndGood;
}

bool Refuse(uint32_t id, const std::string& name, const std::string& why, uint32_t target) {
  TellPlayer(id, "nwn5e: " + name + " - " + why);
  if (LogSlot()) Log("spells2024: 0x%08X %s at 0x%08X refused - %s", id, name.c_str(), target, why.c_str());
  return true;
}

bool SanctuaryLetsThrough(void* attacker, void* target, const char* what);  // build 2a, below

// Refused before the cast, nothing spent (true): a willing creature's spell at an enemy, Mage Armor on one wearing
// armour, an enemy's spell at a creature its Sanctuary keeps it from. A Bonus Action spell's order becomes its feat's
// use (instant, a Bonus Action: Healing Word's way, every one's since build 2a); a slot spell after a Bonus Action one
// the same turn is refused.
bool OrderGuard(void* creature, const SpellOrder& order) {
  if (!Faithful() || !creature || order.fake || t_bonus_cast) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const int spell = static_cast<int>(order.spell);
  void* target = CreatureById(order.target);
  // 0.99.3 (owner, 2026-11-10: "why does heroism need to be cast first before checking if it's a willing target? Is
  // it the rules?"): "a willing creature" - an enemy isn't one, so it's no target (the script's check stays for a
  // cast with no order).
  if (WillingSpell(spell) && target && target != creature && engine::IsEnemy(creature, order.target)) {
    return Refuse(id, SpellNameOf(spell), "a willing creature, not an enemy.", order.target);
  }
  if (spell == rules::kMageArmor && target && ChestArmorClassOf(target) > 0) {
    return Refuse(id, "Mage Armor", "a creature who isn't wearing armour.", order.target);
  }
  if (target && target != creature && engine::IsEnemy(creature, order.target) &&
      !SanctuaryLetsThrough(creature, target, "cast a spell at")) {
    return true;  // told
  }
  if (!ActionEconomyOn() || order.class_slot < 0 || order.class_slot > 7) return false;
  const rules::BonusActionSpell* bonus = rules::BonusActionSpellOf(spell);
  if (!bonus || bonus->spell != spell) {
    bool spent_bonus = false;
    if (InnateLevel(order.spell) >= 1 && SpentThisTurn(id, &spent_bonus) && spent_bonus) {
      TellPlayer(id, "nwn5e: you cast a Bonus Action spell with a spell slot this turn - one spell with a slot a "
                     "turn; cast it next round.");
      if (LogSlot()) Log("spells2024: 0x%08X spell %u refused - a Bonus Action spell spent a slot this turn", id, order.spell);
      return true;
    }
    return false;
  }
  if (!g_use_feat) return false;  // cast as NWN casts it, an action
  void* stats = StatsOf(creature);
  const uint32_t at = bonus->feet > 0 ? order.target : id;  // a Self spell: on you
  void* on = bonus->feet > 0 ? target : creature;
  const std::string feet = std::to_string(bonus->feet);
  std::string refused;
  if (SpentThisTurn(id, nullptr)) {
    refused = "you've already spent a spell slot this turn (one spell with a slot a turn).";
  } else if (BonusActionStatus(creature) != 0) {
    refused = "it takes your Bonus Action, already spent this round.";
  } else if (!on) {
    refused = "a creature you can see within " + feet + " feet.";
  } else if (on != creature && engine::EdgeDistance(creature, on) > rules::FeetToMetres(bonus->feet)) {
    refused = "out of range (" + feet + " feet).";
  } else if (OrderSlotsLeft(creature, order.spell, order.class_slot, order.domain_level, order.metamagic) <= 0) {
    refused = "no spell slot of that level left.";
  } else if (!stats) {
    refused = "no caster.";
  }
  if (!refused.empty()) return Refuse(id, bonus->name, refused, at);
  if (!engine::HasFeat(stats, bonus->feat)) engine::AddFeat(stats, bonus->feat);
  g_bonus_casts[id] = BonusCast{order.spell, order.class_slot, order.domain_level, order.metamagic, GameMs()};
  float nowhere[3] = {0.0f, 0.0f, 0.0f};
  t_bonus_cast = true;
  const int used = g_use_feat(creature, static_cast<uint16_t>(bonus->feat), 0, at, ee::kObjectInvalid, nowhere);
  t_bonus_cast = false;
  if (!used) g_bonus_casts.erase(id);
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s at 0x%08X - a Bonus Action (feat %d, class slot %d, domain level %d): %s", id,
        bonus->name, at, bonus->feat, order.class_slot, order.domain_level, used ? "used" : "refused");
  }
  return true;  // the cast refused: the feat's use runs it
}

// The slot a cast spends: a Bonus Action spell's through its feat - its order's, spent here - or the cast's own.
struct CastInfo {
  bool ok = true;
  int slot = 1;
  int class_slot = -1;  // a Bonus Action spell's (its order's class)
  bool bonus = false;
};

CastInfo CastInfoOf(uint32_t self, void* caster, int level, const char* name) {
  CastInfo info;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const rules::BonusActionSpell* bonus = rules::BonusActionSpellOf(row);
  if (!bonus || row != bonus->bonus_row) {
    info.slot = SlotLevelAt(caster, level);
    return info;
  }
  info.bonus = true;
  const auto at = g_bonus_casts.find(self);
  if (at == g_bonus_casts.end() || GameMs() > at->second.at + 10000 ||
      static_cast<int>(at->second.spell) != bonus->spell) {
    if (at != g_bonus_casts.end()) g_bonus_casts.erase(at);
    TellPlayer(self, std::string("nwn5e: ") + name + " - cast it from your spells (it spends a spell slot).");
    info.ok = false;
    return info;
  }
  const BonusCast cast = at->second;
  g_bonus_casts.erase(at);
  t_bonus_spend = true;
  const bool spent = SpendOrderSlot(caster, cast.spell, cast.class_slot, cast.domain_level, cast.metamagic);
  t_bonus_spend = false;
  if (!spent) {
    TellPlayer(self, std::string("nwn5e: ") + name + " - no spell slot of that level left.");
    if (LogSlot()) Log("spells2024: 0x%08X %s - its slot couldn't be spent", self, name);
    info.ok = false;
    return info;
  }
  const int chosen = rules::ChosenSlotLevel(level, cast.domain_level);
  info.slot = chosen >= level ? chosen : level;
  info.class_slot = cast.class_slot;
  return info;
}

void HealingWord(uint32_t self, void* caster, const rules::Spell2024& spell) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const CastInfo cast = CastInfoOf(self, caster, spell.level, spell.name);
  if (!cast.ok) return;
  const int slot = cast.slot;
  const int modifier =
      cast.bonus ? CastingModifierOfClassSlot(caster, cast.class_slot) : SpellcastingModifier2024(caster);
  for (const Entry& one : OneTarget(self, spell, "0", 0, Look())) {
    const int dice = rules::SpellDiceAt(spell, slot);
    const int roll = RollEngineDice(dice, spell.die);
    const int amount = roll + modifier > 0 ? roll + modifier : 0;
    std::string code = kHeal;
    Replace(&code, "$TARGET", one.target);
    Replace(&code, "$AMOUNT", std::to_string(amount));
    Replace(&code, "$ROW", std::to_string(row));
    // The slot's level where the Life Domain's Disciple of Life reads a cast's (GetLastSpellLevel) while it lands.
    const uint8_t before = Field<uint8_t>(caster, kCreatureLastSpellLevel);
    SetField<uint8_t>(caster, kCreatureLastSpellLevel, static_cast<uint8_t>(slot));
    Run(code, self, spell.script);
    SetField<uint8_t>(caster, kCreatureLastSpellLevel, before);
    if (LogSlot()) {
      Log("spells2024: 0x%08X Healing Word (a level %d slot%s) on 0x%s: %s = %d + %d (spellcasting ability) = %d Hit "
          "Points", self, slot, cast.bonus ? ", a Bonus Action" : "", one.target.c_str(),
          Dice(dice, spell.die).c_str(), roll, modifier, amount);
    }
  }
}

struct OrbLook {
  const char* missile;
  const char* beam;
  const char* vis;
};

OrbLook OrbLookOf(uint32_t type) {
  switch (type) {
    case rules::kDamageTypeFire: return {"VFX_IMP_MIRV_FLAME", "VFX_BEAM_FIRE", "VFX_IMP_FLAME_S"};
    case rules::kDamageTypeLightning: return {"VFX_IMP_MIRV_ELECTRIC", "VFX_BEAM_LIGHTNING", "VFX_IMP_LIGHTNING_S"};
    case rules::kDamageTypeCold: return {"VFX_IMP_MIRV", "VFX_BEAM_COLD", "VFX_IMP_FROST_S"};
    case rules::kDamageTypeAcid: return {"VFX_IMP_MIRV", "VFX_BEAM_DISINTEGRATE", "VFX_IMP_ACID_S"};
    case rules::kDamageTypePoison: return {"VFX_IMP_MIRV", "VFX_BEAM_EVIL", "VFX_IMP_POISON_S"};
    default: return {"VFX_IMP_MIRV", "VFX_BEAM_MIND", "VFX_IMP_SONIC"};  // Thunder
  }
}

// Chromatic Orb's leap: the nearest enemy within 30 feet of the creature it hit, not hit yet this casting.
const char* const kLeapPick = R"nss(
object c = StringToObject("$FROM"); string skip = "$SKIP"; string r = "";
int i = 1;
object n = GetNearestCreature(CREATURE_TYPE_IS_ALIVE, TRUE, c, i);
while (GetIsObjectValid(n) && GetDistanceBetween(c, n) <= $RADIUS) {
  if (n != OBJECT_SELF && GetIsEnemy(n, OBJECT_SELF) && FindSubString(skip, "|" + ObjectToString(n) + "|") < 0) {
    r = ObjectToString(n) + ":0;";
    break;
  }
  i++;
  n = GetNearestCreature(CREATURE_TYPE_IS_ALIVE, TRUE, c, i);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

const char* const kLeapAttack = R"nss(
object t = StringToObject("$TARGET"); string r = "";
if (GetIsObjectValid(t)) {
  SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), TRUE));
  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam($BEAM, StringToObject("$FROM"), BODY_NODE_CHEST), t, 1.0);
  r = ObjectToString(t) + ":" + IntToString(TouchAttackRanged(t)) + ";";
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

void ChromaticOrb(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const rules::CantripOption* option = rules::ChoiceOptionOf(row);
  const rules::ChoiceCantrip* menu = rules::ChoiceCantripOf(row);
  const uint32_t type =
      option && menu && menu->master == rules::kChromaticOrb ? option->value : rules::kDamageTypeFire;  // no choice: Fire
  const OrbLook orb = OrbLookOf(type);
  Look look;
  look.beam = std::string("ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(") + orb.missile + "), t);";
  look.vis = orb.vis;
  if (type == rules::kDamageTypePoison) look.immune = LookOf(rules::kRayOfSickness).immune;
  std::vector<Entry> hits = OneTarget(self, spell, "TouchAttackRanged(t)", 1, look);
  if (hits.empty()) return;
  Entry hit = hits.front();
  std::string skip = "|";
  int leaps = rules::ChromaticOrbLeaps(slot);
  std::string log;
  for (;;) {
    skip += hit.target + "|";
    if (hit.value <= 0) {
      log += " 0x" + hit.target + " missed;";
      break;
    }
    const bool critical = hit.value == 2;
    const int dice = rules::CriticalDice(rules::SpellDiceAt(spell, slot), critical);
    std::vector<int> rolls;
    int amount = 0;
    std::string faces;
    for (int i = 0; i < dice; ++i) {
      const int roll = RollEngineDice(1, spell.die);
      rolls.push_back(roll);
      amount += roll;
      faces += (i ? "," : "") + std::to_string(roll);
    }
    Look applied = look;
    applied.beam.clear();
    const bool landed = Damage(self, spell, hit.target, amount, type, orb.vis, applied);
    if (landed) HuntersMarkSpellHit(self, ParseId(hit.target), critical);  // build 2a
    log += " 0x" + hit.target + (critical ? " a Critical Hit " : " hit ") + Dice(dice, spell.die) + " (" + faces + ") = " +
           std::to_string(amount) + " " + TypeName(type) + (landed ? "" : " - immune") + ";";
    if (!rules::HasMatchingDice(rolls)) break;
    if (leaps <= 0) {
      log += " no leaps left;";
      break;
    }
    std::string pick = kLeapPick;
    Replace(&pick, "$FROM", hit.target);
    Replace(&pick, "$SKIP", skip);
    Replace(&pick, "$RADIUS", Float(rules::FeetToMetres(rules::kChromaticOrbLeapFeet) + 2.0f * kSlack));
    Run(pick, self, spell.script);
    const std::vector<Entry> next = Entries(caster);
    if (next.empty()) {
      log += " matching dice, no one to leap to;";
      break;
    }
    --leaps;
    std::string attack = kLeapAttack;
    Replace(&attack, "$TARGET", next.front().target);
    Replace(&attack, "$FROM", hit.target);
    Replace(&attack, "$BEAM", orb.beam);
    Run(attack, self, spell.script);
    const std::vector<Entry> again = Entries(caster);
    if (again.empty()) break;
    log += " leaps ->";
    hit = again.front();
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Chromatic Orb (a level %d slot, %s, %d leaps at most):%s", self, slot, TypeName(type),
        rules::ChromaticOrbLeaps(slot), log.c_str());
  }
}

// Heroism's creatures: the one touched, then an upcast's - the nearest friends within 10 feet (you first).
const char* const kHeroismTargets = R"nss(
object t = GetSpellTargetObject(); string r = "";
if (GetIsObjectValid(t) && GetObjectType(t) == OBJECT_TYPE_CREATURE) r = ObjectToString(t) + ":" + IntToString(GetIsEnemy(t, OBJECT_SELF)) + ";";
int left = $EXTRA;
if (left > 0 && t != OBJECT_SELF) { r += ObjectToString(OBJECT_SELF) + ":0;"; left--; }
int i = 1;
object n = GetNearestCreature(CREATURE_TYPE_IS_ALIVE, TRUE, OBJECT_SELF, i);
while (left > 0 && GetIsObjectValid(n) && GetDistanceBetween(OBJECT_SELF, n) <= $REACH) {
  if (n != t && !GetIsEnemy(n, OBJECT_SELF) && (GetIsFriend(n, OBJECT_SELF) || GetFactionEqual(n, OBJECT_SELF))) {
    r += ObjectToString(n) + ":0;";
    left--;
  }
  i++;
  n = GetNearestCreature(CREATURE_TYPE_IS_ALIVE, TRUE, OBJECT_SELF, i);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

// One creature's Heroism: any fear on it ends; immune to it while the spell lasts (Concentration, 1 minute).
const char* const kHeroism = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), FALSE));
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) {
  int k = GetEffectType(e);
  if (GetEffectTag(e) == "n5s_heroism" || k == EFFECT_TYPE_FRIGHTENED ||
      (k == EFFECT_TYPE_ICON && GetEffectInteger(e, 0) == EFFECT_ICON_FRIGHTENED)) RemoveEffect(t, e);
  e = GetNextEffect(t);
}
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectImmunity(IMMUNITY_TYPE_FEAR), $ROW),
                    "n5s_heroism"), t, RoundsToSeconds(10));
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_HOLY), t);
)nss";

// Its turn's Temporary Hit Points (run as the creature: no spell's, so they stay when Heroism ends).
const char* const kHeroTemporary = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  if (GetEffectType(e) == EFFECT_TYPE_TEMPORARY_HITPOINTS) RemoveEffect(OBJECT_SELF, e);
  e = GetNextEffect(OBJECT_SELF);
}
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, SupernaturalEffect(EffectTemporaryHitpoints($AMOUNT)), OBJECT_SELF,
                    HoursToSeconds(8));
)nss";

void Heroism(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int temporary = rules::HeroismTemporaryHitPoints(SpellcastingModifier2024(caster));
  std::string code = kHeroismTargets;
  Replace(&code, "$EXTRA", std::to_string(rules::HeroismTargets(slot) - 1));
  Replace(&code, "$REACH", Float(rules::FeetToMetres(rules::kHeroismReachFeet) + 2.0f * kSlack));
  Run(code, self, spell.script);
  std::string log;
  for (const Entry& one : Entries(caster)) {
    if (one.value != 0) {
      TellPlayer(self, "nwn5e: Heroism - a willing creature, not an enemy.");
      log += " 0x" + one.target + " (an enemy: not)";
      continue;
    }
    std::string apply = kHeroism;
    Replace(&apply, "$TARGET", one.target);
    Replace(&apply, "$ROW", std::to_string(spell.row));
    Run(apply, self, spell.script);
    const uint32_t id = ParseId(one.target);
    if (g_heroes.size() > 256) g_heroes.clear();
    g_heroes[id] = Hero{self, temporary, CombatRoundsEnded(id), GameMs()};
    log += " 0x" + one.target;
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Heroism (a level %d slot, %d creatures at most): %d Temporary Hit Points a turn -%s", self,
        slot, rules::HeroismTargets(slot), temporary, log.c_str());
  }
}

bool HasHeroismFrom(const void* creature, uint32_t caster) {
  void* const* list = Field<void* const*>(creature, ee::kObjectEffects);
  const int count = Field<int32_t>(creature, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (effect && Field<int32_t>(effect, ee::kEffectSpellId) == rules::kHeroism &&
        Field<uint32_t>(effect, ee::kEffectCreator) == caster) {
      return true;
    }
  }
  return false;
}

// Heroism's turns: the start of each of the creature's rounds (out of combat, every 6 s) - its Temporary Hit Points.
void HeroUpdate(void* creature) {
  if (g_heroes.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto at = g_heroes.find(id);
  if (at == g_heroes.end()) return;
  Hero& hero = at->second;
  if (engine::IsDeadOrDying(creature) || !HasHeroismFrom(creature, hero.caster)) {
    if (g_hero_logs < 40 && ++g_hero_logs) Log("spells2024: 0x%08X Heroism (0x%08X's) ended", id, hero.caster);
    g_heroes.erase(at);
    return;
  }
  const uint32_t rounds = CombatRoundsEnded(id);
  const uint64_t now = GameMs();
  if (rounds == hero.rounds && now < hero.last + kRoundMs) return;
  hero.rounds = rounds;
  hero.last = now;
  const int current = Field<int16_t>(creature, ee::kObjectTemporaryHitPoints);
  if (hero.temporary <= 0 || !rules::TemporaryHitPointsReplace(current, hero.temporary)) return;
  std::string code = kHeroTemporary;
  Replace(&code, "$AMOUNT", std::to_string(hero.temporary));
  Run(code, id, "Heroism's Temporary Hit Points");
  if (g_hero_logs < 40 && ++g_hero_logs) {
    Log("spells2024: 0x%08X Heroism - its turn: %d Temporary Hit Points (had %d)", id, hero.temporary, current);
  }
}

// --- Build 2a: the buffs ---------------------------------------------------------------------------------------------

constexpr int kAbilityWisdom = 4;
constexpr const char* kTagSanctuary = "n5s_sanctuary";

// A creature's effect of a spell (either of two rows - a Bonus Action spell's own and its feat's), by `creator` when
// given (0: anyone's).
bool HasSpellEffect(const void* creature, int spell_a, int spell_b = -1, uint32_t creator = 0) {
  if (!creature) return false;
  void* const* list = Field<void* const*>(creature, ee::kObjectEffects);
  const int count = Field<int32_t>(creature, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (!effect) continue;
    const int spell = Field<int32_t>(effect, ee::kEffectSpellId);
    if ((spell == spell_a || (spell_b >= 0 && spell == spell_b)) &&
        (creator == 0 || Field<uint32_t>(effect, ee::kEffectCreator) == creator)) {
      return true;
    }
  }
  return false;
}

// A spell's mark on a creature: its effect (an icon - Protection from Evil and Good's with its immunities), tagged,
// with the row cast (Concentration finds it by that row and its caster); an earlier one of the same spell on it ends.
const char* const kMark = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t)) return;
if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), $HOSTILE));
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "$TAG") RemoveEffect(t, e); e = GetNextEffect(t); }
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId($EFFECT, $ROW), "$TAG"), t, $SECONDS);
$DURATION_VFX
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect($VIS), t);
)nss";

struct MarkLook {
  const char* tag;
  std::string effect;
  std::string seconds;
  const char* vis;
  const char* duration_vfx = "";  // a lasting visual of its own, beside the icon (Sanctuary's)
  bool hostile = false;
};

void Mark(uint32_t self, const rules::Spell2024& spell, const std::string& target, int row, const MarkLook& look) {
  std::string code = kMark;
  Replace(&code, "$DURATION_VFX",
          look.duration_vfx[0]
              ? std::string("ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectVisualEffect(") +
                    look.duration_vfx + "), $ROW), \"$TAG\"), t, $SECONDS);"
              : std::string());
  Replace(&code, "$TARGET", target);
  Replace(&code, "$HOSTILE", look.hostile ? "TRUE" : "FALSE");
  Replace(&code, "$TAG", look.tag);
  Replace(&code, "$EFFECT", look.effect);
  Replace(&code, "$ROW", std::to_string(row));
  Replace(&code, "$SECONDS", look.seconds);
  Replace(&code, "$VIS", look.vis);
  Run(code, self, spell.script);
}

const char* const kUnmark = R"nss(
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "$TAG") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }
)nss";

// Bless and Bane: up to $COUNT creatures ($WANT: friends, or enemies) nearest the spot aimed at, within 30 feet of you.
const char* const kGroup = R"nss(
object c = GetSpellTargetObject();
location l = GetSpellTargetLocation();
if (GetIsObjectValid(c)) l = GetLocation(c);
string r = ""; int left = $COUNT; int i = 1;
object n = GetNearestCreatureToLocation(CREATURE_TYPE_IS_ALIVE, TRUE, l, i);
while (left > 0 && GetIsObjectValid(n) && i < 40) {
  if (GetDistanceBetween(OBJECT_SELF, n) <= $RANGE && ($WANT)) { r += ObjectToString(n) + ":0;"; left--; }
  i++;
  n = GetNearestCreatureToLocation(CREATURE_TYPE_IS_ALIVE, TRUE, l, i);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

void BlessOrBane(uint32_t self, void* caster, const rules::Spell2024& spell, int row, int slot) {
  const bool bane = spell.row == rules::kBane;
  std::string code = kGroup;
  Replace(&code, "$COUNT", std::to_string(rules::BlessTargets(slot)));
  Replace(&code, "$RANGE", Float(rules::FeetToMetres(rules::kBlessFeet) + 2.0f * kSlack));
  Replace(&code, "$WANT", bane ? "GetIsEnemy(n, OBJECT_SELF)"
                               : "n == OBJECT_SELF || (!GetIsEnemy(n, OBJECT_SELF) && (GetIsFriend(n, OBJECT_SELF) || "
                                 "GetFactionEqual(n, OBJECT_SELF)))");
  Run(code, self, spell.script);
  const MarkLook look{bane ? "n5s_bane" : "n5s_bless",
                      bane ? "EffectIcon(EFFECT_ICON_ATTACK_DECREASE)" : "EffectIcon(EFFECT_ICON_ATTACK_INCREASE)",
                      "RoundsToSeconds(10)", bane ? "VFX_IMP_DOOM" : "VFX_IMP_HEAD_HOLY", "", bane};
  std::string log;
  for (const Entry& one : Entries(caster)) {
    if (bane) {
      const int save = SaveOf(self, spell, rules::Ability::kCharisma, one.target);  // C1: Bane's is Charisma
      if (save < 0) continue;
      log += " 0x" + one.target + (save == 0 ? " failed" : " saved");
      if (save != 0) continue;
    } else {
      log += " 0x" + one.target;
    }
    Mark(self, spell, one.target, row, look);
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s (a level %d slot, %d creatures at most):%s", self, spell.name, slot,
        rules::BlessTargets(slot), log.empty() ? " no one in range" : log.c_str());
  }
}

// Protection from Evil and Good: immune to Frightened and Charmed from those creature types (the engine's versus
// immunity: nwn5e's fear and charm ask it with their source).
std::string ProtectionEffect() {
  std::string link = "EffectIcon(EFFECT_ICON_IMMUNITY)";
  for (int type : {7, 16, 17, 20, 24}) {
    for (const char* immunity : {"IMMUNITY_TYPE_FEAR", "IMMUNITY_TYPE_CHARM"}) {
      link = "EffectLinkEffects(" + link + ", VersusRacialTypeEffect(EffectImmunity(" + immunity + "), " +
             std::to_string(type) + "))";
    }
  }
  return link;
}

struct Ward {
  uint32_t caster = 0;
  int dc = 10;
};
std::unordered_map<uint32_t, Ward> g_wards;         // Sanctuary: the warded creature -> its caster and DC
std::unordered_set<uint32_t> g_ward_ends;           // ... to end at the creature's next update
struct Quarry {
  uint32_t quarry = 0;
  int row = rules::kHuntersMark;  // the row cast (its marks carry it)
  int hours = 1;
};
std::unordered_map<uint32_t, Quarry> g_quarries;    // Hunter's Mark: its caster -> the marked creature
std::unordered_set<uint32_t> g_dashers;             // Expeditious Retreat's casters (its Dash feat granted)
std::unordered_set<uint32_t> g_striders;            // Longstrider's creatures

using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);

bool Warded(const void* creature, Ward* ward) {
  if (g_wards.empty() || !creature) return false;
  const auto at = g_wards.find(Field<uint32_t>(creature, ee::kObjectId));
  if (at == g_wards.end()) return false;
  if (!HasSpellEffect(creature, rules::kSanctuary, rules::kSanctuaryBonus)) {
    g_wards.erase(at);
    return false;
  }
  if (ward) *ward = at->second;
  return true;
}

void EndWard(uint32_t id, const char* why) {
  if (!g_wards.erase(id)) return;
  g_ward_ends.insert(id);
  TellPlayer(id, std::string("nwn5e: Sanctuary ends - you ") + why + ".");
  if (LogSlot()) Log("spells2024: 0x%08X Sanctuary ends - it %s", id, why);
}

void Sanctuary(uint32_t self, void* caster, const rules::Spell2024& spell, int row, const CastInfo& cast) {
  const int dc = SpellSaveDcForClassSlot(caster, cast.bonus ? cast.class_slot : -1);
  for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
    Mark(self, spell, one.target, row,
         MarkLook{kTagSanctuary, "EffectIcon(EFFECT_ICON_SANCTUARY)", "RoundsToSeconds(10)", "VFX_IMP_GOOD_HELP",
                  "VFX_DUR_SANCTUARY"});
    const uint32_t id = ParseId(one.target);
    if (g_wards.size() > 256) g_wards.clear();
    g_wards[id] = Ward{self, dc};
    g_ward_ends.erase(id);
    if (LogSlot()) Log("spells2024: 0x%08X Sanctuary on 0x%08X (DC %d, 1 minute)", self, id, dc);
  }
}

void HuntersMark(uint32_t self, void* caster, const rules::Spell2024& spell, int row, int slot) {
  const int hours = rules::HuntersMarkHours(slot);
  const std::string seconds = "HoursToSeconds(" + std::to_string(hours) + ")";
  for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
    char me[16];
    sprintf_s(me, "%x", self);
    Mark(self, spell, me, row, MarkLook{"n5s_hunter", "EffectIcon(EFFECT_ICON_DAMAGE_INCREASE)", seconds, "VFX_IMP_HEAD_ODD"});
    Mark(self, spell, one.target, row,
         MarkLook{"n5s_quarry", "EffectIcon(EFFECT_ICON_ENEMY_ATTACK_BONUS)", seconds, "VFX_IMP_HEAD_ODD", "", true});
    g_quarries[self] = Quarry{ParseId(one.target), row, hours};
    void* stats = StatsOf(caster);
    if (stats && !engine::HasFeat(stats, rules::kFeatHuntersMarkMove)) engine::AddFeat(stats, rules::kFeatHuntersMarkMove);
    if (LogSlot()) {
      Log("spells2024: 0x%08X Hunter's Mark (a level %d slot) on 0x%s - 1d6 Force on its hits, %d hour%s", self, slot,
          one.target.c_str(), hours, hours == 1 ? "" : "s");
    }
  }
}

bool QuarryDown(const Quarry& quarry) {
  void* creature = CreatureById(quarry.quarry);
  return !creature || engine::IsDeadOrDying(creature);
}

void HuntersMarkMove(uint32_t self, void* caster, const rules::Spell2024& spell) {
  const auto at = g_quarries.find(self);
  if (at == g_quarries.end() || !HasSpellEffect(caster, rules::kHuntersMark, rules::kHuntersMarkBonus, self)) {
    TellPlayer(self, "nwn5e: Hunter's Mark: Move - no Hunter's Mark of yours lasts.");
    return;
  }
  if (!QuarryDown(at->second)) {
    TellPlayer(self, "nwn5e: Hunter's Mark: Move - your quarry still stands.");
    return;
  }
  const std::string seconds = "HoursToSeconds(" + std::to_string(at->second.hours) + ")";
  for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
    Mark(self, spell, one.target, at->second.row,
         MarkLook{"n5s_quarry", "EffectIcon(EFFECT_ICON_ENEMY_ATTACK_BONUS)", seconds, "VFX_IMP_HEAD_ODD", "", true});
    at->second.quarry = ParseId(one.target);
    TellPlayer(self, "nwn5e: Hunter's Mark moves to a new quarry.");
    if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark moved to 0x%s", self, one.target.c_str());
  }
}

void ExpeditiousRetreat(uint32_t self, void* caster, const rules::Spell2024& spell, int row) {
  char me[16];
  sprintf_s(me, "%x", self);
  Mark(self, spell, me, row,
       MarkLook{"n5s_retreat", "EffectIcon(EFFECT_ICON_MOVEMENT_SPEED_INCREASE)", "TurnsToSeconds(10)", "VFX_IMP_HASTE"});
  StartDash(self);
  void* stats = StatsOf(caster);
  if (stats && !engine::HasFeat(stats, rules::kFeatExpeditiousDash)) engine::AddFeat(stats, rules::kFeatExpeditiousDash);
  g_dashers.insert(self);
  TellPlayer(self, "nwn5e: Expeditious Retreat - you Dash, and while it lasts you can Dash as a Bonus Action "
                   "(Expeditious Retreat: Dash).");
  if (LogSlot()) Log("spells2024: 0x%08X Expeditious Retreat - Dash now, a Bonus Action's Dash while it lasts", self);
}

void ExpeditiousDash(uint32_t self, void* caster) {
  if (!HasSpellEffect(caster, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, self)) {
    TellPlayer(self, "nwn5e: Expeditious Retreat: Dash - no Expeditious Retreat lasts.");
    return;
  }
  StartDash(self);
  RunScriptChunk("ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HASTE), OBJECT_SELF);", self);
  if (LogSlot()) Log("spells2024: 0x%08X Expeditious Retreat: Dash (a Bonus Action)", self);
}

void Buff(uint32_t self, void* caster, const rules::Spell2024& spell) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  if (row == rules::kExpeditiousRetreatDash) {  // Bonus Actions while a spell lasts: no slot
    ExpeditiousDash(self, caster);
    return;
  }
  if (row == rules::kHuntersMarkMove) {
    HuntersMarkMove(self, caster, spell);
    return;
  }
  const CastInfo cast = CastInfoOf(self, caster, spell.level, spell.name);
  if (!cast.ok) return;
  char me[16];
  sprintf_s(me, "%x", self);
  switch (spell.row) {
    case rules::kBless:
    case rules::kBane:
      BlessOrBane(self, caster, spell, row, cast.slot);
      return;
    case rules::kShieldOfFaith:
      for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
        Mark(self, spell, one.target, row,
             MarkLook{"n5s_faith", "EffectIcon(EFFECT_ICON_AC_INCREASE)", "TurnsToSeconds(10)", "VFX_IMP_AC_BONUS"});
        if (LogSlot()) Log("spells2024: 0x%08X Shield of Faith on 0x%s: +2 AC (Concentration)", self, one.target.c_str());
      }
      return;
    case rules::kMageArmor:
      for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
        void* target = CreatureById(ParseId(one.target));
        if (target && ChestArmorClassOf(target) > 0) {
          TellPlayer(self, "nwn5e: Mage Armor - a creature who isn't wearing armour.");
          continue;
        }
        Mark(self, spell, one.target, row,
             MarkLook{"n5s_magearmor", "EffectIcon(EFFECT_ICON_AC_INCREASE)", "HoursToSeconds(8)", "VFX_IMP_AC_BONUS"});
        if (LogSlot()) Log("spells2024: 0x%08X Mage Armor on 0x%s: base AC 13 + Dexterity, 8 hours", self, one.target.c_str());
      }
      return;
    case rules::kDivineFavor:
      Mark(self, spell, me, row,
           MarkLook{"n5s_favor", "EffectIcon(EFFECT_ICON_DAMAGE_INCREASE)", "RoundsToSeconds(10)", "VFX_IMP_HEAD_HOLY"});
      if (LogSlot()) Log("spells2024: 0x%08X Divine Favor: 1d4 Radiant on its weapon hits, 1 minute", self);
      return;
    case rules::kProtectionFromEvilAndGood:
      for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
        void* target = CreatureById(ParseId(one.target));
        if (target && target != caster && engine::IsEnemy(caster, ParseId(one.target))) {
          TellPlayer(self, "nwn5e: Protection from Evil and Good - a willing creature, not an enemy.");
          continue;
        }
        Mark(self, spell, one.target, row,
             MarkLook{"n5s_protection", ProtectionEffect(), "TurnsToSeconds(10)", "VFX_IMP_GOOD_HELP"});
        if (LogSlot()) Log("spells2024: 0x%08X Protection from Evil and Good on 0x%s", self, one.target.c_str());
      }
      return;
    case rules::kSanctuary:
      Sanctuary(self, caster, spell, row, cast);
      return;
    case rules::kExpeditiousRetreat:
      ExpeditiousRetreat(self, caster, spell, row);
      return;
    case rules::kLongstrider: {
      std::string code = kHeroismTargets;
      Replace(&code, "$EXTRA", std::to_string(rules::LongstriderTargets(cast.slot) - 1));
      Replace(&code, "$REACH", Float(rules::FeetToMetres(rules::kHeroismReachFeet) + 2.0f * kSlack));
      Run(code, self, spell.script);
      std::string log;
      for (const Entry& one : Entries(caster)) {
        Mark(self, spell, one.target, row,
             MarkLook{"n5s_strider", "EffectIcon(EFFECT_ICON_MOVEMENT_SPEED_INCREASE)", "HoursToSeconds(1)",
                      "VFX_IMP_HASTE"});
        g_striders.insert(ParseId(one.target));
        log += " 0x" + one.target;
      }
      if (LogSlot()) Log("spells2024: 0x%08X Longstrider (a level %d slot): +10 ft Speed, 1 hour -%s", self, cast.slot, log.c_str());
      return;
    }
    case rules::kHuntersMark:
      HuntersMark(self, caster, spell, row, cast.slot);
      return;
  }
}

// A Bonus Action spell's feat used on its own (not from the spell), and the Bonus Actions a spell gives while it
// lasts when it can't: refused before the Bonus Action is spent (false).
bool BuffFeatGuard(void* creature, int feat) {
  if (!Faithful() || !creature || t_bonus_cast) return true;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  for (const rules::BonusActionSpell& spell : rules::BonusActionSpells()) {
    if (feat != spell.feat) continue;
    TellPlayer(id, std::string("nwn5e: ") + spell.name + " - cast it from your spells (it spends a spell slot).");
    return false;
  }
  if (feat == rules::kFeatExpeditiousDash &&
      !HasSpellEffect(creature, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, id)) {
    TellPlayer(id, "nwn5e: Expeditious Retreat: Dash - no Expeditious Retreat lasts.");
    return false;
  }
  if (feat == rules::kFeatHuntersMarkMove) {
    const auto at = g_quarries.find(id);
    if (at == g_quarries.end() || !HasSpellEffect(creature, rules::kHuntersMark, rules::kHuntersMarkBonus, id)) {
      TellPlayer(id, "nwn5e: Hunter's Mark: Move - no Hunter's Mark of yours lasts.");
      return false;
    }
    if (!QuarryDown(at->second)) {
      TellPlayer(id, "nwn5e: Hunter's Mark: Move - your quarry still stands (the mark moves once it drops).");
      return false;
    }
  }
  return true;
}

// The attacker's Wisdom save against the target's Sanctuary (true: it goes on).
bool SanctuaryLetsThrough(void* attacker, void* target, const char* what) {
  Ward ward;
  if (!attacker || attacker == target || !Warded(target, &ward)) return true;
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));
  if (!save_roll) return true;
  RememberSaveDc2024(ward.caster, ward.dc);
  const uint8_t save = save_roll(attacker, ee::kSaveWill, static_cast<uint16_t>(ward.dc), 0, ward.caster, 1, 0, 1);
  if (save != 0) return true;
  const uint32_t id = Field<uint32_t>(attacker, ee::kObjectId);
  TellPlayer(id, std::string("nwn5e: Sanctuary - you can't bring yourself to ") + what +
                     " the warded creature; choose another target.");
  if (LogSlot()) {
    Log("spells2024: 0x%08X Sanctuary keeps 0x%08X from it (%s; Wisdom save failed, DC %d)", Field<uint32_t>(target, ee::kObjectId),
        id, what, ward.dc);
  }
  return false;
}

// An attack roll at a warded creature: the attacker's save, or the attack is lost; the warded creature attacking ends
// its Sanctuary.
bool SanctuarySkip(AttackRoll& roll, uintptr_t /*caller*/) {
  if (!Faithful() || g_wards.empty() || !roll.attacker || !roll.target || !roll.data) return false;
  const uint32_t attacker = Field<uint32_t>(roll.attacker, ee::kObjectId);
  if (g_wards.count(attacker)) EndWard(attacker, "attacked");
  void* target = CreatureById(Field<uint32_t>(roll.target, ee::kObjectId));
  if (!target || SanctuaryLetsThrough(roll.attacker, target, "attack")) return false;
  roll.data[ee::kAttackD20] = 0;
  SetField<int32_t>(roll.data, ee::kAttackModifier, 0);
  roll.data[ee::kAttackResult] = ee::kAttackResultMiss;
  return true;
}

void BuffCastHeard(void* creature, uint32_t spell) {
  if (g_wards.empty() || !creature) return;
  if (static_cast<int>(spell) == rules::kSanctuary || static_cast<int>(spell) == rules::kSanctuaryBonus) return;
  EndWard(Field<uint32_t>(creature, ee::kObjectId), "cast a spell");
}

void BuffDamageHeard(uint32_t creator, void* target, int damage) {
  if (g_wards.empty() || damage <= 0 || !target || creator == Field<uint32_t>(target, ee::kObjectId)) return;
  if (g_wards.count(creator)) EndWard(creator, "dealt damage");
}

// A weapon hit: Divine Favor's 1d4 Radiant; an attack roll's hit on Hunter's Mark's quarry, 1d6 Force (twice the dice
// on a Critical Hit).
void BuffHit(void* attacker, void* target) {
  if (!Faithful() || !attacker || !target) return;
  uint8_t* data = CurrentAttackData(attacker);
  if (!data || !rules::MasteryAttackHit(data[ee::kAttackResult])) return;
  const bool critical = data[ee::kAttackResult] == ee::kAttackResultCritical;
  const uint32_t id = Field<uint32_t>(attacker, ee::kObjectId);
  auto* entries = reinterpret_cast<int16_t*>(data + ee::kAttackDamageEntries);
  if (engine::ItemInSlot(attacker, ee::kSlotRightHand) &&
      HasSpellEffect(attacker, rules::kDivineFavor, rules::kDivineFavorBonus, id)) {
    const int dice = rules::CriticalDice(1, critical);
    const int extra = RollEngineDice(dice, rules::kDivineFavorDie);
    rules::AddDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeRadiant, extra);
    if (LogSlot()) Log("spells2024: 0x%08X Divine Favor: %dd4 Radiant = %d", id, dice, extra);
  }
  const auto quarry = g_quarries.find(id);
  if (quarry != g_quarries.end() && quarry->second.quarry == Field<uint32_t>(target, ee::kObjectId) &&
      HasSpellEffect(attacker, rules::kHuntersMark, rules::kHuntersMarkBonus, id)) {
    const int dice = rules::CriticalDice(1, critical);
    const int extra = RollEngineDice(dice, rules::kHuntersMarkDie);
    rules::AddDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeForce, extra);
    if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: %dd6 Force = %d", id, dice, extra);
  }
}

float LongstriderSpeed(void* creature, float factor) {
  if (g_striders.empty() || !creature || !Faithful()) return factor;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (!g_striders.count(id) || Field<int32_t>(creature, ee::kCreatureFixedMovementRate) == 1) return factor;
  if (!HasSpellEffect(creature, rules::kLongstrider)) {
    g_striders.erase(id);
    return factor;
  }
  if (OverloadedSpeedCap(creature)) return factor;
  return factor + static_cast<float>(rules::kLongstriderFeet) / 30.0f;
}

// A creature's update: a Sanctuary that ended loses its marks; a spell that ended takes back its Bonus Action feat.
void BuffUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (!g_ward_ends.empty() && g_ward_ends.erase(id)) {
    std::string code = kUnmark;
    Replace(&code, "$TAG", kTagSanctuary);
    Run(code, id, "Sanctuary's end");
  }
  if (!g_quarries.empty()) {
    const auto at = g_quarries.find(id);
    if (at != g_quarries.end() && !HasSpellEffect(creature, rules::kHuntersMark, rules::kHuntersMarkBonus, id)) {
      g_quarries.erase(at);
      if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatHuntersMarkMove);
      if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark ended", id);
    }
  }
  if (!g_dashers.empty() && g_dashers.count(id) &&
      !HasSpellEffect(creature, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, id)) {
    g_dashers.erase(id);
    if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatExpeditiousDash);
    if (LogSlot()) Log("spells2024: 0x%08X Expeditious Retreat ended", id);
  }
}

void Cast(uint32_t self, int row) {
  const rules::Spell2024* spell = rules::Spell2024Of(row);
  void* caster = CreatureById(self);
  if (!spell || !caster) return;
  if (row == rules::kHealingWord) {
    HealingWord(self, caster, *spell);  // its slot: the order's (a Bonus Action) or the cast's
    return;
  }
  if (spell->effect == rules::SpellEffect2024::kBuff && row != rules::kHeroism) {
    Buff(self, caster, *spell);  // build 2a
    return;
  }
  const int slot = SlotLevelOf(caster, *spell);
  if (row == rules::kChromaticOrb) {
    ChromaticOrb(self, caster, *spell, slot);
    return;
  }
  if (row == rules::kHeroism) {
    Heroism(self, caster, *spell, slot);
    return;
  }
  switch (spell->effect) {
    case rules::SpellEffect2024::kSpellAttack: Attack(self, caster, *spell, slot); break;
    case rules::SpellEffect2024::kDarts: Darts(self, *spell, slot); break;
    case rules::SpellEffect2024::kHealing: CureWounds(self, caster, *spell, slot); break;
    case rules::SpellEffect2024::kTemporaryHitPoints: FalseLife(self, caster, *spell, slot); break;
    case rules::SpellEffect2024::kBuff: break;
    case rules::SpellEffect2024::kSave:
      if (spell->shape == rules::SpellShape2024::kOne) {
        SaveOne(self, caster, *spell, slot);
      } else {
        FromCaster(self, caster, *spell, slot);
      }
      break;
  }
}

template <int Row>
void SpellScript(uint32_t self) {
  Cast(self, Row);
}

ScriptHandler HandlerOf(int row) {
  switch (row) {
    case rules::kBurningHands: return &SpellScript<rules::kBurningHands>;
    case rules::kCureWounds: return &SpellScript<rules::kCureWounds>;
    case rules::kMagicMissile: return &SpellScript<rules::kMagicMissile>;
    case rules::kInflictWounds: return &SpellScript<rules::kInflictWounds>;
    case rules::kThunderwave: return &SpellScript<rules::kThunderwave>;
    case rules::kIceKnife: return &SpellScript<rules::kIceKnife>;
    case rules::kGuidingBolt: return &SpellScript<rules::kGuidingBolt>;
    case rules::kRayOfSickness: return &SpellScript<rules::kRayOfSickness>;
    case rules::kFalseLife: return &SpellScript<rules::kFalseLife>;
    case rules::kDissonantWhispers: return &SpellScript<rules::kDissonantWhispers>;
    case rules::kHealingWord: return &SpellScript<rules::kHealingWord>;
    case rules::kChromaticOrb: return &SpellScript<rules::kChromaticOrb>;
    case rules::kHeroism: return &SpellScript<rules::kHeroism>;
    case rules::kBless: return &SpellScript<rules::kBless>;
    case rules::kBane: return &SpellScript<rules::kBane>;
    case rules::kShieldOfFaith: return &SpellScript<rules::kShieldOfFaith>;
    case rules::kMageArmor: return &SpellScript<rules::kMageArmor>;
    case rules::kDivineFavor: return &SpellScript<rules::kDivineFavor>;
    case rules::kProtectionFromEvilAndGood: return &SpellScript<rules::kProtectionFromEvilAndGood>;
    case rules::kSanctuary: return &SpellScript<rules::kSanctuary>;
    case rules::kExpeditiousRetreat: return &SpellScript<rules::kExpeditiousRetreat>;
    case rules::kLongstrider: return &SpellScript<rules::kLongstrider>;
    case rules::kHuntersMark: return &SpellScript<rules::kHuntersMark>;
  }
  return nullptr;
}

// Guiding Bolt's glow comes off at the creature's next update once its Advantage is used.
void GlowUpdate(void* creature) {
  if (g_glow_off.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (!g_glow_off.erase(id)) return;
  Run(kGlowOff, id, "Guiding Bolt's glow");
}

}  // namespace

// rules_spell_slots.cpp's UnReady: a spell slot spent - this turn's (One Spell with a Spell Slot per Turn).
void NoteSlotSpent(void* creature, int level) {
  if (!creature || level < 1) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (g_slot_turns.size() > 512) g_slot_turns.clear();
  g_slot_turns[id] = SlotTurn{CombatRoundsEnded(id), GameMs(), t_bonus_spend};
}

// rules_attack.cpp, rules_spell_attack.cpp, rules_save_roll.cpp (build 2a): Bless's +1d4 and Bane's -1d4 on a roll.
int BlessDie(void* creature, const char* what) {
  if (!Faithful() || !creature) return 0;
  const bool bless = HasSpellEffect(creature, rules::kBless);
  const bool bane = HasSpellEffect(creature, rules::kBane);
  if (!bless && !bane) return 0;
  int total = 0;
  std::string note;
  if (bless) {
    const int die = RollEngineDice(1, rules::kBlessDie);
    total += die;
    note = "Bless +" + std::to_string(die);
  }
  if (bane) {
    const int die = RollEngineDice(1, rules::kBlessDie);
    total -= die;
    note += (note.empty() ? "Bane -" : ", Bane -") + std::to_string(die);
  }
  if (LogSlot()) Log("spells2024: 0x%08X %s on its %s", Field<uint32_t>(creature, ee::kObjectId), note.c_str(), what);
  return total;
}

int BlessAttackBonus(void* stats) {
  return Faithful() && stats && CurrentAttackRoll() ? BlessDie(CreatureOf(stats), "attack roll") : 0;
}

int BlessSaveDie(void* creature) {
  const int die = BlessDie(creature, "save");
  if (die != 0) {
    char line[160];
    sprintf_s(line, "nwn5e: Bless / Bane: %+d on that save (the DC it shows is moved by that).", die);
    TellPlayer(Field<uint32_t>(creature, ee::kObjectId), line);
  }
  return die;
}

// rules_attack.cpp's AC and the sheet's (build 2a): Shield of Faith's +2; Mage Armor's base AC 13 + Dexterity for a
// character wearing no armour (the engine's 10 + Dexterity, raised by what Unarmored Defense doesn't already give).
int SpellArmorClassBonus(void* stats, bool character) {
  void* creature = stats ? CreatureOf(stats) : nullptr;
  if (!Faithful() || !creature) return 0;
  int bonus = HasSpellEffect(creature, rules::kShieldOfFaith, rules::kShieldOfFaithBonus) ? rules::kShieldOfFaithAc : 0;
  if (character && HasSpellEffect(creature, rules::kMageArmor) && ChestArmorClassOf(creature) <= 0) {
    const int monk_wis = ClassLevel2024(stats, rules::CharacterClass::kMonk) > 0 && !HoldsShieldOf(creature)
                             ? std::max(0, engine::AbilityModifier(stats, kAbilityWisdom))
                             : 0;
    const int over = rules::MageArmorBaseAc(0) - 10 - UnarmoredDefenseBonus(stats) - monk_wis;
    if (over > 0) bonus += over;
  }
  return bonus;
}

// rules_advantage.cpp (build 2a): Protection from Evil and Good - Disadvantage on the attack rolls of the creature
// types it wards against.
MasteryRollSources ProtectionRollSources(void* attacker, void* target) {
  MasteryRollSources sources;
  if (!Faithful() || !attacker || !target || !HasSpellEffect(target, rules::kProtectionFromEvilAndGood)) return sources;
  void* stats = StatsOf(attacker);
  if (!stats || !rules::WardedAgainstRacialType(engine::RaceOf(stats))) return sources;
  sources.disadvantage = 1;
  sources.note = "Protection from Evil and Good";
  return sources;
}

// Hunter's Mark on a spell attack's hit on the quarry (this file's spells, rules_cantrips.cpp's attack cantrips):
// 1d6 Force, twice on a Critical Hit.
void HuntersMarkSpellHit(uint32_t caster, uint32_t target, bool critical) {
  if (g_quarries.empty()) return;
  const auto at = g_quarries.find(caster);
  if (at == g_quarries.end() || at->second.quarry != target) return;
  void* creature = CreatureById(caster);
  if (!creature || !HasSpellEffect(creature, rules::kHuntersMark, rules::kHuntersMarkBonus, caster)) return;
  const int dice = rules::CriticalDice(1, critical);
  const int extra = RollEngineDice(dice, rules::kHuntersMarkDie);
  char code[256];
  sprintf_s(code, "object t = StringToObject(\"%x\"); if (GetIsObjectValid(t)) "
            "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectDamage(%d, %u), t);", target, extra,
            rules::kDamageTypeForce);
  RunScriptChunk(code, caster);
  if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark on a spell attack: %dd6 Force = %d", caster, dice, extra);
}

// rules_advantage.cpp: Guiding Bolt - Advantage on the next attack roll against `target` (used up by it).
MasteryRollSources GuidingBoltRollSources(void* target) {
  MasteryRollSources sources;
  if (!target || g_guided.empty()) return sources;
  const uint32_t id = Field<uint32_t>(target, ee::kObjectId);
  const auto at = g_guided.find(id);
  if (at == g_guided.end()) return sources;
  const bool live = GameMs() < at->second;
  g_guided.erase(at);
  g_glow_off.insert(id);
  if (!live) return sources;
  sources.advantage = 1;
  sources.note = "Guiding Bolt";
  return sources;
}

void InstallSpells2024() {
  if (!Faithful()) {
    Log("spells2024: NWN's levelled spells (key spellcasting)");
    return;
  }
  ServeConditionalRows("spells2024");  // spells.if_spells2024.level1.txt (rules_spell_slots.cpp asks too)
  bool scripts = true;
  for (const rules::Spell2024& spell : rules::Spells2024()) {
    const ScriptHandler handler = HandlerOf(spell.row);
    scripts = handler && AddScriptHandler(spell.script, handler) && scripts;
  }
  const bool update = AddCreatureUpdateListener(&GlowUpdate) && AddCreatureUpdateListener(&HeroUpdate);
  const bool buffs = AddFeatUseGuard(&BuffFeatGuard) && AddAttackRollSkipper(&SanctuarySkip) &&
                     AddResolveDamageListener(&BuffHit) && HookMovementRate() &&
                     AddMovementRateBonus(&LongstriderSpeed) && AddDamageListener(&BuffDamageHeard) &&
                     AddSpellCastListener(&BuffCastHeard) && AddCreatureUpdateListener(&BuffUpdate);
  g_use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through its hook
  g_spell_row = reinterpret_cast<SpellRowFn>(EngineExport(ee::kGetSpell));
  g_rules_for_rows = reinterpret_cast<void**>(EngineExport(ee::kRules));
  const bool guard = AddSpellOrderGuard(&OrderGuard) && g_use_feat && g_spell_row && g_rules_for_rows;
  Log("spells2024: SRD 5.2's level 1 spells, builds 1a-2a - %zu spells: damage and healing, Chromatic Orb, Heroism, "
      "the buffs; %zu Bonus Action spells; one spell with a slot a turn (#39 Part 2a)%s",
      rules::Spells2024().size(), rules::BonusActionSpells().size(),
      scripts && update && guard && buffs ? "" : " - SOME hooks missing");
}
