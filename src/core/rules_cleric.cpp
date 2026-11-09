// #42 Cleric Part 1 (owner, 2026-11-09: "A on all"; docs/PLAN.md "Phase 4", the 2024 Cleric), class_features
// faithful. Build 1a, Channel Divinity (Q4 A): NWN's Turn Undead (feat 294, spell 308) is SRD 5.2.1's Channel
// Divinity - at Cleric 2 (cls_feat_cler.if_classfeat2024.txt; a Cleric 1 that has it from NWN can't use it), its
// uses 2 / 3 at 6 / 4 at 18 (rules::ChannelDivinityUses; one back on a Short Rest, all on a Long Rest; a character's
// pool, feat_uses.h), its radial a menu (spells.cleric.txt):
// - Divine Spark (n5_cd_heal / _rad / _nec): another creature within 30 feet regains d8s + Wisdom (Supreme Healing at
//   Cleric 17: the dice's maximum), or makes a Constitution save against Radiant or Necrotic damage, half on a
//   success;
// - Turn Undead (n5_cd_turn - also spell 308's own script, the AI's): the hostile Undead within 30 feet make a Wisdom
//   save; on a failure NWN's "turned" (fleeing, no actions) for a minute, ended early by damage or the Cleric's
//   death; Sear Undead (5) - the Wisdom modifier's d8s of Radiant damage to each that fails, before the turn (it
//   doesn't end it);
// - Preserve Life (n5_cd_pres; Life Domain, Cleric 3): 5 x Cleric level shared among the Bloodied creatures friendly
//   to the Cleric within 30 feet (itself too), the most room first, none past half its maximum.
// The DC is the Cleric's spell save DC: 8 + proficiency bonus + Wisdom. A use the menu can't carry out (no target,
// no one Bloodied, Preserve Life before 3) is given back. Its uses show on the resource panel.
// Build 1b, Divine Order (Q2 A; owner, 2026-11-09: "go for 1b"): its two feats (feat.cleric.txt) in the Cleric's
// own pick at Cleric 1 (rules::BonusPicks2024). Protector: Martial weapons and Heavy armour - feats 45 and 2 answer
// yes; NWN's Heavy armour at Cleric 1 is no longer granted, and answers no for a Cleric without Protector (no
// Fighter or Paladin levels). Thaumaturge: the Wisdom modifier (at least +1) on Arcana and Religion checks
// (rules_skills.cpp) and a cantrip more in the Prepare spells window (prepare_spells.cpp). A Cleric without either
// (one from before) gets Protector - the Heavy armour it had - and `!order` switches the order once.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
#include "feat_uses.h"
#include "feats.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "resource_panel.h"
#include "rest.h"
#include "saves.h"
#include "script_hooks.h"
#include "rules/class_features.h"
#include "rules/cleric.h"
#include "rules/hud_layout.h"
#include "rules/nwn_conversion.h"
#include "rules/skills.h"

namespace {

using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);

constexpr int kFeatTurnUndead = 294;
constexpr int kFeatHeavyArmour = 2;      // NWN's Armor Proficiency (Heavy)
constexpr int kFeatMartialWeapons = 45;  // NWN's Weapon Proficiency (Martial)
constexpr int kNwnClassCleric = 2;       // classes.2da
constexpr const char* kVarOrderSwitched = "nwn5e_order_switched";  // !order used (a record)
constexpr const char* kVarChannelSpent = "nwn5e_cd_spent";  // the pool's spent uses (a record)
constexpr const char* kTurnedTag = "nwn5e_turned";
constexpr float kMetresPerFoot = 0.3048f;
constexpr int kAbilityWisdom = 4;
constexpr int kAbilityCharisma = 5;
constexpr int kLogMax = 400;

enum class Spark { kHeal, kRadiant, kNecrotic };

struct Turned {
  uint32_t by = 0;
  uint64_t until = 0;  // game time
  bool hurt = false;   // damaged: the turn ends at its next update
};

std::unordered_map<uint32_t, Turned> g_turned;  // by the undead; the engine's thread only
std::unordered_set<uint32_t> g_order_checked;   // creatures looked at for a Divine Order
int g_logged = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_logged < kLogMax && ++g_logged; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : 0; }

