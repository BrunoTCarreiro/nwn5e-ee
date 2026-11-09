#pragma once

// CNWSCreature::AIActionCastSpell - every tick of a creature's cast. One hook, rules_conditions.cpp's (CN8: a deafened
// caster's verbal spells don't fail, `conditions` faithful), installed on first use by either (2026-11-09: casting
// from hiding, rules_perception.cpp). False (logged) when it can't be.
bool HookCastAction();
// The spell (spells.2da row) of the cast AIActionCastSpell runs on this thread now; -1 outside one.
int CastingSpellNow();
