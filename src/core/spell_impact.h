#pragma once

#include <string>
#include <vector>

// The moment a spell takes effect, for nwn5e's spell rules (0.87.0: rules_concentration.cpp's 0.86 code, shared
// with #38's material components). CVirtualMachine::RunScript runs a spell's impact script (spells.2da's
// ImpactScript) with its caster as OBJECT_SELF and the spell in the caster's last-spell field (+0x218, GetSpellId's);
// a RunScript start listener (script_hooks.h) sees it begin - before its effects land. The ImpactScript names are
// read once a game by the script bridge (a module may serve its own spells.2da), at the player character's first
// update.

// `listener` hears `caster` (a creature) about to take effect with one of `spells`. False if not hooked (logged).
using SpellImpactListener = void (*)(void* caster, int spell);
bool AddSpellImpactListener(const std::vector<int>& spells, SpellImpactListener listener);

// A game started (CNWSModule::LoadModuleFinish: a new game, or a saved one loaded): the last game's state goes.
using GameStartListener = void (*)();
bool AddGameStartListener(GameStartListener listener);

// The base item (baseitems.2da row) of the item `caster` casts its spell from; -1: not from an item.
int SpellCastItemBase(void* caster);

// A spell's name (CNWSpell::GetSpellNameText); "spell N" when unknown.
std::string SpellNameOf(int spell);
