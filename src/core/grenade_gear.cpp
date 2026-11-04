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
// - #130 (owner, 2026-11-04: "1A (later we'll check if it's too small), 2B, 3A, 4B"), a house rule ([house_rules]
//   flask_patches, off by default): Acid, Alchemist's Fire or Holy Water thrown at the ground leaves a patch - its area
//   of effect (vfx_persistent rows 100-102: one creature wide) for 2 rounds; a creature entering it (n5_fp*_in) or in
//   it at its round tick (n5_fp*_rd, every 6 s) takes the flask's dice, no save, once a round (rules::FlaskMakesPatch);
//   whom, NWN's area-spell rule (x0_i0_spells' spellsIsTarget, SPELL_TARGET_STANDARDHOSTILE: allies only above Normal
//   difficulty, written out here); Holy Water's only undead and evil outsiders.
// - 0.70.0 (owner, 2026-11-04, after "I'm having a bit of an issue with the hazards not affecting the PC": "go with
//   your recommendations"): every grenade hazard hurts anyone in it, the thrower and its henchmen too - the flask
//   patches (4B replaced), Caltrops' field, the bombs' clouds (nwn5e's own scripts, n5_bfire_* / n5_bacid_*: NWN's
//   Incendiary Cloud and Acid Fog effects, the spells' own scripts left to the spells) and Choking Powder's cloud
//   (x0_s3_chokeen / x0_s3_chokehb, the grenade's own). And light (owner: "is it possible to create a temporary light
//   source while it's active?"): an invisible object with an orange light where the fire burns - the Fire Bomb's
//   cloud (VFX_DUR_LIGHT_ORANGE_20, 5 rounds), an Alchemist's Fire patch (_10, 2 rounds); light_sources.cpp loads it
//   as a normal placeable (CreatingFireLight), and the hiding-in-light rule counts it (stealth_watch.cpp).
// - 0.71.0 (owner, 2026-11-04: "one more thing, bombs (poison and fire) along with thunderstone should tunr off torches and other such light sources in range of the explosion as they blow up"): a bomb's or a
//   Thunderstone's blast puts out the lit lights round it (rules::GrenadeBlastRadius: 5 m, 3.33 m;
//   light_sources.cpp's BlastPutsOutLights). The Thunderstone's x0_s3_thunder runs in C++ for it, as NWN's: every
//   creature within 3.33 m that isn't the thrower's friend, a Constitution save on the thrower's DC or deafened 5
//   rounds.
// - 0.71.1: Alchemist's Fire on a weapon makes it a light too; a hazard hurts its thrower and the thrower's friends but
//   doesn't signal them as attacked (its own cloud started the thrower's combat - an Initiative roll - in the owner's
//   0.71.0 run).
// The scripts run in C++ (script_hooks.cpp: RunScript by name); their NWScript is a chunk wrapped in main - no
// include: the hostility test is GetIsReactionTypeFriendly's, as NWN's Thunderstone's own.

#include "grenades.h"

#include <stdint.h>

#include <string>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "item_use.h"
#include "light_sources.h"
#include "log.h"
#include "script_hooks.h"
#include "rules/grenades.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"

namespace {

int g_lines = 0;

// 0.70.0: nwn5e's scripts for the bombs' clouds (no .ncs; script_hooks.cpp), the fire light's tag, the Acid cloud's slow.
constexpr const char* kFireCloudIn = "n5_bfire_in";
constexpr const char* kFireCloudRound = "n5_bfire_rd";
constexpr const char* kAcidCloudInName = "n5_bacid_in";
constexpr const char* kAcidCloudRoundName = "n5_bacid_rd";
constexpr const char* kAcidCloudOutName = "n5_bacid_out";
constexpr const char* kFireLightTag = "nwn5e_fire_light";
constexpr const char* kAcidSlowTag = "n5_bacid_slow";

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
  ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect($VIS), GetSpellTargetLocation());
  $GROUND
  return;
}
$ONLY
SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId()));
if (k == OBJECT_TYPE_CREATURE && ReflexSave(t, $DC, SAVING_THROW_TYPE_NONE, OBJECT_SELF)) return;
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage($DICE, $TYPE), EffectVisualEffect($VIS)), t);
$AFTER
)nss";

