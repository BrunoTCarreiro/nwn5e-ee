#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "rules/grapple.h"  // FeetToMetres
#include "rules/saves.h"

// #39 Part 2a (owner, 2026-11-09: "go A, level by level", then "A on all, but mark featherfall for later removal"),
// `spellcasting` faithful: SRD 5.2.1's levelled spells, cast by core/rules_spells2024.cpp. Build 1a: the level 1
// damage and healing spells - Burning Hands, Magic Missile, Guiding Bolt, Inflict Wounds, Ray of Sickness, Cure
// Wounds, False Life, Dissonant Whispers, Thunderwave, Ice Knife. Their numbers, their areas and their upcasting.
// Build 1b (owner, 2026-11-09: "go for 1b"): Healing Word (a Bonus Action spell that spends a slot), Chromatic Orb
// (a damage-type menu, its orb leaping on matching dice), Heroism (Concentration: Temporary Hit Points each turn).
// Build 2a (owner, 2026-11-10: "go for build 2"): the buffs - Bless, Bane, Shield of Faith, Mage Armor, Divine
// Favor, Protection from Evil and Good, Sanctuary, Expeditious Retreat, Longstrider, Hunter's Mark.
// Build 2b (owner, 2026-11-12: "2a accepted, go for 2b"): conditions on creatures - Command, Charm Person, Animal
// Friendship, Hideous Laughter, Color Spray, Sleep.
// Build 2c (owner, 2026-11-12: "2b accepted, go for 2c"): the areas - Entangle, Grease, Faerie Fire, Fog Cloud.
// Build 3 (owner, 2026-11-12: "2c accepted, go for 3"): reactions and smites - Shield, Feather Fall, Divine Smite,
// Searing Smite, Ensnaring Strike.

