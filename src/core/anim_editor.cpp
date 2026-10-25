// !animedit (owner, 2026-10-25: "Let's try recipe" - "Confirm the recipes"): a
// window for animation recipes - a name, a base animation, an animation to mix
// toward and how much (0 the base, 1 the mix, below 0 exaggerated away from
// it), a speed, the part of the base kept. Save writes the draft
// (animedit\recipes\<name>.recipe); recipes are built into animations when the
// game starts (anim_packs.cpp), so a new or changed one plays after a restart.
// Play base / Play mix / Play recipe play them on the player character in
// place, looping (as !anim: the look-far loop swapped for it); Load lists the
// drafts and nwn5e's own recipes. The animation names are !anim's.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "data/anim_recipe.h"
#include "data/vfx_model.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "nui.h"

bool AnimationRecipeBuilt(const std::string& name, std::string* text);  // anim_packs.cpp

namespace {

constexpr const char* kWindow = "nwn5eanimed";
constexpr const char* kLoadWindow = "nwn5eanimld";
constexpr const char* kHoldSlot = "lookfar";

std::unordered_map<uint32_t, data::AnimRecipe> g_recipes;  // by player
std::vector<std::wstring> g_load_paths;

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  return directory.substr(0, directory.find_last_of(L"\\/"));
}

std::wstring DraftDirectory() { return DllDirectory() + L"\\animedit\\recipes"; }
std::wstring Wide(const std::string& text) { return std::wstring(text.begin(), text.end()); }
std::string Narrow(const std::wstring& text) {
  std::string out;
  for (wchar_t c : text) out += c < 128 ? static_cast<char>(c) : '?';
  return out;
}

std::vector<std::wstring> FilesIn(const std::wstring& directory, const wchar_t* pattern) {
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found;
  HANDLE handle = FindFirstFileW((directory + L"\\" + pattern).c_str(), &found);
  if (handle == INVALID_HANDLE_VALUE) return names;
  do {
    if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(found.cFileName);
  } while (FindNextFileW(handle, &found));
  FindClose(handle);
  std::sort(names.begin(), names.end());
  return names;
}

std::string Num(float value) {
  char text[32];
  sprintf_s(text, "%g", value);
  return text;
}

float ToFloat(const std::string& text, float fallback) {
  char* end = nullptr;
  const float value = strtof(text.c_str(), &end);
  return end && end != text.c_str() ? value : fallback;
}

data::AnimRecipe DefaultRecipe() {
  data::AnimRecipe r;
  r.name = "n5_myidle";
  r.base = "pausepsn";
  r.mix = "pause1";
  r.amount = 0.5f;
  return r;
}

void SetStatus(uint32_t player, const std::string& line) { NuiSetTexts(player, kWindow, {{"status", line}}); }

std::string Layout() {
  using namespace nui;
  const auto label = [](const char* text, float width) { return Sized(Label(Str(text)), width); };
  const auto field = [](const char* bind, float width) { return Sized(TextEdit(bind, "", bind, 32), width, 30.0f); };
  return Window(Col(List({
                    Label(Bind("status"), 52.0f),
                    Row(List({label("Name", 90.0f), field("a_name", 180.0f), Button("new", Str("New")),
                              Button("load", Str("Load...")), Button("save", Str("Save"))})),
                    Row(List({label("Base", 90.0f), field("a_base", 140.0f), label("Mix toward", 90.0f),
                              field("a_mix", 140.0f), Spacer()})),
                    Row(List({label("Amount", 90.0f), field("a_amount", 80.0f), label("Speed", 90.0f),
                              field("a_speed", 80.0f), Spacer()})),
                    Row(List({label("Keep from s", 90.0f), field("a_start", 80.0f), label("to s (0 = end)", 110.0f),
                              field("a_end", 80.0f), Spacer()})),
                    Text(Str("Amount: 0 = the base as it is, 1 = the mix, 0.5 halfway; below 0 exaggerates the base "
                             "away from the mix. Speed 2 = twice as fast. Names are the ones !anim lists. Recipes "
                             "are built when the game starts: Save, then restart to play a new or changed one."),
                         76.0f),
                    Row(List({Button("play_base", Str("Play base")), Button("play_mix", Str("Play mix")),
                              Button("play", Str("Play recipe")), Button("stop", Str("Stop"))})),
                    Row(List({Spacer(), Button("close", Str("Close"), 100.0f)})),
                })),
                Str("nwn5e - animation recipes"), 60.0f, 60.0f, 600.0f, 520.0f);
}

