#include "rules/quickbar.h"

#include <algorithm>

#include "rules/class_features.h"

namespace rules {

namespace {

// Brutal Strike's sub-radial options (spells.barbarian.txt, FeatID = option * 65536 + 1311).
constexpr int kBrutalOptionForceful = 16000;
constexpr int kBrutalOptionHamstring = 16001;
constexpr int kBrutalOptionStaggering = 16002;
constexpr int kBrutalOptionSundering = 16003;
constexpr int kBrutalOptionOff = 16004;
// Cunning Strike's and Devious Strikes' (spells.rogue.txt; * 65536 + 1315 / 1316).
constexpr int kCunningOptionPoison = 16005;
constexpr int kCunningOptionTrip = 16006;
constexpr int kCunningOptionWithdraw = 16007;
constexpr int kCunningOptionOff = 16008;
constexpr int kDeviousOptionDaze = 16009;
constexpr int kDeviousOptionKnockOut = 16010;
constexpr int kDeviousOptionObscure = 16011;
constexpr int kCunningOptionCombatOptions = 16012;

// The record's bits - fixed once shipped (a character's record keeps them).
enum Bit {
  kBitSecondWind = 0,
  kBitRage = 1,
  kBitReckless = 2,
  kBitForceful = 3,
  kBitHamstring = 4,
  kBitStaggering = 5,
  kBitSundering = 6,
  kBitBrutalOff = 7,
  kBitIntimidatingPresence = 8,
  kBitShove = 9,
  kBitShovePush = 10,
  kBitGrapple = 11,
  kBitCunningPoison = 12,
  kBitCunningTrip = 13,
  kBitCunningWithdraw = 14,
  kBitCunningOff = 15,
  kBitDeviousDaze = 16,
  kBitDeviousKnockOut = 17,
  kBitDeviousObscure = 18,
  kBitCombatOptions = 19,
  kBitFlurryOfBlows = 20,
  kBitPatientDefense = 21,
  kBitStepOfTheWind = 22,
  kBitStunningStrike = 23,
  kBitSuperiorDefense = 24,
};

bool IsSecondWind(int value) {
  return value == kFeatSecondWind2 || value == kFeatSecondWind3 || value == kFeatSecondWind4;
}

}  // namespace

const std::vector<int>& QuickbarActionFeats2024() {
  static const std::vector<int> kFeats = {kFeatSecondWind2,    kFeatSecondWind3,  kFeatSecondWind4,
                                          kFeatRage,           kFeatRecklessAttack, kFeatBrutalStrike,
                                          kFeatIntimidatingPresence, kFeatKnockdownShove, kFeatShovePush,
                                          kFeatGrapple,        kFeatCunningStrike, kFeatDeviousStrikes,
                                          kFeatFlurryOfBlows,  kFeatPatientDefense, kFeatStepOfTheWind,
                                          kFeatStunningFist,   kFeatSuperiorDefense};
  return kFeats;
}

std::vector<QuickbarAction> QuickbarActions2024(const QuickbarFacts& facts) {
  auto has = [&facts](int feat) {
    return std::find(facts.feats.begin(), facts.feats.end(), feat) != facts.feats.end();
  };
  std::vector<QuickbarAction> actions;
  const int second_wind = has(kFeatSecondWind4)   ? kFeatSecondWind4
                          : has(kFeatSecondWind3) ? kFeatSecondWind3
                          : has(kFeatSecondWind2) ? kFeatSecondWind2
                                                  : 0;
  if (second_wind) actions.push_back({kBitSecondWind, second_wind, "Second Wind"});
  if (has(kFeatRage)) actions.push_back({kBitRage, kFeatRage, "Rage"});
  if (has(kFeatRecklessAttack)) actions.push_back({kBitReckless, kFeatRecklessAttack, "Reckless Attack"});
  if (has(kFeatBrutalStrike)) {
    const struct {
      BrutalEffect effect;
      int option;
      int bit;
      const char* name;
    } kOptions[] = {
        {BrutalEffect::kForceful, kBrutalOptionForceful, kBitForceful, "Brutal Strike: Forceful Blow"},
        {BrutalEffect::kHamstring, kBrutalOptionHamstring, kBitHamstring, "Brutal Strike: Hamstring Blow"},
        {BrutalEffect::kStaggering, kBrutalOptionStaggering, kBitStaggering, "Brutal Strike: Staggering Blow"},
        {BrutalEffect::kSundering, kBrutalOptionSundering, kBitSundering, "Brutal Strike: Sundering Blow"},
    };
    for (const auto& o : kOptions) {
      if (BrutalEffectAvailable(o.effect, facts.barbarian_level)) {
        actions.push_back({o.bit, QuickbarFeatValue(kFeatBrutalStrike, o.option), o.name});
      }
    }
    actions.push_back({kBitBrutalOff, QuickbarFeatValue(kFeatBrutalStrike, kBrutalOptionOff), "Brutal Strike: Off"});
  }
  if (has(kFeatIntimidatingPresence)) {
    actions.push_back({kBitIntimidatingPresence, kFeatIntimidatingPresence, "Intimidating Presence"});
  }
  if (has(kFeatCunningStrike)) {
    actions.push_back({kBitCunningPoison, QuickbarFeatValue(kFeatCunningStrike, kCunningOptionPoison),
                       "Cunning Strike: Poison"});
    actions.push_back({kBitCunningTrip, QuickbarFeatValue(kFeatCunningStrike, kCunningOptionTrip), "Cunning Strike: Trip"});
    actions.push_back({kBitCunningWithdraw, QuickbarFeatValue(kFeatCunningStrike, kCunningOptionWithdraw),
                       "Cunning Strike: Withdraw"});
    actions.push_back({kBitCunningOff, QuickbarFeatValue(kFeatCunningStrike, kCunningOptionOff), "Cunning Strike: Off"});
    actions.push_back({kBitCombatOptions, QuickbarFeatValue(kFeatCunningStrike, kCunningOptionCombatOptions),
                       "Combat options"});
  }
  if (has(kFeatDeviousStrikes)) {
    actions.push_back({kBitDeviousDaze, QuickbarFeatValue(kFeatDeviousStrikes, kDeviousOptionDaze),
                       "Devious Strikes: Daze"});
    actions.push_back({kBitDeviousKnockOut, QuickbarFeatValue(kFeatDeviousStrikes, kDeviousOptionKnockOut),
                       "Devious Strikes: Knock Out"});
    actions.push_back({kBitDeviousObscure, QuickbarFeatValue(kFeatDeviousStrikes, kDeviousOptionObscure),
                       "Devious Strikes: Obscure"});
  }
  // Monk part 3 (0.56.0): Flurry of Blows' mode, the Focus feats, Stunning Strike (NWN's Stunning Fist).
  if (has(kFeatFlurryOfBlows)) actions.push_back({kBitFlurryOfBlows, kFeatFlurryOfBlows, "Flurry of Blows"});
  if (has(kFeatPatientDefense)) actions.push_back({kBitPatientDefense, kFeatPatientDefense, "Patient Defense"});
  if (has(kFeatStepOfTheWind)) actions.push_back({kBitStepOfTheWind, kFeatStepOfTheWind, "Step of the Wind"});
  if (has(kFeatStunningFist)) actions.push_back({kBitStunningStrike, kFeatStunningFist, "Stunning Strike"});
  if (has(kFeatSuperiorDefense)) actions.push_back({kBitSuperiorDefense, kFeatSuperiorDefense, "Superior Defense"});
  if (has(kFeatKnockdownShove)) actions.push_back({kBitShove, kFeatKnockdownShove, "Shove"});
  if (has(kFeatShovePush)) actions.push_back({kBitShovePush, kFeatShovePush, "Shove: push"});
  if (has(kFeatGrapple)) actions.push_back({kBitGrapple, kFeatGrapple, "Grapple"});
  return actions;
}

bool SameQuickbarAction(int value_a, int value_b) {
  return value_a == value_b || (IsSecondWind(value_a) && IsSecondWind(value_b));
}

std::vector<QuickbarChange> PlanQuickbar(const std::vector<QuickbarSlot>& slots, int placed,
                                         const std::vector<QuickbarAction>& actions, int* new_placed) {
  std::vector<QuickbarSlot> bar = slots;
  bar.resize(kQuickbarSlots);
  int record = placed;
  std::vector<QuickbarChange> changes;
  for (const QuickbarAction& action : actions) {
    const int bit = 1 << action.record_bit;
    bool on_bar = false;
    for (int i = 0; i < kQuickbarSlots; ++i) {
      if (bar[i].type != kQuickbarTypeFeat || !SameQuickbarAction(bar[i].value, action.value)) continue;
      on_bar = true;
      if (bar[i].value != action.value) {  // Second Wind after a level: the button follows the feat
        changes.push_back({i, action.value, action.record_bit, true});
        bar[i].value = action.value;
      }
    }
    if (record & bit) continue;  // placed before: never again
    if (on_bar) {
      record |= bit;
      continue;
    }
    for (int i = 0; i < kQuickbarSlots; ++i) {
      if (bar[i].type != kQuickbarTypeEmpty) continue;
      changes.push_back({i, action.value, action.record_bit, false});
      bar[i] = {kQuickbarTypeFeat, action.value};
      record |= bit;
      break;
    }
  }
  if (new_placed) *new_placed = record;
  return changes;
}

std::string QuickbarSlotLabel(int slot, bool number_keys) {
  if (slot < 0 || slot >= kQuickbarSlots) return "";
  const int key = slot % 12 + 1;
  if (number_keys && slot < 24 && key <= kQuickbarNumberKeys) return std::to_string(key);
  return "F" + std::to_string(key);
}

std::string QuickbarSlotKeys(int slot, bool number_keys) {
  if (slot < 0 || slot >= kQuickbarSlots) return "";
  const int key = slot % 12 + 1;
  const std::string row = slot < 12 ? "" : slot < 24 ? "Shift+" : "Ctrl+";
  const std::string f = row + "F" + std::to_string(key);
  // keys 1-9 on the first row and, Shift held, the second (Ctrl+1-4 keep EE's Quick Groups)
  if (number_keys && slot < 24 && key <= kQuickbarNumberKeys) return row + std::to_string(key) + " / " + f;
  return f;
}

}  // namespace rules
