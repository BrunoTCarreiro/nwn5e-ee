// The VFX editor (owner, 2026-10-25: "is there a way that you can make a small
// vfx editor where I can make new or edit existing VFX to make new ones?" -
// "Something that can be used in game would be ideal"): `!vfxedit` opens a
// window with one effect - its emitters and each emitter's parameters (the
// ASCII model emitter's: update, render, blend, texture, position, rate,
// emission time, life, speed, spread, gravity, size, colour, alpha, spawn
// area, texture sheet, blur, bounce, the inherit / splat / bounce / random
// flags) - and plays it on the player character, the game's own renderer.
//
// How a preview works: the effect is written as an ASCII effect model
// (data/vfx_model.cpp) under a new name each time (n5vx0001, ...) into
// build\vfxedit, a folder registered with the resource manager watching for
// changes (re-read when the new file isn't found); visualeffects row 1600
// (data/rows/visualeffects.editor.txt, empty) is pointed at it in the loaded
// table (C2DA::SetCExoStringEntry - the client reads a visual effect's model
// columns from it each time it shows one), and the row is applied. A new name
// each time, so no cached model is shown.
//
// Save writes the draft (build\vfxedit\saved\<name>.vfx, key = value text) and
// its model (<name>.mdl) - to be added to the mod as a permanent row. Load
// lists the drafts and nwn5e's own effect models (n5_*.mdl, ASCII), and loads
// a game effect by its visualeffects row (its compiled model read where the
// player's game has it, data/vfx_model.cpp's binary reader - nothing of it
// kept in nwn5e but what the owner saves as a draft). The texture and the game
// effect are picked from dropdowns (owner: "I can't remember all the vfx and
// particle names"): the first !vfxedit reads every effect model the effects
// table names, once - the rows whose model has emitters, and the textures
// those emitters use.
//
// The VFX Bench (tools/vfx_bench, 0.42.0; owner, 2026-10-26: "can you come up
// with a webapp to edit and create new vfx? Kind of like the one for
// animations"), with `vfx_bench`: 3 s into a start (the client's frame; the
// first player update if there's no client), the game's effects with emitters
// are exported once to vfxedit\library\ (index.txt:
// row, label, model, node, type; game\<model>.vfx: each model's emitters as a
// draft; textures\<name>.tga / .dds: the particle textures, the player's own
// files, never in the repo) - again with `!vfxexport` - and nwn5e's own effect
// models (served\n5_*.mdl) as drafts in library\nwn5e\. The page asks the game
// to play or stop an effect through vfxedit\bench\request.vfx (a draft, its
// first line `request = play <n>` / `request = stop <n>`; looked at four times a
// second on the player's update), played as Preview on me does; the answer is
// bench\status.txt (`<n>` tab the status line), and bench\alive.txt says the
// game is running (every 2 s). A draft saved in saved\ (the page writes only
// drafts) gets its model there by WriteVfxModel - one writer.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "client_ui.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "data/vfx_model.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "nui.h"

namespace {

constexpr const char* kWindow = "nwn5evfxed";
constexpr const char* kLoadWindow = "nwn5evfxld";
constexpr int kPreviewRow = 1600;  // data/rows/visualeffects.editor.txt
constexpr const char* kAlias = "NWN5EVFX";
constexpr const char* kVarValue = "nwn5e_vfxed_value";
constexpr const char* kVarPick = "nwn5e_vfxed_pick";
constexpr const char* kPreviewTag = "nwn5e_vfxed";

using ExoStringCtorFn = void* (*)(void* string, const char* text);
using ExoStringDtorFn = void (*)(void* string);
using AliasAddFn = void (*)(void* list, void* alias, void* path);
using AddDirectoryFn = int (*)(void* resman, const void* name, uint32_t priority, int detect_changes, void* filter);
using RemoveDirectoryFn = int (*)(void* resman, const void* name);
using ExistsFn = int (*)(void* resman, const void* resref, uint16_t type, uint32_t* size);
using ResRefAssignFn = void* (*)(void* resref, const char* text);
using SetStringEntryFn = int (*)(void* table, int row, void* column, void* value);
using SetBlankEntryFn = int (*)(void* table, int row, void* column);

struct Api {
  ExoStringCtorFn string_ctor = reinterpret_cast<ExoStringCtorFn>(EngineExport(ee::kExoStringFromChars));
  ExoStringDtorFn string_dtor = reinterpret_cast<ExoStringDtorFn>(EngineExport(ee::kExoStringDestructor));
  AliasAddFn alias_add = reinterpret_cast<AliasAddFn>(EngineExport(ee::kAliasListAdd));
  AddDirectoryFn add_directory = reinterpret_cast<AddDirectoryFn>(EngineExport(ee::kAddResourceDirectory));
  RemoveDirectoryFn remove_directory = reinterpret_cast<RemoveDirectoryFn>(EngineExport(ee::kRemoveResourceDirectory));
  ExistsFn exists = reinterpret_cast<ExistsFn>(EngineExport(ee::kResManExists));
  ResRefAssignFn resref_assign = reinterpret_cast<ResRefAssignFn>(EngineExport(ee::kResRefAssignChars));
  SetStringEntryFn set_string = reinterpret_cast<SetStringEntryFn>(EngineExport(ee::kC2DASetStringEntry));
  SetBlankEntryFn set_blank = reinterpret_cast<SetBlankEntryFn>(EngineExport(ee::kC2DASetBlankEntry));
  void** exo_base = reinterpret_cast<void**>(EngineExport(ee::kExoBase));
  void** resman = reinterpret_cast<void**>(EngineExport(ee::kExoResMan));
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));

  bool Complete() const {
    return string_ctor && string_dtor && alias_add && add_directory && remove_directory && exists && resref_assign &&
           set_string && set_blank && exo_base && resman && rules;
  }
};

const Api& E() {
  static const Api api;
  return api;
}

struct Session {
  data::VfxEffect effect;
  int selected = 0;
  std::vector<std::string> textures;  // the texture dropdown's entries, as last shown
};
std::unordered_map<uint32_t, Session> g_sessions;  // by player, the engine's thread only
int g_models_written = 0;
bool g_registered = false;
std::vector<std::string> g_load_entries;  // the load window's rows: "draft:<path>" / "model:<path>"

// --- Files ---------------------------------------------------------------------

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  return directory.substr(0, directory.find_last_of(L"\\/"));
}

