// The game's own character creation, extended (docs/CHARGEN.md §10, route A):
// nwn5e's steps are real pages of the classic creation UI, built from the
// engine's own page classes and controls, laid out by served .gui files.
//
// The Background step (ability_scores faithful). The hub's Alignment step
// (alignment is flavour in nwn5e, CM14) becomes Background — its button says
// so (served cg_main.gui) — and takes two pages:
// 1. the Background page: when the hub opens its Alignment page, nwn5e
//    rebuilds that page in place (same memory: nothing the hub holds
//    dangles) as a Class page — title, help box, list, Recommended / OK /
//    Cancel, the engine's own controls — with nwn5e's vtable copy (its init
//    is BgInit) and layout (cg_background.gui): the four SRD 5.2 backgrounds;
// 2. the Ability Score Increases page: the game's own Ability Scores page
//    (built as the hub builds it) with nwn5e's init and layout
//    (cg_bgincrease.gui: the game's, two captions and the title changed):
//    the background's three abilities take +2 / +1 or +1 / +1 / +1 with the
//    page's own arrows, the other three greyed; OK sets neutral alignment (as
//    the Alignment page would), hands the chosen background feat to
//    rules_abilities (given when the character enters) and exits with the
//    Alignment page's code — the hub's own logic marks the step done, resets
//    the later ones and moves on to Ability Scores.
//
// Every engine piece is found by an anchor and checked (hooks.h); if one is
// missing nothing is hooked and the layouts aren't served: the game's own
// Alignment step stays.

#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/backgrounds.h"
#include "rules/nwn_conversion.h"

void SetCreationBackground(int feat);  // rules_abilities.cpp

