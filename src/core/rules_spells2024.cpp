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
//     0.99.15 (owner, 2026-11-11: "go with A for sanctuary"): a player's Sanctuary on itself stops its attacks
//     (extra_actions' StopOwnAttacks, the Cunning Action Hide's stop) - NWN kept it swinging, and its next swing
//     ended the ward.
//   - Expeditious Retreat (a Bonus Action): a Dash now, and a Bonus Action's Dash while it lasts (feat 1561).
//   - Longstrider: +10 feet of Speed (a third of 30 ft) for an hour; an upcast's extra creatures as Heroism's.
//   The Bonus Action spells all cast as Healing Word does (rules::BonusActionSpells); their feats used on their own
//   are refused before the Bonus Action is spent (a feat-use guard).
// - Build 2b (owner, 2026-11-12: "2a accepted, go for 2b"; then "go with B for Q1 and A for Q2. A for Q2 could be
//   the same help action, just renamed"), conditions on creatures - each a tagged effect with the row cast, made by
//   the caster (Concentration finds it):
//   - Command 1170 (its word from the menu, 1171-1174): a Wisdom save or, for its next round, Drop (what it holds
//     into its inventory, its actions locked), Flee (it runs 60 feet farther away, locked), Grovel (a round's
//     knockdown) or Halt (held in place, locked); one more creature a slot level - the enemies nearest the one aimed
//     at.
//   - Charm Person 16 (a Humanoid) and Animal Friendship 1175 (a Beast): a Wisdom save (Charm Person's with
//     Advantage while you fight it) or EffectCharmed (#106's 2024 Charmed on a player) for 1 / 24 game hours, ended
//     by damage from its caster or the caster's allies; another creature type is refused before the cast.
//   - Hideous Laughter 457 (level 1 now): a Wisdom save or knocked down (Prone, Incapacitated) for the minute; a
//     save at each of its rounds' ends and each time it takes damage (that one with Advantage) ends it.
//   - Color Spray 24: every creature in the 15-foot Cone, a Constitution save or Blinded for two rounds.
//   - Sleep 165: the enemies in a 5-foot-radius Sphere, a Wisdom save or dazed (Incapacitated) for a round, then
//     the save again - a failure, asleep (Unconscious) for the rest of the minute; damage, or Rouse (the Help action
//     renamed, everyone's: an Action on a sleeper within 5 feet - staple_actions.cpp), wakes it. Elves,
//     Constructs, Elementals, Oozes, Undead and NWN's sleep-immune aren't affected.
// - Build 2c (owner, 2026-11-12: "2b accepted, go for 2c"; then "All recommended except Q3, go for B"), the areas -
//   each made in the spell's context (EffectAreaOfEffect, vfx_persistent.if_spells2024.txt's rows, its scripts
//   ours): Concentration finds it and its effects.
//   - Entangle 53: a 20-foot square of Difficult Terrain; each creature in it at the cast but you, a Strength save
//     or Restrained (EffectEntangle) until the spell ends. Q1 A: Escape (feat 1563, an Action - granted while it's
//     held: the quickbar, the radial's Special Attacks page in Dash's place) makes the Strength (Athletics) check;
//     an NPC tries once a round in place of its attacks (the Grapple's way).
//   - Grease 66: a 10-foot square of Difficult Terrain; a Dexterity save or Prone (a round knocked down) as it
//     appears, on entering, at each round's end there.
//   - Faerie Fire 1176 (its colour from the menu, Q4 A): each creature in the 20-foot Cube, a Dexterity save or
//     outlined - a glow and a light; attack rolls against it with Advantage (rules_advantage.cpp) while the
//     attacker can see it; Q3 B: its invisibility suppressed (a holder with its spell and time in its place:
//     Concentration holds) and back for the time it had left.
//   - Fog Cloud 1180: a 20-foot Sphere (20 more a slot level; vfx_persistent rows 107-115) - Q2 A: NWN's darkness
//     on those inside (they can't see out, no one sees them; hiding there as in the dark - rules_perception.cpp).
//   - Difficult Terrain: half Speed inside (a movement rate adjuster). The hazard-avoiding AI's reading of the 2c
//     areas: hazard_watch.cpp.
// - Build 3 (owner, 2026-11-12: "2c accepted, go for 3"; then "go with A on all"), reactions and smites:
//   - Shield 417 (Sorcerer, Wizard): a Reaction with its switch (!options) - when a hit by an attack roll (or a
//     spell attack: rules_spell_attack.cpp asks at its AC) would miss with +5 AC (never a natural 20), or Magic
//     Missile is aimed at it, a creature with Shield prepared and a slot (the lowest that casts it; an NPC's own use
//     too, Q3 A) takes it: +5 AC until its next round, the hit a miss. Not this turn's one spell with a slot. Cast
//     from the spells: +5 AC for a round. Feather Fall: a visual and a line (#153).
//   - The smites (Q1 A): Divine Smite, Searing Smite (Paladin), Ensnaring Strike (Ranger) - the cast arms it for
//     nothing; the next hit it rides (a melee weapon or an Unarmed Strike; Ensnaring Strike any weapon), while the
//     slot and the Bonus Action are there, spends both. Divine Smite 2d8 Radiant (+1d8 a Fiend or an Undead, +1d8 a
//     level); Searing Smite 1d6 Fire and a burn each turn until a Constitution save; Ensnaring Strike a Strength
//     save (Advantage if Large or larger) or Restrained (Concentration), 1d6 Piercing each turn, Escape (Q4 A).
// NWN mode: NWN's spells.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "action_guard.h"
#include "anim_look_player.h"
#include "attack_roll.h"
#include "bonus_actions.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "damage_listeners.h"
#include "data_serving.h"
#include "dice.h"
#include "extra_actions.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "records.h"
#include "game_clock.h"
#include "hooks.h"
#include "item_use.h"
#include "log.h"
#include "mastery.h"
#include "movement_rate.h"
#include "reactions.h"
#include "resolve_damage.h"
#include "resource_panel.h"
#include "saves.h"
#include "script_hooks.h"
#include "spell_cast_listeners.h"
#include "spell_impact.h"
#include "staple_actions.h"
#include "rules/cantrips.h"
#include "condition_effects.h"
#include "rest.h"
#include "player_text.h"
#include "rules/hazard_avoid.h"
#include "rules/level_lists.h"
#include "rules/skills.h"
#include "rules/spell_slots.h"
#include "rules/spell_time.h"
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
void HuntersMarkHit(uint32_t caster, uint32_t target, bool critical, float delay);  // #155: its damage delayed
void* GameObjectById(uint32_t id);  // light_sources.cpp (build 2c: an area of effect by its id)
bool InFogCloud(void* creature);    // below (build 2c; #160's fog search)
void BeginConcentration(void* creature, int spell);  // rules_concentration.cpp (build 3: Ensnaring Strike's)
int AreaOfEffectSpellId(uint32_t id);               // rules_concentration.cpp (build 4a: Detect Magic's areas)
// rules_utility_cantrips.cpp (build 4b): Silent Image's image - made, ended, moved (Minor Illusion's machinery).
uint32_t MakeSilentImage(uint32_t self, void* caster, rules::ImageKind kind, const std::string& name);
void EndSilentImage(uint32_t self);
uint32_t MoveSilentImage(uint32_t self, void* caster, float x, float y, float z);
// rules_familiar.cpp (build 4c): Find Familiar.
void FindFamiliarSpell(uint32_t self, void* caster, int row);
bool InstallFindFamiliar();
void FloatingDiskSpell(uint32_t self, void* caster, int row);  // rules_floating_disk.cpp (build 4d)
bool InstallFloatingDisk();

namespace {

// #39 (owner, 2026-11-17: "go with A. no real-time clock"): a spell's duration in a script is real seconds
// (rules::SpellSeconds) - not the module's clock (HoursToSeconds: an hour 2 real minutes here).
std::string SecondsScript(int minutes) { return std::to_string(rules::SpellSeconds(minutes)) + ".0"; }

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
    case rules::Ability::kStrength: return "FortitudeSave";  // build 2c: the save roll makes it Strength's (SaveOf)
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

// One creature's save (-1: gone): 0 failed, 1 succeeded, 2 immune ($IMMUNE: immune before the roll - Sleep's).
const char* const kSave = R"nss(
object t = StringToObject("$TARGET"); int s = -1;
if (GetIsObjectValid(t) && !GetIsDead(t)) {
  if (t != OBJECT_SELF) SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), $HARMFUL));
  s = 2;
  if (!($IMMUNE)) s = $SAVE(t, GetSpellSaveDC(), $STYPE, OBJECT_SELF);
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
$APPLY
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
                    $ROW)), OBJECT_SELF, 28800.0);  // 8 hours, real time
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_EVIL_HELP), OBJECT_SELF);
)nss";

// Magic Missile: the darts, each its own damage, as NWN's own fly (VFX_IMP_MIRV, the travel time by distance) - unless
// a look for its row flies something (0.99.17, Darts below).
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
// `delay` > 0 (#155: a spell look's flight): the damage lands that many seconds from now, a command held by the
// target (it lands though the caster dies meanwhile; the caster stays its creator), `vis` with it unless empty.
bool Damage(uint32_t self, const rules::Spell2024& spell, const std::string& target, int amount, uint32_t type,
            const std::string& vis, const Look& look, float delay = 0.0f) {
  if (amount <= 0) return true;
  void* caster = CreatureById(self);
  if (caster) engine::SetLocalInt(caster, kVarSave, 0);
  std::string code = kDamage;
  Replace(&code, "$IMMUNE", look.immune);
  const std::string effect = vis.empty() ? "EffectDamage($AMOUNT, $TYPE)"
                                         : "EffectLinkEffects(EffectDamage($AMOUNT, $TYPE), EffectVisualEffect($VIS))";
  Replace(&code, "$APPLY",
          delay > 0.0f ? "effect d = SetEffectSpellId(" + effect + ", $ROW); AssignCommand(t, DelayCommand(" +
                             Float(delay) + ", ApplyEffectToObject(DURATION_TYPE_INSTANT, d, t)));"
                       : "ApplyEffectToObject(DURATION_TYPE_INSTANT, SetEffectSpellId(" + effect + ", $ROW), t);");
  Replace(&code, "$RIDER", look.rider);
  Replace(&code, "$TARGET", target);
  Replace(&code, "$AMOUNT", std::to_string(amount));
  Replace(&code, "$TYPE", std::to_string(type));
  Replace(&code, "$VIS", vis);
  Replace(&code, "$ROW", std::to_string(spell.row));
  Run(code, self, spell.script);
  return !(caster && engine::GetLocalInt(caster, kVarSave) == 2);
}

// Build 2b: `type` the engine's save type (SAVING_THROW_TYPE_MIND_SPELLS: NWN's mind immunity answers 2), `harmful`
// the spell's event (a charm's isn't), `advantage` a spell's Advantage on the roll, `immune` a test before it.
struct SaveOptions {
  const char* type = "SAVING_THROW_TYPE_NONE";
  bool harmful = true;
  bool advantage = false;
  const char* immune = "FALSE";
};

int SaveOf(uint32_t self, const rules::Spell2024& spell, rules::Ability ability, const std::string& target,
           const SaveOptions& options = SaveOptions()) {
  void* caster = CreatureById(self);
  if (!caster) return -1;
  std::string code = kSave;
  Replace(&code, "$TARGET", target);
  Replace(&code, "$SAVE", SaveCall(ability));
  Replace(&code, "$STYPE", options.type);
  Replace(&code, "$HARMFUL", options.harmful ? "TRUE" : "FALSE");
  Replace(&code, "$IMMUNE", options.immune);
  SetForcedSaveAdvantage(options.advantage);
  if (ability == rules::Ability::kStrength) SetForcedSaveAbility(rules::Ability::kStrength);  // build 2c: Entangle's
  Run(code, self, spell.script);
  SetForcedSaveAbility(std::nullopt);
  SetForcedSaveAdvantage(false);
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
        TellPlayer(self, PlayerText("spells2024.guiding_bolt_the_next_attack"));
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

bool TakeShield(void* defender, const char* what);  // build 3, below

void Darts(uint32_t self, const rules::Spell2024& spell, int slot) {
  const std::vector<Entry> targets = OneTarget(self, spell, "GetHasSpellEffect(" + std::to_string(rules::kNwnShield) +
                                                              ", t)", 1, Look());
  for (const Entry& one : targets) {
    // Build 3: Shield's Reaction - "targeted by the Magic Missile spell".
    void* aimed = one.value == 0 ? CreatureById(ParseId(one.target)) : nullptr;
    if (one.value != 0 || (aimed && TakeShield(aimed, "Magic Missile"))) {
      std::string code = kShielded;
      Replace(&code, "$TARGET", one.target);
      Run(code, self, spell.script);
      if (LogSlot()) Log("spells2024: 0x%08X Magic Missile at 0x%s: its Shield stops the darts", self, one.target.c_str());
      continue;
    }
    const int darts = rules::MagicMissileDarts(slot);
    // 0.99.17 (#155; owner, 2026-11-11: "go with A for magic missile"): a look for the row that flies something
    // (SpellLookFlies - its missile set) flies each dart, and each dart's damage lands as that dart does (held by
    // the target, as Chromatic Orb's); its own hit effect plays there, so no VFX_IMP_MAGBLUE. No look, or one with
    // no missile: NWN's own bolts below, as before.
    if (SpellLookFlies(spell.row)) {
      const std::vector<float> lands =
          PlaySpellLookDarts(self, std::vector<uint32_t>(static_cast<size_t>(darts), ParseId(one.target)), spell.row);
      if (lands.size() == static_cast<size_t>(darts)) {
        std::string landed;
        for (int i = 0; i < darts; ++i) {
          const int amount = RollEngineDice(spell.dice, spell.die) + spell.flat;
          Damage(self, spell, one.target, amount, spell.damage_type, "", Look(), lands[static_cast<size_t>(i)]);
          char text[48];
          snprintf(text, sizeof text, "%s%d in %.2f s", i ? ", " : "", amount, lands[static_cast<size_t>(i)]);
          landed += text;
        }
        if (LogSlot()) {
          Log("spells2024: 0x%08X Magic Missile (a level %d slot) at 0x%s: %d darts of its look, 1d4 + 1 Force each - "
              "%s", self, slot, one.target.c_str(), darts, landed.c_str());
        }
        continue;
      }
      if (LogSlot()) {
        Log("spells2024: 0x%08X Magic Missile: its look played %zu of %d darts - NWN's bolts instead", self,
            lands.size(), darts);
      }
    }
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
    TellPlayer(self, PlayerText("spells2024.false_life_temporary_hit_points", {{"amount", std::string(std::to_string(amount))}, {"current", std::string(std::to_string(current))}}));
  } else {
    std::string code = kTemporary;
    Replace(&code, "$AMOUNT", std::to_string(amount));
    Replace(&code, "$ROW", std::to_string(spell.row));
    Run(code, self, spell.script);
    TellPlayer(self, PlayerText("spells2024.false_life_temporary_hit_points_2", {{"amount", std::string(std::to_string(amount))}}));
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
  TellPlayer(id, PlayerText("spells2024.refused", {{"name", name}, {"why", why}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s at 0x%08X refused - %s", id, name.c_str(), target, why.c_str());
  return true;
}

bool SanctuaryLetsThrough(void* attacker, void* target, const char* what);  // build 2a, below
bool ArmSmite(void* creature, uint32_t id, const SpellOrder& order);         // build 3, below
bool RitualOrder(void* creature, uint32_t id, const SpellOrder& order);      // build 4a, below
bool SilentImageMoveOrder(void* creature, uint32_t id, const SpellOrder& order);  // build 4b, below
bool SilentImageLasts(uint32_t id);                                               // build 4b (0.99.47), below
bool LongCastOrder(void* creature, uint32_t id, const SpellOrder& order);        // build 4c, below

// Refused before the cast, nothing spent (true): a willing creature's spell at an enemy, Mage Armor on one wearing
// armour, an enemy's spell at a creature its Sanctuary keeps it from. A Bonus Action spell's order becomes its feat's
// use (instant, a Bonus Action: Healing Word's way, every one's since build 2a); a slot spell after a Bonus Action one
// the same turn is refused.
bool OrderGuard(void* creature, const SpellOrder& order) {
  if (!Faithful() || !creature || order.fake || t_bonus_cast) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const int spell = static_cast<int>(order.spell);
  void* target = CreatureById(order.target);
  // Build 3 (Q1 A): a smite's cast arms it, nothing spent - the next qualifying hit pays.
  if (spell == rules::kDivineSmite || spell == rules::kSearingSmite || spell == rules::kEnsnaringStrike) {
    return ArmSmite(creature, id, order);
  }
  // Build 4a (Q1 A): a Ritual spell from a class out of a fight, the switch on - a Ritual: no slot, 10 minutes.
  if (RitualOrder(creature, id, order)) return true;
  // Build 4b: Silent Image's Move - a Magic action while concentrating, no slot.
  if (SilentImageMoveOrder(creature, id, order)) return true;
  // Build 4c: an hour's casting (Find Familiar) - not in a fight; out of one, the clock its hour on.
  if (LongCastOrder(creature, id, order)) return true;
  // 0.99.3 (owner, 2026-11-10: "why does heroism need to be cast first before checking if it's a willing target? Is
  // it the rules?"): "a willing creature" - an enemy isn't one, so it's no target (the script's check stays for a
  // cast with no order).
  if (WillingSpell(spell) && target && target != creature && engine::IsEnemy(creature, order.target)) {
    return Refuse(id, SpellNameOf(spell), PlayerText("spells2024.refuse_willing"), order.target);
  }
  if (spell == rules::kMageArmor && target && ChestArmorClassOf(target) > 0) {
    return Refuse(id, SpellNameOf(rules::kMageArmor), PlayerText("spells2024.refuse_armoured"), order.target);
  }
  // Build 2b: Charm Person a Humanoid, Animal Friendship a Beast - another creature is refused, nothing spent.
  if ((spell == rules::kCharmPerson || spell == rules::kAnimalFriendship) && target && target != creature) {
    void* target_stats = StatsOf(target);
    if (target_stats && !rules::ConditionSpellTargetFits(spell, engine::RaceOf(target_stats))) {
      return Refuse(id, SpellNameOf(spell),
                    PlayerText(spell == rules::kCharmPerson ? "spells2024.refuse_humanoid" : "spells2024.refuse_beast"),
                    order.target);
    }
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
      TellPlayer(id, PlayerText("spells2024.you_cast_a_bonus_action"));
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
    refused = PlayerText("spells2024.refuse_slot_this_turn");
  } else if (BonusActionStatus(creature) != 0) {
    refused = PlayerText("spells2024.refuse_bonus_spent");
  } else if (!on) {
    refused = PlayerText("spells2024.refuse_no_creature", {{"feet", feet}});
  } else if (on != creature && engine::EdgeDistance(creature, on) > rules::FeetToMetres(bonus->feet)) {
    refused = PlayerText("spells2024.refuse_out_of_range", {{"feet", feet}});
  } else if (OrderSlotsLeft(creature, order.spell, order.class_slot, order.domain_level, order.metamagic) <= 0) {
    refused = PlayerText("spells2024.refuse_no_slot");
  } else if (!stats) {
    refused = PlayerText("spells2024.refuse_no_caster");
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
    TellPlayer(self, PlayerText("spells2024.cast_it_from_your_spells", {{"name", std::string(name)}}));
    info.ok = false;
    return info;
  }
  const BonusCast cast = at->second;
  g_bonus_casts.erase(at);
  t_bonus_spend = true;
  const bool spent = SpendOrderSlot(caster, cast.spell, cast.class_slot, cast.domain_level, cast.metamagic);
  t_bonus_spend = false;
  if (!spent) {
    TellPlayer(self, PlayerText("spells2024.no_spell_slot_of_that", {{"name", std::string(name)}}));
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
  r = ObjectToString(t) + ":" + IntToString(TouchAttackRanged(t)) + ";";
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

// NWN's own orb, when no look plays it: the missile from the caster, or the leap's beam from the creature it hit -
// `after` seconds from now (a look's earlier hit).
void NwnOrbVisual(uint32_t self, const std::string& target, const std::string& from, const OrbLook& orb, float after) {
  std::string code = "object t = StringToObject(\"" + target + "\"); if (!GetIsObjectValid(t)) return; DelayCommand(" +
                     Float(after) + ", ";
  if (from.empty()) {
    code += std::string("ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(") + orb.missile + "), t));";
  } else {
    code += std::string("ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(") + orb.beam + ", StringToObject(\"" +
            from + "\"), BODY_NODE_CHEST), t, 1.0));";
  }
  Run(code, self, "n5_s_chromorb");
}

// #155 (the Spell & Attack Bench): a hit's look - the bench's for this row, when it has one - plays the orb (and each
// leap's) and says when it lands; the damage waits for it. Every roll stays at the cast. No look: NWN's visuals as
// before. 0.99.9 (owner, 2026-11-10: "A", relayed by the animation session - SB-5's "flaming arrow" was a natural
// 1): a miss plays the look too - the orb flies and fizzles at the target, no hit, no leap.
void ChromaticOrb(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const rules::CantripOption* option = rules::ChoiceOptionOf(row);
  const rules::ChoiceCantrip* menu = rules::ChoiceCantripOf(row);
  const uint32_t type =
      option && menu && menu->master == rules::kChromaticOrb ? option->value : rules::kDamageTypeFire;  // no choice: Fire
  const OrbLook orb = OrbLookOf(type);
  Look look;
  look.vis = orb.vis;
  if (type == rules::kDamageTypePoison) look.immune = LookOf(rules::kRayOfSickness).immune;
  std::vector<Entry> hits = OneTarget(self, spell, "TouchAttackRanged(t)", 1, Look());  // the roll; its orb after
  if (hits.empty()) return;
  Entry hit = hits.front();
  std::string from;    // the creature the orb leaps from (none: the cast's)
  float after = 0.0f;  // seconds from now the orb's last hit lands (a look's; 0: at once)
  std::string skip = "|";
  int leaps = rules::ChromaticOrbLeaps(slot);
  std::string log;
  for (;;) {
    skip += hit.target + "|";
    const uint32_t target_id = ParseId(hit.target);
    float lands = 0.0f;
    if (hit.value > 0) {
      lands = from.empty() ? PlaySpellLook(self, target_id, row)
                           : PlaySpellLookLeap(ParseId(from), target_id, row, after);
    } else {  // a miss plays the look too (0.99.9): the orb flies and fizzles
      lands = from.empty() ? PlaySpellLookMiss(self, target_id, row)
                           : PlaySpellLookLeapMiss(ParseId(from), target_id, row, after);
    }
    if (lands <= 0.0f) NwnOrbVisual(self, hit.target, from, orb, after);
    const float when = lands > 0.0f ? lands : after;
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
    const bool landed = Damage(self, spell, hit.target, amount, type, lands > 0.0f ? "" : orb.vis, look, when);
    if (landed) HuntersMarkHit(self, target_id, critical, when);  // build 2a
    log += " 0x" + hit.target + (critical ? " a Critical Hit " : " hit ") + Dice(dice, spell.die) + " (" + faces + ") = " +
           std::to_string(amount) + " " + TypeName(type) + (landed ? "" : " - immune") +
           (lands > 0.0f ? " (its look: lands in " + Float(lands) + " s)" : "") + ";";
    after = when;
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
    Run(attack, self, spell.script);
    const std::vector<Entry> again = Entries(caster);
    if (again.empty()) break;
    log += " leaps ->";
    from = hit.target;
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
                    28800.0);  // 8 hours, real time
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
      TellPlayer(self, PlayerText("spells2024.heroism_a_willing_creature_not"));
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

// The creator of a creature's first effect of `spell` (0xFFFFFFFF: none) - for the log (0.99.36, L3-5).
uint32_t SpellEffectCreator(const void* creature, int spell) {
  if (!creature) return 0xFFFFFFFFu;
  void* const* list = Field<void* const*>(creature, ee::kObjectEffects);
  const int count = Field<int32_t>(creature, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return 0xFFFFFFFFu;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (effect && Field<int32_t>(effect, ee::kEffectSpellId) == spell) return Field<uint32_t>(effect, ee::kEffectCreator);
  }
  return 0xFFFFFFFFu;
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
  bool told = false;  // 0.99.20: the player told its quarry dropped (Hunter's Mark: Move)
  uint32_t armed = 0;  // 0.99.22: an early Move's creature (the Ranger's option), 0: none
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
  TellPlayer(id, PlayerText("spells2024.sanctuary_ends_you", {{"why", std::string(why)}}));
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
    // 0.99.15 (owner, 2026-11-11: "go with A for sanctuary"): a player's Sanctuary on itself stops its attacks.
    void* stats = caster ? StatsOf(caster) : nullptr;
    if (rules::SanctuaryStopsOwnAttacks(id == self, stats && ReadCreatureFacts(stats).is_player)) {
      static constexpr AttackStop kSanctuaryStop{"Sanctuary", "Sanctuary on yourself - it stops your attacks",
                                                 " - warded until it attacks",
                                                 "Sanctuary - you stop attacking (an attack of yours ends it)."};
      StopOwnAttacks(caster, kSanctuaryStop);
    }
  }
}

void HuntersMark(uint32_t self, void* caster, const rules::Spell2024& spell, int row, int slot) {
  const int hours = rules::HuntersMarkHours(slot);
  const std::string seconds = SecondsScript(hours * 60);
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
    TellPlayer(self, PlayerText("spells2024.hunter_s_mark_move_no"));
    return;
  }
  if (!QuarryDown(at->second)) {
    TellPlayer(self, PlayerText("spells2024.hunter_s_mark_move_your"));
    return;
  }
  const std::string seconds = SecondsScript(at->second.hours * 60);
  for (const Entry& one : OneTarget(self, spell, "0", -1, Look())) {
    Mark(self, spell, one.target, at->second.row,
         MarkLook{"n5s_quarry", "EffectIcon(EFFECT_ICON_ENEMY_ATTACK_BONUS)", seconds, "VFX_IMP_HEAD_ODD", "", true});
    at->second.quarry = ParseId(one.target);
    at->second.told = false;
    TellPlayer(self, PlayerText("spells2024.hunter_s_mark_moves_to"));
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
  TellPlayer(self, PlayerText("spells2024.expeditious_retreat_you_dash_and"));
  if (LogSlot()) Log("spells2024: 0x%08X Expeditious Retreat - Dash now, a Bonus Action's Dash while it lasts", self);
}

void ExpeditiousDash(uint32_t self, void* caster) {
  if (!HasSpellEffect(caster, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, self)) {
    TellPlayer(self, PlayerText("spells2024.expeditious_retreat_dash_no_expeditious"));
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
          TellPlayer(self, PlayerText("spells2024.mage_armor_a_creature_who"));
          continue;
        }
        Mark(self, spell, one.target, row,
             MarkLook{"n5s_magearmor", "EffectIcon(EFFECT_ICON_AC_INCREASE)", SecondsScript(480), "VFX_IMP_AC_BONUS"});
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
          TellPlayer(self, PlayerText("spells2024.protection_from_evil_and_good"));
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
             MarkLook{"n5s_strider", "EffectIcon(EFFECT_ICON_MOVEMENT_SPEED_INCREASE)", SecondsScript(60),
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
// 0.99.22 (owner, 2026-11-12: "give rangers an option to keep it armed"): the Ranger's option - a record, off by default.
constexpr const char* kVarHuntersMarkArm = "nwn5e_hm_arm";

bool KeepMoveArmed(void* creature) {
  if (!creature) return false;
  EnsureRecords(creature);
  return engine::GetLocalInt(creature, kVarHuntersMarkArm) == 1;
}

// A creature other than the caster and its quarry, standing: an early Move's pick.
bool OtherCreature(uint32_t target, uint32_t self, uint32_t quarry) {
  if (target == 0 || target == ee::kObjectInvalid || target == self || target == quarry) return false;
  void* creature = CreatureById(target);
  return creature && !engine::IsDeadOrDying(creature);
}

bool BuffFeatGuard(void* creature, int feat) {
  if (!Faithful() || !creature || t_bonus_cast) return true;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  for (const rules::BonusActionSpell& spell : rules::BonusActionSpells()) {
    if (feat != spell.feat) continue;
    TellPlayer(id, PlayerText("spells2024.cast_it_from_your_spells", {{"name", std::string(spell.name)}}));
    return false;
  }
  if (feat == rules::kFeatExpeditiousDash &&
      !HasSpellEffect(creature, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, id)) {
    TellPlayer(id, PlayerText("spells2024.expeditious_retreat_dash_no_expeditious"));
    return false;
  }
  // Build 4b (0.99.47, L4-27): Silent Image's Move - only while its image lasts.
  if (feat == rules::kFeatSilentImageMove && !SilentImageLasts(id)) {
    TellPlayer(id, PlayerText("spells2024.silent_image_move_no_image"));
    if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatSilentImageMove);
    return false;
  }
  if (feat == rules::kFeatHuntersMarkMove) {
    const auto at = g_quarries.find(id);
    if (at == g_quarries.end() || !HasSpellEffect(creature, rules::kHuntersMark, rules::kHuntersMarkBonus, id)) {
      TellPlayer(id, PlayerText("spells2024.hunter_s_mark_move_no"));
      if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: Move refused - no Hunter's Mark lasts", id);  // 0.99.21
      return false;
    }
    const uint32_t picked = CurrentFeatUseTarget();
    const rules::MarkMove now = rules::HuntersMarkMoveNow(QuarryDown(at->second), KeepMoveArmed(creature),
                                                         OtherCreature(picked, id, at->second.quarry));
    if (now == rules::MarkMove::kArm) {  // 0.99.22: nothing spent now - it goes when the quarry drops
      at->second.armed = picked;
      TellPlayer(id, PlayerText("spells2024.hunter_s_mark_move_armed"));
      if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: Move armed on 0x%08X (its quarry 0x%08X stands)", id, picked,
                         at->second.quarry);
      return false;
    }
    if (now == rules::MarkMove::kRefuse) {
      TellPlayer(id, PlayerText("spells2024.hunter_s_mark_move_your_2"));
      if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: Move refused - its quarry 0x%08X still stands", id,
                         at->second.quarry);  // 0.99.21
      return false;
    }
    at->second.armed = 0;  // moved by hand: an armed one is done with
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
  TellPlayer(id, PlayerText("spells2024.sanctuary_you_can_t_bring", {{"what", std::string(what)}}));
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
  ForceAttackMiss(roll.data);  // 0.99.7: its missed-by set too (a ranged miss divides by it)
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
    } else if (at != g_quarries.end() && !at->second.told && QuarryDown(at->second)) {
      // 0.99.20 (owner, 2026-11-12: "I'm not a fan of using bonus actions automatically. I favour A. If there's an empty slot available put it there, but also have it available in the radial menu next to the hunter's mark"):
      // the mark doesn't move by itself - the player is told once, the Move on the quickbar and in the radial.
      at->second.told = true;
      const uint32_t armed = at->second.armed;  // 0.99.22: an early Move, kept armed (the Ranger's option)
      at->second.armed = 0;
      void* next = armed ? CreatureById(armed) : nullptr;
      if (next && !engine::IsDeadOrDying(next) && g_use_feat) {
        TellPlayer(id, PlayerText("spells2024.your_quarry_has_dropped_your"));
        if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: its quarry 0x%08X dropped - the armed Move at 0x%08X", id,
                           at->second.quarry, armed);
        float nowhere[3] = {0.0f, 0.0f, 0.0f};
        g_use_feat(creature, static_cast<uint16_t>(rules::kFeatHuntersMarkMove), 0, armed, ee::kObjectInvalid,
                   nowhere);  // through the hook: the Bonus Action spent, or waiting for the next round
      } else {
        TellPlayer(id, PlayerText("spells2024.your_quarry_has_dropped_hunter"));
        if (LogSlot()) Log("spells2024: 0x%08X Hunter's Mark: its quarry 0x%08X dropped - told (Move)", id,
                           at->second.quarry);
      }
    }
  }
  if (!g_dashers.empty() && g_dashers.count(id) &&
      !HasSpellEffect(creature, rules::kExpeditiousRetreat, rules::kExpeditiousRetreatBonus, id)) {
    g_dashers.erase(id);
    if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatExpeditiousDash);
    if (LogSlot()) Log("spells2024: 0x%08X Expeditious Retreat ended", id);
  }
}

// --- Build 2b: conditions on creatures ---------------------------------------------------------------------------------

constexpr const char* kTagCommand = "n5s_command";
constexpr const char* kTagCharm = "n5s_charm";
constexpr const char* kTagLaugh = "n5s_laugh";
constexpr const char* kTagSleep = "n5s_sleep";
constexpr const char* kTagBlind = "n5s_blind";
constexpr int kCandidates = 12;                         // the enemies looked at for an upcast's extra creatures
constexpr uint64_t kSpellMinuteMs = 10 * kRoundMs;
constexpr uint64_t kWhileItsEffectLasts = ~0ULL >> 2;   // a charm: its effect's own time (NWN's game hours)

// A condition that lasts and changes, by creature and spell - Hideous Laughter's saves, Sleep's second save and its
// waking, a charm ended by its caster's side's damage - read at the creature's update (never inside the engine's
// damage handling, as Knock Out's).
struct Held {
  uint32_t caster = 0;
  int row = 0;             // the row cast (its effects carry it, made by the caster: Concentration finds them)
  int dc = 10;
  uint64_t until = 0;      // game ms
  uint64_t next_save = 0;  // Hideous Laughter: its turn's end; Sleep: its second save (0: none due)
  bool asleep = false;     // Sleep: Unconscious - its second save failed
  int damage_saves = 0;    // Hideous Laughter: the saves its damage brings (with Advantage)
  bool woken = false;      // Sleep: damaged
  bool side_damage = false;  // a charm: damaged by its caster or the caster's allies
};
std::unordered_map<uint64_t, Held> g_held;

uint64_t HeldKey(uint32_t creature, int spell) {
  return (static_cast<uint64_t>(creature) << 32) | static_cast<uint32_t>(spell);
}

void Hold(uint32_t target, int spell, const Held& held) {
  if (g_held.size() > 512) g_held.clear();
  g_held[HeldKey(target, spell)] = held;
}

// 0.99.24: a creature `caster` has charmed (Charm Person, Animal Friendship) - Friendly to it.
bool CharmedBy(const std::string& target, uint32_t caster) {
  void* creature = CreatureById(ParseId(target));
  return creature && HasSpellEffect(creature, rules::kCharmPerson, rules::kAnimalFriendship, caster);
}

const char* HeldName(const Held& held) {
  const rules::Spell2024* spell = rules::Spell2024Of(held.row);
  return spell ? spell->name : "a spell";
}

// A condition's effect on one creature: tagged, with the row cast, made by the caster; an earlier one of the same tag
// on it ends. $AFTER: the word's orders (Command), a laugh.
const char* const kCondition = R"nss(
object t = StringToObject("$TARGET");
if (!GetIsObjectValid(t) || GetIsDead(t)) return;
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "$TAG") RemoveEffect(t, e); e = GetNextEffect(t); }
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId($EFFECT, $ROW), "$TAG"), t, $SECONDS);
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect($VIS), t);
$AFTER
)nss";

