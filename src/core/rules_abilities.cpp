// CM4 / CM5 / CM12 / CM13 (port of 1.69's rules_abilities.cpp; docs/EE-PORT.md
// E4b, EP15), faithful:
// - species give no ability increases: the race table's adjustments are 0
//   (after every rules load), so the creation screen, the sheet and every
//   modifier the engine caches agree;
// - the creation screen's point buy is 2024's: 27 points (the race budget),
//   at most 15 and the second point cost from 13 (ruleset values the screen
//   and the server's check read); each class's Recommended scores are a legal
//   27-point set (rules::RecommendedScores2024);
// - a character's base scores lose NWN's per-level +1s (recorded in its level
//   history) and gain its background's increases (!background) and its
//   Ability Score Improvements (!asi; henchmen raise their main abilities on
//   their own) — none past 20. They are set through the engine's own setters
//   (SetSTRBase ...), which recompute the cached modifiers, and every change
//   is recorded in a local variable: never applied twice, and NWN mode puts
//   the NWN scores back. A character that leveled under NWN's rules gets its
//   2024 feat levels so far as ASIs to choose (CM13). A polymorphed creature
//   is left alone (polymorph keeps and restores its own base scores).
// - EP15 (a): if the level-up screen still offers its +1 every 4 levels (the
//   feat-cadence patches missing), the +1 is taken back at once and the player
//   is pointed to the Ability Score Improvement feat.
// - the background is chosen on the creation screen (docs/CHARGEN.md): the
//   level-1 general pick lists the four SRD backgrounds, each with its seven
//   ability splits (feat.background.txt; the Fighter packages default to
//   Soldier); that feat is the record. !background shows or changes it (the
//   feat swapped); an older !background record in local variables still counts.
// The Origin feat: Alert (Criminal) works (initiative); Savage Attacker and
// Magic Initiate are recorded, and work when those feats are built.
// NWN mode: NWN's (and any recorded change undone).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
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
#include "rules/abilities.h"
#include "rules/backgrounds.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules/skills.h"
#include "rules_loaded.h"
#include "ruleset.h"

void GiveBackgroundKit(uint32_t creature_id, rules::Background background);  // rules_equipment.cpp (C2)

