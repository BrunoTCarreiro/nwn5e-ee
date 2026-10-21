// The game's own character creation, extended (docs/CHARGEN.md §10, route A):
// nwn5e's steps are real pages of the classic creation UI, built from the
// engine's own page classes and controls, laid out by served .gui files.
//
// A2 — the Background step (ability_scores faithful). The hub's Alignment step
// (alignment is flavour in nwn5e, CM14) becomes Background: its button says
// "Background" (served cg_main.gui), and when the hub opens the Alignment page
// nwn5e rebuilds that page in place — same memory, so nothing the hub holds
// dangles — as a Class page (title, help box, list, Recommended / OK / Cancel:
// the engine's own controls) with nwn5e's vtable copy, whose init is BgInit,
// and nwn5e's layout (cg_background.gui). The page lists the four SRD 5.2
// backgrounds; OK asks for the ability split (seven) in the same page; OK
// again sets neutral alignment (as the Alignment page's OK would), hands the
// chosen background feat to rules_abilities (given when the character
// enters) and exits with the Alignment page's code, so the hub marks the step
// done, resets the later ones and enables Ability Scores — its own logic.
//
// Every engine piece is found by an anchor and checked (hooks.h); if one is
// missing nothing is hooked and the layouts aren't served: the game's own
// Alignment step stays.

#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <string>

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
using PageCtorFn = void (*)(void* page, uint32_t creature, int mode);
using DestroyFn = void (*)(void* object, int free_it);
using ObjectFn = void (*)(void* object);
using ObjectIntFn = void (*)(void* object, int value);
using ImportFn = int (*)(void* control, void* panel, const char* tag);
using SetTextFn = void (*)(void* wrapper, uint32_t strref);
using SetReadOnlyFn = void (*)(void* edit, bool read_only);
using SetModelFn = void (*)(void* object, const char* model);
using AddButtonFn = int (*)(void* list, void* button, int id);
using ResRefAssignFn = void* (*)(void* resref, const char* text);
using GetCreatureFn = void* (*)(uint32_t id);

struct Api {
  NewFn engine_new = nullptr;
  PageCtorFn class_ctor = nullptr;
  ObjectFn radio_ctor = nullptr;
  ObjectFn class_cancel = nullptr;
  GetCreatureFn get_creature = nullptr;
  ImportFn import = nullptr;
  SetTextFn set_text = nullptr;
  SetReadOnlyFn set_read_only = nullptr;
  ObjectFn register_update = nullptr;
  ObjectIntFn set_enabled = nullptr;
  SetModelFn set_model = nullptr;
  AddButtonFn add_button = nullptr;
  ObjectFn delete_buttons = nullptr;
  ObjectIntFn select_tab = nullptr;
  ResRefAssignFn resref_assign = nullptr;
  ObjectIntFn deactivate = nullptr;
};
Api g;
ObjectFn g_modal_activate = nullptr;  // trampolines
ObjectFn g_class_ok = nullptr;
uintptr_t g_hub_vtable = 0;
uintptr_t g_alignment_vtable = 0;
uintptr_t g_background_vtable[1 + ee::kPageVtableSlots];  // slot -1 (the RTTI locator) + 41 slots

// nwn5e's strings (data/strings.txt).
constexpr uint32_t kStrBackground = 16000240;
constexpr uint32_t kStrIncreases = 16000241;
constexpr uint32_t kStrBackgroundHelp = 16000242;
constexpr uint32_t kStrIncreasesHelp = 16000243;
constexpr uint32_t kStrFirstName = 16000244;         // Acolyte, Criminal, Sage, Soldier
constexpr uint32_t kStrFirstDescription = 16000204;  // the four backgrounds' descriptions
constexpr uint32_t kStrFirstSplit = 16000210;        // 4 x 7 split names

// One creation at a time (the hub is modal and single).
struct Page {
  int phase = 0;        // 0: the background; 1: its ability split
  int background = -1;  // 0..3 (Acolyte, Criminal, Sage, Soldier)
  int split = -1;       // 0..6
};
Page g_page;
int g_creation_class = -1;  // classes.2da row taken on the Class page

uint8_t* At(void* page, size_t offset) { return static_cast<uint8_t*>(page) + offset; }

void Bind(void* page, size_t callback, void* object, const void* fn) {
  SetField<void*>(page, callback, object);
  SetField<const void*>(page, callback + 8, fn);
}

rules::Background BackgroundAt(int index) { return static_cast<rules::Background>(index + 1); }

bool IsCreationHub(void* object) {
  return object && Field<uintptr_t>(object, 0) == g_hub_vtable && Field<int32_t>(object, ee::kHubCustomizeMode) == 0;
}

void AddItem(void* list, int id, uint32_t strref) {
  void* item = g.engine_new(ee::kListItemSize);
  if (!item) return;
  g.radio_ctor(item);
  g.set_model(item, ee::kListItemModel);
  g.add_button(list, item, id);
  g.set_text(At(item, ee::kListItemCaption), strref);
}

