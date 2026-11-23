#include "rules/emergency_search.h"

namespace rules {

namespace {
int ClampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
}  // namespace

EmergencyTuning SaneEmergency(EmergencyTuning tuning) {
  tuning.glance_ms = ClampInt(tuning.glance_ms, 300, 5000);
  if (!(tuning.alert_metres >= 0.0f)) tuning.alert_metres = 0.0f;
  if (tuning.alert_metres > 60.0f) tuning.alert_metres = 60.0f;
  tuning.fade_ms = ClampInt(tuning.fade_ms, 2000, 120000);
  tuning.heard_ms = ClampInt(tuning.heard_ms, 0, 120000);
  return tuning;
}

bool HarmStartsEmergency(bool on, bool victim_is_player, int damage, bool source_hiding, bool victim_sees_source,
                         bool hostile) {
  return on && !victim_is_player && damage > 0 && source_hiding && !victim_sees_source && hostile;
}

bool EmergencyActive(int64_t now, int64_t last_harm, const EmergencyTuning& tuning) {
  return last_harm > 0 && now >= last_harm && now - last_harm < tuning.fade_ms;
}

EmergencyMove EmergencyMoveFor(int64_t now, int64_t heard_at, const EmergencyTuning& tuning) {
  return heard_at > 0 && now >= heard_at && now - heard_at <= tuning.heard_ms ? EmergencyMove::kToHeard
                                                                              : EmergencyMove::kScanInPlace;
}

bool AlertsAlly(float metres, bool hostile_to_hider, bool hears, bool is_player, bool already,
                const EmergencyTuning& tuning) {
  return metres <= tuning.alert_metres && hostile_to_hider && hears && !is_player && !already;
}

GlanceTuning EmergencyGlance(const EmergencyTuning& tuning) {
  GlanceTuning glance;
  glance.min_degrees = 90;
  glance.max_degrees = 180;
  glance.min_ms = tuning.glance_ms * 4 / 5;
  glance.max_ms = tuning.glance_ms * 6 / 5;
  return SaneGlance(glance);
}

}  // namespace rules
