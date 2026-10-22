// SK1–SK3 (a) (port of 1.69's rules_skills.cpp; docs/EE-PORT.md E3b), with
// `skills` faithful:
// - SK2: GetSkillRank returns the 2024 value: the 2024 skill's ability
//   modifier + proficiency bonus if proficient (+ again with Expertise) + item
//   and spell effects. Ranks, Skill Focus, racial bonuses, armour check
//   penalty and NWN's blindness -4 are dropped. Rows without a 2024 skill
//   (Concentration, Discipline, Parry, Taunt, Appraise, Use Magic Device,
//   Craft Trap) keep NWN's value; "base ranks only" asks (level-up, the sheet
//   message, scripts asking for base ranks) are NWN's.
// - Characters (player characters, henchmen) keep their proficiencies and
//   Expertise in local variables (saved with the game), set with `!skills` and
//   checked against their 2024 choices; until set, they are taken from the
//   skills they have the most ranks in, within those choices. Other creatures
//   are proficient where they have ranks.
// - SK3 (a): a skill rank a script asks for (GetSkillRank: the official
//   campaigns' AutoDC conversation checks) is the 2024 value scaled to the 3e
//   range (2 x), since those DCs are set for 3e totals.
// A character's background skills (CM5, rules_abilities.cpp) join its
// profile; SK1's 2024 names come with the label hook.

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "creature_update.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "records.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"
#include "rules/skills.h"
#include "skill_rank.h"

rules::SkillSet BackgroundSkills(void* creature);  // rules_abilities.cpp (CM5)

namespace {

using TotalEffectBonusFn = int (*)(void* creature, uint8_t type, void* vs, int a3, int a4, uint8_t a5, uint8_t a6,
                                   uint8_t skill, uint8_t a8, int a9);

TotalEffectBonusFn g_total_effect_bonus = nullptr;
uintptr_t g_script_caller = 0;  // the script command's GetSkillRank call (SK3 a)

constexpr const char* kVarProficient = "nwn5e_skills";
constexpr const char* kVarExpertise = "nwn5e_skills_expertise";
constexpr const char* kVarSet = "nwn5e_skills_set";  // 1 chosen with !skills

struct Profile {
  rules::SkillSet proficient = 0;
  rules::SkillSet expertise = 0;
};
// Characters, by object id: chosen with !skills, or taken from the ranks
// fingerprinted by `ranks` — creatures are asked for skill values while a
// game loads, before their ranks are all in, so a profile from ranks is worked
// out again when they change.
struct CachedProfile {
  Profile profile;
  bool chosen = false;
  uint32_t ranks = 0;
  bool told = false;
};
std::unordered_map<uint32_t, CachedProfile> g_profiles;  // the engine's thread only
LogOnce g_logged(400);           // other creatures
LogOnce g_logged_players(1000);  // player characters

bool Faithful() { return g_config.skills == rules::RuleVariant::kFaithful; }

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

std::vector<rules::ClassLevels> ClassLevelsOf(void* stats) {
  std::vector<rules::ClassLevels> classes;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (const auto c = rules::ClassFromNwnId(slot.nwn_class_id)) classes.push_back({*c, slot.levels});
  }
  return classes;
}

rules::Species2024 SpeciesOf(void* stats) {
  switch (engine::RaceOf(stats)) {
    case ee::kRaceHuman:
      return rules::Species2024::kHuman;
    case ee::kRaceElf:
      return rules::Species2024::kElf;
    default:
      return rules::Species2024::kOther;  // other NWN species: the character model (E4)
  }
}

// The Skilled feat (an Origin feat): 3 more skills of any kind.
int SkilledFeats(void* stats) {
  int count = engine::HasFeat(stats, rules::kFeatSkilled) ? 1 : 0;
  for (int i = 0; i < rules::kSkilledRepeats; ++i) count += engine::HasFeat(stats, rules::kFeatSkilledAgainFirst + i) ? 1 : 0;
  return count;  // C7a: Skilled is Repeatable
}

