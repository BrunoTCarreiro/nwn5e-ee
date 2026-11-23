#pragma once

// The emergency search (#9; owner, 2026-11-23: "SHould we implement a "emergency" search mode? In this case the enemy
// can't target the weapon but they're still taking damage, the most they can do is try to find me as quickly as
// possible" - then "yes to both, go with your recommendations"): a creature harmed by a hider it can't find - damage
// whose source is a creature still hidden (Heat Metal's Again, a moved Moonbeam or Flaming Sphere, a trap ...) -
// searches urgently: an active Perception roll each round inside its cone (searching: its d20), quicker and wider
// glances (held about glance_ms, up to turning right round), heading to where it last heard the hider when that was
// within heard_ms (else scanning in place - it never simply knows where the hider is), and a shout that sets the
// hostile creatures within alert_metres that can hear it searching. It lasts while the harm goes on and fades back to
// a normal search fade_ms after the last. nwn5e.ini: emergency_search, emergency_search_glance_ms,
// emergency_search_alert_metres, emergency_search_fade_seconds. This file only decides.

#include <stdint.h>

#include "rules/search_glance.h"

namespace rules {

struct EmergencyTuning {
  int glance_ms = 1000;        // each glance held about this long
  float alert_metres = 20.0f;  // the shout's reach
  int fade_ms = 12000;         // after the last harm, back to a normal search
  int heard_ms = 12000;        // a heard spot this fresh is gone to; older, it scans in place
};
// The keys as read, made sane: glance 300-5000 ms, alert 0-60 m, fade 2-120 s.
EmergencyTuning SaneEmergency(EmergencyTuning tuning);

// Whether harm starts (or keeps up) an emergency search: `damage` taken (> 0) from a source creature that is hiding
// and that the harmed one doesn't see, and that is hostile to it; never a player's own character (it isn't the AI's).
bool HarmStartsEmergency(bool on, bool victim_is_player, int damage, bool source_hiding, bool victim_sees_source,
                         bool hostile);
// Whether it's still searching urgently `now` (ms), its last harm at `last_harm` (0: never).
bool EmergencyActive(int64_t now, int64_t last_harm, const EmergencyTuning& tuning);
// Where it goes: to the spot where it heard the hider at `heard_at` (ms; 0: never) when that's fresh, else nowhere.
enum class EmergencyMove { kToHeard, kScanInPlace };
EmergencyMove EmergencyMoveFor(int64_t now, int64_t heard_at, const EmergencyTuning& tuning);
// Whether its shout sets another creature searching: within alert_metres, hostile to the hider, able to hear, not a
// player's character, not searching urgently already.
bool AlertsAlly(float metres, bool hostile_to_hider, bool hears, bool is_player, bool already,
                const EmergencyTuning& tuning);
// Its glances: 90 up to 180 degrees (right round), each held 0.8-1.2 x glance_ms.
GlanceTuning EmergencyGlance(const EmergencyTuning& tuning);

}  // namespace rules