void ApplyCondition(uint32_t self, const char* what, const std::string& target, int row, const char* tag,
                    const std::string& effect, const std::string& seconds, const char* vis,
                    const std::string& after = std::string()) {
  std::string code = kCondition;
  Replace(&code, "$AFTER", after);
  Replace(&code, "$TARGET", target);
  Replace(&code, "$TAG", tag);
  Replace(&code, "$EFFECT", effect);
  Replace(&code, "$ROW", std::to_string(row));
  Replace(&code, "$SECONDS", seconds);
  Replace(&code, "$VIS", vis);
  Run(code, self, what);
}

std::string Rounds(int rounds) { return "RoundsToSeconds(" + std::to_string(rounds) + ")"; }

// Command's word on its next turn (nwn5e: the next round): its effect and its orders - its actions locked for the
// round (Dissonant Whispers' way) after the word's own: Drop's hands emptied into its inventory (the owner's Q1 B),
// Flee's run away from you (as far as a Dash takes it, 60 feet), Halt's wait in place. Grovel: a round's knockdown
// (Prone, as Topple's).
// 0.99.25 (owner, 2026-11-12, L2b-1: "the enemy not re-arming after Command: Drop. they just stand there and combat
// ends"): ClearAllActions(TRUE) ends its fight, and once the round's lock was over nothing woke its AI (no hit, no new
// sight of you; NWN's generic AI doesn't re-arm at once either). Now an NPC, its turn over, takes back what it
// dropped and is woken as a hostile spell from you would wake it - its own AI fights on.
struct CommandOrders {
  std::string effect;
  std::string after;
};

CommandOrders CommandOrdersOf(rules::CommandWord word) {
  const auto lock = [](const std::string& rearm) {
    return "object c = OBJECT_SELF;\n"
           "AssignCommand(t, ActionDoCommand(SetCommandable(TRUE, t)));\n"
           "if (!GetIsPC(t)) {\n" +
           rearm + "  AssignCommand(t, ActionDoCommand(SignalEvent(t, EventSpellCastAt(c, " +
           std::to_string(rules::kCommand) +
           ", TRUE))));\n"
           "}\n"
           "AssignCommand(t, DelayCommand(6.5, SetCommandable(TRUE, t)));\n"
           "AssignCommand(t, SetCommandable(FALSE, t));\n";
  };
  CommandOrders orders;
  switch (word) {
    case rules::CommandWord::kDrop:
      orders.effect = "EffectVisualEffect(VFX_DUR_MIND_AFFECTING_DISABLED)";
      orders.after = "AssignCommand(t, ClearAllActions(TRUE));\n"
                     "object w = GetItemInSlot(INVENTORY_SLOT_RIGHTHAND, t);\n"
                     "if (GetIsObjectValid(w)) AssignCommand(t, ActionUnequipItem(w));\n"
                     "object h = GetItemInSlot(INVENTORY_SLOT_LEFTHAND, t);\n"
                     "if (GetIsObjectValid(h)) AssignCommand(t, ActionUnequipItem(h));\n"
                     "AssignCommand(t, ActionWait(6.0));\n" +
                     lock("  if (GetIsObjectValid(w)) AssignCommand(t, ActionEquipItem(w, INVENTORY_SLOT_RIGHTHAND));\n"
                          "  if (GetIsObjectValid(h)) AssignCommand(t, ActionEquipItem(h, INVENTORY_SLOT_LEFTHAND));\n");
      break;
    case rules::CommandWord::kFlee:
      orders.effect = "EffectVisualEffect(VFX_DUR_MIND_AFFECTING_FEAR)";
      orders.after = "AssignCommand(t, ClearAllActions(TRUE));\n"
                     "AssignCommand(t, ActionMoveAwayFromObject(OBJECT_SELF, TRUE, GetDistanceBetween(t, OBJECT_SELF) + " +
                     Float(rules::FeetToMetres(60)) + "));\n" + lock("");
      break;
    case rules::CommandWord::kGrovel:
      orders.effect = "EffectKnockdown()";
      break;
    case rules::CommandWord::kHalt:
      orders.effect = "EffectLinkEffects(EffectCutsceneImmobilize(), EffectVisualEffect(VFX_DUR_MIND_AFFECTING_DISABLED))";
      orders.after = "AssignCommand(t, ClearAllActions(TRUE));\nAssignCommand(t, ActionWait(6.0));\n" + lock("");
      break;
  }
  return orders;
}

// Command, Charm Person, Animal Friendship, Hideous Laughter: the creature aimed at ("id:racial type;" first - n5s_save
// 1 when there is one), then up to $MAX enemies nearest it within range - an upcast's extra creatures among them.
const char* const kConditionTargets = R"nss(
object c = GetSpellTargetObject(); string r = ""; int aimed = 0;
location l = GetSpellTargetLocation();
if (GetIsObjectValid(c) && GetObjectType(c) == OBJECT_TYPE_CREATURE) {
  l = GetLocation(c);
  if (c != OBJECT_SELF && !GetIsDead(c)) { r = ObjectToString(c) + ":" + IntToString(GetRacialType(c)) + ";"; aimed = 1; }
}
int i = 1; int seen = 0;
object n = GetNearestCreatureToLocation(CREATURE_TYPE_IS_ALIVE, TRUE, l, i);
while (seen < $MAX && GetIsObjectValid(n) && i < 40) {
  if (n != c && n != OBJECT_SELF && GetDistanceBetween(OBJECT_SELF, n) <= $RANGE && GetIsEnemy(n, OBJECT_SELF)) {
    r += ObjectToString(n) + ":" + IntToString(GetRacialType(n)) + ";"; seen++;
  }
  i++;
  n = GetNearestCreatureToLocation(CREATURE_TYPE_IS_ALIVE, TRUE, l, i);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
SetLocalInt(OBJECT_SELF, "n5s_save", aimed);
)nss";

void Targeted(uint32_t self, void* caster, const rules::Spell2024& spell, int row, int slot) {
  const int count = rules::ConditionSpellTargets(slot);
  std::string code = kConditionTargets;
  Replace(&code, "$MAX", std::to_string(count > 1 ? kCandidates : 0));
  Replace(&code, "$RANGE", Float(rules::FeetToMetres(rules::ConditionSpellRangeFeet(spell.row)) + 2.0f * kSlack));
  Run(code, self, spell.script);
  const bool aimed = engine::GetLocalInt(caster, kVarSave) == 1;
  const bool charm = spell.row == rules::kCharmPerson || spell.row == rules::kAnimalFriendship;
  const std::vector<Entry> entries = Entries(caster);
  std::vector<Entry> picked;
  for (size_t i = 0; i < entries.size() && static_cast<int>(picked.size()) < count; ++i) {
    const bool chosen = i == 0 && aimed;  // the creature aimed at: yours to pick, charmed or not
    if (!chosen && !rules::PickedAgainst(true, CharmedBy(entries[i].target, self))) continue;  // the script's: enemies
    if (rules::ConditionSpellTargetFits(spell.row, entries[i].value)) {
      picked.push_back(entries[i]);
    } else if (i == 0 && aimed) {
      const char* key = spell.row == rules::kCharmPerson ? "spells2024.only_humanoid" : "spells2024.only_beast";
      TellPlayer(self, PlayerText(key, {{"name", spell.name}}));
    }
  }
  const rules::CommandWord word = rules::CommandWordOf(row);
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  const uint64_t now = GameMs();
  std::string log;
  for (const Entry& one : picked) {
    const uint32_t id = ParseId(one.target);
    void* target = CreatureById(id);
    SaveOptions options;
    if (charm) {
      options.type = "SAVING_THROW_TYPE_MIND_SPELLS";  // NWN's charm immunities answer (#106: they stay NWN's)
      options.harmful = false;                         // NWN's charms signal a friendly cast
    }
    if (spell.row == rules::kCharmPerson && target) {
      const bool fighting =
          Field<int32_t>(caster, ee::kCreatureInCombat) != 0 || Field<int32_t>(target, ee::kCreatureInCombat) != 0;
      options.advantage = rules::CharmSaveAdvantage(engine::IsEnemy(caster, id), fighting);
    }
    const int save = SaveOf(self, spell, rules::Ability::kWisdom, one.target, options);
    if (save < 0) continue;
    log += "; 0x" + one.target + " " + SaveWord(save) + (options.advantage ? " (with Advantage)" : "");
    if (save != 0) continue;
    switch (spell.row) {
      case rules::kCommand: {
        const CommandOrders orders = CommandOrdersOf(word);
        ApplyCondition(self, spell.script, one.target, row, kTagCommand, orders.effect,
                       Rounds(rules::ConditionSpellRounds(spell.row)), "VFX_IMP_CONFUSION_S",
                       orders.after + "SendMessageToPC(OBJECT_SELF, \"nwn5e: \" + GetName(t) + \" obeys - " +
                           rules::CommandWordName(word) + ".\");");
        break;
      }
      case rules::kCharmPerson:
      case rules::kAnimalFriendship:
        ApplyCondition(self, spell.script, one.target, row, kTagCharm,
                       "EffectLinkEffects(EffectCharmed(), EffectVisualEffect(VFX_DUR_MIND_AFFECTING_DISABLED))",
                       SecondsScript(rules::ConditionSpellHours(spell.row) * 60), "VFX_IMP_CHARM");
        Hold(id, spell.row, Held{self, row, dc, kWhileItsEffectLasts});
        break;
      case rules::kHideousLaughter:
        ApplyCondition(self, spell.script, one.target, row, kTagLaugh,
                       "EffectLinkEffects(EffectKnockdown(), EffectVisualEffect(VFX_DUR_MIND_AFFECTING_NEGATIVE))",
                       Rounds(rules::ConditionSpellRounds(spell.row)), "VFX_IMP_WILL_SAVING_THROW_USE",
                       "PlayVoiceChat(VOICE_CHAT_LAUGH, t);");
        Hold(id, spell.row, Held{self, row, dc, now + kSpellMinuteMs, now + kRoundMs});
        break;
    }
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s%s%s (a level %d slot, %d creature%s at most; Wisdom save, DC %d)%s", self, spell.name,
        spell.row == rules::kCommand ? ": " : "", spell.row == rules::kCommand ? rules::CommandWordName(word) : "",
        slot, count, count == 1 ? "" : "s", dc, log.empty() ? " - no one" : log.c_str());
  }
}

// Color Spray: every creature in the Cone from you (friend or foe, 2024), a Constitution save or Blinded to the end of
// your next turn (two rounds).
void ColorSpray(uint32_t self, void* caster, const rules::Spell2024& spell, int row) {
  const float edge = rules::FeetToMetres(spell.feet);
  std::string code = kGather;
  Replace(&code, "$REACH", Float(edge * 1.2f + kSlack));
  Replace(&code, "$LOOK", "");
  Run(code, self, spell.script);
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  std::string log;
  int inside = 0;
  for (const Entry& entry : Entries(caster)) {
    if (!rules::InCone(entry.along, entry.side, edge, kSlack)) continue;
    ++inside;
    const int save = SaveOf(self, spell, rules::Ability::kConstitution, entry.target);
    if (save < 0) continue;
    log += "; 0x" + entry.target + " " + SaveWord(save);
    if (save != 0) continue;
    ApplyCondition(self, spell.script, entry.target, row, kTagBlind, "EffectBlindness()",
                   Rounds(rules::ConditionSpellRounds(spell.row)), "VFX_IMP_BLIND_DEAF_M");
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Color Spray (Constitution save, DC %d) - %d in the 15-foot Cone%s", self, dc, inside,
        log.c_str());
  }
}

// Sleep: the enemies (the creatures of your choice) in a 5-foot-radius Sphere around the point aimed at
// ("id:racial type;").
const char* const kSphere = R"nss(
object c = GetSpellTargetObject();
location l = GetSpellTargetLocation();
if (GetIsObjectValid(c)) l = GetLocation(c);
string r = "";
object t = GetFirstObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (t != OBJECT_SELF && !GetIsDead(t) && GetIsEnemy(t, OBJECT_SELF)) {
    r += ObjectToString(t) + ":" + IntToString(GetRacialType(t)) + ";";
  }
  t = GetNextObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

void SleepSpell(uint32_t self, void* caster, const rules::Spell2024& spell, int row) {
  std::string code = kSphere;
  Replace(&code, "$RADIUS", Float(rules::FeetToMetres(spell.feet) + kSlack));
  Run(code, self, spell.script);
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  const uint64_t now = GameMs();
  std::string log;
  for (const Entry& one : Entries(caster)) {
    if (!rules::PickedAgainst(true, CharmedBy(one.target, self))) {  // the script's: enemies (0.99.24)
      log += "; 0x" + one.target + " charmed by you - not chosen";
      continue;
    }
    if (rules::SleepUnaffectedRacialType(one.value)) {  // an elf, a Construct, an Elemental, an Ooze, an Undead
      log += "; 0x" + one.target + " doesn't sleep";
      TellPlayer(self, PlayerText("spells2024.sleep_a_creature_that_doesn"));
      continue;
    }
    SaveOptions options;
    options.type = "SAVING_THROW_TYPE_MIND_SPELLS";
    options.immune = "GetIsImmune(t, IMMUNITY_TYPE_SLEEP, OBJECT_SELF)";  // NWN's sleep immunity
    const int save = SaveOf(self, spell, rules::Ability::kWisdom, one.target, options);
    if (save < 0) continue;
    log += "; 0x" + one.target + " " + SaveWord(save);
    if (save != 0) continue;
    ApplyCondition(self, spell.script, one.target, row, kTagSleep,
                   "EffectLinkEffects(EffectDazed(), EffectVisualEffect(VFX_DUR_MIND_AFFECTING_DISABLED))",
                   Rounds(rules::ConditionSpellRounds(spell.row)), "VFX_IMP_SLEEP");
    Hold(ParseId(one.target), spell.row, Held{self, row, dc, now + kSpellMinuteMs, now + kRoundMs});
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Sleep (Wisdom save, DC %d) - the enemies in the 5-foot Sphere%s", self, dc,
        log.empty() ? ": none" : log.c_str());
  }
}