namespace {

using SetBaseFn = void (*)(void* stats, uint8_t value);
using SetConBaseFn = void (*)(void* stats, uint8_t value, int adjust_current_hp);
using StatFn = uint8_t (*)(void* stats);
using GetLevelStatsFn = void* (*)(void* stats, uint8_t index);
using LevelUpFn = void (*)(void* stats, void* level_stats, uint8_t a, uint8_t b, uint8_t c, int d);

const char* const kDeltaVars[6] = {"nwn5e_delta_str", "nwn5e_delta_dex", "nwn5e_delta_con",
                                   "nwn5e_delta_int", "nwn5e_delta_wis", "nwn5e_delta_cha"};
constexpr const char* kVarBackground = "nwn5e_background";     // rules::Background
constexpr const char* kVarBackgroundBonus = "nwn5e_bg_bonus";  // rules::EncodeAmounts
constexpr const char* kVarLegacy = "nwn5e_asi_legacy";         // legacy ASIs + 1 (0 = not decided yet)
constexpr const char* kVarOrigin = "nwn5e_origin_feat";        // the Origin feat nwn5e gave for the background + 1
constexpr int kMaxLegacy = 7;
constexpr DWORD kSyncEveryMs = 2000;
constexpr int kLastBaseClass = 10;

// The character's background: its background feat (chosen at creation's
// level-1 general pick, or set by !background — saved with the character,
// exports included), else the record an older !background left in local
// variables.
struct CurrentBackground {
  rules::Background background = rules::Background::kNone;
  int amounts[6] = {0, 0, 0, 0, 0, 0};
  int feat = -1;  // the background feat; -1 when read from the old record
};

CurrentBackground ReadBackground(void* creature, void* stats) {
  CurrentBackground current;
  const int last = rules::kBackgroundFeatFirst + rules::kBackgroundFeatCount;
  for (int feat = rules::kBackgroundFeatFirst; stats && feat < last; ++feat) {
    if (!engine::HasFeat(stats, feat)) continue;
    const auto choice = rules::BackgroundFromFeat(feat);
    if (!choice || !rules::BackgroundIncreases(choice->background, choice->chosen, current.amounts).empty()) continue;
    current.background = choice->background;
    current.feat = feat;
    return current;
  }
  for (int& amount : current.amounts) amount = 0;
  if (!creature) return current;
  current.background = static_cast<rules::Background>(engine::GetLocalInt(creature, kVarBackground));
  rules::DecodeAmounts(engine::GetLocalInt(creature, kVarBackgroundBonus), current.amounts);
  return current;
}

// Records a background as its feat (any other background feat removed, the
// old local record cleared); the local record stays the fallback when the
// feat can't be added (served data off).
void SetBackground(void* creature, void* stats, rules::Background background, const int amounts[6]) {
  int feat = -1;
  for (int split = 0; split < rules::kBackgroundSplits && feat < 0; ++split) {
    const int row = rules::BackgroundFeat(background, split);
    const auto choice = rules::BackgroundFromFeat(row);
    int split_amounts[6];
    if (choice && rules::BackgroundIncreases(background, choice->chosen, split_amounts).empty() &&
        std::equal(split_amounts, split_amounts + 6, amounts)) {
      feat = row;
    }
  }
  const int last = rules::kBackgroundFeatFirst + rules::kBackgroundFeatCount;
  for (int row = rules::kBackgroundFeatFirst; row < last; ++row) {
    if (row != feat && engine::HasFeat(stats, row)) engine::RemoveFeat(stats, row);
  }
  if (feat >= 0 && (engine::HasFeat(stats, feat) || engine::AddFeat(stats, feat))) {
    SetRecord(creature, kVarBackground, 0);
    SetRecord(creature, kVarBackgroundBonus, 0);
    return;
  }
  SetRecord(creature, kVarBackground, static_cast<int>(background));
  SetRecord(creature, kVarBackgroundBonus, rules::EncodeAmounts(amounts));
}

// The background chosen on the creation screen's Background page
// (chargen_ui.cpp), waiting for the new character: given (as its feat) to the
// first level-1 player character without a background that appears within
// the hour.
int g_creation_feat = -1;
DWORD g_creation_tick = 0;
constexpr DWORD kCreationWaitMs = 60 * 60 * 1000;

void GiveCreationBackground(void* creature, void* stats, const CreatureFacts& facts) {
  if (g_creation_feat < 0 || !facts.is_player || facts.level != 1) return;
  if (GetTickCount() - g_creation_tick > kCreationWaitMs) {
    g_creation_feat = -1;
    return;
  }
  if (ReadBackground(creature, stats).background != rules::Background::kNone) return;
  const int feat = g_creation_feat;
  g_creation_feat = -1;
  const auto choice = rules::BackgroundFromFeat(feat);
  if (!choice || !engine::AddFeat(stats, feat)) {
    Log("abilities: 0x%08X the background chosen at creation (feat %d) could not be given", Field<uint32_t>(creature, ee::kObjectId), feat);
    return;
  }
  Log("abilities: 0x%08X background %s from the creation screen (feat %d)", Field<uint32_t>(creature, ee::kObjectId),
      rules::BackgroundData(choice->background)->name, feat);
  GiveBackgroundKit(Field<uint32_t>(creature, ee::kObjectId), choice->background);
}

// NWN mode / off: the background feats aren't offered.
void HideBackgroundRows(void* rules_object, bool reload) {
  auto* feats = Field<uint8_t*>(rules_object, ee::kRulesFeats);
  const int count = Field<uint16_t>(rules_object, ee::kRulesFeatCount);
  if (!feats) return;
  int hidden = 0;
  const int last = rules::kBackgroundFeatFirst + rules::kBackgroundFeatCount;
  for (int id = rules::kBackgroundFeatFirst; id < last && id < count; ++id, ++hidden) {
    SetField<uint8_t>(feats + id * ee::kFeatStride, ee::kFeatMinLevel, 0xFF);
  }
  if (!reload) Log("abilities: NWN's - %d background feats hidden", hidden);
}

LevelUpFn g_level_up = nullptr;
bool g_faithful = false;
bool g_active = false;
std::map<uint32_t, DWORD> g_last_sync;  // the engine's server thread only

std::string AsiVar(bool legacy, int k) { return std::string(legacy ? "nwn5e_asi_legacy_" : "nwn5e_asi_") + std::to_string(k); }

int Base(void* stats, int i) { return Field<uint8_t>(stats, ee::kStatsAbilityBase + 2 * i); }

int Score(void* stats, int i) {
  const auto getter = reinterpret_cast<StatFn>(EngineExport(ee::kGetAbilityStat[i]));
  return getter ? getter(stats) : Base(stats, i);
}

void SetBase(void* stats, int i, int value) {
  const uintptr_t setter = EngineExport(ee::kSetAbilityBase[i]);
  if (!setter) return;
  if (i == 2) {
    reinterpret_cast<SetConBaseFn>(setter)(stats, static_cast<uint8_t>(value), 1);  // current HP follows
  } else {
    reinterpret_cast<SetBaseFn>(setter)(stats, static_cast<uint8_t>(value));
  }
}

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

// One of a character's ASIs: from a feat row it owns or a legacy level.
struct AsiSlot {
  bool legacy;
  int k;          // 1-based
  int choice;     // rules::EncodeAsi, 0 = unspent
  int feat = -1;  // C7a: the ASI's choice feat (its abilities), -1 for an older ASI (choice from !asi)
};

// C7a: the ASI choice feats a character has (one per ASI), by ASI.
std::vector<AsiSlot> ChoiceFeatAsis(void* stats) {
  std::vector<AsiSlot> slots;
  for (int slot = 1; slot <= rules::kAsiFeatCount; ++slot) {
    for (int split = 0; split < rules::kAsiSplits; ++split) {
      const int feat = rules::AsiSplitFeat(slot, split);
      if (!engine::HasFeat(stats, feat)) continue;
      slots.push_back({false, slot, rules::EncodeAsi(rules::AsiSplit(split)), feat});
      break;
    }
  }
  return slots;
}

int TotalFeatLevels(void* stats) {
  int total = 0;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    total += rules::FeatLevelsReached2024(c.character_class, c.levels);
  }
  return total;
}

