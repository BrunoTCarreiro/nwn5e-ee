// #131 (owner, 2026-11-04: "1. hmmmmmm, I'm not sure if we're entering some sort of deeper game mechanics but I feel that most living enemies would avoid hazards, while undead and enemies that are charmed or somehow dominated would not / 2 B / 3 B"):
// living AI creatures keep out of a hazard that would hurt them.
// - Which: the grenade hazards (#130's flask patches, the Bombs' clouds, Caltrops, Choking Powder: anyone - Holy
//   Water's patch only undead and evil outsiders) and the harmful spell areas (Acid Fog, Incendiary Cloud, Stinking
//   Cloud, Cloudkill, Wall of Fire, Blade Barrier, Web, Entangle, Grease ...: whom NWN's area-spell rule picks -
//   x0_i0_spells' spellsIsTarget, SPELL_TARGET_STANDARDHOSTILE, written out here) - rules::HazardOfScript.
// - Who: not a player's character, living (not undead, not a construct), in control of itself (not charmed,
//   dominated or confused) - rules::AvoidsHazards; monsters and a player's associates alike.
// - 3B: each time a hazard's enter, exit or round script runs (script_hooks.cpp's observer), the creatures in it are
//   listed (GetFirstInPersistentObject) with whether it hurts them; an avoiding one it hurts steps out - ClearAllActions,
//   then ActionMoveAwayFromLocation(its centre, running, its radius + 1.5 m) - at most once in 4 s. The lists are
//   kept, and an avoiding creature's melee attack on a target standing in a hazard that would hurt it, while it stands
//   outside, waits (action_order.cpp's AIActionAttackObject hook: HazardHoldsAttack) - a ranged attack goes on.
// With the grenades faithful (the hazards that hurt allies are theirs).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <string>
#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "grenades.h"
#include "light_sources.h"
#include "log.h"
#include "script_hooks.h"
#include "rules/hazard_avoid.h"

bool HoldsRangedWeapon(void* creature);  // light_sources.cpp (baseitems.2da RangedWeapon)
bool IsDownHenchmanUnderRules(uint32_t id);  // rules_dying.cpp (a downed henchman lies there)

namespace {

struct Hazard {
  rules::HazardKind kind = rules::HazardKind::kNone;
  uint32_t creator = 0;
  std::unordered_map<uint32_t, bool> inside;  // creature id -> the hazard hurts it
  int64_t seen_ms = 0;
};
std::unordered_map<uint32_t, Hazard> g_hazards;   // area of effect id -> what's in it
std::unordered_map<uint32_t, int64_t> g_stepped;  // creature id -> when it last stepped out
std::unordered_map<uint32_t, int64_t> g_held;     // creature id -> when its held attack was last logged
int g_lines = 0;

bool LogSlot() { return g_lines < 300 && ++g_lines; }
int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

constexpr int kStateConfused = 2;   // EffectConfused's state
constexpr int kStateDominated = 7;  // EffectDominated's (rules_monk.cpp)
constexpr uint16_t kAssociateDominated = 5;

// Charmed, dominated or confused: not in control of itself.
bool NotInControl(void* creature) {
  return effects::HasState(creature, ee::kStateCharmed) || effects::HasState(creature, kStateConfused) ||
         effects::HasState(creature, kStateDominated) ||
         Field<uint16_t>(creature, ee::kCreatureAssociateType) == kAssociateDominated;
}

bool Avoids(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return false;
  return rules::AvoidsHazards(GrenadesFaithful(), ReadCreatureFacts(stats).is_player, engine::RaceOf(stats),
                              NotInControl(creature));
}

// Run as the hazard: each creature in it, and whether the hazard hurts it.
const char* const kListHead = R"nss(
object c = GetAreaOfEffectCreator(); object cm = GetMaster(c); object mod = GetModule(); string out = "";
int hard = GetGameDifficulty() > GAME_DIFFICULTY_NORMAL;
object t = GetFirstInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  int hurt = !GetIsDead(t);
)nss";
// A spell's area: NWN's area-spell rule (x0_i0_spells' spellsIsTarget, SPELL_TARGET_STANDARDHOSTILE).
const char* const kHurtBySpell = R"nss(
  if (hurt) {
    object m = GetMaster(t);
    int pc = GetIsPC(t) || GetLocalInt(mod, "X0_G_ALLOWSPELLSTOHURT") == 10;
    int notfriend = !GetIsReactionTypeFriendly(t, c);
    int own = hard && (t == c || m == c);
    if (!own && GetIsObjectValid(m) && ((!GetIsReactionTypeFriendly(m, c) && GetIsPC(m)) || GetIsReactionTypeHostile(m, c))) own = TRUE;
    hurt = GetIsReactionTypeHostile(t, c) || own || (pc && notfriend) ||
           (notfriend && hard && GetLocalInt(mod, "X2_SWITCH_ENABLE_NPC_AOE_HURT_ALLIES"));
    if (GetIsObjectValid(m) && GetIsObjectValid(cm) && m == cm && !GetLocalInt(mod, "X2_SWITCH_MULTI_HENCH_AOE_MADNESS")) hurt = FALSE;
  }
)nss";
// Holy Water's patch: undead and evil outsiders.
const char* const kHurtByHoly = R"nss(
  if (hurt) {
    int r = GetRacialType(t);
    if (r != RACIAL_TYPE_UNDEAD && !(r == RACIAL_TYPE_OUTSIDER && GetAlignmentGoodEvil(t) == ALIGNMENT_EVIL)) hurt = FALSE;
  }
)nss";
const char* const kListTail = R"nss(
  out += ObjectToString(t) + (hurt ? ":1 " : ":0 ");
  t = GetNextInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "nwn5e_hazard_in", out);