std::wstring Wide(const std::string& text) { return std::wstring(text.begin(), text.end()); }
std::string Narrow(const std::wstring& text) {
  std::string out;
  for (wchar_t c : text) out += c < 128 ? static_cast<char>(c) : '?';
  return out;
}

std::wstring EditorDirectory() { return DllDirectory() + L"\\vfxedit"; }
std::wstring SavedDirectory() { return EditorDirectory() + L"\\saved"; }

bool WriteFile(const std::wstring& path, const std::string& text) {
  std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!file) return false;
  file << text;
  return static_cast<bool>(file);
}

bool ReadFile(const std::wstring& path, std::string* text) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file) return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  *text = buffer.str();
  return true;
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

// --- The engine: the watched folder, the row ------------------------------------

void MakeString(void* buffer, const char* text) { E().string_ctor(buffer, text); }

bool RegisterFolder() {
  if (g_registered) return true;
  if (!E().Complete() || !*E().resman || !*E().exo_base) return false;
  CreateDirectoryW(EditorDirectory().c_str(), nullptr);
  CreateDirectoryW(SavedDirectory().c_str(), nullptr);
  void* aliases = Field<void*>(*E().exo_base, ee::kExoBaseAliasList);
  if (!aliases) return false;
  alignas(16) uint8_t alias[16] = {};
  alignas(16) uint8_t path[16] = {};
  MakeString(alias, kAlias);
  MakeString(path, Narrow(EditorDirectory()).c_str());
  E().alias_add(aliases, alias, path);  // frees both
  alignas(16) uint8_t name[16] = {};
  MakeString(name, (std::string(kAlias) + ":").c_str());
  const int added = E().add_directory(*E().resman, name, ee::kVfxEditorPriority, 1, nullptr);
  E().string_dtor(name);
  g_registered = true;
  Log("vfx editor: %s: = %s registered, watching for changes (%d)", kAlias, Narrow(EditorDirectory()).c_str(), added);
  return true;
}

bool ModelExists(const std::string& model) {
  alignas(16) uint8_t resref[32] = {};
  E().resref_assign(resref, model.c_str());
  uint32_t size = 0;
  return E().exists(*E().resman, resref, ee::kResTypeMdl, &size) != 0;
}

// The folder read again (a file written after it was registered and not seen).
void RefreshFolder() {
  alignas(16) uint8_t name[16] = {};
  MakeString(name, (std::string(kAlias) + ":").c_str());
  E().remove_directory(*E().resman, name);
  E().string_dtor(name);
  alignas(16) uint8_t again[16] = {};
  MakeString(again, (std::string(kAlias) + ":").c_str());
  E().add_directory(*E().resman, again, ee::kVfxEditorPriority, 1, nullptr);
  E().string_dtor(again);
}

void* VisualEffectsTable() {
  void* rules = *E().rules;
  void* arrays = rules ? Field<void*>(rules, ee::kRulesTwoDimArrays) : nullptr;
  return arrays ? Field<void*>(arrays, ee::kTwoDimArraysVisualEffects) : nullptr;
}

bool SetEntry(void* table, const char* column, const char* value) {
  alignas(16) uint8_t c[16] = {};
  alignas(16) uint8_t v[16] = {};
  MakeString(c, column);
  MakeString(v, value);
  return E().set_string(table, kPreviewRow, c, v) != 0;  // the strings are the callee's now
}

void BlankEntry(void* table, const char* column) {
  alignas(16) uint8_t c[16] = {};
  MakeString(c, column);
  E().set_blank(table, kPreviewRow, c);
}

// Row 1600 shows `model`: on the chosen node, as the effect's type.
bool PointPreviewRow(const std::string& model, const data::VfxEffect& effect) {
  void* table = VisualEffectsTable();
  if (!table) return false;
  static const char* const kRoot[] = {"Imp_Root_S_Node", "Imp_Root_M_Node", "Imp_Root_L_Node", "Imp_Root_H_Node"};
  bool ok = SetEntry(table, "Type_FD", effect.duration ? "D" : "F");
  for (const char* column : kRoot) {
    if (effect.attach == "root") ok = SetEntry(table, column, model.c_str()) && ok;
    else BlankEntry(table, column);
  }
  if (effect.attach == "head") ok = SetEntry(table, "Imp_HeadCon_Node", model.c_str()) && ok;
  else BlankEntry(table, "Imp_HeadCon_Node");
  if (effect.attach == "impact") ok = SetEntry(table, "Imp_Impact_Node", model.c_str()) && ok;
  else BlankEntry(table, "Imp_Impact_Node");
  return ok;
}

// --- The game's effects: the rows with emitters, the textures they use --------------

struct GameEffect {
  int row;
  std::string label;
  std::string model;
  std::string attach;
  bool duration;
};
struct GameScan {
  bool done = false;
  std::vector<GameEffect> effects;
  std::vector<std::string> textures;
  std::map<std::string, data::VfxEffect> models;  // by lower-case model: those with emitters (the bench's export)
};
GameScan g_scan;

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

bool ReadEffectModel(const std::string& model, data::VfxEffect* effect, std::string* why) {
  std::string bytes, error;
  if (!ReadGameResource(model, ee::kResTypeMdl, &bytes, why)) return false;
  if (data::ReadVfxBinaryModel(bytes, effect, &error) || data::ReadVfxAsciiModel(bytes, effect, &error)) return true;
  *why = error;
  return false;
}