void Condition(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);  // Command's word's row
  if (spell.row == rules::kColorSpray) {
    ColorSpray(self, caster, spell, row);
  } else if (spell.row == rules::kSleep) {
    SleepSpell(self, caster, spell, row);
  } else {
    Targeted(self, caster, spell, row, slot);
  }
}

// A held condition ends on its creature: its effects off (a script run as it), it told.
void EndHeld(uint32_t id, const Held& held, const char* tag, const std::string& line, const char* why) {
  std::string code = kUnmark;
  Replace(&code, "$TAG", tag);
  Run(code, id, why);
  TellPlayer(id, line);
  if (LogSlot()) Log("spells2024: 0x%08X %s ends on 0x%08X - %s", held.caster, HeldName(held), id, why);
}

// A repeat save against the DC of the cast (a Wisdom save; `advantage`: Hideous Laughter's on damage).
uint8_t HeldSave(void* creature, const Held& held, bool advantage, uint8_t type) {
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through its hook
  if (!save_roll) return 1;
  RememberSaveDc2024(held.caster, held.dc);
  SetForcedSaveAdvantage(advantage);
  const uint8_t save = save_roll(creature, ee::kSaveWill, static_cast<uint16_t>(held.dc), type, held.caster, 1, 0, 0);
  SetForcedSaveAdvantage(false);
  return save;
}

// Damage: Sleep wakes its creature, Hideous Laughter owes a save with Advantage, a charm ends if its caster's side did
// it - all at the creature's next update.
void ConditionDamageHeard(uint32_t creator, void* target, int damage) {
  if (g_held.empty() || damage <= 0 || !target) return;
  const uint32_t id = Field<uint32_t>(target, ee::kObjectId);
  const auto sleep = g_held.find(HeldKey(id, rules::kSleep));
  if (sleep != g_held.end()) sleep->second.woken = true;
  const auto laugh = g_held.find(HeldKey(id, rules::kHideousLaughter));
  if (laugh != g_held.end()) ++laugh->second.damage_saves;
  for (int spell : {rules::kCharmPerson, rules::kAnimalFriendship}) {
    const auto charm = g_held.find(HeldKey(id, spell));
    if (charm == g_held.end() || creator == 0 || creator == ee::kObjectInvalid) continue;
    void* caster = CreatureById(charm->second.caster);
    if (creator == charm->second.caster || (caster && engine::IsFriend(caster, creator))) charm->second.side_damage = true;
  }
}

// The held conditions at their creature's update: gone (its time, its death, Concentration, a dispel) - forgotten; a
// charm's end on its caster's side's damage; Hideous Laughter's saves; Sleep's second save and its waking.
void ConditionUpdate(void* creature) {
  if (g_held.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  for (int spell : {rules::kSleep, rules::kHideousLaughter, rules::kCharmPerson, rules::kAnimalFriendship}) {
    const auto it = g_held.find(HeldKey(id, spell));
    if (it == g_held.end()) continue;
    Held& held = it->second;
    const uint64_t now = GameMs();
    // 0.99.24 (the owner's L2b run, 2026-11-12): damage wakes a sleeper - told and logged though NWN's own sleep
    // effect came off it first (the engine wakes a sleeping creature it damages).
    if (spell == rules::kSleep && held.woken && !engine::IsDeadOrDying(creature)) {
      EndHeld(id, held, kTagSleep, PlayerText("spells2024.the_blow_wakes_you_sleep"), "it took damage");
      g_held.erase(it);
      continue;
    }
    if (now >= held.until || engine::IsDeadOrDying(creature) || !HasSpellEffect(creature, held.row, -1, held.caster)) {
      g_held.erase(it);
      continue;
    }
    if (spell == rules::kCharmPerson || spell == rules::kAnimalFriendship) {
      if (!held.side_damage) continue;
      EndHeld(id, held, kTagCharm, PlayerText("spells2024.the_charm_on_you_breaks"),
              "damaged by its caster's side");
      TellPlayer(held.caster, PlayerText("spells2024.ends_you_or_your_allies", {{"held_name", std::string(HeldName(held))}}));
      g_held.erase(it);
      continue;
    }
    if (spell == rules::kHideousLaughter) {
      const bool after_damage = held.damage_saves > 0;
      if (after_damage) {
        --held.damage_saves;
      } else if (now >= held.next_save) {
        held.next_save = now + kRoundMs;
      } else {
        continue;
      }
      const uint8_t save = HeldSave(creature, held, rules::LaughterSaveAdvantage(after_damage), 0);
      if (LogSlot()) {
        Log("spells2024: 0x%08X Hideous Laughter - its save %s (DC %d%s): %s", id,
            after_damage ? "on damage" : "at its turn's end", held.dc, after_damage ? ", with Advantage" : "",
            save == 0 ? "failed, still laughing" : "succeeded");
      }
      if (save == 0) continue;
      EndHeld(id, held, kTagLaugh, PlayerText("spells2024.you_stop_laughing_hideous_laughter"), "it saved");
      g_held.erase(it);
      continue;
    }
    // Sleep (woken: above).
    if (held.asleep || held.next_save == 0 || now < held.next_save) continue;
    held.next_save = 0;
    const uint8_t save = HeldSave(creature, held, false, static_cast<uint8_t>(ee::kSaveTypeMindSpells));
    if (rules::SleepAfterSecondSave(save != 0) == rules::SleepStage::kAwake) {
      EndHeld(id, held, kTagSleep, PlayerText("spells2024.you_shake_off_the_sleep"), "its second save succeeded");
      g_held.erase(it);
      continue;
    }
    if (!CreatureById(held.caster)) {
      EndHeld(id, held, kTagSleep, PlayerText("spells2024.the_sleep_spell_fades"), "its caster is gone");
      g_held.erase(it);
      continue;
    }
    held.asleep = true;
    const double left = static_cast<double>(held.until > now ? held.until - now : kRoundMs) / 1000.0;
    char code[720];
    sprintf_s(code,
              "object t = StringToObject(\"%x\"); if (!GetIsObjectValid(t)) return; effect e = GetFirstEffect(t); "
              "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") RemoveEffect(t, e); e = GetNextEffect(t); } "
              "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectLinkEffects(EffectSleep(), "
              "EffectVisualEffect(VFX_DUR_MIND_AFFECTING_NEGATIVE)), %d), \"%s\"), t, %.1f); "
              "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_SLEEP), t);",
              id, kTagSleep, held.row, kTagSleep, left);
    Run(code, held.caster, "Sleep's second save");  // made by the caster: Concentration still finds it
    TellPlayer(id, PlayerText("spells2024.you_fall_asleep_sleep_unconscious"));
    if (LogSlot()) Log("spells2024: 0x%08X Sleep - its second save failed (DC %d): Unconscious for %.0f s", id, held.dc, left);
  }
}

// --- Build 2c: the areas ------------------------------------------------------------------------------------------------

constexpr int kAreaEntangle = 105;      // vfx_persistent.if_spells2024.txt
constexpr int kAreaGrease = 106;
constexpr int kAreaFogCloudBase = 106;  // + the slot level: 107-115
constexpr const char* kTagEntangled = "n5s_entangled";
constexpr const char* kTagFaerie = "n5s_faerie";
constexpr const char* kTagFog = "n5s_fog";
constexpr const char* kTagHidden = "n5s_hidden";
constexpr int kStruggleMs = 3000;  // an NPC's Escape try, in place of its round's attacks (the Grapple's)
constexpr int kEffectTypeImprovedInvisibility = 57;  // NWScript's EFFECT_TYPE_IMPROVEDINVISIBILITY

using SkillRankFn = int8_t (*)(void* stats, uint8_t skill, void* vs, int base_only);

struct Entangled {
  uint32_t caster = 0;
  int dc = 10;
  uint64_t next_try = 0;  // an NPC's next Escape try (game ms)
  bool feat = false;      // the Escape feat granted (a player's character)
  int row = rules::kEntangle;           // the spell holding it (build 3: Ensnaring Strike's too)
  const char* tag = kTagEntangled;      // its effect's tag
};
std::unordered_map<uint32_t, Entangled> g_entangled;                    // Entangle's Restrained creatures
constexpr const char* kVarVinesImmune = "n5_vines_immune";            // set by Ensnaring Strike's hold (0.99.30)
// The spell holding a creature, for the log (Entangle's or Ensnaring Strike's).
const char* HoldName(int row) {
  const rules::Spell2024* spell = rules::Spell2024Of(row);
  return spell ? spell->name : "Entangle";
}
std::unordered_map<uint32_t, std::unordered_set<uint32_t>> g_terrain;  // creature -> the Difficult Terrain areas it's in
std::unordered_map<uint32_t, uint32_t> g_fogged;                      // creature -> the Fog Cloud it's in
std::unordered_map<uint64_t, int> g_area_dcs;                          // (caster, row) -> its cast's DC (Grease's saves)
struct Hidden {  // an invisibility Faerie Fire suppressed (Q3 B)
  int type = 1;          // INVISIBILITY_TYPE_*
  int spell = -1;
  uint32_t creator = 0;
  int left = -1;         // seconds left when suppressed (-1: permanent)
  uint64_t at = 0;       // game ms
};
struct Outline {
  uint32_t caster = 0;
  int row = 0;             // the row cast (its colour)
  uint64_t next_look = 0;  // a new invisibility looked for
  std::vector<Hidden> hidden;
};
std::unordered_map<uint32_t, Outline> g_outlined;  // Faerie Fire's outlined creatures

uint64_t CastKey(uint32_t caster, int row) {
  return (static_cast<uint64_t>(caster) << 32) | static_cast<uint32_t>(row);
}

std::string Hex(uint32_t id) {
  char text[16];
  sprintf_s(text, "%x", id);
  return text;
}

bool IsPlayerCreature(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

// An area at the point aimed at (a creature aimed at: where it stands), its scripts nwn5e's (the vfx_persistent rows
// name none), made in the spell's context: it carries the spell's row and its caster - Concentration finds it.
void MakeArea(uint32_t self, const rules::Spell2024& spell, int area, const char* enter, const char* round,
              const char* exit, const std::string& seconds) {
  char code[640];
  sprintf_s(code,
            "object c = GetSpellTargetObject(); location l = GetSpellTargetLocation(); if (GetIsObjectValid(c)) l = "
            "GetLocation(c); ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, EffectAreaOfEffect(%d, \"%s\", \"%s\", "
            "\"%s\"), l, %s);",
            area, enter, round, exit, seconds.c_str());
  Run(code, self, spell.script);
}

// The creatures in a square on the ground around the point aimed at, part of each in it ($SELF: the caster too).
const char* const kSquare = R"nss(
object c = GetSpellTargetObject();
location l = GetSpellTargetLocation();
if (GetIsObjectValid(c)) l = GetLocation(c);
string r = "";
object t = GetFirstObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (($SELF || t != OBJECT_SELF) && !GetIsDead(t)) r += ObjectToString(t) + ":0;";
  t = GetNextObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

std::vector<Entry> InSquare(uint32_t self, void* caster, const rules::Spell2024& spell, bool with_caster) {
  std::string code = kSquare;
  Replace(&code, "$HALF", Float(rules::FeetToMetres(spell.feet) / 2.0f + kSlack));
  Replace(&code, "$SELF", with_caster ? "TRUE" : "FALSE");
  Run(code, self, spell.script);
  return Entries(caster);
}

// An area's creatures: the one entering, the one leaving, or all those in it (its scripts run as the area).
const char* const kEntering = "SetLocalString(OBJECT_SELF, \"n5s_hits\", ObjectToString(GetEnteringObject()) + \":0;\");";
const char* const kExiting = "SetLocalString(OBJECT_SELF, \"n5s_hits\", ObjectToString(GetExitingObject()) + \":0;\");";
const char* const kInside = R"nss(
string r = "";
object t = GetFirstInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (!GetIsDead(t)) r += ObjectToString(t) + ":0;";
  t = GetNextInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5s_hits", r);
)nss";

std::vector<uint32_t> AreaCreatures(uint32_t area, const char* which) {
  Run(which, area, "an area's creatures");
  std::vector<uint32_t> ids;
  void* object = GameObjectById(area);
  if (!object) return ids;
  for (const Entry& one : Entries(object)) {
    const uint32_t id = ParseId(one.target);
    if (CreatureById(id)) ids.push_back(id);
  }
  return ids;
}

uint32_t AreaCreator(uint32_t area) {
  void* object = GameObjectById(area);
  return object ? Field<uint32_t>(object, ee::kAoeCreator) : 0;
}

// --- Entangle: Restrained, and the Escape ---

int AthleticsOf(void* creature) {
  static const auto skill_rank = reinterpret_cast<SkillRankFn>(EngineExport(ee::kGetSkillRank));
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && skill_rank ? skill_rank(stats, static_cast<uint8_t>(rules::kNwnAthletics), nullptr, 0) : 0;
}

// The Escape check (Q1 A): d20 + Athletics against the caster's DC - the Grapple's rule.
bool EscapeTry(uint32_t id, void* creature, const Entangled& held) {
  const int athletics = AthleticsOf(creature);
  const int d20 = RollEngineDice(1, 20);
  const bool free = rules::EscapeSucceeds(d20, athletics, held.dc);
  TellPlayer(id, PlayerText(free ? "spells2024.escape_free" : "spells2024.escape_held",
                            {{"bonus", (athletics >= 0 ? "+" : "") + std::to_string(athletics)},
                             {"total", std::to_string(d20 + athletics)}, {"dc", std::to_string(held.dc)}}));
  if (LogSlot()) {
    Log("spells2024: 0x%08X Escape from %s: Athletics %+d, %d vs DC %d - %s", id, HoldName(held.row), athletics,
        d20 + athletics, held.dc, free ? "free" : "still held");
  }
  return free;
}

void FreeFromEntangle(uint32_t id, void* creature) {
  const auto it = g_entangled.find(id);
  std::string code = kUnmark;
  Replace(&code, "$TAG", it != g_entangled.end() ? it->second.tag : kTagEntangled);
  Run(code, id, "the vines' hold");
  if (it == g_entangled.end()) return;
  if (it->second.feat) {
    if (void* stats = creature ? StatsOf(creature) : nullptr) engine::RemoveFeat(stats, rules::kFeatEscape);
  }
  g_entangled.erase(it);
}

// The Escape action (feat 1563, its row 1181): a player held by Entangle.
void EscapeScript(uint32_t self) {
  void* creature = CreatureById(self);
  const auto it = g_entangled.find(self);
  if (!creature || it == g_entangled.end() || !HasSpellEffect(creature, it->second.row, -1, it->second.caster)) {
    TellPlayer(self, PlayerText("spells2024.escape_nothing_holds_you_entangle"));
    return;
  }
  if (EscapeTry(self, creature, it->second)) FreeFromEntangle(self, creature);
}

void Entangle(uint32_t self, void* caster, const rules::Spell2024& spell, int row) {
  MakeArea(self, spell, kAreaEntangle, "n5_s_entanglea", "n5_s_entanglec", "n5_s_entangleb", Rounds(10));
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  const uint64_t now = GameMs();
  std::string log;
  for (const Entry& one : InSquare(self, caster, spell, false)) {  // "each creature (other than you)"
    const int save = SaveOf(self, spell, rules::Ability::kStrength, one.target);
    if (save < 0) continue;
    log += "; 0x" + one.target + " " + SaveWord(save);
    if (save != 0) continue;
    ApplyCondition(self, spell.script, one.target, row, kTagEntangled,
                   "EffectLinkEffects(EffectEntangle(), EffectVisualEffect(VFX_DUR_ENTANGLE))", Rounds(10), "VFX_IMP_SLOW");
    const uint32_t id = ParseId(one.target);
    void* target = CreatureById(id);
    Entangled held{self, dc, now + kRoundMs};
    if (target && IsPlayerCreature(target)) {
      if (void* stats = StatsOf(target)) {
        if (!engine::HasFeat(stats, rules::kFeatEscape)) engine::AddFeat(stats, rules::kFeatEscape);
        held.feat = true;
        TellPlayer(id, PlayerText("spells2024.entangle_holds_you_restrained_escape"));
      }
    }
    if (g_entangled.size() > 256) g_entangled.clear();
    g_entangled[id] = held;
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Entangle (Strength save, DC %d) - the 20-foot square, Difficult Terrain for a minute%s",
        self, dc, log.empty() ? "; no one in it" : log.c_str());
  }
}

// --- Grease: Prone on entering, at a turn's end there ---

void GreaseSave(uint32_t area, uint32_t id, const char* when) {
  void* creature = CreatureById(id);
  if (!creature || engine::IsDeadOrDying(creature)) return;
  const uint32_t creator = AreaCreator(area);
  const auto at = g_area_dcs.find(CastKey(creator, rules::kGrease));
  const int dc = at != g_area_dcs.end() ? at->second : 10;
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through its hook
  if (!save_roll) return;
  RememberSaveDc2024(creator, dc);
  const uint8_t save = save_roll(creature, ee::kSaveReflex, static_cast<uint16_t>(dc), 0, creator, 1, 0, 0);
  if (LogSlot()) {
    Log("spells2024: 0x%08X Grease (%s): Dexterity save, DC %d - %s", id, when, dc, save == 0 ? "failed: Prone" : "succeeded");
  }
  if (save != 0) return;
  char code[200];
  sprintf_s(code, "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectKnockdown(), StringToObject(\"%x\"), 6.0);", id);
  RunScriptChunk(code, creator && GameObjectById(creator) ? creator : id);
}

void Grease(uint32_t self, void* caster, const rules::Spell2024& spell) {
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  if (g_area_dcs.size() > 256) g_area_dcs.clear();
  g_area_dcs[CastKey(self, rules::kGrease)] = dc;
  MakeArea(self, spell, kAreaGrease, "n5_s_greasea", "n5_s_greasec", "n5_s_greaseb", Rounds(10));
  if (LogSlot()) Log("spells2024: 0x%08X Grease (Dexterity save, DC %d) - the 10-foot square for a minute", self, dc);
}

// The areas' scripts: Difficult Terrain while inside (Entangle, Grease); Grease's saves.
void TerrainIn(uint32_t area, uint32_t id) {
  if (g_terrain.size() > 512) g_terrain.clear();
  g_terrain[id].insert(area);
}

void EntangleEnter(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kEntering)) TerrainIn(area, id);
}

void EntangleRound(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kInside)) TerrainIn(area, id);
}

void TerrainExit(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kExiting)) {
    const auto it = g_terrain.find(id);
    if (it == g_terrain.end()) continue;
    it->second.erase(area);
    if (it->second.empty()) g_terrain.erase(it);
  }
}

void GreaseEnter(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kEntering)) {
    TerrainIn(area, id);
    GreaseSave(area, id, "in it as it appears, or entering it");
  }
}

void GreaseRound(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kInside)) {
    TerrainIn(area, id);
    GreaseSave(area, id, "the end of its turn there");
  }
}

// --- Fog Cloud: darkness inside (Q2 A) ---

void Unfog(uint32_t id) {
  std::string code = kUnmark;
  Replace(&code, "$TAG", kTagFog);
  Run(code, id, "Fog Cloud's darkness");
}

void FogEnter(uint32_t area) {
  const uint32_t creator = AreaCreator(area);
  for (uint32_t id : AreaCreatures(area, kEntering)) {
    char code[420];
    sprintf_s(code,
              "object t = StringToObject(\"%x\"); effect e = GetFirstEffect(t); while (GetIsEffectValid(e)) { if "
              "(GetEffectTag(e) == \"%s\") RemoveEffect(t, e); e = GetNextEffect(t); } "
              "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SetEffectSpellId(EffectDarkness(), %d), \"%s\"), t);",
              id, kTagFog, rules::kFogCloud, kTagFog);
    RunScriptChunk(code, creator && CreatureById(creator) ? creator : area);  // the caster's: Concentration ends it
    if (g_fogged.size() > 512) g_fogged.clear();
    g_fogged[id] = area;
  }
}

void FogExit(uint32_t area) {
  for (uint32_t id : AreaCreatures(area, kExiting)) {
    const auto it = g_fogged.find(id);
    if (it == g_fogged.end() || it->second != area) continue;
    g_fogged.erase(it);
    Unfog(id);
  }
}

void FogCloud(uint32_t self, const rules::Spell2024& spell, int slot) {
  static const char* const kEnter[] = {"n5_s_fog1a", "n5_s_fog2a", "n5_s_fog3a", "n5_s_fog4a", "n5_s_fog5a",
                                       "n5_s_fog6a", "n5_s_fog7a", "n5_s_fog8a", "n5_s_fog9a"};
  static const char* const kExit[] = {"n5_s_fog1b", "n5_s_fog2b", "n5_s_fog3b", "n5_s_fog4b", "n5_s_fog5b",
                                      "n5_s_fog6b", "n5_s_fog7b", "n5_s_fog8b", "n5_s_fog9b"};
  const int level = std::min(9, std::max(1, slot));
  MakeArea(self, spell, kAreaFogCloudBase + level, kEnter[level - 1], "", kExit[level - 1], SecondsScript(60));
  if (LogSlot()) {
    Log("spells2024: 0x%08X Fog Cloud (a level %d slot): a %d-foot-radius Sphere, darkness inside, up to an hour", self,
        level, rules::FogCloudRadiusFeet(level));
  }
}

// --- Faerie Fire: outlined (Q3 B: its invisibility suppressed while it lasts) ---

struct FaerieLook {
  const char* glow;
  const char* light;
};

FaerieLook FaerieLookOf(rules::FaerieColour colour) {
  switch (colour) {
    case rules::FaerieColour::kGreen: return {"VFX_DUR_GLOW_GREEN", "VFX_DUR_LIGHT_WHITE_5"};  // NWN has no green light
    case rules::FaerieColour::kViolet: return {"VFX_DUR_GLOW_PURPLE", "VFX_DUR_LIGHT_PURPLE_5"};
    case rules::FaerieColour::kBlue: break;
  }
  return {"VFX_DUR_GLOW_BLUE", "VFX_DUR_LIGHT_BLUE_5"};
}

// An outlined creature's invisibility effects taken off ("type spell creator left;"), as the creature.
const char* const kSuppress = R"nss(
string r = "";
effect e = GetFirstEffect(OBJECT_SELF);
while (GetIsEffectValid(e)) {
  int k = GetEffectType(e);
  if (k == EFFECT_TYPE_INVISIBILITY || k == EFFECT_TYPE_IMPROVEDINVISIBILITY) {
    int left = -1;
    if (GetEffectDurationType(e) == DURATION_TYPE_TEMPORARY) left = GetEffectDurationRemaining(e);
    r += IntToString(k) + " " + IntToString(GetEffectSpellId(e)) + " " + ObjectToString(GetEffectCreator(e)) + " " +
         IntToString(left) + ";";
    RemoveEffect(OBJECT_SELF, e);
  }
  e = GetNextEffect(OBJECT_SELF);
}
SetLocalString(OBJECT_SELF, "n5s_hidden", r);
)nss";

// Who runs a restored effect: its creator while it's there (its Concentration and its spell find it), else the creature.
uint32_t MakerOf(uint32_t creator, uint32_t id) { return creator && GameObjectById(creator) ? creator : id; }

