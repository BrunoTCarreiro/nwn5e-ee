// G2 Grapple (docs/PLAN.md "Grapple and Throw"; owner, 2026-10-27: "go with the
// recommendations" - D1-D8), `knockdown` faithful (the Unarmed Strike's options,
// unarmed.h):
// - nwn5e's feat 1312, every player character's while the Shove is faithful
//   (rules_features.cpp), used on a creature from the radial's Special Attacks
//   page (radial_special_attacks.cpp: Called Shot: Leg's slot, next to the
//   Shove), the quickbar or !grapple. Queued and resolved as the Shove's push:
//   no attack roll, no damage, the nwstab swing (D7).
// - D1: it needs a free hand - an empty main hand, or a one-handed weapon with
//   nothing in the off hand (a two-handed weapon counts by the 2024 table, a
//   double weapon, or one larger than its wielder - a Small wielder counting as
//   Medium, the size rule's).
// - The target, at most one size larger, makes the Shove's save (the better of
//   Strength and Dexterity against 8 + Str + PB); on a failure it is Grappled,
//   the escape DC the same. D3: one creature at a time; one already grappled
//   can't be grabbed again.
// - D4 Grappled: it can't move (EffectCutsceneImmobilize) and shows the
//   Grappled icon (effecticons 223), both temporary and refreshed while the
//   grapple lasts (a loaded game drops them in seconds); its attacks against
//   anyone but its grappler have Disadvantage (rules_advantage.cpp). GT3: one
//   the game moves that isn't fighting its grappler tries to escape once a
//   round - d20 + the better of Athletics and Acrobatics (the engine's skill
//   ranks: 2024 values with `skills` faithful) against the DC - its round paused
//   for the try (in place of its attacks).
// - D5 the drag: the grappled creature is held at the distance it was grabbed
//   at (1-2 m); when the grappler moves farther it is placed back at that
//   distance along the line between them (rules_push's DragCreatureTo: walkable
//   and clear of placeables, else it stays). The grappler moves at half speed
//   (GetMovementRateFactor, movement_rate.cpp) unless it is Tiny or two or more
//   sizes smaller.
// - D6 it ends: Grapple on it again (or !grapple release) lets go; it escapes;
//   the grappler is incapacitated or down; the grappler's hand is no longer
//   free; it is out of reach (more than 1.5 m past the hold - pushed, stuck
//   behind a wall); either is gone, dead or in another area.
// - D8: a player grappled (monsters' grabs, G3) - its Escape comes with G3.
// The grapples live in memory only: a loaded game starts with none.

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "movement_rate.h"
#include "rules/class_features.h"
#include "rules/grapple.h"
#include "rules/skills.h"
#include "unarmed.h"

namespace {

using SkillRankFn = int8_t (*)(void* stats, uint8_t skill, void* vs, int base_only);
struct Vector3 {
  float x, y, z;
};
using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          Vector3* location);

constexpr const char* kGrappledTag = "nwn5e_grappled";
constexpr int kGrappledIconRow = 223;          // effecticons.txt NWN5E_GRAPPLED
constexpr float kHeldEffectSeconds = 3.0f;     // the held effects, refreshed while the grapple lasts
constexpr DWORD kRefreshEveryMs = 2000;
constexpr float kDragStartMetres = 0.2f;       // dragged once the grappler is this far past the hold
constexpr int kStruggleMs = 3000;              // an escape try's share of the round
constexpr int kLogMax = 400;

struct Grapple {
  uint32_t grappler;
  uint32_t target;
  void* area;
  float hold;   // metres between them (D5)
  int dc;       // the escape DC
  bool slows;   // the grappler drags at half speed
  DWORD next_refresh;
  DWORD next_escape;
};
std::vector<Grapple> g_grapples;  // few: one per grappler (D3)
struct Release {
  uint32_t grappler;
  uint32_t target;
};
std::vector<Release> g_releases;  // held effects to take off in the next creature update
SkillRankFn g_skill_rank = nullptr;
int g_logged = 0;