void ScanGameEffects() {
  if (g_scan.done) return;
  g_scan.done = true;
  const std::vector<std::string> labels = GameTableColumn("visualeffects", "Label");
  const std::vector<std::string> types = GameTableColumn("visualeffects", "Type_FD");
  const std::vector<std::string> root = GameTableColumn("visualeffects", "Imp_Root_M_Node");
  const std::vector<std::string> impact = GameTableColumn("visualeffects", "Imp_Impact_Node");
  const std::vector<std::string> head = GameTableColumn("visualeffects", "Imp_HeadCon_Node");
  const auto at = [](const std::vector<std::string>& column, size_t row) {
    return row < column.size() ? column[row] : std::string();
  };
  std::map<std::string, bool> has_emitters;  // by model
  std::set<std::string> textures;
  for (size_t row = 0; row < labels.size(); ++row) {
    if (static_cast<int>(row) == kPreviewRow || labels[row].empty()) continue;
    std::string attach = "root", model = at(root, row);
    if (model.empty()) {
      model = at(impact, row);
      attach = "impact";
    }
    if (model.empty()) {
      model = at(head, row);
      attach = "head";
    }
    if (model.empty()) continue;
    auto known = has_emitters.find(Lower(model));
    if (known == has_emitters.end()) {
      data::VfxEffect effect;
      std::string why;
      const bool ok = ReadEffectModel(model, &effect, &why);
      if (ok) {
        for (const data::VfxEmitter& e : effect.emitters) {
          if (!e.texture.empty() && Lower(e.texture) != "null") textures.insert(Lower(e.texture));
        }
      }
      known = has_emitters.emplace(Lower(model), ok && !effect.emitters.empty()).first;
      if (known->second) g_scan.models[Lower(model)] = effect;
    }
    if (known->second) g_scan.effects.push_back({static_cast<int>(row), labels[row], model, attach, at(types, row) == "D"});
  }
  g_scan.textures.assign(textures.begin(), textures.end());
  Log("vfx editor: %zu game effects with emitters, %zu particle textures (read once)", g_scan.effects.size(),
      g_scan.textures.size());
}

// --- The window ----------------------------------------------------------------------

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

// The text fields of an emitter: bind, getter, setter.
struct TextField {
  const char* bind;
  std::function<std::string(const data::VfxEmitter&)> get;
  std::function<void(data::VfxEmitter&, const std::string&)> set;
};

TextField FloatField(const char* bind, float data::VfxEmitter::*member) {
  return {bind, [member](const data::VfxEmitter& e) { return Num(e.*member); },
          [member](data::VfxEmitter& e, const std::string& v) { e.*member = ToFloat(v, e.*member); }};
}

TextField ArrayField(const char* bind, float (data::VfxEmitter::*member)[3], int index) {
  return {bind, [member, index](const data::VfxEmitter& e) { return Num((e.*member)[index]); },
          [member, index](data::VfxEmitter& e, const std::string& v) {
            (e.*member)[index] = ToFloat(v, (e.*member)[index]);
          }};
}

const std::vector<TextField>& EmitterFields() {
  static const std::vector<TextField> kFields = {
      {"f_name", [](const data::VfxEmitter& e) { return e.name; },
       [](data::VfxEmitter& e, const std::string& v) { e.name = v.empty() ? "emitter" : v; }},
      ArrayField("f_px", &data::VfxEmitter::position, 0),
      ArrayField("f_py", &data::VfxEmitter::position, 1),
      ArrayField("f_pz", &data::VfxEmitter::position, 2),
      FloatField("f_rate", &data::VfxEmitter::birthrate),
      FloatField("f_emit", &data::VfxEmitter::emit_seconds),
      FloatField("f_life", &data::VfxEmitter::life),
      FloatField("f_mass", &data::VfxEmitter::mass),
      FloatField("f_vel", &data::VfxEmitter::velocity),
      FloatField("f_rvel", &data::VfxEmitter::randvel),
      FloatField("f_spread", &data::VfxEmitter::spread),
      FloatField("f_ax", &data::VfxEmitter::xsize),
      FloatField("f_ay", &data::VfxEmitter::ysize),
      FloatField("f_s0", &data::VfxEmitter::size_start),
      FloatField("f_s1", &data::VfxEmitter::size_end),
      ArrayField("f_c0r", &data::VfxEmitter::color_start, 0),
      ArrayField("f_c0g", &data::VfxEmitter::color_start, 1),
      ArrayField("f_c0b", &data::VfxEmitter::color_start, 2),
      ArrayField("f_c1r", &data::VfxEmitter::color_end, 0),
      ArrayField("f_c1g", &data::VfxEmitter::color_end, 1),
      ArrayField("f_c1b", &data::VfxEmitter::color_end, 2),
      FloatField("f_a0", &data::VfxEmitter::alpha_start),
      FloatField("f_a1", &data::VfxEmitter::alpha_end),
      {"f_grid", [](const data::VfxEmitter& e) { return std::to_string(e.grid); },
       [](data::VfxEmitter& e, const std::string& v) { e.grid = std::max(1, atoi(v.c_str())); }},
      {"f_frames", [](const data::VfxEmitter& e) { return std::to_string(e.frame_end); },
       [](data::VfxEmitter& e, const std::string& v) { e.frame_end = std::max(0, atoi(v.c_str())); }},
      FloatField("f_blur", &data::VfxEmitter::blur_length),
      FloatField("f_bco", &data::VfxEmitter::bounce_co),
  };
  return kFields;
}

const std::vector<std::string> kUpdates = {"Fountain", "Single", "Explosion"};
// The game matches these names exactly (PartEmitter::Initialize; "Aligned_to_World_z" drew nothing defined).
const std::vector<std::string> kRenders = {"Normal", "Motion_Blur", "Aligned_to_World_Z", "Billboard_to_World_Z",
                                           "Billboard_to_Local_Z", "Aligned_to_Particle_Dir", "Linked"};
const std::vector<std::string> kBlends = {"Normal", "Lighten", "Punch-Through"};
const std::vector<std::string> kAttaches = {"root", "head", "impact"};

std::string Next(const std::vector<std::string>& values, const std::string& current) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] == current) return values[(i + 1) % values.size()];
  }
  return values[0];
}

data::VfxEmitter DefaultEmitter() {
  data::VfxEmitter e;
  e.name = "sparks";
  e.texture = "fxpa_sphere";
  e.render = "Normal";
  e.blend = "Lighten";
  e.position[2] = 0.0f;  // on the node: the feet, the chest or over the head (0.42.1)
  e.birthrate = 20.0f;
  e.life = 1.0f;
  e.velocity = 1.5f;
  e.randvel = 0.5f;
  e.spread = 0.8f;
  e.mass = 0.5f;
  e.size_start = 0.08f;
  e.size_end = 0.02f;
  e.color_start[0] = 1.0f;
  e.color_start[1] = 0.8f;
  e.color_start[2] = 0.3f;
  e.color_end[0] = 1.0f;
  e.color_end[1] = 0.3f;
  e.color_end[2] = 0.1f;
  return e;
}

data::VfxEffect DefaultEffect() {
  data::VfxEffect effect;
  effect.name = "n5_new";
  effect.emitters.push_back(DefaultEmitter());
  return effect;
}