void SuppressInvisibility(uint32_t id, Outline& outline, uint64_t now) {
  Run(kSuppress, id, "Faerie Fire's outline");
  void* creature = CreatureById(id);
  const std::string text = creature ? engine::GetLocalString(creature, "n5s_hidden") : std::string();
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find(';', at);
    if (end == std::string::npos) break;
    int type = 0, spell = -1, left = -1;
    char who[16] = {};
    if (sscanf_s(text.substr(at, end - at).c_str(), "%d %d %15s %d", &type, &spell, who,
                 static_cast<unsigned>(sizeof(who)), &left) == 4) {
      Hidden hidden;
      hidden.type = type == kEffectTypeImprovedInvisibility ? 4 : 1;  // INVISIBILITY_TYPE_IMPROVED : _NORMAL
      hidden.spell = spell;
      hidden.creator = ParseId(who);
      hidden.left = left;
      hidden.at = now;
      // A holder in its place - its spell, its creator, its time: Concentration on it holds, and ends it.
      std::string code = "object t = StringToObject(\"" + Hex(id) +
                         "\"); effect v = TagEffect(SetEffectSpellId(EffectVisualEffect(VFX_DUR_CESSATE_NEUTRAL), " +
                         std::to_string(spell) + "), \"" + kTagHidden + "\"); ApplyEffectToObject(" +
                         (left < 0 ? std::string("DURATION_TYPE_PERMANENT, v, t);")
                                   : "DURATION_TYPE_TEMPORARY, v, t, " + std::to_string(left) + ".0);");
      RunScriptChunk(code.c_str(), MakerOf(hidden.creator, id));
      outline.hidden.push_back(hidden);
      TellPlayer(id, PlayerText("spells2024.faerie_fire_outlines_you_your"));
      if (LogSlot()) {
        Log("spells2024: 0x%08X Faerie Fire suppresses its invisibility (spell %d by 0x%08X, %d s left)", id, spell,
            hidden.creator, left);
      }
    }
    at = end + 1;
  }
}

void RestoreInvisibility(uint32_t id, void* creature, const Outline& outline, uint64_t now) {
  for (const Hidden& hidden : outline.hidden) {
    // Its holder still there: its Concentration held and its time hadn't run out.
    const bool held = hidden.spell < 0 || HasSpellEffect(creature, hidden.spell, -1, hidden.creator);
    const int left = rules::InvisibilityLeftSeconds(hidden.left, static_cast<int>((now - hidden.at) / 1000));
    if (!held || left == 0) {
      if (LogSlot()) Log("spells2024: 0x%08X its suppressed invisibility (spell %d) ended meanwhile", id, hidden.spell);
      continue;
    }
    std::string code = "object t = StringToObject(\"" + Hex(id) + "\"); effect v = EffectInvisibility(" +
                       std::to_string(hidden.type) + ");" +
                       (hidden.type == 4 ? " v = EffectLinkEffects(v, EffectConcealment(50));" : "") +
                       (hidden.spell >= 0 ? " v = SetEffectSpellId(v, " + std::to_string(hidden.spell) + ");" : "") +
                       " ApplyEffectToObject(" +
                       (left < 0 ? std::string("DURATION_TYPE_PERMANENT, v, t);")
                                 : "DURATION_TYPE_TEMPORARY, v, t, " + std::to_string(left) + ".0);");
    RunScriptChunk(code.c_str(), MakerOf(hidden.creator, id));
    TellPlayer(id, PlayerText("spells2024.faerie_fire_ends_your_invisibility"));
    if (LogSlot()) Log("spells2024: 0x%08X its invisibility back (spell %d, %d s left)", id, hidden.spell, left);
  }
  std::string code = kUnmark;
  Replace(&code, "$TAG", kTagHidden);
  Run(code, id, "Faerie Fire's holders");
}

void FaerieFire(uint32_t self, void* caster, const rules::Spell2024& spell, int row) {
  const rules::FaerieColour colour = rules::FaerieColourOf(row);
  const FaerieLook look = FaerieLookOf(colour);
  const std::string effect =
      std::string("EffectLinkEffects(EffectVisualEffect(") + look.glow + "), EffectVisualEffect(" + look.light + "))";
  const uint64_t now = GameMs();
  const int dc = SpellSaveDcForClassSlot(caster, -1);
  std::string log;
  for (const Entry& one : InSquare(self, caster, spell, true)) {  // "each creature in the Cube"
    const int save = SaveOf(self, spell, rules::Ability::kDexterity, one.target);
    if (save < 0) continue;
    log += "; 0x" + one.target + " " + SaveWord(save);
    if (save != 0) continue;
    ApplyCondition(self, spell.script, one.target, row, kTagFaerie, effect, Rounds(10), "VFX_IMP_MAGBLUE");
    const uint32_t id = ParseId(one.target);
    if (g_outlined.size() > 256) g_outlined.clear();
    Outline& outline = g_outlined[id];
    outline.caster = self;
    outline.row = row;
    outline.next_look = now + 1000;
    SuppressInvisibility(id, outline, now);
  }
  if (LogSlot()) {
    Log("spells2024: 0x%08X Faerie Fire (%s; Dexterity save, DC %d) - the 20-foot Cube%s", self,
        rules::FaerieColourName(colour), dc, log.empty() ? "; no one in it" : log.c_str());
  }
}

void AreaSpell(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);  // Faerie Fire's colour's row
  switch (spell.row) {
    case rules::kEntangle: Entangle(self, caster, spell, row); return;
    case rules::kGrease: Grease(self, caster, spell); return;
    case rules::kFaerieFire: FaerieFire(self, caster, spell, row); return;
    case rules::kFogCloud: FogCloud(self, spell, slot); return;
  }
}

// #160 (owner, 2026-11-12: "in fog I think enemies would walk around trying to find the player", then "go with A on
// all"), Q2 A: a creature the game moves that loses its enemy in a Fog Cloud walks to where it last saw it, then to a
// spot within 3 m of that each round, three rounds, then gives up. Its last target is kept while it has one.
struct FogSearch {
  uint32_t target = 0;
  int round = 0;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, uint32_t> g_last_target;  // a creature the game moves -> its last attack target
std::unordered_map<uint32_t, FogSearch> g_fog_searches;

bool InControl(void* creature) {
  return !effects::HasState(creature, ee::kStateCharmed) && !effects::HasState(creature, 2) &&  // charmed, confused
         !effects::HasState(creature, 7);                                                         // dominated
}

void FogSearchUpdate(void* creature, uint32_t id, uint64_t now) {
  if (IsPlayerCreature(creature) || engine::IsDeadOrDying(creature)) return;
  const uint32_t target = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
  void* fighting = target != 0 && target != ee::kObjectInvalid ? CreatureById(target) : nullptr;
  auto search = g_fog_searches.find(id);
  if (fighting) {  // it has a target: no search
    if (g_last_target.size() > 1024) g_last_target.clear();
    g_last_target[id] = target;
    if (search != g_fog_searches.end()) {
      if (LogSlot()) Log("spells2024: 0x%08X ends its fog search - it has a target (0x%08X)", id, target);
      g_fog_searches.erase(search);
    }
    return;
  }
  if (search == g_fog_searches.end()) {
    const auto last = g_last_target.find(id);
    if (last == g_last_target.end() || g_fogged.empty()) return;
    void* lost = CreatureById(last->second);
    const bool in_fog = lost && InFogCloud(lost);
    if (!rules::SearchesFog(true, InControl(creature), lost && engine::IsEnemy(creature, last->second), in_fog, false)) {
      if (!in_fog) g_last_target.erase(last);
      return;
    }
    g_fog_searches[id] = FogSearch{last->second, 0, now};
    g_last_target.erase(last);
    search = g_fog_searches.find(id);
  }
  FogSearch& seek = search->second;
  if (now < seek.next) return;
  if (seek.round >= rules::kFogSearchRounds) {
    if (LogSlot()) Log("spells2024: 0x%08X gives up its fog search for 0x%08X", id, seek.target);
    g_fog_searches.erase(search);
    return;
  }
  char code[600];
  if (seek.round == 0) {  // to where it last saw it
    sprintf_s(code,
              "object t = StringToObject(\"%x\"); if (!GetIsObjectValid(t)) return; SetLocalLocation(OBJECT_SELF, "
              "\"n5s_seek\", GetLocation(t)); ClearAllActions(); ActionMoveToLocation(GetLocation(t));",
              seek.target);
  } else {  // a spot within 3 m of it
    sprintf_s(code,
              "location l = GetLocalLocation(OBJECT_SELF, \"n5s_seek\"); vector v = GetPositionFromLocation(l); "
              "float a = IntToFloat(Random(360)); float d = %.2f * IntToFloat(Random(100) + 1) / 100.0; "
              "v.x += d * cos(a); v.y += d * sin(a); ClearAllActions(); "
              "ActionMoveToLocation(Location(GetAreaFromLocation(l), v, 0.0));",
              rules::kFogSearchMetres);
  }
  RunScriptChunk(code, id);
  if (LogSlot()) {
    Log("spells2024: 0x%08X searches the fog for 0x%08X (round %d of %d)", id, seek.target, seek.round + 1,
        rules::kFogSearchRounds);
  }
  ++seek.round;
  seek.next = now + kRoundMs;
}

// Difficult Terrain (Entangle, Grease): half Speed inside - Dash's and Longstrider's too (an adjuster, after them).
float TerrainSpeed(void* creature, float factor) {
  if (g_terrain.empty() || !creature || !Faithful()) return factor;
  const auto it = g_terrain.find(Field<uint32_t>(creature, ee::kObjectId));
  if (it == g_terrain.end()) return factor;
  for (uint32_t area : it->second) {
    if (GameObjectById(area)) return rules::DifficultTerrainFactor(factor, true);
  }
  return factor;
}

// A creature's update: areas gone forgotten (a fog's darkness off); Entangle's hold over, or an NPC's Escape try;
// Faerie Fire's outline over (its invisibility back), or a new invisibility suppressed.
void AreaUpdate(void* creature) {
  if (!creature || !Faithful()) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const uint64_t now = GameMs();
  FogSearchUpdate(creature, id, now);  // #160
  if (g_terrain.empty() && g_fogged.empty() && g_entangled.empty() && g_outlined.empty()) return;
  if (!g_terrain.empty()) {
    const auto it = g_terrain.find(id);
    if (it != g_terrain.end()) {
      for (auto area = it->second.begin(); area != it->second.end();) {
        area = GameObjectById(*area) ? std::next(area) : it->second.erase(area);
      }
      if (it->second.empty()) g_terrain.erase(it);
    }
  }
  if (!g_fogged.empty()) {
    const auto it = g_fogged.find(id);
    if (it != g_fogged.end() && !GameObjectById(it->second)) {
      g_fogged.erase(it);
      Unfog(id);
    }
  }
  if (!g_entangled.empty()) {
    const auto it = g_entangled.find(id);
    if (it != g_entangled.end()) {
      Entangled& held = it->second;
      if (engine::IsDeadOrDying(creature) || !HasSpellEffect(creature, held.row, -1, held.caster)) {
        if (held.feat) {
          if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatEscape);
        }
        const int row = held.row;
        g_entangled.erase(it);
        if (LogSlot()) Log("spells2024: 0x%08X %s no longer holds it", id, HoldName(row));
      } else if (!IsPlayerCreature(creature) && now >= held.next_try) {
        held.next_try = now + kRoundMs;
        const uint32_t caster = held.caster;
        const bool free = EscapeTry(id, creature, held);
        PauseCombatRound(creature, caster, kStruggleMs);  // the try in place of its attacks (the Grapple's)
        if (free) FreeFromEntangle(id, creature);
      }
    }
  }
  if (!g_outlined.empty()) {
    const auto it = g_outlined.find(id);
    if (it != g_outlined.end()) {
      Outline& outline = it->second;
      if (engine::IsDeadOrDying(creature) || !HasSpellEffect(creature, outline.row, -1, outline.caster)) {
        const Outline done = outline;
        g_outlined.erase(it);
        RestoreInvisibility(id, creature, done, now);
      } else if (now >= outline.next_look) {
        outline.next_look = now + 1000;
        SuppressInvisibility(id, outline, now);
      }
    }
  }
}

thread_local bool t_reaction_spend = false;  // build 3: a Reaction's slot - not this turn's one spell with a slot

// --- Build 3: reactions and smites -------------------------------------------------------------------------------------

constexpr const char* kTagShield = "n5s_shield";
constexpr const char* kTagSearing = "n5s_searing";
constexpr const char* kTagEnsnared = "n5s_ensnared";
constexpr int kNwnClassWizard = 10;  // a memorizer: its prepared copies only (no slot of another level)
using IsKnownFn = int (*)(void* stats, uint8_t class_slot, uint32_t spell);
using ArmorClassFn = int16_t (*)(void* stats, void* attacker, int touch);

// --- Shield: a Reaction (Q3 A: an NPC caster's too) ---

// Where a creature's Shield comes from: a class slot's lowest slot that casts it, or its own (innate) uses.
struct ShieldSource {
  int class_slot = -1;
  int domain = 0;  // the slot level (0: the spell's own)
  bool innate = false;
};

bool ShieldSourceOf(void* creature, ShieldSource* out) {
  void* stats = StatsOf(creature);
  if (!stats) return false;
  static const auto known = reinterpret_cast<IsKnownFn>(EngineExport(ee::kIsInKnownSpellList));
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  for (size_t i = 0; i < slots.size() && i < 8; ++i) {
    if (rules::CasterKindOfClass(slots[i].nwn_class_id) == rules::CasterKind::kNone) continue;
    const uint8_t slot = static_cast<uint8_t>(i);
    if (known && !known(stats, slot, static_cast<uint32_t>(rules::kShield))) continue;  // not prepared / known
    if (OrderSlotsLeft(creature, rules::kShield, slot, 0, 0) > 0) {
      out->class_slot = static_cast<int>(i);
      out->domain = 0;
      return true;
    }
    if (slots[i].nwn_class_id == kNwnClassWizard) continue;
    for (int level = 2; level <= 9; ++level) {  // the lowest slot left that casts it
      if (OrderSlotsLeft(creature, rules::kShield, slot, level, 0) > 0) {
        out->class_slot = static_cast<int>(i);
        out->domain = level;
        return true;
      }
    }
  }
  // A creature's own Shield (a special ability, no class slots).
  RunScriptChunk("SetLocalInt(OBJECT_SELF, \"n5s_has\", GetHasSpell(417));", Field<uint32_t>(creature, ee::kObjectId));
  if (engine::GetLocalInt(creature, "n5s_has") > 0) {
    out->innate = true;
    return true;
  }
  return false;
}

// +5 AC until the start of its next turn (SpellArmorClassBonus counts it), no Magic Missile damage (Darts).
void ShieldUp(uint32_t id) {
  char code[640];
  sprintf_s(code,
            "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") "
            "RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); } "
            "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectLinkEffects(EffectIcon("
            "EFFECT_ICON_AC_INCREASE), EffectVisualEffect(VFX_DUR_GLOBE_MINOR)), %d), \"%s\"), OBJECT_SELF, 6.0); "
            "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_AC_BONUS), OBJECT_SELF);",
            kTagShield, rules::kShield, kTagShield);
  RunScriptChunk(code, id);
}

// The Reaction taken: its switch, the Reaction, the slot (not this turn's one spell with a slot), the +5.
bool TakeShield(void* defender, const char* what) {
  if (!Faithful() || !defender || HasSpellEffect(defender, rules::kShield)) return false;  // up already: counted
  if (engine::IsDeadOrDying(defender) || IncapacitatedByCondition(defender) || !ReactionReady(defender)) return false;
  if (!ReactionAllowed(defender, rules::Reaction::kShield)) return false;  // switched off: the Reaction kept
  ShieldSource source;
  if (!ShieldSourceOf(defender, &source)) return false;
  if (!TakeReaction(defender)) return false;
  const uint32_t id = Field<uint32_t>(defender, ee::kObjectId);
  bool spent = true;
  if (source.innate) {
    RunScriptChunk("DecrementRemainingSpellUses(OBJECT_SELF, 417);", id);
  } else {
    t_reaction_spend = true;
    spent = SpendOrderSlot(defender, rules::kShield, source.class_slot, source.domain, 0);
    t_reaction_spend = false;
  }
  if (!spent) {
    if (LogSlot()) Log("spells2024: 0x%08X Shield - its slot couldn't be spent (%s)", id, what);
    return false;
  }
  ShieldUp(id);
  if (LogSlot()) {
    Log("spells2024: 0x%08X Shield (its Reaction, %s; %s) against %s", id,
        source.innate ? "its own use" : source.domain > 0 ? "a higher slot" : "a level 1 slot",
        source.innate ? "innate" : "class slot", what);
  }
  return true;
}

bool ShieldTurns(void* defender, void* attacker, int total, int armor_class, bool natural_twenty, const char* what) {
  if (!defender || (!natural_twenty && total < armor_class)) return false;  // not a hit
  if (!rules::ShieldTurnsHit(total, armor_class, natural_twenty)) return false;
  if (!TakeShield(defender, what)) return false;
  TellPlayer(Field<uint32_t>(defender, ee::kObjectId),
             PlayerText("spells2024.shield_reaction",
                        {{"total", std::to_string(total)}, {"ac", std::to_string(armor_class)}}));
  (void)attacker;
  return true;
}

// A weapon attack's hit Shield would turn: a miss (first of the attack roll's after-listeners - before a hit's riders).
void ShieldAfterRoll(AttackRoll& roll) {
  if (!Faithful() || !roll.data || !roll.target || !roll.attacker) return;
  const uint8_t result = roll.data[ee::kAttackResult];
  if (result != 1 && result != ee::kAttackResultCritical) return;
  void* defender = CreatureById(Field<uint32_t>(roll.target, ee::kObjectId));
  void* stats = defender ? StatsOf(defender) : nullptr;
  if (!stats) return;
  static const auto armor_class = reinterpret_cast<ArmorClassFn>(EngineExport(ee::kGetArmorClassVersus));
  if (!armor_class) return;
  const int d20 = roll.data[ee::kAttackD20];
  const int total = d20 + *reinterpret_cast<int32_t*>(roll.data + ee::kAttackModifier);
  const int ac = armor_class(stats, roll.attacker, 0);
  if (!ShieldTurns(defender, roll.attacker, total, ac, d20 == 20, "an attack")) return;
  roll.data[ee::kAttackResult] = ee::kAttackResultMiss;
  roll.data[ee::kAttackMissedBy] = static_cast<uint8_t>(std::max(1, ac + rules::kShieldAc - total));
  if (result == ee::kAttackResultCritical) {
    *reinterpret_cast<int32_t*>(roll.data + ee::kAttackThreatFlag) = 0;
    roll.data[ee::kAttackThreatRoll] = 0;
  }
}

// Cast from the spells: Shield +5 until the next round; Feather Fall a visual and a line (#153).
void ReactionSpell(uint32_t self, const rules::Spell2024& spell) {
  if (spell.row == rules::kShield) {
    ShieldUp(self);
    TellPlayer(self, PlayerText("spells2024.shield_5_ac_until_your"));
    if (LogSlot()) Log("spells2024: 0x%08X Shield cast from its spells: +5 AC for a round", self);
    return;
  }
  RunScriptChunk("ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WIND), OBJECT_SELF);", self);
  TellPlayer(self, PlayerText("spells2024.feather_fall_nwn_has_no"));
  if (LogSlot()) Log("spells2024: 0x%08X Feather Fall: a visual and a line (#153)", self);
}

// --- The smites (Q1 A): armed by the cast for nothing, paid and delivered by the next qualifying hit ---