const int8_t* Ranks(void* stats) { return Field<const int8_t*>(stats, ee::kStatsSkillRanks); }

// The most NWN ranks among a 2024 skill's rows.
int RanksFor(void* stats, rules::Skill2024 skill) {
  const int8_t* ranks = Ranks(stats);
  if (!ranks) return 0;
  int most = 0;
  const int rows = std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount());  // the rank array's length
  for (int row = 0; row < rows; ++row) {
    if (rules::Skill2024ForNwnSkill(row) == skill) most = std::max<int>(most, ranks[row]);
  }
  return most;
}

// From ranks: the most-ranked skills that fit the 2024 choices, then Expertise
// in the most-ranked of those.
Profile ProfileFromRanks(void* stats, const rules::SkillAllowance& allowance) {
  std::vector<std::pair<int, int>> by_rank;  // (ranks, skill)
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    const int ranks = RanksFor(stats, static_cast<rules::Skill2024>(i));
    if (ranks > 0) by_rank.push_back({ranks, i});
  }
  std::stable_sort(by_rank.begin(), by_rank.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  Profile profile;
  profile.proficient = allowance.granted;
  for (const auto& entry : by_rank) {
    const rules::SkillSet tried = profile.proficient | (1u << entry.second);
    if (rules::CheckSkillChoice(allowance, tried).empty()) profile.proficient = tried;
  }
  for (const auto& entry : by_rank) {
    const rules::SkillSet tried = profile.expertise | (1u << entry.second);
    if (rules::CheckExpertiseChoice(allowance, profile.proficient, tried).empty()) profile.expertise = tried;
  }
  return profile;
}

std::string Names(rules::SkillSet set) {
  std::string names;
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    if (!(set & (1u << i))) continue;
    if (!names.empty()) names += ", ";
    names += rules::Skill2024Name(static_cast<rules::Skill2024>(i));
  }
  return names.empty() ? "none" : names;
}

// A fingerprint of the creature's skill ranks (FNV-1a over the rank bytes).
uint32_t RankFingerprint(void* stats) {
  const int8_t* ranks = Ranks(stats);
  if (!ranks) return 0;
  uint32_t hash = 2166136261u;
  const int rows = std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount());
  for (int row = 0; row < rows; ++row) hash = (hash ^ static_cast<uint8_t>(ranks[row])) * 16777619u;
  return hash;
}

void StoreProfile(void* creature, uint32_t id, const Profile& profile) {
  SetRecord(creature, kVarProficient, static_cast<int>(profile.proficient));
  SetRecord(creature, kVarExpertise, static_cast<int>(profile.expertise));
  SetRecord(creature, kVarSet, 1);
  CachedProfile& cached = g_profiles[id];
  cached.profile = profile;
  cached.chosen = true;
}