std::string Hex(uint32_t id) {
  char text[16];
  sprintf_s(text, "%x", id);
  return text;
}

uint32_t ParseId(const std::string& text) { return static_cast<uint32_t>(strtoul(text.c_str(), nullptr, 16)); }

bool IsCharacter(void* stats) {
  const CreatureFacts facts = ReadCreatureFacts(stats);
  return facts.character && facts.plausible;
}

void Tell(void* creature, const std::string& line) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (stats && ReadCreatureFacts(stats).is_player) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

// A creature's Cleric levels in faithful (characters and NPCs alike); 0 otherwise.
int ClericLevelOf(void* stats) {
  if (!stats || !Faithful()) return 0;
  return ClassLevel2024(stats, rules::CharacterClass::kCleric);
}

// The Cleric's spell save DC: 8 + proficiency bonus + Wisdom (a creature without Cleric levels turning undead - an
// NWN Paladin's Turn Undead, until its brief - Charisma).
int ChannelDcOf(void* stats) {
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const int ability = ClericLevelOf(stats) > 0 ? kAbilityWisdom : kAbilityCharisma;
  return 8 + rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating) +
         engine::AbilityModifier(stats, ability);
}

uint8_t Save(void* target, uint32_t by, int dc, uint8_t save) {
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through the hook
  if (!save_roll || !target) return 1;
  RememberSaveDc2024(by, dc);  // already 2024's: not converted
  return save_roll(target, save, static_cast<uint16_t>(dc), 0, by, 1, 0, 0);
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0 && LogSlot()) Log("cleric: 0x%08X %s - its script returned %d", self, what, result);
}

std::string NameOf(uint32_t id) {
  void* creature = CreatureById(id);
  if (!creature) return "it";
  Run("SetLocalString(OBJECT_SELF, \"nwn5e_cd_name\", GetName(OBJECT_SELF));", id, "a name");
  return engine::GetLocalString(creature, "nwn5e_cd_name");
}

// --- The pool --------------------------------------------------------------------------------------------------

// X6: Channel Divinity's uses, for characters with Cleric levels (2+; feat 294 is hidden before).
bool ChannelUsesTotal(void* stats, int feat, int* total) {
  if (feat != kFeatTurnUndead || !stats || !IsCharacter(stats)) return false;
  const int level = ClericLevelOf(stats);
  if (level < 2) return false;
  *total = rules::ChannelDivinityUses(level);
  return true;
}

// A Cleric 1 has no Channel Divinity (NWN's Turn Undead at 1): feat 294 answers no - unless a Paladin's levels give
// it (NWN's, until the Paladin's brief).
bool ChannelFeatFilter(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if (feat != kFeatTurnUndead || !stats || !Faithful()) return false;
  if (ClassLevel2024(stats, rules::CharacterClass::kCleric) != 1) return false;
  if (ClassLevel2024(stats, rules::CharacterClass::kPaladin) > 0) return false;
  *answer = 0;
  return true;
}

std::string ChannelShortRest(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats || !IsCharacter(stats) || ClericLevelOf(stats) < 2) return "";
  const int spent = SpentFeatUses(creature, kVarChannelSpent);
  if (spent <= 0) return "";
  SetSpentFeatUses(creature, kVarChannelSpent, spent - 1);
  return " Channel Divinity: a use back.";
}

// A use the menu couldn't carry out, given back (a character's pool; the engine spent it when the menu was used).
void GiveBack(void* caster) {
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats || !IsCharacter(stats) || ClericLevelOf(stats) < 2) return;
  const int spent = SpentFeatUses(caster, kVarChannelSpent);
  if (spent > 0) SetSpentFeatUses(caster, kVarChannelSpent, spent - 1);
}

bool ChannelHudPool(void* creature, rules::HudPool* pool) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats || !IsCharacter(stats)) return false;
  const int level = ClericLevelOf(stats);
  const int total = rules::ChannelDivinityUses(level);
  if (total <= 0) return false;
  pool->name = "Channel Divinity";
  pool->left = std::max(0, total - SpentFeatUses(creature, kVarChannelSpent));
  pool->total = total;
  pool->tooltip = "Channel Divinity: " + std::to_string(pool->left) + " of " + std::to_string(total) +
                  " left - Divine Spark, Turn Undead" + (level >= 3 ? ", Preserve Life" : "") +
                  " (the Turn Undead button's menu). One back on a Short Rest, all on a Long Rest.";
  return true;
}

