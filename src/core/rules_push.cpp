// D4 Push (port of 1.69's Push v2 in rules_mastery.cpp; docs/EE-PORT.md E2d).
// Faithful only (NWN mode has no Push).
//
// 2024: a hit with a Push weapon you have mastery with pushes a creature up to
// Large 10 ft straight away from you. In NWN the hit's damage lands a moment
// after the roll, so:
// - at the roll (PushOnHit) the line is planned; if an enemy is now closer to
//   the attacker than the pushed one will be, the attacker turns to it at once
//   (the round's next swing would otherwise go after the pushed one);
// - when the damage lands (PushOnDamage) the target falls (the knockdown's fall
//   animation, its round paused for the fall's length), can't move, and slides
//   along the line in its AI updates;
// - for a few seconds the attacker is watched: if it is left with no orders
//   while the creature it should fight stands, the attack is reissued as a
//   click would (owner's 1.69 tests: the PC stood idle after pushes), and at
//   its round's end a turn made in place is made to stick.
// The line stops before unwalkable ground and placeables (the area's static
// objects); doors are not in that test.
//
// G1 (docs/PLAN.md "Grapple and Throw"): a Shove's push choice (ShovePush) uses
// the same line and slide, 5 ft, with no fall - the creature flinches and is
// held in place for a moment so its own walking doesn't fight the slide.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <windows.h>

#include <string>
#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"

namespace {

struct Vec3 {
  float x, y, z;
};

using CheckActionLengthFn = int (*)(void* round, uint32_t attacker, int ms);
using SetRoundPausedFn = void (*)(void* round, int paused, uint32_t by);
using RoundTimeFn = void (*)(void* round, int ms, int flag);
using SetAnimationFn = void (*)(void* creature, int animation);
using ComputeHeightFn = float (*)(void* area, Vec3 position);
using WalkableLineFn = int (*)(void* area, float x1, float y1, float x2, float y2);
// (..., unsigned int& blocker, int flag, CExoArrayList<CGameObject*>* exclude list, int)
using StaticLineFn = int (*)(void* area, float x1, float y1, float x2, float y2, float z1, float z2, float radius,
                             uint32_t exclude, uint32_t* blocker, int flag, void* exclude_list, int doors);
using SetPositionFn = void (*)(void* object, Vec3 position, int skip_objects_array);
using UpdateSubareasFn = int (*)(void* creature, Vec3 position, uint32_t area);
using GetAreaFn = void* (*)(void* object);
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int passive, int clear_first, int front);

struct Api {
  CheckActionLengthFn check_action_length =
      reinterpret_cast<CheckActionLengthFn>(EngineExport(ee::kCheckActionLength));
  SetRoundPausedFn set_round_paused = reinterpret_cast<SetRoundPausedFn>(EngineExport(ee::kSetRoundPaused));
  RoundTimeFn set_pause_timer = reinterpret_cast<RoundTimeFn>(EngineExport(ee::kSetPauseTimer));
  RoundTimeFn decrement_round_length = reinterpret_cast<RoundTimeFn>(EngineExport(ee::kDecrementRoundLength));
  SetAnimationFn set_animation = reinterpret_cast<SetAnimationFn>(EngineExport(ee::kSetAnimation));
  ComputeHeightFn compute_height = reinterpret_cast<ComputeHeightFn>(EngineExport(ee::kComputeHeight));
  WalkableLineFn walkable_line = reinterpret_cast<WalkableLineFn>(EngineExport(ee::kTestLineWalkable));
  StaticLineFn static_line = reinterpret_cast<StaticLineFn>(EngineExport(ee::kStaticObjectsOnLine));
  SetPositionFn set_position = reinterpret_cast<SetPositionFn>(EngineExport(ee::kSetPosition));
  UpdateSubareasFn update_subareas = reinterpret_cast<UpdateSubareasFn>(EngineExport(ee::kUpdateSubareasOnJump));
  GetAreaFn get_area = reinterpret_cast<GetAreaFn>(EngineExport(ee::kGetArea));
  AddAttackActionsFn add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));

  bool Complete() const {
    return check_action_length && set_round_paused && set_pause_timer && decrement_round_length && set_animation &&
           compute_height && walkable_line && static_line && set_position && update_subareas && get_area &&
           add_attack_actions;
  }
};

