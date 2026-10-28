// Decisions CM9 / CM10: the frame for the 2024 class features (port of 1.69's
// rules_features.cpp; docs/EE-PORT.md E4c) — `!features` lists a character's
// features by class level with what nwn5e does with each
// (rules::ClassFeatures2024), and its SRD subclass, granted at class level 3
// (the prestige classes are off in faithful: rules_progression.cpp). The
// features themselves are built class by class, each from its brief
// (rules_fighter.cpp).
//
// Granted features (Fighter F2 Second Wind, ...): the feats a character's 2024
// class levels give (rules::GrantedFeats2024) are added by nwn5e — so a
// character made before gets them too — and recorded on the character
// (nwn5e_granted_feats: a bit per rules::GrantableFeats2024 entry); NWN mode
// and off remove what was recorded. EE's AddFeat gives a feat with uses per
// day its full uses; the next player update shows the change.
//
// X1 (owner, 2026-10-24: "remove them completely, we don't need to keep NWN
// compatibility"): in faithful the NWN class feats with no 2024 counterpart
// (rules::NwnOnlyClassFeats: the Barbarian's, the Rogue's and the Monk's) are
// taken off characters by the same sync - the classes' feat tables served in
// faithful no longer grant them. The Monk's general feats (Cleave, Improved
// Unarmed Strike, the epic Improved Stunning Fist: rules::NwnOnlyMonkFeats)
// only from characters with Monk levels - others may have picked them.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rules/class_features.h"
#include "rules/nwn_conversion.h"

namespace {

using FeatFn = void (*)(void* stats, uint16_t feat);

FeatFn g_add_feat = nullptr;
FeatFn g_remove_feat = nullptr;

constexpr const char* kVarGranted = "nwn5e_granted_feats";
constexpr DWORD kSyncEveryMs = 2000;
std::unordered_map<uint32_t, DWORD> g_last_sync;  // by creature id

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",   "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger", "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

// !features
std::vector<std::string> FeaturesCommand(void* creature, const std::string& /*args*/) {
  if (g_config.class_features != rules::RuleVariant::kFaithful) {
    return {"nwn5e: !features shows the 2024 class features (class_features=faithful)."};
  }
  void* stats = StatsOf(creature);
  if (!stats) return {"nwn5e: no character."};
  std::vector<std::string> lines;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    const int level = c.levels < 20 ? c.levels : 20;
    std::string head = "nwn5e: " + std::string(ClassName(c.character_class)) + " " + std::to_string(level) + " - " +
                       rules::Subclass2024(c.character_class);
    if (level < rules::kSubclassLevel) head += " at 3";
    lines.push_back(head);
    if (!rules::HasFeatureList(c.character_class)) {
      lines.push_back("  its 2024 features come with its own brief (Phase 3, one class at a time).");
      continue;
    }
    for (const rules::ClassFeature& f : rules::ClassFeatures2024(c.character_class, level)) {
      lines.push_back("  " + std::to_string(f.level) + " " + f.name + (f.subclass ? " (subclass)" : "") + " - " +
                      f.status);
    }
  }
  if (lines.empty()) lines.push_back("nwn5e: no 2024 class.");
  return lines;
}

// X1: the NWN-only class feats off a character (faithful).
void RemoveNwnOnlyFeats(void* stats, const CreatureFacts& facts, uint32_t id) {
  std::vector<std::string> names;
  std::vector<int> feats = rules::NwnOnlyClassFeats();
  if (ClassLevel2024(stats, rules::CharacterClass::kMonk) > 0) {
    feats.insert(feats.end(), rules::NwnOnlyMonkFeats().begin(), rules::NwnOnlyMonkFeats().end());
  }
  for (int feat : feats) {
    if (!engine::HasFeat(stats, feat)) continue;
    g_remove_feat(stats, static_cast<uint16_t>(feat));
    Log("features: %s 0x%08X removed NWN feat %d (no 2024 counterpart, X1)", KindOf(facts), id, feat);
    const std::string name = rules::NwnOnlyClassFeatName(feat);
    if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
  }
  if (!names.empty() && facts.is_player) {
    std::string what;
    for (size_t i = 0; i < names.size(); ++i) {
      what += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
    }
    TellPlayer(id, "nwn5e: NWN's " + what + (names.size() == 1 ? " isn't a" : " aren't") + " 2024 class feature" +
                   (names.size() == 1 ? "" : "s") + " - removed (2024 rules). !features lists yours.");
  }
}

void FeaturesCreatureUpdate(void* creature) {
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (id == ee::kObjectInvalid) return;
  const DWORD now = GetTickCount();
  auto last = g_last_sync.find(id);
  if (last != g_last_sync.end() && now - last->second < kSyncEveryMs) return;
  g_last_sync[id] = now;
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature)) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return;
  if (facts.is_player) EnsureRecords(creature);  // C6
  const bool faithful = g_config.class_features == rules::RuleVariant::kFaithful;
  const int recorded = engine::GetLocalInt(creature, kVarGranted);
  if (faithful) RemoveNwnOnlyFeats(stats, facts, id);
  if (!faithful && !recorded) return;  // nothing to take back
  std::vector<int> wanted =
      faithful ? rules::GrantedFeats2024(rules::GroupClassLevels(ReadClassSlots(stats))) : std::vector<int>();
  if (faithful && facts.is_player) {  // G1: the Shove (players only - the AI of others would use it far more)
    for (int feat : rules::FeatsEveryoneHas2024(g_config.knockdown == rules::RuleVariant::kFaithful)) {
      wanted.push_back(feat);
    }
  }
  int now_recorded = recorded;
  const std::vector<int>& grantable = rules::GrantableFeats2024();
  for (size_t i = 0; i < grantable.size() && i < 31; ++i) {
    const int feat = grantable[i];
    const bool want = std::find(wanted.begin(), wanted.end(), feat) != wanted.end();
    const bool had = (recorded >> i) & 1;
    if (want && !had) {
      if (!engine::HasFeat(stats, feat)) {
        g_add_feat(stats, static_cast<uint16_t>(feat));
      } else if (rules::GrantedOnlyWhenMissing(feat)) {
        continue;  // the character's own (taken): not nwn5e's to take back
      }
      now_recorded |= 1 << i;
      Log("features: %s 0x%08X granted feat %d", KindOf(facts), id, feat);
    } else if (!want && had) {
      if (engine::HasFeat(stats, feat)) g_remove_feat(stats, static_cast<uint16_t>(feat));
      now_recorded &= ~(1 << i);
      Log("features: %s 0x%08X removed feat %d", KindOf(facts), id, feat);
    }
  }
  if (now_recorded != recorded) SetRecord(creature, kVarGranted, now_recorded);
}

}  // namespace

// Every mode: NWN mode and off take back the feats faithful granted.
void InstallFeatureRules() {
  AddChatCommand("!features", &FeaturesCommand);
  g_add_feat = reinterpret_cast<FeatFn>(EngineExport(ee::kAddFeat));
  g_remove_feat = reinterpret_cast<FeatFn>(EngineExport(ee::kRemoveFeat));
  if (!g_add_feat || !g_remove_feat) {
    Log("SKIP features: granted class feats - no AddFeat / RemoveFeat export");
    return;
  }
  RegisterRecords({kVarGranted});
  AddCreatureUpdateListener(&FeaturesCreatureUpdate);
}