std::string Layout() {
  using namespace nui;
  const auto label = [](const char* text, float width) { return Sized(Label(Str(text)), width); };
  const auto field = [](const char* bind, float width) { return Sized(TextEdit(bind, "", bind, 40), width, 30.0f); };
  const std::string fields = Col(List({
      Row(List({label("Emitter name", 120.0f), field("f_name", 200.0f), Spacer()})),
      Row(List({Button("b_update", Bind("l_update")), Button("b_render", Bind("l_render")),
                Button("b_blend", Bind("l_blend"))})),
      Row(List({label("Texture", 120.0f), Sized(Combo(Bind("tex_list"), Bind("f_tex_sel")), 300.0f, 30.0f),
                Spacer()})),
      Row(List({label("Position x y z (m)", 150.0f), field("f_px", 70.0f), field("f_py", 70.0f), field("f_pz", 70.0f),
                Spacer()})),
      Row(List({label("Rate / s", 110.0f), field("f_rate", 70.0f), label("On for s (0 = always)", 170.0f),
                field("f_emit", 70.0f), Spacer()})),
      Row(List({label("Life (s)", 110.0f), field("f_life", 70.0f), label("Gravity (mass)", 170.0f),
                field("f_mass", 70.0f), Spacer()})),
      Row(List({label("Speed", 110.0f), field("f_vel", 70.0f), label("Random speed", 170.0f), field("f_rvel", 70.0f),
                Spacer()})),
      Row(List({label("Spread (rad)", 110.0f), field("f_spread", 70.0f), label("Area x y (cm)", 120.0f),
                field("f_ax", 60.0f), field("f_ay", 60.0f), Spacer()})),
      Row(List({label("Size start", 110.0f), field("f_s0", 70.0f), label("Size end", 170.0f), field("f_s1", 70.0f),
                Spacer()})),
      Row(List({label("Colour start r g b", 150.0f), field("f_c0r", 60.0f), field("f_c0g", 60.0f),
                field("f_c0b", 60.0f), Spacer()})),
      Row(List({label("Colour end r g b", 150.0f), field("f_c1r", 60.0f), field("f_c1g", 60.0f), field("f_c1b", 60.0f),
                Spacer()})),
      Row(List({label("Alpha start", 110.0f), field("f_a0", 70.0f), label("Alpha end", 170.0f), field("f_a1", 70.0f),
                Spacer()})),
      Row(List({label("Sheet grid", 110.0f), field("f_grid", 70.0f), label("Last frame", 170.0f),
                field("f_frames", 70.0f), Spacer()})),
      Row(List({label("Blur length", 110.0f), field("f_blur", 70.0f), label("Bounce", 170.0f), field("f_bco", 70.0f),
                Spacer()})),
      Row(List({Button("t_inherit", Bind("l_inherit")), Button("t_splat", Bind("l_splat")),
                Button("t_bounce", Bind("l_bounce")), Button("t_random", Bind("l_random"))})),
  }));
  return Window(
      Col(List({
          Label(Bind("status"), 44.0f),
          Row(List({label("Effect", 60.0f), Sized(TextEdit("e_name", "name", "e_name", 16), 180.0f, 30.0f),
                    Button("new", Str("New")), Button("load", Str("Load...")), Button("save", Str("Save"))})),
          Row(List({Button("type", Bind("l_type")), label("Seconds", 70.0f),
                    Sized(TextEdit("e_seconds", "", "e_seconds", 8), 70.0f, 30.0f), Button("attach", Bind("l_attach"))})),
          Row(List({Button("preview", Str("Preview on me")), Button("stop", Str("Stop")), Spacer()})),
          Row(List({label("Emitters", 80.0f), Button("em_add", Str("Add")), Button("em_copy", Str("Copy")),
                    Button("em_del", Str("Remove")), Spacer()})),
          Sized(ListView(Cell(Button("em", Bind("em_labels")), 0.0f, true), Bind("em_count"), 26.0f), 0.0f, 110.0f),
          Group(fields),
          Row(List({Spacer(), Button("close", Str("Close"), 100.0f)})),
      })),
      Str("nwn5e - VFX editor"), 40.0f, 40.0f, 620.0f, 880.0f);
}

// Many binds in one script.
void SetBinds(uint32_t player, const std::vector<std::pair<std::string, std::string>>& binds) {
  NuiSetTexts(player, kWindow, binds);
}

void SetStatus(uint32_t player, const std::string& line) { SetBinds(player, {{"status", line}}); }

std::string FlagLabel(const char* name, bool on) { return std::string(on ? "[x] " : "[ ] ") + name; }

void PushEmitterList(uint32_t player, const Session& session) {
  std::string labels = "[";
  for (size_t i = 0; i < session.effect.emitters.size(); ++i) {
    const data::VfxEmitter& e = session.effect.emitters[i];
    const std::string line = std::string(static_cast<int>(i) == session.selected ? "> " : "  ") +
                             std::to_string(i + 1) + "  " + e.name + "  (" + e.update + ", " + e.texture + ")";
    labels += (i ? "," : "") + nui::Str(line);
  }
  labels += "]";
  NuiSetJson(player, kWindow, "em_labels", labels);
  NuiSetJson(player, kWindow, "em_count", std::to_string(session.effect.emitters.size()));
}

// The texture dropdown: the game's particle textures, the selected emitter's
// own added when it isn't among them.
void PushTextures(uint32_t player, Session& session) {
  session.textures = g_scan.textures;
  int selected = 0;
  if (session.selected >= 0 && session.selected < static_cast<int>(session.effect.emitters.size())) {
    const std::string texture = Lower(session.effect.emitters[session.selected].texture);
    auto it = std::find(session.textures.begin(), session.textures.end(), texture);
    if (it == session.textures.end() && !texture.empty()) {
      session.textures.insert(session.textures.begin(), texture);
      it = session.textures.begin();
    }
    selected = it == session.textures.end() ? 0 : static_cast<int>(it - session.textures.begin());
  }
  std::string entries = "[";
  for (size_t i = 0; i < session.textures.size(); ++i) {
    entries += std::string(i ? "," : "") + "[" + nui::Str(session.textures[i]) + "," + std::to_string(i) + "]";
  }
  NuiSetJson(player, kWindow, "tex_list", entries + "]");
  NuiSetJson(player, kWindow, "f_tex_sel", std::to_string(selected));
}

