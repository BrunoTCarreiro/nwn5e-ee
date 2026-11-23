#pragma once

// When the 2024 Hide ends (#9, #33 T3; owner, 2026-11-23: "yes to both, go with your recommendations"). SRD 5.2.1,
// Hide: "You stop being hidden immediately after any of the following occurs: you make a sound louder than a whisper,
// an enemy finds you, you make an attack roll, or you cast a spell with a Verbal component." Not modelled: a sound
// louder than a whisper. Thief 9, Supreme Sneak: "Stealth Attack (Cost: 1d6). If you have the Hide action's Invisible
// condition, this attack doesn't end that condition on you if you end the turn behind Three-Quarters Cover or Total
// Cover." This file only decides.

#include "rules/cover.h"

namespace rules {

enum class HideAct { kAttackRoll, kCast };
struct HideActFacts {
  bool on = true;              // attack_ends_hiding (and the faithful rules)
  bool hiding = false;         // in stealth mode as it acts
  HideAct act = HideAct::kAttackRoll;
  bool verbal = false;         // a cast: the spell has a Verbal component (spells.2da VS)
  bool without_verbal = false; // a cast made without its Verbal component (Subtle Spell - not in nwn5e yet: the hook)
  bool stealth_attack = false; // an attack roll with Supreme Sneak's Stealth Attack among its Cunning Strike
};
// kEngine: not nwn5e's to decide (attack_ends_hiding=0: NWN's own - stealth mode ends as a swing or any cast starts).
// kStays: still hidden. kEndsAfter: hidden through the attack roll (its Advantage) or the cast, out of hiding right
// after. kAtTurnEnd: decided at the end of the turn (Stealth Attack: SupremeSneakKeeps).
enum class HideAfter { kEngine, kNotHiding, kStays, kEndsAfter, kAtTurnEnd };
HideAfter HideAfterAct(const HideActFacts& facts);

// At the end of the turn of a Stealth Attack: the least cover the hider has from the enemies who could see it (the
// SRD names no one; nwn5e reads it as from each of them). Hidden still behind Three-Quarters or Total Cover.
bool SupremeSneakKeeps(Cover least_cover);
// The least of two covers (the one an enemy sees the most of you through).
Cover LessCover(Cover a, Cover b);

// Supreme Sneak: Thief 9 (the SRD's one Rogue subclass, as Thief's Reflexes).
bool HasSupremeSneak(int rogue_level);

}  // namespace rules
