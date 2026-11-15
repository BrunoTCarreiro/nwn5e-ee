// #166 part 1 (docs/PLAN.md "Cover"; owner, 2026-11-15: "what about half-cover?" - "go with your
// recommendations"): 2024 Half Cover from a creature in the way. GetArmorClassVersus (rules_attack.cpp) adds
// CoverArmorClassBonus to each attack's AC: another living creature, not Tiny, whose body reaches the line between
// the attacker and its target and stands between them gives the target +2 AC against that attack. Faithful only
// (the master mode: a 2024 rule with no NWN counterpart). Part 2, with #9's sight lines: walls and objects,
// Three-Quarters Cover, Dexterity saves.

#include <windows.h>

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "log.h"
#include "rules/cover.h"

namespace {

constexpr ULONGLONG kTellEveryMs = 6000;  // a round
constexpr int kLogMax = 200;

struct Seek {
  float ax = 0, ay = 0, tx = 0, ty = 0, attacker_radius = 0, target_radius = 0;
  uint32_t attacker = 0, target = 0, blocker = 0;
};

float X(const void* object) { return Field<float>(object, ee::kObjectPositionX); }
float Y(const void* object) { return Field<float>(object, ee::kObjectPositionY); }
int SizeOf(const void* creature) { return Field<int32_t>(creature, ee::kCreatureSize); }

void Visit(void* context, uint32_t id, void* creature) {
  Seek* seek = static_cast<Seek*>(context);
  if (seek->blocker || id == seek->attacker || id == seek->target) return;
  if (engine::IsDeadOrDying(creature) || !rules::SizeGivesCover(SizeOf(creature))) return;
  if (rules::CreatureInTheWay(seek->ax, seek->ay, seek->tx, seek->ty, X(creature), Y(creature),
                              rules::CreatureBodyRadius(SizeOf(creature)), seek->attacker_radius,
                              seek->target_radius)) {
    seek->blocker = id;
  }
}

std::unordered_map<uint64_t, ULONGLONG> g_told;  // (attacker, target) -> when it was last told and logged
int g_logged = 0;

}  // namespace

bool CoverOn() { return g_config.mode == rules::RuleVariant::kFaithful; }

// The AC bonus cover gives `target_stats`' creature against `attacker` (a creature): +2 when a creature is in the
// way, else 0.
int CoverArmorClassBonus(void* target_stats, void* attacker) {
  if (!CoverOn() || !target_stats || !attacker) return 0;
  void* target = CreatureOf(target_stats);
  if (!target || target == attacker || engine::AreaOf(target) != engine::AreaOf(attacker)) return 0;
  Seek seek;
  seek.ax = X(attacker);
  seek.ay = Y(attacker);
  seek.tx = X(target);
  seek.ty = Y(target);
  seek.attacker_radius = rules::CreatureBodyRadius(SizeOf(attacker));
  seek.target_radius = rules::CreatureBodyRadius(SizeOf(target));
  seek.attacker = Field<uint32_t>(attacker, ee::kObjectId);
  seek.target = Field<uint32_t>(target, ee::kObjectId);
  engine::ForEachOtherCreatureInArea(target, &Visit, &seek);
  if (!seek.blocker) return 0;
  const int bonus = rules::CoverArmorClassBonus(rules::Cover::kHalf);
  const ULONGLONG now = GetTickCount64();
  if (g_told.size() > 512) g_told.clear();
  ULONGLONG& told = g_told[(static_cast<uint64_t>(seek.attacker) << 32) | seek.target];
  if (told == 0 || now >= told + kTellEveryMs) {
    told = now;
    if (g_logged < kLogMax) {
      ++g_logged;
      Log("cover: 0x%08X has Half Cover (+%d AC) against 0x%08X - 0x%08X is in the way", seek.target, bonus,
          seek.attacker, seek.blocker);
    }
    TellPlayer(seek.attacker, "nwn5e: your target has Half Cover (+2 AC) - a creature stands in the way.");
    TellPlayer(seek.target, "nwn5e: Half Cover (+2 AC) - a creature stands between you and the attack.");
  }
  return bonus;
}
