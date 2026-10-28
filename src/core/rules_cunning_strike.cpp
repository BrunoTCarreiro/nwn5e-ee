// Phase 3 Rogue part 5 (docs/PLAN.md "Phase 3 — Rogue", R6; owner, 2026-10-28:
// "go with the recommendations"), class_features faithful:
// - Cunning Strike (Rogue 5), Improved Cunning Strike (11: two effects),
//   Devious Strikes (14: Daze, Knock Out, Obscure). A standing choice (X8),
//   picked on the radial (nwn5e's feats 1315 / 1316 - sub-radials, as Brutal
//   Strike's), the quickbar, the Combat options window (combat_options.cpp,
//   !options) or !cunning; kept as a record (nwn5e_cunning; never chosen =
//   off, so henchmen keep their full dice).
// - When a Sneak Attack's dice roll (rules_rogue.cpp's dice filter, inside
//   CNWSCreature::ResolveDamage), the chosen effects that fit are paid for
//   with dice before the roll (a critical hit doubles what is left); Trip is
//   skipped - and costs nothing - against a Huge or larger target.
// - When that hit's damage lands (damage_listeners: OnApplyDamage), the
//   target saves against 8 + PB + Dexterity modifier (the engine's
//   SavingThrowRoll, Fortitude = Constitution, Reflex = Dexterity; nwn5e's
//   DC, not converted) and the effects follow - in NWN terms:
//   Poison: Poisoned for a minute - the game's poison icon, which nwn5e's
//     conditions read as Poisoned (no ability damage), a Constitution save
//     (poison type: immunity counts) at the end of each of its turns ends it;
//   Trip: knocked down for a round (Topple's knockdown);
//   Withdraw: the Rogue's moves provoke no opportunity attack for the rest of
//     the round (rules_opportunity.cpp, as Tactical Shift);
//   Daze: NWN's Slow for a round (NWN's Dazed is 2024's Incapacitated);
//   Knock Out: asleep (Unconscious) for a minute, until it takes damage or
//     makes a Constitution save at the end of one of its turns;
//   Obscure: Blinded for a round.
// The Poisoner's Kit is waived (NWN has none).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "damage_listeners.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "resolve_damage.h"
#include "saves.h"
#include "script_hooks.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"

void RefreshCombatOptions(uint32_t player);  // combat_options.cpp

namespace {

using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);

constexpr const char* kVarCunning = "nwn5e_cunning";  // rules::EncodeCunningChoice (0: never chosen - off)
constexpr const char* kCunningTag = "nwn5e_cunning";  // the icon: an effect chosen
constexpr int kCunningIconRow = 224;                  // effecticons.txt
constexpr const char* kPoisonTag = "nwn5e_cs_poison";
constexpr const char* kKnockOutTag = "nwn5e_cs_ko";
constexpr int kAbilityDexterity = 1;
constexpr DWORD kPendingMs = 5000;     // a Sneak Attack's effects wait this long for its damage (real time)
constexpr uint64_t kRoundMs = 6000;    // game time
constexpr uint64_t kMinuteMs = 60000;
constexpr int kLogMax = 600;

struct Pending {
  uint32_t target = 0;
  std::vector<rules::CunningEffect> effects;
  int dc = 0;
  int forgone = 0;
  DWORD tick = 0;
};

enum class Lasting : uint64_t { kPoisoned = 0, kKnockedOut = 1 };
struct LastingEffect {
  uint32_t by = 0;
  int dc = 0;
  uint64_t until = 0;      // game time
  uint64_t next_save = 0;  // game time: the end of its turn
  bool woken = false;      // Knock Out: damaged - ended at its next update
};

// The engine's thread only.
std::unordered_map<uint32_t, Pending> g_pending;         // by the Rogue
std::unordered_map<uint64_t, LastingEffect> g_lasting;   // by target << 1 | kind
std::unordered_map<uint32_t, uint64_t> g_withdraw;       // the Rogue -> until (game time)
int g_logged = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_logged < kLogMax && ++g_logged; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
uint64_t Key(uint32_t target, Lasting kind) { return (static_cast<uint64_t>(target) << 1) | static_cast<uint64_t>(kind); }

