// The idle and the walk a creature shows - one place swaps them (EE's
// ReplaceObjectAnimation on pause1, pause2 and walk), sent only on a change:
// - hiding (stealth mode; owner, 2026-10-25: a crouch walk - "go, use stealth
//   mode as the trigger"): a crouch - nwn5e's n5_cridle / n5_crwalk recipes
//   (data\anims; the crouch step of data/anim_recipe.cpp: the hips down, the
//   legs solved to keep the footfalls, a lean), on any creature hiding, player
//   or not; with the weapon walk layers (walk_swordr ... -> n5_crwalk_swordr ...,
//   built by anim_packs.cpp), which would otherwise stand the torso back up.
//   `stealth_crouch`; both rule modes (it's a look);
// - Exhaustion (rules_dying.cpp, faithful): 1-2 the tired idle (pausetrd), 3-5
//   also the injured walk (walkinj). `exhaustion_look`.
// Hiding wins. The new animations exist only on part-based bodies (appearance.2da
// MODELTYPE P: the skeletons the animation packs serve), so the swap script
// checks that first; nothing is touched on other bodies. After a load (nothing
// sent yet) the whole state goes, so a swap the saved game kept is undone too.
// A swap doesn't restart the idle already playing (owner, 2026-10-25: "on being
// discovered the stance should return to standard, right now the PC remains
// crouched"), so when the idle changes on a creature doing nothing, its idle is
// restarted (pause2 for half a second, then pause1: both through the new swap).
// And a swap is undone by mapping the animation to itself, not by removing it
// (owner, again: "when discovered the PC still doesn't return to the normal
// stance"): the server sends the whole list on any change (WriteGameObjUpdate,
// bit 0x18), but the crouch never went away on screen - an emptied list may not
// clear the client's; a changed entry always reaches it (that's how it went on).
// A walk already under way doesn't restart either (owner: "if I leave stealth
// mode while moving the PC doesn't return to normal vertical stance"): when the
// walk's swap changes on a walking creature, its animation is set to the idle
// for one tick (CNWSCreature::SetAnimation(0), after the engine's own update) -
// the next update's movement sets the walk again, now through the new swap.
// Toggling while moving (owner, 2026-10-25: "there's still an issue with the
// animations and the lean the character has when entering or leaving stealth
// mode while walking"), 0.33.1: the idle restart is only for a creature
// standing still - keyboard movement has no action, so 0.31.1's "no action"
// test played the idle for half a second under a moving character; and the
// walk restart waits up to 1.5 s for the walk - stealth turns a run into a
// walk on a later update, which the one check at the swap missed (the run, not
// yet the walk, was playing), so the new walk started before its swap arrived.
// 0.39.1 (owner: "if hiding/unhiding is triggered while moving, the pose
// afterwards is incorrect" - after it stops, the old idle stood, WASD or click
// to move, a weapon held): an idle change skipped while moving waits, and the
// idle is restarted (the same pause2 kick) as soon as the creature stands still
// with nothing to do - within 10 s. 0.39.2 (owner: "Walking backwards with WASD
// with this new pose doesn't quite work, there's a lot of stuttering" - "normal
// backwards also stutters"): walking backwards plays the walk in reverse, and
// a walk mapped to itself may lose the game's own smooth reverse - so the walk
// and its weapon layers go back by removing their swaps (an empty name), while
// the idles stay mapped to themselves: the list is never empty, which is what
// kept a removal from reaching the client before. 0.39.3: a player character
// gets that clean-up once a session even if it never hides - a saved game
// from before kept the walk mapped to itself.
// 0.39.4 (owner: the lean after hiding / unhiding while moving - "(a) and (b)"
// - "it doesn't happen if I have no weapons equipped"): the arm layer a held
// weapon adds (walk_swordr, run_swordr ...) is picked by the client's animation
// player (0x99fc40) only when the creature's animation changes; a stand of one
// server tick never reached the client, so the walk's swap changed (the legs
// follow it) but the arm layer playing went on - upright arms on a crouch walk,
// or the crouch's lean on a normal one. (0.39.4 held the walk at the stand for
// 150 ms; it didn't help - taken out in 0.39.5.) 0.39.5 (owner: "when the PC
// stopped, their torso would not be using the correct stance ... upright and
// swaying as if the character was walking" - and the crouch's lean the other
// way - "removing weapons fixes both issues"): the client stops a walk's arm
// layer by its name through the swap list, so a layer swapped while it played
// was never stopped - it asked for the new one, the old went on swaying on the
// stand. While the creature moves, only the walk's own swap changes now; the
// arm layers' wait (nwn5e_look layers pending) until it stands still, then
// change - the client has stopped the old layer by its own name by then.

#include <windows.h>

#include <stdint.h>

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

bool AnimationRecipeBuilt(const std::string& name, std::string* text);  // anim_packs.cpp

