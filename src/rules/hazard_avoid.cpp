#include "rules/hazard_avoid.h"

#include <ctype.h>

namespace rules {

namespace {

struct HazardScript {
  const char* name;
  HazardKind kind;
  float radius;  // metres (vfx_persistent.2da's RADIUS; a rectangle, half its LENGTH)
};

constexpr HazardScript kScripts[] = {
    // #130's flask patches (vfx_persistent 100-102, one creature wide).
    {"n5_fpacid_in", HazardKind::kAnyone, 0.83f},
    {"n5_fpacid_rd", HazardKind::kAnyone, 0.83f},
    {"n5_fpfire_in", HazardKind::kAnyone, 0.83f},
    {"n5_fpfire_rd", HazardKind::kAnyone, 0.83f},
    {"n5_fpholy_in", HazardKind::kHolyWater, 0.83f},
    {"n5_fpholy_rd", HazardKind::kHolyWater, 0.83f},
    // The Bombs' clouds (0.70.0; Incendiary Cloud's and Acid Fog's areas).
    {"n5_bfire_in", HazardKind::kAnyone, 5.0f},
    {"n5_bfire_rd", HazardKind::kAnyone, 5.0f},
    {"n5_bacid_in", HazardKind::kAnyone, 5.0f},
    {"n5_bacid_rd", HazardKind::kAnyone, 5.0f},
    {"n5_bacid_out", HazardKind::kAnyone, 5.0f},
    // Caltrops (row 37), Choking Powder (Stinking Cloud's area).
    {"x0_s3_calen", HazardKind::kAnyone, 5.0f},
    {"x0_s3_calhb", HazardKind::kAnyone, 5.0f},
    {"x0_s3_chokeen", HazardKind::kAnyone, 6.7f},
    {"x0_s3_chokehb", HazardKind::kAnyone, 6.7f},
    // The harmful spell areas.
    {"nw_s0_acidfoga", HazardKind::kSpell, 5.0f},
    {"nw_s0_acidfogb", HazardKind::kSpell, 5.0f},
    {"nw_s0_acidfogc", HazardKind::kSpell, 5.0f},
    {"nw_s0_incclouda", HazardKind::kSpell, 5.0f},
    {"nw_s0_inccloudb", HazardKind::kSpell, 5.0f},
    {"nw_s0_inccloudc", HazardKind::kSpell, 5.0f},
    {"nw_s0_stinkclda", HazardKind::kSpell, 6.7f},
    {"nw_s0_stinkcldc", HazardKind::kSpell, 6.7f},
    {"nw_s0_cloudkilla", HazardKind::kSpell, 5.0f},
    {"nw_s0_cloudkillb", HazardKind::kSpell, 5.0f},
    {"nw_s0_cloudkillc", HazardKind::kSpell, 5.0f},
    {"nw_s0_wallfirea", HazardKind::kSpell, 5.0f},
    {"nw_s0_wallfirec", HazardKind::kSpell, 5.0f},
    {"nw_s0_bladebara", HazardKind::kSpell, 5.0f},
    {"nw_s0_bladebarb", HazardKind::kSpell, 5.0f},
    {"nw_s0_bladebarc", HazardKind::kSpell, 5.0f},
    {"nw_s0_weba", HazardKind::kSpell, 6.7f},
    {"nw_s0_webb", HazardKind::kSpell, 6.7f},
    {"nw_s0_webc", HazardKind::kSpell, 6.7f},
    {"nw_s0_entanglea", HazardKind::kSpell, 5.0f},
    {"nw_s0_entangleb", HazardKind::kSpell, 5.0f},
    {"nw_s0_entanglec", HazardKind::kSpell, 5.0f},
    {"nw_s0_greasea", HazardKind::kSpell, 6.0f},
    {"nw_s0_greaseb", HazardKind::kSpell, 6.0f},
    {"nw_s0_greasec", HazardKind::kSpell, 6.0f},
    {"nw_s0_crpdooma", HazardKind::kSpell, 6.7f},
    {"nw_s0_crpdoomb", HazardKind::kSpell, 6.7f},
    {"nw_s0_crpdoomc", HazardKind::kSpell, 6.7f},
    {"nw_s0_evardsa", HazardKind::kSpell, 5.0f},
    {"nw_s0_evardsc", HazardKind::kSpell, 5.0f},
    {"nw_s0_stormvenc", HazardKind::kSpell, 10.0f},
    {"x2_s0_cldbewlda", HazardKind::kSpell, 5.0f},
    {"x2_s0_cldbewldc", HazardKind::kSpell, 5.0f},
    {"x2_s0_stneholda", HazardKind::kSpell, 10.0f},
    {"x2_s0_stneholdc", HazardKind::kSpell, 10.0f},
    {"nw_s0_ghoultcha", HazardKind::kSpell, 1.65f},
};

const HazardScript* Find(const std::string& name) {
  for (const HazardScript& script : kScripts) {
    const std::string known = script.name;
    if (known.size() != name.size()) continue;
    size_t k = 0;
    while (k < known.size() && tolower(static_cast<unsigned char>(name[k])) == known[k]) ++k;
    if (k == known.size()) return &script;
  }
  return nullptr;
}

}  // namespace

HazardKind HazardOfScript(const std::string& name) {
  const HazardScript* script = Find(name);
  return script ? script->kind : HazardKind::kNone;
}

float HazardStepOutMetres(const std::string& name) {
  const HazardScript* script = Find(name);
  return script ? script->radius + 1.5f : 0.0f;
}

bool AvoidsHazards(bool faithful, bool player_controlled, int racial_type, bool not_in_control) {
  return faithful && !player_controlled && racial_type != kRacialTypeUndead && racial_type != kRacialTypeConstruct &&
         !not_in_control;
}

bool HazardHoldsAttack(bool avoids, bool would_be_hurt, bool target_inside, bool attacker_inside, bool ranged) {
  return avoids && would_be_hurt && target_inside && !attacker_inside && !ranged;
}

}  // namespace rules
