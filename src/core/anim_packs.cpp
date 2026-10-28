// Animation recipes at startup (owner, 2026-10-25: "Let's try recipe" - "n5test
// works"; 0.51.0: Mocap mode's keys recipes, built from their own keys per
// library - data::KeyedAnimClip): each recipe (data\anims\*.recipe, nwn5e's; animedit\recipes\*.recipe,
// the owner's drafts from !animedit or the Animation Recipe Bench, and
// *.recipe.txt, a draft winning over a same-named one)
// becomes an animation in the "animation packs" written into the served folder
// before it is registered:
// - every body skeleton (p + m/f + a/d/e/g/h/o + 0/2, ASCII models: pmh0 ...)
//   is served as a copy whose supermodel is its library's pack (pmh0 -> n5_ba);
// - each pack (n5_ba, n5_fa, n5_da, n5_dfa and the large-build '2's) holds its
//   library's bone tree and the recipes' animations, built from that library's
//   own animations (or its supermodel chain's: a_fa falls back on a_ba ...),
//   and points on at the library - so every other animation still plays.
// All of it is built from the player's own game files at startup; nothing of
// the game's is in nwn5e. data/anim_recipe.cpp does the reading and the maths.
// The first test (owner: "n5test works") was this, from a script, through the
// game's development folder. `animation_recipes`.
// The libraries read are also copied to animedit\libraries\ (0.24.2; owner:
// "the saving closing and opening the game just to get a preview is not great.
// could we get a webapp working"): the Animation Recipe Bench, a local web app
// (tools/recipe_bench/), reads them from there and previews a recipe at once.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "data/anim_recipe.h"
#include "data/vfx_model.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "log.h"

namespace {

std::map<std::string, std::string> g_built;  // recipe name -> its text as built at startup
// The layers the engine plays over a walk while a weapon is held (literal names in
// nwmain: walk_swordr ...; animroot torso_g, keying the torso, neck, head and arms).
// A recipe on an animation that has them also builds <name>_<hold> from
// <base>_<hold> - the same lean, so a crouch walk leans armed too (owner,
// 2026-10-25: "the PC doesn't lean forward" - the layer put the torso back up).
constexpr const char* kHolds[] = {"swordr", "swordl", "shieldl", "bowl"};
std::vector<std::string> g_animation_names;  // a_ba's chain's animations, for !animedit's dropdowns

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  return directory.substr(0, directory.find_last_of(L"\\/"));
}

std::string Narrow(const std::wstring& text) {
  std::string out;
  for (wchar_t c : text) out += c < 128 ? static_cast<char>(c) : '?';
  return out;
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

bool ReadFile(const std::wstring& path, std::string* text) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file) return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  *text = buffer.str();
  return true;
}

bool WriteFile(const std::wstring& path, const std::string& text) {
  std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!file) return false;
  file << text;
  return static_cast<bool>(file);
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