// --- Divine Spark ----------------------------------------------------------------------------------------------

void DivineSpark(uint32_t self, Spark spark) {
  void* caster = CreatureById(self);
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats) return;
  const int level = ClericLevelOf(stats);
  if (LogSlot()) Log("cleric: 0x%08X Divine Spark - Cleric %d, pool spent %d", self, level,
                     SpentFeatUses(caster, kVarChannelSpent));
  Run("SetLocalString(OBJECT_SELF, \"nwn5e_cd_target\", ObjectToString(GetSpellTargetObject()));", self,
      "Divine Spark's target");
  const uint32_t target_id = ParseId(engine::GetLocalString(caster, "nwn5e_cd_target"));
  void* target = CreatureById(target_id);
  if (!target || target_id == self) {
    Tell(caster, "Divine Spark - at another creature within 30 feet (the use is given back).");
    GiveBack(caster);
    return;
  }
  const int dice = std::max(1, rules::DivineSparkDice(level));
  const int wisdom = engine::AbilityModifier(stats, kAbilityWisdom);
  const bool supreme = spark == Spark::kHeal && level >= 17;  // Supreme Healing: the dice's maximum
  int roll = 0;
  for (int i = 0; i < dice; ++i) roll += supreme ? 8 : RollEngineDice(1, 8);
  const int total = std::max(0, roll + wisdom);
  const std::string name = NameOf(target_id);
  const std::string t = "object t = StringToObject(\"" + Hex(target_id) + "\"); ";
  char rolled[96];
  sprintf_s(rolled, "%dd8 %d %+d", dice, roll, wisdom);
  if (spark == Spark::kHeal) {
    Run(t + "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(" + std::to_string(total) + "), t); "
            "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEALING_M), t);",
        self, "Divine Spark's healing");
    Tell(caster, "Divine Spark: " + name + " regains " + std::to_string(total) + " Hit Points (" + rolled +
                     (supreme ? ", Supreme Healing" : "") + ").");
    if (LogSlot()) Log("cleric: 0x%08X Divine Spark heals 0x%08X %d (%s)", self, target_id, total, rolled);
    return;
  }
  const bool radiant = spark == Spark::kRadiant;
  const int dc = ChannelDcOf(stats);
  const uint8_t saved = Save(target, self, dc, ee::kSaveFortitude);
  const int damage = saved != 0 ? total / 2 : total;
  Run(t + "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectDamage(" + std::to_string(damage) + ", " +
          (radiant ? "DAMAGE_TYPE_DIVINE" : "DAMAGE_TYPE_NEGATIVE") + "), t); "
          "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(" +
          (radiant ? "VFX_IMP_SUNSTRIKE" : "VFX_IMP_NEGATIVE_ENERGY") + "), t);",
      self, "Divine Spark's damage");
  Tell(caster, std::string("Divine Spark (") + (radiant ? "Radiant" : "Necrotic") + "): " + name +
                   " - Constitution save DC " + std::to_string(dc) + (saved != 0 ? ", saved" : ", failed") + ": " +
                   std::to_string(damage) + (radiant ? " Radiant" : " Necrotic") + " damage (" + rolled +
                   (saved != 0 ? ", halved" : "") + ").");
  if (LogSlot()) Log("cleric: 0x%08X Divine Spark %s at 0x%08X: DC %d, %s, %d damage (%s)", self,
                     radiant ? "Radiant" : "Necrotic", target_id, dc, saved != 0 ? "saved" : "failed", damage, rolled);
}

template <Spark kSpark>
void SparkScript(uint32_t self) {
  DivineSpark(self, kSpark);
}

// --- Turn Undead and Sear Undead -------------------------------------------------------------------------------