void PushAll(uint32_t player, const Session& session) {
  const data::VfxEffect& effect = session.effect;
  std::vector<std::pair<std::string, std::string>> binds = {
      {"e_name", effect.name},
      {"e_seconds", Num(effect.seconds)},
      {"l_type", effect.duration ? "Type: stays (D)" : "Type: plays once (F)"},
      {"l_attach", "On: " + effect.attach},
  };
  if (session.selected >= 0 && session.selected < static_cast<int>(effect.emitters.size())) {
    const data::VfxEmitter& e = effect.emitters[session.selected];
    for (const TextField& field : EmitterFields()) binds.emplace_back(field.bind, field.get(e));
    binds.emplace_back("l_update", "Update: " + e.update);
    binds.emplace_back("l_render", "Render: " + e.render);
    binds.emplace_back("l_blend", "Blend: " + e.blend);
    binds.emplace_back("l_inherit", FlagLabel("follows the creature", e.inherit));
    binds.emplace_back("l_splat", FlagLabel("splats on the ground", e.splat));
    binds.emplace_back("l_bounce", FlagLabel("bounces", e.bounce));
    binds.emplace_back("l_random", FlagLabel("random frame", e.random));
  }
  SetBinds(player, binds);
  PushEmitterList(player, session);
  PushTextures(player, const_cast<Session&>(session));
}

// A text field's value.
std::string BindValue(uint32_t player, const std::string& bind, const char* window = kWindow) {
  return NuiGetText(player, window, bind);
}

void WatchFields(uint32_t player) {
  std::vector<std::string> binds = {"e_name", "e_seconds", "f_tex_sel"};
  for (const TextField& field : EmitterFields()) binds.push_back(field.bind);
  NuiWatch(player, kWindow, binds);
}

void Open(uint32_t player) {
  ScanGameEffects();
  Session& session = g_sessions[player];
  if (session.effect.emitters.empty()) session.effect = DefaultEffect();
  if (!NuiOpen(player, kWindow, Layout(),
               {{"status", "Edit the effect, then Preview on me. Fields take effect as you type."}})) {
    return;
  }
  PushAll(player, session);
  WatchFields(player);
}

data::VfxEmitter* Selected(Session& session) {
  if (session.selected < 0 || session.selected >= static_cast<int>(session.effect.emitters.size())) return nullptr;
  return &session.effect.emitters[session.selected];
}

// --- Preview, save, load ------------------------------------------------------------

// Plays `effect` on `player` (Preview on me, the bench's Play in game): the status line.
std::string PlayEffect(uint32_t player, const data::VfxEffect& effect) {
  if (effect.emitters.empty()) return "Add an emitter first.";
  if (!RegisterFolder()) return "The editor's folder could not be registered with the game (see nwn5e.log).";
  char model[16];
  sprintf_s(model, "n5vx%04d", ++g_models_written % 10000);
  const std::wstring path = EditorDirectory() + L"\\" + Wide(model) + L".mdl";
  if (!WriteFile(path, data::WriteVfxModel(effect, model))) return "Could not write " + Narrow(path) + ".";
  bool found = ModelExists(model);
  if (!found) {
    RefreshFolder();
    found = ModelExists(model);
    Log("vfx editor: %s not seen at once - folder read again: %s", model, found ? "found" : "still not found");
  }
  const bool pointed = PointPreviewRow(model, effect);
  if (!found || !pointed) {
    Log("vfx editor: preview %s - found %d, row pointed %d", model, found, pointed);
    return std::string("Preview failed: ") +
           (!found ? "the game doesn't see the new model" : "row 1600 could not be changed") + " (see nwn5e.log).";
  }
  char code[600];
  if (effect.duration) {
    sprintf_s(code,
              "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") "
              "RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }"
              "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(EffectVisualEffect(%d), \"%s\"), OBJECT_SELF, %.1f);",
              kPreviewTag, kPreviewRow, kPreviewTag, std::max(1.0f, effect.seconds));
  } else {
    sprintf_s(code, "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(%d), OBJECT_SELF);", kPreviewRow);
  }
  RunScriptChunk(code, player);
  Log("vfx editor: 0x%08X previews %s (%s, %zu emitters, on %s)", player, model, effect.duration ? "D" : "F",
      effect.emitters.size(), effect.attach.c_str());
  return std::string("Playing on you (model ") + model + ")" +
         (effect.duration ? " for " + Num(effect.seconds) + " s." : ".");
}

void StopEffect(uint32_t player) {
  char code[300];
  sprintf_s(code,
            "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") "
            "RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }",
            kPreviewTag);
  RunScriptChunk(code, player);
}

void Preview(uint32_t player, Session& session) {
  const std::string status = PlayEffect(player, session.effect);
  SetStatus(player, status.rfind("Playing", 0) == 0 ? status + " Change a field and press Preview again." : status);
}

void StopPreview(uint32_t player) {
  StopEffect(player);
  SetStatus(player, "Stopped.");
}

void Save(uint32_t player, Session& session) {
  std::string name = data::VfxResRef(session.effect.name);
  if (name.empty()) name = "n5_new";
  CreateDirectoryW(EditorDirectory().c_str(), nullptr);
  CreateDirectoryW(SavedDirectory().c_str(), nullptr);
  const std::wstring base = SavedDirectory() + L"\\" + Wide(name);
  const bool draft = WriteFile(base + L".vfx", data::WriteVfxDraft(session.effect));
  const bool model = WriteFile(base + L".mdl", data::WriteVfxModel(session.effect, name));
  Log("vfx editor: 0x%08X saved %s (draft %d, model %d)", player, name.c_str(), draft, model);
  SetStatus(player, draft && model ? "Saved as " + name + " (vfxedit\\saved\\" + name +
                                         ".vfx and .mdl). Ask Claude to add it to the mod as an effect."
                                   : "Could not save " + name + ".");
}

std::string LoadLayout() {
  using namespace nui;
  return Window(Col(List({
                    Label(Bind("ld_status"), 40.0f),
                    Row(List({Sized(Label(Str("Game effect")), 100.0f),
                              Sized(Combo(Bind("ld_rows"), Bind("ld_row_sel")), 400.0f, 30.0f),
                              Button("ld_game", Str("Load it"))})),
                    ListView(Cell(Button("ld", Bind("ld_labels")), 0.0f, true), Bind("ld_count"), 26.0f),
                    Row(List({Spacer(), Button("ld_close", Str("Close"), 100.0f)})),
                })),
                Str("nwn5e - load an effect"), 670.0f, 40.0f, 640.0f, 560.0f);
}

