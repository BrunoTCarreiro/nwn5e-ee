// A flourish inside the combat round (#121): see anim_flourishes.h. data::DecideFlourish (src/data) picks the moment;
// this side reads the creature and its round, and acts.

#include "anim_flourishes.h"

#include <stdint.h>

#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

#include "creature.h"
#include "creature_update.h"
#include "data/anim_flourish.h"
#include "data/anim_recipe.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"

bool AnimationRecipeBuilt(const std::string& name, std::string* text);  // anim_packs.cpp (Mocap mode's recipes)

namespace {

// The combat round and the object (nwmain 8193.37-17, the #121 research; the shared fields are ee_layout.h's).
constexpr size_t kRoundStarted = 0x2f04;        // int32: the round runs (StartCombatRound sets it, clears any pause)
constexpr size_t kRoundTimer = 0x2f0c;          // int32 ms into the round
constexpr size_t kRoundLength = 0x2f10;         // int32 ms (a pause shrinks it: DecrementRoundLength)
constexpr size_t kObjectAnimationSpeed = 0xdc;  // float, beside the animation +0xd8

constexpr int64_t kSwapGapMs = 100;       // the swap reaches the client before the animation
constexpr int64_t kGiveUpMs = 12000;      // game time: two rounds
constexpr int64_t kRestoreAfterMs = 600;  // the swap stays this long after the clip
constexpr int kAnimationIdle = 0;
constexpr const char* kSwapVar = "n5_flourish_swap";  // the swap script's answer: 1 = part-based, swapped

using CheckActionLengthFn = int (*)(void* round, uint32_t by, int ms);
using SetRoundPausedFn = void (*)(void* round, int paused, uint32_t by);
using RoundTimeFn = void (*)(void* round, int ms, int flag);
using SetAnimationFn = void (*)(void* creature, int animation);

struct Api {
  CheckActionLengthFn check_action_length =
      reinterpret_cast<CheckActionLengthFn>(EngineExport(ee::kCheckActionLength));
  SetRoundPausedFn set_round_paused = reinterpret_cast<SetRoundPausedFn>(EngineExport(ee::kSetRoundPaused));
  RoundTimeFn set_pause_timer = reinterpret_cast<RoundTimeFn>(EngineExport(ee::kSetPauseTimer));
  RoundTimeFn decrement_round_length = reinterpret_cast<RoundTimeFn>(EngineExport(ee::kDecrementRoundLength));
  SetAnimationFn set_animation = reinterpret_cast<SetAnimationFn>(EngineExport(ee::kSetAnimation));

