#pragma once

#include <stdint.h>

// Listeners on CNWSCreature::BroadcastSpellCast: a creature's cast as it begins, once (AIActionCastSpell broadcasts it)
// (#150, 0.96.7). One hook, rules_skill_dcs.cpp's (Spellcraft's own, `skills` faithful), installed on first use by
// either; the listeners are told before the cast is broadcast.
using SpellCastListener = void (*)(void* creature, uint32_t spell);
// False (logged) when the hook can't be installed.
bool AddSpellCastListener(SpellCastListener listener);
