// CM7 (port of 1.69's rules_feat_cadence.cpp; docs/EE-PORT.md E4a, EP15),
// faithful: feats come at 2024's levels:
// - no NWN general feat (C3): every race's NormalFeatEveryNthLevel and
//   NumberNormalFeatsEveryNthLevel are 0 after load — the server's count
//   (ValidateLevelUp, LevelUpAutomatic) and the client's read them; the
//   Origin feat comes with the background, and a human keeps its extra feat
//   at level 1 (ExtraFeatsAtFirstLevel: 2024's Versatile);
// - the class bonus-feat slots (CNWClass +0x2B9 + class level, read by both
//   sides and by henchmen's automatic level-ups) become 2024's feat levels
//   for the 11 base classes — 4, 8, 12, 16, 19, the Fighter also 6 and 14,
//   the Rogue 10 (and the Fighting Style levels with class_features) —
//   rewritten after every rules load;
// - those slots offer Ability Score Improvement (data/rows/feat.asi.txt and
//   the classes' bonus lists, served by CM0); its abilities are chosen with !asi;
// - no ability +1 every 4 levels (EP15): the level-up screen's ability page,
//   the client's message writer and the server's reader are skipped together
//   (three patterns, all or none); without them the page stays and the
//   ability rules take the +1 back (rules_abilities.cpp).
// NWN's own bonus feats at its levels (Fighter, Wizard, Rogue, Ranger's
// favored enemies) are gone in faithful; class features are CM9.
// NWN mode: NWN's cadence, and the ASI rows are hidden (MinLevel 0xFF).

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/class_features.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules_loaded.h"

bool CreationChainHandlesNoFeats();  // chargen_ui.cpp

namespace {

constexpr int kMaxBonusFeatLevel = 60;  // GetBonusFeats reads levels 1..60
constexpr int kLastBaseClass = 10;      // classes.2da rows 0..10: Barbarian .. Wizard

void SetFaithfulBonusFeatSlots(void* rules, bool reload) {
  auto* classes = Field<uint8_t*>(rules, ee::kRulesClasses);
  const int count = Field<uint8_t>(rules, ee::kRulesClassCount);
  if (!classes) return;
  int changed = 0;
  const bool styles = g_config.class_features == rules::RuleVariant::kFaithful;
  const bool masteries =
      g_config.weapon_mastery == rules::RuleVariant::kFaithful && g_config.feats == rules::RuleVariant::kFaithful;
  for (int id = 0; id < count && id <= kLastBaseClass; ++id) {
    const auto character_class = rules::ClassFromNwnId(id);
    if (!character_class) continue;
    uint8_t* slots = classes + id * ee::kClassStride + ee::kClassBonusFeats;
    for (int level = 1; level <= kMaxBonusFeatLevel; ++level) {
      // The 2024 feat levels, plus the Fighting Style picks (Fighter F1) and the
      // Weapon Mastery picks (C4: chosen as feats on the screens).
      slots[level] = static_cast<uint8_t>(rules::BonusPicks2024(*character_class, level, styles, masteries).Total());
    }
    ++changed;
  }
  auto* races = Field<uint8_t*>(rules, ee::kRulesRaces);
  const int race_count = Field<uint8_t>(rules, ee::kRulesRaceCount);
  int races_changed = 0, origin_races = 0;
  for (int r = 0; races && r < race_count; ++r) {
    uint8_t* race = races + r * ee::kRaceStride;
    if (Field<int32_t>(race, ee::kRaceFeatEveryNthLevel) != 0) {
      SetField<int32_t>(race, ee::kRaceFeatEveryNthLevel, 0);
      ++races_changed;
    }
    // C3 (docs/CHARGEN.md): no general feat at level 1 — the Origin feat comes
    // with the background; a human keeps its extra one (2024's Versatile).
    // Only when the creation's Customize chain can end without a Feats page
    // (chargen_ui.cpp); otherwise those characters couldn't finish it.
    if (CreationChainHandlesNoFeats() && Field<int32_t>(race, ee::kRaceNormalFeats) != 0) {
      SetField<int32_t>(race, ee::kRaceNormalFeats, 0);
      ++origin_races;
    }
  }
  Log("feat cadence: 2024 feat slots%s for %d base classes (4, 8, 12, 16, 19; Fighter 6, 14; Rogue 10); no "
      "general feat (%d races) - the Origin feat comes with the background, a human keeps its extra feat (%d races)",
      reload ? " again after a rules load" : "", changed, races_changed, origin_races);
}

void HideAsiRows(void* rules, bool reload) {
  auto* feats = Field<uint8_t*>(rules, ee::kRulesFeats);
  const int count = Field<uint16_t>(rules, ee::kRulesFeatCount);
  if (!feats) return;
  int hidden = 0;
  for (int id = rules::kAsiFeatFirst; id < rules::kAsiFeatFirst + rules::kAsiFeatCount && id < count; ++id) {
    SetField<uint8_t>(feats + id * ee::kFeatStride, ee::kFeatMinLevel, 0xFF);
    ++hidden;
  }
  const int last_choice = rules::AsiSplitFeat(rules::kAsiFeatCount, rules::kAsiSplits - 1);  // C7a's choices
  for (int id = rules::kAsiSplitFeatFirst; id <= last_choice && id < count; ++id) {
    SetField<uint8_t>(feats + id * ee::kFeatStride, ee::kFeatMinLevel, 0xFF);
    ++hidden;
  }
  if (!reload) Log("feat cadence: NWN's - %d Ability Score Improvement rows hidden", hidden);
}

// EP15 (b): the ability page every 4 levels, gone on both sides at once.
void RemoveAbilityPage() {
  static const uint8_t kJne[1] = {0x75};
  static const uint8_t kJmp[1] = {0xeb};
  const PatternPatch patches[] = {
      {"the server's reader of the chosen ability", ee::kHandleLevelUpMessage, ee::kLevelUpServerPattern, 4, kJne,
       kJmp, 1},
      {"the client's level-up message writer", nullptr, ee::kLevelUpWriterPattern, 2, kJne, kJmp, 1},
      {"the level-up screen's ability page", nullptr, ee::kLevelUpPanelPattern, 7, kJne, kJmp, 1},
  };
  if (ApplyPatternPatches("feat cadence: no ability +1 every 4 levels (EP15)", patches,
                          sizeof(patches) / sizeof(patches[0]))) {
    Log("feat cadence: the level-up screen has no ability page (faithful)");
  } else {
    Log("feat cadence: the ability page stays - its +1 is taken back by the ability rules (EP15 a)");
  }
}

}  // namespace

void InstallFeatCadenceRules() {
  if (g_config.feat_cadence != rules::RuleVariant::kFaithful) {
    OnRulesLoaded(&HideAsiRows);
    return;
  }
  OnRulesLoaded(&SetFaithfulBonusFeatSlots);
  RemoveAbilityPage();
}
