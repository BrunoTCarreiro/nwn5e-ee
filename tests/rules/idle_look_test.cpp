#include <math.h>

#include "check.h"
#include "rules/idle_look.h"

using namespace rules;

// #9 d. Breaks caught: the defaults off the ini's, a pair given high first left so, the waits or angles outside
// their range or not spread over it, the shortest wait under the hold, a turn to the wrong side, the hold unclamped.
TEST(IdleLookTuningAndRolls) {
  const IdleLookTuning def;
  CHECK_EQ(def.min_seconds, 8);
  CHECK_EQ(def.max_seconds, 16);
  CHECK_EQ(def.min_degrees, 40);
  CHECK_EQ(def.max_degrees, 120);
  CHECK_EQ(def.hold_seconds, 3);
  const IdleLookTuning same = SaneIdleLook(def);
  CHECK_EQ(same.min_seconds == 8 && same.max_seconds == 16 && same.min_degrees == 40 && same.max_degrees == 120 &&
               same.hold_seconds == 3,
           true);
  IdleLookTuning raw;
  raw.min_seconds = 20;
  raw.max_seconds = 5;      // given high first
  raw.min_degrees = 200;
  raw.max_degrees = 1;
  raw.hold_seconds = 0;
  IdleLookTuning sane = SaneIdleLook(raw);
  CHECK_EQ(sane.min_seconds, 5);
  CHECK_EQ(sane.max_seconds, 20);
  CHECK_EQ(sane.min_degrees, 5);
  CHECK_EQ(sane.max_degrees, 180);
  CHECK_EQ(sane.hold_seconds, 1);
  raw = IdleLookTuning();
  raw.min_seconds = 1;
  raw.max_seconds = 1;
  raw.hold_seconds = 99;
  sane = SaneIdleLook(raw);
  CHECK_EQ(sane.hold_seconds, 30);
  CHECK_EQ(sane.min_seconds, 31);   // a look is over before the next
  CHECK_EQ(sane.max_seconds, 31);
  raw = IdleLookTuning();
  raw.min_seconds = 1000;
  raw.max_seconds = 2000;
  sane = SaneIdleLook(raw);
  CHECK_EQ(sane.min_seconds, 600);
  CHECK_EQ(sane.max_seconds, 600);
  CHECK_EQ(IdleLookWaitMs(def, 0.0), 8000);
  CHECK_EQ(IdleLookWaitMs(def, 0.5), 12000);
  CHECK_EQ(IdleLookWaitMs(def, 0.9999999), 16000);
  CHECK_EQ(IdleLookWaitMs(def, 7.0), 16000);
  CHECK_EQ(IdleLookWaitMs(def, -1.0), 8000);
  CHECK_EQ(IdleLookTurn(def, 0.0, true) == 40.0f, true);
  CHECK_EQ(IdleLookTurn(def, 0.5, true) == 80.0f, true);
  CHECK_EQ(IdleLookTurn(def, 0.5, false) == -80.0f, true);
  CHECK_EQ(fabsf(IdleLookTurn(def, 0.9999999, false) + 120.0f) < 0.01f, true);
  CHECK_EQ(FacingDegrees(370.0f) == 10.0f, true);
  CHECK_EQ(FacingDegrees(-30.0f) == 330.0f, true);
  CHECK_EQ(FacingDegrees(90.0f) == 90.0f, true);
  CHECK_EQ(FacingDegrees(-720.0f) == 0.0f, true);
  CHECK_EQ(FacingDegrees(360.0f) == 0.0f, true);
}

// Breaks caught: any one of the blockers ignored, the order changed (a fighting associate is an associate), the
// reach's edge out, a moved post not noticed or a small shuffle taken as one.
TEST(IdleLookOnlyCalmHostilesAtTheirPost) {
  IdleLookFacts calm;
  calm.metres = 12.0f;
  CHECK_EQ(IdleLookBlocker(calm) == IdleLookBlock::kNone, true);
  const auto with = [&](void (*set)(IdleLookFacts&)) {
    IdleLookFacts f = calm;
    set(f);
    return IdleLookBlocker(f);
  };
  CHECK_EQ(with([](IdleLookFacts& f) { f.player = true; }) == IdleLookBlock::kPlayer, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.associate = true; }) == IdleLookBlock::kAssociate, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.dead = true; }) == IdleLookBlock::kDead, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.hostile = false; }) == IdleLookBlock::kNotHostile, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.fighting = true; }) == IdleLookBlock::kFighting, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.searching = true; }) == IdleLookBlock::kSearching, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.investigating = true; }) == IdleLookBlock::kInvestigating, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.hiding = true; }) == IdleLookBlock::kHiding, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.busy = true; }) == IdleLookBlock::kBusy, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.metres = 40.5f; }) == IdleLookBlock::kFar, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.metres = 40.0f; }) == IdleLookBlock::kNone, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.associate = true; f.fighting = true; }) == IdleLookBlock::kAssociate, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.fighting = true; f.busy = true; }) == IdleLookBlock::kFighting, true);
  CHECK_EQ(with([](IdleLookFacts& f) { f.investigating = true; f.busy = true; }) == IdleLookBlock::kInvestigating,
           true);
  CHECK_EQ(IdleLookMovedOff(10.0f, 10.0f, 11.0f, 11.0f), false);   // 1.41 m: a shuffle
  CHECK_EQ(IdleLookMovedOff(10.0f, 10.0f, 11.2f, 11.0f), true);    // 1.56 m: moved
  CHECK_EQ(IdleLookMovedOff(10.0f, 10.0f, 8.4f, 10.0f), true);
  CHECK_EQ(IdleLookMovedOff(10.0f, 10.0f, 10.0f, 10.0f), false);
}
