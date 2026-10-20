#pragma once

// A creature's applied effects (EE layout, ee_layout.h), and the script
// bridge nwn5e uses to create effects and messages without building engine
// objects itself (docs/EE-PORT.md principle 5).

#include <stdint.h>

#include "creature.h"
#include "ee_layout.h"

namespace effects {

int Integer(const void* effect, int index);  // 0 past the effect's count
inline uint16_t Type(const void* effect) { return Field<uint16_t>(effect, ee::kEffectType); }
inline uint32_t Creator(const void* effect) { return Field<uint32_t>(effect, ee::kEffectCreator); }
inline float Duration(const void* effect) { return Field<float>(effect, ee::kEffectDuration); }
inline uint16_t Flags(const void* effect) { return Field<uint16_t>(effect, ee::kEffectFlags); }

// fn(effect) for each effect of `type` on `object`.
template <typename Fn>
void ForEachOfType(const void* object, uint16_t type, Fn fn) {
  if (!object) return;
  void* const* list = Field<void* const*>(object, ee::kObjectEffects);
  const int count = Field<int32_t>(object, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (effect && Type(effect) == type) fn(effect);
  }
}

bool HasType(const void* object, uint16_t type);
// A SetState effect of `state` (6 stunned, 8 paralysed, 9 asleep, ...),
// whatever higher state the creature also holds.
bool HasState(const void* object, int state);

}  // namespace effects

// Runs NWScript source as `self` (EE's ExecuteScriptChunk; the code is
// wrapped into a main()). Returns the engine's result (logged the first time
// it is seen, its meaning not yet known), or -1 when the bridge is missing.
int RunScriptChunk(const char* code, uint32_t self);

// Applies an NWScript effect (e.g. "EffectKnockdown()") to `target`, created
// by `creator` (the script runs as it), after `delay` seconds — always through
// DelayCommand, so never inside the engine's own effect handling.
// `duration_type` is an NWScript constant ("DURATION_TYPE_TEMPORARY").
void ApplyEffectAs(uint32_t creator, uint32_t target, const char* duration_type, const char* effect, float seconds,
                   float delay);