// Alchemist's Fire on an item: NWN's coating of a melee weapon (1d4+1 fire a hit, 4 rounds). 0.71.1 (owner,
// 2026-11-04: "alchemist's fire on a weapon should also make it a light source"): and an orange light while it burns
// (ItemPropertyLight, low: 10 m - the wielder's light effect, which the hiding-in-light rule counts as carried).
const char* const kCoating = R"nss(
if (k == OBJECT_TYPE_ITEM) {
  if (!GetWeaponRanged(t) && StringToInt(Get2DAString("baseitems", "WeaponType", GetBaseItemType(t))) > 0) {
    SetLocalInt(t, "X2_SPELL_CLEVEL_FLAMING_WEAPON", 1);
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_FIRE), GetItemPossessor(t));
    AddItemProperty(DURATION_TYPE_TEMPORARY, ItemPropertyOnHitCastSpell(124, 1), t, RoundsToSeconds(4));
    AddItemProperty(DURATION_TYPE_TEMPORARY, ItemPropertyVisualEffect(ITEM_VISUAL_FIRE), t, RoundsToSeconds(4));
    AddItemProperty(DURATION_TYPE_TEMPORARY, ItemPropertyLight(IP_CONST_LIGHTBRIGHTNESS_LOW, IP_CONST_LIGHTCOLOR_ORANGE),
                    t, RoundsToSeconds(4));
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
string sEnter = "$BAIN"; string sRound = "$BARD"; string sExit = "$BAOUT";
if (GetSpellId() == 744) { sEnter = "$BFIN"; sRound = "$BFRD"; sExit = ""; }
ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, EffectAreaOfEffect(cloud, sEnter, sRound, sExit), l, RoundsToSeconds(5));
if (GetSpellId() == 744) {
  object lt = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", l, FALSE, "$LIGHTTAG");
  ApplyEffectToObject(DURATION_TYPE_PERMANENT, EffectVisualEffect(VFX_DUR_LIGHT_ORANGE_20), lt);
  DestroyObject(lt, RoundsToSeconds(5));
}
)nss";

// 0.70.0: the Fire Bomb's cloud (NWN's Incendiary Cloud, nw_s0_incclouda / c): a creature entering it, or every creature,
// door and placeable in it at its round tick - 4d6 Fire, a Dexterity save on the thrower's DC for half (GetSpellSaveDC:
// rules_save_roll.cpp's grenade DC). Anyone (no spell resistance: alchemy, not magic). OBJECT_SELF the cloud.
const char* const kFireCloud = R"nss(
object c = GetAreaOfEffectCreator(); int dc = GetSpellSaveDC();
object t = $FIRST;
while (GetIsObjectValid(t)) {
  if (!GetIsDead(t) && GetTag(t) != "$LIGHTTAG") {
    if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, 744));
    int n = GetReflexAdjustedDamage(d6(4), t, dc, SAVING_THROW_TYPE_FIRE, c);
    if (n > 0) ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage(n, DAMAGE_TYPE_FIRE), EffectVisualEffect(VFX_IMP_FLAME_S)), t);
  }
  t = $NEXT;
}
)nss";

// 0.70.0: the Acid Bomb's cloud (NWN's Acid Fog, nw_s0_acidfoga / b / c): entering, 4d6 Acid and a Constitution save
// or slowed (half speed) while in it; at its round tick, 2d6 Acid to every creature in it (NWN's - its save there
// changes nothing); leaving, the slow ends. Anyone.
const char* const kAcidCloudIn = R"nss(
object c = GetAreaOfEffectCreator(); object t = GetEnteringObject();
if (!GetIsObjectValid(t) || GetIsDead(t)) return;
if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, 745));
if (FortitudeSave(t, GetSpellSaveDC(), SAVING_THROW_TYPE_ACID, c) == 0) {
  ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(EffectMovementSpeedDecrease(50), "$SLOWTAG"), t);
}
ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage(d6(4), DAMAGE_TYPE_ACID), EffectVisualEffect(VFX_IMP_ACID_S)), t);
)nss";

