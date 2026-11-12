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
    // #39 build 2c: the 2024 areas - Entangle's 20-foot square and Grease's 10-foot one (half their diagonal), Fog
    // Cloud's Sphere by its slot level (20 feet more a level).
    {"n5_s_entanglea", HazardKind::kDifficultTerrain, 4.31f},
    {"n5_s_entangleb", HazardKind::kDifficultTerrain, 4.31f},
    {"n5_s_entanglec", HazardKind::kDifficultTerrain, 4.31f},
    {"n5_s_greasea", HazardKind::kGrease, 2.16f},
    {"n5_s_greaseb", HazardKind::kGrease, 2.16f},
    {"n5_s_greasec", HazardKind::kGrease, 2.16f},
    {"n5_s_fog1a", HazardKind::kFog, 6.10f},
    {"n5_s_fog1b", HazardKind::kFog, 6.10f},
    {"n5_s_fog2a", HazardKind::kFog, 12.19f},
    {"n5_s_fog2b", HazardKind::kFog, 12.19f},
    {"n5_s_fog3a", HazardKind::kFog, 18.29f},
    {"n5_s_fog3b", HazardKind::kFog, 18.29f},
    {"n5_s_fog4a", HazardKind::kFog, 24.38f},
    {"n5_s_fog4b", HazardKind::kFog, 24.38f},
    {"n5_s_fog5a", HazardKind::kFog, 30.48f},
    {"n5_s_fog5b", HazardKind::kFog, 30.48f},
    {"n5_s_fog6a", HazardKind::kFog, 36.58f},
    {"n5_s_fog6b", HazardKind::kFog, 36.58f},
    {"n5_s_fog7a", HazardKind::kFog, 42.67f},
    {"n5_s_fog7b", HazardKind::kFog, 42.67f},
    {"n5_s_fog8a", HazardKind::kFog, 48.77f},
    {"n5_s_fog8b", HazardKind::kFog, 48.77f},
    {"n5_s_fog9a", HazardKind::kFog, 54.86f},
    {"n5_s_fog9b", HazardKind::kFog, 54.86f},
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

bool IsSentientRacialType(int nwn_racial_type) {
  // racialtypes.2da: 8 Animal, 9 Beast, 25 Vermin, 29 Ooze.
  return nwn_racial_type != 8 && nwn_racial_type != 9 && nwn_racial_type != 25 && nwn_racial_type != 29;
}

bool IsCarefulMind(int intelligence_modifier, int wisdom_modifier) {
  return intelligence_modifier + wisdom_modifier >= 0;
}

TerrainStep TerrainAreaStep(HazardKind kind, bool avoids, int racial_type, bool careful, bool ranged, bool target_inside,
                            bool held) {
  if (kind != HazardKind::kDifficultTerrain && kind != HazardKind::kGrease) return TerrainStep::kStay;  // the fog
  if (held || !avoids || !IsSentientRacialType(racial_type) || !careful) return TerrainStep::kStay;
  if (!ranged && target_inside) return TerrainStep::kStay;  // it goes in after its target
  return kind == HazardKind::kGrease ? TerrainStep::kKeepOut : TerrainStep::kWalkOutOnce;
}

bool SearchesFog(bool ai_controlled, bool in_control, bool target_enemy, bool target_in_fog, bool has_attack_target) {
  return ai_controlled && in_control && target_enemy && target_in_fog && !has_attack_target;
}

}  // namespace rules