// A file's last write time, for "the newest copy of a recipe wins".
uint64_t WriteTime(const std::wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
  return (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

}  // namespace

// The recipe files in a folder: <name>.recipe (nwn5e's, !animedit's, the
// Animation Recipe Bench's) and <name>.recipe.txt - what Notepad or a browser
// download makes of one (with " (1)" on a second copy).
std::vector<std::wstring> RecipeFiles(const std::wstring& directory) {
  std::vector<std::wstring> files = FilesIn(directory, L"*.recipe");
  for (const std::wstring& file : FilesIn(directory, L"*.recipe*.txt")) files.push_back(file);
  return files;
}

namespace {

// The recipes, by name: nwn5e's, then the owner's drafts over them. Within a
// folder the newest file with a name wins (the page's .recipe.txt or
// !animedit's .recipe, whichever was saved last).
std::map<std::string, data::AnimRecipe> ReadRecipes() {
  std::map<std::string, data::AnimRecipe> recipes;
  for (const std::wstring& directory : {DllDirectory() + L"\\data\\anims", DllDirectory() + L"\\animedit\\recipes"}) {
    std::map<std::string, uint64_t> newest;
    for (const std::wstring& file : RecipeFiles(directory)) {
      std::string text, error;
      data::AnimRecipe recipe;
      if (!ReadFile(directory + L"\\" + file, &text) || !data::ReadAnimRecipe(text, &recipe, &error)) {
        Log("anim packs: SKIP %s: %s", Narrow(file).c_str(), error.c_str());
        continue;
      }
      recipe.name = data::VfxResRef(recipe.name, 32);
      if (recipe.name.empty()) continue;
      const uint64_t written = WriteTime(directory + L"\\" + file);
      const auto seen = newest.find(recipe.name);
      if (seen != newest.end() && seen->second > written) continue;
      newest[recipe.name] = written;
      recipes[recipe.name] = recipe;
    }
  }
  return recipes;
}

// The libraries, read once each.
struct Library {
  std::string bytes;
  data::AnimLibrary info;
  bool ok = false;
};
std::map<std::string, Library> g_libraries;

const Library& LoadLibrary(const std::string& name) {
  auto it = g_libraries.find(name);
  if (it != g_libraries.end()) return it->second;
  Library& library = g_libraries[name];
  std::string why, error;
  library.ok = ReadGameResource(name, ee::kResTypeMdl, &library.bytes, &why) &&
               data::ReadAnimLibrary(library.bytes, &library.info, &error);
  if (!library.ok) Log("anim packs: library %s not read (%s%s)", name.c_str(), why.c_str(), error.c_str());
  return library;
}

// An animation as `library` plays it: its own, else its supermodel chain's.
bool FindClip(const std::string& library, const std::string& name, data::AnimClip* clip) {
  std::string current = Lower(library);
  for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
    const Library& lib = LoadLibrary(current);
    if (!lib.ok) return false;
    if (std::find(lib.info.clip_names.begin(), lib.info.clip_names.end(), Lower(name)) != lib.info.clip_names.end()) {
      return data::ReadAnimClip(lib.bytes, name, clip);
    }
    current = Lower(lib.info.supermodel);
  }
  return false;
}

std::string PackName(const std::string& library) {
  return "n5_" + (library.rfind("a_", 0) == 0 ? library.substr(2) : library);
}

}  // namespace

// The libraries read this start, for the web page: animedit\libraries\<name>.mdl.
void ExportLibraries() {
  const std::wstring directory = DllDirectory() + L"\\animedit\\libraries";
  CreateDirectoryW((DllDirectory() + L"\\animedit").c_str(), nullptr);
  CreateDirectoryW(directory.c_str(), nullptr);
  int written = 0;
  for (const auto& entry : g_libraries) {
    if (!entry.second.ok) continue;
    const std::wstring path = directory + L"\\" + std::wstring(entry.first.begin(), entry.first.end()) + L".mdl";
    if (WriteFile(path, entry.second.bytes)) ++written;
  }
  if (written) Log("anim packs: %d animation libraries copied to animedit\\libraries (for the web page)", written);
}