const char* const kAcidCloudRound = R"nss(
object c = GetAreaOfEffectCreator();
object t = GetFirstInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (!GetIsDead(t)) {
    if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, 745));
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage(d6(2), DAMAGE_TYPE_ACID), EffectVisualEffect(VFX_IMP_ACID_S)), t);
  }
  t = GetNextInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE);
}
)nss";

const char* const kAcidCloudOut = R"nss(
object t = GetExitingObject();
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) {
  if (GetEffectTag(e) == "$SLOWTAG") { RemoveEffect(t, e); break; }
  e = GetNextEffect(t);
}
)nss";

// 0.70.0: Choking Powder's cloud (NWN's x0_s3_choke names x0_s3_chokeen / x0_s3_chokehb; x0_i0_spells'
// spellsStinkingCloud): a creature entering it, or every creature in it at its round tick - a Constitution save on the
// thrower's DC or dazed (Incapacitated) 2 rounds, unless immune to poison. Anyone.
const char* const kChoke = R"nss(
object c = GetAreaOfEffectCreator(); int dc = GetSpellSaveDC();
object t = $FIRST;
while (GetIsObjectValid(t)) {
  if (!GetIsDead(t)) {
    if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, 467));
    if (FortitudeSave(t, dc, SAVING_THROW_TYPE_POISON, c) == 0 && !GetIsImmune(t, IMMUNITY_TYPE_POISON)) {
      effect eDaze = EffectLinkEffects(EffectLinkEffects(EffectVisualEffect(VFX_DUR_MIND_AFFECTING_DISABLED), EffectDazed()),
                                       EffectVisualEffect(VFX_DUR_CESSATE_NEGATIVE));
      ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_DAZED_S), t);
      ApplyEffectToObject(DURATION_TYPE_TEMPORARY, eDaze, t, RoundsToSeconds(2));
    }
  }
  t = $NEXT;
}
)nss";

// 0.71.0: the Thunderstone (NWN's x0_s3_thunder, its DC the thrower's): the screen shakes, every creature within its
// radius that isn't the thrower's friend makes a Constitution save or is deafened 5 rounds.
const char* const kThunder = R"nss(
location l = GetSpellTargetLocation();
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(VFX_FNF_SCREEN_SHAKE), OBJECT_SELF, RoundsToSeconds(3));
ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_LOS_NORMAL_30), l);
object t = GetFirstObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(t)) {
  if (t != OBJECT_SELF && !GetIsReactionTypeFriendly(t)) {
    SignalEvent(t, EventSpellCastAt(OBJECT_SELF, GetSpellId(), TRUE));
    float w = GetDistanceBetweenLocations(l, GetLocation(t)) / 20.0;
    if (FortitudeSave(t, $DC, SAVING_THROW_TYPE_NONE, OBJECT_SELF) == 0) {
      DelayCommand(w, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectDeaf(), t, RoundsToSeconds(5)));
      DelayCommand(w, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_NATURE), t));
    }
  }
  t = GetNextObjectInShape(SHAPE_SPHERE, $RADIUS, l, TRUE, OBJECT_TYPE_CREATURE);
}
)nss";

// 4A: Caltrops' field, a creature entering it (OBJECT_SELF the area of effect) - 0.70.0: any creature, the thrower
// and its allies too (NWN's spared friends).
const char* const kCaltropsEnter = R"nss(
object t = GetEnteringObject(); object c = GetAreaOfEffectCreator();
if (GetObjectType(t) != OBJECT_TYPE_CREATURE || GetIsDead(t)) return;
if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, 471));
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

