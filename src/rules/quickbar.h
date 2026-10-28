#pragma once

// The quickbar (docs/PLAN.md "Quickbar"; owner, 2026-10-27: "do the 1-9 hook
// plus auto-placing new actions" - so the player needn't "dig through 4 layers
// of radial menus to set the strike they want to use"). Faithful only.
// - nwn5e's class actions go on a player's quickbar the first time the
//   character has them: the first empty slot, the number keys' first; nothing
//   is ever overwritten, an action already there is left alone, and one the
//   player took off stays off (a record per character).
// - Number keys 1-9 press the first nine slots, as F1-F9 do (the client;
//   core/rules_quickbar.cpp).
// A slot is the engine's CNWSQuickbarButton: 36 of them - F1-F12, Shift+F1-F12,
// Ctrl+F1-F12; a feat button (type 4) holds one number, the feat, or for one
// option of a feat's sub-radial that option's spells.2da FeatID.

#include <string>
#include <vector>

namespace rules {

constexpr int kQuickbarSlots = 36;
constexpr int kQuickbarTypeEmpty = 0;
constexpr int kQuickbarTypeFeat = 4;
constexpr int kQuickbarNumberKeys = 9;  // keys 1-9: slots 0-8

// A feat button's number for one option of a feat's sub-radial: the option's
// spells.2da FeatID, option * 65536 + the feat (Brutal Strike's options are
// 16000-16004, data/rows/spells.barbarian.txt).
constexpr int QuickbarFeatValue(int feat, int option) { return option * 65536 + feat; }

// One action nwn5e puts on the quickbar.
struct QuickbarAction {
  int record_bit;    // its bit in the character's record - fixed, new actions take new bits
  int value;         // the feat button's number
  const char* name;  // for the chat line
};

// What the character has, of what the table asks about.
struct QuickbarFacts {
  std::vector<int> feats;   // the feats held, of QuickbarActionFeats2024()
  int barbarian_level = 0;  // Brutal Strike's options come by level
};

// The feats the table asks about (so the caller tests only these).
const std::vector<int>& QuickbarActionFeats2024();

// The actions the character has now, in the order they're placed: Second
// Wind; Rage, Reckless Attack, Brutal Strike's options (Forceful and Hamstring
// Blow from 9, Staggering and Sundering from 13, Off), Intimidating Presence;
// Cunning Strike's options (Poison, Trip, Withdraw, Off, Combat options) and
// Devious Strikes' (Daze, Knock Out, Obscure); then everyone's Shove, Shove:
// push and Grapple.
std::vector<QuickbarAction> QuickbarActions2024(const QuickbarFacts& facts);

// Second Wind is three feats (its uses grow with Fighter level): a button
// holding any of them is the one action.
bool SameQuickbarAction(int value_a, int value_b);

struct QuickbarSlot {
  int type = kQuickbarTypeEmpty;
  int value = 0;  // a feat button's number
};

struct QuickbarChange {
  int slot;
  int value;
  int record_bit;
  bool refresh;  // a button of the action rewritten (Second Wind after a level), not a new one
};

// The buttons to write, and the record after them (*placed):
// - an action whose bit is set was placed before: not again (the player may
//   have taken it off) - but a button of it holding an older feat (Second
//   Wind 2 after Fighter 4) is rewritten in place;
// - an action already on the quickbar (any slot) is only recorded;
// - else it goes in the first empty slot, in slot order (F1 / key 1 first);
//   with no slot free it waits (not recorded).
std::vector<QuickbarChange> PlanQuickbar(const std::vector<QuickbarSlot>& slots, int placed,
                                         const std::vector<QuickbarAction>& actions, int* new_placed);

// The label drawn on a slot (0.48.2): "1".."9" then "F10".."F12" on the first
// two rows with the number keys (else "F1".."F12"), "F1".."F12" on the Ctrl
// row - no "Shift-" / "Ctrl-": a row shows only while its key is held, and the
// label's box fits about three characters.
std::string QuickbarSlotLabel(int slot, bool number_keys);

// The keys that press a slot: "1 / F1" ... "9 / F9" with the number keys
// (else "F1"), "F10" - "F12", "Shift+1 / Shift+F1" ... "Shift+F12" (Shift
// shows the second row), "Ctrl+F1" - "Ctrl+F12".
std::string QuickbarSlotKeys(int slot, bool number_keys);

}  // namespace rules
