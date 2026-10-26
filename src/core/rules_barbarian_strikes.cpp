// Phase 3 Barbarian part 3 (docs/PLAN.md "Phase 3 — Barbarian (CM9)", B6,
// BZ1, B10, X8; decided 2026-10-24), class_features faithful:
// - B6 Reckless Attack (2): a radial toggle (nwn5e's feat 1310 -> spell 1010
//   -> n5_reckless, run in C++ by script_hooks.cpp; an icon and a chat line).
//   While on, the creature's Strength attacks have Advantage, and from its
//   first attack on, attack rolls against it have Advantage until a round
//   after its last one ("until the start of your next turn"). Henchmen and
//   other creatures are reckless whenever they rage (no AI for the toggle).
//   Both are advantage sources in rules_advantage.cpp's attack roll.
// - BZ1 Frenzy (Berserker 3 - the subclass comes at 3): raging and reckless,
//   the first creature it hits each round with a Strength attack takes as
//   many d6 as its Rage Damage bonus, added to the weapon's damage (its type),
//   doubled on a critical hit (GetDamageRoll, rules_fighter.cpp).
// - B10 Brutal Strike (9; 13; 17): reckless, the first Strength attack each
//   round that has Advantage (so no Disadvantage) gives it up; on a hit +1d10
//   (2d10 at 17; doubled on a crit) and the picked effects: Forceful Blow
//   (pushed 15 ft - rules_push.cpp, when weapon mastery's Push is on),
//   Hamstring Blow (speed -50% = -15 ft for a round), from 13 Staggering Blow
//   (Disadvantage on its next save - rules_save_roll.cpp; no opportunity
//   attacks for a round - rules_opportunity.cpp) and Sundering Blow (+5 on the
//   next attack roll another creature makes at it within a round -
//   rules_attack.cpp); from 17 two different ones.
// - X8: the effects are picked on Brutal Strike's sub-radial (spells
//   1012-1016 -> n5_bs_*): an effect picked again is removed, Off turns it
//   off; below 17 one effect, from 17 the newer two. Kept on the character
//   (nwn5e_brutal; never chosen = Hamstring Blow, also every other creature's).
// `!reckless` toggles it too, `!brutal [effect | off]` picks or shows.
// A round is the creature's combat round (CombatRoundsEnded) and, as a
// fallback, 6 s of game time.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "log.h"
#include "mastery.h"
#include "script_hooks.h"
#include "rules/class_features.h"
#include "rules/d20.h"
#include "rules/weapons.h"

bool BarbarianRaging(void* creature);  // rules_barbarian.cpp