const Api& E() {
  static const Api api;
  return api;
}

constexpr float kPushMetres = 3.0f;          // 10 ft
constexpr int kPushFallMs = 1200;            // the fall's round pause
constexpr DWORD kPushSlideMs = 100;          // the move along the line (owner: 0.45 -> 0.25 -> 0.15 s on 1.69, 0.1 s on EE)
constexpr DWORD kPushWatchMs = 8000;
constexpr DWORD kAwaitDamageMs = 5000;
constexpr DWORD kRetargetMaxMs = 15000;
constexpr float kBodyMetres = 0.5f;          // walkable line kept behind the new spot
constexpr int kAnimationDamage = 14;         // the flinch a hit plays (nwmain's animation switch)
constexpr float kShoveHoldSeconds = 0.5f;    // a shoved creature held in place while it slides

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
Vec3 PositionOf(const void* object) {
  return Vec3{Field<float>(object, ee::kObjectPositionX), Field<float>(object, ee::kObjectPositionY),
              Field<float>(object, ee::kObjectPositionZ)};
}

int g_log_lines = 0;
bool LogSlot() { return g_config.log_attacks && g_log_lines < 1000 && ++g_log_lines; }

// --- Engine pieces -----------------------------------------------------------

struct PushPlan {
  const char* refused = nullptr;  // why it can't be pushed at all
  Vec3 from{};
  Vec3 to{};
  Vec3 away{};
  float metres = 0.0f;
  uint32_t blocker = ee::kObjectInvalid;  // a placeable that cut the line short
};

void* AreaOf(void* object) { return object ? E().get_area(object) : nullptr; }

// The line straight away from the attacker, as far as walkable ground and
// placeables allow (in 0.5 m steps, 3 m first).
PushPlan PlanPush(void* target, void* attacker, float distance) {
  PushPlan plan;
  if (engine::IsDeadOrDying(target)) {
    plan.refused = "dead";
    return plan;
  }
  if (Field<int32_t>(target, ee::kCreatureSize) > ee::kCreatureSizeLarge) {
    plan.refused = "larger than Large";
    return plan;
  }
  void* area = AreaOf(target);
  if (!area || AreaOf(attacker) != area) {
    plan.refused = "not in the attacker's area";
    return plan;
  }
  plan.from = PositionOf(target);
  const Vec3 attacker_at = PositionOf(attacker);
  float ax = plan.from.x - attacker_at.x;
  float ay = plan.from.y - attacker_at.y;
  float length = sqrtf(ax * ax + ay * ay);
  if (length < 0.01f) {  // on top of each other: along the attacker's facing
    ax = Field<float>(attacker, ee::kObjectOrientationX);
    ay = Field<float>(attacker, ee::kObjectOrientationY);
    length = sqrtf(ax * ax + ay * ay);
    if (length < 0.01f) {
      plan.refused = "no direction";
      return plan;
    }
  }
  plan.away = Vec3{ax / length, ay / length, 0.0f};
  // The static-object radius creature placement uses (BumpFriends).
  const void* path = Field<void*>(target, ee::kCreaturePathInfo);
  float radius = path ? Field<float>(path, ee::kPathInfoRadius) : 0.5f;
  if (!(radius > 0.05f && radius < 3.0f)) radius = 0.5f;
  for (float metres = distance; metres >= 0.49f; metres -= 0.5f) {
    const Vec3 to{plan.from.x + plan.away.x * metres, plan.from.y + plan.away.y * metres, plan.from.z};
    const float end_x = to.x + plan.away.x * kBodyMetres;
    const float end_y = to.y + plan.away.y * kBodyMetres;
    // Off the tile grid; inside it, walkability is the line test's.
    if (E().compute_height(area, to) <= ee::kAreaOffGridHeight + 0.01f) continue;
    if (E().walkable_line(area, plan.from.x, plan.from.y, end_x, end_y) != 1) continue;
    uint32_t blocker = ee::kObjectInvalid;
    if (E().static_line(area, plan.from.x, plan.from.y, end_x, end_y, plan.from.z - 0.1f, plan.from.z + 1.8f, radius,
                        ee::kObjectInvalid, &blocker, 1, nullptr, 0) != 1) {
      if (plan.blocker == ee::kObjectInvalid) plan.blocker = blocker;
      continue;
    }
    plan.to = to;
    plan.metres = metres;
    return plan;
  }
  plan.refused = plan.blocker != ee::kObjectInvalid ? "a placeable right behind it" : "no walkable ground behind it";
  return plan;
}

