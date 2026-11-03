// X8's second half (docs/PLAN.md "Phase 3 — Barbarian", X8: "a 'Combat
// options' NUI window (every class choice with descriptions and checkboxes, no
// five-option limit) when Cunning Strike comes"; Rogue R6, 0.50.0), class_features
// faithful. !options - or "Combat options" on Cunning Strike's and Devious
// Strikes' radial menus - opens a window with a checkbox per standing choice
// the character has: Reckless Attack (Barbarian 2), Brutal Strike's effects (9;
// 13; two at 17), Cunning Strike's (Rogue 5; two at 11; Devious Strikes' at
// 14), Open Hand Technique's (Monk 3, 0.60.0: one of three, or none). A click is
// the same pick the radial, the quickbar and the chat commands
// make (with their chat line), and the window follows every pick, wherever it
// was made (a second effect past the limit drops the older one).

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "log.h"
#include "nui.h"
#include "extra_actions.h"
#include "reactions.h"
#include "script_hooks.h"
#include "rules/class_features.h"

// rules_barbarian_strikes.cpp
int BrutalStrikeBarbarianLevel(void* creature);
bool RecklessAttackOn(void* creature);
void SetRecklessAttack(void* creature, bool on);
rules::BrutalChoice BrutalStrikeChoice(void* creature);
void PickBrutalStrike(void* creature, rules::BrutalEffect effect);
// rules_cunning_strike.cpp
int CunningStrikeRogueLevel(void* creature);
rules::CunningChoice CunningStrikeChoice(void* creature);
int CunningStrikeDcOf(void* creature);
void PickCunningStrike(void* creature, rules::CunningEffect effect);
// bonus_actions.cpp (Phase 6, #52)
bool ActionEconomyOn();
int DefaultBonusKey(void* creature);
void SetDefaultBonusKey(void* creature, int key);
bool HasBonusFamily(void* creature, int family);
// rules_monk.cpp
int OpenHandMonkLevel(void* creature);
rules::OpenHandTechnique OpenHandTechniqueOf(void* creature);
int MonkFocusDc(void* creature);
void PickOpenHandTechnique(void* creature, rules::OpenHandTechnique technique);

namespace {

constexpr const char* kWindow = "n5opts";
constexpr const char* kStatus = "o_status";
constexpr float kRowHeight = 28.0f;
constexpr float kCheckWidth = 190.0f;

enum class Kind { kBonus, kReaction, kAutomatic, kReckless, kBrutal, kCunning, kOpenHand };

struct Option {
  const char* bind;  // the checkbox's bool bind (and its id)
  Kind kind;
  int effect;  // rules::BrutalEffect / rules::CunningEffect
  const char* label;
  const char* text;
};

const Option kOptions[] = {
    // 0.61.1: the default Bonus Action - one of these, or none (all off).
    {"o_ba_attack", Kind::kBonus, 0, "Bonus attack", "your off-hand attack, Martial Arts' strike or Flurry of Blows"},
    {"o_ba_secwind", Kind::kBonus, 1220, "Second Wind", "heals you (not at full hit points); a use a round"},
    {"o_ba_rage", Kind::kBonus, 293, "Rage", "starts a Rage (not while raging); a use"},
    {"o_ba_intim", Kind::kBonus, 1314, "Intimidating Presence", "frightens the enemies around you; a use"},
    {"o_ba_patient", Kind::kBonus, 1317, "Patient Defense", "Disengage and Dodge; a Focus point a round"},
    {"o_ba_step", Kind::kBonus, 1318, "Step of the Wind", "Disengage and Dash; a Focus point a round"},
    {"o_ba_whole", Kind::kBonus, 211, "Wholeness of Body", "heals you (not at full hit points); a use"},
    // #53: the Rogue's.
    {"o_ba_cadash", Kind::kBonus, 1326, "Cunning Action: Dash", "twice as fast this round"},
    {"o_ba_cadis", Kind::kBonus, 1327, "Cunning Action: Disengage", "no opportunity attacks as you move this round"},
    {"o_ba_cahide", Kind::kBonus, 1328, "Cunning Action: Hide", "into stealth mode (not while in it)"},
    {"o_ba_steady", Kind::kBonus, 1329, "Steady Aim", "Advantage on your next attack; you can't move this round"},
    // #53 (owner: "a lot of these automatic actions should be managed by the user"): automatic ones, switchable.
    {"o_au_thief", Kind::kAutomatic, 1, "Thief's Reflexes", "a fight's first round: your Attack action twice"},
    // #54 (owner, 2026-11-02): the reactions, each switchable; effect = rules::Reaction.
    {"o_re_aoo", Kind::kReaction, 0, "Opportunity attacks", "an attack at an enemy leaving your reach"},
    {"o_re_dodge", Kind::kReaction, 1, "Uncanny Dodge", "halves the first hit you see each round"},
    {"o_re_deflect", Kind::kReaction, 2, "Deflect Attacks",
     "less damage from the first weapon hit each round (any hit from Monk 13)"},
    {"o_re_redirect", Kind::kReaction, 4, "Deflect: redirect",
     "a hit taken to 0: a Focus point, its attacker saves (Dex) or takes 2 Martial Arts dice + Dex"},
    {"o_re_retal", Kind::kReaction, 3, "Retaliation", "an attack back at a creature in reach that damages you"},
    {"o_reckless", Kind::kReckless, 0, "Reckless Attack",
     "Advantage on your Strength attacks; attacks against you have Advantage until a round after your last"},
    {"o_bs_force", Kind::kBrutal, 1, "Forceful Blow", "pushed 15 ft straight away from you"},
    {"o_bs_ham", Kind::kBrutal, 2, "Hamstring Blow", "its speed -15 ft for a round"},
    {"o_bs_stag", Kind::kBrutal, 3, "Staggering Blow", "Disadvantage on its next save; no opportunity attacks for a round"},
    {"o_bs_sund", Kind::kBrutal, 4, "Sundering Blow", "+5 on the next attack roll another creature makes at it"},
    {"o_cs_poison", Kind::kCunning, 1, "Poison (1d6)", "Constitution save or Poisoned for a minute (saves again each round)"},
    {"o_cs_trip", Kind::kCunning, 2, "Trip (1d6)", "Large or smaller: Dexterity save or knocked prone for a round"},
    {"o_cs_wdraw", Kind::kCunning, 3, "Withdraw (1d6)", "you move away this round without provoking opportunity attacks"},
    {"o_cs_daze", Kind::kCunning, 4, "Daze (2d6)", "Constitution save or Slowed for a round"},
    {"o_cs_ko", Kind::kCunning, 5, "Knock Out (6d6)", "Constitution save or asleep for a minute, until it takes damage"},
    {"o_cs_obsc", Kind::kCunning, 6, "Obscure (3d6)", "Dexterity save or Blinded for a round"},
    {"o_oh_topple", Kind::kOpenHand, 1, "Topple", "Dexterity save or knocked prone for a round"},
    {"o_oh_push", Kind::kOpenHand, 2, "Push", "Strength save or pushed 15 ft away from you"},
    {"o_oh_addle", Kind::kOpenHand, 3, "Addle", "no opportunity attacks until its next turn"},
};

bool g_syncing = false;  // the window's own bind updates: not the player's clicks

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }

