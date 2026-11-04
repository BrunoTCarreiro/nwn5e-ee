#pragma once

#include <stdint.h>

// nwn5e's own scripts, run in C++ (0.41.0; first light_sources.cpp's): a hook
// on CVirtualMachine::RunScript - every event script and every spell's impact
// script goes through it - runs the handler registered for a name instead of
// a compiled script. The names are nwn5e's ("n5_..."), named by served rows
// (a placeable's events, a spells.2da ImpactScript); no .ncs file exists for
// them - or the game's own a rule replaces (#124: x0_s3_acid ...; 0.69.2: the
// lookup took only "n5" names before). `self` is OBJECT_SELF (the placeable,
// the caster).
using ScriptHandler = void (*)(uint32_t self);

// Hooks RunScript on first use; false (and logged) if it can't be hooked, or
// if the hook's lookup wouldn't find the name (registered twice).
bool AddScriptHandler(const char* name, ScriptHandler handler);

// 0.73.0 (#131): `observer` runs after the script `name` has run - the game's
// own or a handler - with its OBJECT_SELF and the name as registered.
using ScriptObserver = void (*)(uint32_t self, const char* name);
bool AddScriptObserver(const char* name, ScriptObserver observer);
