#pragma once

#include <stdint.h>

// nwn5e's own scripts, run in C++ (0.41.0; first light_sources.cpp's): a hook
// on CVirtualMachine::RunScript - every event script and every spell's impact
// script goes through it - runs the handler registered for a name instead of
// a compiled script. The names are nwn5e's ("n5_..."), named by served rows
// (a placeable's events, a spells.2da ImpactScript); no .ncs file exists for
// them. `self` is OBJECT_SELF (the placeable, the caster).
using ScriptHandler = void (*)(uint32_t self);

// Hooks RunScript on first use; false (and logged) if it can't be hooked.
bool AddScriptHandler(const char* name, ScriptHandler handler);
