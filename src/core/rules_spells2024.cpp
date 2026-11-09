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
#include "log.h"
#include "mastery.h"
#include "reactions.h"
#include "saves.h"
#include "script_hooks.h"
#include "rules/cantrips.h"
#include "rules/spells2024.h"

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

void Cast(uint32_t self, int row) {
  const rules::Spell2024* spell = rules::Spell2024Of(row);
  void* caster = CreatureById(self);
  if (!spell || !caster) return;
  const int slot = SlotLevelOf(caster, *spell);
  switch (spell->effect) {
    case rules::SpellEffect2024::kSpellAttack: Attack(self, caster, *spell, slot); break;
    case rules::SpellEffect2024::kDarts: Darts(self, *spell, slot); break;
    case rules::SpellEffect2024::kHealing: CureWounds(self, caster, *spell, slot); break;
    case rules::SpellEffect2024::kTemporaryHitPoints: FalseLife(self, caster, *spell, slot); break;
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
  const bool update = AddCreatureUpdateListener(&GlowUpdate);
  Log("spells2024: SRD 5.2's level 1 spells, build 1a - %zu damage and healing spells (#39 Part 2a)%s",
      rules::Spells2024().size(), scripts && update ? "" : " - SOME hooks missing");
}
