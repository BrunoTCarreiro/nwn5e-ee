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

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
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

int CanChooseFeat(void* level_up_stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen) {
  if (!g_told) {
    g_told = true;
    Log("feats: the screens offer only the 2024 feats nwn5e has built, each kind of bonus pick to its count "
        "(feats faithful)");
  }
  if (!rules::IsChoosableFeat2024(feat, MasteriesAsFeats())) return 0;
  if (level_up_stats && !AsiChoiceOffered(level_up_stats, feat)) return 0;
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
}
