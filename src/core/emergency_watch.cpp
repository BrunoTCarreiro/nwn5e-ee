// The emergency search (#9; owner, 2026-11-23: "SHould we implement a "emergency" search mode? ... the most they can
// do is try to find me as quickly as possible" - then "yes to both, go with your recommendations"; rules/
// emergency_search.h). A damage listener: damage to a creature from a creature hiding that it doesn't see, hostile to
// it (Heat Metal's Again, a moved Moonbeam or Flaming Sphere, anything whose damage effect names the hider), starts
// or keeps up its emergency search:
// - search mode (SetActionMode DETECT: an active Perception roll inside its cone, its d20, as any searcher), set again
//   each round while it lasts;
// - to where it last heard the hider when that was within 12 s (rules_perception.cpp's HeardWhen), else it waits and
//   scans in place - it never simply knows where the hider is;
// - its glances quicker (held about emergency_search_glance_ms) and wider, up to right round (stealth_watch.cpp asks
//   EmergencyGlanceFor);
// - a shout: hostile creatures to the hider within emergency_search_alert_metres that can hear (not deafened), not
//   players' characters, go to the shouter's spot and search there (as an investigation);
// - fading back to a normal search emergency_search_fade_seconds after the last harm.
// Only damage: a harmful effect without damage (a condition) doesn't start it. Logged at each start and alert.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "damage_listeners.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "rules/emergency_search.h"

bool HeardWhen(uint32_t observer, uint32_t hider, int64_t* at, float* x, float* y, float* z);  // rules_perception.cpp
int64_t PerceptionNowMs();                                                                        // rules_perception.cpp

namespace {

struct Urgent {
  uint32_t hider = 0;
  int64_t last_harm = 0;
  int64_t detect_set = 0;
};
std::unordered_map<uint32_t, Urgent> g_urgent;      // creature id -> its emergency search
std::unordered_map<uint32_t, int64_t> g_alerted;    // creature id -> when a shout set it searching
rules::EmergencyTuning g_tuning;
rules::GlanceTuning g_glance;
int g_logged = 0;

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

bool On() {
  return g_config.emergency_search && g_config.hiders_need_search &&
         g_config.perception == rules::RuleVariant::kFaithful;
}

bool LogSlot() {
  if (g_logged >= 200) return false;
  ++g_logged;
  return true;
}

bool IsPlayerCharacter(void* creature) {
  void* stats = StatsOf(creature);
  return stats && ReadCreatureFacts(stats).is_player;
}

struct Shout {
  void* shouter;
  uint32_t hider;
  float x, y, z;
  int64_t now;
  int alerted = 0;
};

void Hear(void* context, uint32_t id, void* other) {
  Shout* shout = static_cast<Shout*>(context);
  if (engine::IsDeadOrDying(other)) return;
  const float metres = sqrtf(engine::CentreDistanceSquared(other, shout->shouter));
  const auto alerted = g_alerted.find(id);
  const bool already = g_urgent.count(id) > 0 ||
                       (alerted != g_alerted.end() && shout->now - alerted->second < g_tuning.fade_ms);
  if (!rules::AlertsAlly(metres, engine::IsEnemy(other, shout->hider), !effects::HasType(other, ee::kEffectTypeDeaf),
                         IsPlayerCharacter(other), already, g_tuning)) {
    return;
  }
  g_alerted[id] = shout->now;
  ++shout->alerted;
  char code[400];
  sprintf_s(code,
            "location at = Location(GetArea(OBJECT_SELF), Vector(%.2f, %.2f, %.2f), GetFacing(OBJECT_SELF));"
            "DelayCommand(0.10, SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE));"
            "DelayCommand(0.11, ActionMoveToLocation(at, TRUE));"
            "DelayCommand(0.12, ActionWait(6.0));",
            shout->x, shout->y, shout->z);
  RunScriptChunk(code, id);
  if (LogSlot()) {
    Log("emergency search: 0x%08X shouts - 0x%08X (%.1f m) runs to it and searches", IdOf(shout->shouter), id, metres);
  }
}

void Start(void* creature, uint32_t hider, int damage) {
  const uint32_t id = IdOf(creature);
  const int64_t now = PerceptionNowMs();
  int64_t heard = 0;
  float hx = 0, hy = 0, hz = 0;
  const bool known = HeardWhen(id, hider, &heard, &hx, &hy, &hz);
  const rules::EmergencyMove move = rules::EmergencyMoveFor(now, known ? heard : 0, g_tuning);
  char code[600];
  if (move == rules::EmergencyMove::kToHeard) {
    sprintf_s(code,
              "location at = Location(GetArea(OBJECT_SELF), Vector(%.2f, %.2f, %.2f), GetFacing(OBJECT_SELF));"
              "DelayCommand(0.10, SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE));"
              "DelayCommand(0.11, ActionMoveToLocation(at, TRUE));"
              "DelayCommand(0.12, ActionWait(6.0));",
              hx, hy, hz);
  } else {
    sprintf_s(code,
              "DelayCommand(0.10, SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE));"
              "DelayCommand(0.11, ActionWait(6.0));");
  }
  RunScriptChunk(code, id);
  if (LogSlot()) {
    Log("emergency search: 0x%08X took %d damage from 0x%08X, hidden, unseen - searches urgently: %s; glances held "
        "%.1f-%.1f s, up to %d degrees; for %d s after the last harm",
        id, damage, hider,
        move == rules::EmergencyMove::kToHeard
            ? ("to where it heard it " + std::to_string((now - heard) / 100 / 10.0).substr(0, 4) + " s ago").c_str()
            : "scans in place (it hasn't heard it lately)",
        g_glance.min_ms / 1000.0, g_glance.max_ms / 1000.0, g_glance.max_degrees, g_tuning.fade_ms / 1000);
  }
  Shout shout{creature, hider, Field<float>(creature, ee::kObjectPositionX), Field<float>(creature, ee::kObjectPositionY),
              Field<float>(creature, ee::kObjectPositionZ), now};
  engine::ForEachOtherCreatureInArea(creature, &Hear, &shout);
  if (shout.alerted == 0 && LogSlot()) {
    Log("emergency search: 0x%08X shouts - no one within %.0f m to hear it", id, g_tuning.alert_metres);
  }
}

void OnDamage(uint32_t creator, void* target, int damage) {
  if (!On() || !target || damage <= 0 || creator == ee::kObjectInvalid) return;
  void* source = CreatureById(creator);
  if (!source || source == target) return;
  const bool hiding = Field<uint8_t>(source, ee::kCreatureStealthMode) == 1;
  if (!rules::HarmStartsEmergency(true, IsPlayerCharacter(target), damage, hiding, engine::Sees(target, creator),
                                  engine::IsEnemy(target, creator))) {
    return;
  }
  const uint32_t id = IdOf(target);
  const int64_t now = PerceptionNowMs();
  Urgent& urgent = g_urgent[id];
  const bool active = rules::EmergencyActive(now, urgent.last_harm, g_tuning);
  urgent.hider = creator;
  urgent.last_harm = now;
  if (!active) {
    urgent.detect_set = now;
    Start(target, creator, damage);
  } else if (now - urgent.detect_set >= 6000) {  // a round on: search mode again (the AI may have dropped it)
    urgent.detect_set = now;
    RunScriptChunk("SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE);", id);
  }
}

}  // namespace