// Moves a creature within its area as a jump does (SetPosition, then the
// triggers and sub-areas it enters); false off the tile grid. (The line was
// checked walkable when it was planned.)
bool PlaceOnGround(void* creature, float x, float y) {
  void* area = AreaOf(creature);
  if (!area) return false;
  Vec3 at{x, y, Field<float>(creature, ee::kObjectPositionZ)};
  const float height = E().compute_height(area, at);
  if (height <= ee::kAreaOffGridHeight + 0.01f) return false;
  at.z = height;
  E().set_position(creature, at, 0);
  E().update_subareas(creature, at, Field<uint32_t>(area, ee::kAreaObjectId));
  return true;
}

bool LyingDown(int animation) {
  return animation == ee::kAnimationFallFacing || animation == ee::kAnimationFallTurned || animation == 0x20 ||
         animation == 0x21 || animation == 0x24;
}

enum class Fall { kFell, kFellUnpaused, kLyingDown };

// The knockdown's fall, and the creature's round paused for its length (as
// the engine pauses a round for a knockdown).
Fall PlayFall(void* target, uint32_t attacker_id, const Vec3& away, uint32_t* paused_by) {
  *paused_by = ee::kObjectInvalid;
  if (LyingDown(Field<int32_t>(target, ee::kCreatureAnimation))) return Fall::kLyingDown;
  void* round = Field<void*>(target, ee::kCreatureCombatRound);
  bool paused = false;
  if (round) {
    if (Field<int32_t>(round, ee::kRoundPaused) == 1) *paused_by = Field<uint32_t>(round, ee::kRoundPausedBy);
    if (E().check_action_length(round, attacker_id, kPushFallMs) != 0) {
      if (*paused_by == attacker_id) {
        const int running = Field<int32_t>(round, ee::kRoundPauseTimer);
        if (running < kPushFallMs) {
          E().decrement_round_length(round, kPushFallMs - running, 0);
          SetField<int32_t>(round, ee::kRoundPauseTimer, kPushFallMs);
        }
      } else {
        E().set_round_paused(round, 1, attacker_id);
        E().set_pause_timer(round, kPushFallMs, 0);
        E().decrement_round_length(round, kPushFallMs, 0);
      }
      paused = true;
    }
  }
  // As the knockdown picks it: facing the attacker (away from the push) or not.
  const float fx = Field<float>(target, ee::kObjectOrientationX);
  const float fy = Field<float>(target, ee::kObjectOrientationY);
  const float length = sqrtf(fx * fx + fy * fy);
  const float dot = length > 0.0f ? (-away.x * fx - away.y * fy) / length : 0.0f;
  E().set_animation(target, dot >= ee::kFallFacingDot ? ee::kAnimationFallFacing : ee::kAnimationFallTurned);
  return paused ? Fall::kFell : Fall::kFellUnpaused;
}

// --- The attacker's orders -------------------------------------------------

// The action the creature carries out next (the head of its queue), or null.
void* HeadAction(void* creature) {
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  const void* head = list ? Field<void*>(list, ee::kListHead) : nullptr;
  return head ? Field<void*>(head, ee::kListNodeData) : nullptr;
}

void* HeadAttackAction(void* creature) {
  void* action = HeadAction(creature);
  return action && Field<int32_t>(action, ee::kActionId) == ee::kActionAttackObject ? action : nullptr;
}

uint32_t OrderedTarget(void* creature) {
  const void* action = HeadAttackAction(creature);
  return action ? Field<uint32_t>(action, ee::kActionFirstParam) : ee::kObjectInvalid;
}

// Outside RunActions the running action is back at the queue's head and the
// current action reads none; inside it (an attack roll), the queue's head is
// the next order and the attack being carried out is the current action.
bool HasNoOrders(void* creature) {
  return !HeadAction(creature) && Field<uint16_t>(creature, ee::kObjectCurrentAction) == ee::kActionNone;
}