bool Available(void* creature, const Option& o) {
  switch (o.kind) {
    case Kind::kBonus: return ActionEconomyOn() && (o.effect == 0 || HasBonusFamily(creature, o.effect));
    case Kind::kReaction: {  // #54: the ones the character has
      if (!ActionEconomyOn()) return false;
      switch (static_cast<rules::Reaction>(o.effect)) {
        case rules::Reaction::kOpportunityAttack: return true;
        case rules::Reaction::kUncannyDodge: return rules::HasUncannyDodge(CunningStrikeRogueLevel(creature));
        case rules::Reaction::kDeflectAttacks:
        case rules::Reaction::kDeflectRedirect: return rules::HasDeflectAttacks(OpenHandMonkLevel(creature));
        case rules::Reaction::kRetaliation: return rules::HasRetaliation(BrutalStrikeBarbarianLevel(creature));
      }
      return false;
    }
    case Kind::kAutomatic: return ActionEconomyOn() && rules::HasThiefsReflexes(CunningStrikeRogueLevel(creature));
    case Kind::kReckless: return rules::HasRecklessAttack(BrutalStrikeBarbarianLevel(creature));
    case Kind::kBrutal:
      return rules::BrutalEffectAvailable(static_cast<rules::BrutalEffect>(o.effect), BrutalStrikeBarbarianLevel(creature));
    case Kind::kCunning:
      return rules::CunningEffectAvailable(static_cast<rules::CunningEffect>(o.effect), CunningStrikeRogueLevel(creature));
    case Kind::kOpenHand: return rules::HasOpenHandTechnique(OpenHandMonkLevel(creature));
  }
  return false;
}

bool Checked(void* creature, const Option& o) {
  switch (o.kind) {
    case Kind::kBonus: return DefaultBonusKey(creature) == o.effect;
    case Kind::kReaction: return ReactionSwitchOn(creature, static_cast<rules::Reaction>(o.effect));
    case Kind::kAutomatic: return ThiefsReflexesOn(creature);
    case Kind::kReckless: return RecklessAttackOn(creature);
    case Kind::kBrutal: {
      for (rules::BrutalEffect e :
           rules::ActiveBrutalEffects(BrutalStrikeChoice(creature), BrutalStrikeBarbarianLevel(creature))) {
        if (static_cast<int>(e) == o.effect) return true;
      }
      return false;
    }
    case Kind::kCunning: {
      for (rules::CunningEffect e :
           rules::ActiveCunningEffects(CunningStrikeChoice(creature), CunningStrikeRogueLevel(creature))) {
        if (static_cast<int>(e) == o.effect) return true;
      }
      return false;
    }
    case Kind::kOpenHand: return static_cast<int>(OpenHandTechniqueOf(creature)) == o.effect;
  }
  return false;
}