SetLocalString(OBJECT_SELF, "nwn5e_hazard_by", ObjectToString(c));
)nss";

// A hazard's script ran: what's in it, and who steps out.
void HazardScriptRan(uint32_t aoe, const char* name) {
  const rules::HazardKind kind = rules::HazardOfScript(name);
  if (kind == rules::HazardKind::kNone || !GrenadesFaithful()) return;
  const std::string code = std::string(kListHead) +
                           (kind == rules::HazardKind::kSpell       ? kHurtBySpell
                            : kind == rules::HazardKind::kHolyWater ? kHurtByHoly
                                                                    : "") +
                           kListTail;
  if (RunScriptChunk(code.c_str(), aoe) != 0) {
    if (LogSlot()) Log("hazards: 0x%08X (%s) - its listing script failed", aoe, name);
    return;
  }
  void* object = GameObjectById(aoe);
  if (!object) return;
  const int64_t now = NowMs();
  Hazard& hazard = g_hazards[aoe];
  hazard.kind = kind;
  hazard.creator = static_cast<uint32_t>(strtoul(engine::GetLocalString(object, "nwn5e_hazard_by").c_str(), nullptr, 16));
  hazard.inside.clear();
  hazard.seen_ms = now;
  const std::string list = engine::GetLocalString(object, "nwn5e_hazard_in");
  for (const char* p = list.c_str(); *p;) {
    char* end = nullptr;
    const unsigned long id = strtoul(p, &end, 16);
    if (end == p || *end != ':') break;
    hazard.inside[static_cast<uint32_t>(id)] = end[1] == '1';
    for (p = end + 2; *p == ' '; ++p) {
    }
  }
  // Who steps out: an avoiding creature it hurts, once in 4 s.
  const float metres = rules::HazardStepOutMetres(name);
  std::string moves;
  for (const auto& in : hazard.inside) {
    if (!in.second) continue;
    void* creature = CreatureById(in.first);
    if (!creature || IsDownHenchmanUnderRules(in.first) || !Avoids(creature)) continue;  // 0.73.1: down can't move
    int64_t& last = g_stepped[in.first];
    if (now - last < 4000) continue;
    last = now;
    char line[200];
    sprintf_s(line, "t = StringToObject(\"%x\"); AssignCommand(t, ClearAllActions());"
                    " AssignCommand(t, ActionMoveAwayFromLocation(h, TRUE, %.2f));", in.first, metres);
    moves += line;
    if (LogSlot()) Log("hazards: 0x%08X steps out of 0x%08X (%s, %.1f m from its centre) (#131)", in.first, aoe, name, metres);
  }
  if (!moves.empty()) RunScriptChunk(("location h = GetLocation(OBJECT_SELF); object t;" + moves).c_str(), aoe);
  // Hazards gone (or not heard from for a while) forgotten.
  for (auto it = g_hazards.begin(); it != g_hazards.end();) {
    it = (now - it->second.seen_ms > 15000 || !GameObjectById(it->first)) ? g_hazards.erase(it) : std::next(it);
  }
}

}  // namespace