std::string OrderState(void* creature) {
  const void* action = HeadAction(creature);
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  const uint16_t current = Field<uint16_t>(creature, ee::kObjectCurrentAction);
  char text[240];
  sprintf_s(text,
            "queue %s%d at 0x%08X, current %d at 0x%08X, attack target 0x%08X, attempted 0x%08X, round target 0x%08X",
            action ? "" : "empty ", action ? Field<int32_t>(action, ee::kActionId) : -1,
            action ? Field<uint32_t>(action, ee::kActionFirstParam) : ee::kObjectInvalid,
            current == ee::kActionNone ? -1 : static_cast<int>(current),
            Field<uint32_t>(creature, ee::kObjectCurrentActionTarget),
            Field<uint32_t>(creature, ee::kCreatureAttackTarget),
            Field<uint32_t>(creature, ee::kCreatureAttemptedAttackTarget),
            round ? Field<uint32_t>(round, ee::kRoundTarget) : ee::kObjectInvalid);
  return text;
}

// Points the attacker's attack order at `next` wherever it names `pushed`:
// the round's target (as the after-kill retarget does, so the round's next
// swings go there), the queued attack action, the attack being carried out
// and the attack targets. Returns how many order fields held `pushed`.
int RewriteAttackOrder(void* attacker, uint32_t pushed, uint32_t next) {
  int changed = 0;
  if (void* round = Field<void*>(attacker, ee::kCreatureCombatRound)) {
    SetField<uint32_t>(round, ee::kRoundTarget, next);
    SetField<int32_t>(attacker, ee::kCreatureTargetSwitched, 1);
  }
  if (void* action = HeadAttackAction(attacker)) {
    if (Field<uint32_t>(action, ee::kActionFirstParam) == pushed) {
      SetField<uint32_t>(action, ee::kActionFirstParam, next);
      ++changed;
    }
  }
  if (Field<uint32_t>(attacker, ee::kCreatureAttackTarget) == pushed) {
    SetField<uint32_t>(attacker, ee::kCreatureAttackTarget, next);
    ++changed;
  }
  if (Field<uint32_t>(attacker, ee::kCreatureAttemptedAttackTarget) == pushed) {
    SetField<uint32_t>(attacker, ee::kCreatureAttemptedAttackTarget, next);
    ++changed;
  }
  if (Field<uint16_t>(attacker, ee::kObjectCurrentAction) == ee::kActionAttackObject &&
      Field<uint32_t>(attacker, ee::kObjectCurrentActionTarget) == pushed) {
    SetField<uint32_t>(attacker, ee::kObjectCurrentActionTarget, next);
    ++changed;
  }
  return changed;
}

// --- State -------------------------------------------------------------------

struct Awaiting {
  uint32_t target;
  DWORD tick;
  float metres;  // how far (Push 3 m; Brutal Strike's Forceful Blow 4.6 m)
};
struct Slide {
  Vec3 from;
  Vec3 to;
  float metres;
  void* area;
  DWORD start;
};
struct Retarget {
  uint32_t pushed;
  uint32_t next;
  DWORD tick;
};
struct Watch {
  uint32_t intended;
  DWORD until;
  std::string state;
  bool reissued;
};
struct HeldFall {
  int animation;
  DWORD until;
  bool logged;
};

// All on the engine's server thread.
std::unordered_map<uint32_t, Awaiting> g_awaiting;    // attacker -> the target its Push hit waits to land on
std::unordered_map<uint32_t, Slide> g_slides;         // pushed creature ->
std::unordered_map<uint32_t, Retarget> g_retargets;   // attacker -> the closer enemy it turned to
std::unordered_map<uint32_t, Watch> g_watches;        // attacker ->
std::unordered_map<uint32_t, HeldFall> g_falls;       // pushed creature ->

// The nearest enemy the attacker sees, edge to edge nearer than the pushed one will be.
struct CloserSearch {
  void* attacker;
  uint32_t pushed;
  float best;
  uint32_t next;
  std::string candidates;
};