// A player character's ASIs, legacy first (deciding the legacy count the
// first time faithful sees it).
std::vector<AsiSlot> PlayerAsis(void* creature, void* stats) {
  const std::vector<AsiSlot> choice_feats = ChoiceFeatAsis(stats);
  int owned = static_cast<int>(choice_feats.size());
  for (int k = 1; k <= rules::kAsiFeatCount; ++k) {
    if (engine::HasFeat(stats, rules::kAsiFeatFirst + k - 1)) ++owned;
  }
  int legacy = engine::GetLocalInt(creature, kVarLegacy) - 1;
  if (legacy < 0) {
    legacy = std::min(kMaxLegacy, std::max(0, TotalFeatLevels(stats) - owned));
    SetRecord(creature, kVarLegacy, legacy + 1);
    if (legacy > 0) {
      Log("abilities: 0x%08X leveled under NWN's rules - %d ASIs to choose for its 2024 feat levels so far",
          IdOf(creature), legacy);
    }
  }
  std::vector<AsiSlot> slots;
  for (int k = 1; k <= legacy; ++k) slots.push_back({true, k, engine::GetLocalInt(creature, AsiVar(true, k).c_str())});
  for (int k = 1; k <= rules::kAsiFeatCount; ++k) {
    if (!engine::HasFeat(stats, rules::kAsiFeatFirst + k - 1)) continue;
    slots.push_back({false, k, engine::GetLocalInt(creature, AsiVar(false, k).c_str())});
  }
  slots.insert(slots.end(), choice_feats.begin(), choice_feats.end());
  return slots;
}