// The help pane and the OK button for the current choice.
void ShowChoice(void* page) {
  if (g_page.phase == 0) {
    if (g_page.background >= 0) {
      g.set_text(At(page, ee::kPageHelpTitle), kStrFirstName + g_page.background);
      g.set_text(At(page, ee::kPageHelpBody), kStrFirstDescription + g_page.background);
    } else {
      g.set_text(At(page, ee::kPageHelpTitle), kStrBackground);
      g.set_text(At(page, ee::kPageHelpBody), kStrBackgroundHelp);
    }
    g.set_enabled(At(page, ee::kPageOk), g_page.background >= 0 ? 1 : 0);
  } else {
    g.set_text(At(page, ee::kPageHelpTitle), kStrFirstName + g_page.background);
    g.set_text(At(page, ee::kPageHelpBody), kStrIncreasesHelp);
    g.set_enabled(At(page, ee::kPageOk), g_page.split >= 0 ? 1 : 0);
  }
}

// The list for the current phase, its selection, the title and help.
void Fill(void* page) {
  void* list = At(page, ee::kPageList);
  g.delete_buttons(list);
  int selected = -1;
  if (g_page.phase == 0) {
    for (int i = 0; i < 4; ++i) AddItem(list, i, kStrFirstName + i);
    selected = g_page.background;
    g.set_text(At(page, ee::kPageTitleText), kStrBackground);
  } else {
    for (int split = 0; split < rules::kBackgroundSplits; ++split) {
      AddItem(list, split, kStrFirstSplit + g_page.background * rules::kBackgroundSplits + split);
    }
    selected = g_page.split;
    g.set_text(At(page, ee::kPageTitleText), kStrIncreases);
  }
  if (selected >= 0) g.select_tab(At(list, ee::kListTabSet), selected);
  g.register_update(list);
  ShowChoice(page);
}

void BgSelect(void* page, int index) {
  const int* ids = Field<const int*>(page, ee::kPageListIds);
  const int id = ids && index >= 0 ? ids[index] : -1;
  if (id < 0) return;
  (g_page.phase == 0 ? g_page.background : g_page.split) = id;
  ShowChoice(page);
}

void BgTitleHelp(void* page) {
  g.set_text(At(page, ee::kPageHelpTitle), g_page.phase == 0 ? kStrBackground : kStrIncreases);
  g.set_text(At(page, ee::kPageHelpBody), g_page.phase == 0 ? kStrBackgroundHelp : kStrIncreasesHelp);
}

void BgRecommended(void* page) {
  const auto character_class =
      rules::ClassFromNwnId(g_creation_class >= 0 ? g_creation_class : 4).value_or(rules::CharacterClass::kFighter);
  if (g_page.phase == 0) {
    g_page.background = static_cast<int>(rules::RecommendedBackground2024(character_class)) - 1;
    g.select_tab(At(page, ee::kPageList + ee::kListTabSet), g_page.background);
  } else {
    g_page.split = rules::RecommendedBackgroundSplit2024(BackgroundAt(g_page.background), character_class);
    g.select_tab(At(page, ee::kPageList + ee::kListTabSet), g_page.split);
  }
  ShowChoice(page);
}

// The Alignment page's own write: neutral (it's flavour in nwn5e).
void SetNeutral(void* page) {
  void* creature = g.get_creature(Field<uint32_t>(page, ee::kClassPageCreature));
  void* stats = creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
  if (!stats) return;
  SetField<uint16_t>(stats, ee::kClientStatsLawChaos, ee::kAlignmentNeutral);
  SetField<uint16_t>(stats, ee::kClientStatsGoodEvil, ee::kAlignmentNeutral);
}

void BgOk(void* page) {
  if (g_page.phase == 0) {
    if (g_page.background < 0) return;
    g_page.phase = 1;
    if (g_page.split < 0) {  // start on the class's suggestion
      const auto character_class = rules::ClassFromNwnId(g_creation_class >= 0 ? g_creation_class : 4)
                                       .value_or(rules::CharacterClass::kFighter);
      g_page.split = rules::RecommendedBackgroundSplit2024(BackgroundAt(g_page.background), character_class);
    }
    Fill(page);
    return;
  }
  if (g_page.split < 0) return;
  const int feat = rules::BackgroundFeat(BackgroundAt(g_page.background), g_page.split);
  SetNeutral(page);
  SetCreationBackground(feat);
  Log("creation: Background page done - %s, split %d (feat %d); neutral alignment",
      rules::BackgroundData(BackgroundAt(g_page.background))->name, g_page.split, feat);
  g.deactivate(page, ee::kExitAlignmentDone);  // the hub: step done, Ability Scores next
  reinterpret_cast<DestroyFn>(g_background_vtable[1])(page, 1);  // last: the page is gone
}