void ConsiderCloser(void* context, uint32_t id, void* creature) {
  auto* s = static_cast<CloserSearch*>(context);
  if (id == s->pushed || !engine::IsEnemy(s->attacker, id) || engine::IsDeadOrDying(creature)) return;
  const float distance = engine::EdgeDistance(s->attacker, creature);
  const bool seen = engine::Sees(s->attacker, id);
  char text[48];
  sprintf_s(text, "%s0x%08X %.1f m%s", s->candidates.empty() ? "" : ", ", id, distance, seen ? "" : " unseen");
  s->candidates += text;
  if (seen && distance < s->best) {
    s->best = distance;
    s->next = id;
  }
}

// After a push: when an enemy is now closer to the attacker than the pushed
// one, the attacker goes for the nearest such enemy instead of chasing (owner,
// 2026-10-19). Decided at the roll, rewritten again when the hit lands and
// checked at the round's end; every decision is logged with the candidates.
uint32_t RetargetAfterPush(void* attacker, uint32_t attacker_id, uint32_t pushed, float pushed_distance) {
  void* round = Field<void*>(attacker, ee::kCreatureCombatRound);
  CloserSearch search{attacker, pushed, pushed_distance, ee::kObjectInvalid, std::string()};
  engine::ForEachOtherCreatureInArea(attacker, &ConsiderCloser, &search);
  if (LogSlot()) {
    Log("push: 0x%08X retarget (roll): %s; pushed 0x%08X ends %.1f m away; enemies: %s -> %s", attacker_id,
        OrderState(attacker).c_str(), pushed, pushed_distance,
        search.candidates.empty() ? "none" : search.candidates.c_str(),
        !round ? "no round" : search.next == ee::kObjectInvalid ? "none closer, keeps the pushed one" : "switch");
  }
  if (!round || search.next == ee::kObjectInvalid) return ee::kObjectInvalid;
  RewriteAttackOrder(attacker, pushed, search.next);
  g_retargets[attacker_id] = Retarget{pushed, search.next, GetTickCount()};
  return search.next;
}

// The hit's damage landed: plan the line again, then fall and slide.
void Push(void* attacker, uint32_t attacker_id, void* target, uint32_t target_id, float metres) {
  const PushPlan plan = PlanPush(target, attacker, metres);
  if (plan.refused) {
    if (LogSlot()) {
      if (plan.blocker != ee::kObjectInvalid) {
        Log("push: 0x%08X on 0x%08X: not pushed (%s, placeable 0x%08X)", attacker_id, target_id, plan.refused,
            plan.blocker);
      } else {
        Log("push: 0x%08X on 0x%08X: not pushed (%s)", attacker_id, target_id, plan.refused);
      }
    }
    return;
  }
  // The attacker turned to a closer enemy at the roll: by now the engine has
  // filled in its attack order (on the pushed one), so it is rewritten again.
  const auto turned = g_retargets.find(attacker_id);
  if (turned != g_retargets.end() && turned->second.pushed == target_id) {
    const uint32_t ordered_before = OrderedTarget(attacker);
    const int changed = RewriteAttackOrder(attacker, target_id, turned->second.next);
    if (LogSlot()) {
      Log("push: 0x%08X retarget (landing): attack order was 0x%08X; %d order fields moved to 0x%08X", attacker_id,
          ordered_before, changed, turned->second.next);
    }
  }
  // It can't walk while it falls and slides (its own attack order would pull
  // it back toward the attacker).
  ApplyEffectAs(attacker_id, target_id, "DURATION_TYPE_TEMPORARY", "EffectCutsceneImmobilize()",
                static_cast<float>(kPushFallMs) / 1000.0f, 0.0f);
  uint32_t paused_by = ee::kObjectInvalid;
  const Fall fall = PlayFall(target, attacker_id, plan.away, &paused_by);
  if (fall != Fall::kLyingDown) {
    g_falls[target_id] = HeldFall{Field<int32_t>(target, ee::kCreatureAnimation), GetTickCount() + kPushFallMs - 150,
                                  false};
  }
  g_slides[target_id] = Slide{plan.from, plan.to, plan.metres, AreaOf(target), GetTickCount()};
  if (LogSlot()) {
    char blocker[48] = "";
    if (plan.blocker != ee::kObjectInvalid) sprintf_s(blocker, " (cut short by placeable 0x%08X)", plan.blocker);
    char fall_note[96];
    if (fall == Fall::kFell) {
      sprintf_s(fall_note, ", falls and gets up");
    } else if (fall == Fall::kFellUnpaused && paused_by != ee::kObjectInvalid) {
      sprintf_s(fall_note, ", falls without a round pause (its round is paused by 0x%08X)", paused_by);
    } else if (fall == Fall::kFellUnpaused) {
      sprintf_s(fall_note, ", falls without a round pause (its round has no time left)");
    } else {
      sprintf_s(fall_note, " (no fall: already lying down)");
    }
    Log("push: 0x%08X on 0x%08X: slides %.1f m away%s%s", attacker_id, target_id, plan.metres, blocker, fall_note);
  }
}

