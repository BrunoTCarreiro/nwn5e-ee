#pragma once

#include "rules/hud_layout.h"

// The resource panel (rules/hud_layout.h; 0.57.0): a class's pool for it, from
// its rules file (the Monk's Focus: rules_monk.cpp). Asked on the server's
// thread for the single-player character, several times a second - keep it
// cheap. False: the creature has none.
using HudPoolProvider = bool (*)(void* creature, rules::HudPool* pool);
void AddHudPool(HudPoolProvider provider);
// 0.86.0 (#37): a click on a pool block whose `click` is `action` (rules::HudPool) - the left button pressed
// over it on the client - runs `handler` for the single-player character on the server's thread.
using HudClickHandler = void (*)(void* creature);
void AddHudClick(const char* action, HudClickHandler handler);

void InstallResourcePanel();

// The panel open right under the action queue strip (0.57.4: the strip's leaving
// tiles then fade in place). The client's frame only.
bool ResourcePanelUnderStrip();