bool LogSlot() { return g_logged < kLogMax && ++g_logged; }
bool Faithful() { return g_config.knockdown == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

float Distance2D(const void* a, const void* b) {
  const float dx = Field<float>(a, ee::kObjectPositionX) - Field<float>(b, ee::kObjectPositionX);
  const float dy = Field<float>(a, ee::kObjectPositionY) - Field<float>(b, ee::kObjectPositionY);
  return sqrtf(dx * dx + dy * dy);
}

Grapple* ByGrappler(uint32_t id) {
  for (Grapple& g : g_grapples) {
    if (g.grappler == id) return &g;
  }
  return nullptr;
}

Grapple* ByTarget(uint32_t id) {
  for (Grapple& g : g_grapples) {
    if (g.target == id) return &g;
  }
  return nullptr;
}

// "nwn5e: <before><its name><after>" to a player (the name through the script bridge).
void TellAbout(uint32_t player, const std::string& before, uint32_t other, const std::string& after) {
  if (player == ee::kObjectInvalid) return;
  char hex[16];
  sprintf_s(hex, "%x", other);
  const std::string script = "SendMessageToPC(OBJECT_SELF, \"nwn5e: " + before + "\" + GetName(StringToObject(\"" + hex +
                             "\")) + \"" + after + "\");";
  RunScriptChunk(script.c_str(), player);
}

// D1: a two-handed hold - the 2024 Two-Handed property, a double weapon, or a
// weapon larger than its wielder (a Small one wields as Medium).
bool WieldedTwoHanded(void* creature, void* item) {
  const int row = engine::BaseItemRow(item);
  const void* base = engine::BaseItem(row);
  if (!base) return false;
  if (Field<uint8_t>(base, ee::kBaseItemWield) == ee::kWieldDouble || rules::IsTwoHandedWeapon2024(row)) return true;
  const int wielder = std::max(Field<int32_t>(creature, ee::kCreatureSize), ee::kCreatureSizeMedium);
  return Field<uint8_t>(base, ee::kBaseItemSize) > wielder;
}

bool HandFree(void* creature) {
  void* main_hand = engine::ItemInSlot(creature, ee::kSlotRightHand);
  void* off_hand = engine::ItemInSlot(creature, ee::kSlotLeftHand);
  return rules::GrappleHandFree(main_hand != nullptr, main_hand && WieldedTwoHanded(creature, main_hand),
                                off_hand != nullptr);
}

// The held effects on the target, on (refreshed) or off - created by the grappler.
void ShowHeld(uint32_t grappler, uint32_t target, bool on) {
  char hex[16];
  sprintf_s(hex, "%x", target);
  std::string script = std::string("object t = StringToObject(\"") + hex + "\"); effect e = GetFirstEffect(t); "
                       "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"" + kGrappledTag + "\") "
                       "RemoveEffect(t, e); e = GetNextEffect(t); }";
  if (on) {
    char seconds[16];
    sprintf_s(seconds, "%.1f", kHeldEffectSeconds);
    script += std::string(" ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect("
                          "EffectLinkEffects(EffectCutsceneImmobilize(), EffectIcon(") +
              std::to_string(kGrappledIconRow) + "))), \"" + kGrappledTag + "\"), t, " + seconds + ");";
  }
  RunScriptChunk(script.c_str(), CreatureById(grappler) ? grappler : target);
}

// Ends a grapple (its held effects come off in the next creature update) and
// says why to the players among the two.
void EndGrapple(uint32_t grappler, const std::string& to_grappler, const std::string& to_target, const char* why) {
  const auto it = std::find_if(g_grapples.begin(), g_grapples.end(),
                               [grappler](const Grapple& g) { return g.grappler == grappler; });
  if (it == g_grapples.end()) return;
  const Grapple g = *it;
  g_grapples.erase(it);
  g_releases.push_back(Release{g.grappler, g.target});
  void* grappler_creature = CreatureById(g.grappler);
  void* target_creature = CreatureById(g.target);
  if (!to_grappler.empty() && grappler_creature && IsPlayer(grappler_creature)) {
    TellAbout(g.grappler, to_grappler, g.target, ".");
  }
  if (!to_target.empty() && target_creature && IsPlayer(target_creature)) {
    TellAbout(g.target, to_target, g.grappler, ".");
  }
  if (LogSlot()) Log("grapple: 0x%08X lets go of 0x%08X - %s", g.grappler, g.target, why);
}

int SkillBonus(void* stats, int skill) {
  return g_skill_rank && stats ? g_skill_rank(stats, static_cast<uint8_t>(skill), nullptr, 0) : 0;
}

// GT3: once a round, a grappled creature the game moves that isn't fighting its
// grappler tries to escape; true when it got free.
bool EscapeTry(Grapple& g, void* target, DWORD now) {
  if (now < g.next_escape || IsPlayer(target)) return false;
  g.next_escape = now + rules::kGrappleEscapeEveryMs;
  const bool fighting_grappler = Field<uint32_t>(target, ee::kCreatureAttackTarget) == g.grappler;
  if (!rules::GrappledCreatureTriesToEscape(fighting_grappler)) return false;
  void* stats = StatsOf(target);
  const int athletics = SkillBonus(stats, rules::kNwnAthletics);
  const int acrobatics = SkillBonus(stats, ee::kSkillTumble);
  const int bonus = rules::EscapeCheckBonus(athletics, acrobatics);
  const int d20 = RollEngineDice(1, 20);
  const bool free = rules::EscapeSucceeds(d20, bonus, g.dc);
  char check[96];
  sprintf_s(check, " (%s %+d: %d vs DC %d)", athletics >= acrobatics ? "Athletics" : "Acrobatics", bonus, d20 + bonus,
            g.dc);
  const uint32_t grappler_id = g.grappler, target_id = g.target;  // EndGrapple drops `g`
  if (LogSlot()) Log("grapple: 0x%08X tries to escape 0x%08X%s - %s", target_id, grappler_id, check, free ? "free" : "held");
  void* grappler = CreatureById(grappler_id);
  const bool tell = grappler && IsPlayer(grappler);
  if (free) {
    EndGrapple(grappler_id, "", "", "it escaped");
    if (tell) TellAbout(grappler_id, "Grapple: ", target_id, std::string(" escapes your grapple") + check);
    return true;
  }
  PauseCombatRound(target, grappler_id, kStruggleMs);  // the try in place of its attacks
  if (tell) TellAbout(grappler_id, "Grapple: ", target_id, std::string(" struggles against your grapple") + check + " - held");
  return false;
}

// The grappler's update: the end conditions, the drag, the held effects, the escape try.
void UpdateGrapple(Grapple& g, void* grappler, DWORD now) {
  void* target = CreatureById(g.target);
  const char* ended = nullptr;
  std::string to_grappler;
  if (!target || engine::IsDeadOrDying(target)) {
    ended = "it is down or gone";
  } else if (engine::IsDeadOrDying(grappler) || IncapacitatedByCondition(grappler)) {
    ended = "the grappler is incapacitated";
    to_grappler = "Grapple: you can't hold on - you let go of ";
  } else if (engine::AreaOf(grappler) != g.area || engine::AreaOf(target) != g.area) {
    ended = "another area";
  } else if (!HandFree(grappler)) {
    ended = "the grappler's hand isn't free";
    to_grappler = "Grapple: your hand isn't free any more - you let go of ";
  }
  if (ended) {
    EndGrapple(g.grappler, to_grappler, to_grappler.empty() ? "" : "Grapple: you are free of ", ended);
    return;
  }
  // D5: dragged back to the hold distance along the line between them.
  float distance = Distance2D(grappler, target);
  if (distance > g.hold + kDragStartMetres) {
    const float gx = Field<float>(grappler, ee::kObjectPositionX), gy = Field<float>(grappler, ee::kObjectPositionY);
    const float tx = Field<float>(target, ee::kObjectPositionX), ty = Field<float>(target, ee::kObjectPositionY);
    const float scale = g.hold / distance;
    if (DragCreatureTo(target, gx + (tx - gx) * scale, gy + (ty - gy) * scale)) distance = Distance2D(grappler, target);
  }
  if (rules::GrappleOutOfReach(distance, g.hold)) {
    EndGrapple(g.grappler, "Grapple: out of reach - you let go of ", "Grapple: you are free of ", "out of reach");
    return;
  }
  if (now >= g.next_refresh) {
    g.next_refresh = now + kRefreshEveryMs;
    ShowHeld(g.grappler, g.target, true);
  }
  EscapeTry(g, target, now);
}

void GrappleUpdate(void* creature) {
  if (!g_releases.empty()) {
    const std::vector<Release> releases = std::move(g_releases);
    g_releases.clear();
    for (const Release& r : releases) {
      if (CreatureById(r.target) && !ByTarget(r.target)) ShowHeld(r.grappler, r.target, false);
    }
  }
  if (g_grapples.empty()) return;
  Grapple* g = ByGrappler(IdOf(creature));
  if (g) UpdateGrapple(*g, creature, GetTickCount());
}

// D5: half speed while dragging (unless it is Tiny or two sizes smaller).
float GrappleMovementFactor(void* creature, float factor) {
  if (g_grapples.empty() || !creature) return factor;
  const Grapple* g = ByGrappler(IdOf(creature));
  return g && g->slows ? factor * 0.5f : factor;
}

// --- Using it: let go, refuse, or queue -----------------------------------------

int GrappleStart(void* creature, uint32_t target) {
  const uint32_t id = IdOf(creature);
  if (Grapple* g = ByGrappler(id)) {
    if (g->target == target) {
      EndGrapple(id, "Grapple: you let go of ", "Grapple: you are free of ", "let go");
      return 1;
    }
    TellUnarmed(creature, "nwn5e: Grapple - you are already grappling a creature (one at a time). Use Grapple on it "
                          "to let go first, or !grapple release.");
    return 0;
  }
  if (!HandFree(creature)) {
    TellUnarmed(creature, "nwn5e: Grapple - you need a free hand: nothing in your off hand and no two-handed weapon.");
    return 0;
  }
  return -1;
}

void ResolveGrapple(void* attacker, void* target) {
  void* creature = CreatureById(IdOf(target));
  if (!creature) {
    TellUnarmed(attacker, "nwn5e: Grapple - only a creature can be grappled.");
    return;
  }
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(creature);
  if (ByGrappler(attacker_id)) {
    TellUnarmed(attacker, "nwn5e: Grapple - you are already grappling a creature.");
    return;
  }
  if (ByTarget(target_id) || ByGrappler(target_id)) {
    TellUnarmed(attacker, "nwn5e: Grapple - it is already in a grapple.");
    return;
  }
  if (!HandFree(attacker)) {
    TellUnarmed(attacker, "nwn5e: Grapple - you need a free hand: nothing in your off hand and no two-handed weapon.");
    return;
  }
  const int grappler_size = Field<int32_t>(attacker, ee::kCreatureSize);
  const int target_size = Field<int32_t>(creature, ee::kCreatureSize);
  if (!rules::GrappleSizeAllowed(grappler_size, target_size)) {
    TellUnarmed(attacker, "nwn5e: Grapple - too big to grapple (at most one size larger than you).");
    if (LogSlot()) Log("grapple: 0x%08X on 0x%08X: too big (%d vs %d)", attacker_id, target_id, target_size,
                       grappler_size);
    return;
  }
  const UnarmedSave save = RollUnarmedSave(attacker, creature);
  if (!save.rolled) return;
  std::string outcome = save.immune ? "immune" : "made - not grappled";
  if (save.failed) {
    const DWORD now = GetTickCount();
    Grapple g{attacker_id, target_id, engine::AreaOf(attacker), rules::GrappleHoldMetres(Distance2D(attacker, creature)),
              save.dc, rules::DragCostsExtra(grappler_size, target_size, false), now,
              now + rules::kGrappleEscapeEveryMs};
    g_grapples.push_back(g);
    char text[200];
    sprintf_s(text, "failed - Grappled (escape DC %d). It can't move and comes with you%s. Grapple on it again lets go.",
              save.dc, g.slows ? " - you move at half speed while you hold it" : "");
    outcome = text;
    if (LogSlot()) {
      Log("grapple: 0x%08X grapples 0x%08X - hold %.1f m, escape DC %d, drag %s", attacker_id, target_id, g.hold,
          save.dc, g.slows ? "at half speed" : "at full speed");
    }
  }
  ReportUnarmed(attacker, creature, "Grapple", save, outcome);
}

// !grapple [release]: the creature you are attacking, or let go.
std::vector<std::string> GrappleCommand(void* creature, const std::string& args) {
  const uint32_t id = IdOf(creature);
  if (args.find("release") != std::string::npos || args.find("let go") != std::string::npos) {
    if (!ByGrappler(id)) return {"nwn5e: !grapple release - you aren't grappling anything."};
    EndGrapple(id, "Grapple: you let go of ", "Grapple: you are free of ", "let go (!grapple release)");
    return {};
  }
  if (const Grapple* g = ByGrappler(id)) {
    TellAbout(id, "Grapple: you are grappling ", g->target,
              " (escape DC " + std::to_string(g->dc) + "). !grapple release lets go.");
    return {};
  }
  const uint32_t target = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
  if (!CreatureById(target)) return {"nwn5e: !grapple: attack a creature first - or use Grapple on the radial."};
  static const auto use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through the hook
  if (!use_feat) return {"nwn5e: !grapple: no UseFeat in this game build."};
  Vector3 nowhere{0.0f, 0.0f, 0.0f};
  use_feat(creature, static_cast<uint16_t>(rules::kFeatGrapple), 0, target, ee::kObjectInvalid, &nowhere);
  return {};
}

}  // namespace