namespace {

constexpr const char* kVarReckless = "nwn5e_reckless";  // a player's toggle: 1 on
constexpr const char* kVarBrutal = "nwn5e_brutal";      // X8: rules::EncodeBrutalChoice (0: never chosen)
constexpr const char* kRecklessTag = "nwn5e_reckless";
constexpr const char* kBrutalTag = "nwn5e_brutal";
constexpr int kRecklessIconRow = 221;  // effecticons.txt
constexpr int kBrutalIconRow = 222;
constexpr int kBaseItemThrowingAxe = 63;
constexpr int kAbilityStrength = 0;
constexpr int kAbilityDexterity = 1;
constexpr uint64_t kRoundMs = 6000;      // "until the start of your next turn": a round
constexpr uint64_t kPendingMs = 5000;    // a Brutal Strike hit waits this long for its damage roll
constexpr float kSwingDelay = 0.5f;      // effects land with the swing
constexpr float kRoundSeconds = 6.0f;
int g_log_lines = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_config.log_attacks && g_log_lines < 800 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

int BarbarianLevel(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats ? ClassLevel2024(stats, rules::CharacterClass::kBarbarian) : 0;
}

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

// Reckless: a player character's toggle; any other creature while it rages.
bool RecklessOn(void* creature) {
  if (!creature || !Faithful() || !rules::HasRecklessAttack(BarbarianLevel(creature))) return false;
  if (IsPlayer(creature)) return engine::GetLocalInt(creature, kVarReckless) != 0;
  return BarbarianRaging(creature);
}

// A Strength attack: melee or a thrown Throwing Axe; not a finesse weapon used
// with a better Dexterity (2024 Finesse, faithful).
bool StrengthAttack(void* creature, const uint8_t* data) {
  if (!creature || !data) return false;
  const int item = AttackWeaponBaseItem(creature);
  if (Field<int32_t>(data, ee::kAttackRanged) != 0 && item != kBaseItemThrowingAxe) return false;
  void* stats = StatsOf(creature);
  if (stats && item >= 0 && rules::IsFinesseNwnBaseItem(item) && g_config.finesse == rules::RuleVariant::kFaithful &&
      engine::AbilityModifier(stats, kAbilityDexterity) > engine::AbilityModifier(stats, kAbilityStrength)) {
    return false;
  }
  return true;
}

// Once a round: the creature's combat round, or 6 s when rounds aren't counted.
struct RoundUse {
  uint32_t round = 0;
  uint64_t at = 0;
};
bool UsedThisRound(std::unordered_map<uint32_t, RoundUse>* uses, uint32_t id) {
  const uint32_t round = CombatRoundsEnded(id) + 1;
  const uint64_t now = GameMs();
  const auto it = uses->find(id);
  return it != uses->end() && it->second.round == round && now - it->second.at < 2 * kRoundMs;
}
void MarkUsed(std::unordered_map<uint32_t, RoundUse>* uses, uint32_t id) {
  (*uses)[id] = RoundUse{CombatRoundsEnded(id) + 1, GameMs()};
}

struct BrutalPending {
  uint32_t target = 0;
  const uint8_t* data = nullptr;
  int level = 0;
  bool hit = false;
  uint64_t tick = 0;  // real time
  std::vector<rules::BrutalEffect> effects;
};
struct Mark {
  uint32_t by = 0;
  uint64_t until = 0;  // game time
  bool save = false;   // Staggering: its next save still to come
};

// The engine's thread only.
std::unordered_map<uint32_t, uint64_t> g_reckless_until;  // attacks against it have Advantage till then (game ms)
std::unordered_map<uint32_t, RoundUse> g_frenzy;           // Frenzy's round, by attacker
std::unordered_map<uint32_t, RoundUse> g_brutal_round;     // Brutal Strike's round, by attacker
std::unordered_map<uint32_t, BrutalPending> g_brutal;      // by attacker: the attack that gave up its Advantage
std::unordered_map<uint32_t, Mark> g_staggered;            // by target
std::unordered_map<uint32_t, Mark> g_sundered;             // by target

std::string EffectNames(const std::vector<rules::BrutalEffect>& effects) {
  std::string names;
  for (rules::BrutalEffect e : effects) names += (names.empty() ? "" : " + ") + std::string(rules::BrutalEffectName(e));
  return names.empty() ? "off" : names;
}

// The icon on the character (its tag), on or off.
void ShowIcon(uint32_t id, const char* tag, int row, bool on) {
  std::string script = std::string("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if ("
                                   "GetEffectTag(e) == \"") + tag + "\") RemoveEffect(OBJECT_SELF, e); e = "
                                   "GetNextEffect(OBJECT_SELF); }";
  if (on) {
    script += "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectIcon(" +
              std::to_string(row) + ")), \"" + tag + "\"), OBJECT_SELF);";
  }
  RunScriptChunk(script.c_str(), id);
}

// --- The toggle and the picks (n5_reckless, n5_bs_*, !reckless, !brutal) ---------

void SetReckless(void* creature, bool on) {
  const uint32_t id = IdOf(creature);
  engine::SetLocalInt(creature, kVarReckless, on ? 1 : 0);
  ShowIcon(id, kRecklessTag, kRecklessIconRow, on);
  Log("barbarian: 0x%08X Reckless Attack %s", id, on ? "on" : "off");
  Tell(creature, on ? "Reckless Attack on - Advantage on your Strength attacks; attack rolls against you have "
                      "Advantage until a round after your last attack."
                    : "Reckless Attack off.");
}

void ToggleReckless(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  if (!Faithful() || !rules::HasRecklessAttack(BarbarianLevel(creature))) {
    Tell(creature, "Reckless Attack comes at Barbarian 2.");
    return;
  }
  SetReckless(creature, engine::GetLocalInt(creature, kVarReckless) == 0);
}

void PickBrutal(void* creature, rules::BrutalEffect effect) {
  const int level = BarbarianLevel(creature);
  if (!Faithful() || !rules::HasBrutalStrike(level)) {
    Tell(creature, "Brutal Strike comes at Barbarian 9.");
    return;
  }
  rules::BrutalChoice choice = rules::DecodeBrutalChoice(engine::GetLocalInt(creature, kVarBrutal));
  const rules::BrutalPick pick = rules::PickBrutalEffect(&choice, effect, level);
  if (pick == rules::BrutalPick::kLocked) {
    Tell(creature, std::string(rules::BrutalEffectName(effect)) + " comes at Barbarian 13.");
    return;
  }
  engine::SetLocalInt(creature, kVarBrutal, rules::EncodeBrutalChoice(choice));
  const std::vector<rules::BrutalEffect> active = rules::ActiveBrutalEffects(choice, level);
  ShowIcon(IdOf(creature), kBrutalTag, kBrutalIconRow, !active.empty());
  Log("barbarian: 0x%08X Brutal Strike: %s", IdOf(creature), EffectNames(active).c_str());
  std::string line = "Brutal Strike: " + EffectNames(active);
  if (!active.empty() && rules::BrutalStrikeEffectCount(level) > 1 && active.size() < 2) line += " (pick another for two)";
  if (!active.empty() && engine::GetLocalInt(creature, kVarReckless) == 0) line += " - it needs Reckless Attack on";
  Tell(creature, line + ".");
}

template <rules::BrutalEffect kEffect>
void PickBrutalScript(uint32_t self) {
  if (void* creature = CreatureById(self)) PickBrutal(creature, kEffect);
}

std::vector<std::string> RecklessCommand(void* creature, const std::string& args) {
  if (!Faithful() || !rules::HasRecklessAttack(BarbarianLevel(creature))) {
    return {"nwn5e: Reckless Attack comes at Barbarian 2."};
  }
  const bool on = args.find("on") != std::string::npos   ? true
                  : args.find("off") != std::string::npos ? false
                                                           : engine::GetLocalInt(creature, kVarReckless) == 0;
  SetReckless(creature, on);
  return {};
}

std::vector<std::string> BrutalCommand(void* creature, const std::string& args) {
  const int level = BarbarianLevel(creature);
  if (!Faithful() || !rules::HasBrutalStrike(level)) return {"nwn5e: Brutal Strike comes at Barbarian 9."};
  struct Word {
    const char* word;
    rules::BrutalEffect effect;
  };
  static const Word kWords[] = {{"forceful", rules::BrutalEffect::kForceful},
                                {"hamstring", rules::BrutalEffect::kHamstring},
                                {"staggering", rules::BrutalEffect::kStaggering},
                                {"sundering", rules::BrutalEffect::kSundering},
                                {"off", rules::BrutalEffect::kNone}};
  for (const Word& w : kWords) {
    if (args.find(w.word) != std::string::npos) {
      PickBrutal(creature, w.effect);
      return {};
    }
  }
  const rules::BrutalChoice choice = rules::DecodeBrutalChoice(engine::GetLocalInt(creature, kVarBrutal));
  return {"nwn5e: Brutal Strike (Barbarian " + std::to_string(level) + ", " +
          std::to_string(rules::BrutalStrikeDice(level)) + "d10, " +
          std::to_string(rules::BrutalStrikeEffectCount(level)) + " effect" +
          (rules::BrutalStrikeEffectCount(level) > 1 ? "s" : "") +
          "): " + EffectNames(rules::ActiveBrutalEffects(choice, level)) +
          (engine::GetLocalInt(creature, kVarReckless) ? "; Reckless Attack on" : "; Reckless Attack off") +
          ". !brutal forceful | hamstring | staggering | sundering | off (or its radial menu)."};
}

// --- Brutal Strike's effects on a hit ------------------------------------------------

void ApplyBrutalEffects(void* attacker, void* target, const BrutalPending& pending) {
  const uint32_t attacker_id = IdOf(attacker), target_id = IdOf(target);
  std::string what;
  for (rules::BrutalEffect effect : pending.effects) {
    std::string part = rules::BrutalEffectName(effect);
    switch (effect) {
      case rules::BrutalEffect::kForceful: {
        const std::string push = PushOnHit(attacker, target, rules::kForcefulBlowMetres);
        part += push.empty() ? " (no push: weapon mastery's Push is off)" : " (" + push + ")";
        break;
      }
      case rules::BrutalEffect::kHamstring: {
        char effect_text[64];
        sprintf_s(effect_text, "EffectMovementSpeedDecrease(%d)", rules::kHamstringSpeedPercent);
        ApplyEffectAs(attacker_id, target_id, "DURATION_TYPE_TEMPORARY", effect_text, kRoundSeconds, kSwingDelay);
        part += " (speed -15 ft for a round)";
        break;
      }
      case rules::BrutalEffect::kStaggering:
        g_staggered[target_id] = Mark{attacker_id, GameMs() + kRoundMs, true};
        part += " (Disadvantage on its next save, no opportunity attacks for a round)";
        break;
      case rules::BrutalEffect::kSundering:
        g_sundered[target_id] = Mark{attacker_id, GameMs() + kRoundMs, false};
        part += " (+5 on another's next attack at it)";
        break;
      default:
        continue;
    }
    what += (what.empty() ? "" : "; ") + part;
  }
  if (LogSlot()) Log("barbarian: 0x%08X Brutal Strike hits 0x%08X: %s", attacker_id, target_id, what.c_str());
  Tell(attacker, "Brutal Strike: " + EffectNames(pending.effects) + ".");
}

void AfterAttackRoll(AttackRoll& roll) {
  if (g_brutal.empty() || !roll.attacker || !roll.target || !roll.data) return;
  const auto it = g_brutal.find(IdOf(roll.attacker));
  if (it == g_brutal.end() || it->second.hit || it->second.data != roll.data || it->second.target != IdOf(roll.target)) {
    return;
  }
  if (!rules::MasteryAttackHit(roll.data[ee::kAttackResult])) {
    if (LogSlot()) Log("barbarian: 0x%08X Brutal Strike missed 0x%08X", IdOf(roll.attacker), it->second.target);
    g_brutal.erase(it);
    return;
  }
  it->second.hit = true;
  void* target = CreatureById(it->second.target);
  if (target) ApplyBrutalEffects(roll.attacker, target, it->second);
}

int RollDice(int count, int sides) {
  int total = 0;
  for (int i = 0; i < count; ++i) total += RollEngineDice(1, sides);
  return total;
}

}  // namespace