int RogueLevelOf(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats ? ClassLevel2024(stats, rules::CharacterClass::kRogue) : 0;
}

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

rules::CunningChoice ChoiceOf(void* creature) {
  return rules::DecodeCunningChoice(creature ? engine::GetLocalInt(creature, kVarCunning) : 0);
}

int DcOf(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return 0;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  return rules::CunningStrikeDc(engine::AbilityModifier(stats, kAbilityDexterity),
                                rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating));
}

std::string EffectNames(const std::vector<rules::CunningEffect>& effects, bool costs) {
  std::string names;
  for (rules::CunningEffect e : effects) {
    names += (names.empty() ? "" : " + ") + std::string(rules::CunningEffectName(e));
    if (costs) names += " (" + std::to_string(rules::CunningEffectCost(e)) + "d6)";
  }
  return names.empty() ? "off" : names;
}

// The icon on the character (its tag), on or off.
void ShowIcon(uint32_t id, bool on) {
  std::string script = std::string("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if ("
                                   "GetEffectTag(e) == \"") + kCunningTag + "\") RemoveEffect(OBJECT_SELF, e); e = "
                                   "GetNextEffect(OBJECT_SELF); }";
  if (on) {
    script += "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectIcon(" +
              std::to_string(kCunningIconRow) + ")), \"" + kCunningTag + "\"), OBJECT_SELF);";
  }
  RunScriptChunk(script.c_str(), id);
}

// Removes the effects with `tag` from the creature `id` (run as it).
void RemoveTagged(uint32_t id, const char* tag, const char* message) {
  std::string script = std::string("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if ("
                                   "GetEffectTag(e) == \"") + tag + "\") { RemoveEffect(OBJECT_SELF, e); e = "
                                   "GetFirstEffect(OBJECT_SELF); } else e = GetNextEffect(OBJECT_SELF); }";
  if (message) script += std::string(" if (GetIsPC(OBJECT_SELF)) SendMessageToPC(OBJECT_SELF, \"") + message + "\");";
  RunScriptChunk(script.c_str(), id);
}

// --- The choice (n5_cs_*, !cunning, the Combat options window) ---------------------

void PickCunning(void* creature, rules::CunningEffect effect) {
  const int level = RogueLevelOf(creature);
  if (!Faithful() || !rules::HasCunningStrike(level)) {
    Tell(creature, "Cunning Strike comes at Rogue 5.");
    return;
  }
  EnsureRecords(creature);
  rules::CunningChoice choice = ChoiceOf(creature);
  if (rules::PickCunningEffect(&choice, effect, level) == rules::CunningPick::kLocked) {
    Tell(creature, std::string(rules::CunningEffectName(effect)) + " comes at Rogue 14 (Devious Strikes).");
    return;
  }
  const uint32_t id = IdOf(creature);
  SetRecord(creature, kVarCunning, rules::EncodeCunningChoice(choice));
  const std::vector<rules::CunningEffect> active = rules::ActiveCunningEffects(choice, level);
  ShowIcon(id, !active.empty());
  Log("rogue: 0x%08X Cunning Strike: %s", id, EffectNames(active, true).c_str());
  std::string line = "Cunning Strike: " + EffectNames(active, true);
  if (active.empty()) {
    line += " - your Sneak Attacks keep all their dice";
  } else {
    line += " - paid with Sneak Attack dice on your next Sneak Attacks (DC " + std::to_string(DcOf(creature)) + ")";
    if (rules::CunningStrikeEffectCount(level) > 1 && active.size() < 2) line += "; pick another for two";
  }
  Tell(creature, line + ".");
  RefreshCombatOptions(id);
}

