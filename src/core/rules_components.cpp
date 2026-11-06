// #38 costly material components, paid in gold (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "38, rituals A, components
// B for now, we'll need to check the meterial costs", then "for now keep the gp cost. we'll see how it works on a
// playthrough"), `spellcasting` faithful (#36's key). The costs: rules::CostlyComponent (SRD 5.2.1's).
// - The order (action_guard.h's AddSpellOrderGuard): a cast from a class slot (0-7 - not a spell-like ability, a
//   script's cast or one that only plays) of a spell with a costly component. Who pays: the caster if it's a player
//   character, else its master if that one is (a henchman, a familiar, a summon); every other caster (enemies,
//   NPCs) pays nothing. The payer must have the whole cost on hand, or the order is refused (told). Each order is
//   checked against the gold at that moment (several queued at once can ask for more than there is).
// - The cast taking effect (spell_impact.h): a cast let through above, not from an item (a scroll's, a wand's or a
//   potion's spell needs no component), takes the consumed part from the payer (TakeGoldFromCreature: NWN says
//   "Lost N GP"), told.
// NWN mode: free (NWN has no components).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "action_guard.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "spell_impact.h"
#include "rules/material_components.h"

namespace {

constexpr const char* kVarGold = "nwn5e_component_gold";
constexpr const char* kVarPayer = "nwn5e_component_payer";
constexpr ULONGLONG kPendingMs = 5 * 60 * 1000;  // a let-through cast that never took effect is forgotten
constexpr ULONGLONG kToldEveryMs = 10 * 1000;    // a refusal told at most this often (a henchman's AI retries)

struct Pending {
  int spell = -1;
  uint32_t payer = 0;
  ULONGLONG at = 0;
};

std::unordered_map<uint32_t, std::vector<Pending>> g_pending;  // by caster
std::unordered_map<uint64_t, ULONGLONG> g_told;                // (caster, spell) -> when last refused aloud
int g_logged = 0;

bool Faithful() { return g_config.spellcasting == rules::RuleVariant::kFaithful; }

uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }

std::string Gp(int gold) { return std::to_string(gold) + " GP"; }

// Who pays for `creature`'s components (the script bridge: itself if a player character, else its master if that
// one is), with that one's gold; kObjectInvalid: nobody - the caster pays nothing.
uint32_t PayerOf(void* creature, int* gold) {
  const uint32_t id = IdOf(creature);
  char code[512];
  sprintf_s(code,
            "object p = OBJECT_SELF; if (!GetIsPC(p)) p = GetMaster(p); int g = -1; string s = \"\"; "
            "if (GetIsObjectValid(p) && GetIsPC(p)) { g = GetGold(p); s = ObjectToString(p); } "
            "SetLocalInt(OBJECT_SELF, \"%s\", g); SetLocalString(OBJECT_SELF, \"%s\", s);",
            kVarGold, kVarPayer);
  RunScriptChunk(code, id);
  *gold = engine::GetLocalInt(creature, kVarGold);
  const std::string payer = engine::GetLocalString(creature, kVarPayer);
  if (*gold < 0 || payer.empty()) return ee::kObjectInvalid;
  return static_cast<uint32_t>(strtoul(payer.c_str(), nullptr, 16));
}

std::string CostWords(const rules::ComponentCost& cost) {
  if (cost.consumed == 0) return "a material component worth " + Gp(cost.needed) + " (kept)";
  if (cost.consumed == cost.needed) return "a material component worth " + Gp(cost.needed) + ", used up by the spell";
  return "material components worth " + Gp(cost.needed) + " (" + Gp(cost.consumed) + " of it used up by the spell)";
}

// The order: refused (true) when the payer hasn't the cost on hand.
bool OrderGuard(void* creature, const SpellOrder& order) {
  if (order.fake || order.class_slot < 0 || order.class_slot > 7) return false;
  const int spell = static_cast<int>(order.spell);
  const rules::ComponentCost cost = rules::CostlyComponent(spell);
  if (cost.needed <= 0) return false;
  int gold = 0;
  const uint32_t payer = PayerOf(creature, &gold);
  if (payer == ee::kObjectInvalid) return false;  // an enemy, an NPC: nothing to pay
  const uint32_t id = IdOf(creature);
  const ULONGLONG now = GetTickCount64();
  if (!rules::ComponentAffordable(gold, cost)) {
    ULONGLONG& told = g_told[(static_cast<uint64_t>(id) << 32) | static_cast<uint32_t>(spell)];
    if (now >= told + kToldEveryMs) {
      told = now;
      TellPlayer(payer, "nwn5e: " + SpellNameOf(spell) + " needs " + CostWords(cost) + " - you have " + Gp(gold) + ".");
    }
    Log("components: 0x%08X %s refused - %d GP needed, the payer 0x%08X has %d", id, SpellNameOf(spell).c_str(),
        cost.needed, payer, gold);
    return true;
  }
  std::vector<Pending>& pending = g_pending[id];
  pending.push_back({spell, payer, now});
  return false;
}

// The cast taking effect: a let-through order's consumed part paid.
void Impact(void* creature, int spell) {
  const uint32_t id = IdOf(creature);
  auto found = g_pending.find(id);
  if (found == g_pending.end()) return;
  std::vector<Pending>& pending = found->second;
  const ULONGLONG now = GetTickCount64();
  pending.erase(std::remove_if(pending.begin(), pending.end(),
                               [now](const Pending& p) { return now > p.at + kPendingMs; }),
                pending.end());
  auto at = std::find_if(pending.begin(), pending.end(), [spell](const Pending& p) { return p.spell == spell; });
  if (at == pending.end()) return;  // not an order checked above (a script's, an item's)
  const Pending paid = *at;
  pending.erase(at);
  if (SpellCastItemBase(creature) != -1) return;  // from an item: its own component
  const rules::ComponentCost cost = rules::CostlyComponent(spell);
  if (cost.consumed <= 0) return;
  char code[96];
  sprintf_s(code, "TakeGoldFromCreature(%d, OBJECT_SELF, TRUE);", cost.consumed);
  RunScriptChunk(code, paid.payer);
  TellPlayer(paid.payer, "nwn5e: " + SpellNameOf(spell) + " used up its material component - " + Gp(cost.consumed) + ".");
  if (g_logged < 40) {
    ++g_logged;
    Log("components: 0x%08X %s - %d GP used up (the payer 0x%08X)", id, SpellNameOf(spell).c_str(), cost.consumed,
        paid.payer);
  }
}

void GameStarted() {
  g_pending.clear();
  g_told.clear();
}

}  // namespace

void InstallComponents() {
  if (!Faithful()) {
    Log("components: NWN's - spells need no material components (key spellcasting)");
    return;
  }
  const bool hooked = AddSpellOrderGuard(&OrderGuard) && AddSpellImpactListener(rules::CostlySpells(), &Impact) &&
                      AddGameStartListener(&GameStarted);
  Log(hooked ? "components: SRD 5.2's costly material components in gold (%zu of NWN's spells) - the cost on hand to "
               "cast, the consumed part taken; a player character's casts and its associates' (#38)"
             : "SKIP components #38: not hooked - spells need no material components (%zu)",
      rules::CostlySpells().size());
}