// stealth_watch.cpp: the glances of a creature searching urgently now (else null: its normal ones).
const rules::GlanceTuning* EmergencyGlanceFor(uint32_t creature) {
  if (!On()) return nullptr;
  const auto it = g_urgent.find(creature);
  if (it == g_urgent.end()) return nullptr;
  if (!rules::EmergencyActive(PerceptionNowMs(), it->second.last_harm, g_tuning)) {
    if (LogSlot()) Log("emergency search: 0x%08X - no harm for %d s: a normal search again", creature,
                       g_tuning.fade_ms / 1000);
    g_urgent.erase(it);
    return nullptr;
  }
  return &g_glance;
}

void InstallEmergencySearch() {
  rules::EmergencyTuning tuning;
  tuning.glance_ms = g_config.emergency_search_glance_ms;
  tuning.alert_metres = static_cast<float>(g_config.emergency_search_alert_metres);
  tuning.fade_ms = g_config.emergency_search_fade_seconds * 1000;
  g_tuning = rules::SaneEmergency(tuning);
  g_glance = rules::EmergencyGlance(g_tuning);
  if (!On()) {
    Log("emergency search: off (emergency_search=0, or hiders_need_search / perception not faithful)");
    return;
  }
  if (!AddDamageListener(&OnDamage)) {
    Log("SKIP emergency search: OnApplyDamage isn't hooked");
    return;
  }
  Log("emergency search: emergency_search=1 - damage from a hider it can't see: searching each round, glances held "
      "%.1f-%.1f s up to %d degrees, to where it heard the hider within %d s (else in place), a shout to hostile "
      "creatures within %.0f m, back to a normal search %d s after the last harm",
      g_glance.min_ms / 1000.0, g_glance.max_ms / 1000.0, g_glance.max_degrees, g_tuning.heard_ms / 1000,
      g_tuning.alert_metres, g_tuning.fade_ms / 1000);
}