// rules_advantage.cpp's attack roll: Reckless Attack's Advantage - the
// attacker's Strength attack, and an attack at a creature that has been
// attacking recklessly (see the top).
MasteryRollSources RecklessRollSources(void* attacker, void* target, const uint8_t* data) {
  MasteryRollSources out;
  if (!Faithful() || !attacker) return out;
  const uint64_t now = GameMs();
  if (RecklessOn(attacker)) {
    g_reckless_until[IdOf(attacker)] = now + kRoundMs;
    if (StrengthAttack(attacker, data)) {
      ++out.advantage;
      out.note = "Reckless Attack";
    }
  }
  if (target && !g_reckless_until.empty()) {
    const auto it = g_reckless_until.find(IdOf(target));
    if (it != g_reckless_until.end()) {
      if (now < it->second) {
        ++out.advantage;
        out.note += (out.note.empty() ? "" : ", ") + std::string("a reckless target");
      } else {
        g_reckless_until.erase(it);
      }
    }
  }
  return out;
}

// rules_advantage.cpp, the roll's mode decided: Brutal Strike gives up the
// Advantage of the first reckless Strength attack each round (see the top).
bool BrutalStrikeOnRoll(void* attacker, void* target, const uint8_t* data, rules::RollMode* mode, std::string* note) {
  if (!Faithful() || !mode || *mode != rules::RollMode::kAdvantage || !attacker || !target || !data) return false;
  if (!CreatureById(IdOf(target)) || !RecklessOn(attacker) || !StrengthAttack(attacker, data)) return false;
  const int level = BarbarianLevel(attacker);
  if (!rules::HasBrutalStrike(level)) return false;
  const rules::BrutalChoice choice = rules::DecodeBrutalChoice(engine::GetLocalInt(attacker, kVarBrutal));
  const std::vector<rules::BrutalEffect> effects = rules::ActiveBrutalEffects(choice, level);
  const uint32_t id = IdOf(attacker);
  if (effects.empty() || UsedThisRound(&g_brutal_round, id)) return false;
  MarkUsed(&g_brutal_round, id);
  BrutalPending pending;
  pending.target = IdOf(target);
  pending.data = data;
  pending.level = level;
  pending.tick = GetTickCount64();
  pending.effects = effects;
  g_brutal[id] = pending;
  *mode = rules::RollMode::kNormal;
  if (note) *note += (note->empty() ? "" : "; ") + std::string("Brutal Strike: Advantage given up");
  return true;
}