struct Armed {
  int row = 0;
  int class_slot = -1;
  int domain_level = 0;
  int metamagic = 0;
  int slot = 1;       // the slot level it's paid with
  bool paid = false;  // armed by a cast that spent its slot already (an item's, a script's)
  uint64_t told = 0;  // the last "it waits" line (game ms)
};
std::unordered_map<uint32_t, Armed> g_armed;  // the caster's armed smite
struct SmitePending {  // a smite landed: its rider at the target's next update (not inside the damage's handling)
  uint32_t caster = 0;
  int row = 0;
  int slot = 1;
  int dc = 10;
};
std::unordered_map<uint32_t, SmitePending> g_smite_pending;  // by target
struct Burning {  // Searing Smite
  uint32_t caster = 0;
  int dice = 1;
  int dc = 10;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Burning> g_burning;
struct Ensnared {  // Ensnaring Strike's vines (its hold: g_entangled)
  uint32_t caster = 0;
  int dice = 1;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Ensnared> g_ensnared;

bool IsSmite(int row) { return rules::IsSmiteSpell(row); }

const char* SmiteRide(int row) {
  return row == rules::kEnsnaringStrike ? "a weapon" : "a melee weapon or an Unarmed Strike";
}

bool ArmSmite(void* creature, uint32_t id, const SpellOrder& order) {
  const rules::Spell2024* spell = rules::Spell2024Of(static_cast<int>(order.spell));
  if (!spell || order.class_slot < 0 || order.class_slot > 7) return false;  // not a class's: as the engine casts it
  if (OrderSlotsLeft(creature, order.spell, order.class_slot, order.domain_level, order.metamagic) <= 0) {
    return Refuse(id, spell->name, PlayerText("spells2024.refuse_no_slot"), id);
  }
  Armed armed;
  armed.row = spell->row;
  armed.class_slot = order.class_slot;
  armed.domain_level = order.domain_level;
  armed.metamagic = order.metamagic;
  const int chosen = rules::ChosenSlotLevel(spell->level, order.domain_level);
  armed.slot = chosen >= spell->level ? chosen : spell->level;
  if (g_armed.size() > 256) g_armed.clear();
  g_armed[id] = armed;
  TellPlayer(id, PlayerText("spells2024.smite_armed", {{"spell", spell->name}, {"slot", std::to_string(armed.slot)},
                                                      {"ride", SmiteRide(spell->row)}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s armed (a level %d slot, class slot %d) - nothing spent", id, spell->name,
                     armed.slot, armed.class_slot);
  return true;  // the cast refused: nothing spent now
}

// A smite's cast that ran (an item's, a script's): armed, its slot spent by that cast.
void ArmFromScript(uint32_t self, const rules::Spell2024& spell, int slot) {
  Armed armed;
  armed.row = spell.row;
  armed.slot = slot;
  armed.paid = true;
  g_armed[self] = armed;
  TellPlayer(self, PlayerText("spells2024.armed_your_next_qualifying_hit", {{"name", std::string(spell.name)}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s armed by its cast (paid, a level %d slot)", self, spell.name, slot);
}

void SmiteVisual(uint32_t caster, uint32_t target, const char* vis) {
  char code[260];
  sprintf_s(code, "object t = StringToObject(\"%x\"); DelayCommand(0.1, ApplyEffectToObject(DURATION_TYPE_INSTANT, "
                  "EffectVisualEffect(%s), t));", target, vis);
  RunScriptChunk(code, caster);
}

// A hit: the armed smite's cost, then its damage on the hit (twice the dice on a Critical Hit) and its rider later.
void SmiteHit(void* attacker, void* target) {
  if (g_armed.empty() || !attacker || !target || !Faithful()) return;
  const uint32_t id = Field<uint32_t>(attacker, ee::kObjectId);
  const auto it = g_armed.find(id);
  if (it == g_armed.end()) return;
  uint8_t* data = CurrentAttackData(attacker);
  if (!data || !rules::MasteryAttackHit(data[ee::kAttackResult])) return;
  const uint32_t victim_id = Field<uint32_t>(target, ee::kObjectId);
  void* victim = CreatureById(victim_id);
  if (!victim) return;  // a creature's hit only
  const bool ranged = *reinterpret_cast<int32_t*>(data + ee::kAttackRanged) != 0;
  const bool unarmed = !ranged && AttackWeaponBaseItem(attacker) < 0;
  Armed& armed = it->second;
  if (!rules::SmiteRidesOn(armed.row, ranged, unarmed)) return;
  const rules::Spell2024* spell = rules::Spell2024Of(armed.row);
  if (!spell) return;
  const uint64_t now = GameMs();
  std::string waits;
  if (BonusActionStatus(attacker) != 0) {
    waits = "your Bonus Action is spent this round";
  } else if (!armed.paid && SpentThisTurn(id, nullptr)) {
    waits = "you've spent a spell slot this turn (one spell with a slot a turn)";
  }
  if (!waits.empty()) {
    if (now >= armed.told + kRoundMs) {
      armed.told = now;
      TellPlayer(id, PlayerText("spells2024.waits_it_stays_armed", {{"name", std::string(spell->name)}, {"waits", std::string(waits)}}));
      if (LogSlot()) Log("spells2024: 0x%08X %s waits - %s", id, spell->name, waits.c_str());
    }
    return;
  }
  if (!armed.paid) {
    if (OrderSlotsLeft(attacker, armed.row, armed.class_slot, armed.domain_level, armed.metamagic) <= 0) {
      TellPlayer(id, PlayerText("spells2024.no_spell_slot_of_that_2", {{"name", std::string(spell->name)}}));
      g_armed.erase(it);
      return;
    }
    t_bonus_spend = true;  // a Bonus Action spell's slot (one spell with a slot a turn)
    const bool spent = SpendOrderSlot(attacker, armed.row, armed.class_slot, armed.domain_level, armed.metamagic);
    t_bonus_spend = false;
    if (!spent) {
      TellPlayer(id, PlayerText("spells2024.its_slot_couldn_t_be", {{"name", std::string(spell->name)}}));
      g_armed.erase(it);
      return;
    }
  }
  SpendBonusAction(attacker, spell->name);
  const Armed paid = armed;
  g_armed.erase(it);
  const bool critical = data[ee::kAttackResult] == ee::kAttackResultCritical;
  auto* entries = reinterpret_cast<int16_t*>(data + ee::kAttackDamageEntries);
  const int dc = SpellSaveDcForClassSlot(attacker, paid.class_slot);
  std::string log;
  switch (paid.row) {
    case rules::kDivineSmite: {
      void* stats = StatsOf(victim);
      const int race = stats ? engine::RaceOf(stats) : -1;
      bool evil = false;
      if (race == 20) {  // an Outsider: a Fiend if evil
        char code[200];
        sprintf_s(code, "SetLocalInt(OBJECT_SELF, \"n5s_evil\", GetAlignmentGoodEvil(StringToObject(\"%x\")) == "
                        "ALIGNMENT_EVIL);", victim_id);
        RunScriptChunk(code, id);
        evil = engine::GetLocalInt(attacker, "n5s_evil") == 1;
      }
      const bool fiend_or_undead = rules::FiendOrUndead(race, evil);
      const int dice = rules::CriticalDice(rules::DivineSmiteDice(paid.slot, fiend_or_undead), critical);
      const int amount = RollEngineDice(dice, 8);
      rules::AddDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeRadiant, amount);
      SmiteVisual(id, victim_id, "VFX_IMP_SUNSTRIKE");
      char text[160];
      sprintf_s(text, "%dd8 Radiant = %d%s", dice, amount, fiend_or_undead ? " (a Fiend or an Undead)" : "");
      log = text;
      break;
    }
    case rules::kSearingSmite: {
      const int dice = rules::CriticalDice(rules::SearingSmiteDice(paid.slot), critical);
      const int amount = RollEngineDice(dice, 6);
      rules::AddDamage(entries, ee::kAttackDamageEntryCount, rules::kDamageTypeFire, amount);
      SmiteVisual(id, victim_id, "VFX_IMP_FLAME_S");
      g_smite_pending[victim_id] = SmitePending{id, paid.row, paid.slot, dc};
      char text[120];
      sprintf_s(text, "%dd6 Fire = %d, and it burns (Constitution save, DC %d)", dice, amount, dc);
      log = text;
      break;
    }
    case rules::kEnsnaringStrike:
      g_smite_pending[victim_id] = SmitePending{id, paid.row, paid.slot, dc};
      log = "its vines (Strength save, DC " + std::to_string(dc) + ")";
      break;
  }
  TellPlayer(id, PlayerText("spells2024.a_level_slot_your_bonus", {{"name", std::string(spell->name)}, {"slot", std::string(std::to_string(paid.slot))}, {"log", std::string(log)}}));
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s on 0x%08X (a level %d slot%s): %s", id, spell->name, victim_id, paid.slot,
        critical ? ", a Critical Hit" : "", log.c_str());
  }
}

uint8_t StrengthSave(void* creature, uint32_t caster, int dc, bool advantage) {
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through its hook
  if (!save_roll) return 1;
  RememberSaveDc2024(caster, dc);
  SetForcedSaveAbility(rules::Ability::kStrength);
  SetForcedSaveAdvantage(advantage);
  const uint8_t save = save_roll(creature, ee::kSaveFortitude, static_cast<uint16_t>(dc), 0, caster, 1, 0, 0);
  SetForcedSaveAdvantage(false);
  SetForcedSaveAbility(std::nullopt);
  return save;
}

// The riders, at the target's update: Searing Smite's burning; Ensnaring Strike's save and vines. Then each of its
// turns: the burn (1d6 Fire, a Constitution save ends it), the vines' 1d6 Piercing.
// #162 (owner, 2026-11-13: "go with G, B and D for #162"): an armed smite shown. Kept in step from the creature's
// update - never inside an attack's damage - so every way a smite ends (its hit, a rest, a click, no slot left,
// another smite cast) and a load (the icon saved, the arming not) is followed.
constexpr const char* kTagArmedIcon = "n5s_armed";
std::unordered_map<uint32_t, int> g_armed_look;  // creature -> the smite whose look is on (B, D)
std::unordered_map<uint32_t, int> g_armed_icon;  // creature -> the armed icon on its effect bar (G)
std::unordered_set<uint32_t> g_armed_seen;       // creatures updated this game (a loaded save's icon looked for once)
struct IconTry {
  int want = -1;
  uint64_t at = 0;
};
std::unordered_map<uint32_t, IconTry> g_icon_tried;

int ArmedIconShown(const void* creature) {
  int shown = 0;
  effects::ForEachOfType(creature, ee::kEffectTypeIcon, [&](const void* effect) {
    const int icon = effects::Integer(effect, 0);
    if (icon >= rules::kSmiteArmedIconFirst && icon <= rules::kSmiteArmedIconLast) shown = icon;
  });
  return shown;
}

// B, D: the look of an armed smite - the animation session's (a flash and the smite's sound as it arms; the weapon's
// glow while armed), through its PlaySpellLookArmed (anim_look_player.h; anim 804dfc4).
void ArmedLook(void* creature, uint32_t id, int row, bool armed) {
  PlaySpellLookArmed(creature, row, armed);
  const rules::Spell2024* spell = rules::Spell2024Of(row);
  if (LogSlot()) {
    Log("spells2024: 0x%08X %s %s (its look: #162)", id, spell ? spell->name : "a smite",
        armed ? "armed" : "no longer armed");
  }
}

void SyncArmed(void* creature, uint32_t id, bool fresh) {
  const auto armed = g_armed.find(id);
  const int row = armed != g_armed.end() ? armed->second.row : 0;
  // B, D: at each change of the armed smite.
  const auto look = g_armed_look.find(id);
  const int looked = look != g_armed_look.end() ? look->second : 0;
  if (looked != row) {
    if (looked) ArmedLook(creature, id, looked, false);
    if (row) ArmedLook(creature, id, row, true);
    if (row) {
      g_armed_look[id] = row;
    } else {
      g_armed_look.erase(id);
    }
  }
  // G: its icon on the effect bar.
  const int want = rules::SmiteArmedIconRow(row);
  const auto on = g_armed_icon.find(id);
  const int have = fresh ? ArmedIconShown(creature) : (on != g_armed_icon.end() ? on->second : 0);
  if (want == have) return;
  const uint64_t now = GetTickCount64();
  IconTry& tried = g_icon_tried[id];
  if (tried.want == want && now < tried.at + 1000) return;  // the same change at most once a second
  tried.want = want;
  tried.at = now;
  std::string code = std::string("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"") +
                     kTagArmedIcon + "\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }";
  if (want > 0) {
    code += " ApplyEffectToObject(DURATION_TYPE_PERMANENT, SupernaturalEffect(TagEffect(EffectIcon(" +
            std::to_string(want) + "), \"" + kTagArmedIcon + "\")), OBJECT_SELF);";
  }
  RunScriptChunk(code.c_str(), id);
  if (want > 0) {
    g_armed_icon[id] = want;
  } else {
    g_armed_icon.erase(id);
  }
}

// G: the resource panel's line while a smite is armed; a click disarms it.
bool SmiteHudPool(void* creature, rules::HudPool* pool) {
  if (!creature || g_armed.empty()) return false;
  const auto it = g_armed.find(Field<uint32_t>(creature, ee::kObjectId));
  if (it == g_armed.end()) return false;
  const rules::Spell2024* spell = rules::Spell2024Of(it->second.row);
  if (!spell) return false;
  pool->name = std::string("Armed: ") + spell->name + " (a level " + std::to_string(it->second.slot) + " slot)";
  pool->line = true;
  pool->click = "smite";
  pool->tooltip = std::string(spell->name) + " is armed: your next hit with " + SmiteRide(spell->row) +
                  ", while you have the slot and your Bonus Action, spends both and delivers it. A rest disarms "
                  "it.\n\nClick to disarm it now.";
  return true;
}

void SmitePanelClicked(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto it = g_armed.find(id);
  if (it == g_armed.end()) return;
  const rules::Spell2024* spell = rules::Spell2024Of(it->second.row);
  const bool paid = it->second.paid;
  g_armed.erase(it);
  const std::string name = spell ? std::string(spell->name) : PlayerText("spells2024.the_smite");
  TellPlayer(id, PlayerText(paid ? "spells2024.smite_disarmed_paid" : "spells2024.smite_disarmed_free",
                            {{"name", name}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s disarmed (the resource panel)", id, name.c_str());
}

// A game started (a new one, a saved one loaded): the last game's armed smites go (a loaded icon is taken off at
// its creature's first update). Their looks stay listed (0.99.34, AS-9: the owner, 2026-11-13: "the glow doesn't
// disappear after load, it shouldn't even appear as the smite isn't armed"): the creature's first update then
// sends the look its armed = false - the glow the save kept taken off at once.
void SmitesGameStarted() {
  g_armed.clear();
  g_armed_icon.clear();
  g_armed_seen.clear();
  g_icon_tried.clear();
}

void SmiteUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const bool fresh = g_armed_seen.size() < 100000 && g_armed_seen.insert(id).second;
  if (fresh || !g_armed.empty() || !g_armed_look.empty() || !g_armed_icon.empty()) SyncArmed(creature, id, fresh);
  if (g_smite_pending.empty() && g_burning.empty() && g_ensnared.empty()) return;
  const uint64_t now = GameMs();
  if (!g_smite_pending.empty()) {
    const auto at = g_smite_pending.find(id);
    if (at != g_smite_pending.end()) {
      const SmitePending pending = at->second;
      g_smite_pending.erase(at);
      void* caster = CreatureById(pending.caster);
      if (caster && !engine::IsDeadOrDying(creature)) {
        if (pending.row == rules::kSearingSmite) {
          // 0.99.41 (L3-13: the burn's log showed no effect of the spell on the target): an EffectIcon linked with a
          // visual never stays (0.92.2's probe; Resistance's marker, 0.95.9) - the icon (the burn's mark) and the
          // flames go on as two effects, both with the row and the tag, so the save's end takes both.
          char code[600];
          sprintf_s(code,
                    "object t = StringToObject(\"%x\"); ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect("
                    "SetEffectSpellId(EffectIcon(EFFECT_ICON_DAMAGE_DECREASE), %d), \"%s\"), t, 60.0);"
                    " ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId("
                    "EffectVisualEffect(VFX_DUR_INFERNO_CHEST), %d), \"%s\"), t, 60.0);",
                    id, rules::kSearingSmite, kTagSearing, rules::kSearingSmite, kTagSearing);
          const int ran = RunScriptChunk(code, pending.caster);
          g_burning[id] = Burning{pending.caster, rules::SearingSmiteDice(pending.slot), pending.dc, now + kRoundMs};
          // 0.99.36 (L3-5: no burn round came): the mark as it went on, for the log.
          if (LogSlot()) {
            Log("spells2024: 0x%08X Searing Smite's mark put on by 0x%08X (its script %d): the first effect of spell %d "
                "on it is 0x%08X's; the next burn at game %.1f s",
                id, pending.caster, ran, rules::kSearingSmite, SpellEffectCreator(creature, rules::kSearingSmite),
                static_cast<double>(now + kRoundMs) / 1000.0);
          }
        } else if (pending.row == rules::kEnsnaringStrike) {
          const bool advantage = rules::EnsnaringSaveAdvantage(Field<int32_t>(creature, ee::kCreatureSize));
          const uint8_t save = StrengthSave(creature, pending.caster, pending.dc, advantage);
          if (LogSlot()) {
            Log("spells2024: 0x%08X Ensnaring Strike's Strength save (DC %d%s): %s", id, pending.dc,
                advantage ? ", Advantage: Large or larger" : "", save == 0 ? "failed - Restrained" : "succeeded");
          }
          if (save == 0) {
            BeginConcentration(caster, rules::kEnsnaringStrike);
            // 0.99.30 (L3-8): NWN's immunity to entangling (Freedom of Movement, a creature's own) refuses the hold -
            // said, not left to vanish unexplained.
            char code[600];
            sprintf_s(code,
                      "object t = StringToObject(\"%x\"); SetLocalInt(t, \"%s\", GetIsImmune(t, "
                      "IMMUNITY_TYPE_ENTANGLE, OBJECT_SELF)); ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect("
                      "SetEffectSpellId(EffectLinkEffects(EffectEntangle(), EffectVisualEffect(VFX_DUR_ENTANGLE)), %d), "
                      "\"%s\"), t, 60.0);",
                      id, kVarVinesImmune, rules::kEnsnaringStrike, kTagEnsnared);
            RunScriptChunk(code, pending.caster);
            if (engine::GetLocalInt(creature, kVarVinesImmune) != 0) {
              if (LogSlot()) Log("spells2024: 0x%08X immune to entangling - Ensnaring Strike's vines can't hold it", id);
              TellPlayer(pending.caster, PlayerText("spells2024.ensnaring_strike_the_vines_can"));
              TellPlayer(id, PlayerText("spells2024.ensnaring_strike_s_vines_can"));
            } else {
              Entangled held{pending.caster, pending.dc, now + kRoundMs, false, rules::kEnsnaringStrike, kTagEnsnared};
              if (IsPlayerCreature(creature)) {
                if (void* stats = StatsOf(creature)) {
                  if (!engine::HasFeat(stats, rules::kFeatEscape)) engine::AddFeat(stats, rules::kFeatEscape);
                  held.feat = true;
                  TellPlayer(id, PlayerText("spells2024.ensnaring_strike_s_vines_hold"));
                }
              }
              g_entangled[id] = held;
              g_ensnared[id] = Ensnared{pending.caster, rules::EnsnaringStrikeDice(pending.slot), now + kRoundMs};
            }
          } else {
            TellPlayer(pending.caster, PlayerText("spells2024.ensnaring_strike_the_vines_shrivel"));
          }
        }
      }
    }
  }
  if (!g_burning.empty()) {
    const auto at = g_burning.find(id);
    if (at != g_burning.end()) {
      Burning& burn = at->second;
      const bool down = engine::IsDeadOrDying(creature);
      if (down || !HasSpellEffect(creature, rules::kSearingSmite, -1, burn.caster)) {
        if (LogSlot()) {  // 0.99.36 (L3-5): why a burn ends without its save
          Log("spells2024: 0x%08X Searing Smite's burn ends - %s (game %.1f s; the first effect of spell %d on it is "
              "0x%08X's, the burn's caster 0x%08X)",
              id, down ? "it's down" : "its mark is gone", static_cast<double>(now) / 1000.0, rules::kSearingSmite,
              SpellEffectCreator(creature, rules::kSearingSmite), burn.caster);
        }
        g_burning.erase(at);
      } else if (now >= burn.next) {
        burn.next = now + kRoundMs;
        const int amount = RollEngineDice(burn.dice, 6);
        char code[300];
        sprintf_s(code, "object t = StringToObject(\"%x\"); ApplyEffectToObject(DURATION_TYPE_INSTANT, "
                        "EffectLinkEffects(EffectDamage(%d, DAMAGE_TYPE_FIRE), EffectVisualEffect(VFX_IMP_FLAME_S)), t);",
                  id, amount);
        RunScriptChunk(code, CreatureById(burn.caster) ? burn.caster : id);
        static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));
        uint8_t save = 1;
        if (save_roll) {
          RememberSaveDc2024(burn.caster, burn.dc);
          save = save_roll(creature, ee::kSaveFortitude, static_cast<uint16_t>(burn.dc), 0, burn.caster, 1, 0, 0);
        }
        if (LogSlot()) {
          Log("spells2024: 0x%08X Searing Smite burns: %dd6 Fire = %d; Constitution save (DC %d) %s", id, burn.dice,
              amount, burn.dc, save == 0 ? "failed - it burns on" : "succeeded - it ends");
        }
        if (save != 0) {
          std::string unmark = kUnmark;
          Replace(&unmark, "$TAG", kTagSearing);
          Run(unmark, id, "Searing Smite's end");
          g_burning.erase(at);
        }
      }
    }
  }
  if (!g_ensnared.empty()) {
    const auto at = g_ensnared.find(id);
    if (at != g_ensnared.end()) {
      Ensnared& vines = at->second;
      if (engine::IsDeadOrDying(creature) || !HasSpellEffect(creature, rules::kEnsnaringStrike, -1, vines.caster)) {
        g_ensnared.erase(at);
      } else if (now >= vines.next) {
        vines.next = now + kRoundMs;
        const int amount = RollEngineDice(vines.dice, 6);
        char code[260];
        sprintf_s(code, "object t = StringToObject(\"%x\"); ApplyEffectToObject(DURATION_TYPE_INSTANT, "
                        "EffectDamage(%d, DAMAGE_TYPE_PIERCING), t);", id, amount);
        RunScriptChunk(code, CreatureById(vines.caster) ? vines.caster : id);
        if (LogSlot()) Log("spells2024: 0x%08X Ensnaring Strike's vines: %dd6 Piercing = %d", id, vines.dice, amount);
      }
    }
  }
}

// A rest disarms a smite.
void SmitesRest(void* stats) {
  void* creature = stats ? CreatureOf(stats) : nullptr;
  if (!creature || g_armed.empty()) return;
  g_armed.erase(Field<uint32_t>(creature, ee::kObjectId));
}

// ===== #39 Part 2a build 4a (owner, 2026-11-15: "go for build 4"; 2026-11-16: "I accepted your recommendations,
// except for Q4 ... Keep it in the roleplay list"): detection and utility, and rituals.
constexpr int kAlarmArea = 116;        // vfx_persistent.if_spells2024.txt: the ward (no look)
constexpr int kGoodberryPower = 600;   // iprp_spells.if_spells2024.txt: the berries' Cast Spell (spell 1197)
constexpr ULONGLONG kDetectEveryMs = 2000;
constexpr const char* kTagDetectMark = "n5s_detect";
constexpr const char* kTagUtility = "n5s_utility";
constexpr const char* kVarRitualOff = "nwn5e_ritual_off";  // a record: 1 - Rituals cast with a slot (Q1 A's switch)

// The effect icons (effecticons.if_spells2024.txt 228-233) of the spells' marks on the caster.
int UtilityIconRow(int row) {
  switch (row) {
    case rules::kDetectEvilAndGood: return 228;
    case rules::kDetectMagic: return 229;
    case rules::kDetectPoisonAndDisease: return 230;
    case rules::kComprehendLanguages: return 231;
    case rules::kSpeakWithAnimals: return 232;
    case rules::kUnseenServant: return 233;
    case rules::kDisguiseSelf: return 234;  // build 4b: the disguise's hour
    case rules::kSilentImage: return 235;   // Silent Image's mark (its Concentration)
    default: return 0;
  }
}

// A duration in a script (Shield of Faith's 10 minutes, Comprehend Languages' hour): real seconds - #39 (owner,
// 2026-11-17: "go with A. no real-time clock"). Before, hours were the module's (HoursToSeconds), minutes turns.
std::string DurationScript(int minutes) { return SecondsScript(minutes); }

// NWScript (a script has no functions of its own): `sn`, the school (rules::SpellSchoolName) of spell row `id`.
std::string SchoolScript() {
  std::string code = "string sl = Get2DAString(\"spells\", \"School\", id); string sn = \"\";";
  for (char letter : std::string("ACDEVINT")) {
    const char* name = rules::SpellSchoolName(letter);
    if (name) code += std::string(" if (sl == \"") + letter + "\") sn = \"" + name + "\";";
  }
  return code;
}

// NWScript: `what` for a creature `o` Detect Evil and Good senses (rules::DetectEvilAndGoodKind).
std::string EvilAndGoodScript() {
  std::string code = "    int race = GetRacialType(o); int ge = GetAlignmentGoodEvil(o); string k = \"\";";
  for (int race = 0; race < 32; ++race) {
    for (int ge : {1, 4, 5}) {
      const char* kind = rules::DetectEvilAndGoodKind(race, ge);
      if (kind) {
        code += " if (race == " + std::to_string(race) + " && ge == " + std::to_string(ge) + ") k = \"" + kind + "\";";
      }
    }
  }
  return code + " if (k != \"\") what = GetName(o) + \", \" + k;\n";
}

// NWScript: the kind Detect Poison and Disease reads off item property `ip` of a weapon (`natural`: a creature's
// own), added to `k` (rules::PoisonSenseKind - a weapon's On Hit and a creature weapon's On Monster Hit; 0.99.43,
// L4-9: the giant spider's bite is the second).
std::string PoisonScript() {
  std::string code = "          int pt = GetItemPropertyType(ip); int st = GetItemPropertySubType(ip); string pk = \"\";";
  for (int property : {rules::kItemPropertyOnHit, rules::kItemPropertyOnMonsterHit}) {
    for (int subtype = 0; subtype < 64; ++subtype) {
      for (int natural = 0; natural < 2; ++natural) {
        const char* kind = rules::PoisonSenseKind(property, subtype, natural != 0);
        if (!kind) continue;
        code += " if (pt == " + std::to_string(property) + " && st == " + std::to_string(subtype) +
                " && natural == " + std::to_string(natural) + ") pk = \"" + kind + "\";";
      }
    }
  }
  return code + "\n          if (pk != \"\" && FindSubString(k, pk) < 0) k += (k == \"\" ? \"\" : \", \") + pk;\n";
}

// Detect Magic: creatures and objects under spells (each named with its school), items and creatures bearing magic
// items; its areas go to `aoes` (named from their spell by the DLL: a script can't read an area's spell).
const char* const kMagicSense = R"nss(
    int t = GetObjectType(o);
    if (t == OBJECT_TYPE_AREA_OF_EFFECT) aoes += "|" + ObjectToString(o) + "|";
    else {
      string sp = ""; string ids = "";
      effect e = GetFirstEffect(o);
      while (GetIsEffectValid(e)) {
        int id = GetEffectSpellId(e);
        if (id >= 0 && id != row && FindSubString(ids, "|" + IntToString(id) + "|") < 0) {
          ids += "|" + IntToString(id) + "|";
          if (GetStringLength(sp) < 160) {
            $SCHOOL
            string nm = GetStringByStrRef(StringToInt(Get2DAString("spells", "Name", id)));
            sp += (sp == "" ? "" : ", ") + nm + (sn == "" ? "" : " (" + sn + ")");
          }
        }
        e = GetNextEffect(o);
      }
      int magic = FALSE;
      if (t == OBJECT_TYPE_ITEM) {
        itemproperty mp = GetFirstItemProperty(o); int mi;
        for (mi = 0; mi < 8 && !magic; mi++) { if (GetIsItemPropertyValid(mp)) magic = TRUE; mp = GetNextItemProperty(o); }
      }
      if (t == OBJECT_TYPE_CREATURE) {
        int s;
        for (s = 0; s < 14 && !magic; s++) {
          object it = GetItemInSlot(s, o);
          if (GetIsObjectValid(it)) {
            itemproperty cp = GetFirstItemProperty(it); int ci;
            for (ci = 0; ci < 8 && !magic; ci++) { if (GetIsItemPropertyValid(cp)) magic = TRUE; cp = GetNextItemProperty(it); }
          }
        }
      }
      if (sp != "") what = GetName(o) + " - under " + sp;
      if (magic) what = (what == "" ? GetName(o) + " - " : what + "; ") + (t == OBJECT_TYPE_ITEM ? "a magic item" : "it bears magic items");
    }
)nss";

// Detect Poison and Disease: poisoned or diseased creatures, venomous ones (natural weapons that poison), poisoned
// weapons (held, or lying there), attacks that spread disease - each weapon's properties read by $POISON
// (PoisonScript).
const char* const kPoisonSense = R"nss(
    int t = GetObjectType(o); string k = "";
    if (t == OBJECT_TYPE_CREATURE) {
      effect e = GetFirstEffect(o);
      while (GetIsEffectValid(e)) {
        int et = GetEffectType(e);
        if (et == EFFECT_TYPE_POISON && FindSubString(k, "poisoned") < 0) k += (k == "" ? "" : ", ") + "poisoned";
        if (et == EFFECT_TYPE_DISEASE && FindSubString(k, "diseased") < 0) k += (k == "" ? "" : ", ") + "diseased (a contagion)";
        e = GetNextEffect(o);
      }
    }
    int i;
    for (i = 0; i < 5; i++) {
      object w = OBJECT_INVALID;
      if (t == OBJECT_TYPE_ITEM) { if (i == 0) w = o; }
      else w = GetItemInSlot(i == 0 ? 14 : (i == 1 ? 15 : (i == 2 ? 16 : (i == 3 ? 4 : 5))), o);
      if (GetIsObjectValid(w)) {
        int natural = t == OBJECT_TYPE_CREATURE && i < 3;
        itemproperty ip = GetFirstItemProperty(w); int pw;
        for (pw = 0; pw < 8; pw++) {
          if (GetIsItemPropertyValid(ip)) {
$POISON          }
          ip = GetNextItemProperty(w);
        }
      }
    }
    if (k != "") what = GetName(o) + " - " + k;
)nss";

// One look of a Detect: what it senses within its range and in a clear line glows (an effect of the spell by the
// caster - Concentration's end takes it) and is named once; what no longer is loses the glow. 0.99.43: Detect
// Magic's $SIGHT drops what the caster can't see (L4-2: nothing from inside a Fog Cloud - n5d_blind, set by the
// DLL -, no creature unseen, no object inside a Fog Cloud - $FOGS lists those near) and says once that it senses
// magic it can't see; the glows added and taken go to n5d_chg for the log (L4-1: the sword's glow blinked).
// 0.99.45 (L4-14 and L4-20: the sword's glow added one look and taken the next, every 2 s - "found, not sensed"):
// an item's properties are effects in its applied-effects list, and the engine's GetFirst/NextItemProperty walk
// returns any exposed temporary or permanent effect there but type 67 (nwmain disassembled) - the glow (a visual
// effect, type 30, temporary) came first, GetIsItemPropertyValid (type 91) said no, and the sword was no magic
// item while it glowed. Every property walk here steps past what isn't a property, 8 entries at most (the walk
// starts over after its end; an item lying there carries a Detect's glow or two).
const char* const kDetectScan = R"nss(
object me = OBJECT_SELF; int row = $ROW; string told = GetLocalString(me, "n5d_told"); string seen = ""; string aoes = "";
string chg = ""; int n = 0; int hidden = 0; int blind = GetLocalInt(me, "n5d_blind"); location here = GetLocation(me);
string found = "";
$FOGS
object o = GetFirstObjectInShape(SHAPE_SPHERE, $RADIUS, here, TRUE, $FILTER);
while (GetIsObjectValid(o)) {
  string what = ""; found += "|" + ObjectToString(o) + "|";
  if (o != me) {
$CLASSIFY
$SIGHT
  }
  if (what != "") {
    string key = "|" + ObjectToString(o) + "|";
    seen += key;
    if (!GetHasSpellEffect(row, o)) { ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectVisualEffect($GLOW), row), "n5s_detglow"), o, TurnsToSeconds(10)); chg += " +" + GetName(o); }
    if (FindSubString(told, key) < 0) { told += key; if (n < 8) SendMessageToPC(me, "nwn5e: $NAME - " + what + "."); n++; }
  }
  o = GetNextObjectInShape(SHAPE_SPHERE, $RADIUS, here, TRUE, $FILTER);
}
string old = GetLocalString(me, "n5d_glow");
int p = FindSubString(old, "|");
while (p >= 0) {
  int q = FindSubString(old, "|", p + 1);
  if (q < 0) break;
  string key = GetSubString(old, p, q - p + 1);
  if (FindSubString(seen, key) < 0) {
    object g = StringToObject(GetSubString(key, 1, q - p - 1));
    effect e = GetFirstEffect(g);
    while (GetIsEffectValid(e)) {
      if (GetEffectTag(e) == "n5s_detglow" && GetEffectCreator(e) == me) RemoveEffect(g, e);
      e = GetNextEffect(g);
    }
    if (GetIsObjectValid(g)) chg += " -" + GetName(g) + (FindSubString(found, key) >= 0 ? " (found, not sensed)" : "");
  }
  p = FindSubString(old, "|", q + 1);
}
if (hidden > 0 && !GetLocalInt(me, "n5d_hid")) SendMessageToPC(me, "nwn5e: $NAME - you sense magic within $FEET feet that you can't see.");
SetLocalInt(me, "n5d_hid", hidden > 0);
SetLocalString(me, "n5d_told", told); SetLocalString(me, "n5d_glow", seen); SetLocalString(me, "n5d_aoes", aoes);
SetLocalString(me, "n5d_chg", chg);
)nss";

