#include "rules/idle_look.h"

#include <math.h>

namespace rules {

namespace {
int Clamp(int value, int low, int high) { return value < low ? low : (value > high ? high : value); }
double Unit(double roll) { return roll < 0.0 ? 0.0 : (roll >= 1.0 ? 0.999999 : roll); }
}  // namespace

IdleLookTuning SaneIdleLook(IdleLookTuning tuning) {
  if (tuning.min_seconds > tuning.max_seconds) {
    const int swap = tuning.min_seconds;
    tuning.min_seconds = tuning.max_seconds;
    tuning.max_seconds = swap;
  }
  if (tuning.min_degrees > tuning.max_degrees) {
    const int swap = tuning.min_degrees;
    tuning.min_degrees = tuning.max_degrees;
    tuning.max_degrees = swap;
  }
  tuning.hold_seconds = Clamp(tuning.hold_seconds, 1, 30);
  tuning.min_seconds = Clamp(tuning.min_seconds, 2, 600);
  tuning.max_seconds = Clamp(tuning.max_seconds, 2, 600);
  if (tuning.min_seconds < tuning.hold_seconds + 1) tuning.min_seconds = tuning.hold_seconds + 1;
  if (tuning.max_seconds < tuning.min_seconds) tuning.max_seconds = tuning.min_seconds;
  tuning.min_degrees = Clamp(tuning.min_degrees, 5, 180);
  tuning.max_degrees = Clamp(tuning.max_degrees, 5, 180);
  return tuning;
}

int IdleLookWaitMs(const IdleLookTuning& tuning, double roll) {
  const double seconds = tuning.min_seconds + (tuning.max_seconds - tuning.min_seconds) * Unit(roll);
  return static_cast<int>(seconds * 1000.0 + 0.5);
}

float IdleLookTurn(const IdleLookTuning& tuning, double roll, bool left) {
  const float degrees =
      static_cast<float>(tuning.min_degrees + (tuning.max_degrees - tuning.min_degrees) * Unit(roll));
  return left ? degrees : -degrees;
}

float FacingDegrees(float degrees) {
  float d = fmodf(degrees, 360.0f);
  if (d < 0.0f) d += 360.0f;
  if (d >= 360.0f) d -= 360.0f;
  return d;
}

IdleLookBlock IdleLookBlocker(const IdleLookFacts& facts) {
  if (facts.player) return IdleLookBlock::kPlayer;
  if (facts.associate) return IdleLookBlock::kAssociate;
  if (facts.dead) return IdleLookBlock::kDead;
  if (!facts.hostile) return IdleLookBlock::kNotHostile;
  if (facts.fighting) return IdleLookBlock::kFighting;
  if (facts.searching) return IdleLookBlock::kSearching;
  if (facts.investigating) return IdleLookBlock::kInvestigating;
  if (facts.hiding) return IdleLookBlock::kHiding;
  if (facts.busy) return IdleLookBlock::kBusy;
  if (facts.metres > kIdleLookReach) return IdleLookBlock::kFar;
  return IdleLookBlock::kNone;
}

const char* IdleLookBlockName(IdleLookBlock block) {
  switch (block) {
    case IdleLookBlock::kNone: return "looks about";
    case IdleLookBlock::kPlayer: return "a player character";
    case IdleLookBlock::kAssociate: return "someone's associate";
    case IdleLookBlock::kDead: return "dead or dying";
    case IdleLookBlock::kNotHostile: return "not hostile";
    case IdleLookBlock::kFighting: return "fighting";
    case IdleLookBlock::kSearching: return "searching";
    case IdleLookBlock::kInvestigating: return "investigating a noise";
    case IdleLookBlock::kHiding: return "hiding";
    case IdleLookBlock::kBusy: return "busy (an action: walking, sitting ...)";
    case IdleLookBlock::kFar: return "too far";
  }
  return "?";
}

bool IdleLookMovedOff(float post_x, float post_y, float x, float y) {
  const float dx = x - post_x, dy = y - post_y;
  return dx * dx + dy * dy > 1.5f * 1.5f;
}

}  // namespace rules
