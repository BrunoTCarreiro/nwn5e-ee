// Walking backwards with the keyboard while walk-limited (owner, 2026-10-26:
// "walking back while crouched or standing still has issues, a lot of
// stuttering. In some cases the character doesn't seem to be able to move much
// it just stutters the position"; !animtrace).
//
// The engine: each keyboard-movement message (CNWSMessage::
// HandlePlayerToServerInputDriveControl) adds a drive action through
// CNWSCreature::AddDriveAction(group, position, facing, ..., keys, ...); the
// keys are 1 forward, 2 backward, 3 forward running, 4 / 8 sideways, and
// AIActionDrive moves the creature by them (backward always at the walk rate)
// and picks its animation (walk-back 3 for 2; run 4 when no key is left). A
// walk-limited creature (+0x76c: OnApplyLimitMovementSpeed - the effect
// ComputeModifiedMovementRate applies for any movement stage penalty: stealth
// mode, detect mode, heavy encumbrance; ruleset MOVEMENT_STAGE_PENALTY_*) has
// its run turned into a walk in AIActionDrive (3 -> 1) - but AddDriveAction
// also clears the keys' 2 for it, which is the backward key: the server never
// stepped a hider (or a searcher, or a heavily loaded character) backwards,
// while its client did - the client's position pulled back over and over, and
// the run animation (4, the no-key default) on top. The unmodded game too.
// Here, when backward is the only direction held, the limit is left out of
// AddDriveAction (backing up is walked anyway); AIActionDrive still limits a
// forward run. `drive_backward_fix`.
//
// Also logged: each time a player character's walk limit turns on or off, and
// why (detect / stealth mode, encumbrance) - CNWSCreature::
// ComputeModifiedMovementRate.

#include <windows.h>

#include <stdint.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "dev_seam.h"


namespace {

using AddDriveActionFn = int (*)(void* creature, uint16_t group, const float* target, int facing, int a, int keys,
                                 int c);
using ComputeMovementRateFn = void (*)(void* creature);
AddDriveActionFn g_add_drive = nullptr;
ComputeMovementRateFn g_compute_rate = nullptr;
constexpr const char* kAddDriveAction = "?AddDriveAction@CNWSCreature@@QEAAHGAEBVVector@@HHHH@Z";
constexpr const char* kComputeMovementRate = "?ComputeModifiedMovementRate@CNWSCreature@@QEAAXXZ";
constexpr size_t kCreatureLimitSpeed = 0x76c;  // int32: walk-limited (OnApply / OnRemoveLimitMovementSpeed)
constexpr size_t kCreatureEncumbrance = ee::kCreatureEncumbrance;  // 0 none, 1 heavy, 2 overloaded
constexpr int kKeyForward = 1, kKeyBackward = 2;
LogOnce g_fixed_logged(64);  // the first fix per creature

int AddDriveAction(void* creature, uint16_t group, const float* target, int facing, int a, int keys, int c) {
  if (g_dev_seam.anim_drive) g_dev_seam.anim_drive(creature, target, facing, a, keys, c);  // !animtrace (dev layer)
  const int32_t limit = creature ? Field<int32_t>(creature, kCreatureLimitSpeed) : 0;
  if (!limit || !g_config.drive_backward_fix || (keys & (kKeyForward | kKeyBackward)) != kKeyBackward) {
    return g_add_drive(creature, group, target, facing, a, keys, c);
  }
  // Backward only: AddDriveAction would clear the key (see the top) - the limit is left out of it.
  SetField<int32_t>(creature, kCreatureLimitSpeed, 0);
  const int result = g_add_drive(creature, group, target, facing, a, keys, c);
  SetField<int32_t>(creature, kCreatureLimitSpeed, limit);
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (g_fixed_logged.First(id)) Log("drive: 0x%08X walks backwards while walk-limited (the backward key kept)", id);
  return result;
}

// A player character's walk limit: on / off, and why - logged when it changes.
std::unordered_map<uint32_t, int> g_limit_state;

void ComputeModifiedMovementRate(void* creature) {
  g_compute_rate(creature);
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const int detect = Field<uint8_t>(creature, ee::kCreatureDetectMode);
  const int stealth = Field<uint8_t>(creature, ee::kCreatureStealthMode);
  const int encumbrance = Field<int32_t>(creature, kCreatureEncumbrance);
  const int limit = Field<int32_t>(creature, kCreatureLimitSpeed);
  const int state = (limit ? 1 : 0) | (detect ? 2 : 0) | (stealth ? 4 : 0) | (encumbrance & 3) << 3;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto it = g_limit_state.find(id);
  if (it != g_limit_state.end() && it->second == state) return;
  g_limit_state[id] = state;
  Log("movement: 0x%08X %s (detect mode %d, stealth mode %d, encumbrance %d)", id,
      limit ? "walk-limited - can't run" : "not limited", detect, stealth, encumbrance);
}

}  // namespace

void InstallDriveFix() {
  if (HookExport("CNWSCreature::AddDriveAction (walking backwards while walk-limited)", kAddDriveAction,
                 reinterpret_cast<void*>(&AddDriveAction), reinterpret_cast<void**>(&g_add_drive))) {
    Log("drive: %s", g_config.drive_backward_fix
                         ? "a walk-limited character (stealth / detect mode, heavy load) walks backwards with the "
                           "keyboard (drive_backward_fix)"
                         : "the game's own - a walk-limited character can't walk backwards with the keyboard "
                           "(drive_backward_fix=0)");
  }
  HookExport("CNWSCreature::ComputeModifiedMovementRate (a player's walk limit, logged)", kComputeMovementRate,
             reinterpret_cast<void*>(&ComputeModifiedMovementRate), reinterpret_cast<void**>(&g_compute_rate));
}