namespace rules {

enum class SpellEffect2024 {
  kSpellAttack,         // a spell attack: its dice on a hit, twice the dice on a Critical Hit
  kSave,                // a saving throw: its dice on a failure; half (rounded down) on a success if `half_on_save`
  kDarts,               // Magic Missile: darts that always hit, each its own damage
  kHealing,             // Hit Points regained: its dice + the spellcasting ability modifier
  kTemporaryHitPoints,  // False Life: its dice + `flat`
  kBuff,                // Heroism: no roll
  kCondition,           // build 2b: a condition on creatures - a save, no damage
  kArea,                // build 2c: an area that lasts (its creatures' saves at the cast, its terrain after)
  kReaction,            // build 3: taken by its trigger (Shield), or a visual and a line (Feather Fall)
  kSmite,               // build 3: armed by the cast, paid and delivered by the next qualifying hit
  kUtility,             // build 4: detection and utility - its own effect, no damage
};

enum class SpellShape2024 {
  kOne,             // one creature
  kCone,            // a Cone from the caster toward the point aimed at, `feet` long
  kCubeFromCaster,  // a Cube with one face on the caster, toward the point aimed at, `feet` on a side
  kSphere,          // a Sphere centred on the point aimed at, `feet` its radius (Sleep)
  kSquare,          // a square on the ground centred on the point aimed at, `feet` on a side (a Cube's: its floor)
};

struct Spell2024 {
  int row = -1;              // spells.2da
  const char* name = "";
  const char* script = "";   // its ImpactScript (core/rules_spells2024.cpp runs it)
  int level = 1;
  SpellEffect2024 effect = SpellEffect2024::kSave;
  bool melee = false;        // a Touch spell
  Ability save = Ability::kDexterity;
  bool half_on_save = false;
  int dice = 0;              // per target (Magic Missile: per dart)
  int die = 0;
  int flat = 0;              // + flat (Magic Missile's dart, False Life)
  int upcast_dice = 0;       // dice more for each slot level above `level`
  int upcast_flat = 0;       // False Life: 5 more for each slot level above 1
  uint32_t damage_type = 0;  // the engine's damage type (cantrips.h's constants; 0: none)
  SpellShape2024 shape = SpellShape2024::kOne;
  int feet = 0;              // the area's size
  bool bonus_action = false;  // cast with a Bonus Action (Healing Word)
  bool ritual = false;        // build 4: the Ritual tag (Q1 A: a Ritual out of combat, no slot)
};

// The spells built (ascending by row) and one by its row (null: not one of them).
const std::vector<Spell2024>& Spells2024();
const Spell2024* Spell2024Of(int row);

// The new rows (spells.if_spells2024.level1.txt).
constexpr int kGuidingBolt = 1147;
constexpr int kRayOfSickness = 1148;
constexpr int kFalseLife = 1149;
constexpr int kDissonantWhispers = 1150;
// NWN's rows rewritten.
constexpr int kBurningHands = 10;
constexpr int kCureWounds = 32;     // NWN's Cure Light Wounds
constexpr int kMagicMissile = 107;
constexpr int kInflictWounds = 432;  // NWN's Inflict Light Wounds
constexpr int kThunderwave = 436;    // NWN's Balagarn's Iron Horn
constexpr int kIceKnife = 543;       // NWN's Ice Dagger
constexpr int kNwnShield = 417;      // NWN's Shield (until build 3): no Magic Missile damage while it lasts
// Build 1b.
constexpr int kHealingWord = 1151;
constexpr int kHealingWordBonus = 1152;  // its Bonus Action: the row its feat casts (no class lists)
constexpr int kFeatHealingWord = 1555;   // the feat carrying it (feat.2da SPELLID 1152)
constexpr int kChromaticOrb = 1153;      // its six types, 1154-1159: cantrips.h's choice menus
constexpr int kChromaticOrbAcid = 1154;
constexpr int kChromaticOrbCold = 1155;
constexpr int kChromaticOrbFire = 1156;
constexpr int kChromaticOrbLightning = 1157;
constexpr int kChromaticOrbPoison = 1158;
constexpr int kChromaticOrbThunder = 1159;
constexpr int kHeroism = 1160;
constexpr int kHealingWordFeet = 60;
constexpr int kChromaticOrbLeapFeet = 30;
// nwn5e: an upcast Heroism's extra creatures - the friends this close to you ("a willing creature you touch").
constexpr int kHeroismReachFeet = 10;

// The slot level a cast spends: the one chosen (0: the spell's own), never below the spell's own, at most 9.
int CastSlotLevel(int spell_level, int chosen);
// The dice at a slot level: `dice`, and `per_level` more for each slot level above the spell's.
int DiceAtSlot(int dice, int per_level, int spell_level, int slot_level);
int SpellDiceAt(const Spell2024& spell, int slot_level);
// A Critical Hit rolls the attack's damage dice twice.
int CriticalDice(int dice, bool critical);
// The damage after a save: all of it on a failure; on a success half (rounded down) or none.
int DamageAfterSave(int damage, bool saved, bool half_on_save);
// Magic Missile: three darts, one more for each slot level above 1.
int MagicMissileDarts(int slot_level);
// False Life: the 2d4 roll + 4, and 5 more for each slot level above 1.
int FalseLifeTemporaryHitPoints(int roll, int slot_level);
// Temporary Hit Points don't stack: new ones replace those you have only when there are more of them.
bool TemporaryHitPointsReplace(int current, int offered);
// Ice Knife: the shard's 1d10 Piercing on a hit, then its explosion - the target and each creature within 5 feet,
// a Dexterity save or 2d6 Cold (nothing on a success), 1d6 more for each slot level above 1.
constexpr int kIceKnifeExplosionDie = 6;
constexpr int kIceKnifeExplosionFeet = 5;
int IceKnifeExplosionDice(int slot_level);
// Build 2a.
constexpr int kBless = 6;
constexpr int kBane = 449;
constexpr int kShieldOfFaith = 450;
constexpr int kMageArmor = 102;
constexpr int kDivineFavor = 414;
constexpr int kProtectionFromEvilAndGood = 321;  // NWN's Protection from Alignment, one spell now
constexpr int kSanctuary = 154;
constexpr int kExpeditiousRetreat = 456;
constexpr int kLongstrider = 1161;
constexpr int kHuntersMark = 1162;
// The Bonus Action spells' rows their feats cast, and two Bonus Actions while a spell lasts.
constexpr int kShieldOfFaithBonus = 1163;
constexpr int kDivineFavorBonus = 1164;
constexpr int kSanctuaryBonus = 1165;
constexpr int kExpeditiousRetreatBonus = 1166;
constexpr int kHuntersMarkBonus = 1167;
constexpr int kExpeditiousRetreatDash = 1168;  // Expeditious Retreat's Dash, a Bonus Action each turn
constexpr int kHuntersMarkMove = 1169;         // Hunter's Mark moved to a new creature
constexpr int kFeatShieldOfFaith = 1556;
constexpr int kFeatDivineFavor = 1557;
constexpr int kFeatSanctuary = 1558;
constexpr int kFeatExpeditiousRetreat = 1559;
constexpr int kFeatHuntersMark = 1560;
constexpr int kFeatExpeditiousDash = 1561;
constexpr int kFeatHuntersMarkMove = 1562;

// A spell cast with a Bonus Action that spends a slot (Healing Word's way, build 1b): its order becomes the use of
// `feat`, which casts `bonus_row` (instant, one Bonus Action a round); the order's slot is spent by the script.
struct BonusActionSpell {
  int spell = -1;      // the row on the class lists
  int bonus_row = -1;  // the row its feat casts
  int feat = -1;
  int feet = 0;        // its range (0: Self)
  const char* name = "";
};
const std::vector<BonusActionSpell>& BonusActionSpells();
// By its row or its feat's row (null: none).
const BonusActionSpell* BonusActionSpellOf(int row);
// The feats of those spells and of the Bonus Actions they give while they last (Expeditious Retreat's Dash,
// Hunter's Mark's move) - each a Bonus Action (action_economy.h); "" / false for any other feat.
bool IsSlotSpellBonusFeat(int feat);
const char* SlotSpellBonusFeatName(int feat);

// Bless and Bane: 1d4 added to (Bane: taken from) attack rolls and saving throws; three creatures, one more for
// each slot level above 1 (nwn5e: the nearest to the spot aimed at, within 30 feet of you).
constexpr int kBlessDie = 4;
constexpr int kBlessFeet = 30;
int BlessTargets(int slot_level);
// Shield of Faith: +2 AC. Mage Armor: a base AC of 13 + Dexterity while no armour is worn.
constexpr int kShieldOfFaithAc = 2;
int MageArmorBaseAc(int dexterity_modifier);
// Divine Favor: a weapon hit's extra 1d4 Radiant. Hunter's Mark: an attack roll's hit on the quarry, 1d6 Force.
// A Critical Hit rolls them twice.
constexpr int kDivineFavorDie = 4;
constexpr int kHuntersMarkDie = 6;
// Hunter's Mark's Concentration: 1 hour; 8 hours with a level 3-4 slot, 24 with a level 5+ one.
int HuntersMarkHours(int slot_level);
// Longstrider: +10 feet of Speed; one creature, one more for each slot level above 1.
constexpr int kLongstriderFeet = 10;
int LongstriderTargets(int slot_level);
// Protection from Evil and Good: the creature types it wards against (NWN's racial types: Aberration, Elemental,
// Fey, Outsider - a Celestial or a Fiend -, Undead).
bool WardedAgainstRacialType(int nwn_racial_type);
// Sanctuary ends when the warded creature makes an attack roll. NWN keeps a player attacking its target until it
// is given another order, so a player's Sanctuary on itself mid-fight ended at its next swing (the owner's L2-7,
// 2026-11-10). #39 build 2a (owner, 2026-11-11: "go with A for sanctuary"): a player's Sanctuary on
// itself stops its attacks; a click on an enemy attacks again (and ends it). On someone else - a henchman, whose
// AI decides - or cast by a henchman: nothing stopped.
bool SanctuaryStopsOwnAttacks(bool on_caster, bool caster_is_player);
// Hunter's Mark: Move used (0.99.22; owner, 2026-11-12: "give rangers an option to keep it armed"): its quarry down - the
// mark moves; still standing - with the Ranger's option on and another creature picked it's armed (it goes once
// the quarry drops, that round's Bonus Action), else refused (SRD 5.2: the mark moves only once the target drops).
enum class MarkMove { kMove, kArm, kRefuse };
MarkMove HuntersMarkMoveNow(bool quarry_down, bool keep_armed, bool other_creature_picked);

// Build 2b: conditions on creatures.
constexpr int kCharmPerson = 16;
constexpr int kCharmPersonOrAnimal = 17;  // NWN's: off the faithful lists (Charm Person, Animal Friendship)
constexpr int kColorSpray = 24;
constexpr int kSleep = 165;
constexpr int kHideousLaughter = 457;  // NWN's Tasha's Hideous Laughter (level 2 there)
constexpr int kCommand = 1170;         // its four words, 1171-1174: cantrips.h's choice menus
constexpr int kCommandDrop = 1171;
constexpr int kCommandFlee = 1172;
constexpr int kCommandGrovel = 1173;
constexpr int kCommandHalt = 1174;
constexpr int kAnimalFriendship = 1175;
// Command: a failed Wisdom save and the target follows the word on its next turn (nwn5e: the next round) - Drop
// (what it holds goes into its inventory, its AI arms itself again: owner, 2026-11-12: "go with B for Q1 and A for Q2. A for Q2 could be the same help action, just renamed",
// Q1 B; then its turn ends), Flee (its turn spent moving away from you), Grovel (Prone, then its turn ends), Halt
// (no move, no action). The master cast with no word chosen (an item's): Halt.
enum class CommandWord { kDrop, kFlee, kGrovel, kHalt };
CommandWord CommandWordOf(int row);
const char* CommandWordName(CommandWord word);
// Command, Charm Person, Animal Friendship, Hideous Laughter: one creature, one more for each slot level above 1
// (nwn5e: the one aimed at, then the enemies nearest it within range).
int ConditionSpellTargets(int slot_level);
// Their ranges in feet (Color Spray 0: its Cone from you).
int ConditionSpellRangeFeet(int row);
// Their durations: in rounds (6 s) for a minute or less - Command 1 (its next turn), Color Spray 2 (to the end of
// your next turn, the cantrips' reading), Hideous Laughter and Sleep 10 - else in hours (NWN's game hours, as Mage
// Armor's 8): Charm Person 1, Animal Friendship 24. 0: not one of them, or not in that unit.
int ConditionSpellRounds(int row);
int ConditionSpellHours(int row);
// Charm Person: a Humanoid (NWN's racial types: the seven player races, goblinoids, orcs, reptilians); Animal
// Friendship: a Beast (animals, beasts, vermin). The others: any creature.
bool IsHumanoidRacialType(int nwn_racial_type);
bool IsBeastRacialType(int nwn_racial_type);
bool ConditionSpellTargetFits(int row, int nwn_racial_type);
// Charm Person's save has Advantage if you or your allies are fighting it (nwn5e: an enemy, while you or it is in
// combat).
bool CharmSaveAdvantage(bool enemy, bool fighting);
// Hideous Laughter: a save at the end of each of its turns and each time it takes damage - that one with Advantage.
bool LaughterSaveAdvantage(bool after_damage);
// Sleep: a failed save - Incapacitated until the end of its next turn, then the save again: a failure Unconscious
// for the duration, a success awake. Damage, or someone within 5 feet taking an action to shake it (Rouse, the
// Help action renamed - owner, 2026-11-12: "go with B for Q1 and A for Q2. A for Q2 could be the same help action, just renamed",
// Q2 A), ends it on that creature. Creatures that don't sleep (elves) or are immune to Exhaustion (Constructs,
// Elementals, Oozes, Undead) succeed - and NWN's sleep immunity.
enum class SleepStage { kIncapacitated, kUnconscious, kAwake };
SleepStage SleepAfterSecondSave(bool saved);
bool SleepUnaffectedRacialType(int nwn_racial_type);
// 0.99.24 (the owner's L2b run, 2026-11-12: an upcast Hideous Laughter knocked down the orc Charm Person had just
// charmed): who an upcast's extra creatures and Sleep's creatures of your choice are picked from - your enemies, not
// one you've charmed (Friendly to you; NWN's enemy test doesn't know it).
bool PickedAgainst(bool enemy, bool charmed_by_caster);

// Build 2c: the areas (owner, 2026-11-12: "All recommended except Q3, go for B").
constexpr int kEntangle = 53;
constexpr int kGrease = 66;
constexpr int kFaerieFire = 1176;        // its colours, 1177-1179: cantrips.h's choice menus (Q4 A)
constexpr int kFaerieFireBlue = 1177;
constexpr int kFaerieFireGreen = 1178;
constexpr int kFaerieFireViolet = 1179;
constexpr int kFogCloud = 1180;
constexpr int kEscape = 1181;            // the Escape action's row (Q1 A)
constexpr int kFeatEscape = 1563;
// Entangle: each creature in its square at the cast but you, a Strength save or Restrained until the spell ends; an
// action's Strength (Athletics) check against your spell save DC frees it (Q1 A: the Escape action; an NPC tries once
// a round in place of its attacks). Grease: a Dexterity save or Prone when it appears, on entering and at the end of
// a turn there. Both are Difficult Terrain - every foot of movement costs one more: nwn5e, half Speed inside. The
// Escape check is the Grapple's (grapple.h: EscapeSucceeds, d20 + the bonus against the DC), with Athletics alone.
float DifficultTerrainFactor(float factor, bool inside);
// Faerie Fire's light (Q4 A): blue, green or violet - the master cast with none chosen (an item's): blue.
enum class FaerieColour { kBlue, kGreen, kViolet };
FaerieColour FaerieColourOf(int row);
const char* FaerieColourName(FaerieColour colour);
// Faerie Fire's outline (Q3 B): an outlined creature can't benefit from Invisible - its invisibility is suppressed
// while it's outlined and comes back for the time it had left (-1: a permanent one, back for good; 0: it ran out).
int InvisibilityLeftSeconds(int left_when_suppressed, int seconds_suppressed);
// Fog Cloud: a 20-foot-radius Sphere, 20 feet more for each slot level above 1.
int FogCloudRadiusFeet(int slot_level);
// The areas' ranges in feet: Entangle 90, Grease and Faerie Fire 60, Fog Cloud 120.
int AreaSpellRangeFeet(int row);

// Build 3: reactions and smites (owner, 2026-11-12: "go with A on all").
constexpr int kShield = kNwnShield;     // NWN's Shield 417, SRD 5.2.1's now
constexpr int kFeatherFall = 1182;      // a visual and a line - NWN has no falling (#153)
constexpr int kDivineSmite = 1183;
constexpr int kSearingSmite = 1184;
constexpr int kEnsnaringStrike = 1185;
// Shield: a Reaction when you're hit by an attack roll (or targeted by Magic Missile) - +5 AC until the start of
// your next turn, the triggering attack included. nwn5e: taken when it would turn the hit - never a natural 20.
constexpr int kShieldAc = 5;
bool ShieldTurnsHit(int total, int armor_class, bool natural_twenty);
// The smites (Q1 A): armed by the cast for nothing; the next qualifying hit, while you have the armed slot's level
// and the Bonus Action, spends both and delivers it. Divine Smite and Searing Smite ride a melee weapon or an
// Unarmed Strike; Ensnaring Strike any weapon (an Unarmed Strike isn't one).
bool SmiteRidesOn(int row, bool ranged, bool unarmed);
// Divine Smite: 2d8 Radiant, 1d8 more on a Fiend or an Undead and for each slot level above 1.
bool FiendOrUndead(int nwn_racial_type, bool evil);
int DivineSmiteDice(int slot_level, bool fiend_or_undead);
// Searing Smite: 1d6 Fire on the hit and at the start of each of its turns (a Constitution save ends it); Ensnaring
// Strike: Restrained on a failed Strength save - Advantage if Large or larger - and 1d6 Piercing at the start of each
// of its turns. Each 1d6 more for each slot level above 1.
int SearingSmiteDice(int slot_level);
int EnsnaringStrikeDice(int slot_level);
bool EnsnaringSaveAdvantage(int nwn_creature_size);
// The smites. A smite's cast only arms it (Q1 A): the spell is cast on the hit, so a Concentration smite's
// Concentration begins there, never at the arming cast (0.99.30, L3-8).
bool IsSmiteSpell(int row);
// #162 G (owner, 2026-11-13: "go with G, B and D for #162"): an armed smite's icon on the effect bar -
// effecticons 225-227 (data/rows/effecticons.if_spells2024.txt), the game's own icons; 0 for any other spell.
constexpr int kSmiteArmedIconFirst = 225;
constexpr int kSmiteArmedIconLast = 227;
int SmiteArmedIconRow(int row);

// Build 4a (owner, 2026-11-15: "go for build 4"; 2026-11-16: "I accepted your recommendations, except for Q4 ...
// Keep it in the roleplay list"): detection and utility. New rows; Identify is NWN's row, rewritten.
constexpr int kAlarm = 1186;
constexpr int kAlarmAudible = 1187;
constexpr int kAlarmMental = 1188;
constexpr int kComprehendLanguages = 1189;
constexpr int kCreateOrDestroyWater = 1190;
constexpr int kCreateWater = 1191;
constexpr int kDestroyWater = 1192;
constexpr int kDetectEvilAndGood = 1193;
constexpr int kDetectMagic = 1194;
constexpr int kDetectPoisonAndDisease = 1195;
constexpr int kGoodberry = 1196;
constexpr int kGoodberryEat = 1197;  // the berries' power (their Cast Spell property): on no class list
constexpr int kIllusoryScript = 1198;
constexpr int kPurifyFoodAndDrink = 1199;
constexpr int kSpeakWithAnimals = 1200;
constexpr int kUnseenServant = 1201;
constexpr int kIdentify = 86;  // NWN's row, rewritten (its +Lore gone)

// Build 4b (owner, 2026-11-16: "yes, 4a accepted. go for 4b"): Disguise Self and Silent Image, real (Q4 A,
// 2026-11-09); Find Familiar waits for its picks. New rows; each menu's rows on no class list.
constexpr int kDisguiseSelf = 1202;
constexpr int kDisguiseDwarf = 1203;
constexpr int kDisguiseElf = 1204;
constexpr int kDisguiseGnome = 1205;
constexpr int kDisguiseHalfling = 1206;
constexpr int kDisguiseHuman = 1207;
constexpr int kDisguiseOrc = 1208;
constexpr int kSilentImage = 1209;
constexpr int kSilentImageTreasure = 1210;
constexpr int kSilentImageCorpse = 1211;
constexpr int kSilentImageHumanoid = 1212;
constexpr int kSilentImageCreature = 1213;
constexpr int kSilentImageLarge = 1214;
constexpr int kSilentImageMove = 1215;  // its Move: a Magic action while concentrating, no slot

// Rituals (Q1 A, #38's "A for now" revisited): a prepared spell with the Ritual tag can be cast as a Ritual - no
// spell slot (so never above its level), 10 minutes longer. nwn5e: a class's cast out of combat, while the
// character's switch is on (!options, on by default); in combat it takes a slot as usual.
constexpr int kRitualMinutes = 10;
bool IsRitualSpell(int row);  // the row, or its menu's master, has the tag
bool CastAsRitual(bool ritual_spell, bool switch_on, bool in_combat, bool class_cast);
int CastingMinutes(int row);                    // 1: Alarm, Identify, Illusory Script; 0: an Action
int RitualCastingMinutes(int casting_minutes);  // its casting time + 10

// Alarm: a ward (a 20-foot Cube) for 8 hours; a creature not designated (nwn5e: the caster's party) entering it
// alerts - Audible (a handbell heard within 60 feet) or Mental (the caster, waking it).
constexpr int kAlarmCubeFeet = 20;
constexpr int kAlarmHours = 8;
constexpr int kAlarmAudibleFeet = 60;
constexpr int kAlarmBellSeconds = 10;  // the handbell's sound lasts 10 seconds
enum class AlarmMode { kNone, kAudible, kMental };
AlarmMode AlarmModeOf(int row);

// Create or Destroy Water: Create's rain in a 30-foot Cube puts out exposed flames there; Destroy's ends the fog
// there. The Cube 5 feet more for each slot level above 1.
enum class WaterMode { kNone, kCreate, kDestroy };
WaterMode WaterModeOf(int row);
int WaterCubeFeet(int slot_level);

// The Detects: 10 minutes' Concentration; what each senses within 30 feet, in sight (1 foot of stone blocks it),
// glows and is named (Q3 A, 2026-11-09).
constexpr int kDetectFeet = 30;
constexpr int kDetectMinutes = 10;
bool IsDetectSpell(int row);
// Detect Evil and Good: an Aberration, Celestial, Elemental, Fey, Fiend or Undead (Protection from Evil and Good's
// types) - the kind named; nullptr: not sensed. NWN's Outsider (`nwn_good_evil`: ALIGNMENT_GOOD 4, ALIGNMENT_EVIL
// 5): a Fiend if evil, a Celestial if good.
const char* DetectEvilAndGoodKind(int nwn_racial_type, int nwn_good_evil);
// A spell's school by spells.2da's School letter (Detect Magic names it); nullptr for none.
const char* SpellSchoolName(char letter);
// Detect Poison and Disease (0.99.43; L4-9, the giant spider's bite): what a weapon's item property tells it. NWN
// keeps a poison or a disease two ways - a weapon's On Hit (itemprops 48: 19 poison, 20 disease) and a creature
// weapon's On Monster Hit (72: 6 poison, 2 disease). `natural`: a creature's own weapon (a bite, a claw). nullptr:
// nothing it senses.
constexpr int kItemPropertyOnHit = 48;
constexpr int kItemPropertyOnMonsterHit = 72;
const char* PoisonSenseKind(int item_property, int subtype, bool natural);
// Detect Magic's aura shows only on what the caster sees (SRD 5.2: "any visible creature or object") - nothing in
// a Fog Cloud (Heavily Obscured) or from inside one, no creature unseen (0.99.43, L4-2). Detect Evil and Good and
// Detect Poison and Disease sense locations, seen or not.
bool DetectNeedsSight(int row);

// Goodberry: ten berries for 24 hours, each eaten (a Bonus Action) for 1 Hit Point.
constexpr int kGoodberryCount = 10;
constexpr int kGoodberryHours = 24;
constexpr int kGoodberryHitPoints = 1;

// Q2 A (Comprehend Languages, Illusory Script, Purify Food and Drink, Speak with Animals) and Unseen Servant (Q4):
// no NWN use - castable, a line, and an effect on the caster for its duration that a module can read
// (GetHasSpellEffect(row)): its minutes, 0 for none (instantaneous, or on writing, not on the caster).
int UtilityEffectMinutes(int row);

// The Wizard's alone of the arcane spells NWN's one Wiz_Sorc column shares (Alarm, Identify, Illusory Script,
// Unseen Servant): not the Sorcerer's.
bool WizardOnlySpell(int row);

// Disguise Self: another species' look for an hour (its menu - the 2024 species NWN has a player model for):
// NWN's appearance - 0 Dwarf, 1 Elf, 2 Gnome, 3 Halfling, 6 Human, 5 for an Orc (the Half-Orc's model); -1 for
// any other row (the master: no species picked).
constexpr int kDisguiseSelfHours = 1;
int DisguiseAppearanceOf(int row);
// A player model's head, p<gender><race><phenotype>_head<NNN> ("pmd0_head001"): the disguise keeps the head
// when the new species' model has it, else head 1. "" for an appearance that isn't a player model.
std::string PlayerHeadModel(bool female, int nwn_appearance, int phenotype, int head);
// Silent Image: an image no larger than a 15-foot Cube within 60 feet, for 10 minutes (Concentration). Its Move
// (a Magic action) takes it to any spot within range - 60 feet of the caster.
constexpr int kSilentImageSeconds = 600;
constexpr int kSilentImageRangeFeet = 60;
bool SilentImageMoveAllowed(float feet_from_caster);
// A menu row cast for no slot (Silent Image's Move): its radial entry opens no slot levels.
bool IsNoSlotOption(int row);
// 0.99.47 (L4-27, owner: "how do I move the illusion? I don't see the option" - a levelled spell's slotless leaf
// isn't shown): Silent Image's Move is a feat, as Hunter's Mark's is - granted while the image lasts, casting its
// Move row (1215) with the Action (feat.2da ReqAction 1), in the spell's menu and the quickbar. The feat of a menu
// row taken as a feat (0: none).
constexpr int kFeatSilentImageMove = 1564;
int ChoiceOptionFeat(int row);
constexpr int kRouseFeet = 5;

// Chromatic Orb: two or more of its d8s alike - the orb leaps to another creature within 30 feet of the target (a
// new attack roll, a new damage roll), at most as many times as the slot's level, each creature once a casting.
bool HasMatchingDice(const std::vector<int>& rolls);
int ChromaticOrbLeaps(int slot_level);
// Heroism: one creature, one more for each slot level above 1; Temporary Hit Points equal to the spellcasting
// ability modifier (none below 1) at the start of each of its turns.
int HeroismTargets(int slot_level);
int HeroismTemporaryHitPoints(int modifier);
// SRD 5.2.1, One Spell with a Spell Slot per Turn: a turn is the creature's round - the same one when none of its
// rounds ended since and less than a round's time (6 s, out of combat) went by.
bool SameTurn(uint32_t rounds_then, uint32_t rounds_now, long long ms_since);
// Dissonant Whispers: a failed save spends the target's Reaction moving away - as far as its Speed (nwn5e: 30
// feet, everyone's) takes it.
constexpr int kDissonantWhispersFleeFeet = 30;

// The areas, from the caster toward the point it aims at: a creature `along` metres ahead and `side` metres to the
// side is in it when part of it is (`slack`: its own half-width).
bool InCone(float along, float side, float length, float slack);
bool InCubeFromCaster(float along, float side, float edge, float slack);

}  // namespace rules