// rules_fighter.cpp's GetDamageRoll (a hit's weapon damage): Brutal Strike's
// d10s and Frenzy's d6s, of the weapon's type, doubled on a critical hit.
int BarbarianExtraDamage(void* stats, void* target, int /*offhand*/, int critical) {
  if (!Faithful() || !stats) return 0;
  void* creature = CreatureOf(stats);
  const uint8_t* data = creature ? CurrentAttackData(creature) : nullptr;
  if (!data) return 0;  // not a hit being resolved (a sheet, an estimate)
  const uint32_t id = IdOf(creature);
  const uint32_t target_id = target ? IdOf(target) : 0;
  const int doubling = critical ? 2 : 1;
  int extra = 0;
  const auto brutal = g_brutal.find(id);
  if (brutal != g_brutal.end()) {
    if (brutal->second.hit && brutal->second.target == target_id &&
        GetTickCount64() - brutal->second.tick <= kPendingMs) {
      const int dice = rules::BrutalStrikeDice(brutal->second.level) * doubling;
      const int rolled = RollDice(dice, 10);
      extra += rolled;
      if (LogSlot()) {
        Log("barbarian: 0x%08X Brutal Strike +%dd10 = %d%s", id, dice, rolled, critical ? " (critical)" : "");
      }
    }
    if (brutal->second.hit || GetTickCount64() - brutal->second.tick > kPendingMs) g_brutal.erase(brutal);
  }
  if (target_id && CreatureById(target_id) && BarbarianRaging(creature) && RecklessOn(creature) &&
      StrengthAttack(creature, data) && !UsedThisRound(&g_frenzy, id)) {
    const int level = BarbarianLevel(creature);
    if (rules::HasFrenzy(level)) {
      MarkUsed(&g_frenzy, id);
      const int dice = rules::FrenzyDice(level) * doubling;
      const int rolled = RollDice(dice, 6);
      extra += rolled;
      if (LogSlot()) {
        Log("barbarian: 0x%08X Frenzy on 0x%08X: +%dd6 = %d%s", id, target_id, dice, rolled,
            critical ? " (critical)" : "");
      }
    }
  }
  return extra;
}

