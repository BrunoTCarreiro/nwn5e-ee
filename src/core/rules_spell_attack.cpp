// SK10 (port of 1.69's rules_spell_attack.cpp; docs/EE-PORT.md E3c),
// `spell_attacks` faithful: spell attacks the 2024 way. NWN's TouchAttackMelee
// / TouchAttackRanged (one handler, ExecuteCommandDoTouchAttack) roll their
// own d20 with the melee / ranged attack bonus (asked with touch = 1) against
// touch AC, with NWN's threat and confirmation roll; ResolveAttackRoll is never
// called, so the attack-roll rules don't see it. Faithful, for a creature
// casting on a creature:
// - the bonus is the proficiency bonus + the casting ability modifier of the
//   spell being cast (the class slot or item it is cast from, as the spell DC
//   reads it, C5; a scroll's 2024 attack bonus for items) + the touch-attack
//   effect bonuses the engine adds (Bless and the like) — answered in the
//   attack-bonus hooks (rules_sheet.cpp) while the handler runs;
// - against full AC (2024 has no touch AC): the AC hook asks without touch;
// - the d20 with advantage / disadvantage as for any attack (NWN's
//   situational terms, close combat for ranged, conditions — concealment
//   too, its miss chance being gone; Halfling Luck), by the dice filter at
//   the handler's d20;
// - with crit_no_confirmation, a natural 20 is a critical hit: the
//   confirmation die passes (the spell script still decides the critical
//   damage — later).
// A caster that is not a creature (a trap, a placeable) keeps NWN's roll.
// EE: the script functions take a third argument, the attacker, so the
// caster is read from the calls, not OBJECT_SELF. NWN mode: NWN's (with the
// global attack conversion).

#include <stdint.h>

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "saves.h"

int RollSpellAttackD20(void* caster, void* target, bool ranged, std::string* note);  // rules_advantage.cpp