// Detect Magic (rules::DetectNeedsSight): the Fog Clouds near the caster (their tag NWN5E_FOG_CLOUD_<slot>, the
// radius rules::FogCloudRadiusFeet's), listed before the look - a second shape search inside the first would end it.
const char* const kDetectFogs = R"nss(
int nf = 0;
object fz = GetFirstObjectInShape(SHAPE_SPHERE, $FOGREACH, here, FALSE, OBJECT_TYPE_AREA_OF_EFFECT);
while (GetIsObjectValid(fz) && nf < 8) {
  string tz = GetStringUpperCase(GetTag(fz));
  if (GetStringLeft(tz, 16) == "NWN5E_FOG_CLOUD_") {
    int sz = StringToInt(GetSubString(tz, 16, 2)); float rz = 0.0;
$FOGRADII
    SetLocalObject(me, "n5d_fog" + IntToString(nf), fz); SetLocalFloat(me, "n5d_fr" + IntToString(nf), rz); nf++;
  }
  fz = GetNextObjectInShape(SHAPE_SPHERE, $FOGREACH, here, FALSE, OBJECT_TYPE_AREA_OF_EFFECT);
}
)nss";

const char* const kDetectSight = R"nss(
    if (what != "") {
      int unseen = blind; int ot = GetObjectType(o); int f;
      if (!unseen && ot == OBJECT_TYPE_CREATURE) unseen = !GetObjectSeen(o, me);
      for (f = 0; f < nf && !unseen && ot != OBJECT_TYPE_CREATURE; f++) {
        object fg = GetLocalObject(me, "n5d_fog" + IntToString(f));
        if (GetIsObjectValid(fg) && GetDistanceBetween(fg, o) <= GetLocalFloat(me, "n5d_fr" + IntToString(f))) unseen = TRUE;
      }
      if (unseen) { hidden++; what = ""; }
    }
)nss";

const std::string& DetectScan(int row) {
  static std::unordered_map<int, std::string> built;
  const auto at = built.find(row);
  if (at != built.end()) return at->second;
  std::string code = kDetectScan;
  std::string classify, filter, glow;
  if (row == rules::kDetectEvilAndGood) {
    classify = EvilAndGoodScript();
    filter = "OBJECT_TYPE_CREATURE";
    glow = "VFX_DUR_GLOW_LIGHT_YELLOW";
  } else if (row == rules::kDetectMagic) {
    classify = kMagicSense;
    Replace(&classify, "$SCHOOL", SchoolScript());
    filter = "OBJECT_TYPE_CREATURE | OBJECT_TYPE_ITEM | OBJECT_TYPE_PLACEABLE | OBJECT_TYPE_AREA_OF_EFFECT";
    // 0.99.46 (owner, 2026-11-16: "Can we use a vfx effect for the detect magic glow?"): the owner's VFX Bench
    // effect - visualeffects 2092 NWN5E_DETECT_MAGIC (data/rows/visualeffects.detect.txt, the animation session's;
    // 2100 is the looks' orb row - anim 1b21980).
    glow = "2092";
  } else {
    classify = kPoisonSense;
    Replace(&classify, "$POISON", PoisonScript());
    filter = "OBJECT_TYPE_CREATURE | OBJECT_TYPE_ITEM";
    glow = "VFX_DUR_GLOW_GREEN";
  }
  Replace(&code, "$CLASSIFY", classify);
  if (rules::DetectNeedsSight(row)) {
    std::string radii;
    int widest = 0;
    for (int slot = 1; slot <= 9; ++slot) {
      const int feet = rules::FogCloudRadiusFeet(slot);
      widest = std::max(widest, feet);
      radii += " if (sz == " + std::to_string(slot) + ") rz = " + Float(rules::FeetToMetres(feet)) + ";";
    }
    Replace(&code, "$FOGS", kDetectFogs);
    Replace(&code, "$FOGRADII", radii);
    Replace(&code, "$FOGREACH", Float(rules::FeetToMetres(rules::kDetectFeet + widest)));
    Replace(&code, "$SIGHT", kDetectSight);
  } else {
    Replace(&code, "$FOGS", "int nf = 0;");
    Replace(&code, "$SIGHT", "");
  }
  Replace(&code, "$FEET", std::to_string(rules::kDetectFeet));
  Replace(&code, "$ROW", std::to_string(row));
  Replace(&code, "$RADIUS", Float(rules::FeetToMetres(rules::kDetectFeet)));
  Replace(&code, "$FILTER", filter);
  Replace(&code, "$GLOW", glow);
  Replace(&code, "$NAME", SpellNameOf(row));
  return built[row] = code;
}

struct Detecting {
  int row = 0;
  ULONGLONG next = 0;
  std::unordered_set<uint32_t> told_areas;  // Detect Magic's areas already named
};
std::unordered_map<uint32_t, Detecting> g_detecting;  // by caster

void DetectBegin(uint32_t self, int row) {
  char code[640];
  sprintf_s(code,
            "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectIcon(%d), %d), \"%s\"), "
            "OBJECT_SELF, TurnsToSeconds(%d)); DeleteLocalString(OBJECT_SELF, \"n5d_told\"); DeleteLocalString(OBJECT_SELF, "
            "\"n5d_glow\"); DeleteLocalInt(OBJECT_SELF, \"n5d_hid\"); "
            "ApplyEffectToObject(DURATION_TYPE_INSTANT, "
            "EffectVisualEffect(VFX_IMP_MAGICAL_VISION), OBJECT_SELF);",
            UtilityIconRow(row), row, kTagDetectMark, rules::kDetectMinutes);
  Run(code, self, "a Detect's mark");
  g_detecting[self] = Detecting{row, 0, {}};
  TellPlayer(self, PlayerText("spells2024.for_10_minutes_your_concentration", {{"spell_name", std::string(SpellNameOf(row))}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s begins (10 minutes, Concentration)", self, SpellNameOf(row).c_str());
}

// Detect Magic's areas, named from the spell the DLL reads off each.
void NameDetectedAreas(uint32_t self, void* caster, Detecting& detect) {
  const std::string areas = engine::GetLocalString(caster, "n5d_aoes");
  std::string code;
  int named = 0;
  for (size_t at = areas.find('|'); at != std::string::npos && named < 6;) {
    const size_t end = areas.find('|', at + 1);
    if (end == std::string::npos) break;
    const uint32_t id = ParseId(areas.substr(at + 1, end - at - 1));
    at = areas.find('|', end + 1);
    if (!id || !detect.told_areas.insert(id).second) continue;
    const int row = AreaOfEffectSpellId(id);
    ++named;
    if (row < 0) {
      code += " SendMessageToPC(OBJECT_SELF, \"nwn5e: Detect Magic - a magical area.\");";
      continue;
    }
    code += " { int id = " + std::to_string(row) + "; " + SchoolScript() +
            " SendMessageToPC(OBJECT_SELF, \"nwn5e: Detect Magic - an area of \" + GetStringByStrRef(StringToInt("
            "Get2DAString(\"spells\", \"Name\", id))) + (sn == \"\" ? \"\" : \" (\" + sn + \")\") + \".\"); }";
  }
  if (!code.empty()) Run(code, self, "Detect Magic's areas");
}

// Every creature update: a detecting caster looks again every 2 seconds, until its mark is gone (its Concentration
// ended: the sweep took the glows too).
void DetectUpdate(void* creature) {
  if (g_detecting.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto it = g_detecting.find(id);
  if (it == g_detecting.end()) return;
  Detecting& detect = it->second;
  const ULONGLONG now = GetTickCount64();
  if (now < detect.next) return;
  detect.next = now + kDetectEveryMs;
  if (engine::IsDeadOrDying(creature) || !HasSpellEffect(creature, detect.row, -1, id)) {
    if (LogSlot()) Log("spells2024: 0x%08X %s ends", id, SpellNameOf(detect.row).c_str());
    g_detecting.erase(it);
    return;
  }
  if (rules::DetectNeedsSight(detect.row)) engine::SetLocalInt(creature, "n5d_blind", InFogCloud(creature) ? 1 : 0);
  Run(DetectScan(detect.row), id, "a Detect's look");
  const std::string changed = engine::GetLocalString(creature, "n5d_chg");
  if (!changed.empty() && LogSlot()) {
    Log("spells2024: 0x%08X %s's look - glows%s", id, SpellNameOf(detect.row).c_str(), changed.c_str());
  }
  if (detect.row == rules::kDetectMagic) NameDetectedAreas(id, creature, detect);
}

void DetectsGameStarted() { g_detecting.clear(); }

// Identify: an item touched - identified, its charges told, a spell's temporary property or making named - or a
// creature's spells named.
const char* const kIdentifyScript = R"nss(
object t = GetSpellTargetObject();
int k = GetObjectType(t);
if (k == OBJECT_TYPE_ITEM) {
  SetIdentified(t, TRUE);
  string s = "nwn5e: Identify - " + GetName(t) + ": its properties are on its examine window";
  int c = GetItemCharges(t);
  if (c > 0) s += "; " + IntToString(c) + " charges";
  int temp = FALSE;
  itemproperty ip = GetFirstItemProperty(t); int iw;  // 0.99.45: past a Detect's glow (kDetectScan's note)
  for (iw = 0; iw < 8; iw++) { if (GetIsItemPropertyValid(ip) && GetItemPropertyDurationType(ip) == DURATION_TYPE_TEMPORARY) temp = TRUE; ip = GetNextItemProperty(t); }
  if (temp) s += "; a spell is affecting it (a temporary property)";
  if (GetLocalInt(t, "n5_goodberry_until") > 0) s += "; made by the Goodberry spell";
  SendMessageToPC(OBJECT_SELF, s + ".");
  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGICAL_VISION), OBJECT_SELF);
} else if (k == OBJECT_TYPE_CREATURE) {
  string list = ""; string ids = "";
  effect e = GetFirstEffect(t);
  while (GetIsEffectValid(e)) {
    int id = GetEffectSpellId(e);
    // 0.99.43 (L4-4: "affected by Detect Magic, Bless" - a Detect's glow isn't a spell on the creature)
    if (id >= 0 && GetEffectTag(e) != "n5s_detglow" && FindSubString(ids, "|" + IntToString(id) + "|") < 0) {
      ids += "|" + IntToString(id) + "|";
      list += (list == "" ? "" : ", ") + GetStringByStrRef(StringToInt(Get2DAString("spells", "Name", id)));
    }
    e = GetNextEffect(t);
  }
  SendMessageToPC(OBJECT_SELF, "nwn5e: Identify - " + GetName(t) + (list == "" ? ": no spells are affecting it." : ": affected by " + list + "."));
  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGICAL_VISION), t);
} else {
  SendMessageToPC(OBJECT_SELF, "nwn5e: Identify - touch an item (your inventory's too) or a creature.");
}
)nss";

// Alarm's ward entered: a creature outside the caster's party sets it off.
const char* const kAlarmTrip = R"nss(
object w = OBJECT_SELF; object c = GetAreaOfEffectCreator(w); object e = StringToObject("$ID");
if (!GetIsObjectValid(e) || e == c || GetFactionEqual(e, c) || GetMaster(e) == c || GetFactionEqual(GetMaster(e), c)) return;
if ($AUDIBLE) {
$BELL  object pc = GetFirstPC();
  while (GetIsObjectValid(pc)) {
    if (GetArea(pc) == GetArea(w) && GetDistanceBetweenLocations(GetLocation(pc), GetLocation(w)) <= $HEARD) SendMessageToPC(pc, "nwn5e: Alarm - a handbell rings at the ward.");
    pc = GetNextPC();
  }
} else {
  SendMessageToPC(c, "nwn5e: Alarm - a mental ping: a creature has entered your ward.");
  if (GetIsResting(c)) { AssignCommand(c, ClearAllActions()); SendMessageToPC(c, "nwn5e: Alarm - the ping wakes you."); }
}
)nss";

// Alarm's handbell (0.99.43, L4-5: no sound - PlaySound is an action, and the intruder's AI held its queue): an
// invisible object at the ward rings the game's as_cv_bell1 (2.5 s) for the rule's 10 seconds, then goes.
std::string AlarmBellScript() {
  constexpr float kRing = 2.5f;
  std::string code = "  object bell = CreateObject(OBJECT_TYPE_PLACEABLE, \"plc_invisobj\", GetLocation(w));\n";
  for (float at = 0.0f; at < static_cast<float>(rules::kAlarmBellSeconds); at += kRing) {
    code += "  AssignCommand(bell, DelayCommand(" + Float(at) + ", PlaySound(\"as_cv_bell1\")));\n";
  }
  return code + "  DestroyObject(bell, " + Float(rules::kAlarmBellSeconds + 1.0f) + ");\n";
}

void AlarmTrip(uint32_t area, bool audible) {
  for (uint32_t id : AreaCreatures(area, kEntering)) {
    std::string code = kAlarmTrip;
    char hex[16];
    sprintf_s(hex, "%x", id);
    Replace(&code, "$ID", hex);
    Replace(&code, "$AUDIBLE", audible ? "TRUE" : "FALSE");
    Replace(&code, "$BELL", AlarmBellScript());
    Replace(&code, "$HEARD", Float(rules::FeetToMetres(rules::kAlarmAudibleFeet)));
    Run(code, area, "Alarm's ward");
    if (LogSlot()) Log("spells2024: 0x%08X entered Alarm's ward 0x%08X (%s)", id, area, audible ? "audible" : "mental");
  }
}
void AlarmEnterAudible(uint32_t area) { AlarmTrip(area, true); }
void AlarmEnterMental(uint32_t area) { AlarmTrip(area, false); }

void AlarmSpell(uint32_t self, const rules::Spell2024& spell, int row) {
  const rules::AlarmMode mode = rules::AlarmModeOf(row);
  if (mode == rules::AlarmMode::kNone) {
    TellPlayer(self, PlayerText("spells2024.alarm_pick_audible_or_mental"));
    return;
  }
  const bool audible = mode == rules::AlarmMode::kAudible;
  char code[640];
  sprintf_s(code,
            "object t = GetSpellTargetObject(); location l = GetSpellTargetLocation(); if (GetIsObjectValid(t) && "
            "GetObjectType(t) != OBJECT_TYPE_CREATURE) l = GetLocation(t); ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, "
            "EffectAreaOfEffect(%d, \"%s\", \"\", \"\"), l, %d.0); ApplyEffectAtLocation("
            "DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGIC_PROTECTION), l);",
            kAlarmArea, audible ? "n5_s_alarmaud" : "n5_s_alarmmen", rules::SpellSeconds(rules::kAlarmHours * 60));
  Run(code, self, spell.script);
  TellPlayer(self, PlayerText(audible ? "spells2024.alarm_set_audible" : "spells2024.alarm_set_mental"));
  if (LogSlot()) Log("spells2024: 0x%08X Alarm (%s) set", self, audible ? "audible" : "mental");
}

// Create or Destroy Water: Destroy - the fog areas centred in the Cube end; Create - rain there puts out exposed
// flames (Alchemist's Fire's burning on creatures, lit torches and braziers).
const char* const kDestroyFog = R"nss(
location l = GetSpellTargetLocation(); int n = 0;
object a = GetFirstObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_AREA_OF_EFFECT);
while (GetIsObjectValid(a)) {
  if (FindSubString(GetStringUpperCase(GetTag(a)), "FOG") >= 0) { DestroyObject(a); n++; }
  a = GetNextObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_AREA_OF_EFFECT);
}
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_DISPEL), l);
SendMessageToPC(OBJECT_SELF, n > 0 ? "nwn5e: Destroy Water - the fog in the $FEET-foot Cube is gone (" + IntToString(n) + (n == 1 ? " area)." : " areas).") : "nwn5e: Destroy Water - no fog there.");
)nss";

const char* const kRain = R"nss(
location l = GetSpellTargetLocation(); int doused = 0; int lights = 0;
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WATER), l);
object o = GetFirstObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);
while (GetIsObjectValid(o)) {
  if (GetObjectType(o) == OBJECT_TYPE_CREATURE) {
    int was = FALSE;
    effect e = GetFirstEffect(o);
    while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "n5g_burning") { RemoveEffect(o, e); was = TRUE; } e = GetNextEffect(o); }
    if (was) { SetLocalInt(o, "n5g_doused", 1); doused++; }
  } else if (GetPlaceableIllumination(o)) {
    SetPlaceableIllumination(o, FALSE); AssignCommand(o, PlayAnimation(ANIMATION_PLACEABLE_DEACTIVATE)); lights++;
  }
  o = GetNextObjectInShape(SHAPE_CUBE, $HALF, l, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);
}
if (lights > 0) RecomputeStaticLighting(GetArea(OBJECT_SELF));
string s = "nwn5e: Create Water - rain falls in a $FEET-foot Cube";
if (doused + lights == 0) s += "; no exposed flames there."; else s += ": " + IntToString(doused) + " burning creature(s) and " + IntToString(lights) + " light(s) put out.";
SendMessageToPC(OBJECT_SELF, s);
)nss";

void WaterSpell(uint32_t self, const rules::Spell2024& spell, int row, int slot) {
  const rules::WaterMode mode = rules::WaterModeOf(row);
  if (mode == rules::WaterMode::kNone) {
    TellPlayer(self, PlayerText("spells2024.create_or_destroy_water_pick"));
    return;
  }
  const int feet = rules::WaterCubeFeet(slot);
  std::string code = mode == rules::WaterMode::kDestroy ? kDestroyFog : kRain;
  Replace(&code, "$HALF", Float(rules::FeetToMetres(feet) / 2.0f));
  Replace(&code, "$FEET", std::to_string(feet));
  Run(code, self, spell.script);
  if (LogSlot()) Log("spells2024: 0x%08X %s, a %d-foot Cube", self, mode == rules::WaterMode::kDestroy ? "Destroy Water" : "Create Water", feet);
}

// Goodberry: ten berries in the caster's pack (one stack - a potion's, so eating one is a Bonus Action), gone after 24
// hours; each the "Goodberry" power (spell 1197, below).
const char* const kGoodberries = R"nss(
int now = (GetCalendarYear() * 336 + (GetCalendarMonth() - 1) * 28 + GetCalendarDay() - 1) * 24 + GetTimeHour();
object b = CreateItemOnObject("nw_it_mpotion001", OBJECT_SELF, 1, "n5_goodberry_" + IntToString(now));
if (!GetIsObjectValid(b)) { SendMessageToPC(OBJECT_SELF, "nwn5e: Goodberry - no room for the berries."); return; }
itemproperty ip = GetFirstItemProperty(b);
while (GetIsItemPropertyValid(ip)) { RemoveItemProperty(b, ip); ip = GetNextItemProperty(b); }
AddItemProperty(DURATION_TYPE_PERMANENT, ItemPropertyCastSpell($POWER, IP_CONST_CASTSPELL_NUMUSES_SINGLE_USE), b);
SetName(b, "Goodberry"); SetIdentified(b, TRUE); SetItemStackSize(b, $COUNT);
SetDescription(b, "A berry infused with magic by the Goodberry spell. Eating it (a Bonus Action) restores 1 Hit Point. Uneaten, it disappears when the spell ends, 24 hours after it.", TRUE);
SetLocalInt(b, "n5_goodberry_until", GetLocalInt(OBJECT_SELF, "n5_spell_clock") + $SECONDS);
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEALING_S), OBJECT_SELF);
SendMessageToPC(OBJECT_SELF, "nwn5e: Goodberry - ten berries in your pack; eat one (a Bonus Action) for 1 Hit Point. Uneaten, they disappear in 24 hours.");
)nss";