// A character's proficiencies: chosen with !skills (its local variables), or
// else taken from its ranks — worked out again whenever the ranks change, never
// stored; the player is told once.
Profile CharacterProfile(void* creature, void* stats, bool is_player) {
  const uint32_t id = IdOf(creature);
  CachedProfile& cached = g_profiles[id];
  const uint32_t ranks = RankFingerprint(stats);
  if (cached.chosen && cached.ranks == ranks && ranks != 0) return cached.profile;
  EnsureRecords(creature);  // C6: an exported character's profile, from its database
  if (engine::GetLocalInt(creature, kVarSet) == 1) {  // chosen (its variables may load after its ranks)
    Profile chosen;
    chosen.proficient = static_cast<rules::SkillSet>(engine::GetLocalInt(creature, kVarProficient));
    chosen.expertise = static_cast<rules::SkillSet>(engine::GetLocalInt(creature, kVarExpertise));
    // C7b: what the Skills page added at a level-up (2024 ranks) joins what
    // !skills chose, as long as it fits the character's choices.
    const int8_t* rank_array = Ranks(stats);
    if (rank_array && rules::RanksAre2024(rank_array, std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount()))) {
      const rules::SkillAllowance allowance =
          rules::SkillAllowanceFor(ClassLevelsOf(stats), SpeciesOf(stats), SkilledFeats(stats));
      const Profile ranked = ProfileFromRanks(stats, allowance);
      for (int i = 0; i < rules::kSkill2024Count; ++i) {
        const rules::SkillSet bit = 1u << i;
        if ((ranked.proficient & bit) && !(chosen.proficient & bit) &&
            rules::CheckSkillChoice(allowance, chosen.proficient | bit).empty()) {
          chosen.proficient |= bit;
        }
      }
      for (int i = 0; i < rules::kSkill2024Count; ++i) {
        const rules::SkillSet bit = 1u << i;
        if ((ranked.expertise & bit) && !(chosen.expertise & bit) &&
            rules::CheckExpertiseChoice(allowance, chosen.proficient, chosen.expertise | bit).empty()) {
          chosen.expertise |= bit;
        }
      }
    }
    cached.profile = chosen;
    cached.chosen = true;
    cached.ranks = ranks;
    return cached.profile;
  }
  if (cached.ranks != ranks || ranks == 0) {
    cached.profile = ProfileFromRanks(stats, rules::SkillAllowanceFor(ClassLevelsOf(stats), SpeciesOf(stats),
                                                                      SkilledFeats(stats)));
    cached.ranks = ranks;
  }
  if (!cached.told && is_player) {
    cached.told = true;
    Log("skills: 0x%08X 2024 proficiencies taken from its ranks: %s; Expertise: %s", id,
        Names(cached.profile.proficient).c_str(), Names(cached.profile.expertise).c_str());
    TellPlayer(id, "nwn5e: your 2024 skills were taken from your ranks - proficient: " +
                       Names(cached.profile.proficient) + "; Expertise: " + Names(cached.profile.expertise) +
                       ". Change them with !skills.");
  }
  return cached.profile;
}

struct SkillParts {
  int ability = 0;
  int proficiency_bonus = 0;
  bool proficient = false;
  bool expertise = false;
  int effects = 0;
};

// The 2024 value of `skill` for `stats`, or false when NWN's stands.
bool SkillValue2024(void* stats, int skill, void* vs, int* value, SkillParts* parts, bool* is_player) {
  if (engine::IsDungeonMaster(stats)) return false;
  const auto mapped = rules::Skill2024ForNwnSkill(skill);
  void* creature = CreatureOf(stats);
  if (!mapped || !creature) return false;
  // Not while the creature is being built (loading a game or module): its
  // ranks are still being filled in. NWN's value until it is in an area.
  if (!engine::AreaOf(creature) || IdOf(creature) == ee::kObjectInvalid) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return false;
  *is_player = facts.is_player;
  const bool character = CountsAsCharacter(stats, facts);
  Profile profile;
  if (character) {
    profile = CharacterProfile(creature, stats, facts.is_player);
    profile.proficient |= BackgroundSkills(creature);  // CM5
  } else if (RanksFor(stats, *mapped) > 0) {
    profile.proficient = rules::SkillBit(*mapped);  // monsters: proficient where they have ranks
  }
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const int effects =
      g_total_effect_bonus ? g_total_effect_bonus(creature, ee::kEffectBonusTypeSkill, vs, 0, 0, 0, 0,
                                                  static_cast<uint8_t>(skill), 0xFF, 0)
                           : 0;
  parts->ability = engine::AbilityModifier(stats, static_cast<int>(rules::AbilityFor(*mapped)));
  parts->proficiency_bonus = pb;
  parts->proficient = rules::Has(profile.proficient, *mapped);
  parts->expertise = rules::Has(profile.expertise, *mapped);
  parts->effects = effects;
  *value = std::clamp(rules::SkillBonus2024(parts->ability, pb, parts->proficient, parts->expertise, effects), -127,
                      127);
  return true;
}

const char* AbilityShort(rules::Ability ability) {
  static const char* const kNames[] = {"Str", "Dex", "Con", "Int", "Wis", "Cha"};
  return kNames[static_cast<int>(ability)];
}