// C6: the background's Origin feat as a real feat (Soldier: Savage Attacker,
// Criminal: Alert) — on the sheet, and kept by exports — taken back when the
// background changes or outside faithful. A feat the character chose itself
// (a Human's Versatile pick) is never taken.
void SyncOriginFeat(void* creature, void* stats, rules::Background background) {
  const int given = engine::GetLocalInt(creature, kVarOrigin) - 1;
  const int wanted = background == rules::Background::kNone ? -1 : rules::OriginFeatId(background);
  const uint32_t id = IdOf(creature);
  if (given >= 0 && given != wanted) {
    if (engine::HasFeat(stats, given)) engine::RemoveFeat(stats, given);
    SetRecord(creature, kVarOrigin, 0);
    Log("abilities: 0x%08X Origin feat %d (from its background) taken back", id, given);
  }
  if (wanted >= 0 && given != wanted && !engine::HasFeat(stats, wanted) && engine::AddFeat(stats, wanted)) {
    SetRecord(creature, kVarOrigin, wanted + 1);
    Log("abilities: 0x%08X Origin feat %d given (its background's)", id, wanted);
  }
}

// Brings the character's base scores to its faithful (or NWN) values.
void Sync(void* creature, bool force) {
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature)) return;  // still loading
  if (Field<int32_t>(creature, ee::kCreaturePolymorphed) != 0) return;  // polymorph keeps its own scores
  const uint32_t id = IdOf(creature);
  if (id == ee::kObjectInvalid) return;
  const DWORD now = GetTickCount();
  auto last = g_last_sync.find(id);
  if (!force && last != g_last_sync.end() && now - last->second < kSyncEveryMs) return;
  g_last_sync[id] = now;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character || !facts.plausible) return;
  if (facts.is_player) EnsureRecords(creature);  // C6: an exported character's records, back first

  int applied[6], nwn_base[6], delta[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 6; ++i) {
    applied[i] = engine::GetLocalInt(creature, kDeltaVars[i]);
    nwn_base[i] = Base(stats, i) - applied[i];
  }
  int bumps[6] = {0, 0, 0, 0, 0, 0}, background[6] = {0, 0, 0, 0, 0, 0};
  std::vector<rules::AbilityIncrease> asis;
  if (g_faithful) {
    static const auto level_stats = reinterpret_cast<GetLevelStatsFn>(EngineExport(ee::kGetLevelStats));
    const int history = Field<int32_t>(stats, ee::kStatsLevelHistoryCount);
    for (int level = 0; level < facts.level && level < history && level_stats; ++level) {
      void* entry = level_stats(stats, static_cast<uint8_t>(level));
      const int bumped = entry ? Field<uint8_t>(entry, ee::kLevelStatsAbility) : 6;
      if (bumped >= 0 && bumped < 6) ++bumps[bumped];
    }
    if (facts.is_player) {
      GiveCreationBackground(creature, stats, facts);
      CurrentBackground current = ReadBackground(creature, stats);
      if (current.feat < 0 && current.background != rules::Background::kNone) {
        // C6: an older !background record (local variables) moved to its feat.
        SetBackground(creature, stats, current.background, current.amounts);
        current = ReadBackground(creature, stats);
        if (current.feat >= 0) Log("abilities: 0x%08X its older background record moved to feat %d", id, current.feat);
      }
      SyncOriginFeat(creature, stats, current.background);
      std::copy(current.amounts, current.amounts + 6, background);
      for (const AsiSlot& slot : PlayerAsis(creature, stats)) {
        if (const auto asi = rules::DecodeAsi(slot.choice)) asis.push_back(*asi);
      }
    } else {  // a henchman raises its main abilities on its own (CM12)
      const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
      const auto main = slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
      if (main) {
        int scores[6];
        for (int i = 0; i < 6; ++i) scores[i] = nwn_base[i] - bumps[i];
        asis = rules::AutomaticAsis(TotalFeatLevels(stats), rules::AsiPriority2024(*main, scores[0], scores[1]),
                                    scores);
      }
    }
    rules::FaithfulAbilityDelta(nwn_base, bumps, background, asis, delta);
  } else if (facts.is_player) {
    SyncOriginFeat(creature, stats, rules::Background::kNone);  // NWN mode: no background feats
  }
  std::string changes;
  for (int i = 0; i < 6; ++i) {
    if (delta[i] == applied[i]) continue;
    const int value = std::max(3, std::min(255, nwn_base[i] + delta[i]));
    SetBase(stats, i, value);
    SetRecord(creature, kDeltaVars[i], delta[i]);
    char part[64];
    sprintf_s(part, "%s%s %d (NWN %d, %+d)", changes.empty() ? "" : ", ",
              rules::AbilityName(static_cast<rules::Ability>(i)), value, nwn_base[i], delta[i]);
    changes += part;
  }
  if (!changes.empty()) {
    Log("abilities: %s 0x%08X base scores %s: %s (level +1s %d, background %s, %d ASIs)", KindOf(facts), id,
        g_faithful ? "2024" : "back to NWN's", changes.c_str(),
        bumps[0] + bumps[1] + bumps[2] + bumps[3] + bumps[4] + bumps[5],
        ReadBackground(creature, stats).background != rules::Background::kNone ? "yes" : "no",
        static_cast<int>(asis.size()));
  }
}