// --- Per creature update -------------------------------------------------------

void WatchAttacker(void* creature, uint32_t id) {
  const auto it = g_watches.find(id);
  if (it == g_watches.end()) return;
  Watch& watch = it->second;
  if (GetTickCount() > watch.until || engine::IsDeadOrDying(creature)) {
    g_watches.erase(it);
    return;
  }
  const std::string state = OrderState(creature);
  if (state != watch.state) {
    watch.state = state;
    if (LogSlot()) Log("push: 0x%08X after the push: %s", id, state.c_str());
  }
  if (watch.reissued || !HasNoOrders(creature)) return;
  void* intended = CreatureById(watch.intended);
  if (!intended || engine::IsDeadOrDying(intended) || !engine::IsEnemy(creature, watch.intended)) return;
  watch.reissued = true;
  E().add_attack_actions(creature, watch.intended, 0, 1, 0);
  if (LogSlot()) Log("push: 0x%08X after the push: left with no orders; attack on 0x%08X reissued", id, watch.intended);
}

// The fall animation is put back when the creature's own update replaced it.
void HoldFall(void* creature, uint32_t id) {
  const auto it = g_falls.find(id);
  if (it == g_falls.end()) return;
  HeldFall& fall = it->second;
  if (GetTickCount() > fall.until || engine::IsDeadOrDying(creature)) {
    g_falls.erase(it);
    return;
  }
  const int animation = Field<int32_t>(creature, ee::kCreatureAnimation);
  if (animation == fall.animation) return;
  E().set_animation(creature, fall.animation);
  if (!fall.logged && LogSlot()) {
    fall.logged = true;
    Log("push: 0x%08X fall: animation %d replaced it, fall %d put back", id, animation, fall.animation);
  }
}

void SlideAlong(void* creature, uint32_t id) {
  const auto it = g_slides.find(id);
  if (it == g_slides.end()) return;
  const Slide& slide = it->second;
  if (engine::IsDeadOrDying(creature) || AreaOf(creature) != slide.area) {
    g_slides.erase(it);
    return;
  }
  const DWORD elapsed = GetTickCount() - slide.start;
  const float t = elapsed >= kPushSlideMs ? 1.0f : static_cast<float>(elapsed) / static_cast<float>(kPushSlideMs);
  PlaceOnGround(creature, slide.from.x + (slide.to.x - slide.from.x) * t, slide.from.y + (slide.to.y - slide.from.y) * t);
  if (t >= 1.0f) g_slides.erase(it);
}

void OnCreatureUpdate(void* creature) {
  if (g_watches.empty() && g_falls.empty() && g_slides.empty()) return;
  const uint32_t id = IdOf(creature);
  if (!g_watches.empty()) WatchAttacker(creature, id);
  if (!g_falls.empty()) HoldFall(creature, id);
  if (!g_slides.empty()) SlideAlong(creature, id);
}

bool g_on = false;

}  // namespace