int SkillValue(const SkillQuery& query, int engine_value) {
  int value = 0;
  SkillParts parts;
  bool is_player = false;
  if (query.base_only || !SkillValue2024(query.stats, query.skill, query.vs, &value, &parts, &is_player)) {
    return engine_value;
  }
  if (g_config.log_attacks) {
    const uint32_t id = ObjectIdOf(query.stats);
    // Player characters have their own budget, so a town full of NPCs can't
    // use it up before the player's values are logged.
    LogOnce& logged = is_player ? g_logged_players : g_logged;
    if (logged.First(LogOnce::Key(id, query.skill, value))) {
      const auto mapped = rules::Skill2024ForNwnSkill(query.skill);
      Log("skills: 0x%08X %s: 3e %+d -> 2024 %+d (%s %+d, PB %+d %s, effects %+d, vs 0x%08X)", id,
          rules::Faithful2024SkillLabel(query.skill), engine_value, value, AbilityShort(rules::AbilityFor(*mapped)),
          parts.ability, parts.proficiency_bonus,
          parts.expertise ? "x2 (Expertise)" : parts.proficient ? "(proficient)" : "(not proficient)", parts.effects,
          query.vs ? IdOf(query.vs) : ee::kObjectInvalid);
    }
  }
  return value;
}

// SK3 (a): the script command's GetSkillRank, on the 3e scale.
int ScriptScale(const SkillQuery& query, int value) {
  if (query.caller != g_script_caller || query.base_only || !rules::Skill2024ForNwnSkill(query.skill) ||
      engine::IsDungeonMaster(query.stats)) {
    return value;
  }
  return rules::ScriptSkillValue2024(value);
}

// `!skills [expertise] [skill ...]`.
std::vector<std::string> SkillsCommand(void* creature, const std::string& args) {
  std::vector<std::string> lines;
  if (!Faithful()) {
    lines.push_back("nwn5e: skills follow NWN's rules (skills=nwn); nothing to choose.");
    return lines;
  }
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return lines;
  const uint32_t id = IdOf(creature);
  const rules::SkillAllowance allowance =
      rules::SkillAllowanceFor(ClassLevelsOf(stats), SpeciesOf(stats), SkilledFeats(stats));
  Profile profile = CharacterProfile(creature, stats, true);
  std::string rest = args;
  const bool expertise = rest.size() >= 9 && _strnicmp(rest.c_str(), "expertise", 9) == 0;
  if (expertise) rest = rest.substr(9);
  if (rest.find_first_not_of(' ') == std::string::npos) {
    if (expertise) {
      lines.push_back("nwn5e: Expertise (" + std::to_string(rules::CountSkills(profile.expertise)) + " of " +
                      std::to_string(allowance.expertise) + "): " + Names(profile.expertise) +
                      ". Set with !skills expertise <skill> ...");
      return lines;
    }
    lines.push_back("nwn5e: 2024 skills - proficient: " + Names(profile.proficient) + "; Expertise: " +
                    Names(profile.expertise) + ".");
    lines.push_back("nwn5e: you choose " + std::to_string(allowance.class_skills) + " from your class (" +
                    Names(allowance.class_list) + ")" +
                    (allowance.multiclass_skills ? ", " + std::to_string(allowance.multiclass_skills) +
                                                       " from your other classes" : std::string()) +
                    ", " + std::to_string(allowance.background_skills) + " more and 1 tool (background)" +
                    (allowance.species_any_skill ? ", 1 more (Human)" : std::string()) +
                    (allowance.species_list ? ", 1 of Insight, Perception or Survival (Elf)" : std::string()) +
                    (allowance.feat_skills ? ", " + std::to_string(allowance.feat_skills) + " more (Skilled)"
                                           : std::string()) +
                    (allowance.granted ? "; always: " + Names(allowance.granted) : std::string()) +
                    (allowance.expertise ? "; Expertise in " + std::to_string(allowance.expertise) : std::string()) +
                    ".");
    lines.push_back("nwn5e: set them with !skills <skill> <skill> ... (replaces the list), Expertise with "
                    "!skills expertise <skill> ...");
    return lines;
  }
  std::vector<rules::Skill2024> chosen;
  std::string unknown;
  if (!rules::ParseSkill2024List(rest, &chosen, &unknown)) {
    lines.push_back("nwn5e: unknown skill \"" + unknown + "\" (2024 names: Acrobatics, Animal Handling, Arcana, "
                    "Athletics, Deception, History, Insight, Intimidation, Investigation, Medicine, Nature, "
                    "Perception, Performance, Persuasion, Religion, Sleight of Hand, Stealth, Survival, Thieves' "
                    "Tools, Smith's Tools).");
    return lines;
  }
  rules::SkillSet set = 0;
  for (rules::Skill2024 skill : chosen) set |= rules::SkillBit(skill);
  if (expertise) {
    const std::string problem = rules::CheckExpertiseChoice(allowance, profile.proficient, set);
    if (!problem.empty()) {
      lines.push_back("nwn5e: Expertise not changed - " + problem + ".");
      return lines;
    }
    profile.expertise = set;
  } else {
    set |= allowance.granted;
    const std::string problem = rules::CheckSkillChoice(allowance, set);
    if (!problem.empty()) {
      lines.push_back("nwn5e: skills not changed - " + problem + ".");
      return lines;
    }
    profile.proficient = set;
    profile.expertise &= set;
  }
  StoreProfile(creature, id, profile);
  Log("skills: 0x%08X chose - proficient: %s; Expertise: %s", id, Names(profile.proficient).c_str(),
      Names(profile.expertise).c_str());
  lines.push_back("nwn5e: 2024 skills - proficient: " + Names(profile.proficient) + "; Expertise: " +
                  Names(profile.expertise) + ".");
  return lines;
}