void OnCreatureUpdate(void* creature) { Sync(creature, false); }

// EP15 (a): a level-up that still raised an ability is synced at once.
void LevelUp(void* stats, void* level_stats, uint8_t a, uint8_t b, uint8_t c, int d) {
  g_level_up(stats, level_stats, a, b, c, d);
  void* creature = stats ? CreatureOf(stats) : nullptr;
  if (!creature) return;
  const int bumped = level_stats ? Field<uint8_t>(level_stats, ee::kLevelStatsAbility) : 6;
  Sync(creature, true);
  if (g_faithful && bumped >= 0 && bumped < 6 && ReadCreatureFacts(stats).is_player) {
    TellPlayer(IdOf(creature), std::string("nwn5e: the 2024 rules have no ability +1 every 4 levels - your ") +
                                   rules::AbilityName(static_cast<rules::Ability>(bumped)) +
                                   " +1 was taken back. Your class's feat levels offer Ability Score Improvement "
                                   "(choose its abilities with !asi).");
  }
}

// After every rules load: no racial adjustments, a 27-point budget, legal
// Recommended scores.
void ApplyLoadedTables(void* rules, bool reload) {
  auto* races = Field<uint8_t*>(rules, ee::kRulesRaces);
  const int race_count = Field<uint8_t>(rules, ee::kRulesRaceCount);
  int adjusted = 0;
  for (int r = 0; races && r < race_count; ++r) {
    uint8_t* race = races + r * ee::kRaceStride;
    int8_t* adjust = reinterpret_cast<int8_t*>(race + ee::kRaceAbilityAdjust);
    bool any = false;
    for (int i = 0; i < 6; ++i) any = any || adjust[i] != 0;
    if (any) ++adjusted;
    for (int i = 0; i < 6; ++i) adjust[i] = 0;
    SetField<int32_t>(race, ee::kRacePointBuyBudget, 27);
  }
  auto* classes = Field<uint8_t*>(rules, ee::kRulesClasses);
  const int class_count = Field<uint8_t>(rules, ee::kRulesClassCount);
  int recommended = 0;
  for (int id = 0; classes && id < class_count && id <= kLastBaseClass; ++id) {
    const auto c = rules::ClassFromNwnId(id);
    if (!c) continue;
    const std::array<int, 6> scores = rules::RecommendedScores2024(*c);
    for (int i = 0; i < 6; ++i) {
      SetField<uint8_t>(classes + id * ee::kClassStride, ee::kClassRecommendedScores + i,
                        static_cast<uint8_t>(scores[i]));
    }
    ++recommended;
  }
  Log("abilities: racial adjustments removed (%d races), point-buy budget 27, Recommended scores for %d classes%s",
      adjusted, recommended, reload ? " - again after a rules load" : "");
}

