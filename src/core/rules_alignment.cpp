// CM14 (port of 1.69's rules_alignment.cpp; docs/EE-PORT.md E4a), faithful:
// alignment is descriptive — no SRD 5.2 rule reads it:
// - classes have no alignment limits: AlignRestrict, AlignRstrctType and
//   InvertRestrict are cleared in the loaded class table (after every load),
//   so the creation screen's alignment boxes, the level-up class list, the
//   server's checks and the alignment-shift code (which marks a paladin or
//   monk out of alignment) see none — EE's creation screen reads the same
//   fields, so 1.69's patch of its hard-coded limits is no longer needed;
// - items' Use Limitation: Alignment Group / Specific Alignment is ignored
//   (CheckItemAlignmentRestrictions answers "usable").
// Spells and weapon properties keyed to alignment change with their phases;
// the campaigns keep shifting and checking alignment as story. NWN mode: NWN's.

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules_loaded.h"

namespace {

using ItemCheckFn = int (*)(void* creature, void* item);
ItemCheckFn g_item_alignment = nullptr;
int g_item_lines = 0;

void LiftClassAlignmentLimits(void* rules, bool reload) {
  const int count = Field<uint8_t>(rules, ee::kRulesClassCount);
  auto* classes = Field<uint8_t*>(rules, ee::kRulesClasses);
  if (!classes) return;
  std::string lifted;
  int changed = 0;
  for (int i = 0; i < count; ++i) {
    uint8_t* c = classes + i * ee::kClassStride;
    const uint8_t mask = Field<uint8_t>(c, ee::kClassAlignRestrict);
    const uint8_t type = Field<uint8_t>(c, ee::kClassAlignRestrictType);
    const int32_t invert = Field<int32_t>(c, ee::kClassInvertRestrict);
    if (!mask && !type && !invert) continue;
    ++changed;
    char row[48];
    sprintf_s(row, "%s%d (0x%02X/%u/%d)", lifted.empty() ? "" : ", ", i, mask, type, invert);
    lifted += row;
    SetField<uint8_t>(c, ee::kClassAlignRestrict, 0);
    SetField<uint8_t>(c, ee::kClassAlignRestrictType, 0);
    SetField<int32_t>(c, ee::kClassInvertRestrict, 0);
  }
  if (reload) {
    Log("alignment: class limits lifted again after a rules load (%d of %d classes)", changed, count);
  } else {
    Log("alignment: class limits lifted for %d of %d classes - rows (mask/axes/invert): %s", changed, count,
        lifted.c_str());
  }
}

int CheckItemAlignmentRestrictions(void* creature, void* item) {
  const int engine = g_item_alignment(creature, item);
  if (!engine && g_item_lines < 20 && g_config.log_attacks) {
    ++g_item_lines;
    Log("alignment: an item's alignment limit ignored for 0x%08X (faithful)",
        creature ? Field<uint32_t>(creature, ee::kObjectId) : 0);
  }
  return 1;
}

}  // namespace

void InstallAlignmentRules() {
  if (g_config.alignment != rules::RuleVariant::kFaithful) return;
  OnRulesLoaded(&LiftClassAlignmentLimits);
  HookExport("CNWSCreature::CheckItemAlignmentRestrictions (items' alignment limits ignored)",
             ee::kCheckItemAlignmentRestrictions, reinterpret_cast<void*>(&CheckItemAlignmentRestrictions),
             reinterpret_cast<void**>(&g_item_alignment));
}