// The page's init (its vtable slot 12), with the layout open — as the Class page's own.
void BgInit(void* page) {
  g.import(At(page, ee::kPageHelpBox), page, "HelpBox");
  g.set_read_only(At(page, ee::kPageHelpBox), true);
  g.register_update(At(page, ee::kPageHelpBox));
  SetField<uint32_t>(page, ee::kPageTitleFlags, Field<uint32_t>(page, ee::kPageTitleFlags) | 4);
  g.import(At(page, ee::kPageTitle), page, "Title");
  SetField<int32_t>(page, ee::kPageTitleId, 0);
  Bind(page, ee::kPageTitleClick, page, reinterpret_cast<const void*>(&BgTitleHelp));
  g.import(At(page, ee::kPageCancel), page, "CancelButton");
  Bind(page, ee::kPageCancelClick, page, reinterpret_cast<const void*>(g.class_cancel));
  g.import(At(page, ee::kPageOk), page, "OkButton");
  Bind(page, ee::kPageOkClick, page, reinterpret_cast<const void*>(&BgOk));
  g.import(At(page, ee::kPageRecommend), page, "RecommendButton");
  Bind(page, ee::kPageRecommendClick, page, reinterpret_cast<const void*>(&BgRecommended));
  g.import(At(page, ee::kPageList), page, "BackgroundListBox");
  Bind(page, ee::kPageListSelect, page, reinterpret_cast<const void*>(&BgSelect));
  Fill(page);
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
    g_page.phase = 0;  // a choice made earlier stays selected
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

}  // namespace

void InstallCreationPages() {
  if (g_config.ability_scores != rules::RuleVariant::kFaithful) return;  // NWN's Alignment step stays
  bool ok = Export(&g.import, ee::kImportLayoutData);
  ok &= Export(&g.set_text, ee::kSetTextStrref);
  ok &= Export(&g.set_read_only, ee::kSetReadOnly);
  ok &= Export(&g.register_update, ee::kRegisterUpdate);
  ok &= Export(&g.set_enabled, ee::kTextButtonSetEnabled);
  ok &= Export(&g.set_model, ee::kSetModel);
  ok &= Export(&g.add_button, ee::kListAddButton);
  ok &= Export(&g.delete_buttons, ee::kListDeleteButtons);
  ok &= Export(&g.select_tab, ee::kTabSetSelectTab);
  ok &= Export(&g.resref_assign, ee::kResRefAssign);
  ok &= Export(&g.deactivate, ee::kGuiModalPanelDeactivate);
  if (!ok) return;
  // The engine's operator new: the call in CGuiPanel::Activate's scene-view allocation.
  const uintptr_t new_site = FindPatternIn("creation pages: operator new", FunctionAt(EngineExport(ee::kGuiPanelActivate)),
                                           ee::kOperatorNewPattern);
  g.engine_new = reinterpret_cast<NewFn>(new_site ? CallTarget(new_site + ee::kOperatorNewCall) : 0);
  // Vtables, handlers and constructors.
  g_hub_vtable = FindVtable("creation pages: the hub", ee::kRttiHub);
  g_alignment_vtable = FindVtable("creation pages: the Alignment page", ee::kRttiAlignmentPage);
  const uintptr_t class_vtable = FindVtable("creation pages: the Class page", ee::kRttiClassPage);
  const uintptr_t hub_init = VtableSlot(g_hub_vtable, ee::kVtablePageInit);
  const uintptr_t class_init = VtableSlot(class_vtable, ee::kVtablePageInit);
  const uintptr_t alignment_init = VtableSlot(g_alignment_vtable, ee::kVtablePageInit);
  const uintptr_t class_button = hub_init ? HandlerByTag("creation pages: the hub's Class button", hub_init, "ClassButton") : 0;
  g.class_ctor = reinterpret_cast<PageCtorFn>(
      class_vtable && class_button ? OtherReference("the Class page's constructor", class_vtable, class_button) : 0);
  g.class_cancel = reinterpret_cast<ObjectFn>(
      class_init ? HandlerByTag("creation pages: the Class page's Cancel", class_init, "CancelButton") : 0);
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
  Log("creation pages: new 0x%zx, Class ctor 0x%zx, list item ctor 0x%zx, GetCreature 0x%zx, Class Cancel 0x%zx, "
      "Class OK 0x%zx",
      rva(reinterpret_cast<void*>(g.engine_new)), rva(reinterpret_cast<void*>(g.class_ctor)),
      rva(reinterpret_cast<void*>(g.radio_ctor)), rva(reinterpret_cast<void*>(g.get_creature)),
      rva(reinterpret_cast<void*>(g.class_cancel)), class_ok ? class_ok - base : 0);
  if (!g.engine_new || !g.class_ctor || !g.radio_ctor || !g.get_creature || !g.class_cancel || !class_ok ||
      !g_hub_vtable || !g_alignment_vtable) {
    Log("SKIP creation pages: an anchor is missing - the game's own Alignment step stays");
    return;
  }
  // nwn5e's Background page vtable: the Class page's, its init replaced.
  memcpy(g_background_vtable, reinterpret_cast<const void*>(class_vtable - sizeof(uintptr_t)),
         sizeof(g_background_vtable));
  g_background_vtable[1 + ee::kVtablePageInit] = reinterpret_cast<uintptr_t>(&BgInit);
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