// #130, a house rule: a flask's patch (OBJECT_SELF its area of effect) - a creature entering it, or everyone in it at
// its round tick: the flask's dice, no save (2B, 3A), once a round (a local on the creature for
// rules::kFlaskPatchLockSeconds). 0.70.0 (owner: "go with your recommendations"): anyone in it, the thrower and its
// henchmen too, as the 2024 Oil's (4B's NWN area-spell rule, sparing allies on Normal, replaced).
const char* const kPatch = R"nss(
object c = GetAreaOfEffectCreator();
string key = "n5_fpatch_" + ObjectToString(OBJECT_SELF);
object t = $FIRST;
while (GetIsObjectValid(t)) {
  int hurt = GetObjectType(t) == OBJECT_TYPE_CREATURE && !GetIsDead(t) && !GetLocalInt(t, key);
  $ONLY
  if (hurt) {
    SetLocalInt(t, key, 1);
    AssignCommand(t, DelayCommand($LOCK, DeleteLocalInt(t, key)));
    if (!GetIsReactionTypeFriendly(t, c)) SignalEvent(t, EventSpellCastAt(c, $SPELL));
    ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectLinkEffects(EffectDamage($DICE, $TYPE), EffectVisualEffect($VIS)), t);
  }
  t = $NEXT;
}
)nss";

// Holy Water's patch: undead and evil outsiders only.
const char* const kPatchHolyOnly = R"nss(
  if (hurt) {
    int r = GetRacialType(t);
    if (r != RACIAL_TYPE_UNDEAD && !(r == RACIAL_TYPE_OUTSIDER && GetAlignmentGoodEvil(t) == ALIGNMENT_EVIL)) hurt = FALSE;
  }
)nss";

const char* FlaskName(rules::Grenade grenade) {
  return grenade == rules::Grenade::kAlchemistsFire ? "Alchemist's Fire"
         : grenade == rules::Grenade::kHolyWater    ? "Holy Water"
                                                    : "Acid";
}

const char* FlaskDamageType(rules::Grenade grenade) {
  return grenade == rules::Grenade::kAlchemistsFire ? "DAMAGE_TYPE_FIRE"
         : grenade == rules::Grenade::kHolyWater    ? "DAMAGE_TYPE_DIVINE"
                                                    : "DAMAGE_TYPE_ACID";
}

const char* FlaskImpact(rules::Grenade grenade) {
  return grenade == rules::Grenade::kAlchemistsFire ? "VFX_IMP_FLAME_S"
         : grenade == rules::Grenade::kHolyWater    ? "VFX_IMP_HEAD_HOLY"
                                                    : "VFX_IMP_ACID_S";
}

int FlaskSpell(rules::Grenade grenade) {
  return grenade == rules::Grenade::kAlchemistsFire ? 464 : grenade == rules::Grenade::kHolyWater ? 466 : 469;
}

// The patch's scripts, nwn5e's names (no .ncs; script_hooks.cpp): n5_fpacid_in / n5_fpacid_rd, n5_fpfire_*,
// n5_fpholy_*.
std::string PatchScriptName(rules::Grenade grenade, const char* part) {
  const char* flask = grenade == rules::Grenade::kAlchemistsFire ? "fire"
                      : grenade == rules::Grenade::kHolyWater    ? "holy"
                                                                 : "acid";
  return std::string("n5_fp") + flask + "_" + part;
}