void OpenLoad(uint32_t player) {
  ScanGameEffects();
  g_load_entries.clear();
  std::string labels = "[";
  const auto add = [&](const std::string& entry, const std::string& label) {
    labels += (g_load_entries.empty() ? "" : ",") + nui::Str(label);
    g_load_entries.push_back(entry);
  };
  for (const std::wstring& file : FilesIn(SavedDirectory(), L"*.vfx")) {
    add("draft:" + Narrow(SavedDirectory() + L"\\" + file), "draft   " + Narrow(file.substr(0, file.size() - 4)));
  }
  const std::wstring served = DllDirectory() + L"\\served";
  for (const std::wstring& file : FilesIn(served, L"n5_*.mdl")) {
    add("model:" + Narrow(served + L"\\" + file), "nwn5e   " + Narrow(file.substr(0, file.size() - 4)));
  }
  labels += "]";
  if (!NuiOpen(player, kLoadWindow, LoadLayout(),
               {{"ld_status", std::to_string(g_load_entries.size()) + " of yours and nwn5e's below - or pick one of " +
                                  std::to_string(g_scan.effects.size()) + " game effects and Load it."}})) {
    return;
  }
  std::string rows = "[";
  for (size_t i = 0; i < g_scan.effects.size(); ++i) {
    rows += std::string(i ? "," : "") + "[" +
            nui::Str(std::to_string(g_scan.effects[i].row) + "  " + g_scan.effects[i].label) + "," + std::to_string(i) + "]";
  }
  NuiSetJson(player, kLoadWindow, "ld_rows", rows + "]");
  NuiSetJson(player, kLoadWindow, "ld_row_sel", "0");
  NuiSetJson(player, kLoadWindow, "ld_labels", labels);
  NuiSetJson(player, kLoadWindow, "ld_count", std::to_string(g_load_entries.size()));
}

void LoadEntry(uint32_t player, int index) {
  if (index < 0 || index >= static_cast<int>(g_load_entries.size())) return;
  const std::string& entry = g_load_entries[index];
  const bool draft = entry.rfind("draft:", 0) == 0;
  const std::string path = entry.substr(6);
  std::string text, error;
  data::VfxEffect effect;
  const bool ok = ReadFile(Wide(path), &text) &&
                  (draft ? data::ReadVfxDraft(text, &effect, &error) : data::ReadVfxAsciiModel(text, &effect, &error));
  Session& session = g_sessions[player];
  if (!ok) {
    SetStatus(player, "Could not load it: " + (error.empty() ? std::string("unreadable") : error));
    return;
  }
  if (!draft) effect.name += "_copy";  // a copy of nwn5e's: saving doesn't replace the original
  session.effect = effect;
  session.selected = 0;
  NuiClose(player, kLoadWindow);
  PushAll(player, session);
  SetStatus(player, "Loaded " + effect.name + " (" + std::to_string(effect.emitters.size()) +
                        " emitters). Preview on me to see it.");
  Log("vfx editor: 0x%08X loaded %s", player, path.c_str());
}

// --- The VFX Bench (tools/vfx_bench) -----------------------------------------------------

// 2: lasting effects' emitters no longer cut short (0.42.0); 3: emitters read
// from the geometry, not an animation's copy (0.42.1); 4: placed in the model with
// their parents, and turned (orientation), with fps and particle_rot (0.42.2); 6: the nodes they hang under and the motion keys (0.42.3);
// 7: twosidedtex and deadspace (0.42.4); 8: point to point and its targets (0.42.5); 9: chunk
// models (0.42.6).
constexpr int kLibraryFormat = 9;
constexpr uint16_t kResTypeTga = 3;
constexpr uint16_t kResTypeDds = 2033;
constexpr uint64_t kBenchCheckMs = 250;
constexpr uint64_t kAliveMs = 2000;

std::wstring LibraryDirectory() { return EditorDirectory() + L"\\library"; }
std::wstring BenchDirectory() { return EditorDirectory() + L"\\bench"; }

bool FileStamp(const std::wstring& path, uint64_t* stamp) {
  WIN32_FILE_ATTRIBUTE_DATA info;
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) return false;
  *stamp = (static_cast<uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) | info.ftLastWriteTime.dwLowDateTime;
  return true;
}

// The game's effects for the page: its effects table's rows with emitters, each
// model's emitters as a draft, the particle textures (the player's own files).
// Once (index.txt of this format there), or again when `force`d.
std::string ExportLibrary(bool force) {
  const std::wstring library = LibraryDirectory();
  const std::wstring index_path = library + L"\\index.txt";
  const std::string header = "# nwn5e vfx library " + std::to_string(kLibraryFormat);
  std::string existing;
  if (!force && ReadFile(index_path, &existing) && existing.rfind(header, 0) == 0) return {};
  const uint64_t started = GetTickCount64();
  ScanGameEffects();
  CreateDirectoryW(EditorDirectory().c_str(), nullptr);
  CreateDirectoryW(library.c_str(), nullptr);
  CreateDirectoryW((library + L"\\game").c_str(), nullptr);
  CreateDirectoryW((library + L"\\textures").c_str(), nullptr);
  int drafts = 0;
  for (const auto& entry : g_scan.models) {
    data::VfxEffect effect = entry.second;
    effect.name = entry.first;
    if (WriteFile(library + L"\\game\\" + Wide(entry.first) + L".vfx", data::WriteVfxDraft(effect))) ++drafts;
  }
  int textures = 0;
  std::string missing;
  for (const std::string& texture : g_scan.textures) {
    std::string bytes, why;
    const bool tga = ReadGameResource(texture, kResTypeTga, &bytes, &why);
    if (!tga && !ReadGameResource(texture, kResTypeDds, &bytes, &why)) {
      missing += (missing.empty() ? "" : ", ") + texture;
      continue;
    }
    if (WriteFile(library + L"\\textures\\" + Wide(texture) + (tga ? L".tga" : L".dds"), bytes)) ++textures;
  }
  std::string index = header + " - the game's effects with emitters: row, label, model, node, type (written by nwn5e)\n";
  for (const GameEffect& effect : g_scan.effects) {
    index += std::to_string(effect.row) + "\t" + effect.label + "\t" + Lower(effect.model) + "\t" + effect.attach + "\t" +
             (effect.duration ? "D" : "F") + "\n";
  }
  WriteFile(index_path, index);  // last: an export cut short is done again
  Log("vfx bench: exported %zu game effects (%d models as drafts), %d of %zu particle textures to vfxedit\\library "
      "in %llu ms%s%s",
      g_scan.effects.size(), drafts, textures, g_scan.textures.size(),
      static_cast<unsigned long long>(GetTickCount64() - started), missing.empty() ? "" : " - not found: ",
      missing.c_str());
  return std::to_string(g_scan.effects.size()) + " game effects, " + std::to_string(textures) + " textures";
}