void PushAll(uint32_t player, const data::AnimRecipe& r) {
  NuiSetTexts(player, kWindow,
              {{"a_name", r.name}, {"a_base", r.base}, {"a_mix", r.mix}, {"a_amount", Num(r.amount)},
               {"a_speed", Num(r.speed)}, {"a_start", Num(r.start)}, {"a_end", Num(r.end)}});
}

void Hold(uint32_t player, const std::string& name) {
  const std::string clean = data::VfxResRef(name, 32);
  if (clean.empty()) return;
  const std::string code = "ReplaceObjectAnimation(OBJECT_SELF, \"" + std::string(kHoldSlot) + "\", \"" + clean +
                           "\"); ClearAllActions(); ActionPlayAnimation(ANIMATION_LOOPING_LOOK_FAR, 1.0, 600.0);";
  RunScriptChunk(code.c_str(), player);
}

void Stop(uint32_t player) {
  RunScriptChunk(("ClearAllActions(); ReplaceObjectAnimation(OBJECT_SELF, \"" + std::string(kHoldSlot) + "\");").c_str(),
                 player);
}

void Open(uint32_t player) {
  data::AnimRecipe& r = g_recipes[player];
  if (r.name.empty()) r = DefaultRecipe();
  if (!NuiOpen(player, kWindow, Layout(), {{"status", "A recipe: a base animation mixed toward another. "
                                                      "Play base and Play mix to compare."}})) {
    return;
  }
  PushAll(player, r);
  NuiWatch(player, kWindow, {"a_name", "a_base", "a_mix", "a_amount", "a_speed", "a_start", "a_end"});
}

void Save(uint32_t player, data::AnimRecipe& r) {
  r.name = data::VfxResRef(r.name, 32);
  if (r.name.empty()) {
    SetStatus(player, "Give the recipe a name (letters, digits, _).");
    return;
  }
  CreateDirectoryW((DllDirectory() + L"\\animedit").c_str(), nullptr);
  CreateDirectoryW(DraftDirectory().c_str(), nullptr);
  std::ofstream file((DraftDirectory() + L"\\" + Wide(r.name) + L".recipe").c_str(), std::ios::binary | std::ios::trunc);
  file << data::WriteAnimRecipe(r);
  const bool ok = static_cast<bool>(file);
  Log("anim editor: 0x%08X saved recipe %s (%s)", player, r.name.c_str(), ok ? "ok" : "failed");
  PushAll(player, r);
  SetStatus(player, ok ? "Saved " + r.name + ". Restart the game to build it, then Play recipe." : "Could not save it.");
}

void PlayRecipe(uint32_t player, const data::AnimRecipe& r) {
  std::string built;
  if (!AnimationRecipeBuilt(r.name, &built)) {
    SetStatus(player, r.name + " isn't built yet: Save it, then restart the game.");
    return;
  }
  Hold(player, r.name);
  SetStatus(player, built == data::WriteAnimRecipe(r)
                        ? "Playing " + r.name + "."
                        : "Playing " + r.name + " as built when the game started - restart to see your changes.");
}

std::string LoadLayout() {
  using namespace nui;
  return Window(Col(List({Label(Bind("ld_status"), 30.0f),
                          ListView(Cell(Button("ld", Bind("ld_labels")), 0.0f, true), Bind("ld_count"), 26.0f),
                          Row(List({Spacer(), Button("ld_close", Str("Close"), 100.0f)}))})),
                Str("nwn5e - load a recipe"), 670.0f, 60.0f, 380.0f, 480.0f);
}