// rules_advantage.cpp: a grappled creature's attack on anyone but its grappler.
MasteryRollSources GrappleRollSources(void* attacker, void* target) {
  MasteryRollSources sources;
  if (g_grapples.empty() || !attacker || !target) return sources;
  const Grapple* g = ByTarget(IdOf(attacker));
  if (g && rules::GrappledAttackHasDisadvantage(IdOf(target) == g->grappler)) {
    sources.disadvantage = 1;
    sources.note = "Grappled (not its grappler): Disadvantage";
  }
  return sources;
}

void InstallGrappleRules() {
  if (!Faithful()) return;
  g_skill_rank = reinterpret_cast<SkillRankFn>(EngineExport(ee::kGetSkillRank));
  const bool on = AddUnarmedOption(static_cast<uint16_t>(rules::kFeatGrapple), "Grapple", &ResolveGrapple,
                                   &GrappleStart) &&
                  AddCreatureUpdateListener(&GrappleUpdate);
  if (!on) {
    Log("SKIP grapple: the Shove's hooks or the creature updates are off");
    return;
  }
  if (!AddMovementRateAdjuster(&GrappleMovementFactor)) Log("grapple: the drag keeps full speed (no movement hook)");
  AddChatCommand("!grapple", &GrappleCommand);
  Log("grapple: Grapple (feat 1312) - a free hand, the Shove's save, Grappled (can't move, Disadvantage against "
      "others), the drag, escapes, letting go (faithful)");
}