int MaxBaseScore(int) { return 15; }
int SecondPointFrom(int) { return 13; }
// 2024 point buy: every score from 8, the class's main ability too (NWN keeps it at 11 or more).
int MinPrimaryScore(int) { return 8; }

std::string ScoresLine(void* stats) {
  std::ostringstream out;
  for (int i = 0; i < 6; ++i) {
    out << (i ? ", " : "") << rules::AbilityShortName(static_cast<rules::Ability>(i)) << " " << Score(stats, i);
  }
  return out.str();
}

std::vector<std::string> Words(const std::string& text) {
  std::istringstream in(text);
  std::vector<std::string> words;
  std::string word;
  while (in >> word) words.push_back(word);
  return words;
}

// !asi [<ability> [<ability>] | undo]
std::vector<std::string> AsiCommand(void* creature, const std::string& args) {
  if (!g_faithful) return {"nwn5e: !asi is for the 2024 rules (ability_scores=faithful)."};
  void* stats = StatsOf(creature);
  if (!stats) return {};
  std::vector<AsiSlot> slots = PlayerAsis(creature, stats);
  const std::vector<std::string> words = Words(args);
  int unspent = 0;
  for (const AsiSlot& slot : slots) unspent += slot.choice ? 0 : 1;
  auto describe = [&]() {
    std::string line = "nwn5e: " + ScoresLine(stats) + ". Ability Score Improvements: " + std::to_string(slots.size()) +
                       " (" + std::to_string(unspent) + " unspent).";
    if (unspent) line += " Choose with !asi str (+2) or !asi str con (+1 each).";
    line += " !asi change str [con] changes the last one.";
    return line;
  };
  if (words.empty()) {
    if (slots.empty()) {
      return {describe(), "nwn5e: you get one at each feat level of your class (4, 8, 12, 16, 19; Fighter 6, 14; "
                          "Rogue 10) - take \"Ability Score Improvement\" in the feat slot and pick its abilities "
                          "there."};
    }
    return {describe()};
  }
  if (words[0] == "change") {  // C7a: the last ASI's abilities, swapped
    if (words.size() < 2 || words.size() > 3) return {"nwn5e: !asi change str (+2) or !asi change str con (+1 each)."};
    const auto first = rules::ParseAbility(words[1].c_str());
    const auto second = words.size() == 3 ? rules::ParseAbility(words[2].c_str()) : std::nullopt;
    if (!first || (words.size() == 3 && (!second || *second == *first))) {
      return {"nwn5e: abilities are str, dex, con, int, wis, cha (two different ones for +1 each)."};
    }
    for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
      if (!it->choice) continue;
      if (it->feat >= 0) {
        const int split = rules::AsiSplitIndex({*first, second});
        const int feat = rules::AsiSplitFeat(it->k, split);
        if (feat < 0) return {"nwn5e: that isn't an Ability Score Improvement."};
        if (feat != it->feat) {
          engine::RemoveFeat(stats, it->feat);
          if (!engine::AddFeat(stats, feat)) {
            engine::AddFeat(stats, it->feat);
            return {"nwn5e: the change didn't take."};
          }
        }
      } else {
        SetRecord(creature, AsiVar(it->legacy, it->k).c_str(), rules::EncodeAsi({*first, second}));
      }
      Sync(creature, true);
      Log("abilities: 0x%08X ASI changed to %s%s%s", IdOf(creature), rules::AbilityName(*first), second ? " + " : "",
          second ? rules::AbilityName(*second) : "");
      return {"nwn5e: your last Ability Score Improvement is now " + std::string(rules::AbilityName(*first)) +
                  (second ? std::string(" +1, ") + rules::AbilityName(*second) + " +1." : " +2."),
              describe()};
    }
    return {"nwn5e: no Ability Score Improvement to change.", describe()};
  }
  if (words[0] == "undo") {
    for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
      if (!it->choice || it->feat >= 0) continue;  // a choice feat stays: !asi change swaps it
      SetRecord(creature, AsiVar(it->legacy, it->k).c_str(), 0);
      it->choice = 0;
      ++unspent;
      Sync(creature, true);
      return {"nwn5e: the last Ability Score Improvement is unspent again.", describe()};
    }
    return {"nwn5e: nothing to undo.", describe()};
  }
  if (words.size() > 2) return {"nwn5e: name one ability (+2) or two (+1 each)."};
  const auto first = rules::ParseAbility(words[0].c_str());
  const auto second = words.size() == 2 ? rules::ParseAbility(words[1].c_str()) : std::nullopt;
  if (!first || (words.size() == 2 && !second)) return {"nwn5e: abilities are str, dex, con, int, wis, cha."};
  if (second && *second == *first) return {"nwn5e: for +2 to one ability, name it once: !asi " + words[0] + "."};
  for (AsiSlot& slot : slots) {
    if (slot.choice) continue;
    slot.choice = rules::EncodeAsi({*first, second});
    SetRecord(creature, AsiVar(slot.legacy, slot.k).c_str(), slot.choice);
    --unspent;
    Sync(creature, true);
    Log("abilities: 0x%08X ASI %s%d = %s%s%s", IdOf(creature), slot.legacy ? "legacy " : "", slot.k,
        rules::AbilityName(*first), second ? " + " : "", second ? rules::AbilityName(*second) : "");
    return {"nwn5e: Ability Score Improvement: " + std::string(rules::AbilityName(*first)) +
                (second ? std::string(" +1, ") + rules::AbilityName(*second) + " +1" : " +2") +
                " (no score goes above 20).",
            describe()};
  }
  return {"nwn5e: no unspent Ability Score Improvement - take one at your class's next feat level.", describe()};
}