template <rules::CunningEffect kEffect>
void PickCunningScript(uint32_t self) {
  if (void* creature = CreatureById(self)) PickCunning(creature, kEffect);
}

std::vector<std::string> CunningCommand(void* creature, const std::string& args) {
  const int level = RogueLevelOf(creature);
  if (!Faithful() || !rules::HasCunningStrike(level)) return {"nwn5e: Cunning Strike comes at Rogue 5."};
  struct Word {
    const char* word;
    rules::CunningEffect effect;
  };
  static const Word kWords[] = {{"poison", rules::CunningEffect::kPoison},   {"trip", rules::CunningEffect::kTrip},
                                {"withdraw", rules::CunningEffect::kWithdraw}, {"daze", rules::CunningEffect::kDaze},
                                {"knock", rules::CunningEffect::kKnockOut},   {"obscure", rules::CunningEffect::kObscure},
                                {"off", rules::CunningEffect::kNone}};
  bool picked = false;
  for (const Word& w : kWords) {
    if (args.find(w.word) == std::string::npos) continue;
    PickCunning(creature, w.effect);
    picked = true;
    if (w.effect == rules::CunningEffect::kNone) break;
  }
  if (picked) return {};
  EnsureRecords(creature);
  return {"nwn5e: Cunning Strike (Rogue " + std::to_string(level) + ", " +
          std::to_string(rules::CunningStrikeEffectCount(level)) + " effect" +
          (rules::CunningStrikeEffectCount(level) > 1 ? "s" : "") + " per Sneak Attack, DC " +
          std::to_string(DcOf(creature)) + "): " +
          EffectNames(rules::ActiveCunningEffects(ChoiceOf(creature), level), true) +
          ". !cunning poison | trip | withdraw" + (rules::HasDeviousStrikes(level) ? " | daze | knock out | obscure" : "") +
          " | off (or its radial menu, or !options)."};
}

// --- Paying for the effects (rules_rogue.cpp's sneak dice) -------------------------

bool CanTrip(void* target) { return Field<int32_t>(target, ee::kCreatureSize) <= ee::kCreatureSizeLarge; }

// --- The effects, when the hit's damage lands ----------------------------------------

uint8_t Save(void* target, uint32_t by, int dc, uint8_t save, uint8_t type, int from_combat) {
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through the hook
  if (!save_roll) return 1;
  RememberSaveDc2024(by, dc);  // already 2024: not converted
  return save_roll(target, save, static_cast<uint16_t>(dc), type, by, 1, 0, from_combat);
}

// Effects applied to `target` a tick later (never inside the engine's own
// damage handling), as the Rogue.
void ApplyLater(uint32_t rogue, uint32_t target, const std::string& applications) {
  char head[96];
  sprintf_s(head, "object t = StringToObject(\"%x\"); if (!GetIsObjectValid(t)) return; ", target);
  RunScriptChunk((head + applications).c_str(), rogue);
}

std::string Delayed(const std::string& duration, const std::string& effect, float seconds) {
  char tail[48];
  sprintf_s(tail, ", t, %.1f));", seconds);
  return "DelayCommand(0.0, ApplyEffectToObject(" + duration + ", " + effect + tail;
}

std::string Visual(const char* vfx) {
  return std::string("DelayCommand(0.0, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(") + vfx +
         "), t));";
}

std::string Outcome(uint8_t save, const char* failed) {
  return save == 2 ? "immune" : save == 0 ? std::string("failed - ") + failed : "made";
}