// --- C8 (docs/CHARGEN.md): Skilled on the server ------------------------------------

// The server's level-up check (ValidateLevelUp) prices each new rank in NWN
// skill points against NWN's budget for the level - banked points (+0xDC6) +
// the class's base + Int, a cross-class rank twice - and refuses more
// ("Character applied invalid number of skillpoints", 66155): Skilled's three
// picks, a Performance pick, are often more than NWN gives (a Fighter with Int
// 8 at 4: 3 against 1). A player character whose ranks are the 2024 record
// gets the budget topped up for the call; the client's Skills page keeps the
// 2024 count, and the check's other parts (unusable skills, ranks, feats, hit
// points) all run. On success LevelUp banks the client's points left anyway.
using ValidateLevelUpFn = uint32_t (*)(void* stats, void* level, uint8_t domain1, uint8_t domain2, uint8_t school);
ValidateLevelUpFn g_validate_level_up = nullptr;

bool RanksAre2024(void* stats) {
  const int8_t* ranks = Ranks(stats);
  return ranks && rules::RanksAre2024(ranks, std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount()));
}

uint32_t ValidateLevelUp(void* stats, void* level, uint8_t domain1, uint8_t domain2, uint8_t school) {
  if (!stats || Field<int32_t>(stats, ee::kStatsIsPlayer) == 0 || !RanksAre2024(stats)) {
    return g_validate_level_up(stats, level, domain1, domain2, school);
  }
  auto* banked = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(stats) + ee::kStatsBankedSkillPoints);
  const uint16_t kept = *banked;
  *banked = 0xff;  // the engine's cost is an 8-bit sum
  const uint32_t refused = g_validate_level_up(stats, level, domain1, domain2, school);
  if (refused) {
    *banked = kept;
    Log("skills: 0x%08X level-up refused by the game (string %u) - not for its skill points", ObjectIdOf(stats),
        refused);
  }
  return refused;
}

// Skilled taken without the Skills page (a build's Recommended, the level-up's
// Recommended): its picks made as Recommended would - the class's usual
// skills, as server ranks - and the player told; !skills changes them.
// Characters whose skills are NWN ranks or chosen with !skills keep them.
using SetSkillRankFn = void (*)(void* stats, uint8_t skill, int8_t rank);
SetSkillRankFn g_set_skill_rank = nullptr;
std::unordered_map<uint32_t, uint32_t> g_skilled_seen;  // id -> the ranks and Skilled count last looked at

