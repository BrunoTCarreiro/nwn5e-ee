#pragma once

// Phase 6 - the action economy (docs/PLAN.md "Phase 6 — Action economy";
// decided by the owner 2026-11-02, #51): per-round budgets on NWN's 6 s round -
// one Action, one Bonus Action, one Reaction. Part 1 (#52): the Bonus Action.

namespace rules {

constexpr long long kActionRoundMs = 6000;

// The Bonus Action is free again when it wasn't used, when a round of the
// creature's own ended since, or when a round's time went by (out of combat:
// no round runs).
bool BonusActionFree(bool used, bool round_ended_since, long long ms_since_use);

// The feats that take the Bonus Action (2024): Second Wind, Rage (NWN's Rage
// and its successors), Intimidating Presence, Patient Defense, Step of the
// Wind, Wholeness of Body. Instant in the engine (feat.2da ReqAction 0), one a
// round.
bool TakesBonusAction(int feat);
// The feats that take no action at all: Reckless Attack (decided at the first
// attack), Superior Defense (at the start of a turn) and the standing-choice
// pickers (Brutal Strike, Cunning Strike, Devious Strikes, Open Hand
// Technique). Instant, no budget.
bool TakesNoAction(int feat);
// The name a chat line gives a Bonus Action feat ("" for any other).
const char* BonusActionName(int feat);

// The default Bonus Action (Combat configuration; 0.61.1, owner: "This option
// should allow the player to select what bonus action they'd want to be
// triggered automatically"): the automatic bonus attack (the off-hand attack,
// Martial Arts' strike, Flurry of Blows in its mode), a Bonus Action ability
// (fired at the start of each of the creature's rounds in combat while its
// Bonus Action is free), or none - kept for one clicked.
enum class DefaultBonusAction { kBonusAttack = 0, kNone = 1, kFeat = 2 };
struct DefaultBonus {
  DefaultBonusAction kind = DefaultBonusAction::kBonusAttack;
  int feat = -1;  // kFeat: the ability's family (BonusFeatFamily)
};
DefaultBonus DecodeDefaultBonus(int record);  // 0 the bonus attack (never chosen), 1 none, the family + 2
int EncodeDefaultBonus(DefaultBonus choice);
// A Bonus Action feat's family: Rage's successors are Rage (293), Second Wind's
// are its first (1220); any other feat is its own.
int BonusFeatFamily(int feat);
// Pointless to fire: the healing ones (Second Wind, Wholeness of Body) at full
// hit points.
bool DefaultBonusPointless(int family, int hit_points, int max_hit_points);
// A round's plan: fire the chosen ability when the round's Bonus Action is free
// and the ability is ready (held, a use left, not already on - Rage - and not
// pointless); otherwise the automatic bonus attack (the ability's fallback, or
// the choice itself) - or none.
struct DefaultPlan {
  bool fire_feat = false;
  DefaultBonusAction attack = DefaultBonusAction::kBonusAttack;
};
DefaultPlan PlanDefaultBonus(DefaultBonus choice, bool bonus_used, bool feat_ready);

// A round's start: which automatic bonus attacks it keeps and whether they
// spend its Bonus Action. `used`: the round's Bonus Action already went (a
// waiting one clicked last round, or one used before the round began). An
// off-hand attack through Nick is part of the Attack action - always kept,
// never the Bonus Action.
struct RoundBonus {
  bool keep_off_hand = false;
  bool keep_martial_arts = false;
  bool spends_bonus = false;
};
RoundBonus DecideRoundBonus(bool used, DefaultBonusAction choice, bool off_hand, bool martial_arts, bool nick);

}  // namespace rules