namespace {

using SetAnimationFn = void (*)(void* creature, int animation);
using StationaryFn = int (*)(void* object, uint16_t animation);
constexpr size_t kObjectAnimation = 0xd8;  // uint16: the animation playing (CNWSObject)
constexpr int kAnimWalk = 2, kAnimWalkBack = 3;
constexpr uint64_t kRestartWindowMs = 1500;  // how long a changed walk waits for the walk to restart it

int Playing(void* creature) { return creature ? Field<uint16_t>(creature, kObjectAnimation) : -1; }

// Whether the creature's animation is one it plays standing still (the engine's own test).
bool Stationary(void* creature) {
  static const auto stationary =
      reinterpret_cast<StationaryFn>(EngineExport("?AnimationStationary@CNWSObject@@QEAAHG@Z"));
  return !creature || !stationary || stationary(creature, static_cast<uint16_t>(Playing(creature))) != 0;
}

constexpr const char* kCrouchIdle = "n5_cridle";
constexpr const char* kCrouchWalk = "n5_crwalk";
constexpr const char* kHolds[] = {"swordr", "swordl", "shieldl", "bowl"};  // walk_<hold>: anim_packs.cpp

struct Look {
  int exhaustion = 0;
  bool hiding = false;
  bool sent = false;  // false: nothing sent yet - the next send restores what isn't swapped too
  std::string idle, walk;
  uint64_t restart_until = 0;  // the walk's swap changed: restart a walk seen before this (ms)
  uint64_t idle_until = 0;     // the idle's swap changed while it moved: restart its idle when it stops (ms)
  bool layers_pending = false;  // the walk's arm layers wait till it stands still (see the top)
  uint64_t still_since = 0;     // ... for 300 ms (its stop has reached the client)
};

// A walking creature's walk restarted once through its new swap (see above);
// false while it isn't walking yet.
bool RestartWalk(uint32_t id, Look& look) {
  static const auto set_animation = reinterpret_cast<SetAnimationFn>(EngineExport(ee::kCreatureSetAnimation));
  if (!look.restart_until) return false;
  if (GetTickCount64() > look.restart_until) {
    look.restart_until = 0;
    return false;
  }
  void* creature = CreatureById(id);
  const int playing = Playing(creature);
  if (!set_animation || (playing != kAnimWalk && playing != kAnimWalkBack)) return false;
  set_animation(creature, 0);
  look.restart_until = 0;
  Log("look: 0x%08X walk restarted through the new swap (walk %s)", id, look.walk.empty() ? "normal" : look.walk.c_str());
  return true;
}
std::unordered_map<uint32_t, Look> g_looks;  // by creature, the engine's thread only
int g_crouch = -1;                           // the crouch recipes were built: -1 not asked yet

bool CrouchBuilt() {
  if (g_crouch < 0) {
    std::string text;
    g_crouch = AnimationRecipeBuilt(kCrouchIdle, &text) && AnimationRecipeBuilt(kCrouchWalk, &text) ? 1 : 0;
    Log(g_crouch ? "look: hiding creatures crouch (%s, %s)"
                 : "look: no crouch when hiding - %s / %s weren't built (animation_recipes)",
        kCrouchIdle, kCrouchWalk);
  }
  return g_crouch == 1;
}

// The weapon arm layers' swaps for a walk: the crouch walk's own (built by
// anim_packs.cpp), else removed (a limp or the game's walk: theirs).
std::string LayerCode(const std::string& walk) {
  std::string code;
  for (const char* hold : kHolds) {
    const std::string layer = walk == kCrouchWalk ? std::string(kCrouchWalk) + "_" + hold : "";
    std::string text;
    code += std::string("ReplaceObjectAnimation(OBJECT_SELF, \"walk_") + hold + "\", \"" +
            (!layer.empty() && AnimationRecipeBuilt(layer, &text) ? layer : "") + "\");";
  }
  return code;
}

void Send(uint32_t id, Look& look) {
  const bool crouch = look.hiding && CrouchBuilt();
  const std::string idle = crouch ? kCrouchIdle : look.exhaustion >= 1 ? "pausetrd" : "";
  const std::string walk = crouch ? kCrouchWalk : look.exhaustion >= 3 ? "walkinj" : "";
  if (look.sent && idle == look.idle && walk == look.walk) return;
  std::string code;
  const auto slot = [&](const char* name, const std::string& with) {
    code += std::string("ReplaceObjectAnimation(OBJECT_SELF, \"") + name + "\", \"" + (with.empty() ? name : with) + "\");";
  };
  // The walk's: removed when it's the game's own (see the top).
  const auto walk_slot = [&](const char* name, const std::string& with) {
    code += std::string("ReplaceObjectAnimation(OBJECT_SELF, \"") + name + "\", \"" + with + "\");";
  };
  void* creature = CreatureById(id);
  const int playing = Playing(creature);
  const bool moving = !Stationary(creature);
  const bool kick = look.sent && idle != look.idle && !moving;
  if (look.sent && idle != look.idle) look.idle_until = moving ? GetTickCount64() + 10000 : 0;
  if (!look.sent || idle != look.idle) {
    slot("pause1", idle);
    slot("pause2", idle);
    if (kick) {
      code += "int busy = GetCurrentAction(OBJECT_SELF); SetLocalInt(OBJECT_SELF, \"nwn5e_look_kick\", busy);"
              "if (busy == ACTION_INVALID) PlayAnimation(ANIMATION_LOOPING_PAUSE2, 1.0, 0.5);";
    }
  }
  if (!look.sent || walk != look.walk) {
    walk_slot("walk", walk);
    // The weapon arm layers follow - while it moves, once it stands still (see the top).
    if (look.sent && moving) {
      look.layers_pending = true;
    } else {
      code += LayerCode(walk);
      look.layers_pending = false;
    }
  }
  const bool first = !look.sent;
  const bool walk_changed = look.sent && walk != look.walk;
  look.sent = true;
  look.idle = idle;
  look.walk = walk;
  RunScriptChunk(("if (Get2DAString(\"appearance\", \"MODELTYPE\", GetAppearanceType(OBJECT_SELF)) == \"P\") {" + code + "}")
                     .c_str(),
                 id);
  if (walk_changed) {
    look.restart_until = GetTickCount64() + kRestartWindowMs;
    RestartWalk(id, look);
  }
  if (!first || !idle.empty() || !walk.empty()) {
    char note[96] = "";
    if (!first && creature) {
      const int busy = kick ? engine::GetLocalInt(creature, "nwn5e_look_kick") : -1;
      if (moving) {
        sprintf_s(note, "; moving (animation %d), idle not restarted", playing);
      } else if (kick) {
        sprintf_s(note, busy == 65535 ? "; idle restarted (animation %d)" : "; busy (action %d), idle not restarted",
                  busy == 65535 ? playing : busy);
      } else {
        sprintf_s(note, "; animation %d", playing);
      }
    }
    Log("look: 0x%08X idle %s, walk %s (%s, Exhaustion %d)%s", id, idle.empty() ? "normal" : idle.c_str(),
        walk.empty() ? "normal" : walk.c_str(), look.hiding ? "hiding" : "not hiding", look.exhaustion, note);
  }
}

// Each creature, each pass of the AI loop: did it start or stop hiding?
void Update(void* creature) {
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  auto it = g_looks.find(id);
  if (it == g_looks.end()) {
    // A player character: its saved swaps cleaned once (the walk's removed, idles its own).
    const bool player = Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0;
    if (!hiding && !player) return;  // the common case: nothing to do
    it = g_looks.emplace(id, Look{}).first;
    if (!hiding) {
      Send(id, it->second);
      Log("look: 0x%08X its walk swaps cleared (a new session: the game's walk, its idles its own)", id);
      return;
    }
  }
  Look& look = it->second;
  if (look.restart_until) RestartWalk(id, look);
  if (look.layers_pending && !Stationary(creature)) look.still_since = 0;
  if (look.layers_pending && Stationary(creature) &&
      (look.still_since ? GetTickCount64() - look.still_since >= 300 : (look.still_since = GetTickCount64(), false))) {
    // It stands still: its arm layers follow the walk now (see the top).
    look.layers_pending = false;
    look.still_since = 0;
    RunScriptChunk(("if (Get2DAString(\"appearance\", \"MODELTYPE\", GetAppearanceType(OBJECT_SELF)) == \"P\") {" +
                    LayerCode(look.walk) + "}")
                       .c_str(),
                   id);
    Log("look: 0x%08X stopped - arm layers follow the walk (%s)", id, look.walk.empty() ? "normal" : look.walk.c_str());
  }
  if (look.idle_until) {
    // It moved when its idle changed: restart the idle once it stands still with nothing to do.
    if (GetTickCount64() > look.idle_until) {
      look.idle_until = 0;
    } else if (Stationary(creature)) {
      RunScriptChunk("if (GetCurrentAction(OBJECT_SELF) == ACTION_INVALID) {"
                     " PlayAnimation(ANIMATION_LOOPING_PAUSE2, 1.0, 0.5); SetLocalInt(OBJECT_SELF, \"nwn5e_look_kick\", 1); }",
                     id);
      if (engine::GetLocalInt(creature, "nwn5e_look_kick") == 1) {
        engine::SetLocalInt(creature, "nwn5e_look_kick", 0);
        look.idle_until = 0;
        Log("look: 0x%08X stopped - idle restarted (idle %s)", id, look.idle.empty() ? "normal" : look.idle.c_str());
      }
    }
  }
  if (look.hiding == hiding && look.sent) return;
  look.hiding = hiding;
  Send(id, look);
}

}  // namespace

// rules_dying.cpp, at each upkeep: the character's Exhaustion level (0 when the
// exhausted look is off or it's dead from it).
void SetExhaustionLook(uint32_t id, int level) {
  Look& look = g_looks[id];
  look.exhaustion = level;
  Send(id, look);
}

// !anim's Stop puts the animations back: the look is sent again (now, and at the
// next upkeep or AI pass if the creature isn't known yet).
void ResendLook(uint32_t id) {
  auto it = g_looks.find(id);
  if (it == g_looks.end()) return;
  it->second.sent = false;
  Send(id, it->second);
}

void InstallStanceLook() {
  if (!g_config.stealth_crouch) return;
  if (AddCreatureUpdateListener(&Update)) Log("look: a hiding creature crouches (stealth_crouch)");
}