std::string ShovePush(void* attacker, void* target, float metres, float* pushed) {
  if (pushed) *pushed = 0.0f;
  if (!g_on) return "the push is off";
  if (!attacker || !target) return "no creature";
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(target);
  const PushPlan plan = PlanPush(target, attacker, metres);
  if (plan.refused) {
    if (LogSlot()) Log("push: shove 0x%08X on 0x%08X: not pushed (%s)", attacker_id, target_id, plan.refused);
    return plan.refused;
  }
  ApplyEffectAs(attacker_id, target_id, "DURATION_TYPE_TEMPORARY", "EffectCutsceneImmobilize()", kShoveHoldSeconds,
                0.0f);
  if (!LyingDown(Field<int32_t>(target, ee::kCreatureAnimation))) E().set_animation(target, kAnimationDamage);
  g_slides[target_id] = Slide{plan.from, plan.to, plan.metres, AreaOf(target), GetTickCount()};
  if (pushed) *pushed = plan.metres;
  if (LogSlot()) {
    Log("push: shove 0x%08X on 0x%08X: slides %.1f m away, on its feet%s", attacker_id, target_id, plan.metres,
        plan.blocker != ee::kObjectInvalid ? " (cut short by a placeable)" : "");
  }
  return std::string();
}

std::string PushOnHit(void* attacker, void* target, float metres) {
  if (!g_on || !attacker || !target) return std::string();
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(target);
  g_awaiting[attacker_id] = Awaiting{target_id, GetTickCount(), metres};
  std::string what = "Push: when the hit lands";
  const PushPlan plan = PlanPush(target, attacker, metres);
  if (plan.refused) return what + " (now: " + plan.refused + ")";
  const uint32_t next =
      RetargetAfterPush(attacker, attacker_id, target_id, engine::EdgeDistance(attacker, target) + plan.metres);
  if (next != ee::kObjectInvalid) {
    char text[64];
    sprintf_s(text, "; attacker now attacks the closer 0x%08X", next);
    what += text;
  }
  g_watches[attacker_id] =
      Watch{next != ee::kObjectInvalid ? next : target_id, GetTickCount() + kPushWatchMs, std::string(), false};
  return what;
}

bool PushAwaiting() { return !g_awaiting.empty(); }

void PushOnDamage(uint32_t attacker_id, void* target) {
  const auto it = g_awaiting.find(attacker_id);
  if (it == g_awaiting.end()) return;
  const uint32_t target_id = IdOf(target);
  if (it->second.target != target_id) return;
  const bool fresh = GetTickCount() - it->second.tick <= kAwaitDamageMs;
  const float metres = it->second.metres;
  g_awaiting.erase(it);
  void* attacker = fresh ? CreatureById(attacker_id) : nullptr;
  if (attacker) Push(attacker, attacker_id, target, target_id, metres);
}

// At the end of the attacker's round: the turn made in place holds when its
// attack action now names the closer enemy; if it went back to the pushed
// one, the switch is made as a player's attack click makes it (the round is
// over, so no attack is lost).
void PushOnRoundEnd(void* owner) {
  if (g_retargets.empty() || !owner) return;
  const auto it = g_retargets.find(IdOf(owner));
  if (it == g_retargets.end()) return;
  const Retarget retarget = it->second;
  g_retargets.erase(it);
  if (GetTickCount() - retarget.tick > kRetargetMaxMs) return;
  const uint32_t ordered = OrderedTarget(owner);
  const char* outcome = "kept";
  if (ordered == retarget.pushed) {
    void* next = CreatureById(retarget.next);
    if (!next || engine::IsDeadOrDying(next) || !engine::IsEnemy(owner, retarget.next)) {
      outcome = "went back to the pushed one; the closer enemy is gone";
    } else {
      E().add_attack_actions(owner, retarget.next, 0, 1, 0);
      outcome = "went back to the pushed one; attack order switched (as a click)";
    }
  } else if (ordered != retarget.next) {
    outcome = HeadAttackAction(owner) ? "another attack order since" : "no attack order";
  }
  if (LogSlot()) {
    Log("push: 0x%08X round end: %s, wanted 0x%08X: %s", IdOf(owner), OrderState(owner).c_str(), retarget.next,
        outcome);
  }
}

bool InstallPushRules() {
  if (g_on) return true;  // the mastery's install and the Shove's
  if (!E().Complete()) {
    Log("push: an engine function is missing in this game build; Push is off");
    return false;
  }
  g_on = AddCreatureUpdateListener(&OnCreatureUpdate);
  if (g_on) Log("push: on (faithful weapon mastery, Forceful Blow, the Shove's push)");
  return g_on;
}
