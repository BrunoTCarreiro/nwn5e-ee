#pragma once

// The light gem (#9 a; owner, 2026-11-15: "Sure, let's trial a light gem"): while the player character is in stealth
// mode, a small card on the HUD shows the light where it stands - Bright Light, Dim Light or Darkness, the SRD 5.2.1's
// terms - from the estimate the stealth rules use (core/stealth_watch.cpp's nwn5e_light_level, the 2024 light rule:
// stealth_light). A trial (light_gem). This file only decides; core/light_gem.cpp draws it.

#include "rules/hud_layout.h"

namespace rules {

enum class GemLight { kNone, kDark, kDim, kBright };

// From the stored level (local nwn5e_light_level: 0 not worked out, 1 dark, 2 dim, 3 bright).
GemLight GemLightOf(int stored_level);

// Whether the gem shows: its key on, the character in stealth mode, its light worked out.
bool LightGemShown(bool enabled, bool hiding, GemLight light);

// Its words' key in data/text/en.txt ("light_gem.bright" ...; the SRD's terms), "" for none.
const char* LightGemTextKey(GemLight light);

// The gem's colour (0-255): a warm white in bright light, a muted grey-gold in dim, a deep blue-black in darkness.
struct GemColour {
  int r = 0, g = 0, b = 0, a = 0;
};
GemColour LightGemColour(GemLight light);

// The card (GUI units): the gem and the words.
constexpr float kLightGemWidth = 104.0f;
constexpr float kLightGemHeight = 18.0f;

// Its window's spot: right above the clock's (bottom right, over the compass - rules/hud_clock.h), its card's right
// edge on the clock card's and `gap` GUI units over its top, so it follows the clock wherever that is put. Windows
// `gem_w` x `gem_h` and `clock_w` x `clock_h` GUI units, each card `inset` in from its window's edge; the clock's
// spot as rules::PlaceClock takes it (its centre `clock_from_right`, its bottom `clock_from_bottom`). Kept on screen
// as PlaceClock keeps it, but for `overhang`.
HudBox PlaceLightGem(float screen_w, float screen_h, float scale, float gem_w, float gem_h, float clock_from_right,
                     float clock_from_bottom, float clock_w, float clock_h, float inset, float gap, float overhang);

}  // namespace rules