const char* const kUndeadNear = R"nss(
string r = "";
object o = GetFirstObjectInShape(SHAPE_SPHERE, $METRES, GetLocation(OBJECT_SELF), FALSE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(o)) {
  if (o != OBJECT_SELF && !GetIsDead(o) && GetRacialType(o) == RACIAL_TYPE_UNDEAD && GetIsEnemy(o)) {
    r += ObjectToString(o) + ";";
  }
  o = GetNextObjectInShape(SHAPE_SPHERE, $METRES, GetLocation(OBJECT_SELF), FALSE, OBJECT_TYPE_CREATURE);
}
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_LOS_HOLY_30), GetLocation(OBJECT_SELF));
SetLocalString(OBJECT_SELF, "nwn5e_cd_undead", r);
)nss";

std::vector<uint32_t> Ids(const std::string& list) {
  std::vector<uint32_t> ids;
  size_t at = 0;
  while (at < list.size()) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    const uint32_t id = ParseId(list.substr(at, end - at));
    if (id) ids.push_back(id);
    at = end + 1;
  }
  return ids;
}

std::string Number(float value) {
  char text[32];
  sprintf_s(text, "%.2f", value);
  return text;
}

void TurnUndead(uint32_t self) {
  void* caster = CreatureById(self);
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats) return;
  const int level = ClericLevelOf(stats);
  std::string code = kUndeadNear;
  const std::string metres = Number(rules::kTurnUndeadFeet * kMetresPerFoot);
  for (size_t at = code.find("$METRES"); at != std::string::npos; at = code.find("$METRES", at)) {
    code.replace(at, 7, metres);
  }
  Run(code, self, "Turn Undead's undead");
  const std::vector<uint32_t> undead = Ids(engine::GetLocalString(caster, "nwn5e_cd_undead"));
  if (LogSlot()) Log("cleric: 0x%08X Turn Undead - Cleric %d, %zu hostile Undead within 30 feet, pool spent %d", self,
                     level, undead.size(), SpentFeatUses(caster, kVarChannelSpent));
  if (undead.empty()) {
    Tell(caster, "Turn Undead - no hostile Undead within 30 feet (the use is given back).");
    GiveBack(caster);
    return;
  }
  const int dc = ChannelDcOf(stats);
  int sear = 0;
  int sear_dice = 0;
  if (level >= 5) {  // Sear Undead: one roll for every Undead that fails
    sear_dice = rules::SearUndeadDice(engine::AbilityModifier(stats, kAbilityWisdom));
    sear = RollEngineDice(sear_dice, 8);
  }
  std::string line;
  const uint64_t until = GameMs() + static_cast<uint64_t>(rules::kTurnUndeadSeconds) * 1000;
  for (const uint32_t id : undead) {
    void* target = CreatureById(id);
    if (!target) continue;
    const uint8_t saved = Save(target, self, dc, ee::kSaveWill);
    const std::string name = NameOf(id);
    if (!line.empty()) line += "; ";
    if (saved != 0) {
      line += name + " saves";
      continue;
    }
    std::string effects = "object t = StringToObject(\"" + Hex(id) + "\"); ";
    if (sear > 0) {  // before the turn: Sear Undead's damage doesn't end it
      effects += "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectDamage(" + std::to_string(sear) +
                 ", DAMAGE_TYPE_DIVINE), t); ";
    }
    effects += "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(EffectLinkEffects(EffectTurned(), "
               "EffectVisualEffect(VFX_DUR_MIND_AFFECTING_FEAR)), \"" + std::string(kTurnedTag) + "\"), t, " +
               std::to_string(rules::kTurnUndeadSeconds) + ".0); "
               "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_HOLY), t);";
    Run(effects, self, "Turn Undead's turn");
    g_turned[id] = Turned{self, until, false};
    line += name + " fails - turned for a minute" + (sear > 0 ? ", " + std::to_string(sear) + " Radiant" : "");
  }
  Tell(caster, "Turn Undead (Wisdom save DC " + std::to_string(dc) + (sear > 0 ? ", Sear Undead " +
               std::to_string(sear_dice) + "d8 = " + std::to_string(sear) : std::string()) + "): " + line + ".");
  if (LogSlot()) Log("cleric: 0x%08X Turn Undead: DC %d, Sear %d - %s", self, dc, sear, line.c_str());
}