void ApplyEffects(void* rogue, void* target, const Pending& pending) {
  const uint32_t rogue_id = IdOf(rogue), target_id = IdOf(target);
  const uint64_t now = GameMs();
  const int dc = pending.dc;
  const std::string dcs = "DC " + std::to_string(dc);
  std::string what, script;
  for (rules::CunningEffect effect : pending.effects) {
    std::string part = rules::CunningEffectName(effect);
    switch (effect) {
      case rules::CunningEffect::kPoison: {
        const uint8_t save = Save(target, rogue_id, dc, ee::kSaveFortitude, ee::kSaveTypePoison, 1);
        part += " (" + dcs + " Constitution save " + Outcome(save, "Poisoned for a minute") + ")";
        if (save != 0) break;
        script += Delayed("DURATION_TYPE_TEMPORARY",
                          std::string("TagEffect(SupernaturalEffect(EffectIcon(EFFECT_ICON_POISON)), \"") + kPoisonTag +
                              "\")",
                          kMinuteMs / 1000.0f) +
                  Visual("VFX_IMP_POISON_S");
        g_lasting[Key(target_id, Lasting::kPoisoned)] = LastingEffect{rogue_id, dc, now + kMinuteMs, now + kRoundMs};
        break;
      }
      case rules::CunningEffect::kTrip: {
        const uint8_t save = Save(target, rogue_id, dc, ee::kSaveReflex, 0, 1);
        part += " (" + dcs + " Dexterity save " + Outcome(save, "knocked prone for a round") + ")";
        if (save == 0) script += Delayed("DURATION_TYPE_TEMPORARY", "EffectKnockdown()", kRoundMs / 1000.0f);
        break;
      }
      case rules::CunningEffect::kWithdraw:
        g_withdraw[rogue_id] = now + kRoundMs;
        part += " (you can move away this round without provoking opportunity attacks)";
        break;
      case rules::CunningEffect::kDaze: {
        const uint8_t save = Save(target, rogue_id, dc, ee::kSaveFortitude, 0, 1);
        part += " (" + dcs + " Constitution save " + Outcome(save, "Slowed for a round") + ")";
        if (save == 0) {
          script += Delayed("DURATION_TYPE_TEMPORARY", "EffectSlow()", kRoundMs / 1000.0f) + Visual("VFX_IMP_SLOW");
        }
        break;
      }
      case rules::CunningEffect::kKnockOut: {
        const uint8_t save = Save(target, rogue_id, dc, ee::kSaveFortitude, 0, 1);
        part += " (" + dcs + " Constitution save " + Outcome(save, "knocked out for a minute, until damaged") + ")";
        if (save != 0) break;
        script += "if (GetIsImmune(t, IMMUNITY_TYPE_SLEEP, OBJECT_SELF)) { if (GetIsPC(OBJECT_SELF)) "
                  "SendMessageToPC(OBJECT_SELF, \"nwn5e: Knock Out - \" + GetName(t) + \" can't be put to sleep "
                  "(NWN's sleep immunity).\"); } else { " +
                  Delayed("DURATION_TYPE_TEMPORARY", std::string("TagEffect(EffectSleep(), \"") + kKnockOutTag + "\")",
                          kMinuteMs / 1000.0f) +
                  Visual("VFX_IMP_SLEEP") + " }";
        g_lasting[Key(target_id, Lasting::kKnockedOut)] = LastingEffect{rogue_id, dc, now + kMinuteMs, now + kRoundMs};
        break;
      }
      case rules::CunningEffect::kObscure: {
        const uint8_t save = Save(target, rogue_id, dc, ee::kSaveReflex, 0, 1);
        part += " (" + dcs + " Dexterity save " + Outcome(save, "Blinded for a round") + ")";
        if (save == 0) {
          script += Delayed("DURATION_TYPE_TEMPORARY", "EffectBlindness()", kRoundMs / 1000.0f) +
                    Visual("VFX_IMP_BLIND_DEAF_M");
        }
        break;
      }
      default:
        continue;
    }
    what += (what.empty() ? "" : "; ") + part;
  }
  if (!script.empty()) ApplyLater(rogue_id, target_id, script);
  if (LogSlot()) Log("rogue: 0x%08X Cunning Strike on 0x%08X (%dd6 given up): %s", rogue_id, target_id, pending.forgone,
                     what.c_str());
  Tell(rogue, "Cunning Strike (" + std::to_string(pending.forgone) + "d6 given up): " + what + ".");
}

