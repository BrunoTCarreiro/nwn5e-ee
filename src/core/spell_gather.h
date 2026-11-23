#pragma once

// #166 / #184, decision A (owner, 2026-11-23: "A"): NWN's own unconverted area spells follow the 2024 rule - a creature
// behind Total Cover from the spell's point of origin isn't in its area (SRD 5.2.1, Areas of Effect), as nwn5e's own
// leave it out. The spell's own gather is where: the NWScript GetFirstObjectInShape / GetNextObjectInShape (one engine
// command, CNWSVirtualMachineCommands::ExecuteCommandGetObjectInShape - 128 the first, 129 the next) while that spell's
// impact script runs on its caster. Such a creature is skipped there - the next one in the shape handed back instead -
// so the script never sees it: no save, no effect. Logged once a cast ("cover: 0x... left out of Fireball (58)'s area -
// Total Cover from its point of origin (plc_statue1)"), both told once.
// - Which runs: a script start whose name is the ImpactScript of its OBJECT_SELF's last spell, that spell an NWN area
//   spell (rules::NwnAreaSpell: spells.2da TargetShape cone, sphere, hsphere, cube, rectangle; not nwn5e's own). Each
//   script run is bracketed (script_hooks.h's start and end listeners), so a script it runs in turn (a module's spell
//   hook, an ExecuteScript) is its own entry and isn't gathered; a lookup by another object, or in any other script,
//   is the engine's own. A lasting area's scripts (its area object's) aren't impact scripts: unchanged.
// - The test is the cover rule's (object_cover.cpp LeftOutOfSpellGather): the spell's point of origin (SpellOriginOf:
//   a cone's or a line's from the caster's eyes, a sphere's 1 m over its point, a falling column's top) and the seven
//   body points' lines - the engine's walls and shut doors, and the placeables' boxes (not for a lasting area under
//   areas_fill_around). Only creatures, never the caster. Half and Three-Quarters stay in (+2 / +5 on a Dexterity save);
//   Total Cover's +5 is gone where this applies (rules::AreaSaveCoverBonus).
// cover_objects=1, the faithful rules.

#include <stdint.h>

// The hook and the listeners (InstallPerceptionRules, after the cover's table).
void InstallSpellGather();
// Whether `self` is running NWN's area spell `spell`'s impact script now (the gather's scope).
bool SpellGatherRunning(uint32_t self, int spell);