// 3B: an avoiding creature's melee attack on a target standing in a hazard that would hurt it, from outside, waits.
bool HazardHoldsAttack(void* creature, void* node) {
  if (g_hazards.empty() || !creature || !node) return false;
  const uint32_t target = Field<uint32_t>(node, ee::kActionFirstParam);
  const uint32_t self = IdOf(creature);
  bool checked = false, avoids = false, ranged = false;
  for (const auto& entry : g_hazards) {
    const Hazard& hazard = entry.second;
    if (!hazard.inside.count(target)) continue;
    if (!checked) {
      checked = true;
      avoids = Avoids(creature);
      ranged = avoids && HoldsRangedWeapon(creature);
    }
    if (!avoids) return false;
    bool hurt = true;
    if (hazard.kind == rules::HazardKind::kSpell) {
      void* creator = CreatureById(hazard.creator);
      hurt = creator && engine::IsEnemy(creator, self);
    } else if (hazard.kind == rules::HazardKind::kHolyWater) {
      void* stats = StatsOf(creature);
      hurt = stats && engine::RaceOf(stats) == rules::kRacialTypeOutsider;
    }
    if (rules::HazardHoldsAttack(avoids, hurt, true, hazard.inside.count(self) > 0, ranged) &&
        GameObjectById(entry.first)) {
      const int64_t now = NowMs();
      int64_t& logged = g_held[self];
      if (now - logged > 6000 && LogSlot()) {
        logged = now;
        Log("hazards: 0x%08X holds its attack on 0x%08X - the target stands in hazard 0x%08X (#131)", self, target,
            entry.first);
      }
      return true;
    }
  }
  return false;
}

void InstallHazardWatch() {
  if (!GrenadesFaithful()) return;
  static const char* const kNames[] = {
      "n5_fpacid_in",   "n5_fpacid_rd",    "n5_fpfire_in",    "n5_fpfire_rd",    "n5_fpholy_in",    "n5_fpholy_rd",
      "n5_bfire_in",    "n5_bfire_rd",     "n5_bacid_in",     "n5_bacid_rd",     "n5_bacid_out",    "x0_s3_calen",
      "x0_s3_calhb",    "x0_s3_chokeen",   "x0_s3_chokehb",   "nw_s0_acidfoga",  "nw_s0_acidfogb",  "nw_s0_acidfogc",
      "nw_s0_incclouda", "nw_s0_inccloudb", "nw_s0_inccloudc", "nw_s0_stinkclda", "nw_s0_stinkcldc",
      "nw_s0_cloudkilla", "nw_s0_cloudkillb", "nw_s0_cloudkillc", "nw_s0_wallfirea", "nw_s0_wallfirec",
      "nw_s0_bladebara", "nw_s0_bladebarb", "nw_s0_bladebarc", "nw_s0_weba", "nw_s0_webb", "nw_s0_webc",
      "nw_s0_entanglea", "nw_s0_entangleb", "nw_s0_entanglec", "nw_s0_greasea", "nw_s0_greaseb", "nw_s0_greasec",
      "nw_s0_crpdooma", "nw_s0_crpdoomb", "nw_s0_crpdoomc", "nw_s0_evardsa", "nw_s0_evardsc", "nw_s0_stormvenc",
      "x2_s0_cldbewlda", "x2_s0_cldbewldc", "x2_s0_stneholda", "x2_s0_stneholdc", "nw_s0_ghoultcha"};
  int watched = 0, missed = 0;
  for (const char* name : kNames) {
    if (rules::HazardOfScript(name) == rules::HazardKind::kNone) {
      ++missed;
      continue;
    }
    if (AddScriptObserver(name, &HazardScriptRan)) ++watched; else ++missed;
  }
  Log("hazards: %d hazard scripts watched - living creatures step out of a hazard that would hurt them and hold a "
      "melee attack on a target in one (#131)%s", watched, missed ? " - SOME NOT watched" : "");
}
