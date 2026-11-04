// #124 (owner, 2026-11-04: "good recs, go with them" - 1A 2A 3A 4A 5A; docs/PLAN.md, Phase 6): NWN's grenades (base
// item 81: a single-use Cast Spell - spells.2da 464-471, 744-745) as 2024 gear, key `grenades` faithful.
// - 2A, the flasks (Acid x0_s3_acid, Alchemist's Fire x0_s3_alchem, Holy Water x0_s3_holy): in place of NWN's
//   DoGrenade (a ranged touch attack, then 1 splash to every hostile within 6.67 m), the creature (or door,
//   placeable) at the spell's target makes a Dexterity save on the thrower's DC (rules::GrenadeSaveDc: 8 + its
//   Dexterity modifier + its Proficiency Bonus; a trap's 15) or takes 2024's damage (rules::GrenadeHitDice) - Holy
//   Water only on undead and evil outsiders (2024's Undead and Fiends); no splash. Alchemist's Fire keeps NWN's
//   coating of a melee weapon (an item target), and on a failed save the creature burns (3A): 1d4 Fire at the start
//   of each of its next 3 rounds (rules::kBurningRounds).
// - 5A, the bombs (x2_s3_bomb: 744 fire, 745 acid): the target's Dexterity save for half of 10d6 (Evasion as ever:
//   GetReflexAdjustedDamage), then NWN's 5-round cloud, its saves on the thrower's DC. Tanglefoot Bag, Thunderstone
//   and Choking Powder keep NWN's scripts - entangled (Restrained), deafened, dazed (Incapacitated) - their saves on
//   the thrower's DC (rules_save_roll.cpp: a grenade's save).
// - 4A, Caltrops: NWN's throw and 5 m field (x0_s3_caltrop) with 2024's effect - entering (x0_s3_calen), a DC 15
//   Dexterity save or 1 Piercing and stopped until its next round (EffectCutsceneImmobilize, 6 s); nothing each round
//   (x0_s3_calhb); the field gone after 25 points (NWN's NW_L_TOTAL_DAMAGE).
// - 1A (action_order.cpp, round_swings.cpp): mid-round a player's throw goes at once and takes one of the round's
//   remaining swings.
// The scripts run in C++ (script_hooks.cpp: RunScript by name); their NWScript is a chunk wrapped in main - no
// include: the hostility test is GetIsReactionTypeFriendly's, as NWN's Thunderstone's own.

#include "grenades.h"

#include <stdint.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "item_use.h"
#include "log.h"
#include "script_hooks.h"
#include "rules/grenades.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"

namespace {

int g_lines = 0;

bool LogSlot() { return g_lines < 200 && ++g_lines; }

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

std::string Dice(rules::GrenadeDice dice) {
  return "d" + std::to_string(dice.sides) + "(" + std::to_string(dice.count) + ")";
}

int DcFor(uint32_t thrower) {
  int dc = rules::kGrenadeObjectDc;
  GrenadeDcOf(thrower, &dc);
  return dc;
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0) Log("grenades: 0x%08X %s - its script returned %d", self, what, result);
}

// 2A: a flask on the spell's target - the Dexterity save, else 2024's damage.
const char* const kFlask = R"nss(
object t = GetSpellTargetObject(); int k = GetObjectType(t);
$ITEM
if (!GetIsObjectValid(t) || (k != OBJECT_TYPE_CREATURE && k != OBJECT_TYPE_DOOR && k != OBJECT_TYPE_PLACEABLE)) {
  ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect($VIS), GetSpellTargetLocation()); return;
}
$ONLY
SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
if (k == OBJECT_TYPE_CREATURE && ReflexSave(t, $DC, SAVING_THROW_TYPE_NONE, OBJECT_SELF)) return;
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage($DICE, $TYPE), EffectVisualEffect($VIS)), t);
$AFTER
)nss";

// Alchemist's Fire on an item: NWN's coating of a melee weapon (1d4+1 fire a hit, 4 rounds).
const char* const kCoating = R"nss(
if (k == OBJECT_TYPE_ITEM) {
  if (!GetWeaponRanged(t) && StringToInt(Get2DAString("baseitems", "WeaponType", GetBaseItemType(t))) > 0) {
    SetLocalInt(t, "X2_SPELL_CLEVEL_FLAMING_WEAPON", 1);
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_FIRE), GetItemPossessor(t));
    AddItemProperty(DURATION_TYPE_TEMPORARY, ItemPropertyOnHitCastSpell(124, 1), t, RoundsToSeconds(4));
    AddItemProperty(DURATION_TYPE_TEMPORARY, ItemPropertyVisualEffect(ITEM_VISUAL_FIRE), t, RoundsToSeconds(4));
  } else {
    FloatingTextStrRefOnCreature(100944, OBJECT_SELF);
  }
  return;
}
)nss";

