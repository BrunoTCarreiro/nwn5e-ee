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
// NWN mode: NWN's spells.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "action_guard.h"
#include "bonus_actions.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "reactions.h"
#include "saves.h"
#include "script_hooks.h"
#include "rules/cantrips.h"
#include "rules/spell_slots.h"
#include "rules/spells2024.h"

// rules_spell_slots.cpp (#39 2a build 1b): the slot a Bonus Action spell's order names - left, spent.
int OrderSlotsLeft(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic);
bool SpendOrderSlot(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic);

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

int SlotLevelOf(void* caster, const rules::Spell2024& spell) {
  if (!caster) return spell.level;
  if (Field<uint32_t>(caster, ee::kCreatureLastSpellCastItem) != ee::kObjectInvalid) return spell.level;  // an item's
  return rules::CastSlotLevel(spell.level, Field<uint8_t>(caster, kCreatureLastSpellLevel));
}

const char* SaveCall(rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kConstitution: return "FortitudeSave";
    case rules::Ability::kWisdom: return "WillSave";
    default: return "ReflexSave";
  }
}

const char* AbilityName(rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kConstitution: return "Constitution";
    case rules::Ability::kWisdom: return "Wisdom";
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

// Healing Word's order becomes its feat's use (instant, a Bonus Action); a slot spell after a Bonus Action one the same
// turn is refused. True: the order refused.
bool OrderGuard(void* creature, const SpellOrder& order) {
  if (!Faithful() || !ActionEconomyOn() || !creature || order.fake || t_bonus_cast || order.class_slot < 0 ||
      order.class_slot > 7) {
    return false;
  }
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (static_cast<int>(order.spell) != rules::kHealingWord) {
    bool bonus = false;
    if (InnateLevel(order.spell) >= 1 && SpentThisTurn(id, &bonus) && bonus) {
      TellPlayer(id, "nwn5e: you cast Healing Word with a spell slot this turn - one spell with a slot a turn; cast it "
                     "next round.");
      if (LogSlot()) Log("spells2024: 0x%08X spell %u refused - a Bonus Action spell spent a slot this turn", id, order.spell);
      return true;
    }
    return false;
  }
  if (!g_use_feat) return false;  // cast as NWN casts it, an action
  void* stats = StatsOf(creature);
  void* target = CreatureById(order.target);
  std::string refused;
  if (SpentThisTurn(id, nullptr)) {
    refused = "you've already spent a spell slot this turn (one spell with a slot a turn).";
  } else if (BonusActionStatus(creature) != 0) {
    refused = "it takes your Bonus Action, already spent this round.";
  } else if (!target) {
    refused = "a creature you can see within 60 feet.";
  } else if (target != creature && engine::EdgeDistance(creature, target) > rules::FeetToMetres(rules::kHealingWordFeet)) {
    refused = "out of range (60 feet).";
  } else if (OrderSlotsLeft(creature, order.spell, order.class_slot, order.domain_level, order.metamagic) <= 0) {
    refused = "no spell slot of that level left.";
  } else if (!stats) {
    refused = "no caster.";
  }
  if (!refused.empty()) {
    TellPlayer(id, "nwn5e: Healing Word - " + refused);
    if (LogSlot()) Log("spells2024: 0x%08X Healing Word refused - %s", id, refused.c_str());
    return true;
  }
  if (!engine::HasFeat(stats, rules::kFeatHealingWord)) engine::AddFeat(stats, rules::kFeatHealingWord);
  g_bonus_casts[id] = BonusCast{order.spell, order.class_slot, order.domain_level, order.metamagic, GameMs()};
  float nowhere[3] = {0.0f, 0.0f, 0.0f};
  t_bonus_cast = true;
  const int used = g_use_feat(creature, static_cast<uint16_t>(rules::kFeatHealingWord), 0, order.target,
                              ee::kObjectInvalid, nowhere);
  t_bonus_cast = false;
  if (!used) g_bonus_casts.erase(id);
  if (LogSlot()) {
    Log("spells2024: 0x%08X Healing Word at 0x%08X - a Bonus Action (feat %d, class slot %d, domain level %d): %s", id,
        order.target, rules::kFeatHealingWord, order.class_slot, order.domain_level, used ? "used" : "refused");
  }
  return true;  // the cast refused: the feat's use runs it
}

void HealingWord(uint32_t self, void* caster, const rules::Spell2024& spell) {
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  int slot = spell.level;
  int modifier = 0;
  if (row == rules::kHealingWordBonus) {
    const auto at = g_bonus_casts.find(self);
    if (at == g_bonus_casts.end() || GameMs() > at->second.at + 10000) {
      if (at != g_bonus_casts.end()) g_bonus_casts.erase(at);
      TellPlayer(self, "nwn5e: Healing Word - cast it from your spells (it spends a spell slot).");
      return;
    }
    const BonusCast cast = at->second;
    g_bonus_casts.erase(at);
    t_bonus_spend = true;
    const bool spent = SpendOrderSlot(caster, cast.spell, cast.class_slot, cast.domain_level, cast.metamagic);
    t_bonus_spend = false;
    if (!spent) {
      TellPlayer(self, "nwn5e: Healing Word - no spell slot of that level left.");
      if (LogSlot()) Log("spells2024: 0x%08X Healing Word - its slot couldn't be spent", self);
      return;
    }
    const int chosen = rules::ChosenSlotLevel(spell.level, cast.domain_level);
    slot = chosen >= spell.level ? chosen : spell.level;
    modifier = CastingModifierOfClassSlot(caster, cast.class_slot);
  } else {
    slot = SlotLevelOf(caster, spell);
    modifier = SpellcastingModifier2024(caster);
  }
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
          "Points", self, slot, row == rules::kHealingWordBonus ? ", a Bonus Action" : "", one.target.c_str(),
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

void Cast(uint32_t self, int row) {
  const rules::Spell2024* spell = rules::Spell2024Of(row);
  void* caster = CreatureById(self);
  if (!spell || !caster) return;
  if (row == rules::kHealingWord) {
    HealingWord(self, caster, *spell);  // its slot: the order's (a Bonus Action) or the cast's
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
  g_use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through its hook
  g_spell_row = reinterpret_cast<SpellRowFn>(EngineExport(ee::kGetSpell));
  g_rules_for_rows = reinterpret_cast<void**>(EngineExport(ee::kRules));
  const bool guard = AddSpellOrderGuard(&OrderGuard) && g_use_feat && g_spell_row && g_rules_for_rows;
  Log("spells2024: SRD 5.2's level 1 spells, builds 1a-1b - %zu spells: damage and healing, Healing Word a Bonus "
      "Action, Chromatic Orb, Heroism; one spell with a slot a turn (#39 Part 2a)%s",
      rules::Spells2024().size(), scripts && update && guard ? "" : " - SOME hooks missing");
}