void FlaskScript(uint32_t self, rules::Grenade grenade) {
  const rules::GrenadeDice dice = rules::GrenadeHitDice(grenade);
  const int dc = DcFor(self);
  std::string code = kFlask;
  const bool fire = grenade == rules::Grenade::kAlchemistsFire;
  const bool holy = grenade == rules::Grenade::kHolyWater;
  // #130's house rule: thrown at the ground, a patch - its area of effect for 2 rounds.
  const bool patch = rules::FlaskMakesPatch(grenade, GrenadesFaithful(), g_config.flask_patches);
  std::string ground;
  if (patch) {
    ground = "ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, EffectAreaOfEffect(" +
             std::to_string(rules::FlaskPatchAreaEffect(grenade)) + ", \"" + PatchScriptName(grenade, "in") + "\", \"" +
             PatchScriptName(grenade, "rd") + "\", \"\"), GetSpellTargetLocation(), RoundsToSeconds(" +
             std::to_string(rules::kFlaskPatchRounds) + "));";
    if (fire) {  // 0.70.0: its fire's light while it burns
      ground += " object lt = CreateObject(OBJECT_TYPE_PLACEABLE, \"plc_invisobj\", GetSpellTargetLocation(), FALSE, \"" +
                std::string(kFireLightTag) + "\"); ApplyEffectToObject(DURATION_TYPE_PERMANENT, " +
                "EffectVisualEffect(VFX_DUR_LIGHT_ORANGE_10), lt); DestroyObject(lt, RoundsToSeconds(" +
                std::to_string(rules::kFlaskPatchRounds) + "));";
    }
  }
  Replace(&code, "$GROUND", ground);
  Replace(&code, "$ITEM", fire ? kCoating : "");
  Replace(&code, "$ONLY", holy ? kHolyOnly : "");
  Replace(&code, "$AFTER", fire ? kBurning : "");
  Replace(&code, "$ROUNDS", std::to_string(rules::kBurningRounds));
  Replace(&code, "$SIDES", std::to_string(rules::kBurningDieSides));
  Replace(&code, "$DC", std::to_string(dc));
  Replace(&code, "$DICE", Dice(dice));
  Replace(&code, "$TYPE", FlaskDamageType(grenade));
  Replace(&code, "$VIS", FlaskImpact(grenade));
  const char* name = FlaskName(grenade);
  if (LogSlot()) {
    Log("grenades: 0x%08X %s - the target's Dexterity save, DC %d, or %s%s (2024)%s", self, name, dc,
        Dice(dice).c_str(), fire ? " and burning 3 rounds" : "",
        patch ? "; at the ground a patch for 2 rounds (the house rule)" : "");
  }
  CreatingFireLight(patch && fire);  // light_sources.cpp: the fire light's invisible object a normal placeable
  Run(code, self, name);
  CreatingFireLight(false);
}

// #130: a patch's scripts - a creature entering it, or its round tick (everyone in it).
void PatchScript(uint32_t self, rules::Grenade grenade, bool entering) {
  const rules::GrenadeDice dice = rules::GrenadeHitDice(grenade);
  std::string code = kPatch;
  Replace(&code, "$FIRST",
          entering ? "GetEnteringObject()" : "GetFirstInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE)");
  Replace(&code, "$NEXT",
          entering ? "OBJECT_INVALID" : "GetNextInPersistentObject(OBJECT_SELF, OBJECT_TYPE_CREATURE)");
  Replace(&code, "$ONLY", grenade == rules::Grenade::kHolyWater ? kPatchHolyOnly : "");
  Replace(&code, "$LOCK", std::to_string(rules::kFlaskPatchLockSeconds));
  Replace(&code, "$SPELL", std::to_string(FlaskSpell(grenade)));
  Replace(&code, "$DICE", Dice(dice));
  Replace(&code, "$TYPE", FlaskDamageType(grenade));
  Replace(&code, "$VIS", FlaskImpact(grenade));
  if (LogSlot()) {
    Log("grenades: 0x%08X %s's patch (the house rule) - %s: %s, no save, once a round", self, FlaskName(grenade),
        entering ? "a creature entering" : "its round, everyone in it", Dice(dice).c_str());
  }
  Run(code, self, "a flask's patch");
}

void AcidPatchIn(uint32_t self) { PatchScript(self, rules::Grenade::kAcid, true); }
void AcidPatchRound(uint32_t self) { PatchScript(self, rules::Grenade::kAcid, false); }
void FirePatchIn(uint32_t self) { PatchScript(self, rules::Grenade::kAlchemistsFire, true); }
void FirePatchRound(uint32_t self) { PatchScript(self, rules::Grenade::kAlchemistsFire, false); }
void HolyPatchIn(uint32_t self) { PatchScript(self, rules::Grenade::kHolyWater, true); }
void HolyPatchRound(uint32_t self) { PatchScript(self, rules::Grenade::kHolyWater, false); }

void AcidScript(uint32_t self) { FlaskScript(self, rules::Grenade::kAcid); }
void AlchemistsFireScript(uint32_t self) { FlaskScript(self, rules::Grenade::kAlchemistsFire); }
void HolyWaterScript(uint32_t self) { FlaskScript(self, rules::Grenade::kHolyWater); }