namespace {

using NewFn = void* (*)(size_t size);
using ClassCtorFn = void (*)(void* page, uint32_t creature, int mode);
using AbilitiesCtorFn = void (*)(void* page, uint32_t creature);
using DestroyFn = void (*)(void* object, int free_it);
using ObjectFn = void (*)(void* object);
using ObjectIntFn = void (*)(void* object, int value);
using ImportFn = int (*)(void* control, void* panel, const char* tag);
using SetTextFn = void (*)(void* wrapper, uint32_t strref);
using SetColorFn = void (*)(void* wrapper, float r, float g, float b, float a);
using SetReadOnlyFn = void (*)(void* edit, bool read_only);
using SetModelFn = void (*)(void* object, const char* model);
using AddButtonFn = int (*)(void* list, void* button, int id);
using ResRefAssignFn = void* (*)(void* resref, const char* text);
using GetCreatureFn = void* (*)(uint32_t id);

struct Api {
  NewFn engine_new = nullptr;
  ClassCtorFn class_ctor = nullptr;
  AbilitiesCtorFn abilities_ctor = nullptr;
  ObjectFn radio_ctor = nullptr;
  ObjectFn class_cancel = nullptr;
  ObjectFn abilities_cancel = nullptr;
  GetCreatureFn get_creature = nullptr;
  ImportFn import = nullptr;
  SetTextFn set_text = nullptr;
  SetColorFn set_color = nullptr;
  SetReadOnlyFn set_read_only = nullptr;
  ObjectFn register_update = nullptr;
  ObjectIntFn set_text_button_enabled = nullptr;
  ObjectIntFn set_button_enabled = nullptr;
  SetModelFn set_model = nullptr;
  AddButtonFn add_button = nullptr;
  ObjectFn delete_buttons = nullptr;
  ObjectIntFn select_tab = nullptr;
  ResRefAssignFn resref_assign = nullptr;
  ObjectIntFn deactivate = nullptr;
  ObjectFn modal_activate_export = nullptr;
};
Api g;
ObjectFn g_modal_activate = nullptr;  // trampolines
ObjectFn g_class_ok = nullptr;
uintptr_t g_hub_vtable = 0;
uintptr_t g_alignment_vtable = 0;
// nwn5e's page vtables: slot -1 (the RTTI locator) + 41 slots, copied from
// the engine page they are built on, the init (slot 12) replaced.
uintptr_t g_background_vtable[1 + ee::kPageVtableSlots];
uintptr_t g_increase_vtable[1 + ee::kPageVtableSlots];

// nwn5e's strings (data/strings.txt).
constexpr uint32_t kStrBackground = 16000240;
constexpr uint32_t kStrIncreases = 16000241;
constexpr uint32_t kStrBackgroundHelp = 16000242;
constexpr uint32_t kStrIncreasesHelp = 16000243;
constexpr uint32_t kStrFirstName = 16000244;         // Acolyte, Criminal, Sage, Soldier
constexpr uint32_t kStrFirstDescription = 16000204;  // the four backgrounds' descriptions
constexpr uint32_t kStrPlusZero = 16000250;          // "+0", "+1", "+2"
constexpr uint32_t kStrDash = 16000253;
constexpr uint32_t kStrDigitZero = 16000254;  // "0" .. "3"

// One creation at a time (the hub is modal and single).
int g_background = -1;      // 0..3 (Acolyte, Criminal, Sage, Soldier)
int g_increases[6] = {};    // the split being chosen, by rules::Ability
int g_increases_for = -1;   // the background g_increases belong to
int g_creation_class = -1;  // classes.2da row taken on the Class page

uint8_t* At(void* page, size_t offset) { return static_cast<uint8_t*>(page) + offset; }

void Bind(void* page, size_t callback, void* object, const void* fn) {
  SetField<void*>(page, callback, object);
  SetField<const void*>(page, callback + 8, fn);
}

rules::Background BackgroundAt(int index) { return static_cast<rules::Background>(index + 1); }

rules::CharacterClass CreationClass() {
  return rules::ClassFromNwnId(g_creation_class >= 0 ? g_creation_class : 4).value_or(rules::CharacterClass::kFighter);
}

bool IsCreationHub(void* object) {
  return object && Field<uintptr_t>(object, 0) == g_hub_vtable && Field<int32_t>(object, ee::kHubCustomizeMode) == 0;
}

// The Alignment page's own write: neutral (it's flavour in nwn5e).
void SetNeutral(uint32_t creature_id) {
  void* creature = g.get_creature(creature_id);
  void* stats = creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
  if (!stats) return;
  SetField<uint16_t>(stats, ee::kClientStatsLawChaos, ee::kAlignmentNeutral);
  SetField<uint16_t>(stats, ee::kClientStatsGoodEvil, ee::kAlignmentNeutral);
}

// --- Page 2: Ability Score Increases (the game's Ability Scores page) --------

bool InBackground(int ability) {
  const rules::BackgroundInfo* info = rules::BackgroundData(BackgroundAt(g_background));
  if (!info) return false;
  for (rules::Ability a : info->abilities) {
    if (static_cast<int>(a) == ability) return true;
  }
  return false;
}

int IncreasesLeft() {
  int used = 0;
  for (int amount : g_increases) used += amount;
  return 3 - used;
}

void ShowIncreases(void* page) {
  const int left = IncreasesLeft();
  for (int i = 0; i < 6; ++i) {
    const ee::AbilityRow& row = ee::kAbilityRows[i];
    const bool live = InBackground(i);
    const float shade = live ? 1.0f : 0.5f;
    g.set_color(At(page, row.label_at + ee::kLabelText), shade, shade, shade, 1.0f);
    g.set_color(At(page, row.value_at + ee::kLabelText), shade, shade, shade, 1.0f);
    g.set_text(At(page, row.value_at + ee::kLabelText), live ? kStrPlusZero + g_increases[i] : kStrDash);
    g.set_button_enabled(At(page, row.up_at), live && g_increases[i] < 2 && left > 0 ? 1 : 0);
    g.set_button_enabled(At(page, row.down_at), live && g_increases[i] > 0 ? 1 : 0);
  }
  g.set_text(At(page, ee::kAbilitiesPointsText), kStrDigitZero + (left < 0 ? 0 : left));
  g.set_text(At(page, ee::kAbilitiesCostText), kStrPlusZero + 2);  // "Highest Increase: +2"
  g.set_text_button_enabled(At(page, ee::kAbilitiesOk), left == 0 ? 1 : 0);
}

template <int kAbility, int kStep>
void Arrow(void* page) {
  int& amount = g_increases[kAbility];
  if (!InBackground(kAbility)) return;
  if (kStep > 0 && (amount >= 2 || IncreasesLeft() <= 0)) return;
  if (kStep < 0 && amount <= 0) return;
  amount += kStep;
  ShowIncreases(page);
}

using ArrowFn = void (*)(void* page);
constexpr ArrowFn kDown[6] = {&Arrow<0, -1>, &Arrow<1, -1>, &Arrow<2, -1>, &Arrow<3, -1>, &Arrow<4, -1>, &Arrow<5, -1>};
constexpr ArrowFn kUp[6] = {&Arrow<0, 1>, &Arrow<1, 1>, &Arrow<2, 1>, &Arrow<3, 1>, &Arrow<4, 1>, &Arrow<5, 1>};

void IncreasesRecommended(void* page) {
  const rules::Background background = BackgroundAt(g_background);
  const auto choice = rules::BackgroundFromFeat(
      rules::BackgroundFeat(background, rules::RecommendedBackgroundSplit2024(background, CreationClass())));
  if (!choice) return;
  rules::BackgroundIncreases(background, choice->chosen, g_increases);
  ShowIncreases(page);
}

void IncreasesHelp(void* page) {
  g.set_text(At(page, ee::kAbilitiesHelpTitle), kStrFirstName + g_background);
  g.set_text(At(page, ee::kAbilitiesHelpBody), kStrIncreasesHelp);
}

void IncreasesOk(void* page) {
  if (IncreasesLeft() != 0) return;
  const rules::Background background = BackgroundAt(g_background);
  int feat = -1;
  for (int split = 0; split < rules::kBackgroundSplits && feat < 0; ++split) {
    const auto choice = rules::BackgroundFromFeat(rules::BackgroundFeat(background, split));
    int amounts[6];
    if (choice && rules::BackgroundIncreases(background, choice->chosen, amounts).empty() &&
        memcmp(amounts, g_increases, sizeof(amounts)) == 0) {
      feat = rules::BackgroundFeat(background, split);
    }
  }
  if (feat < 0) return;  // not a legal split (the arrows' limits prevent it)
  SetNeutral(Field<uint32_t>(page, ee::kAbilitiesPageCreature));
  SetCreationBackground(feat);
  Log("creation: Background done - %s (feat %d); neutral alignment", rules::BackgroundData(background)->name, feat);
  g.deactivate(page, ee::kExitAlignmentDone);  // the hub: step done, Ability Scores next
  reinterpret_cast<DestroyFn>(g_increase_vtable[1])(page, 1);  // last: the page is gone
}

// The increases page's init (its vtable slot 12), with the layout open.
void IncreasesInit(void* page) {
  g.import(At(page, ee::kAbilitiesCancel), page, "CancelButton");
  Bind(page, ee::kAbilitiesCancelClick, page, reinterpret_cast<const void*>(g.abilities_cancel));
  g.import(At(page, ee::kAbilitiesOk), page, "OkButton");
  Bind(page, ee::kAbilitiesOkClick, page, reinterpret_cast<const void*>(&IncreasesOk));
  g.import(At(page, ee::kAbilitiesRecommend), page, "RecommendButton");
  Bind(page, ee::kAbilitiesRecommendClick, page, reinterpret_cast<const void*>(&IncreasesRecommended));
  g.import(At(page, ee::kAbilitiesHelpBox), page, "HelpEdit");
  g.set_read_only(At(page, ee::kAbilitiesHelpBox), true);
  g.register_update(At(page, ee::kAbilitiesHelpBox));
  IncreasesHelp(page);
  g.import(At(page, ee::kPageTitle), page, "Title");
  Bind(page, ee::kPageTitleClick, page, reinterpret_cast<const void*>(&IncreasesHelp));
  g.set_text(At(page, ee::kPageTitleText), kStrIncreases);
  for (int i = 0; i < 6; ++i) {
    const ee::AbilityRow& row = ee::kAbilityRows[i];
    g.import(At(page, row.label_at), page, row.label);
    g.import(At(page, row.value_at), page, row.value);
    g.import(At(page, row.down_at), page, row.down);
    Bind(page, row.down_at + ee::kTimerButtonClick, page, reinterpret_cast<const void*>(kDown[i]));
    g.import(At(page, row.up_at), page, row.up);
    Bind(page, row.up_at + ee::kTimerButtonClick, page, reinterpret_cast<const void*>(kUp[i]));
  }
  g.import(At(page, ee::kAbilitiesPoints), page, "PointsEdit");
  g.import(At(page, ee::kAbilitiesCost), page, "CostEdit");
  if (g_increases_for != g_background) {  // a new background: start from nothing
    for (int& amount : g_increases) amount = 0;
    g_increases_for = g_background;
  }
  ShowIncreases(page);
  Log("creation: Ability Score Increases page built (%s)", rules::BackgroundData(BackgroundAt(g_background))->name);
}

// --- Page 1: the Background (a Class page) -----------------------------------

void AddItem(void* list, int id, uint32_t strref) {
  void* item = g.engine_new(ee::kListItemSize);
  if (!item) return;
  g.radio_ctor(item);
  g.set_model(item, ee::kListItemModel);
  g.add_button(list, item, id);
  g.set_text(At(item, ee::kListItemCaption), strref);
}

void ShowBackground(void* page) {
  if (g_background >= 0) {
    g.set_text(At(page, ee::kPageHelpTitle), kStrFirstName + g_background);
    g.set_text(At(page, ee::kPageHelpBody), kStrFirstDescription + g_background);
  } else {
    g.set_text(At(page, ee::kPageHelpTitle), kStrBackground);
    g.set_text(At(page, ee::kPageHelpBody), kStrBackgroundHelp);
  }
  g.set_text_button_enabled(At(page, ee::kPageOk), g_background >= 0 ? 1 : 0);
}

void BgSelect(void* page, int index) {
  const int* ids = Field<const int*>(page, ee::kPageListIds);
  if (!ids || index < 0) return;
  g_background = ids[index];
  ShowBackground(page);
}

void BgTitleHelp(void* page) {
  g.set_text(At(page, ee::kPageHelpTitle), kStrBackground);
  g.set_text(At(page, ee::kPageHelpBody), kStrBackgroundHelp);
}

void BgRecommended(void* page) {
  g_background = static_cast<int>(rules::RecommendedBackground2024(CreationClass())) - 1;
  g.select_tab(At(page, ee::kPageList + ee::kListTabSet), g_background);
  ShowBackground(page);
}

// OK: on to the increases page — built as the hub builds its Ability Scores
// page, handed the same exit delegate — then this page closes.
void BgOk(void* page) {
  if (g_background < 0) return;
  void* next = g.engine_new(ee::kAbilitiesPageSize);
  if (!next) return;
  g.abilities_ctor(next, Field<uint32_t>(page, ee::kClassPageCreature));
  SetField<const void*>(next, 0, &g_increase_vtable[1]);
  g.resref_assign(At(next, ee::kPageLayout), "CG_BGINCREASE");
  SetField<void*>(next, ee::kPageExitObject, Field<void*>(page, ee::kPageExitObject));
  SetField<void*>(next, ee::kPageExitFunction, Field<void*>(page, ee::kPageExitFunction));
  g.deactivate(page, ee::kExitBackgroundNext);  // the hub only refreshes its sheet
  g.modal_activate_export(next);
  reinterpret_cast<DestroyFn>(g_background_vtable[1])(page, 1);  // last: this page is gone
}

// The Background page's init (its vtable slot 12), with the layout open — as the Class page's own.
void BgInit(void* page) {
  g.import(At(page, ee::kPageHelpBox), page, "HelpBox");
  g.set_read_only(At(page, ee::kPageHelpBox), true);
  g.register_update(At(page, ee::kPageHelpBox));
  SetField<uint32_t>(page, ee::kPageTitleFlags, Field<uint32_t>(page, ee::kPageTitleFlags) | 4);
  g.import(At(page, ee::kPageTitle), page, "Title");
  SetField<int32_t>(page, ee::kPageTitleId, 0);
  Bind(page, ee::kPageTitleClick, page, reinterpret_cast<const void*>(&BgTitleHelp));
  g.set_text(At(page, ee::kPageTitleText), kStrBackground);
  g.import(At(page, ee::kPageCancel), page, "CancelButton");
  Bind(page, ee::kPageCancelClick, page, reinterpret_cast<const void*>(g.class_cancel));
  g.import(At(page, ee::kPageOk), page, "OkButton");
  Bind(page, ee::kPageOkClick, page, reinterpret_cast<const void*>(&BgOk));
  g.import(At(page, ee::kPageRecommend), page, "RecommendButton");
  Bind(page, ee::kPageRecommendClick, page, reinterpret_cast<const void*>(&BgRecommended));
  void* list = At(page, ee::kPageList);
  g.import(list, page, "BackgroundListBox");
  Bind(page, ee::kPageListSelect, page, reinterpret_cast<const void*>(&BgSelect));
  g.delete_buttons(list);
  for (int i = 0; i < 4; ++i) AddItem(list, i, kStrFirstName + i);
  if (g_background >= 0) g.select_tab(At(list, ee::kListTabSet), g_background);  // a choice made earlier stays
  g.register_update(list);
  ShowBackground(page);
  Log("creation: Background page built (class row %d)", g_creation_class);
}

// The hub opens its Alignment page: rebuilt in place as the Background page.
void ModalActivate(void* panel) {
  if (panel && Field<uintptr_t>(panel, 0) == g_alignment_vtable &&
      IsCreationHub(Field<void*>(panel, ee::kPageExitObject))) {
    void* exit_object = Field<void*>(panel, ee::kPageExitObject);
    void* exit_function = Field<void*>(panel, ee::kPageExitFunction);
    const uint32_t creature = Field<uint32_t>(panel, ee::kAlignmentPageCreature);
    reinterpret_cast<DestroyFn>(VtableSlot(g_alignment_vtable, 0))(panel, 0);  // destroy, keep the memory
    g.class_ctor(panel, creature, 0);
    SetField<const void*>(panel, 0, &g_background_vtable[1]);
    g.resref_assign(At(panel, ee::kPageLayout), "CG_BACKGROUND");
    SetField<void*>(panel, ee::kPageExitObject, exit_object);
    SetField<void*>(panel, ee::kPageExitFunction, exit_function);
    Log("creation: the Alignment step opens nwn5e's Background page");
  }
  g_modal_activate(panel);
}

// The Class page's OK (creation): remember the class for Recommended.
void ClassOk(void* page) {
  if (IsCreationHub(Field<void*>(page, ee::kPageExitObject))) {
    const int selected = Field<int32_t>(page, ee::kPageListSelected);
    const int* ids = Field<const int*>(page, ee::kPageListIds);
    if (selected >= 0 && ids) g_creation_class = ids[selected];
  }
  g_class_ok(page);
}

template <typename Fn>
bool Export(Fn* out, const char* name) {
  *out = reinterpret_cast<Fn>(EngineExport(name));
  if (!*out) Log("SKIP creation pages: no export %s", name);
  return *out != nullptr;
}

// The one function referencing `vtable` other than `not_this` (a page's constructor).
uintptr_t OtherReference(const char* what, uintptr_t vtable, uintptr_t not_this) {
  uintptr_t found = 0;
  int count = 0;
  for (uintptr_t reference : FindRipReferences(vtable)) {
    const uintptr_t entry = FunctionEntry(reference);
    if (entry && entry != not_this && entry != found) {
      found = entry;
      ++count;
    }
  }
  if (count != 1) {
    Log("SKIP creation pages: %s - %d candidates (expected 1)", what, count);
    return 0;
  }
  return found;
}

// The constructor a hub button handler calls right after `new`, checked by
// the page vtable it writes.
uintptr_t ConstructorAfterNew(const char* what, uintptr_t handler, uintptr_t page_vtable) {
  const CodeRange range = FunctionAt(handler);
  const uintptr_t allocator = reinterpret_cast<uintptr_t>(g.engine_new);
  bool after_new = false;
  for (uintptr_t at = handler; range.Contains(at) && at + kCallLength <= range.end; ++at) {
    const uintptr_t target = CallTarget(at);
    if (!target || FunctionEntry(target) != target) continue;
    if (target == allocator) {
      after_new = true;
      continue;
    }
    if (!after_new) continue;
    for (uintptr_t reference : FindRipReferences(page_vtable)) {
      if (FunctionAt(target).Contains(reference)) return target;
    }
    break;
  }
  Log("SKIP creation pages: %s not found", what);
  return 0;
}

void CopyVtable(uintptr_t* copy, uintptr_t vtable, uintptr_t init) {
  memcpy(copy, reinterpret_cast<const void*>(vtable - sizeof(uintptr_t)), sizeof(uintptr_t) * (1 + ee::kPageVtableSlots));
  copy[1 + ee::kVtablePageInit] = init;
}

}  // namespace