// data_serving's BuildServedTables, into the served folder before it is registered.
void GenerateAnimationPacks(const std::wstring& served) {
  g_built.clear();
  g_animation_names.clear();
  if (!g_config.animation_recipes) return;
  // The humanoid animations (a_ba and its supermodels), for !animedit.
  std::string library = "a_ba";
  for (int depth = 0; depth < 8 && !library.empty(); ++depth) {
    const Library& lib = LoadLibrary(library);
    if (!lib.ok) break;
    for (const std::string& name : lib.info.clip_names) {
      if (std::find(g_animation_names.begin(), g_animation_names.end(), name) == g_animation_names.end()) {
        g_animation_names.push_back(name);
      }
    }
    library = Lower(lib.info.supermodel);
  }
  std::sort(g_animation_names.begin(), g_animation_names.end());
  const std::map<std::string, data::AnimRecipe> recipes = ReadRecipes();
  if (recipes.empty()) {
    for (const char* library : {"a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"}) LoadLibrary(library);
    ExportLibraries();
    g_libraries.clear();
    return;
  }
  // The skeletons and their libraries.
  std::map<std::string, std::string> packs;  // library -> pack
  int skeletons = 0;
  static const char kRaces[] = {'a', 'd', 'e', 'g', 'h', 'o'};
  for (char gender : {'m', 'f'}) {
    for (char race : kRaces) {
      for (char build : {'0', '2'}) {
        const std::string name = std::string("p") + gender + race + build;
        std::string text, why;
        if (!ReadGameResource(name, ee::kResTypeMdl, &text, &why) || text.size() < 16 || text[0] == '\0') continue;
        const size_t at = Lower(text).find("setsupermodel");
        if (at == std::string::npos) continue;
        std::istringstream line(text.substr(at, text.find('\n', at) - at));
        std::string keyword, self, library;
        line >> keyword >> self >> library;
        library = Lower(library);
        if (library.empty() || library == "null" || !LoadLibrary(library).ok) continue;
        const std::string pack = PackName(library);
        const size_t end = text.find_first_of("\r\n", at);
        text.replace(at, (end == std::string::npos ? text.size() : end) - at, "setsupermodel " + self + " " + pack);
        if (WriteFile(served + L"\\" + std::wstring(name.begin(), name.end()) + L".mdl", text)) {
          packs[library] = pack;
          ++skeletons;
        }
      }
    }
  }
  // The packs.
  int animations = 0;
  for (const auto& entry : packs) {
    const Library& library = LoadLibrary(entry.first);
    std::vector<data::AnimClip> clips;
    for (const auto& recipe : recipes) {
      if (recipe.second.keyed) {  // Mocap mode: the library's own keys (no weapon layers)
        data::AnimClip keyed;
        if (data::KeyedAnimClip(recipe.second, entry.first, library.info.bones, &keyed)) {
          clips.push_back(keyed);
        } else {
          Log("anim packs: %s - no keys for %s (save it again in the bench)", recipe.first.c_str(), entry.first.c_str());
        }
        continue;
      }
      data::AnimClip base, mix;
      if (!FindClip(entry.first, recipe.second.base, &base)) {
        Log("anim packs: %s - %s has no %s", recipe.first.c_str(), entry.first.c_str(), recipe.second.base.c_str());
        continue;
      }
      const bool has_mix = !recipe.second.mix.empty() && FindClip(entry.first, recipe.second.mix, &mix);
      clips.push_back(data::DeriveAnimClip(recipe.second, base, has_mix ? &mix : nullptr, &library.info.bones));
      for (const char* hold : kHolds) {
        data::AnimClip layer, layer_mix;
        if (!FindClip(entry.first, recipe.second.base + "_" + hold, &layer)) continue;
        data::AnimRecipe companion = recipe.second;
        companion.name = recipe.second.name + "_" + hold;
        const bool layer_has_mix = has_mix && FindClip(entry.first, recipe.second.mix + "_" + hold, &layer_mix);
        if (!layer_has_mix) companion.mix.clear();
        clips.push_back(data::DeriveAnimClip(companion, layer, layer_has_mix ? &layer_mix : nullptr, &library.info.bones));
        g_built[companion.name] = data::WriteAnimRecipe(companion);
      }
    }
    if (clips.empty()) continue;
    const std::string pack = entry.second;
    if (WriteFile(served + L"\\" + std::wstring(pack.begin(), pack.end()) + L".mdl",
                  data::WriteAnimPack(pack, entry.first, library.info.bones, clips))) {
      animations += static_cast<int>(clips.size());
    }
  }
  for (const auto& recipe : recipes) g_built[recipe.first] = data::WriteAnimRecipe(recipe.second);
  ExportLibraries();
  g_libraries.clear();  // the libraries' bytes aren't kept
  Log("anim packs: %zu recipes -> %zu packs (%d animations), %d skeletons pointed at them (animation_recipes)",
      recipes.size(), packs.size(), animations, skeletons);
}

// !animedit's dropdowns: the humanoid animations a recipe can use.
const std::vector<std::string>& AnimationNames() { return g_animation_names; }

// !animedit: was `name` built at startup, and from which recipe text?
bool AnimationRecipeBuilt(const std::string& name, std::string* text) {
  const auto it = g_built.find(name);
  if (it == g_built.end()) return false;
  if (text) *text = it->second;
  return true;
}