  bool Complete() const {
    return check_action_length && set_round_paused && set_pause_timer && decrement_round_length && set_animation;
  }
};

const Api& E() {
  static const Api api;
  return api;
}

// The animations CNWSCreature::AnimationStationary calls moving (walks, runs).
bool MovingAnimation(int animation) {
  return (animation >= 2 && animation <= 4) || animation == 78 || animation == 79 ||
         (animation >= 84 && animation <= 87);
}

enum class Waiting { kNone, kMoving, kNextRound, kPaused, kNoRoom };

struct Request {
  int animation = 0;
  std::string base;  // the swapped animation (empty: no swap)
  std::string clip;
  int clip_ms = 0;
  int pause_ms = 0;
  FlourishWanted wanted = nullptr;
  int64_t not_before = 0;
  int64_t give_up = 0;
  bool playing = false;
  bool paused_round = false;  // played in its paused round (the pause's end sets the ready itself)
  bool ended = false;         // the clip's end handled
  int64_t clip_end = 0;
  int64_t restore_at = 0;
  Waiting waiting = Waiting::kNone;  // the last wait logged
};

bool g_installed = false;
std::unordered_map<uint32_t, Request> g_requests;

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

int64_t Now() { return static_cast<int64_t>(GameMs()); }

void RestoreSwap(uint32_t id, const Request& request) {
  if (request.base.empty()) return;
  RunScriptChunk(("ReplaceObjectAnimation(OBJECT_SELF, \"" + request.base + "\", \"" + request.base + "\");").c_str(),
                 id);
}

// " as n5_rage" when swapped.
std::string ShownAs(const Request& request) { return request.base.empty() ? "" : " as " + request.clip; }

void Play(void* creature, uint32_t id, Request& request, void* round, bool pause) {
  const int was = Field<int32_t>(creature, ee::kCreatureAnimation);
  if (pause) {
    E().set_round_paused(round, 1, id);
    E().set_pause_timer(round, request.pause_ms, 0);
    E().decrement_round_length(round, request.pause_ms, 0);
  }
  SetField<float>(creature, kObjectAnimationSpeed, 1.0f);
  E().set_animation(creature, request.animation);
  request.playing = true;
  request.paused_round = pause;
  request.clip_end = Now() + request.clip_ms;
  request.restore_at = request.clip_end + kRestoreAfterMs;
  if (pause) {
    Log("flourish: 0x%08X plays animation %d%s in its round - paused %d by 0x%08X for %d ms, round %d/%d ms "
        "(animation was %d)",
        id, request.animation, ShownAs(request).c_str(), Field<int32_t>(round, ee::kRoundPaused),
        Field<uint32_t>(round, ee::kRoundPausedBy), Field<int32_t>(round, ee::kRoundPauseTimer),
        Field<int32_t>(round, kRoundTimer), Field<int32_t>(round, kRoundLength), was);
  } else {
    Log("flourish: 0x%08X plays animation %d%s at once - no round running (animation was %d)", id,
        request.animation, ShownAs(request).c_str(), was);
  }
}

// After it plays: out of combat nothing sets the animation back (a paused round's end sets the ready itself), so the
// idle at the clip's end - unless something else took over - and the next flourish is sent again. Then the swap.
void Finish(void* creature, uint32_t id, Request& request, int64_t now) {
  if (!request.ended && now >= request.clip_end) {
    request.ended = true;
    if (!request.paused_round && Field<int32_t>(creature, ee::kCreatureAnimation) == request.animation) {
      E().set_animation(creature, kAnimationIdle);
      Log("flourish: 0x%08X animation %d over - back to the idle", id, request.animation);
    }
  }
  if (now < request.restore_at) return;
  RestoreSwap(id, request);
  Log("flourish: 0x%08X done%s", id, request.base.empty() ? "" : " - the swap restored");
  g_requests.erase(id);
}

void LogWait(uint32_t id, Request& request, const data::FlourishTick& tick, void* round) {
  const Waiting why = tick.moving            ? Waiting::kMoving
                      : !tick.round_started ? Waiting::kNextRound
                      : tick.paused         ? Waiting::kPaused
                                            : Waiting::kNoRoom;
  if (why == request.waiting) return;
  request.waiting = why;
  switch (why) {
    case Waiting::kMoving: Log("flourish: 0x%08X waits - moving", id); break;
    case Waiting::kNextRound: Log("flourish: 0x%08X waits - attacking, its next round not started", id); break;
    case Waiting::kPaused:
      Log("flourish: 0x%08X waits - its round is paused by 0x%08X (%d ms left)", id,
          Field<uint32_t>(round, ee::kRoundPausedBy), Field<int32_t>(round, ee::kRoundPauseTimer));
      break;
    default:
      Log("flourish: 0x%08X waits - no room for %d ms in its round (%d/%d ms)", id, request.pause_ms,
          Field<int32_t>(round, kRoundTimer), Field<int32_t>(round, kRoundLength));
      break;
  }
}

// After the engine's update (listeners run after AIUpdate), so the engine's own swing and walk are already set.
void OnCreatureUpdate(void* creature) {
  if (g_requests.empty() || !creature) return;
  const uint32_t id = IdOf(creature);
  const auto found = g_requests.find(id);
  if (found == g_requests.end()) return;
  Request& request = found->second;
  const int64_t now = Now();
  if (request.playing) {
    Finish(creature, id, request, now);
    return;
  }
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  const uint16_t action = Field<uint16_t>(creature, ee::kObjectCurrentAction);
  data::FlourishTick tick;
  tick.alive = !engine::IsDeadOrDying(creature);
  tick.wanted = !request.wanted || request.wanted(creature);
  tick.has_round = round != nullptr;
  tick.round_started = round && Field<int32_t>(round, kRoundStarted) != 0;
  tick.paused = round && Field<int32_t>(round, ee::kRoundPaused) == 1;
  tick.moving = action == ee::kActionMoveToPoint || MovingAnimation(Field<int32_t>(creature, ee::kCreatureAnimation));
  tick.attacking = action == ee::kActionAttackObject;
  tick.fits = tick.round_started && !tick.paused && E().check_action_length(round, id, request.pause_ms) != 0;
  tick.now_ms = now;
  tick.not_before_ms = request.not_before;
  tick.give_up_ms = request.give_up;
  switch (data::DecideFlourish(tick)) {
    case data::FlourishStep::Drop:
      RestoreSwap(id, request);
      Log("flourish: 0x%08X drops animation %d - %s", id, request.animation,
          !tick.alive ? "dead or dying" : !tick.wanted ? "no longer wanted" : "waited 12 s");
      g_requests.erase(found);
      return;
    case data::FlourishStep::PlayNow: Play(creature, id, request, round, false); return;
    case data::FlourishStep::PauseAndPlay: Play(creature, id, request, round, true); return;
    case data::FlourishStep::Wait: break;
  }
  if (now >= request.not_before) LogWait(id, request, tick, round);
}

}  // namespace