// "This effect ends early on the creature if it takes any damage, if you have the Incapacitated condition, or if you
// die" - damage marks it (not inside the engine's damage handling), its next update ends it.
void TurnedHurt(uint32_t /*creator*/, void* target, int damage) {
  if (g_turned.empty() || damage <= 0 || !target) return;
  const auto it = g_turned.find(IdOf(target));
  if (it != g_turned.end()) it->second.hurt = true;
}

void EndTurn(uint32_t id, const Turned& turned, const char* why) {
  Run("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"" +
          std::string(kTurnedTag) + "\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }",
      id, "a turn's end");
  if (void* cleric = CreatureById(turned.by)) Tell(cleric, NameOf(id) + " is no longer turned (" + why + ").");
  if (LogSlot()) Log("cleric: 0x%08X no longer turned by 0x%08X - %s", id, turned.by, why);
}

void TurnedUpdate(void* creature) {
  if (g_turned.empty() || !creature) return;
  const uint32_t id = IdOf(creature);
  const auto it = g_turned.find(id);
  if (it == g_turned.end()) return;
  const Turned turned = it->second;
  if (GameMs() >= turned.until || engine::IsDeadOrDying(creature)) {
    g_turned.erase(it);
    return;
  }
  void* cleric = CreatureById(turned.by);
  if (turned.hurt || !cleric || engine::IsDeadOrDying(cleric)) {
    g_turned.erase(it);
    EndTurn(id, turned, turned.hurt ? "it took damage" : "its Cleric is down");
  }
}

// --- Preserve Life ---------------------------------------------------------------------------------------------

const char* const kWoundedNear = R"nss(
string r = "";
object o = GetFirstObjectInShape(SHAPE_SPHERE, $METRES, GetLocation(OBJECT_SELF), FALSE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(o)) {
  if (!GetIsDead(o) && (o == OBJECT_SELF || GetFactionEqual(o) || GetIsFriend(o))) {
    r += ObjectToString(o) + ":" + IntToString(GetCurrentHitPoints(o)) + ":" + IntToString(GetMaxHitPoints(o)) + ";";
  }
  o = GetNextObjectInShape(SHAPE_SPHERE, $METRES, GetLocation(OBJECT_SELF), FALSE, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "nwn5e_cd_wounded", r);
)nss";

void PreserveLife(uint32_t self) {
  void* caster = CreatureById(self);
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (!stats) return;
  const int level = ClericLevelOf(stats);
  if (level < 3) {
    Tell(caster, "Preserve Life comes with the Life Domain, at Cleric 3 (the use is given back).");
    GiveBack(caster);
    return;
  }
  std::string code = kWoundedNear;
  const std::string metres = Number(rules::kTurnUndeadFeet * kMetresPerFoot);
  for (size_t at = code.find("$METRES"); at != std::string::npos; at = code.find("$METRES", at)) {
    code.replace(at, 7, metres);
  }
  Run(code, self, "Preserve Life's wounded");
  std::vector<uint32_t> ids;
  std::vector<int> rooms;
  const std::string list = engine::GetLocalString(caster, "nwn5e_cd_wounded");
  size_t at = 0;
  while (at < list.size()) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    unsigned int id = 0;
    int hp = 0, max = 0;
    if (sscanf_s(list.substr(at, end - at).c_str(), "%x:%d:%d", &id, &hp, &max) == 3 && rules::IsBloodied(hp, max)) {
      ids.push_back(id);
      rooms.push_back(rules::PreserveLifeRoom(hp, max));
    }
    at = end + 1;
  }
  const int pool = rules::PreserveLifePool(level);
  const std::vector<int> shares = rules::PreserveLifeShares(rooms, pool);
  std::string line;
  int given = 0;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (shares[i] <= 0) continue;
    Run("object t = StringToObject(\"" + Hex(ids[i]) + "\"); ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(" +
            std::to_string(shares[i]) + "), t); ApplyEffectToObject(DURATION_TYPE_INSTANT, "
            "EffectVisualEffect(VFX_IMP_HEALING_L), t);",
        self, "Preserve Life's healing");
    if (!line.empty()) line += ", ";
    line += NameOf(ids[i]) + " " + std::to_string(shares[i]);
    given += shares[i];
  }
  if (LogSlot()) Log("cleric: 0x%08X Preserve Life: %d of %d to %zu Bloodied - %s", self, given, pool, ids.size(),
                     line.c_str());
  if (given == 0) {
    Tell(caster, "Preserve Life - no Bloodied creature friendly to you within 30 feet, none below half its Hit "
                 "Points (the use is given back).");
    GiveBack(caster);
    return;
  }
  Tell(caster, "Preserve Life (" + std::to_string(pool) + " Hit Points to share, up to half each one's maximum): " +
                   line + ".");
}