// nwn5e's own effect models (served\n5_*.mdl, ASCII) as drafts: library\nwn5e\.
int ExportNwn5eModels() {
  const std::wstring served = DllDirectory() + L"\\served";
  const std::wstring out = LibraryDirectory() + L"\\nwn5e";
  CreateDirectoryW(EditorDirectory().c_str(), nullptr);
  CreateDirectoryW(LibraryDirectory().c_str(), nullptr);
  CreateDirectoryW(out.c_str(), nullptr);
  int written = 0;
  for (const std::wstring& file : FilesIn(served, L"n5_*.mdl")) {
    std::string text, error, old;
    data::VfxEffect effect;
    if (!ReadFile(served + L"\\" + file, &text) || !data::ReadVfxAsciiModel(text, &effect, &error) ||
        effect.emitters.empty()) {
      continue;
    }
    effect.name = Narrow(file.substr(0, file.size() - 4));
    const std::string draft = data::WriteVfxDraft(effect);
    const std::wstring path = out + L"\\" + file.substr(0, file.size() - 4) + L".vfx";
    if (ReadFile(path, &old) && old == draft) continue;
    if (WriteFile(path, draft)) ++written;
  }
  return written;
}

// A draft saved by the page gets its model (saved\<name>.mdl) when it has none or an older one.
void WriteSavedModels() {
  for (const std::wstring& file : FilesIn(SavedDirectory(), L"*.vfx")) {
    const std::wstring base = SavedDirectory() + L"\\" + file.substr(0, file.size() - 4);
    uint64_t draft_stamp = 0, model_stamp = 0;
    if (!FileStamp(base + L".vfx", &draft_stamp)) continue;
    if (FileStamp(base + L".mdl", &model_stamp) && model_stamp >= draft_stamp) continue;
    std::string text, error;
    data::VfxEffect effect;
    const std::string name = data::VfxResRef(Narrow(file.substr(0, file.size() - 4)));
    if (name.empty() || !ReadFile(base + L".vfx", &text) || !data::ReadVfxDraft(text, &effect, &error)) continue;
    if (WriteFile(base + L".mdl", data::WriteVfxModel(effect, name))) {
      Log("vfx bench: saved\\%s.mdl written from its draft", name.c_str());
    }
  }
}

struct BenchLink {
  bool exported = false;
  bool primed = false;
  uint64_t next = 0;
  uint64_t alive = 0;
  uint64_t seen = 0;  // the request file's write time last handled
};
BenchLink g_bench;

// The page's request: `request = play <n>` / `request = stop <n>` on its first lines.
void HandleRequest(uint32_t player, const std::string& text) {
  std::string kind, serial;
  std::istringstream in(text);
  std::string line;
  for (int i = 0; i < 5 && std::getline(in, line); ++i) {
    const size_t at = line.find("request");
    const size_t eq = line.find('=');
    if (at == std::string::npos || eq == std::string::npos || eq < at) continue;
    std::istringstream words(line.substr(eq + 1));
    words >> kind >> serial;
    break;
  }
  std::string status;
  if (kind == "stop") {
    StopEffect(player);
    status = "Stopped.";
  } else if (kind == "play") {
    data::VfxEffect effect;
    std::string error;
    status = data::ReadVfxDraft(text, &effect, &error) ? PlayEffect(player, effect) : "The draft didn't read: " + error;
  } else {
    status = "Unknown request.";
  }
  CreateDirectoryW(BenchDirectory().c_str(), nullptr);
  WriteFile(BenchDirectory() + L"\\status.txt", serial + "\t" + status + "\n");
  Log("vfx bench: request %s %s - %s", kind.c_str(), serial.c_str(), status.c_str());
}

void ExportOnce() {
  if (g_bench.exported) return;
  g_bench.exported = true;
  ExportLibrary(false);
  const int drafts = ExportNwn5eModels();
  if (drafts) Log("vfx bench: %d of nwn5e's effect models as drafts in vfxedit\\library\\nwn5e", drafts);
}

// The client's frame: the export 3 s into a start (at the main menu, before a game).
void ExportTick() {
  static uint64_t first = 0;
  if (g_bench.exported) return;
  const uint64_t now = GetTickCount64();
  if (!first) first = now;
  if (now - first >= 3000) ExportOnce();
}

void BenchUpdate(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint64_t now = GetTickCount64();
  if (now < g_bench.next) return;
  g_bench.next = now + kBenchCheckMs;
  const uint32_t player = Field<uint32_t>(creature, ee::kObjectId);
  ExportOnce();
  if (now >= g_bench.alive) {
    g_bench.alive = now + kAliveMs;
    CreateDirectoryW(EditorDirectory().c_str(), nullptr);
    CreateDirectoryW(BenchDirectory().c_str(), nullptr);
    WriteFile(BenchDirectory() + L"\\alive.txt", "nwn5e is running\n");
    WriteSavedModels();
  }
  uint64_t stamp = 0;
  const std::wstring request = BenchDirectory() + L"\\request.vfx";
  const bool there = FileStamp(request, &stamp);
  if (!g_bench.primed) {  // a request from before this start isn't played
    g_bench.primed = true;
    g_bench.seen = there ? stamp : 0;
    return;
  }
  if (!there || stamp == g_bench.seen) return;
  g_bench.seen = stamp;
  std::string text;
  if (ReadFile(request, &text)) HandleRequest(player, text);
}

std::vector<std::string> VfxExportCommand(void*, const std::string&) {
  const std::string done = ExportLibrary(true);
  const int drafts = ExportNwn5eModels();
  return {"nwn5e: VFX Bench library written again (" + done + "; " + std::to_string(drafts) +
          " of nwn5e's effects changed) - press Reload in the bench."};
}

// --- Events -----------------------------------------------------------------------------

