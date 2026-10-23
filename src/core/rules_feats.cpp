// CM8 (which feats exist) and C3 / C4 (docs/CHARGEN.md), `feats` faithful: the
// creation and level-up screens offer only the SRD 5.2 feats nwn5e has built
// (rules::IsChoosableFeat2024) — the client's CanChooseFeat, which the Feats
// page asks for every feat it lists and the packages' default picks ask for
// every feat they try, says no to the others. With feat_cadence's slots that
// makes the Feats page 2024's: a Fighter's level-1 bonus picks are its
// Fighting Style and 3 Weapon Masteries (ASI waits for level 4), a human's
// extra pick an Origin feat (Alert so far). The packages' feat tables get the
// 2024 feats appended (data/rows/packft*.txt), so Recommended picks them too;
// NWN mode reaches them only after its own.
//
// C4: NWN's class bonus picks are one pool per level, so this keeps each group
// to its count for the class level being taken (rules::BonusPicks2024): a
// level-1 Fighter can't take four masteries and no style, nor two styles.
// The class and its new level are the stats' class slot being levelled.
//
// Characters keep the NWN feats they have (the feature mask, CM9, decides
// what those do). Henchmen's automatic level-ups (the server's own
// CanChooseFeat) are not changed here.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules_loaded.h"
#include "rules/backgrounds.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"

namespace {

using CanChooseFeatFn = int (*)(void* level_up_stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left,
                                void* chosen);
CanChooseFeatFn g_can_choose_feat = nullptr;
bool g_told = false;

bool MasteriesAsFeats() { return g_config.weapon_mastery == rules::RuleVariant::kFaithful; }

void Count(rules::BonusPicks* taken, int feat) {
  switch (rules::BonusFeatGroupOf(feat)) {
    case rules::BonusFeatGroup::kGeneral: ++taken->general; break;
    case rules::BonusFeatGroup::kFightingStyle: ++taken->style; break;
    case rules::BonusFeatGroup::kWeaponMastery: ++taken->mastery; break;
    case rules::BonusFeatGroup::kOrigin: ++taken->origin; break;
    case rules::BonusFeatGroup::kOther: break;
  }
}

}  // namespace

int CreationBackgroundFeat();  // rules_abilities.cpp