namespace {

using TouchCommandFn = int (*)(void* commands, int command, int parameters);
using TotalEffectBonusFn = int (*)(void* creature, uint8_t type, void* vs, int a3, int a4, uint8_t a5, uint8_t a6,
                                   uint8_t a7, uint8_t a8, int a9);
using ArmorClassFn = int16_t (*)(void* stats, void* attacker, int touch);

TouchCommandFn g_touch_command = nullptr;
TotalEffectBonusFn g_total_effect_bonus = nullptr;
uintptr_t g_d20_caller = 0;      // the creature path's d20
uintptr_t g_confirm_caller = 0;  // ... and its confirmation die

// The touch attack being resolved (the engine's server thread).
struct Touch {
  bool active = false;
  bool ranged = false;
  void* caster = nullptr;  // from the AC call
  void* target = nullptr;
  int bonus = 0;
  bool bonus_known = false;
};
Touch g_touch;
LogOnce g_logged(300);
int g_roll_lines = 0;

bool Faithful() { return g_config.spell_attacks == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

int TouchCommand(void* commands, int command, int parameters) {
  const Touch outer = g_touch;
  g_touch = Touch();
  g_touch.active = true;
  g_touch.ranged = command == ee::kCommandTouchAttackRanged;
  const int result = g_touch_command(commands, command, parameters);
  g_touch = outer;
  return result;
}

bool LogPlayerInvolved(void* a, void* b) {
  for (void* creature : {a, b}) {
    void* stats = creature ? StatsOf(creature) : nullptr;
    if (stats && ReadCreatureFacts(stats).is_player) return true;
  }
  return false;
}

// The dice of the touch attack: its d20, and the confirmation of a natural 20.
bool TouchDice(uintptr_t caller, int count, int sides, int* result) {
  if (!g_touch.active || count != 1 || sides != 20) return false;
  if (caller == g_confirm_caller && g_config.crit_no_confirmation) {
    *result = 1000;  // the confirmation passes: a natural 20 is a critical hit (its die is not shown)
    return true;
  }
  if (caller != g_d20_caller || !g_touch.caster || !g_touch.target) return false;
  std::string note;
  const int d20 = RollSpellAttackD20(g_touch.caster, g_touch.target, g_touch.ranged, &note);
  if (g_config.log_attacks && g_roll_lines < 300 && LogPlayerInvolved(g_touch.caster, g_touch.target)) {
    ++g_roll_lines;
    static const auto armor_class = reinterpret_cast<ArmorClassFn>(EngineExport(ee::kGetArmorClassVersus));
    void* target_stats = StatsOf(g_touch.target);
    const int ac = armor_class && target_stats ? armor_class(target_stats, g_touch.caster, 0) : 0;
    const int total = d20 + g_touch.bonus;
    Log("spell attack roll: 0x%08X vs 0x%08X (%s touch): d20 %d %+d = %d vs AC %d -> %s (%s)", IdOf(g_touch.caster),
        IdOf(g_touch.target), g_touch.ranged ? "ranged" : "melee", d20, g_touch.bonus, total, ac,
        d20 == 20 ? "critical hit (natural 20)" : d20 == 1 ? "miss (natural 1)" : total >= ac ? "hit" : "miss",
        note.empty() ? "normal" : note.c_str());
  }
  *result = d20;
  return true;
}

}  // namespace

// rules_sheet.cpp: the attack-bonus hooks with touch = 1 (only the touch handler asks so).
int TouchAttackBonus(void* stats, int value, bool ranged) {
  if (!g_touch.active || !Faithful() || !stats) return value;
  void* creature = CreatureOf(stats);
  int casting = 0;
  const char* source = "";
  if (!creature || !SpellAttackCasting(creature, &casting, &source)) return value;
  const int effects = g_total_effect_bonus ? g_total_effect_bonus(creature, ee::kEffectBonusTypeTouchAttack, nullptr,
                                                                  0, 0, 0, 0, 0xFF, 0xFF, 0)
                                           : 0;
  const int bonus = casting + effects;
  g_touch.bonus = bonus;
  g_touch.bonus_known = true;
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(ObjectIdOf(stats), value, bonus) ^ (ranged ? 1ull << 63 : 0))) {
    Log("spell attack: object 0x%08X %s touch (%s): NWN %+d -> 2024 %+d (casting %+d, effects %+d)", ObjectIdOf(stats),
        ranged ? "ranged" : "melee", source, value, bonus, casting, effects);
  }
  return bonus;
}

// rules_attack.cpp: the AC hook asked for touch AC. Inside a touch attack
// (faithful) it is full AC; the caster and target are noted for the d20.
bool SpellAttackAgainstFullAc(void* target_stats, void* attacker) {
  if (!g_touch.active || !Faithful()) return false;
  g_touch.target = CreatureOf(target_stats);
  g_touch.caster = attacker;
  return true;
}

void InstallSpellAttackRules() {
  if (!Faithful()) return;
  g_total_effect_bonus = reinterpret_cast<TotalEffectBonusFn>(EngineExport(ee::kGetTotalEffectBonus));
  const std::vector<uintptr_t> dice =
      FindCallsTo(FunctionAt(EngineExport(ee::kExecuteCommandDoTouchAttack)), EngineExport(ee::kRollDice));
  if (dice.size() != 4) {
    Log("SKIP spell attacks: %zu RollDice calls in the touch attack handler (expected 4)", dice.size());
    return;
  }
  g_d20_caller = dice[0] + kCallLength;      // creature caster: the d20 (+0x199) ...
  g_confirm_caller = dice[1] + kCallLength;  // ... and the confirmation (+0x23d)
  if (!HookExport("script TouchAttackMelee / TouchAttackRanged (2024 spell attacks)", ee::kExecuteCommandDoTouchAttack,
                  reinterpret_cast<void*>(&TouchCommand), reinterpret_cast<void**>(&g_touch_command))) {
    return;
  }
  AddDiceFilter(&TouchDice);
  Log("spell attacks: PB + casting ability against full AC, the d20 with advantage / disadvantage (faithful)");
}