void OnDamage(uint32_t creator, void* target, int damage) {
  if (!target) return;
  const uint32_t target_id = IdOf(target);
  // Knock Out ends when it takes damage (ended at its next update - not inside
  // the engine's damage handling; checked before this hit's own effects, so a
  // new Knock Out from this hit stays).
  if (damage > 0 && !g_lasting.empty()) {
    const auto ko = g_lasting.find(Key(target_id, Lasting::kKnockedOut));
    if (ko != g_lasting.end() && !ko->second.woken) {
      ko->second.woken = true;
      if (LogSlot()) Log("rogue: 0x%08X wakes up - damaged (%d) while knocked out by 0x%08X", target_id, damage,
                         ko->second.by);
    }
  }
  if (g_pending.empty()) return;
  const auto it = g_pending.find(creator);
  if (it == g_pending.end()) return;
  if (it->second.target != target_id) return;
  const Pending pending = it->second;
  g_pending.erase(it);
  void* rogue = CreatureById(creator);
  void* victim = CreatureById(target_id);
  if (!rogue || !victim || GetTickCount() - pending.tick > kPendingMs) return;
  if (engine::IsDeadOrDying(victim)) {
    if (LogSlot()) Log("rogue: 0x%08X Cunning Strike on 0x%08X - it went down: no effects", creator, target_id);
    return;
  }
  ApplyEffects(rogue, victim, pending);
}

// The repeat saves at the end of the target's turns (every 6 s), and the end of
// the effects (a minute, its death, removed some other way).
void LastingUpdate(void* creature) {
  if (g_lasting.empty() || !creature) return;
  const uint32_t id = IdOf(creature);
  for (Lasting kind : {Lasting::kPoisoned, Lasting::kKnockedOut}) {
    const auto it = g_lasting.find(Key(id, kind));
    if (it == g_lasting.end()) continue;
    LastingEffect& lasting = it->second;
    const uint64_t now = GameMs();
    if (now >= lasting.until || engine::IsDeadOrDying(creature)) {
      g_lasting.erase(it);
      continue;
    }
    if (lasting.woken) {
      RemoveTagged(id, kKnockOutTag, "nwn5e: the blow wakes you.");
      g_lasting.erase(it);
      continue;
    }
    if (now < lasting.next_save) continue;
    lasting.next_save = now + kRoundMs;
    const bool poisoned = kind == Lasting::kPoisoned;
    const bool still_on = poisoned ? ReadConditions(creature).poisoned : effects::HasState(creature, ee::kStateAsleep);
    if (!still_on) {  // cured, dispelled, rested - or never took (sleep immunity)
      g_lasting.erase(it);
      continue;
    }
    const uint8_t save =
        Save(creature, lasting.by, lasting.dc, ee::kSaveFortitude, poisoned ? ee::kSaveTypePoison : 0, 0);
    if (save == 0) continue;
    RemoveTagged(id, poisoned ? kPoisonTag : kKnockOutTag,
                 poisoned ? "nwn5e: you shake off the poison." : "nwn5e: you come round.");
    Log("rogue: 0x%08X saved against 0x%08X's Cunning Strike - no longer %s", id, lasting.by,
        poisoned ? "poisoned" : "knocked out");
    g_lasting.erase(it);
  }
}

}  // namespace