void OpenLoad(uint32_t player) {
  g_load_paths.clear();
  std::string labels = "[";
  const auto add = [&](const std::wstring& directory, const char* kind) {
    for (const std::wstring& file : FilesIn(directory, L"*.recipe")) {
      labels += (g_load_paths.empty() ? "" : ",") +
                nui::Str(std::string(kind) + "   " + Narrow(file.substr(0, file.size() - 7)));
      g_load_paths.push_back(directory + L"\\" + file);
    }
  };
  add(DraftDirectory(), "yours");
  add(DllDirectory() + L"\\data\\anims", "nwn5e");
  labels += "]";
  if (!NuiOpen(player, kLoadWindow, LoadLayout(),
               {{"ld_status", std::to_string(g_load_paths.size()) + " recipes - click one to load it."}})) {
    return;
  }
  NuiSetJson(player, kLoadWindow, "ld_labels", labels);
  NuiSetJson(player, kLoadWindow, "ld_count", std::to_string(g_load_paths.size()));
}

void LoadEntry(uint32_t player, int index) {
  if (index < 0 || index >= static_cast<int>(g_load_paths.size())) return;
  std::ifstream file(g_load_paths[index].c_str(), std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  data::AnimRecipe recipe;
  std::string error;
  if (!data::ReadAnimRecipe(text.str(), &recipe, &error)) {
    SetStatus(player, "Could not load it: " + error);
    return;
  }
  g_recipes[player] = recipe;
  NuiClose(player, kLoadWindow);
  PushAll(player, recipe);
  SetStatus(player, "Loaded " + recipe.name + ".");
}

void OnEvent(const NuiEvent& event) {
  const uint32_t player = event.player;
  data::AnimRecipe& r = g_recipes[player];
  if (r.name.empty() && r.base.empty()) r = DefaultRecipe();
  if (event.type == "close") {
    Stop(player);
    return;
  }
  if (event.type == "watch") {
    const std::string value = NuiGetText(player, kWindow, event.element);
    if (event.element == "a_name") r.name = value;
    else if (event.element == "a_base") r.base = value;
    else if (event.element == "a_mix") r.mix = value;
    else if (event.element == "a_amount") r.amount = ToFloat(value, r.amount);
    else if (event.element == "a_speed") r.speed = std::max(0.05f, ToFloat(value, r.speed));
    else if (event.element == "a_start") r.start = std::max(0.0f, ToFloat(value, r.start));
    else if (event.element == "a_end") r.end = std::max(0.0f, ToFloat(value, r.end));
    return;
  }
  if (event.type != "click") return;
  const std::string& id = event.element;
  if (id == "close") {
    Stop(player);
    NuiClose(player, kWindow);
  } else if (id == "new") {
    r = DefaultRecipe();
    PushAll(player, r);
    SetStatus(player, "A new recipe.");
  } else if (id == "load") {
    OpenLoad(player);
  } else if (id == "save") {
    Save(player, r);
  } else if (id == "play_base") {
    Hold(player, r.base);
    SetStatus(player, "Playing the base, " + r.base + ".");
  } else if (id == "play_mix") {
    if (r.mix.empty()) {
      SetStatus(player, "No mix animation set.");
    } else {
      Hold(player, r.mix);
      SetStatus(player, "Playing the mix, " + r.mix + ".");
    }
  } else if (id == "play") {
    PlayRecipe(player, r);
  } else if (id == "stop") {
    Stop(player);
    SetStatus(player, "Stopped.");
  }
}

void OnLoadEvent(const NuiEvent& event) {
  if (event.type != "click") return;
  if (event.element == "ld_close") NuiClose(event.player, kLoadWindow);
  else if (event.element == "ld") LoadEntry(event.player, event.array_index);
}

std::vector<std::string> AnimEditCommand(void* creature, const std::string&) {
  Open(Field<uint32_t>(creature, ee::kObjectId));
  return {};
}

}  // namespace

void InstallAnimEditor() {
  if (!AddNuiWindow(kWindow, &OnEvent) || !AddNuiWindow(kLoadWindow, &OnLoadEvent)) return;
  AddChatCommand("!animedit", &AnimEditCommand);
}