// 3A: burning - 1d4 Fire at the start of each of its next rounds.
const char* const kBurning = R"nss(
if (k == OBJECT_TYPE_CREATURE) {
  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(VFX_DUR_INFERNO_CHEST), t, RoundsToSeconds($ROUNDS));
  int i;
  for (i = 1; i <= $ROUNDS; i++) {
    DelayCommand(RoundsToSeconds(i), ApplyEffectToObject(DURATION_TYPE_INSTANT,
                 EffectLinkEffects(EffectDamage(d$SIDES(), DAMAGE_TYPE_FIRE), EffectVisualEffect(VFX_IMP_FLAME_S)), t));
  }
}
)nss";

// Holy Water: undead and evil outsiders only (2024's Undead and Fiends).
const char* const kHolyOnly = R"nss(
if (k != OBJECT_TYPE_CREATURE) return;
int r = GetRacialType(t);
if (r != RACIAL_TYPE_UNDEAD && !(r == RACIAL_TYPE_OUTSIDER && GetAlignmentGoodEvil(t) == ALIGNMENT_EVIL)) {
  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_HOLY), t); return;
}
)nss";

// 5A: a bomb - the target's Dexterity save for half, then NWN's 5-round cloud.
const char* const kBomb = R"nss(
object t = GetSpellTargetObject(); int k = GetObjectType(t); location l = GetSpellTargetLocation();
int type = DAMAGE_TYPE_ACID; int save = SAVING_THROW_TYPE_ACID; int imp = VFX_IMP_ACID_L;
int burst = VFX_FNF_LOS_NORMAL_30; int cloud = AOE_PER_FOGACID;
if (GetSpellId() == 744) {
  type = DAMAGE_TYPE_FIRE; save = SAVING_THROW_TYPE_FIRE; imp = VFX_IMP_FLAME_M; burst = VFX_FNF_FIREBALL;
  cloud = AOE_PER_FOGFIRE;
}
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(burst), l);
if (GetIsObjectValid(t) && (k == OBJECT_TYPE_CREATURE || k == OBJECT_TYPE_DOOR || k == OBJECT_TYPE_PLACEABLE)) {
  SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
  int n = $DICE;
  if (k == OBJECT_TYPE_CREATURE) n = GetReflexAdjustedDamage(n, t, $DC, save, OBJECT_SELF);
  if (n > 0) ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage(n, type), EffectVisualEffect(imp)), t);
}
ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, EffectAreaOfEffect(cloud), l, RoundsToSeconds(5));
)nss";

// 4A: Caltrops' field, a creature entering it (OBJECT_SELF the area of effect).
const char* const kCaltropsEnter = R"nss(
object t = GetEnteringObject(); object c = GetAreaOfEffectCreator();
if (GetObjectType(t) != OBJECT_TYPE_CREATURE || t == c || GetIsReactionTypeFriendly(t, c)) return;
SignalEvent(t, EventSpellCastAt(c, 471));
if (ReflexSave(t, $DC, SAVING_THROW_TYPE_NONE, c)) return;
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage($DAMAGE, DAMAGE_TYPE_PIERCING),
                    EffectVisualEffect(VFX_IMP_SPIKE_TRAP)), t);
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectCutsceneImmobilize(), t, 6.0);
int n = GetLocalInt(OBJECT_SELF, "NW_L_TOTAL_DAMAGE") + $DAMAGE;
SetLocalInt(OBJECT_SELF, "NW_L_TOTAL_DAMAGE", n);
if (n >= $LIFE) {
  object v = GetLocalObject(OBJECT_SELF, "X0_L_IMPACT");
  if (GetIsObjectValid(v)) DestroyObject(v);
  DestroyObject(OBJECT_SELF);
}
)nss";

