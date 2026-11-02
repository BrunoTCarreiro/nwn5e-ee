#pragma once

// Phase 6 - the action economy (docs/PLAN.md "Phase 6 — Action economy";
// decided by the owner 2026-11-02, #51): per-round budgets on NWN's 6 s round -
// one Action, one Bonus Action, one Reaction. Part 1 (#52): the Bonus Action.
// Part 2 (#120): the Action queue and the default attack.

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

// Part 2 (#120; owner, 2026-11-02: "if the PC ever targeted an enemy for normal
// attacks, a normal attack should be placed as a default action to be used if
// there are no other actions in the queue"): what a player character attacks
// when its queue runs dry in a fight - the last enemy it was ordered to attack
// while that one stands (alive, an enemy, seen), else the nearest enemy it
// sees. Nothing out of combat, or when it wasn't ordered to attack an enemy
// since the fight began or its last move order.
enum class DefaultAttackPick { kNone, kLastTarget, kNearest };
DefaultAttackPick PickDefaultAttack(bool armed, bool in_combat, bool last_target_stands, bool nearest_found);
// How far the default attack looks for the nearest enemy: 10 m with a melee
// weapon (one it can walk to and reach, as the potion resume looked), a ranged
// weapon's [rules] ranged_retarget_range.
constexpr float kDefaultAttackMeleeRange = 10.0f;
float DefaultAttackRange(bool ranged_weapon, int ranged_retarget_range);

// The Action queue (#120; the design's point 1: "one a round in order"). NWN
// drops a player character's running attack - the rest of its round's attacks
// - the moment it's given a spell, an item or a feat (#120's recon: the order's
// ActionManager(1) clears the queue). In 2024 the round it attacks is its
// Action, so an order given mid-round keeps the attack: what NWN makes wait for
// the round's end (a spell, a scroll, a wand - WaitForEndOfRound) goes after
// the round's attacks, and at the round's end the attack steps aside for it;
// what doesn't (a potion, now a Bonus Action) goes before the attack, which
// carries on after it. Not attacking mid-round: as NWN.
// The player's orders that keep the attack (CNWSMessage's input minors): a
// spell 0x12, an item 0x09, a feat 0x06, a talent 0x23.
bool OrderKeepsAttack(int input_minor);
enum class OrderPlace { kAsNwn, kAfterAttacks, kBeforeAttacks };
OrderPlace PlaceOrder(bool attacking_mid_round, bool waits_for_round_end);
// At the round's end (none running): the attack yields to an Action waiting
// behind it.
bool AttackYields(bool round_running, bool action_waiting);

// Potions (#120; #51's decision 4 (i), SRD 5.2: drinking a potion takes a Bonus
// Action): NWN's potion base items (baseitems.2da 49 potions, 104 crafted
// potions; 54, which the 1.69 port also listed, is a deleted row in EE).
bool IsPotionBaseItem(int base_item);

}  // namespace rules