namespace {

// The Feats page groups a master's sub-feats only while they come one after
// another in the feat list's order (by name): the seven ASIs' choices share
// their names ("+2 Strength" ...), so they interleave and each would show as
// its own "Ability Score Improvement". After every rules load the block they
// hold is reordered ASI by ASI (names in order within each).
void GroupAsiChoices(void* rules, bool reload) {
  auto* order = Field<uint16_t*>(rules, ee::kRulesFeatsSorted);
  const int count = Field<uint16_t>(rules, ee::kRulesFeatsSortedCount);
  if (!order) return;
  std::vector<int> positions;
  std::vector<uint16_t> choices;
  for (int i = 0; i < count; ++i) {
    if (!rules::AsiFeatFromId(order[i])) continue;
    positions.push_back(i);
    choices.push_back(order[i]);
  }
  std::stable_sort(choices.begin(), choices.end(), [](uint16_t a, uint16_t b) {
    return rules::AsiFeatFromId(a)->slot < rules::AsiFeatFromId(b)->slot;
  });
  for (size_t k = 0; k < positions.size(); ++k) order[positions[k]] = choices[k];
  const bool together = positions.empty() || positions.back() - positions.front() + 1 == static_cast<int>(positions.size());
  if (!reload) {
    Log("feats: the %zu ASI choices grouped by ASI in the feat list%s", choices.size(),
        together ? "" : " - NOT together (another feat's name falls among theirs): the Feats page may split them");
  }
}

// The client stats' feats (the character's, before this level's picks).
bool ClientHasFeat(void* stats, int feat) {
  const auto* feats = Field<const uint16_t*>(stats, ee::kClientStatsFeats);
  const int count = Field<uint16_t>(stats, ee::kClientStatsFeatCount);
  for (int i = 0; feats && i < count; ++i) {
    if (feats[i] == feat) return true;
  }
  return false;
}

// C7a: the ASIs a character has made — older ASI rows and the ASI choices.
template <typename HasFeatFn>
int OwnedAsis(HasFeatFn has) {
  int owned = 0;
  for (int k = 0; k < rules::kAsiFeatCount; ++k) owned += has(rules::kAsiFeatFirst + k) ? 1 : 0;
  for (int slot = 1; slot <= rules::kAsiFeatCount; ++slot) {
    for (int split = 0; split < rules::kAsiSplits; ++split) {
      if (has(rules::AsiSplitFeat(slot, split))) {
        ++owned;
        break;
      }
    }
  }
  return owned;
}

int ClientTotalLevel(void* stats) {
  const int classes = std::min<int>(Field<uint8_t>(stats, ee::kClientStatsClassCount), ee::kClientStatsClassSlots);
  int total = 0;
  for (int i = 0; i < classes; ++i) {
    total += Field<uint8_t>(stats, i * ee::kClientStatsClassSlotSize + ee::kClientStatsClassSlotLevel);
  }
  return total;
}

// C7a: an ASI choice is offered only as the next ASI, and not when it would
// change nothing (its abilities at 20).
bool AsiChoiceOffered(void* stats, int feat) {
  const auto choice = rules::AsiFeatFromId(feat);
  if (!choice) return true;
  if (!rules::IsNextAsiChoice(feat, OwnedAsis([stats](int f) { return ClientHasFeat(stats, f); }))) return false;
  int scores[6];
  for (int i = 0; i < 6; ++i) scores[i] = Field<uint8_t>(stats, ee::kClientStatsAbilities + i);
  return rules::AsiChangesSomething(choice->increase, scores);
}

// Whether `feat` still fits this level's bonus picks, by group.
bool FitsThisLevel(void* stats, uint16_t feat, const void* chosen) {
  const uint8_t slot = Field<uint8_t>(stats, ee::kClientStatsClassBeingLevelled);
  if (slot >= ee::kClientStatsClassSlots) return true;
  const size_t at = slot * ee::kClientStatsClassSlotSize;
  const auto character_class = rules::ClassFromNwnId(Field<uint8_t>(stats, at + ee::kClientStatsClassSlotClass));
  const int class_level = Field<uint8_t>(stats, at + ee::kClientStatsClassSlotLevel);
  if (!character_class) return true;  // not a base class: NWN's own rules
  rules::BonusPicks allowed = rules::BonusPicks2024(
      *character_class, class_level, g_config.class_features == rules::RuleVariant::kFaithful, MasteriesAsFeats());
  // A Human's Versatile pick at level 1 takes an Origin feat (C7a: Origin feats count).
  if (ClientTotalLevel(stats) == 1 && Field<uint16_t>(stats, ee::kClientStatsRace) == ee::kRaceHuman) allowed.origin = 1;
  rules::BonusPicks taken;
  bool listed = false;
  if (chosen) {
    const auto* feats = Field<const uint16_t*>(chosen, ee::kExoArrayListData);
    const int count = Field<int32_t>(chosen, ee::kExoArrayListCount);
    for (int i = 0; feats && i < count; ++i) {
      Count(&taken, feats[i]);
      listed |= feats[i] == feat;
    }
  }
  if (!listed) Count(&taken, feat);
  return rules::FitsBonusPicks(taken, allowed);
}

// --- The Feats page's title (owner, 2026-10-23: "go with option 1"): this
// level's picks by kind and what is left - "Origin feat (Human): choose 1;
// Fighting Style: chosen; Weapon Masteries: 2 more" - as the Skills page's
// header names its step. The page has no description box (a feat's
// description is its "?" popup); its title line is the one full-width text.
std::string FeatPicksLine(void* stats, const void* chosen) {
  const uint8_t slot = Field<uint8_t>(stats, ee::kClientStatsClassBeingLevelled);
  if (slot >= ee::kClientStatsClassSlots) return "";
  const size_t at = slot * ee::kClientStatsClassSlotSize;
  const auto character_class = rules::ClassFromNwnId(Field<uint8_t>(stats, at + ee::kClientStatsClassSlotClass));
  const int class_level = Field<uint8_t>(stats, at + ee::kClientStatsClassSlotLevel);
  if (!character_class) return "";
  rules::BonusPicks allowed = rules::BonusPicks2024(
      *character_class, class_level, g_config.class_features == rules::RuleVariant::kFaithful, MasteriesAsFeats());
  const bool human_first = ClientTotalLevel(stats) == 1 && Field<uint16_t>(stats, ee::kClientStatsRace) == ee::kRaceHuman;
  if (human_first) allowed.origin = 1;
  rules::BonusPicks taken;
  if (chosen) {
    const auto* feats = Field<const uint16_t*>(chosen, ee::kExoArrayListData);
    const int count = Field<int32_t>(chosen, ee::kExoArrayListCount);
    for (int i = 0; feats && i < count && i < 512; ++i) Count(&taken, feats[i]);
  }
  // Styles and Origin feats past their own picks took the general one(s).
  const int overflow = std::max(0, taken.style - allowed.style) + std::max(0, taken.origin - allowed.origin);
  std::string line;
  auto part = [&line](const std::string& what, int left, int total) {
    if (total <= 0) return;
    line += (line.empty() ? "" : "; ") + what + ": ";
    if (left <= 0) {
      line += "chosen";
    } else if (total == 1) {
      line += "choose 1";
    } else {
      line += std::to_string(left) + (left == total ? " to choose" : " more");
    }
  };
  part(human_first ? "Origin feat (Human)" : "Origin feat", allowed.origin - taken.origin, allowed.origin);
  part("Fighting Style", allowed.style - taken.style, allowed.style);
  part("Weapon Masteries", allowed.mastery - taken.mastery, allowed.mastery);
  part("Ability Score Improvement or a feat", allowed.general - taken.general - overflow, allowed.general);
  return line.empty() ? "No feat choices at this level" : line;
}

using FeatsPageFn = void (*)(void* page, void* a2, void* a3, void* a4);
using GetCreatureFn = void* (*)(uint32_t id);
using SetTextFn = void (*)(void* wrapper, const void* exo_string);
using ExoStringFn = void (*)(void* exo_string, const char* text);
using ExoStringDestroyFn = void (*)(void* exo_string);
FeatsPageFn g_build_feat_list = nullptr;
GetCreatureFn g_get_client_creature = nullptr;
SetTextFn g_set_wrapper_text = nullptr;
ExoStringFn g_make_exo_string = nullptr;
ExoStringDestroyFn g_destroy_exo_string = nullptr;

void BuildFeatList(void* page, void* a2, void* a3, void* a4) {
  g_build_feat_list(page, a2, a3, a4);
  void* creature = g_get_client_creature(Field<uint32_t>(page, ee::kFeatsPageCreature));
  void* stats = creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
  if (!stats) return;
  const std::string line = FeatPicksLine(stats, static_cast<uint8_t*>(page) + ee::kFeatsPageChosen);
  if (line.empty()) return;
  alignas(16) uint8_t exo[32] = {};  // CExoString: the engine owns its buffer
  g_make_exo_string(exo, line.c_str());
  g_set_wrapper_text(static_cast<uint8_t*>(page) + ee::kFeatsPageTitleText, exo);
  g_destroy_exo_string(exo);
}

// The list builder: the Feats page's init (vtable slot 12) jumps to it last.
void InstallFeatsPageTitle() {
  const uintptr_t vtable = FindVtable("feats page: its vtable", ee::kRttiFeatsPage);
  const uintptr_t init = vtable ? VtableSlot(vtable, ee::kVtablePageInit) : 0;
  const CodeRange init_code = init ? FunctionAt(init) : CodeRange();
  const uintptr_t jump = init ? FindPatternIn("feats page: its init's last jump", init_code, ee::kFeatsInitTailJump) : 0;
  const uintptr_t at = jump ? jump + ee::kFeatsInitTailJumpAt : 0;
  const uintptr_t build = at && *reinterpret_cast<const uint8_t*>(at) == 0xe9
                              ? at + 5 + *reinterpret_cast<const int32_t*>(at + 1)
                              : 0;
  const uintptr_t creature_call =
      init ? FindPatternIn("feats page: its init's GetCreature", init_code, ee::kFeatsInitGetCreature) : 0;
  g_get_client_creature =
      reinterpret_cast<GetCreatureFn>(creature_call ? CallTarget(creature_call + ee::kFeatsInitGetCreatureAt) : 0);
  g_set_wrapper_text = reinterpret_cast<SetTextFn>(EngineExport(ee::kStringWrapperSetText));
  g_make_exo_string = reinterpret_cast<ExoStringFn>(EngineExport(ee::kExoStringFromChars));
  g_destroy_exo_string = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  if (!build || FunctionEntry(build) != build || !g_get_client_creature || !g_set_wrapper_text || !g_make_exo_string ||
      !g_destroy_exo_string) {
    Log("SKIP feats: the Feats page's title line - an anchor is missing (it stays \"Select Feats\")");
    return;
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("feats: the Feats page's list builder at rva 0x%zx - its title names this level's picks", build - base);
  HookAddress("client Feats page list (its title: this level's picks)", build, reinterpret_cast<void*>(&BuildFeatList),
              reinterpret_cast<void**>(&g_build_feat_list));
}

int CanChooseFeat(void* level_up_stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen) {
  if (!g_told) {
    g_told = true;
    Log("feats: the screens offer only the 2024 feats nwn5e has built, each kind of bonus pick to its count "
        "(feats faithful)");
  }
  if (!rules::IsChoosableFeat2024(feat, MasteriesAsFeats())) return 0;
  if (level_up_stats && !AsiChoiceOffered(level_up_stats, feat)) return 0;
  // The background's Origin feat (Criminal: Alert, Soldier: Savage Attacker)
  // comes with it when the character enters: not a pick at creation too.
  if (level_up_stats && rules::IsOriginFeat2024(feat) && ClientTotalLevel(level_up_stats) == 1) {
    if (const auto choice = rules::BackgroundFromFeat(CreationBackgroundFeat())) {
      if (rules::OriginFeatId(choice->background) == feat) return 0;
    }
  }
  if (rules::BonusFeatGroupOf(feat) != rules::BonusFeatGroup::kOther && level_up_stats &&
      !FitsThisLevel(level_up_stats, feat, chosen)) {
    return 0;
  }
  return g_can_choose_feat(level_up_stats, feat, general_left, bonus_left, chosen);
}

// The server's CanChooseFeat, whose one caller is the automatic level-up
// (CNWSCreatureStats::LevelUpAutomatic: henchmen, the script's
// LevelUpHenchman, !test): the 2024 feats only, as the screens offer.
CanChooseFeatFn g_server_can_choose_feat = nullptr;

int ServerCanChooseFeat(void* stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen) {
  if (!rules::IsChoosableFeat2024(feat, MasteriesAsFeats())) return 0;
  if (rules::AsiFeatFromId(feat) &&
      !rules::IsNextAsiChoice(feat, OwnedAsis([stats](int f) { return engine::HasFeat(stats, f); }))) {
    return 0;  // C7a: the next ASI's choices only
  }
  return g_server_can_choose_feat(stats, feat, general_left, bonus_left, chosen);
}

}  // namespace

void InstallFeatRules() {
  if (g_config.feats != rules::RuleVariant::kFaithful) return;
  ServeConditionalRows("feats2024");  // C7a: the ASI choices and Origin feats in the classes' feat slots
  OnRulesLoaded(&GroupAsiChoices);
  HookExport("CNWSCreatureStats::CanChooseFeat (feats: automatic level-ups - henchmen - take the 2024 feats)",
             ee::kServerCanChooseFeat, reinterpret_cast<void*>(&ServerCanChooseFeat),
             reinterpret_cast<void**>(&g_server_can_choose_feat));
  const uintptr_t can_choose = FunctionByString("feats: the client's CanChooseFeat", ee::kCanChooseFeatMessage);
  if (!can_choose || FunctionEntry(can_choose) != can_choose) {
    Log("SKIP feats: the client's CanChooseFeat not found - NWN's feats stay on the screens");
    return;
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("feats: the client's CanChooseFeat at rva 0x%zx", can_choose - base);
  HookAddress("CNWCLevelUpStats::CanChooseFeat (feats: only the 2024 feats offered)", can_choose,
              reinterpret_cast<void*>(&CanChooseFeat), reinterpret_cast<void**>(&g_can_choose_feat));
  InstallFeatsPageTitle();
}