void RequestFlourishClip(void* creature, int engine_animation, const char* base, float base_seconds,
                         const std::string& clip, FlourishWanted wanted) {
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  if (!g_installed) {
    Log("flourish: 0x%08X animation %d skipped - not installed", id, engine_animation);
    return;
  }
  const auto old = g_requests.find(id);
  if (old != g_requests.end()) {
    RestoreSwap(id, old->second);
    Log("flourish: 0x%08X animation %d replaced by a new one", id, old->second.animation);
    g_requests.erase(old);
  }
  Request request;
  request.animation = engine_animation;
  request.wanted = wanted;
  float seconds = base_seconds;
  std::string text;
  std::string error;
  data::AnimRecipe recipe;
  if (base && *base && !clip.empty() && AnimationRecipeBuilt(clip, &text) &&
      data::ReadAnimRecipe(text, &recipe, &error) && recipe.length > 0.0f) {
    const std::string var = std::string("\"") + kSwapVar + "\"";
    RunScriptChunk(("SetLocalInt(OBJECT_SELF, " + var + ", 0);"
                    " if (Get2DAString(\"appearance\", \"MODELTYPE\", GetAppearanceType(OBJECT_SELF)) == \"P\") {"
                    " ReplaceObjectAnimation(OBJECT_SELF, \"" + base + "\", \"" + clip + "\");"
                    " SetLocalInt(OBJECT_SELF, " + var + ", 1); }")
                       .c_str(),
                   id);
    if (engine::GetLocalInt(creature, kSwapVar) == 1) {
      request.base = base;
      request.clip = clip;
      seconds = recipe.length;
    }
    RunScriptChunk(("DeleteLocalInt(OBJECT_SELF, " + var + ");").c_str(), id);
  }
  request.clip_ms = seconds > 0.0f && std::isfinite(seconds) ? static_cast<int>(std::lround(seconds * 1000.0f)) : 0;
  request.pause_ms = data::FlourishPauseMs(seconds);
  const int64_t now = Now();
  request.not_before = now + kSwapGapMs;
  request.give_up = now + kGiveUpMs;
  Log("flourish: 0x%08X asks for animation %d - %s, %.2f s%s", id, engine_animation,
      request.base.empty() ? (base ? base : "?") : request.clip.c_str(), seconds,
      request.base.empty() ? "" : (" (" + request.base + " swapped)").c_str());
  g_requests[id] = std::move(request);
}

void InstallFlourish() {
  if (g_installed) return;
  if (!E().Complete()) {
    Log("flourish: an engine call is missing - flourishes are skipped");
    return;
  }
  if (!AddCreatureUpdateListener(&OnCreatureUpdate)) {
    Log("flourish: no creature update - flourishes are skipped");
    return;
  }
  g_installed = true;
  Log("flourish: a flourish waits for its swing and plays in its paused round (#121)");
}