const char* const kGoodberryEat = R"nss(
object b = GetSpellCastItem();
int now = GetLocalInt(OBJECT_SELF, "n5_spell_clock");
int until = GetLocalInt(b, "n5_goodberry_until");
if (until > 0 && now >= until) { SendMessageToPC(OBJECT_SELF, "nwn5e: the Goodberry is gone - the spell ended (24 hours)."); DestroyObject(b); return; }
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectHeal($HP), EffectVisualEffect(VFX_IMP_HEALING_S)), OBJECT_SELF);
SendMessageToPC(OBJECT_SELF, "nwn5e: Goodberry - $HP Hit Point.");
)nss";

void GoodberrySpell(uint32_t self, const rules::Spell2024& spell) {
  std::string code = kGoodberries;
  Replace(&code, "$POWER", std::to_string(kGoodberryPower));
  Replace(&code, "$COUNT", std::to_string(rules::kGoodberryCount));
  Replace(&code, "$HOURS", std::to_string(rules::kGoodberryHours));
  Replace(&code, "$SECONDS", std::to_string(rules::SpellSeconds(rules::kGoodberryHours * 60)));
  Run(code, self, spell.script);
  if (LogSlot()) Log("spells2024: 0x%08X Goodberry - %d berries", self, rules::kGoodberryCount);
}

void GoodberryEaten(uint32_t self, const rules::Spell2024& spell) {
  std::string code = kGoodberryEat;
  Replace(&code, "$HP", std::to_string(rules::kGoodberryHitPoints));
  Run(code, self, spell.script);
  if (LogSlot()) Log("spells2024: 0x%08X ate a Goodberry", self);
}

// Q2 A and Unseen Servant (Q4): no NWN use - a line, and the spell's effect on the caster for its duration (a module
// can check GetHasSpellEffect(row)).
void FlagSpell(uint32_t self, const rules::Spell2024& spell) {
  const int minutes = rules::UtilityEffectMinutes(spell.row);
  if (minutes > 0) {
    char code[480];
    sprintf_s(code,
              "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectIcon(%d), %d), \"%s\"), "
              "OBJECT_SELF, %s); ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGICAL_VISION), "
              "OBJECT_SELF);",
              UtilityIconRow(spell.row), spell.row, kTagUtility, DurationScript(minutes).c_str());
    Run(code, self, spell.script);
  }
  const char* key = "spells2024.flag_purify";
  switch (spell.row) {
    case rules::kComprehendLanguages: key = "spells2024.flag_comprehend"; break;
    case rules::kSpeakWithAnimals: key = "spells2024.flag_speak_animals"; break;
    case rules::kUnseenServant: key = "spells2024.flag_unseen_servant"; break;
    case rules::kIllusoryScript: key = "spells2024.flag_illusory_script"; break;
    default: break;
  }
  TellPlayer(self, PlayerText(key, {{"name", spell.name}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s (%d minutes on it)", self, spell.name, minutes);
}

// Build 4b: Disguise Self (Q4 A, real) - another species' model for an hour (its menu). The caster's own look is
// kept on it (n5s_disg_app, n5s_disg_head, its colours) and given back when the spell's mark is gone (its hour, a
// dispel ...). 0.99.50 (owner, 2026-11-16: "disguice self should give a random appearance of the selected species,
// random clothing as well, probably can be picked from the options available at character creation"; "go with A"):
// a random head of the species (one of its models the game has: character creation's), skin and hair among its
// people's (rules::DisguiseSkinColours / HairColours), tattoos any; the worn outfit re-skinned - random pieces whose
// models the species has (the torso to one of its armour value: the AC stays) and colours - its own look kept on it
// and given back when the spell ends (worn or not).
constexpr const char* kTagDisguise = "n5s_disguise";
std::unordered_map<uint32_t, ULONGLONG> g_disguised;          // a disguised creature -> its next look
std::unordered_map<uint32_t, ULONGLONG> g_disguise_pc_look;   // a player character -> its next look for one kept in a save

const char* const kDisguiseRead = R"nss(
object me = OBJECT_SELF; int poly = FALSE;
effect e = GetFirstEffect(me);
while (GetIsEffectValid(e)) { if (GetEffectType(e) == EFFECT_TYPE_POLYMORPH) poly = TRUE; e = GetNextEffect(me); }
SetLocalInt(me, "n5s_disg_poly", poly);
if (!poly && !GetLocalInt(me, "n5s_disg")) {
  SetLocalInt(me, "n5s_disg_app", GetAppearanceType(me));
  SetLocalInt(me, "n5s_disg_head", GetCreatureBodyPart(CREATURE_PART_HEAD, me));
  SetLocalInt(me, "n5s_disg_skin", GetColor(me, COLOR_CHANNEL_SKIN));
  SetLocalInt(me, "n5s_disg_hair", GetColor(me, COLOR_CHANNEL_HAIR));
  SetLocalInt(me, "n5s_disg_tat1", GetColor(me, COLOR_CHANNEL_TATTOO_1));
  SetLocalInt(me, "n5s_disg_tat2", GetColor(me, COLOR_CHANNEL_TATTOO_2));
}
SetLocalInt(me, "n5s_disg_female", GetGender(me) == GENDER_FEMALE);
SetLocalInt(me, "n5s_disg_pheno", GetPhenoType(me));
)nss";

const char* const kDisguiseOn = R"nss(
object me = OBJECT_SELF;
effect e = GetFirstEffect(me);
while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "$TAG") RemoveEffect(me, e); e = GetNextEffect(me); }
SetLocalInt(me, "n5s_disg", 1);
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectIcon($ICON), $ROW), "$TAG"), me, $DISGSECONDS);
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_POLYMORPH), me);
SetCreatureAppearanceType(me, $APP);
int heads = 0; int head = 1; int n;
for (n = 1; n < 256; n++) {
  string num = IntToString(n);
  while (GetStringLength(num) < 3) num = "0" + num;
  if (ResManGetAliasFor("$PREhead" + num, 2002) != "") { heads++; if (Random(heads) == 0) head = n; }
}
SetCreatureBodyPart(CREATURE_PART_HEAD, head, me);
SetLocalInt(me, "n5s_disg_newhead", head); SetLocalInt(me, "n5s_disg_heads", heads);
SetColor(me, COLOR_CHANNEL_SKIN, $SKIN);
SetColor(me, COLOR_CHANNEL_HAIR, $HAIR);
SetColor(me, COLOR_CHANNEL_TATTOO_1, Random(176));
SetColor(me, COLOR_CHANNEL_TATTOO_2, Random(176));
)nss";

const char* const kDisguiseOff = R"nss(
object me = OBJECT_SELF;
if (GetLocalInt(me, "n5s_disg")) {
  SetCreatureAppearanceType(me, GetLocalInt(me, "n5s_disg_app"));
  SetCreatureBodyPart(CREATURE_PART_HEAD, GetLocalInt(me, "n5s_disg_head"), me);
  SetColor(me, COLOR_CHANNEL_SKIN, GetLocalInt(me, "n5s_disg_skin"));
  SetColor(me, COLOR_CHANNEL_HAIR, GetLocalInt(me, "n5s_disg_hair"));
  SetColor(me, COLOR_CHANNEL_TATTOO_1, GetLocalInt(me, "n5s_disg_tat1"));
  SetColor(me, COLOR_CHANNEL_TATTOO_2, GetLocalInt(me, "n5s_disg_tat2"));
  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_POLYMORPH), me);
}
DeleteLocalInt(me, "n5s_disg"); DeleteLocalInt(me, "n5s_disg_app"); DeleteLocalInt(me, "n5s_disg_head");
)nss";

// The outfit (RunWholeScript: its helpers): Mod - an item's appearance changed (NWN's CopyItemAndModify: a copy, the
// old one gone - its id kept in n5s_disg_old, for TakeOutOldCopies; 0.99.52); Pick - a random piece of a parts table whose model the species has (the torso, its armour value) -
// each fitting row as likely (0.99.51: clothing has 11 torsos of 201 rows, too few for random tries).
const char* const kOutfitHelpers = R"nss(
object Mod(object o, int t, int i, int v) {
  if (GetItemAppearance(o, t, i) == v) return o;
  object n = CopyItemAndModify(o, t, i, v, TRUE);
  if (!GetIsObjectValid(n)) return o;
  SetLocalString(OBJECT_SELF, "n5s_disg_old", GetLocalString(OBJECT_SELF, "n5s_disg_old") + ObjectToString(o) + " ");
  DestroyObject(o);
  return n;
}
int Pick(string table, string model, string pre, float ac, int torso) {
  int rows = Get2DARowCount(table);
  int found = 0; int pick = -1; int r;
  for (r = 0; r < rows; r++) {
    string a = Get2DAString(table, "ACBONUS", r);
    if (a != "" && (!torso || StringToFloat(a) == ac)) {
      string num = IntToString(r);
      while (GetStringLength(num) < 3) num = "0" + num;
      if (ResManGetAliasFor(pre + model + num, 2002) != "") { found++; if (Random(found) == 0) pick = r; }
    }
  }
  return pick;
}
)nss";

const char* const kOutfitOn = R"nss(
void main() {
  object me = OBJECT_SELF;
  object o = GetItemInSlot(INVENTORY_SLOT_CHEST, me);
  SetLocalInt(me, "n5s_disg_outfit", 0);
  if (!GetIsObjectValid(o)) return;
  string pre = "$PRE";
  int i;
  if (!GetLocalInt(o, "n5s_disg_item")) {
    for (i = 0; i < 19; i++) SetLocalInt(o, "n5s_m" + IntToString(i), GetItemAppearance(o, ITEM_APPR_TYPE_ARMOR_MODEL, i));
    for (i = 0; i < 6; i++) SetLocalInt(o, "n5s_c" + IntToString(i), GetItemAppearance(o, ITEM_APPR_TYPE_ARMOR_COLOR, i));
    SetLocalInt(o, "n5s_disg_item", 1);
  }
  float ac = StringToFloat(Get2DAString("parts_chest", "ACBONUS", GetItemAppearance(o, ITEM_APPR_TYPE_ARMOR_MODEL, 7)));
  int p;
$PIECES  for (i = 0; i < 6; i++) o = Mod(o, ITEM_APPR_TYPE_ARMOR_COLOR, i, Random(176));
  AssignCommand(me, ActionEquipItem(o, INVENTORY_SLOT_CHEST));
  SetLocalInt(me, "n5s_disg_outfit", 1);
}
)nss";

const char* const kOutfitOff = R"nss(
void main() {
  object me = OBJECT_SELF;
  object o = GetItemInSlot(INVENTORY_SLOT_CHEST, me);
  int worn = GetIsObjectValid(o) && GetLocalInt(o, "n5s_disg_item");
  if (!worn) {
    o = GetFirstItemInInventory(me);
    while (GetIsObjectValid(o) && !GetLocalInt(o, "n5s_disg_item")) o = GetNextItemInInventory(me);
  }
  if (!GetIsObjectValid(o)) return;
  int i;
  for (i = 0; i < 19; i++) o = Mod(o, ITEM_APPR_TYPE_ARMOR_MODEL, i, GetLocalInt(o, "n5s_m" + IntToString(i)));
  for (i = 0; i < 6; i++) o = Mod(o, ITEM_APPR_TYPE_ARMOR_COLOR, i, GetLocalInt(o, "n5s_c" + IntToString(i)));
  DeleteLocalInt(o, "n5s_disg_item");
  if (worn) AssignCommand(me, ActionEquipItem(o, INVENTORY_SLOT_CHEST));
}
)nss";

std::string OutfitOnScript(const std::string& prefix) {
  std::string pieces;
  for (const rules::ArmourPiece& piece : rules::DisguiseArmourPieces()) {
    pieces += std::string("  p = Pick(\"") + piece.table + "\", \"" + piece.model + "\", pre, ac, " +
              (piece.index == rules::kArmourTorso ? "TRUE" : "FALSE") + "); if (p >= 0) { o = Mod(o, "
              "ITEM_APPR_TYPE_ARMOR_MODEL, " + std::to_string(piece.index) + ", p);";
    if (piece.mirror >= 0) pieces += " o = Mod(o, ITEM_APPR_TYPE_ARMOR_MODEL, " + std::to_string(piece.mirror) + ", p);";
    pieces += " }\n";
  }
  std::string code = std::string(kOutfitHelpers) + kOutfitOn;
  Replace(&code, "$PIECES", pieces);
  Replace(&code, "$PRE", prefix);
  return code;
}

// 0.99.52 (owner, 2026-11-16: "check the logs, specifically the many \"lost item\" lines"): each copy Mod replaced
// left its owner through DestroyObject, and the engine's destroy says "Lost Item: <name>" (ee::kCreatureRemoveItem) -
// 22 to 24 lines a cast. Right after the outfit's script (its DestroyObjects wait for the next AI update), each old
// copy is taken out of the inventory without it; the destroy then finds it with no owner and says nothing. Returns
// how many.
int TakeOutOldCopies(void* creature) {
  using RemoveItemFn = int(__fastcall*)(void* creature, void* item, int a, int feedback, int keep, int possessor);
  static const auto remove_item = reinterpret_cast<RemoveItemFn>(EngineExport(ee::kCreatureRemoveItem));
  if (!creature) return 0;
  const std::string olds = engine::GetLocalString(creature, "n5s_disg_old");
  if (olds.empty()) return 0;
  engine::SetLocalString(creature, "n5s_disg_old", "");
  if (!remove_item) {
    static bool told = false;
    if (!told) Log("spells2024: Disguise Self's outfit - CNWSCreature::RemoveItem not found; its copies say Lost Item");
    told = true;
    return 0;
  }
  int taken = 0;
  const char* at = olds.c_str();
  while (*at) {
    char* end = nullptr;
    const unsigned long id = strtoul(at, &end, 16);
    if (end == at) break;
    at = end;
    while (*at == ' ') ++at;
    void* item = ItemById(static_cast<uint32_t>(id));
    if (item && remove_item(creature, item, 0, 0, 0, 1)) ++taken;
  }
  return taken;
}

int RandomOf(const std::vector<int>& values) {
  if (values.empty()) return 0;
  return values[static_cast<size_t>(RollEngineDice(1, static_cast<int>(values.size())) - 1)];
}

const char* SpeciesName(int appearance) {
  switch (appearance) {
    case 0: return "a dwarf";
    case 1: return "an elf";
    case 2: return "a gnome";
    case 3: return "a halfling";
    case 5: return "an orc";
    default: return "a human";
  }
}

void DisguiseSpell(uint32_t self, void* caster, int row) {
  const int appearance = rules::DisguiseAppearanceOf(row);
  if (appearance < 0) {
    TellPlayer(self, PlayerText("spells2024.disguise_self_pick_a_species"));
    return;
  }
  Run(kDisguiseRead, self, "Disguise Self's look");
  const int own = engine::GetLocalInt(caster, "n5s_disg_app");
  if (engine::GetLocalInt(caster, "n5s_disg_poly") || rules::PlayerHeadModel(false, own, 0, 1).empty()) {
    TellPlayer(self, PlayerText("spells2024.disguise_self_your_form_isn"));
    return;
  }
  const bool female = engine::GetLocalInt(caster, "n5s_disg_female") != 0;
  const int phenotype = engine::GetLocalInt(caster, "n5s_disg_pheno");
  const int head = engine::GetLocalInt(caster, "n5s_disg_head");
  const std::string prefix = rules::PlayerModelPrefix(female, appearance, phenotype);
  const int skin = RandomOf(rules::DisguiseSkinColours(appearance));
  const int hair = RandomOf(rules::DisguiseHairColours(appearance));
  std::string code = kDisguiseOn;
  Replace(&code, "$TAG", kTagDisguise);
  Replace(&code, "$ICON", std::to_string(UtilityIconRow(rules::kDisguiseSelf)));
  Replace(&code, "$ROW", std::to_string(rules::kDisguiseSelf));
  Replace(&code, "$HOURS", std::to_string(rules::kDisguiseSelfHours));
  Replace(&code, "$DISGSECONDS", SecondsScript(rules::kDisguiseSelfHours * 60));
  Replace(&code, "$APP", std::to_string(appearance));
  Replace(&code, "$PRE", prefix);
  Replace(&code, "$SKIN", std::to_string(skin));
  Replace(&code, "$HAIR", std::to_string(hair));
  Run(code, self, "Disguise Self");
  RunWholeScript(OutfitOnScript(prefix).c_str(), self);
  const int taken = TakeOutOldCopies(caster);
  g_disguised[self] = 0;
  TellPlayer(self, PlayerText("spells2024.disguise_self_for_an_hour", {{"species_name", std::string(SpeciesName(appearance))}}));
  if (LogSlot()) {
    Log("spells2024: 0x%08X Disguise Self - appearance %d (own %d), head %d of %d (%s; own %d), skin %d, hair %d, "
        "outfit %s (%d old copies taken out quietly)",
        self, appearance, own, engine::GetLocalInt(caster, "n5s_disg_newhead"),
        engine::GetLocalInt(caster, "n5s_disg_heads"), prefix.c_str(), head, skin, hair,
        engine::GetLocalInt(caster, "n5s_disg_outfit") ? "re-skinned" : "none worn", taken);
  }
}

// Every creature update: a disguised creature looks every 2 s; its mark gone, its own look back. A player character
// loaded disguised (the save keeps the look and its locals) is found by its local, looked for every 5 s.
void DisguiseUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const ULONGLONG now = GetTickCount64();
  auto it = g_disguised.find(id);
  if (it == g_disguised.end()) {
    if (!IsPlayerCreature(creature)) return;
    ULONGLONG& next = g_disguise_pc_look[id];
    if (now < next) return;
    next = now + 5000;
    if (engine::GetLocalInt(creature, "n5s_disg") != 1) return;
    it = g_disguised.emplace(id, 0).first;
  }
  if (now < it->second) return;
  it->second = now + kDetectEveryMs;
  if (HasSpellEffect(creature, rules::kDisguiseSelf)) return;
  g_disguised.erase(it);
  Run(kDisguiseOff, id, "Disguise Self's end");
  RunWholeScript((std::string(kOutfitHelpers) + kOutfitOff).c_str(), id);  // 0.99.50: its outfit back
  const int taken = TakeOutOldCopies(creature);
  TellPlayer(id, PlayerText("spells2024.disguise_self_ends_you_look"));
  if (LogSlot()) Log("spells2024: 0x%08X Disguise Self ends - its own look back (%d old copies taken out quietly)", id, taken);
}

// Build 4b: Silent Image (Q4 A, real) - Minor Illusion's image (rules_utility_cantrips.cpp; a Large Creature too)
// for 10 minutes' Concentration: a mark on the caster (effecticons 235, the cast row's) holds it, and the image goes
// when the mark does. Its Move (a Magic action, no slot): the order refused and the Move row cheat-cast at the point
// (the cast takes the caster's action); a creature image walks there, an object is put there.
struct SilentImageCast {
  int row = 0;
  uint32_t image = 0;
  ULONGLONG next = 0;
};
std::unordered_map<uint32_t, SilentImageCast> g_silent;  // by caster
constexpr const char* kTagSilentMark = "n5s_simark";

void SilentImageMoved(uint32_t self, void* caster) {
  const auto it = g_silent.find(self);
  if (it == g_silent.end()) {
    TellPlayer(self, PlayerText("spells2024.silent_image_no_image_of"));
    return;
  }
  Run("vector v = GetPositionFromLocation(GetSpellTargetLocation()); SetLocalString(OBJECT_SELF, \"n5c_xyz\", "
      "FloatToString(v.x, 0, 2) + \" \" + FloatToString(v.y, 0, 2) + \" \" + FloatToString(v.z, 0, 2));",
      self, "Silent Image's Move point");
  float x = 0.0f, y = 0.0f, z = 0.0f;
  if (sscanf_s(engine::GetLocalString(caster, "n5c_xyz").c_str(), "%f %f %f", &x, &y, &z) != 3) return;
  const float dx = x - Field<float>(caster, ee::kObjectPositionX);
  const float dy = y - Field<float>(caster, ee::kObjectPositionY);
  if (!rules::SilentImageMoveAllowed(std::sqrt(dx * dx + dy * dy) / rules::FeetToMetres(1))) {
    TellPlayer(self, PlayerText("spells2024.silent_image_it_moves_to"));
    return;
  }
  const uint32_t moved = MoveSilentImage(self, caster, x, y, z);
  if (!moved) {
    TellPlayer(self, PlayerText("spells2024.silent_image_your_image_is"));
    g_silent.erase(it);
    return;
  }
  it->second.image = moved;
  TellPlayer(self, PlayerText("spells2024.silent_image_the_image_moves"));
  if (LogSlot()) Log("spells2024: 0x%08X Silent Image moved to %.1f, %.1f (image 0x%08X)", self, x, y, moved);
}

void SilentImageSpell(uint32_t self, void* caster, int row) {
  if (row == rules::kSilentImageMove) {
    SilentImageMoved(self, caster);
    return;
  }
  rules::ImageKind kind = rules::SilentImageKindOf(row);
  if (kind == rules::ImageKind::kNone) kind = rules::ImageKind::kHumanoid;  // the master, no image picked
  char code[900];
  sprintf_s(code,
            "object t = GetSpellTargetObject(); location l = GetSpellTargetLocation(); if (GetIsObjectValid(t)) l = "
            "GetLocation(t); SetLocalLocation(OBJECT_SELF, \"n5c_at\", l); SetLocalInt(OBJECT_SELF, \"n5c_dc\", "
            "GetSpellSaveDC()); effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if "
            "(GetEffectTag(e) == \"%s\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); } "
            "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SetEffectSpellId(EffectIcon(%d), %d), \"%s\"), "
            "OBJECT_SELF, TurnsToSeconds(%d));",
            kTagSilentMark, UtilityIconRow(rules::kSilentImage), row, kTagSilentMark, rules::kSilentImageSeconds / 60);
  Run(code, self, "Silent Image's point and mark");
  const uint32_t image = MakeSilentImage(self, caster, kind, "Silent Image");
  g_silent[self] = SilentImageCast{row, image, 0};
  // 0.99.47 (L4-27): its Move - the feat while the image lasts (the spell's menu, the quickbar).
  if (void* stats = StatsOf(caster)) {
    if (!engine::HasFeat(stats, rules::kFeatSilentImageMove)) engine::AddFeat(stats, rules::kFeatSilentImageMove);
  }
  if (LogSlot()) Log("spells2024: 0x%08X Silent Image (row %d) - image 0x%08X", self, row, image);
}