// !background [<name> <ability> <ability> [<ability>]]
std::vector<std::string> BackgroundCommand(void* creature, const std::string& args) {
  if (!g_faithful) return {"nwn5e: !background is for the 2024 rules (ability_scores=faithful)."};
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const std::vector<std::string> words = Words(args);
  if (words.empty()) {
    std::vector<std::string> lines;
    const CurrentBackground chosen = ReadBackground(creature, stats);
    const rules::BackgroundInfo* current = rules::BackgroundData(chosen.background);
    lines.push_back(current ? "nwn5e: your background is " + std::string(current->name) +
                                  (chosen.feat >= 0 ? " (its feat is on your character sheet)." : ".")
                            : "nwn5e: choose your background (2024): !background <name> <+2 ability> <+1 ability>, "
                              "or three abilities for +1 each.");
    for (const rules::BackgroundInfo& b : rules::Backgrounds2024()) {
      lines.push_back(std::string(b.name) + ": " + rules::AbilityShortName(b.abilities[0]) + " / " +
                      rules::AbilityShortName(b.abilities[1]) + " / " + rules::AbilityShortName(b.abilities[2]) +
                      "; Origin feat " + b.origin_feat + "; tool " + b.tool + ".");
    }
    return lines;
  }
  const auto background = rules::ParseBackground(words[0]);
  if (!background) return {"nwn5e: the backgrounds are Acolyte, Criminal, Sage and Soldier (SRD 5.2)."};
  std::vector<rules::Ability> chosen;
  for (size_t i = 1; i < words.size(); ++i) {
    const auto ability = rules::ParseAbility(words[i].c_str());
    if (!ability) return {"nwn5e: abilities are str, dex, con, int, wis, cha."};
    chosen.push_back(*ability);
  }
  int amounts[6];
  const std::string error = rules::BackgroundIncreases(*background, chosen, amounts);
  if (!error.empty()) return {"nwn5e: " + error + "."};
  SetBackground(creature, stats, *background, amounts);
  Sync(creature, true);
  const rules::BackgroundInfo* info = rules::BackgroundData(*background);
  std::string increases;
  for (int i = 0; i < 6; ++i) {
    if (!amounts[i]) continue;
    increases += (increases.empty() ? "" : ", ") + std::string(rules::AbilityName(static_cast<rules::Ability>(i))) +
                 " +" + std::to_string(amounts[i]);
  }
  Log("abilities: 0x%08X background %s (%s)", IdOf(creature), info->name, increases.c_str());
  return {"nwn5e: background " + std::string(info->name) + ": " + increases + "; Origin feat " + info->origin_feat +
              (*background == rules::Background::kCriminal ? " (works now)" : " (works when that feat is built)") +
              "; its skills are proficient.",
          "nwn5e: " + ScoresLine(stats) + "."};
}

}  // namespace