// --- Divine Order (build 1b) ----------------------------------------------------------------------------------

bool HasOrder(void* stats, int feat) { return stats && engine::HasFeat(stats, feat); }

// Protector: Martial weapons and Heavy armour. A Cleric's Heavy armour is a Protector's only (NWN gave it at 1) -
// unless a Fighter's or a Paladin's levels give it.
bool OrderFeatFilter(void* stats, int feat, uintptr_t /*caller*/, int* answer) {
  if ((feat != kFeatHeavyArmour && feat != kFeatMartialWeapons) || !stats || !Faithful()) return false;
  if (ClassLevel2024(stats, rules::CharacterClass::kCleric) <= 0 || !IsCharacter(stats)) return false;
  if (HasOrder(stats, rules::kFeatDivineOrderProtector)) {
    *answer = 1;
    return true;
  }
  if (feat == kFeatHeavyArmour && ClassLevel2024(stats, rules::CharacterClass::kFighter) == 0 &&
      ClassLevel2024(stats, rules::CharacterClass::kPaladin) == 0) {
    *answer = 0;
    return true;
  }
  return false;
}

// A Cleric from before Divine Order (or one levelled without the pick): Protector, which keeps the Heavy armour NWN
// gave it (Q2 A) - once a creature, when it's in an area.
void OrderUpdate(void* creature) {
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature)) return;
  if (ClassLevel2024(stats, rules::CharacterClass::kCleric) <= 0) return;  // not a Cleric (yet: !test, a level-up)
  if (g_order_checked.count(id)) return;
  g_order_checked.insert(id);
  if (!IsCharacter(stats)) return;
  if (HasOrder(stats, rules::kFeatDivineOrderProtector) || HasOrder(stats, rules::kFeatDivineOrderThaumaturge)) return;
  if (!engine::AddFeat(stats, rules::kFeatDivineOrderProtector)) {
    Log("cleric: 0x%08X has no Divine Order - Protector didn't take", id);
    return;
  }
  Tell(creature, "Divine Order: Protector - Martial weapons and Heavy armour (a Cleric from before the 2024 "
                 "Divine Order). !order thaumaturge switches it once: a cantrip more and Wisdom on Arcana and "
                 "Religion checks.");
  Log("cleric: 0x%08X given Divine Order: Protector (it had none)", id);
}

// !order [protector | thaumaturge]: the Divine Order, and switching it once.
std::vector<std::string> OrderCommand(void* creature, const std::string& args) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats || !Faithful() || ClassLevel2024(stats, rules::CharacterClass::kCleric) <= 0) {
    return {"nwn5e: !order - a Cleric's Divine Order (class_features faithful)."};
  }
  EnsureRecords(creature);
  const bool protector = HasOrder(stats, rules::kFeatDivineOrderProtector);
  const bool thaumaturge = HasOrder(stats, rules::kFeatDivineOrderThaumaturge);
  const std::string now = protector ? "Protector" : thaumaturge ? "Thaumaturge" : "none yet";
  const bool switched = engine::GetLocalInt(creature, kVarOrderSwitched) != 0;
  std::string lower = args;
  for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  const bool want_protector = lower.find("prot") != std::string::npos;
  const bool want_thaumaturge = lower.find("thaum") != std::string::npos;
  if (!want_protector && !want_thaumaturge) {
    return {"nwn5e: Divine Order: " + now + ". Protector: Martial weapons and Heavy armour; Thaumaturge: a cantrip "
            "more and Wisdom (at least +1) on Arcana and Religion checks. " +
            (switched ? std::string("Switched once already.") : std::string("!order protector or !order thaumaturge "
                                                                            "switches it, once."))};
  }
  if ((want_protector && protector) || (want_thaumaturge && thaumaturge)) {
    return {"nwn5e: Divine Order is " + now + " already."};
  }
  if (switched) return {"nwn5e: Divine Order switches once - it stays " + now + "."};
  const int from = protector ? rules::kFeatDivineOrderProtector : rules::kFeatDivineOrderThaumaturge;
  const int to = want_protector ? rules::kFeatDivineOrderProtector : rules::kFeatDivineOrderThaumaturge;
  if (protector || thaumaturge) engine::RemoveFeat(stats, from);
  if (!engine::AddFeat(stats, to)) return {"nwn5e: !order - the feat didn't take; nothing changed."};
  SetRecord(creature, kVarOrderSwitched, 1);
  Log("cleric: 0x%08X Divine Order %s -> %s (!order)", IdOf(creature), now.c_str(),
      want_protector ? "Protector" : "Thaumaturge");
  return {std::string("nwn5e: Divine Order: ") +
          (want_protector ? "Protector - Martial weapons and Heavy armour. A cantrip past your count goes in the "
                            "Prepare spells window."
                          : "Thaumaturge - Wisdom on Arcana and Religion checks; pick your extra cantrip in the "
                            "Prepare spells window (the class abilities radial). Heavy armour goes.")};
}

}  // namespace