void OnEvent(const NuiEvent& event) {
  const uint32_t player = event.player;
  Session& session = g_sessions[player];
  if (session.effect.emitters.empty() && session.effect.name.empty()) session.effect = DefaultEffect();
  data::VfxEmitter* e = Selected(session);
  if (event.type == "watch" && event.element == "f_tex_sel") {
    const int index = NuiGetInt(player, kWindow, "f_tex_sel");
    if (e && index >= 0 && index < static_cast<int>(session.textures.size())) {
      e->texture = session.textures[index];
      PushEmitterList(player, session);
    }
    return;
  }
  if (event.type == "watch") {
    const std::string value = BindValue(player, event.element);
    if (event.element == "e_name") {
      session.effect.name = value;
      return;
    }
    if (event.element == "e_seconds") {
      session.effect.seconds = std::max(0.1f, ToFloat(value, session.effect.seconds));
      return;
    }
    if (!e) return;
    for (const TextField& field : EmitterFields()) {
      if (event.element != field.bind) continue;
      field.set(*e, value);
      if (event.element == "f_name") PushEmitterList(player, session);
      return;
    }
    return;
  }
  if (event.type != "click") return;
  const std::string& id = event.element;
  if (id == "close") {
    NuiClose(player, kWindow);
  } else if (id == "new") {
    session.effect = DefaultEffect();
    session.selected = 0;
    PushAll(player, session);
    SetStatus(player, "A new effect with one emitter.");
  } else if (id == "load") {
    OpenLoad(player);
  } else if (id == "save") {
    Save(player, session);
  } else if (id == "preview") {
    Preview(player, session);
  } else if (id == "stop") {
    StopPreview(player);
  } else if (id == "type") {
    session.effect.duration = !session.effect.duration;
    PushAll(player, session);
  } else if (id == "attach") {
    session.effect.attach = Next(kAttaches, session.effect.attach);
    PushAll(player, session);
  } else if (id == "em" && event.array_index >= 0 &&
             event.array_index < static_cast<int>(session.effect.emitters.size())) {
    session.selected = event.array_index;
    PushAll(player, session);
  } else if (id == "em_add") {
    session.effect.emitters.push_back(DefaultEmitter());
    session.selected = static_cast<int>(session.effect.emitters.size()) - 1;
    PushAll(player, session);
  } else if (id == "em_copy" && e) {
    data::VfxEmitter copy = *e;
    copy.name += "2";
    session.effect.emitters.push_back(copy);
    session.selected = static_cast<int>(session.effect.emitters.size()) - 1;
    PushAll(player, session);
  } else if (id == "em_del" && e) {
    session.effect.emitters.erase(session.effect.emitters.begin() + session.selected);
    session.selected = std::max(0, std::min(session.selected, static_cast<int>(session.effect.emitters.size()) - 1));
    PushAll(player, session);
  } else if (e && id == "b_update") {
    e->update = Next(kUpdates, e->update);
    PushAll(player, session);
  } else if (e && id == "b_render") {
    e->render = Next(kRenders, e->render);
    PushAll(player, session);
  } else if (e && id == "b_blend") {
    e->blend = Next(kBlends, e->blend);
    PushAll(player, session);
  } else if (e && (id == "t_inherit" || id == "t_splat" || id == "t_bounce" || id == "t_random")) {
    bool& flag = id == "t_inherit" ? e->inherit : id == "t_splat" ? e->splat : id == "t_bounce" ? e->bounce : e->random;
    flag = !flag;
    PushAll(player, session);
  }
}

// A game effect picked from the dropdown: its model (the root, impact or head
// node's), read where the player's game has it.
void LoadGameRow(uint32_t player) {
  const int index = NuiGetInt(player, kLoadWindow, "ld_row_sel");
  if (index < 0 || index >= static_cast<int>(g_scan.effects.size())) {
    NuiSetText(player, kLoadWindow, "ld_status", "Pick a game effect first.");
    return;
  }
  const GameEffect& game = g_scan.effects[index];
  const int row = game.row;
  const std::string& model = game.model;
  const std::string& attach = game.attach;
  data::VfxEffect effect;
  std::string why;
  if (!ReadEffectModel(model, &effect, &why)) {
    NuiSetText(player, kLoadWindow, "ld_status", "Couldn't load " + model + ": " + why + ".");
    return;
  }
  effect.name = "fx" + std::to_string(row);
  effect.attach = attach.empty() ? "root" : attach;
  effect.duration = game.duration;
  Session& session = g_sessions[player];
  session.effect = effect;
  session.selected = 0;
  NuiClose(player, kLoadWindow);
  PushAll(player, session);
  SetStatus(player, "Loaded game effect " + std::to_string(row) + " (" + model + ", " +
                        std::to_string(effect.emitters.size()) + " emitters) as " + effect.name +
                        ". Preview on me, change it, Save it under a new name.");
  Log("vfx editor: 0x%08X loaded game row %d (%s, %zu emitters)", player, row, model.c_str(), effect.emitters.size());
}

void OnLoadEvent(const NuiEvent& event) {
  if (event.type != "click") return;
  if (event.element == "ld_close") NuiClose(event.player, kLoadWindow);
  else if (event.element == "ld_game") LoadGameRow(event.player);
  else if (event.element == "ld") LoadEntry(event.player, event.array_index);
}

std::vector<std::string> VfxEditCommand(void* creature, const std::string&) {
  Open(Field<uint32_t>(creature, ee::kObjectId));
  return {};
}

}  // namespace

void InstallVfxEditor() {
  if (!E().Complete()) {
    Log("SKIP vfx editor: an engine export is missing (resource folders, the 2DA setter, the rules)");
    return;
  }
  if (!AddNuiWindow(kWindow, &OnEvent) || !AddNuiWindow(kLoadWindow, &OnLoadEvent)) return;
  AddChatCommand("!vfxedit", &VfxEditCommand);
  Log("vfx editor: !vfxedit (preview row %d, models in vfxedit\\)", kPreviewRow);
  if (g_config.vfx_bench && AddCreatureUpdateListener(&BenchUpdate)) {
    AddClientTickListener(&ExportTick);
    AddChatCommand("!vfxexport", &VfxExportCommand);
    Log("vfx bench: the game's effects exported to vfxedit\\library once; plays the bench's requests "
        "(vfxedit\\bench); !vfxexport (vfx_bench)");
  }
}