// A pick toggles: a chosen effect is removed, another is chosen.
void Toggle(void* creature, const Option& o, bool on) {
  switch (o.kind) {
    case Kind::kBonus: SetDefaultBonusKey(creature, on ? o.effect : -1); break;  // one of them, or none
    case Kind::kReaction: SetReactionSwitch(creature, static_cast<rules::Reaction>(o.effect), on); break;
    case Kind::kAutomatic: SetThiefsReflexes(creature, on); break;
    case Kind::kReckless: SetRecklessAttack(creature, on); break;
    case Kind::kBrutal: PickBrutalStrike(creature, static_cast<rules::BrutalEffect>(o.effect)); break;
    case Kind::kCunning: PickCunningStrike(creature, static_cast<rules::CunningEffect>(o.effect)); break;
    case Kind::kOpenHand:  // one of three: a click on the chosen one turns the technique off
      PickOpenHandTechnique(creature, on ? static_cast<rules::OpenHandTechnique>(o.effect) : rules::OpenHandTechnique::kOff);
      break;
  }
}

std::string Status(void* creature) {
  std::string line;
  const int barbarian = BrutalStrikeBarbarianLevel(creature);
  if (rules::HasBrutalStrike(barbarian)) {
    std::string names;
    for (rules::BrutalEffect e : rules::ActiveBrutalEffects(BrutalStrikeChoice(creature), barbarian)) {
      names += (names.empty() ? "" : " + ") + std::string(rules::BrutalEffectName(e));
    }
    line += "Brutal Strike: " + (names.empty() ? std::string("off") : names) +
            (RecklessAttackOn(creature) ? "" : " (needs Reckless Attack on)");
  }
  const int rogue = CunningStrikeRogueLevel(creature);
  if (rules::HasCunningStrike(rogue)) {
    std::string names;
    int dice = 0;
    for (rules::CunningEffect e : rules::ActiveCunningEffects(CunningStrikeChoice(creature), rogue)) {
      names += (names.empty() ? "" : " + ") + std::string(rules::CunningEffectName(e));
      dice += rules::CunningEffectCost(e);
    }
    if (!line.empty()) line += ".  ";
    line += "Cunning Strike: " + (names.empty() ? std::string("off - full Sneak Attack dice")
                                                : names + " (" + std::to_string(dice) + "d6 of your " +
                                                      std::to_string(rules::SneakAttackDice2024(rogue)) + "d6)");
  }
  if (rules::HasOpenHandTechnique(OpenHandMonkLevel(creature))) {
    if (!line.empty()) line += ".  ";
    line += std::string("Open Hand Technique: ") + rules::OpenHandTechniqueName(OpenHandTechniqueOf(creature));
  }
  return line;
}

std::string Header(const std::string& text) { return nui::Sized(nui::Label(nui::Str(text)), 0.0f, 24.0f); }

std::string OptionRow(const Option& o) {
  return nui::Row(nui::List({nui::Sized(nui::Check(o.bind, o.label, o.bind), kCheckWidth, kRowHeight),
                             nui::Sized(nui::Label(nui::Str(o.text)), 0.0f, kRowHeight)}));
}