void FlaskScript(uint32_t self, rules::Grenade grenade) {
  const rules::GrenadeDice dice = rules::GrenadeHitDice(grenade);
  const int dc = DcFor(self);
  std::string code = kFlask;
  const bool fire = grenade == rules::Grenade::kAlchemistsFire;
  const bool holy = grenade == rules::Grenade::kHolyWater;
  Replace(&code, "$ITEM", fire ? kCoating : "");
  Replace(&code, "$ONLY", holy ? kHolyOnly : "");
  Replace(&code, "$AFTER", fire ? kBurning : "");
  Replace(&code, "$ROUNDS", std::to_string(rules::kBurningRounds));
  Replace(&code, "$SIDES", std::to_string(rules::kBurningDieSides));
  Replace(&code, "$DC", std::to_string(dc));
  Replace(&code, "$DICE", Dice(dice));
  Replace(&code, "$TYPE", fire ? "DAMAGE_TYPE_FIRE" : holy ? "DAMAGE_TYPE_DIVINE" : "DAMAGE_TYPE_ACID");
  Replace(&code, "$VIS", fire ? "VFX_IMP_FLAME_S" : holy ? "VFX_IMP_HEAD_HOLY" : "VFX_IMP_ACID_S");
  const char* name = fire ? "Alchemist's Fire" : holy ? "Holy Water" : "Acid";
  if (LogSlot()) {
    Log("grenades: 0x%08X %s - the target's Dexterity save, DC %d, or %s%s (2024)", self, name, dc,
        Dice(dice).c_str(), fire ? " and burning 3 rounds" : "");
  }
  Run(code, self, name);
}

void AcidScript(uint32_t self) { FlaskScript(self, rules::Grenade::kAcid); }
void AlchemistsFireScript(uint32_t self) { FlaskScript(self, rules::Grenade::kAlchemistsFire); }
void HolyWaterScript(uint32_t self) { FlaskScript(self, rules::Grenade::kHolyWater); }

void BombScript(uint32_t self) {
  const int dc = DcFor(self);
  std::string code = kBomb;
  Replace(&code, "$DICE", Dice(rules::GrenadeHitDice(rules::Grenade::kFireBomb)));
  Replace(&code, "$DC", std::to_string(dc));
  if (LogSlot()) Log("grenades: 0x%08X a bomb - the target's Dexterity save, DC %d, for half of 10d6; its cloud", self, dc);
  Run(code, self, "bomb");
}

void CaltropsEnterScript(uint32_t self) {
  std::string code = kCaltropsEnter;
  Replace(&code, "$DC", std::to_string(rules::kCaltropsSaveDc));
  Replace(&code, "$DAMAGE", std::to_string(rules::kCaltropsDamage));
  Replace(&code, "$LIFE", std::to_string(rules::kCaltropsFieldDamage));
  Run(code, self, "Caltrops");
}

void CaltropsRoundScript(uint32_t) {}  // 4A: entering only - NWN's point each round is gone

}  // namespace

bool GrenadesFaithful() { return g_config.grenades == rules::RuleVariant::kFaithful; }

bool GrenadeDcOf(uint32_t thrower, int* dc) {
  void* creature = CreatureById(thrower);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return false;
  const int pb = rules::ProficiencyBonusForCreature(CountsAsCharacter(stats, facts), facts.level,
                                                    facts.challenge_rating);
  *dc = rules::GrenadeSaveDc(engine::AbilityModifier(stats, static_cast<int>(rules::Ability::kDexterity)), pb);
  return true;
}

bool IsGrenadeCast(const void* node) {
  if (!node || Field<int32_t>(node, ee::kActionId) != ee::kActionItemCastSpell) return false;
  void* item = ItemById(Field<uint32_t>(node, ee::kActionFirstParam));  // its first parameter: the item
  return item && engine::BaseItemRow(item) == rules::kBaseItemGrenade;
}

void InstallGrenades() {
  if (!GrenadesFaithful()) {
    Log("grenades: NWN's (a touch attack and splash; key grenades)");
    return;
  }
  const bool scripts = AddScriptHandler("x0_s3_acid", &AcidScript) &&
                       AddScriptHandler("x0_s3_alchem", &AlchemistsFireScript) &&
                       AddScriptHandler("x0_s3_holy", &HolyWaterScript) && AddScriptHandler("x2_s3_bomb", &BombScript) &&
                       AddScriptHandler("x0_s3_calen", &CaltropsEnterScript) &&
                       AddScriptHandler("x0_s3_calhb", &CaltropsRoundScript);
  Log("grenades: 2024 gear - the flasks a Dexterity save (DC 8 + Dex + PB) for 2024's damage, no splash; burning 3 "
      "rounds; the bombs Dexterity half; Caltrops DC 15 on entering; a throw mid-round takes a swing%s",
      scripts ? "" : " - NOT available (no script hook)");
}