void BombScript(uint32_t self) {
  const int dc = DcFor(self);
  std::string code = kBomb;
  Replace(&code, "$DICE", Dice(rules::GrenadeHitDice(rules::Grenade::kFireBomb)));
  Replace(&code, "$DC", std::to_string(dc));
  Replace(&code, "$BFIN", kFireCloudIn);
  Replace(&code, "$BFRD", kFireCloudRound);
  Replace(&code, "$BAIN", kAcidCloudInName);
  Replace(&code, "$BARD", kAcidCloudRoundName);
  Replace(&code, "$BAOUT", kAcidCloudOutName);
  Replace(&code, "$LIGHTTAG", kFireLightTag);
  if (LogSlot()) {
    Log("grenades: 0x%08X a bomb - the target's Dexterity save, DC %d, for half of 10d6; its cloud hurts anyone in it "
        "(a Fire Bomb's gives light)", self, dc);
  }
  CreatingFireLight(true);  // a Fire Bomb's light (light_sources.cpp)
  Run(code, self, "bomb");
  CreatingFireLight(false);
  BlastPutsOutLights(self, rules::GrenadeBlastRadius(rules::Grenade::kFireBomb), "a bomb's blast");  // 0.71.0
}

// 0.71.0: the Thunderstone - NWN's, its DC the thrower's - and its bang puts out the lights round it.
void ThunderstoneScript(uint32_t self) {
  const int dc = DcFor(self);
  const float radius = rules::GrenadeBlastRadius(rules::Grenade::kThunderstone);
  std::string code = kThunder;
  Replace(&code, "$RADIUS", std::to_string(radius));
  Replace(&code, "$DC", std::to_string(dc));
  if (LogSlot()) {
    Log("grenades: 0x%08X a Thunderstone - non-friends within %.2f m: a Constitution save, DC %d, or deafened 5 rounds",
        self, radius, dc);
  }
  Run(code, self, "Thunderstone");
  BlastPutsOutLights(self, radius, "a Thunderstone's bang");
}

void CaltropsEnterScript(uint32_t self) {
  std::string code = kCaltropsEnter;
  Replace(&code, "$DC", std::to_string(rules::kCaltropsSaveDc));
  Replace(&code, "$DAMAGE", std::to_string(rules::kCaltropsDamage));
  Replace(&code, "$LIFE", std::to_string(rules::kCaltropsFieldDamage));
  Run(code, self, "Caltrops");
}

void CaltropsRoundScript(uint32_t) {}  // 4A: entering only - NWN's point each round is gone

// 0.70.0: the clouds' scripts - a creature entering, or the round tick (everyone in it).
void CloudScript(uint32_t self, const char* chunk, bool entering, int types, const char* what) {
  std::string code = chunk;
  const std::string kinds = std::to_string(types);
  Replace(&code, "$FIRST", entering ? "GetEnteringObject()" : "GetFirstInPersistentObject(OBJECT_SELF, " + kinds + ")");
  Replace(&code, "$NEXT", entering ? "OBJECT_INVALID" : "GetNextInPersistentObject(OBJECT_SELF, " + kinds + ")");
  Replace(&code, "$LIGHTTAG", kFireLightTag);
  Replace(&code, "$SLOWTAG", kAcidSlowTag);
  if (LogSlot()) Log("grenades: 0x%08X %s - %s (anyone in it)", self, what, entering ? "a creature entering" : "its round");
  Run(code, self, what);
}

constexpr int kCreatures = 1;              // OBJECT_TYPE_CREATURE
constexpr int kCreaturesDoorsPlaceables = 1 | 8 | 64;  // NWN's Incendiary Cloud's round: creatures, doors, placeables