void InstallCreationPages() {
  if (g_config.ability_scores != rules::RuleVariant::kFaithful) return;  // NWN's Alignment step stays
  bool ok = Export(&g.import, ee::kImportLayoutData);
  ok &= Export(&g.set_text, ee::kSetTextStrref);
  ok &= Export(&g.set_color, ee::kSetColor);
  ok &= Export(&g.set_read_only, ee::kSetReadOnly);
  ok &= Export(&g.register_update, ee::kRegisterUpdate);
  ok &= Export(&g.set_text_button_enabled, ee::kTextButtonSetEnabled);
  ok &= Export(&g.set_button_enabled, ee::kButtonSetEnabled);
  ok &= Export(&g.set_model, ee::kSetModel);
  ok &= Export(&g.add_button, ee::kListAddButton);
  ok &= Export(&g.delete_buttons, ee::kListDeleteButtons);
  ok &= Export(&g.select_tab, ee::kTabSetSelectTab);
  ok &= Export(&g.resref_assign, ee::kResRefAssign);
  ok &= Export(&g.deactivate, ee::kGuiModalPanelDeactivate);
  ok &= Export(&g.modal_activate_export, ee::kGuiModalPanelActivate);
  if (!ok) return;
  // The engine's operator new: the call in CGuiPanel::Activate's scene-view allocation.
  const uintptr_t new_site = FindPatternIn("creation pages: operator new", FunctionAt(EngineExport(ee::kGuiPanelActivate)),
                                           ee::kOperatorNewPattern);
  g.engine_new = reinterpret_cast<NewFn>(new_site ? CallTarget(new_site + ee::kOperatorNewCall) : 0);
  // Vtables, handlers and constructors.
  g_hub_vtable = FindVtable("creation pages: the hub", ee::kRttiHub);
  g_alignment_vtable = FindVtable("creation pages: the Alignment page", ee::kRttiAlignmentPage);
  const uintptr_t class_vtable = FindVtable("creation pages: the Class page", ee::kRttiClassPage);
  const uintptr_t abilities_vtable = FindVtable("creation pages: the Ability Scores page", ee::kRttiAbilitiesPage);
  const uintptr_t hub_init = VtableSlot(g_hub_vtable, ee::kVtablePageInit);
  const uintptr_t class_init = VtableSlot(class_vtable, ee::kVtablePageInit);
  const uintptr_t alignment_init = VtableSlot(g_alignment_vtable, ee::kVtablePageInit);
  const uintptr_t abilities_init = VtableSlot(abilities_vtable, ee::kVtablePageInit);
  const uintptr_t class_button = hub_init ? HandlerByTag("creation pages: the hub's Class button", hub_init, "ClassButton") : 0;
  const uintptr_t abilities_button =
      hub_init ? HandlerByTag("creation pages: the hub's Abilities button", hub_init, "AbilitiesButton") : 0;
  g.class_ctor = reinterpret_cast<ClassCtorFn>(
      class_vtable && class_button ? OtherReference("the Class page's constructor", class_vtable, class_button) : 0);
  g.abilities_ctor = reinterpret_cast<AbilitiesCtorFn>(
      g.engine_new && abilities_button && abilities_vtable
          ? ConstructorAfterNew("the Ability Scores page's constructor", abilities_button, abilities_vtable)
          : 0);
  g.class_cancel = reinterpret_cast<ObjectFn>(
      class_init ? HandlerByTag("creation pages: the Class page's Cancel", class_init, "CancelButton") : 0);
  g.abilities_cancel = reinterpret_cast<ObjectFn>(
      abilities_init ? HandlerByTag("creation pages: the Ability Scores page's Cancel", abilities_init, "CancelButton")
                     : 0);
  const uintptr_t class_ok = class_init ? HandlerByTag("creation pages: the Class page's OK", class_init, "OkButton") : 0;
  const uintptr_t alignment_ok =
      alignment_init ? HandlerByTag("creation pages: the Alignment page's OK", alignment_init, "OkButton") : 0;
  g.get_creature = reinterpret_cast<GetCreatureFn>(alignment_ok ? FirstCallIn(alignment_ok) : 0);
  // The list items' constructor: of the two functions using the radio button's
  // vtable, the one that starts by constructing the CGuiCheckbox base.
  const uintptr_t checkbox_ctor = EngineExport(ee::kCheckboxConstructor);
  for (uintptr_t reference : FindRipReferences(FindVtable("creation pages: list item", ee::kRttiCharGenRadioButton))) {
    const uintptr_t entry = FunctionEntry(reference);
    if (entry && FirstCallIn(entry) == checkbox_ctor) g.radio_ctor = reinterpret_cast<ObjectFn>(entry);
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  auto rva = [base](const void* p) { return p ? reinterpret_cast<uintptr_t>(p) - base : 0; };
  Log("creation pages: new 0x%zx, Class ctor 0x%zx, Abilities ctor 0x%zx, list item ctor 0x%zx, GetCreature 0x%zx, "
      "Class Cancel 0x%zx, Abilities Cancel 0x%zx, Class OK 0x%zx",
      rva(reinterpret_cast<void*>(g.engine_new)), rva(reinterpret_cast<void*>(g.class_ctor)),
      rva(reinterpret_cast<void*>(g.abilities_ctor)), rva(reinterpret_cast<void*>(g.radio_ctor)),
      rva(reinterpret_cast<void*>(g.get_creature)), rva(reinterpret_cast<void*>(g.class_cancel)),
      rva(reinterpret_cast<void*>(g.abilities_cancel)), class_ok ? class_ok - base : 0);
  if (!g.engine_new || !g.class_ctor || !g.abilities_ctor || !g.radio_ctor || !g.get_creature || !g.class_cancel ||
      !g.abilities_cancel || !class_ok || !g_hub_vtable || !g_alignment_vtable || !abilities_vtable) {
    Log("SKIP creation pages: an anchor is missing - the game's own Alignment step stays");
    return;
  }
  CopyVtable(g_background_vtable, class_vtable, reinterpret_cast<uintptr_t>(&BgInit));
  CopyVtable(g_increase_vtable, abilities_vtable, reinterpret_cast<uintptr_t>(&IncreasesInit));
  if (!HookExport("CGuiModalPanel::Activate (creation: the Alignment step opens the Background page)",
                  ee::kGuiModalPanelActivate, reinterpret_cast<void*>(&ModalActivate),
                  reinterpret_cast<void**>(&g_modal_activate))) {
    return;
  }
  HookAddress("CCharacterClassPanel::HandleOkButton (creation: the class, for Background's Recommended)", class_ok,
              reinterpret_cast<void*>(&ClassOk), reinterpret_cast<void**>(&g_class_ok));
  ServeCreationLayouts();
  Log("creation pages: Background takes the Alignment step (ability_scores faithful)");
}
