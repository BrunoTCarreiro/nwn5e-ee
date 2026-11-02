// A flourish inside the combat round (#121): wait for the creature's swing, pause its round for the clip, or play at
// once out of combat - data::DecideFlourish's order, and the pause that covers the clip.
#include "check.h"
#include "data/anim_flourish.h"

namespace {

using data::DecideFlourish;
using data::FlourishStep;
using data::FlourishTick;

// A creature mid-fight, between swings, the clip fitting: the case that pauses and plays.
FlourishTick Ready() {
  FlourishTick t;
  t.alive = true;
  t.wanted = true;
  t.has_round = true;
  t.round_started = true;
  t.paused = false;
  t.moving = false;
  t.fits = true;
  t.now_ms = 10000;
  t.not_before_ms = 9900;
  t.give_up_ms = 22000;
  return t;
}

int Step(const FlourishTick& t) { return static_cast<int>(DecideFlourish(t)); }
const int kWait = static_cast<int>(FlourishStep::Wait);
const int kPlayNow = static_cast<int>(FlourishStep::PlayNow);
const int kPause = static_cast<int>(FlourishStep::PauseAndPlay);
const int kDrop = static_cast<int>(FlourishStep::Drop);

}  // namespace

TEST(FlourishPausesWhenItFits) { CHECK_EQ(Step(Ready()), kPause); }

TEST(FlourishDropsWhenDeadOrUnwanted) {
  FlourishTick dead = Ready();
  dead.alive = false;
  FlourishTick unwanted = Ready();
  unwanted.wanted = false;
  // Even while it would otherwise wait (paused, before the gap): the drop comes first.
  FlourishTick waiting = Ready();
  waiting.paused = true;
  waiting.not_before_ms = 20000;
  waiting.wanted = false;
  CHECK_EQ(Step(dead), kDrop);
  CHECK_EQ(Step(unwanted), kDrop);
  CHECK_EQ(Step(waiting), kDrop);
}

TEST(FlourishDropsPastGiveUp) {
  FlourishTick late = Ready();
  late.give_up_ms = 9999;
  late.paused = true;
  CHECK_EQ(Step(late), kDrop);
  FlourishTick edge = Ready();
  edge.give_up_ms = 10000;  // at give-up exactly: still tries
  CHECK_EQ(Step(edge), kPause);
}

TEST(FlourishWaitsForTheSwapGap) {
  FlourishTick early = Ready();
  early.not_before_ms = 10050;  // the swap hasn't reached the client yet
  CHECK_EQ(Step(early), kWait);
  FlourishTick early_out = early;  // out of combat too
  early_out.has_round = false;
  CHECK_EQ(Step(early_out), kWait);
}

TEST(FlourishPlaysAtOnceOutOfCombat) {
  FlourishTick none = Ready();
  none.has_round = false;
  none.paused = true;  // a pause still counting down out of combat doesn't hold it
  FlourishTick idle = Ready();
  idle.round_started = false;
  idle.fits = false;
  CHECK_EQ(Step(none), kPlayNow);
  CHECK_EQ(Step(idle), kPlayNow);
}

TEST(FlourishWaitsWhilePaused) {
  FlourishTick swinging = Ready();  // its own swing, or an enemy's swing at it
  swinging.paused = true;
  CHECK_EQ(Step(swinging), kWait);
}

TEST(FlourishWaitsForAnEnemysSwingBeforeItsRound) {
  FlourishTick struck = Ready();  // not attacking yet, an enemy's swing pauses its round all the same
  struck.round_started = false;
  struck.paused = true;
  CHECK_EQ(Step(struck), kWait);
}

TEST(FlourishWaitsWhileMoving) {
  FlourishTick moving = Ready();
  moving.moving = true;
  CHECK_EQ(Step(moving), kWait);
}

TEST(FlourishWaitsForTheNextRoundWhileAttacking) {
  FlourishTick between = Ready();  // between two rounds of a fight: the next starts on its next update
  between.round_started = false;
  between.attacking = true;
  between.fits = true;  // even if the stale round would take the pause: the round's start clears it
  CHECK_EQ(Step(between), kWait);
}

TEST(FlourishWaitsWhileMovingOutOfCombat) {
  FlourishTick walking = Ready();  // the walk would overwrite it at once
  walking.has_round = false;
  walking.moving = true;
  CHECK_EQ(Step(walking), kWait);
}

TEST(FlourishWaitsWhenItDoesNotFit) {
  FlourishTick late = Ready();  // after the round's last swing: the next round's start clears the timer
  late.fits = false;
  CHECK_EQ(Step(late), kWait);
}

TEST(FlourishPauseCoversTheClip) {
  CHECK_EQ(data::FlourishPauseMs(1.47f), 1545);
  CHECK_EQ(data::FlourishPauseMs(3.46667f), 3542);
  CHECK_EQ(data::FlourishPauseMs(0.0f), 75);
  CHECK_EQ(data::FlourishPauseMs(-1.0f), 75);
}