void FireCloudIn(uint32_t self) { CloudScript(self, kFireCloud, true, kCreatures, "a Fire Bomb's cloud"); }
void FireCloudRound(uint32_t self) {
  CloudScript(self, kFireCloud, false, kCreaturesDoorsPlaceables, "a Fire Bomb's cloud");
}
void AcidCloudIn(uint32_t self) { CloudScript(self, kAcidCloudIn, true, kCreatures, "an Acid Bomb's cloud"); }
void AcidCloudRound(uint32_t self) { CloudScript(self, kAcidCloudRound, false, kCreatures, "an Acid Bomb's cloud"); }
void AcidCloudOut(uint32_t self) {
  std::string code = kAcidCloudOut;
  Replace(&code, "$SLOWTAG", kAcidSlowTag);
  Run(code, self, "an Acid Bomb's cloud, leaving");
}
void ChokeIn(uint32_t self) { CloudScript(self, kChoke, true, kCreatures, "Choking Powder's cloud"); }
void ChokeRound(uint32_t self) { CloudScript(self, kChoke, false, kCreatures, "Choking Powder's cloud"); }

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
  // 0.69.1: the aiming circles as the 2024 areas (data/rows/spells.if_grenades2024.txt) - NWN's were its splash.
  ServeConditionalRows("grenades2024");
  const bool scripts = AddScriptHandler("x0_s3_acid", &AcidScript) &&
                       AddScriptHandler("x0_s3_alchem", &AlchemistsFireScript) &&
                       AddScriptHandler("x0_s3_holy", &HolyWaterScript) && AddScriptHandler("x2_s3_bomb", &BombScript) &&
                       AddScriptHandler("x0_s3_calen", &CaltropsEnterScript) &&
                       AddScriptHandler("x0_s3_thunder", &ThunderstoneScript) &&
                       AddScriptHandler("x0_s3_calhb", &CaltropsRoundScript);
  Log("grenades: 2024 gear - the flasks a Dexterity save (DC 8 + Dex + PB) for 2024's damage, no splash; burning 3 "
      "rounds; the bombs Dexterity half; Caltrops DC 15 on entering; a throw mid-round takes a swing%s",
      scripts ? "" : " - NOT available (no script hook)");
  // #130: the patches' scripts (the house rule is read at each throw: a test profile may turn it on).
  const bool patches = AddScriptHandler(PatchScriptName(rules::Grenade::kAcid, "in").c_str(), &AcidPatchIn) &&
                       AddScriptHandler(PatchScriptName(rules::Grenade::kAcid, "rd").c_str(), &AcidPatchRound) &&
                       AddScriptHandler(PatchScriptName(rules::Grenade::kAlchemistsFire, "in").c_str(), &FirePatchIn) &&
                       AddScriptHandler(PatchScriptName(rules::Grenade::kAlchemistsFire, "rd").c_str(),
                                        &FirePatchRound) &&
                       AddScriptHandler(PatchScriptName(rules::Grenade::kHolyWater, "in").c_str(), &HolyPatchIn) &&
                       AddScriptHandler(PatchScriptName(rules::Grenade::kHolyWater, "rd").c_str(), &HolyPatchRound);
  // 0.70.0: the bombs' clouds and Choking Powder's - anyone in them.
  const bool clouds = AddScriptHandler(kFireCloudIn, &FireCloudIn) && AddScriptHandler(kFireCloudRound, &FireCloudRound) &&
                      AddScriptHandler(kAcidCloudInName, &AcidCloudIn) &&
                      AddScriptHandler(kAcidCloudRoundName, &AcidCloudRound) &&
                      AddScriptHandler(kAcidCloudOutName, &AcidCloudOut) &&
                      AddScriptHandler("x0_s3_chokeen", &ChokeIn) && AddScriptHandler("x0_s3_chokehb", &ChokeRound);
  Log("grenades: every grenade hazard hurts anyone in it - the patches, Caltrops, the bombs' and Choking Powder's "
      "clouds; fire gives light%s", clouds ? "" : " - the clouds NOT available (no script hook)");
  Log("grenades: flask patches, a house rule ([house_rules] flask_patches): %s%s",
      g_config.flask_patches ? "on - a flask thrown at the ground leaves a patch for 2 rounds" : "off",
      patches ? "" : " - NOT available (no script hook)");
}
