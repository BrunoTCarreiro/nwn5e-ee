#include "check.h"
#include "rules/emergency_search.h"

using namespace rules;

// The emergency search. Breaks caught: a player's character or a creature that sees the source searching urgently,
// no damage starting it, a friend's harm starting it, the fade off by its edge, a stale heard spot gone to, a fresh
// one ignored, the shout reaching past its range or a creature that can't hear, the glances not right round or not
// quick, the keys not made sane.
TEST(EmergencySearchTimingAndChoices) {
  const EmergencyTuning t;
  CHECK_EQ(HarmStartsEmergency(true, false, 5, true, false, true), true);
  CHECK_EQ(HarmStartsEmergency(false, false, 5, true, false, true), false);
  CHECK_EQ(HarmStartsEmergency(true, true, 5, true, false, true), false);
  CHECK_EQ(HarmStartsEmergency(true, false, 0, true, false, true), false);
  CHECK_EQ(HarmStartsEmergency(true, false, 5, false, false, true), false);
  CHECK_EQ(HarmStartsEmergency(true, false, 5, true, true, true), false);
  CHECK_EQ(HarmStartsEmergency(true, false, 5, true, false, false), false);
  CHECK_EQ(EmergencyActive(20000, 10000, t), true);
  CHECK_EQ(EmergencyActive(21999, 10000, t), true);
  CHECK_EQ(EmergencyActive(22000, 10000, t), false);
  CHECK_EQ(EmergencyActive(5000, 0, t), false);
  CHECK_EQ(EmergencyMoveFor(20000, 9000, t) == EmergencyMove::kToHeard, true);
  CHECK_EQ(EmergencyMoveFor(20000, 8000, t) == EmergencyMove::kToHeard, true);
  CHECK_EQ(EmergencyMoveFor(20000, 7999, t) == EmergencyMove::kScanInPlace, true);
  CHECK_EQ(EmergencyMoveFor(20000, 0, t) == EmergencyMove::kScanInPlace, true);
  CHECK_EQ(AlertsAlly(19.5f, true, true, false, false, t), true);
  CHECK_EQ(AlertsAlly(20.5f, true, true, false, false, t), false);
  CHECK_EQ(AlertsAlly(5.0f, false, true, false, false, t), false);
  CHECK_EQ(AlertsAlly(5.0f, true, false, false, false, t), false);
  CHECK_EQ(AlertsAlly(5.0f, true, true, true, false, t), false);
  CHECK_EQ(AlertsAlly(5.0f, true, true, false, true, t), false);
  const GlanceTuning g = EmergencyGlance(t);
  CHECK_EQ(g.max_degrees, 180);
  CHECK_EQ(g.min_degrees, 90);
  CHECK_EQ(g.min_ms, 800);
  CHECK_EQ(g.max_ms, 1200);
  EmergencyTuning wild;
  wild.glance_ms = 50;
  wild.alert_metres = -3.0f;
  wild.fade_ms = 500;
  const EmergencyTuning sane = SaneEmergency(wild);
  CHECK_EQ(sane.glance_ms == 300 && sane.alert_metres == 0.0f && sane.fade_ms == 2000, true);
  wild.alert_metres = 500.0f;
  CHECK_EQ(SaneEmergency(wild).alert_metres == 60.0f, true);
}