// rules_rogue.cpp's sneak dice (inside ResolveDamage): the dice the creature's
// chosen effects cost for this Sneak Attack (0: none), remembered for its damage.
int CunningStrikeForgo(void* stats, int dice) {
  if (!Faithful() || !stats || dice <= 0) return 0;
  void* rogue = CreatureOf(stats);
  const int level = rogue ? RogueLevelOf(rogue) : 0;
  if (!rules::HasCunningStrike(level)) return 0;
  const rules::CunningChoice choice = ChoiceOf(rogue);
  if (rules::ActiveCunningEffects(choice, level).empty()) return 0;
  void* attacker = nullptr;
  void* object = nullptr;
  if (!ResolvingDamage(&attacker, &object) || attacker != rogue || !object) return 0;
  void* target = CreatureById(IdOf(object));
  if (!target) return 0;  // a door, a placeable: the full dice
  int left = dice;
  const std::vector<rules::CunningEffect> effects =
      rules::CunningEffectsForSneakAttack(choice, level, dice, CanTrip(target), &left);
  const uint32_t id = IdOf(rogue);
  if (effects.empty()) {
    if (LogSlot()) Log("rogue: 0x%08X Cunning Strike: %s doesn't fit this Sneak Attack (%dd6%s) - full dice", id,
                       EffectNames(rules::ActiveCunningEffects(choice, level), true).c_str(), dice,
                       CanTrip(target) ? "" : "; no Trip on a Huge target");
    return 0;
  }
  Pending pending;
  pending.target = IdOf(target);
  pending.effects = effects;
  pending.dc = DcOf(rogue);
  pending.forgone = dice - left;
  pending.tick = GetTickCount();
  g_pending[id] = pending;
  if (LogSlot()) Log("rogue: 0x%08X Cunning Strike: %s - %dd6 of %dd6 given up, DC %d", id,
                     EffectNames(effects, true).c_str(), pending.forgone, dice, pending.dc);
  return pending.forgone;
}

// rules_opportunity.cpp: Withdraw - the Rogue's moves provoke nothing this round.
bool CunningWithdrawing(uint32_t id) {
  if (g_withdraw.empty()) return false;
  const auto it = g_withdraw.find(id);
  if (it == g_withdraw.end()) return false;
  if (GameMs() < it->second) return true;
  g_withdraw.erase(it);
  return false;
}

// The Combat options window (combat_options.cpp).
int CunningStrikeRogueLevel(void* creature) { return Faithful() ? RogueLevelOf(creature) : 0; }
rules::CunningChoice CunningStrikeChoice(void* creature) {
  if (creature) EnsureRecords(creature);
  return ChoiceOf(creature);
}
int CunningStrikeDcOf(void* creature) { return DcOf(creature); }
void PickCunningStrike(void* creature, rules::CunningEffect effect) {
  if (creature) PickCunning(creature, effect);
}

void InstallCunningStrike() {
  if (!Faithful()) return;
  RegisterRecords({kVarCunning});
  bool scripts = AddScriptHandler("n5_cs_poison", &PickCunningScript<rules::CunningEffect::kPoison>);
  scripts = AddScriptHandler("n5_cs_trip", &PickCunningScript<rules::CunningEffect::kTrip>) && scripts;
  scripts = AddScriptHandler("n5_cs_wdraw", &PickCunningScript<rules::CunningEffect::kWithdraw>) && scripts;
  scripts = AddScriptHandler("n5_cs_off", &PickCunningScript<rules::CunningEffect::kNone>) && scripts;
  scripts = AddScriptHandler("n5_cs_daze", &PickCunningScript<rules::CunningEffect::kDaze>) && scripts;
  scripts = AddScriptHandler("n5_cs_ko", &PickCunningScript<rules::CunningEffect::kKnockOut>) && scripts;
  scripts = AddScriptHandler("n5_cs_obsc", &PickCunningScript<rules::CunningEffect::kObscure>) && scripts;
  if (!scripts) Log("rogue: the radial's Cunning Strike options do nothing - use !cunning or !options");
  AddChatCommand("!cunning", &CunningCommand);
  const bool damage = AddDamageListener(&OnDamage);
  const bool updates = AddCreatureUpdateListener(&LastingUpdate);
  Log("rogue: part 5 - Cunning Strike %s (Poison, Trip, Withdraw; two at 11; Daze, Knock Out, Obscure at 14)%s",
      damage ? "on" : "NOT on (no damage hook)", updates ? "" : " - no repeat saves (no creature updates)");
}
