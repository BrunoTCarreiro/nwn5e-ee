#pragma once

#include "rules/hud_layout.h"

// The resource panel (rules/hud_layout.h; 0.57.0): a class's pool for it, from
// its rules file (the Monk's Focus: rules_monk.cpp). Asked on the server's
// thread for the single-player character, several times a second - keep it
// cheap. False: the creature has none.
using HudPoolProvider = bool (*)(void* creature, rules::HudPool* pool);
void AddHudPool(HudPoolProvider provider);

void InstallResourcePanel();

// The panel open right under the action queue strip (0.57.4: the strip's leaving
// tiles then fade in place). The client's frame only.
bool ResourcePanelUnderStrip();