// rules_skills.cpp: Thaumaturge's Wisdom modifier (at least +1) on Arcana and Religion checks.
int ThaumaturgeSkillBonus(void* stats, rules::Skill2024 skill) {
  if (!Faithful() || !stats || (skill != rules::Skill2024::kArcana && skill != rules::Skill2024::kReligion)) return 0;
  if (ClassLevel2024(stats, rules::CharacterClass::kCleric) <= 0) return 0;
  if (!HasOrder(stats, rules::kFeatDivineOrderThaumaturge)) return 0;
  return rules::ThaumaturgeCheckBonus(engine::AbilityModifier(stats, kAbilityWisdom));
}

// prepare_spells.cpp: a Thaumaturge's cantrip more on its Cleric list.
int ClericExtraCantrips(void* stats, int class_row) {
  if (!Faithful() || !stats || class_row != kNwnClassCleric) return 0;
  return rules::ThaumaturgeExtraCantrips(HasOrder(stats, rules::kFeatDivineOrderThaumaturge));
}

void InstallClericRules() {
  if (!Faithful()) return;
  ServeConditionalRows("classfeat2024");
  bool scripts = AddScriptHandler("n5_cd_heal", &SparkScript<Spark::kHeal>);
  scripts = AddScriptHandler("n5_cd_rad", &SparkScript<Spark::kRadiant>) && scripts;
  scripts = AddScriptHandler("n5_cd_nec", &SparkScript<Spark::kNecrotic>) && scripts;
  scripts = AddScriptHandler("n5_cd_turn", &TurnUndead) && scripts;
  scripts = AddScriptHandler("n5_cd_pres", &PreserveLife) && scripts;
  const bool uses = AddFeatUsesProvider(kVarChannelSpent, &ChannelUsesTotal);
  const bool level = AddFeatFilter(&ChannelFeatFilter);
  const bool turned = AddDamageListener(&TurnedHurt) && AddCreatureUpdateListener(&TurnedUpdate);
  AddShortRestListener(&ChannelShortRest);
  AddHudPool(&ChannelHudPool);
  RegisterRecords({kVarOrderSwitched});
  const bool order = AddFeatFilter(&OrderFeatFilter) && AddCreatureUpdateListener(&OrderUpdate);
  AddChatCommand("!order", &OrderCommand);
  Log("cleric: build 1b - Divine Order (the Cleric 1 pick: Protector - Martial weapons, Heavy armour - or Thaumaturge "
      "- a cantrip more, Wisdom on Arcana and Religion; !order; Protector for a Cleric without one) (#42)%s",
      order ? "" : " - SOME hooks missing");
  Log("cleric: build 1a - Channel Divinity (Turn Undead's menu: Divine Spark, Turn Undead with Sear Undead, Preserve "
      "Life; 2024 uses, a Short Rest's use back) (#42)%s%s%s%s",
      scripts ? "" : " - SOME scripts not hooked", uses ? "" : " - NWN's uses", level ? "" : " - from Cleric 1",
      turned ? "" : " - damage doesn't end a turn");
}
