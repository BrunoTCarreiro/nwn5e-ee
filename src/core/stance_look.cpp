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
// restarted (pause2 for an instant, then pause1: both through the new swap).

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "log.h"

bool AnimationRecipeBuilt(const std::string& name, std::string* text);  // anim_packs.cpp

namespace {

constexpr const char* kCrouchIdle = "n5_cridle";
constexpr const char* kCrouchWalk = "n5_crwalk";
constexpr const char* kHolds[] = {"swordr", "swordl", "shieldl", "bowl"};  // walk_<hold>: anim_packs.cpp

struct Look {
  int exhaustion = 0;
  bool hiding = false;
  bool sent = false;  // false: nothing sent yet - the next send restores what isn't swapped too
  std::string idle, walk;
};
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

void Send(uint32_t id, Look& look) {
  const bool crouch = look.hiding && CrouchBuilt();
  const std::string idle = crouch ? kCrouchIdle : look.exhaustion >= 1 ? "pausetrd" : "";
  const std::string walk = crouch ? kCrouchWalk : look.exhaustion >= 3 ? "walkinj" : "";
  if (look.sent && idle == look.idle && walk == look.walk) return;
  std::string code;
  const auto slot = [&](const char* name, const std::string& with) {
    code += with.empty() ? std::string("ReplaceObjectAnimation(OBJECT_SELF, \"") + name + "\");"
                         : std::string("ReplaceObjectAnimation(OBJECT_SELF, \"") + name + "\", \"" + with + "\");";
  };
  if (!look.sent || idle != look.idle) {
    slot("pause1", idle);
    slot("pause2", idle);
    if (look.sent) code += "if (GetCurrentAction(OBJECT_SELF) == ACTION_INVALID) PlayAnimation(ANIMATION_LOOPING_PAUSE2, 1.0, 0.1);";
  }
  if (!look.sent || walk != look.walk) {
    slot("walk", walk);
    // The weapon layers follow the crouch walk (a limp or the game's walk: theirs).
    for (const char* hold : kHolds) {
      const std::string layer = walk == kCrouchWalk ? std::string(kCrouchWalk) + "_" + hold : "";
      std::string text;
      slot(("walk_" + std::string(hold)).c_str(), !layer.empty() && AnimationRecipeBuilt(layer, &text) ? layer : "");
    }
  }
  const bool first = !look.sent;
  look.sent = true;
  look.idle = idle;
  look.walk = walk;
  RunScriptChunk(("if (Get2DAString(\"appearance\", \"MODELTYPE\", GetAppearanceType(OBJECT_SELF)) == \"P\") {" + code + "}")
                     .c_str(),
                 id);
  if (!first || !idle.empty() || !walk.empty()) {
    Log("look: 0x%08X idle %s, walk %s (%s, Exhaustion %d)", id, idle.empty() ? "normal" : idle.c_str(),
        walk.empty() ? "normal" : walk.c_str(), look.hiding ? "hiding" : "not hiding", look.exhaustion);
  }
}

// Each creature, each pass of the AI loop: did it start or stop hiding?
void Update(void* creature) {
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  auto it = g_looks.find(id);
  if (it == g_looks.end()) {
    if (!hiding) return;  // the common case: nothing to do
    it = g_looks.emplace(id, Look{}).first;
  }
  Look& look = it->second;
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
