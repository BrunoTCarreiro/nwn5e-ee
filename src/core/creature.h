#pragma once

// What the rule hooks need to know about a creature (EE layout: ee_layout.h;
// exported getters where EE has them), and the attack-bonus conversion they
// share (port of 1.69's creature.h).

#include <stdint.h>

#include <vector>

#include "rules/nwn_conversion.h"

template <typename T>
T Field(const void* object, size_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) + offset);
}

template <typename T>
void SetField(void* object, size_t offset, T value) {
  *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset) = value;
}

struct CreatureFacts {
  bool is_player = false;
  int associate_type = 0;
  bool character = false;  // player character or henchman: character rules (decision 11)
  int level = 0;
  float challenge_rating = 0.0f;
  bool plausible = false;  // characters always; others when the CR read is plausible
};

void* CreatureOf(void* stats);     // CNWSCreatureStats -> CNWSCreature (null if unknown)
void* StatsOf(void* creature);     // CNWSCreature -> CNWSCreatureStats (null if unknown)
uint32_t ObjectIdOf(void* stats);  // object id of the creature owning these stats (0 when unknown)

// The creature with this object id, or null (not a creature).
void* CreatureById(uint32_t id);

// Reads the facts; logs (once per creature) a challenge rating that is not plausible.
CreatureFacts ReadCreatureFacts(void* stats);

// "PC", "henchman" or "creature", for the log.
const char* KindOf(const CreatureFacts& facts);

// Class slots (classes.2da row + levels), at most 8 in EE.
std::vector<rules::NwnClassSlot> ReadClassSlots(void* stats);

// Levels in a 2024 class, prestige classes counted as their parent (decision 10).
int ClassLevel2024(void* stats, rules::CharacterClass character_class);

// An engine attack bonus built on GetBaseAttackBonus(0), converted to 2024:
// minus the 3e BAB, plus the proficiency bonus. Returns the engine value when
// the creature's facts are not plausible.
int ConvertEngineAttackBonus(void* stats, int engine_value, const CreatureFacts& facts, int* base_attack_bonus,
                             int* proficiency_bonus);
