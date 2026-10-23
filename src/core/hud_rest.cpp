// Phase 5a (owner, 2026-10-23: "For the short rest button I'd like the
// dedicated button in the hud"): a Short Rest button right of the Rest
// button. The buttons under the portrait (CPanelPartyBar, built in code from
// pnl_party_bar.mdl's hooks - no .gui) are a full 2 x 4 grid: Map /
// Inventory, Journal / Character, Rest / Spellbook, Options / Player List;
// the Player List button is dead in single player (disabled when the game
// isn't multiplayer). So, in single player only, the engine's own buttons
// move: Spellbook takes the Player List's place (the Spellbook art on its
// model, its tooltip and click), and the Spellbook's place becomes Short Rest
// (the Rest button's model and art for now, a "Short Rest" tooltip, nwn5e's
// click, the Rest button's "can't rest" feedback), greyed whenever Rest is.
// No new controls: nothing to destroy before the bar's own destructor. Done
// in the bar's per-frame update (its first runs after CGuiInGame::Initialize
// has bound the bar's clicks), once per bar - the marker is our click on the
// Spellbook's place. A click hands the Short Rest to the server side
// (rules_rest.cpp) for the player's next update.
// The icon: the owner's own art when nwn5e serves n5_but_srest.tga (64 x 64,
// the Rest button's three plates - normal, highlighted, disabled - in the
// same places; data\files, or the private layer's if it reuses the game's
// art), put on the button's model in place of PB_BUT_REST; else the Rest
// button's eye.

#include <windows.h>

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

void RequestShortRest();  // rules_rest.cpp

namespace {

constexpr uint32_t kStrShortRest = 16000438;

using UpdateFn = void (*)(void* bar, float seconds);
using SetModelFn = void (*)(void* control, const char* model);
using AddToPanelFn = void (*)(void* control, void* panel, const char* hook);
using SetEnabledFn = void (*)(void* button, int enabled);
using ReplaceTextureFn = int (*)(void* model, const char* old_texture, const char* new_texture);

UpdateFn g_update = nullptr;
SetModelFn g_set_model = nullptr;
AddToPanelFn g_add_to_panel = nullptr;
SetEnabledFn g_set_enabled = nullptr;
void** g_app_manager = nullptr;  // &g_pAppManager

uint8_t* At(void* bar, size_t offset) { return static_cast<uint8_t*>(bar) + offset; }

bool SinglePlayer() {
  void* app = g_app_manager ? *g_app_manager : nullptr;
  return app && Field<int32_t>(app, ee::kAppManagerMultiplayer) == 0;
}

void ShortRestClicked(void*) { RequestShortRest(); }

void ReplaceTexture(void* button, const char* old_texture, const char* new_texture) {
  void* model = Field<void*>(button, ee::kGuiObjectModel);
  if (!model) return;
  auto replace = reinterpret_cast<ReplaceTextureFn>((*reinterpret_cast<void***>(model))[ee::kGobVtableReplaceTexture]);
  replace(model, old_texture, new_texture);
}

void Convert(void* bar) {
  uint8_t* spell = At(bar, ee::kPartyBarSpellButton);
  uint8_t* player_list = At(bar, ee::kPartyBarPlayerListButton);
  // Spellbook into the Player List's place: its art, tooltip and click.
  void* spell_object = Field<void*>(spell, ee::kButtonClickObject);
  void* spell_function = Field<void*>(spell, ee::kButtonClickFunction);
  ReplaceTexture(player_list, "PB_BUT_PVP", "PB_BUT_SPELL");
  SetField<uint32_t>(player_list, ee::kButtonTooltip, ee::kStrSpellbook);
  SetField<void*>(player_list, ee::kButtonClickObject, spell_object);
  SetField<void*>(player_list, ee::kButtonClickFunction, spell_function);
  g_set_enabled(player_list, 1);
  // The Spellbook's place becomes Short Rest.
  g_set_model(spell, "PB_BUT_REST");
  g_add_to_panel(spell, bar, "SpellButton");
  const bool own_icon = ServedFileExists(L"n5_but_srest.tga") || ServedFileExists(L"n5_but_srest.dds");
  if (own_icon) ReplaceTexture(spell, "PB_BUT_REST", "N5_BUT_SREST");
  SetField<uint32_t>(spell, ee::kButtonTooltip, kStrShortRest);
  SetField<uint16_t>(spell, ee::kPartyBarButtonFeedback, ee::kFeedbackCantRest);
  SetField<void*>(spell, ee::kButtonClickObject, bar);
  SetField<void*>(spell, ee::kButtonClickFunction, reinterpret_cast<void*>(&ShortRestClicked));
  Log("hud: the Short Rest button is right of Rest (Spellbook moved to the Player List's place; icon: %s)",
      own_icon ? "n5_but_srest" : "the Rest button's");
}

void PartyBarUpdate(void* bar, float seconds) {
  g_update(bar, seconds);
  if (!bar || !SinglePlayer()) return;
  uint8_t* spell = At(bar, ee::kPartyBarSpellButton);
  if (Field<void*>(spell, ee::kButtonClickFunction) != reinterpret_cast<void*>(&ShortRestClicked)) {
    if (!Field<void*>(At(bar, ee::kPartyBarSpellButton), ee::kButtonClickFunction)) return;  // not bound yet
    Convert(bar);
  }
  // Spellbook stays live in its new place (the Player List's, greyed in single player).
  uint8_t* player_list = At(bar, ee::kPartyBarPlayerListButton);
  if (Field<uint32_t>(player_list, ee::kGuiControlFlags) & 1) g_set_enabled(player_list, 1);
  // Greyed whenever Rest is (a no-rest area; the server's "can't rest now").
  const bool rest_disabled = Field<uint32_t>(At(bar, ee::kPartyBarRestButton), ee::kGuiControlFlags) & 1;
  const bool ours_disabled = Field<uint32_t>(spell, ee::kGuiControlFlags) & 1;
  if (rest_disabled != ours_disabled) g_set_enabled(spell, rest_disabled ? 0 : 1);
}

}  // namespace

void InstallShortRestButton() {
  if (g_config.rests != rules::RuleVariant::kFaithful) return;
  g_set_model = reinterpret_cast<SetModelFn>(EngineExport(ee::kGuiObjectSetModel));
  g_add_to_panel = reinterpret_cast<AddToPanelFn>(EngineExport(ee::kGuiObjectAddToPanel));
  g_set_enabled = reinterpret_cast<SetEnabledFn>(EngineExport(ee::kGuiButtonSetEnabled));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManagerGlobal));
  const uintptr_t vtable = FindVtable("hud: the party bar's vtable", ee::kRttiPartyBar);
  const uintptr_t update = vtable ? VtableSlot(vtable, ee::kGuiPanelVtableUpdate) : 0;
  if (!g_set_model || !g_add_to_panel || !g_set_enabled || !g_app_manager || !update) {
    Log("SKIP hud: the Short Rest button - an anchor is missing (!shortrest still works)");
    return;
  }
  HookAddress("client party bar update (the Short Rest button)", update, reinterpret_cast<void*>(&PartyBarUpdate),
              reinterpret_cast<void**>(&g_update));
}