// The Move's order: refused (no slot) - the Move row cheat-cast at the point instead, within 60 feet of the caster.
bool SilentImageMoveOrder(void* creature, uint32_t id, const SpellOrder& order) {
  if (static_cast<int>(order.spell) != rules::kSilentImageMove) return false;
  if (order.class_slot < 0 || order.class_slot > 7) return false;  // its own cheat cast, going through
  const auto it = g_silent.find(id);
  if (it == g_silent.end() || !GameObjectById(it->second.image)) {
    TellPlayer(id, PlayerText("spells2024.silent_image_no_image_of_2"));
    return true;
  }
  float x = order.x, y = order.y, z = order.z;
  if (order.target != ee::kObjectInvalid && order.target != id) {
    if (void* object = GameObjectById(order.target)) {
      x = Field<float>(object, ee::kObjectPositionX);
      y = Field<float>(object, ee::kObjectPositionY);
      z = Field<float>(object, ee::kObjectPositionY + 4);
    }
  }
  const float dx = x - Field<float>(creature, ee::kObjectPositionX);
  const float dy = y - Field<float>(creature, ee::kObjectPositionY);
  const float feet = std::sqrt(dx * dx + dy * dy) / rules::FeetToMetres(1);
  if (!rules::SilentImageMoveAllowed(feet)) {
    TellPlayer(id, PlayerText("spells2024.silent_image_it_moves_to"));
    return true;
  }
  char code[400];
  sprintf_s(code,
            "ActionCastSpellAtLocation(%d, Location(GetArea(OBJECT_SELF), Vector(%.3f, %.3f, %.3f), 0.0), METAMAGIC_NONE, "
            "TRUE, PROJECTILE_PATH_TYPE_DEFAULT, FALSE);",
            rules::kSilentImageMove, x, y, z);
  Run(code, id, "Silent Image's Move");
  if (LogSlot()) Log("spells2024: 0x%08X Silent Image's Move - %.0f ft from the caster, its cast queued", id, feet);
  return true;
}

// Every creature update: a caster's image looks every second - its mark gone (Concentration ended, or its 10
// minutes), the image goes; the image gone (seen through, struck), it's let go.
void SilentImageUpdate(void* creature) {
  if (g_silent.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto it = g_silent.find(id);
  if (it == g_silent.end()) return;
  const ULONGLONG now = GetTickCount64();
  if (now < it->second.next) return;
  it->second.next = now + 1000;
  if (!HasSpellEffect(creature, it->second.row)) {
    EndSilentImage(id);
    g_silent.erase(it);
    if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatSilentImageMove);
    TellPlayer(id, PlayerText("spells2024.silent_image_ends_your_concentration"));
    if (LogSlot()) Log("spells2024: 0x%08X Silent Image ends - its mark gone", id);
    return;
  }
  if (!GameObjectById(it->second.image)) {
    g_silent.erase(it);
    if (void* stats = StatsOf(creature)) engine::RemoveFeat(stats, rules::kFeatSilentImageMove);
    if (LogSlot()) Log("spells2024: 0x%08X Silent Image's image is gone (seen through or struck)", id);
  }
}

bool SilentImageLasts(uint32_t id) {
  const auto it = g_silent.find(id);
  return it != g_silent.end() && GameObjectById(it->second.image) != nullptr;
}

void IllusionsGameStarted() {
  g_disguised.clear();
  g_disguise_pc_look.clear();
  g_silent.clear();
}

void UtilitySpell(uint32_t self, void* caster, const rules::Spell2024& spell, int slot) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);  // a menu's choice: Alarm's kind, Water's way
  switch (spell.row) {
    case rules::kIdentify: Run(kIdentifyScript, self, spell.script); break;
    case rules::kAlarm: AlarmSpell(self, spell, row); break;
    case rules::kCreateOrDestroyWater: WaterSpell(self, spell, row, slot); break;
    case rules::kDetectEvilAndGood:
    case rules::kDetectMagic:
    case rules::kDetectPoisonAndDisease: DetectBegin(self, spell.row); break;
    case rules::kGoodberry: GoodberrySpell(self, spell); break;
    case rules::kGoodberryEat: GoodberryEaten(self, spell); break;
    case rules::kDisguiseSelf: DisguiseSpell(self, caster, row); break;  // build 4b
    case rules::kSilentImage: SilentImageSpell(self, caster, row); break;
    case rules::kFindFamiliar: FindFamiliarSpell(self, caster, row); break;  // build 4c
    case rules::kFloatingDisk: FloatingDiskSpell(self, caster, row); break;  // build 4d
    default: FlagSpell(self, spell); break;
  }
}

// Rituals (Q1 A): the character's switch (!options, on by default) and whether it's in a fight.
bool RitualSwitchOn(void* creature) {
  if (!creature) return false;
  EnsureRecords(creature);
  return engine::GetLocalInt(creature, kVarRitualOff) != 1;
}

bool InFightNow(uint32_t id, void* creature) {
  RunScriptChunk("SetLocalInt(OBJECT_SELF, \"n5_rit_fight\", GetIsInCombat() || GetIsObjectValid(GetNearestCreature("
                 "CREATURE_TYPE_REPUTATION, REPUTATION_TYPE_ENEMY, OBJECT_SELF, 1, CREATURE_TYPE_PERCEPTION, "
                 "PERCEPTION_SEEN)));",
                 id);
  return engine::GetLocalInt(creature, "n5_rit_fight") != 0;
}

// A class's cast of a Ritual spell out of a fight, the switch on: a Ritual - no slot (the order refused), the clock
// 10 minutes on (11 for a 1-minute spell), the spell cast at its level for nothing.
bool RitualOrder(void* creature, uint32_t id, const SpellOrder& order) {
  const int row = static_cast<int>(order.spell);
  const bool class_cast = order.class_slot >= 0 && order.class_slot <= 7;
  if (!rules::IsRitualSpell(row) || !class_cast || !IsPlayerCreature(creature)) return false;
  if (!rules::CastAsRitual(true, RitualSwitchOn(creature), InFightNow(id, creature), true)) return false;
  const int minutes = rules::RitualCastingMinutes(rules::CastingMinutes(row));
  char code[1100];
  sprintf_s(code,
            "int per = FloatToInt(HoursToSeconds(1) * 1000.0); int add = per * %d / 60;"
            " int ms = (GetTimeMinute() * 60 + GetTimeSecond()) * 1000 + GetTimeMillisecond() + add; int h = GetTimeHour();"
            " while (ms >= per) { ms -= per; h++; }"
            " DelayCommand(0.0, SetTime(h, ms / 60000, (ms / 1000) %% 60, ms %% 1000));"
            " object t = StringToObject(\"%x\");"
            " if (GetIsObjectValid(t)) ActionCastSpellAtObject(%d, t, METAMAGIC_NONE, TRUE, 0, PROJECTILE_PATH_TYPE_DEFAULT, TRUE);"
            " else ActionCastSpellAtLocation(%d, Location(GetArea(OBJECT_SELF), Vector(%.3f, %.3f, %.3f), 0.0), METAMAGIC_NONE,"
            " TRUE, PROJECTILE_PATH_TYPE_DEFAULT, TRUE);",
            minutes, order.target, row, row, order.x, order.y, order.z);
  Run(code, id, "a Ritual");
  TellPlayer(id, PlayerText("spells2024.cast_as_a_ritual_minutes", {{"spell_name", std::string(SpellNameOf(row))}, {"minutes", std::string(std::to_string(minutes))}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s cast as a Ritual - %d minutes, no slot", id, SpellNameOf(row).c_str(), minutes);
  return true;
}

// Build 4c: a spell an hour or more to cast (Find Familiar) from a class, not as a Ritual: not in a fight (refused,
// nothing spent); out of one, the clock moves its hour as it's cast.
bool LongCastOrder(void* creature, uint32_t id, const SpellOrder& order) {
  const int row = static_cast<int>(order.spell);
  const int minutes = rules::CastingMinutes(row);
  const bool class_cast = order.class_slot >= 0 && order.class_slot <= 7;
  if (minutes < 60 || !class_cast || !IsPlayerCreature(creature)) return false;
  if (InFightNow(id, creature)) {
    TellPlayer(id, PlayerText("spells2024.takes_an_hour_to_cast", {{"spell_name", std::string(SpellNameOf(row))}}));
    return true;
  }
  char code[600];
  sprintf_s(code,
            "int per = FloatToInt(HoursToSeconds(1) * 1000.0); int add = per * %d / 60;"
            " int ms = (GetTimeMinute() * 60 + GetTimeSecond()) * 1000 + GetTimeMillisecond() + add; int h = GetTimeHour();"
            " while (ms >= per) { ms -= per; h++; }"
            " DelayCommand(0.0, SetTime(h, ms / 60000, (ms / 1000) %% 60, ms %% 1000));",
            minutes);
  Run(code, id, "an hour's casting");
  TellPlayer(id, PlayerText("spells2024.minutes_pass_as_you_cast", {{"spell_name", std::string(SpellNameOf(row))}, {"minutes", std::string(std::to_string(minutes))}}));
  if (LogSlot()) Log("spells2024: 0x%08X %s cast with a slot - %d minutes", id, SpellNameOf(row).c_str(), minutes);
  return false;
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
  if (spell->effect == rules::SpellEffect2024::kCondition) {
    Condition(self, caster, *spell, slot);  // build 2b
    return;
  }
  if (spell->effect == rules::SpellEffect2024::kArea) {
    AreaSpell(self, caster, *spell, slot);  // build 2c
    return;
  }
  if (spell->effect == rules::SpellEffect2024::kUtility) {
    UtilitySpell(self, caster, *spell, slot);  // build 4a
    return;
  }
  if (spell->effect == rules::SpellEffect2024::kReaction) {
    ReactionSpell(self, *spell);  // build 3
    return;
  }
  if (spell->effect == rules::SpellEffect2024::kSmite) {
    ArmFromScript(self, *spell, slot);  // build 3: a cast that ran (an item's, a script's)
    return;
  }
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
    case rules::SpellEffect2024::kCondition: break;
    case rules::SpellEffect2024::kArea: break;
    case rules::SpellEffect2024::kReaction: break;
    case rules::SpellEffect2024::kSmite: break;
    case rules::SpellEffect2024::kUtility: break;
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
    case rules::kCommand: return &SpellScript<rules::kCommand>;
    case rules::kCharmPerson: return &SpellScript<rules::kCharmPerson>;
    case rules::kAnimalFriendship: return &SpellScript<rules::kAnimalFriendship>;
    case rules::kHideousLaughter: return &SpellScript<rules::kHideousLaughter>;
    case rules::kColorSpray: return &SpellScript<rules::kColorSpray>;
    case rules::kSleep: return &SpellScript<rules::kSleep>;
    case rules::kEntangle: return &SpellScript<rules::kEntangle>;
    case rules::kGrease: return &SpellScript<rules::kGrease>;
    case rules::kFaerieFire: return &SpellScript<rules::kFaerieFire>;
    case rules::kFogCloud: return &SpellScript<rules::kFogCloud>;
    case rules::kShield: return &SpellScript<rules::kShield>;
    case rules::kFeatherFall: return &SpellScript<rules::kFeatherFall>;
    case rules::kDivineSmite: return &SpellScript<rules::kDivineSmite>;
    case rules::kSearingSmite: return &SpellScript<rules::kSearingSmite>;
    case rules::kEnsnaringStrike: return &SpellScript<rules::kEnsnaringStrike>;
    case rules::kIdentify: return &SpellScript<rules::kIdentify>;
    case rules::kAlarm: return &SpellScript<rules::kAlarm>;
    case rules::kComprehendLanguages: return &SpellScript<rules::kComprehendLanguages>;
    case rules::kCreateOrDestroyWater: return &SpellScript<rules::kCreateOrDestroyWater>;
    case rules::kDetectEvilAndGood: return &SpellScript<rules::kDetectEvilAndGood>;
    case rules::kDetectMagic: return &SpellScript<rules::kDetectMagic>;
    case rules::kDetectPoisonAndDisease: return &SpellScript<rules::kDetectPoisonAndDisease>;
    case rules::kGoodberry: return &SpellScript<rules::kGoodberry>;
    case rules::kGoodberryEat: return &SpellScript<rules::kGoodberryEat>;
    case rules::kIllusoryScript: return &SpellScript<rules::kIllusoryScript>;
    case rules::kPurifyFoodAndDrink: return &SpellScript<rules::kPurifyFoodAndDrink>;
    case rules::kSpeakWithAnimals: return &SpellScript<rules::kSpeakWithAnimals>;
    case rules::kUnseenServant: return &SpellScript<rules::kUnseenServant>;
    case rules::kDisguiseSelf: return &SpellScript<rules::kDisguiseSelf>;  // build 4b
    case rules::kSilentImage: return &SpellScript<rules::kSilentImage>;
    case rules::kFindFamiliar: return &SpellScript<rules::kFindFamiliar>;  // build 4c
    case rules::kFloatingDisk: return &SpellScript<rules::kFloatingDisk>;  // build 4d
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

// combat_options.cpp (0.99.22): the Ranger's option, its section by Ranger level.
int HuntersMarkRangerLevel(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && Faithful() ? ClassLevel2024(stats, rules::CharacterClass::kRanger) : 0;
}
bool HuntersMarkArmOn(void* creature) { return KeepMoveArmed(creature); }
// combat_options.cpp (build 4a, Q1 A): the Ritual switch - for a 2024 caster.
bool RitualCaster(void* creature) {
  void* stats = creature && Faithful() ? StatsOf(creature) : nullptr;
  if (!stats) return false;
  for (rules::CharacterClass c : {rules::CharacterClass::kBard, rules::CharacterClass::kCleric, rules::CharacterClass::kDruid,
                                  rules::CharacterClass::kPaladin, rules::CharacterClass::kRanger,
                                  rules::CharacterClass::kSorcerer, rules::CharacterClass::kWizard}) {
    if (ClassLevel2024(stats, c) > 0) return true;
  }
  return false;
}
bool RitualSwitch(void* creature) { return RitualSwitchOn(creature); }
void SetRitualSwitch(void* creature, bool on) {
  if (!creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarRitualOff, on ? 0 : 1);
  Log("spells2024: 0x%08X Rituals out of combat %s", Field<uint32_t>(creature, ee::kObjectId), on ? "on" : "off");
}
void SetHuntersMarkArm(void* creature, bool on) {
  if (!creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarHuntersMarkArm, on ? 1 : 0);
  Log("spells2024: 0x%08X Hunter's Mark: keep an early Move armed %s", Field<uint32_t>(creature, ee::kObjectId),
      on ? "on" : "off");
}

// staple_actions.cpp's Rouse (#39 build 2b; owner, 2026-11-12: "A for Q2 could be the same help action, just
// renamed"): `target`, under the Sleep spell, within 5 feet of `rouser`, is shaken awake. 1: woken; 0: no Sleep spell
// on it; -1: too far.
int WakeFromSleepSpell(uint32_t rouser, uint32_t target) {
  if (!Faithful()) return 0;
  void* sleeper = CreatureById(target);
  void* creature = CreatureById(rouser);
  if (!sleeper || !creature || !HasSpellEffect(sleeper, rules::kSleep)) return 0;
  const float gap = engine::EdgeDistance(creature, sleeper);
  if (gap > rules::FeetToMetres(rules::kRouseFeet) + 2.0f * kSlack) {
    Log("spells2024: 0x%08X Rouse on 0x%08X - %.2f m away, not within 5 feet", rouser, target, gap);
    return -1;
  }
  const auto at = g_held.find(HeldKey(target, rules::kSleep));
  const bool asleep = at != g_held.end() && at->second.asleep;
  if (at != g_held.end()) g_held.erase(at);
  std::string code = kUnmark;
  Replace(&code, "$TAG", kTagSleep);
  Run(code, target, "Rouse");
  TellPlayer(target, PlayerText("spells2024.someone_shakes_you_awake_sleep"));
  Log("spells2024: 0x%08X Rouse - shakes 0x%08X out of Sleep (%s, %.2f m)", rouser, target,
      asleep ? "Unconscious" : "Incapacitated", gap);
  return 1;
}

// rules_advantage.cpp (#39 build 2c): Faerie Fire - attack rolls against an outlined creature have Advantage if the
// attacker can see it.
MasteryRollSources FaerieFireRollSources(void* target, bool cant_see) {
  MasteryRollSources sources;
  if (g_outlined.empty() || !target || cant_see || !Faithful()) return sources;
  const auto at = g_outlined.find(Field<uint32_t>(target, ee::kObjectId));
  if (at == g_outlined.end() || !HasSpellEffect(target, at->second.row, -1, at->second.caster)) return sources;
  sources.advantage = 1;
  sources.note = "Faerie Fire";
  return sources;
}

// rules_spell_attack.cpp (#39 build 3): a spell attack's hit that Shield would turn - its Reaction taken (true).
bool ShieldReaction(void* defender, void* attacker, int total, int armor_class, bool natural_twenty) {
  return Faithful() && ShieldTurns(defender, attacker, total, armor_class, natural_twenty, "a spell attack");
}

// combat_options.cpp (#39 build 3): Shield's switch shows for a creature with Shield prepared or known.
bool ShieldKnown(void* creature) {
  void* stats = creature && Faithful() ? StatsOf(creature) : nullptr;
  if (!stats) return false;
  static const auto known = reinterpret_cast<IsKnownFn>(EngineExport(ee::kIsInKnownSpellList));
  if (!known) return false;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  for (size_t i = 0; i < slots.size() && i < 8; ++i) {
    if (rules::CasterKindOfClass(slots[i].nwn_class_id) == rules::CasterKind::kNone) continue;
    if (known(stats, static_cast<uint8_t>(i), static_cast<uint32_t>(rules::kShield))) return true;
  }
  return false;
}

// rules_perception.cpp (#39 build 2c, Q2 A): a creature in a Fog Cloud - Heavily Obscured.
bool InFogCloud(void* creature) {
  if (g_fogged.empty() || !creature) return false;
  const auto at = g_fogged.find(Field<uint32_t>(creature, ee::kObjectId));
  return at != g_fogged.end() && GameObjectById(at->second) != nullptr;
}

// rules_spell_slots.cpp's UnReady: a spell slot spent - this turn's (One Spell with a Spell Slot per Turn).
void NoteSlotSpent(void* creature, int level) {
  if (!creature || level < 1 || t_reaction_spend) return;  // build 3: a Reaction's
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
    TellPlayer(Field<uint32_t>(creature, ee::kObjectId),
               PlayerText("spells2024.bless_bane_save", {{"die", (die >= 0 ? "+" : "") + std::to_string(die)}}));
  }
  return die;
}

// rules_attack.cpp's AC and the sheet's (build 2a): Shield of Faith's +2; Mage Armor's base AC 13 + Dexterity for a
// character wearing no armour (the engine's 10 + Dexterity, raised by what Unarmored Defense doesn't already give).
int SpellArmorClassBonus(void* stats, bool character) {
  void* creature = stats ? CreatureOf(stats) : nullptr;
  if (!Faithful() || !creature) return 0;
  int bonus = HasSpellEffect(creature, rules::kShieldOfFaith, rules::kShieldOfFaithBonus) ? rules::kShieldOfFaithAc : 0;
  if (HasSpellEffect(creature, rules::kShield)) bonus += rules::kShieldAc;  // build 3: Shield's +5
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
// 1d6 Force, twice on a Critical Hit - `delay` seconds from now when the spell's damage waits (#155: a look).
void HuntersMarkSpellHit(uint32_t caster, uint32_t target, bool critical) { HuntersMarkHit(caster, target, critical, 0.0f); }

void HuntersMarkHit(uint32_t caster, uint32_t target, bool critical, float delay) {
  if (g_quarries.empty()) return;
  const auto at = g_quarries.find(caster);
  if (at == g_quarries.end() || at->second.quarry != target) return;
  void* creature = CreatureById(caster);
  if (!creature || !HasSpellEffect(creature, rules::kHuntersMark, rules::kHuntersMarkBonus, caster)) return;
  const int dice = rules::CriticalDice(1, critical);
  const int extra = RollEngineDice(dice, rules::kHuntersMarkDie);
  char code[320];
  sprintf_s(code, "object t = StringToObject(\"%x\"); if (!GetIsObjectValid(t)) return; effect d = EffectDamage(%d, %u); "
            "AssignCommand(t, DelayCommand(%.3f, ApplyEffectToObject(DURATION_TYPE_INSTANT, d, t)));", target, extra,
            rules::kDamageTypeForce, delay);
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
  // Part 2a's end, the level 1 list switch (rules/level_lists.h): spells.if_spells2024.lists1.txt with the rows above;
  // Summon Creature I off too unless the house rule keeps it.
  if (!g_config.summon_creature) ServeConditionalRows("retiresummons");
  {
    int retired = 0;
    for (const rules::ClassListRow& row : rules::Level1ListSwitch()) retired += row.change == rules::ListSwitch::kRetired;
    Log("spells2024: the level 1 lists are SRD 5.2.1's - %d NWN spells off the class lists, %zu moved to their 2024 "
        "level; Summon Creature I %s; %zu Wiz_Sorc spells the Wizard's alone",
        retired, rules::Level1ListSwitch().size() - retired,
        g_config.summon_creature ? "kept (house rule summon_creature)" : "off too", rules::WizardOnlyRows().size());
  }
  RegisterRecords({kVarHuntersMarkArm});  // 0.99.22: the Ranger's option
  RegisterRecords({kVarRitualOff});       // build 4a: the Ritual switch
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
  const bool conditions = AddDamageListener(&ConditionDamageHeard) && AddCreatureUpdateListener(&ConditionUpdate);
  const bool areas = AddScriptHandler("n5_s_entanglea", &EntangleEnter) && AddScriptHandler("n5_s_entangleb", &TerrainExit) &&
                     AddScriptHandler("n5_s_entanglec", &EntangleRound) && AddScriptHandler("n5_s_greasea", &GreaseEnter) &&
                     AddScriptHandler("n5_s_greaseb", &TerrainExit) && AddScriptHandler("n5_s_greasec", &GreaseRound) &&
                     AddScriptHandler("n5_s_fog1a", &FogEnter) && AddScriptHandler("n5_s_fog1b", &FogExit) && AddScriptHandler("n5_s_fog2a", &FogEnter) && AddScriptHandler("n5_s_fog2b", &FogExit) && AddScriptHandler("n5_s_fog3a", &FogEnter) && AddScriptHandler("n5_s_fog3b", &FogExit) && AddScriptHandler("n5_s_fog4a", &FogEnter) && AddScriptHandler("n5_s_fog4b", &FogExit) && AddScriptHandler("n5_s_fog5a", &FogEnter) && AddScriptHandler("n5_s_fog5b", &FogExit) && AddScriptHandler("n5_s_fog6a", &FogEnter) && AddScriptHandler("n5_s_fog6b", &FogExit) && AddScriptHandler("n5_s_fog7a", &FogEnter) && AddScriptHandler("n5_s_fog7b", &FogExit) && AddScriptHandler("n5_s_fog8a", &FogEnter) && AddScriptHandler("n5_s_fog8b", &FogExit) && AddScriptHandler("n5_s_fog9a", &FogEnter) && AddScriptHandler("n5_s_fog9b", &FogExit) && 
                     AddScriptHandler("n5_escape", &EscapeScript) && AddCreatureUpdateListener(&AreaUpdate) &&
                     AddMovementRateAdjuster(&TerrainSpeed);
  const bool reactions = AddAttackRollListenerFirst(&ShieldAfterRoll) && AddResolveDamageListener(&SmiteHit) &&
                         AddCreatureUpdateListener(&SmiteUpdate) && AddRestListener(&SmitesRest) &&
                         AddGameStartListener(&SmitesGameStarted);
  const bool utility = AddScriptHandler("n5_s_alarmaud", &AlarmEnterAudible) &&  // build 4a
                       AddScriptHandler("n5_s_alarmmen", &AlarmEnterMental) &&
                       AddCreatureUpdateListener(&DetectUpdate) && AddGameStartListener(&DetectsGameStarted) &&
                       AddCreatureUpdateListener(&DisguiseUpdate) && AddCreatureUpdateListener(&SilentImageUpdate) &&
                       AddGameStartListener(&IllusionsGameStarted) &&  // build 4b
                       InstallFindFamiliar() && InstallFloatingDisk();  // builds 4c, 4d
  if (reactions) {  // #162 G
    AddHudPool(&SmiteHudPool);
    AddHudClick("smite", &SmitePanelClicked);
  }
  Log("spells2024: SRD 5.2's level 1 spells, builds 1a-4d - %zu spells: damage and healing, Chromatic Orb, Heroism, "
      "the buffs, the conditions on creatures, the areas, Shield and the smites, detection and utility, Disguise "
      "Self and Silent Image, Find Familiar, Floating Disk; Rituals out of combat; %zu Bonus Action spells; one spell with a slot a turn (#39 Part 2a)%s",
      rules::Spells2024().size(), rules::BonusActionSpells().size(),
      scripts && update && guard && buffs && conditions && areas && reactions && utility ? "" : " - SOME hooks missing");
}