std::string Layout(void* creature, int* rows) {
  std::string children;
  auto add = [&children](const std::string& element) {
    if (!children.empty()) children += ",";
    children += element;
  };
  int count = 0;
  if (ActionEconomyOn()) {  // Phase 6 (#52)
    add(Header("Bonus Action - one a round. In a fight, at the start of each round in which yours is free:"));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kBonus || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  if (ActionEconomyOn()) {  // #54
    add(Header("Reactions - one a round, taken when its moment comes (one switched off leaves it for the others):"));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kReaction || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  bool automatic = false;  // #53
  for (const Option& o : kOptions) automatic = automatic || (o.kind == Kind::kAutomatic && Available(creature, o));
  if (automatic) {
    add(Header("Automatic actions - taken for you when their moment comes:"));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kAutomatic || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  const int barbarian = BrutalStrikeBarbarianLevel(creature);
  if (rules::HasRecklessAttack(barbarian)) {
    add(Header("Barbarian " + std::to_string(barbarian)));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kReckless || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  if (rules::HasBrutalStrike(barbarian)) {
    add(Header(std::string("Brutal Strike - reckless, your first Strength attack each round with Advantage gives it "
                           "up for +") +
               std::to_string(rules::BrutalStrikeDice(barbarian)) + "d10 and " +
               (rules::BrutalStrikeEffectCount(barbarian) > 1 ? "two effects:" : "one effect:")));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kBrutal || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  const int rogue = CunningStrikeRogueLevel(creature);
  if (rules::HasCunningStrike(rogue)) {
    add(Header("Cunning Strike (Rogue " + std::to_string(rogue) + ") - a Sneak Attack gives up dice for " +
               (rules::CunningStrikeEffectCount(rogue) > 1 ? "up to two effects" : "one effect") + "; save DC " +
               std::to_string(CunningStrikeDcOf(creature)) + ":"));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kCunning || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  const int monk = OpenHandMonkLevel(creature);
  if (rules::HasOpenHandTechnique(monk)) {
    add(Header("Open Hand Technique (Monk " + std::to_string(monk) + ") - each Flurry of Blows hit, one of these; "
               "save DC " + std::to_string(MonkFocusDc(creature)) + ":"));
    ++count;
  }
  for (const Option& o : kOptions) {
    if (o.kind != Kind::kOpenHand || !Available(creature, o)) continue;
    add(OptionRow(o));
    ++count;
  }
  add(nui::Sized(nui::Label(nui::Bind(kStatus)), 0.0f, 48.0f));
  add(nui::Row(nui::List({nui::Spacer(), nui::Button("o_close", nui::Str("Close"), 100.0f)})));
  if (rows) *rows = count;
  return nui::Col(children);
}

void Sync(uint32_t player, void* creature) {
  std::vector<std::pair<std::string, bool>> values;
  for (const Option& o : kOptions) {
    if (Available(creature, o)) values.emplace_back(o.bind, Checked(creature, o));
  }
  g_syncing = true;
  NuiSetBools(player, kWindow, values);
  NuiSetText(player, kWindow, kStatus, Status(creature));
  g_syncing = false;
}

bool HasAnyOption(void* creature) {
  for (const Option& o : kOptions) {
    if (Available(creature, o)) return true;
  }
  return false;
}

void Open(uint32_t player) {
  void* creature = CreatureById(player);
  if (!creature) return;
  if (!Faithful() || !HasAnyOption(creature)) {
    TellPlayer(player, "nwn5e: no combat configuration yet - the Bonus Action and reactions (actions faithful), "
                       "Reckless Attack "
                       "(Barbarian 2), Brutal Strike (Barbarian 9), Cunning Strike (Rogue 5), Open Hand Technique "
                       "(Monk 3).");
    return;
  }
  int rows = 0;
  const std::string root = Layout(creature, &rows);
  const float height = 90.0f + rows * (kRowHeight + 6.0f) + 60.0f;
  if (!NuiOpen(player, kWindow, nui::Window(root, nui::Str("Combat configuration"), 120.0f, 120.0f, 720.0f, height))) {
    return;
  }
  Sync(player, creature);
  std::vector<std::string> binds;
  for (const Option& o : kOptions) {
    if (Available(creature, o)) binds.push_back(o.bind);
  }
  NuiWatch(player, kWindow, binds);
}

void OnEvent(const NuiEvent& event) {
  if (event.type == "click" && event.element == "o_close") {
    NuiClose(event.player, kWindow);
    return;
  }
  if (event.type != "watch" || g_syncing) return;
  void* creature = CreatureById(event.player);
  if (!creature) return;
  for (const Option& o : kOptions) {
    if (event.element != o.bind) continue;
    const bool want = NuiGetBool(event.player, kWindow, o.bind);
    // Only a change the player made: the window's own updates match the state.
    if (!Available(creature, o) || want == Checked(creature, o)) return;
    Toggle(creature, o, want);
    Sync(event.player, creature);  // a second effect past the limit drops the older one
    return;
  }
}

void OpenScript(uint32_t self) { Open(self); }

std::vector<std::string> OptionsCommand(void* creature, const std::string&) {
  Open(Field<uint32_t>(creature, ee::kObjectId));
  return {};
}

}  // namespace

void RefreshCombatOptions(uint32_t player) {
  if (g_syncing || !NuiToken(player, kWindow)) return;
  if (void* creature = CreatureById(player)) Sync(player, creature);
}

void InstallCombatOptions() {
  if (!Faithful()) return;
  const bool window = AddNuiWindow(kWindow, &OnEvent);
  AddScriptHandler("n5_cmbopts", &OpenScript);
  AddChatCommand("!options", &OptionsCommand);
  Log("combat options: !options%s (Combat configuration: the Bonus Action, Reckless Attack, Brutal Strike, Cunning "
      "Strike, Open Hand Technique)",
      window ? "" : " NOT on (no NUI event hook)");
}
