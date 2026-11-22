#pragma once

// Idle guards looking about (#9 d; owner, 2026-11-15: "yes, sounds fun but we have to tune this"). While a player
// character hides, a calm hostile near it turns now and then to look about - some degrees left or right of the way
// it faces at its post, holds the look, and turns back - so its vision cone sweeps and a hider must time its moves
// (core/stealth_watch.cpp). Tuned in play: nwn5e.ini's idle_look keys. This file only decides; the engine side reads
// the creature and turns it.

namespace rules {

struct IdleLookTuning {
  int min_seconds = 8;    // between one creature's looks: a random time from min to max
  int max_seconds = 16;
  int min_degrees = 40;   // how far it turns, left or right: a random angle from min to max
  int max_degrees = 120;
  int hold_seconds = 3;   // how long it holds the look before turning back
};

// The keys as read, made sane: the seconds 2-600 and the degrees 5-180 (a pair given high first is swapped), the
// hold 1-30 s, and the shortest wait at least the hold + 1 s (a look is over before the next one).
IdleLookTuning SaneIdleLook(IdleLookTuning tuning);

// The wait until a creature's next look, in ms: min .. max seconds by `roll` (0 up to 1).
int IdleLookWaitMs(const IdleLookTuning& tuning, double roll);

// How far it turns, in degrees: min .. max by `roll` (0 up to 1); anticlockwise (left) +, clockwise (right) -.
float IdleLookTurn(const IdleLookTuning& tuning, double roll, bool left);

// A facing as SetFacing takes it: degrees anticlockwise from east, 0 up to 360.
float FacingDegrees(float degrees);

// What keeps a creature from looking about now, the first that applies in this order.
enum class IdleLookBlock {
  kNone,           // it may look about
  kPlayer,         // a player character or a Dungeon Master
  kAssociate,      // someone's henchman, summon, familiar or companion
  kDead,           // dead or dying
  kNotHostile,     // not an enemy of the hider
  kFighting,       // in combat
  kSearching,      // in detect mode (it sees all round anyway)
  kInvestigating,  // walking to a noise it heard (stealth watch) - or back
  kHiding,         // in stealth mode itself
  kBusy,           // an action under way or queued: walking a route, sitting, a conversation's ...
  kFar,            // more than kIdleLookReach from the hider
};
struct IdleLookFacts {
  bool player = false;
  bool associate = false;
  bool dead = false;
  bool hostile = true;
  bool fighting = false;
  bool searching = false;
  bool investigating = false;
  bool hiding = false;
  bool busy = false;
  float metres = 0.0f;  // to the hider
};
constexpr float kIdleLookReach = 40.0f;  // metres: the stealth markers' reach
IdleLookBlock IdleLookBlocker(const IdleLookFacts& facts);
const char* IdleLookBlockName(IdleLookBlock block);  // for the log: "busy", "fighting" ...

// Whether it has moved off its post (more than 1.5 m from where it stood): its post - where it stands and faces
// between looks - is taken afresh then.
bool IdleLookMovedOff(float post_x, float post_y, float x, float y);

}  // namespace rules