void FillSkilledPicks(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats || Field<int32_t>(stats, ee::kStatsIsPlayer) == 0) return;
  if (!engine::AreaOf(creature) || IdOf(creature) == ee::kObjectInvalid) return;  // still being built
  const int skilled = SkilledFeats(stats);
  if (skilled == 0) return;
  const uint32_t id = IdOf(creature);
  const uint32_t seen = RankFingerprint(stats) ^ (static_cast<uint32_t>(skilled) * 2654435761u);
  uint32_t& last = g_skilled_seen[id];
  if (last == seen) return;
  last = seen;
  if (!RanksAre2024(stats)) return;
  EnsureRecords(creature);
  if (engine::GetLocalInt(creature, kVarSet) == 1) return;  // chosen with !skills
  const std::vector<rules::ClassLevels> classes = ClassLevelsOf(stats);
  if (classes.empty()) return;
  const rules::SkillAllowance allowance = rules::SkillAllowanceFor(classes, SpeciesOf(stats), skilled);
  rules::SkillSet proficient = 0;
  const int8_t* ranks = Ranks(stats);
  const int rows = std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount());
  for (int row = 0; row < rows; ++row) {
    if (ranks[row] >= 1 && rules::IsSkillsPageRow(row)) proficient |= rules::SkillBit(*rules::Skill2024ForNwnSkill(row));
  }
  const std::vector<rules::Skill2024> filled = rules::FillSkillChoices(
      allowance, proficient, rules::SkillPreference2024(classes.front().character_class), 3 * skilled);
  if (filled.empty()) return;
  rules::SkillSet added = 0;
  for (rules::Skill2024 skill : filled) {
    const int row = rules::SkillsPageRow(skill);
    if (row < 0 || row >= rows) continue;
    g_set_skill_rank(stats, static_cast<uint8_t>(row), 1);
    added |= rules::SkillBit(skill);
  }
  last = RankFingerprint(stats) ^ (static_cast<uint32_t>(skilled) * 2654435761u);
  Log("skills: 0x%08X Skilled's picks made for it (not chosen on the Skills page): %s", id, Names(added).c_str());
  TellPlayer(id, "nwn5e: Skilled - its skills weren't chosen on the Skills page, so you have the usual ones: " +
                     Names(added) + ". Change them with !skills.");
}

}  // namespace

void InstallSkillRules() {
  if (!Faithful()) return;
  RegisterRecords({kVarProficient, kVarExpertise, kVarSet});
  g_total_effect_bonus = reinterpret_cast<TotalEffectBonusFn>(EngineExport(ee::kGetTotalEffectBonus));
  if (!g_total_effect_bonus) Log("skills: GetTotalEffectBonus missing; 2024 skills have no item / spell bonuses");
  if (!AddSkillAdjuster(&SkillValue)) return;
  g_script_caller = SkillRankCallerIn(ee::kExecuteCommandGetSkillRank);
  if (g_script_caller) AddSkillAdjuster(&ScriptScale);
  AddChatCommand("!skills", &SkillsCommand);
  // C8: Skilled's picks - the server's level-up check, and Skilled taken without the page.
  HookExport("CNWSCreatureStats::ValidateLevelUp (C8: the 2024 picks over NWN's skill points)", ee::kValidateLevelUp,
             reinterpret_cast<void*>(&ValidateLevelUp), reinterpret_cast<void**>(&g_validate_level_up));
  g_set_skill_rank = reinterpret_cast<SetSkillRankFn>(EngineExport(ee::kStatsSetSkillRank));
  if (g_set_skill_rank) {
    AddCreatureUpdateListener(&FillSkilledPicks);
  } else {
    Log("SKIP skills: Skilled taken without the Skills page - no export %s", ee::kStatsSetSkillRank);
  }
}