// !test (test_commands.cpp): the character's base scores become `scores`.
// The sync runs first, then the engine's scores are set and its recorded
// change kept — the next sync sees the same change and leaves them.
void SetTestAbilityScores(void* creature, const int scores[6]) {
  void* stats = StatsOf(creature);
  if (!stats) return;
  Sync(creature, true);
  for (int i = 0; i < 6; ++i) SetBase(stats, i, scores[i]);
}

// From the creation screen's Background page (chargen_ui.cpp): the chosen
// background feat, given to the new character when it enters.
void SetCreationBackground(int feat) {
  g_creation_feat = feat;
  g_creation_tick = GetTickCount();
  Log("abilities: background feat %d chosen on the creation screen - given when the character enters", feat);
}

// The background chosen on the creation screen's Background page, until the
// character enters (-1: none) — C5's Skills page shows its skills taken.
int CreationBackgroundFeat() { return g_creation_feat; }

// The background's skills (and tool), for the skill values (rules_skills.cpp).
rules::SkillSet BackgroundSkills(void* creature) {
  if (!g_faithful || !creature) return 0;
  const rules::BackgroundInfo* info = rules::BackgroundData(ReadBackground(creature, StatsOf(creature)).background);
  return info ? info->skills : 0;
}

// Alert from the Criminal background (rules_initiative.cpp).
// Savage Attacker from the Soldier background (rules_fighter.cpp).
bool HasSavageAttackerFromBackground(void* creature) {
  return g_faithful && creature &&
         ReadBackground(creature, StatsOf(creature)).background == rules::Background::kSoldier;
}

bool HasAlertFromBackground(void* creature) {
  return g_faithful && creature &&
         ReadBackground(creature, StatsOf(creature)).background == rules::Background::kCriminal;
}

void InstallAbilityRules() {
  g_faithful = g_config.ability_scores == rules::RuleVariant::kFaithful;
  RegisterRecords({kDeltaVars[0], kDeltaVars[1], kDeltaVars[2], kDeltaVars[3], kDeltaVars[4], kDeltaVars[5],
                   kVarBackground, kVarBackgroundBonus, kVarLegacy, kVarOrigin});
  for (int k = 1; k <= kMaxLegacy; ++k) RegisterRecord(AsiVar(true, k));
  for (int k = 1; k <= rules::kAsiFeatCount; ++k) RegisterRecord(AsiVar(false, k));
  // The sync runs in every mode: nwn and off put any recorded change back.
  g_active = AddCreatureUpdateListener(&OnCreatureUpdate);
  HookExport("CNWSCreatureStats::LevelUp (base scores synced at a level-up)", ee::kLevelUp,
             reinterpret_cast<void*>(&LevelUp), reinterpret_cast<void**>(&g_level_up));
  AddChatCommand("!asi", &AsiCommand);
  AddChatCommand("!background", &BackgroundCommand);
  if (!g_faithful) {
    OnRulesLoaded(&HideBackgroundRows);
    return;
  }
  OnRulesLoaded(&ApplyLoadedTables);
  AnswerRulesetInt("CHARGEN_BASE_ABILITY_MAX", "abilities: point buy to 15 (2024)", &MaxBaseScore);
  AnswerRulesetInt("CHARGEN_ABILITY_COST_INCREMENT2", "abilities: 2 points from 13 (2024)", &SecondPointFrom);
  AnswerRulesetInt("CHARGEN_BASE_ABILITY_MIN_PRIMARY", "abilities: the main ability from 8 too (2024)", &MinPrimaryScore);
  Log("abilities: 2024 point buy, no racial adjustments, backgrounds and ASIs (faithful)");
}