// rules_save_roll.cpp's save d20: Staggering Blow's Disadvantage, on its next save only.
int TakeStaggeredSave(void* creature) {
  if (g_staggered.empty() || !creature) return 0;
  const auto it = g_staggered.find(IdOf(creature));
  if (it == g_staggered.end()) return 0;
  if (GameMs() >= it->second.until) {
    g_staggered.erase(it);
    return 0;
  }
  if (!it->second.save) return 0;
  it->second.save = false;
  if (LogSlot()) Log("barbarian: 0x%08X staggered (by 0x%08X): Disadvantage on this save", IdOf(creature), it->second.by);
  return 1;
}

// rules_opportunity.cpp: a staggered creature makes no opportunity attacks.
bool StaggeredNoOpportunityAttack(uint32_t id) {
  if (g_staggered.empty()) return false;
  const auto it = g_staggered.find(id);
  if (it == g_staggered.end()) return false;
  if (GameMs() >= it->second.until) {
    g_staggered.erase(it);
    return false;
  }
  if (LogSlot()) Log("barbarian: 0x%08X staggered: no opportunity attack", id);
  return true;
}

// rules_attack.cpp's attack modifier: Sundering Blow's +5 on the next attack
// roll another creature makes at the target (within a round).
int SunderingAttackBonus(void* stats, void* target) {
  if (g_sundered.empty() || !stats || !target || !CurrentAttackRoll()) return 0;
  const auto it = g_sundered.find(IdOf(target));
  if (it == g_sundered.end()) return 0;
  if (GameMs() >= it->second.until) {
    g_sundered.erase(it);
    return 0;
  }
  const uint32_t attacker = ObjectIdOf(stats);
  if (attacker == it->second.by) return 0;  // another creature's attack
  g_sundered.erase(it);
  if (LogSlot()) Log("barbarian: 0x%08X sundered: +%d on 0x%08X's attack", IdOf(target), rules::kSunderingAttackBonus, attacker);
  return rules::kSunderingAttackBonus;
}

void InstallBarbarianStrikes() {
  if (!Faithful()) return;
  CountCombatRounds();  // Frenzy and Brutal Strike: once a round
  bool scripts = AddScriptHandler("n5_reckless", &ToggleReckless);
  scripts = AddScriptHandler("n5_bs_force", &PickBrutalScript<rules::BrutalEffect::kForceful>) && scripts;
  scripts = AddScriptHandler("n5_bs_ham", &PickBrutalScript<rules::BrutalEffect::kHamstring>) && scripts;
  scripts = AddScriptHandler("n5_bs_stag", &PickBrutalScript<rules::BrutalEffect::kStaggering>) && scripts;
  scripts = AddScriptHandler("n5_bs_sund", &PickBrutalScript<rules::BrutalEffect::kSundering>) && scripts;
  scripts = AddScriptHandler("n5_bs_off", &PickBrutalScript<rules::BrutalEffect::kNone>) && scripts;
  if (!scripts) Log("barbarian: the radial's Reckless Attack / Brutal Strike do nothing - use !reckless / !brutal");
  if (!AddAttackRollListeners(nullptr, &AfterAttackRoll)) Log("barbarian: no attack-roll hook - Brutal Strike has no effects");
  AddChatCommand("!reckless", &RecklessCommand);
  AddChatCommand("!brutal", &BrutalCommand);
  Log("barbarian: part 3 - Reckless Attack (radial toggle; henchmen while raging), Frenzy, Brutal Strike (radial "
      "effects; Hamstring Blow until picked)");
}
