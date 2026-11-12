// #158 (owner, 2026-11-12: "can we add a history to the chat input? since we're using it a lot for commands, having a
// way to repeat commands without having to type them all over again would be great. like when the chat input is
// active, pressing up would cycle through the previous commands"): Up and Down in the chat bar walk the lines sent
// (data::ChatHistory), kept in nwn5e_chat_history.txt next to the DLL - the newest 50, so a restart keeps them.
// EE's chat bar keeps none; the engine's only history (DoConsoleHist, g_listHistory) is its developer console's.
//
// Static analysis of 89.8193.37 (nwmain.exe):
// - A text box's keys come to CGuiEdit::HandleKeyPress(int). CGuiSingleLineEdit's own comes first and keeps Enter
//   (13 or 10) for itself; every other key goes on to CGuiEdit's. Keys that aren't characters come as codes, made
//   by HandleWMKeyDownMessage from SDL's (0x4222B: 0x126 for Up, then Left 0x125, Right 0x127, Down 0x128) and
//   handed to CGuiMan::HandleKeyPress: 0x126 / 0x128 a line up / down (nothing in a one-line box), 0x125 / 0x127
//   the caret left / right, 0x124 home, 0x123 delete; 27 Escape, 13 Enter. 0.99.19 (owner, 2026-11-12: "up does
//   nothing in the chat"): 0.99.18 had Up / Down as 0x125 / 0x127 - Left / Right - reading the line and caret
//   calls the wrong way round.
// - A box's Enter callback: its owner at +0xA8, the function at +0xB0. The chat bar's owner is the in-game chat panel
//   (RTTI .?AVCGuiInGameChatPrompt@@), so only a box whose owner's class starts .?AVCGuiInGameChat is touched.
// - A box's text: CGuiEdit::GetText; set through its CAuroraStringWrapper (+0x80), as CPanelGenericEditBox::
//   SetEditText sets its box; the caret through the text object at +0x90 (vtable +0x160 get, +0x168 set - the left /
//   right keys' own calls, and Home's set to 0; +0x150 / +0x158 are the line's).
#include "chat_input.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "data/chat_history.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

constexpr const char* kEditKeyPress = "?HandleKeyPress@CGuiEdit@@MEAAHH@Z";
constexpr const char* kSingleLineKeyPress = "?HandleKeyPress@CGuiSingleLineEdit@@UEAAHH@Z";
constexpr const char* kEditGetText = "?GetText@CGuiEdit@@QEAA?AVCExoString@@XZ";
constexpr int kKeyUp = 0x126;
constexpr int kKeyDown = 0x128;
constexpr int kKeyEscape = 27;
constexpr size_t kCallbackOwner = 0xA8;
constexpr size_t kStringWrapper = 0x80;
constexpr size_t kTextObject = 0x90;
constexpr size_t kSetCaretSlot = 0x168 / sizeof(void*);
constexpr char kChatClassPrefix[] = ".?AVCGuiInGameChat";

using KeyPressFn = int (*)(void* edit, int key);
using GetTextFn = void* (*)(void* edit, void* out_exo_string);
using SetTextFn = void (*)(void* wrapper, const void* exo_string);
using ExoStringFn = void (*)(void* exo_string, const char* text);
using ExoStringDestroyFn = void (*)(void* exo_string);
using SetCaretFn = void (*)(void* text_object, int position);

KeyPressFn g_edit_key = nullptr;
KeyPressFn g_single_key = nullptr;
GetTextFn g_get_text = nullptr;
SetTextFn g_set_text = nullptr;
ExoStringFn g_make_string = nullptr;
ExoStringDestroyFn g_destroy_string = nullptr;
uintptr_t g_image = 0;
size_t g_image_size = 0;
data::ChatHistory g_history;
std::wstring g_file;
thread_local int t_single = 0;  // inside CGuiSingleLineEdit's handler: its Enter isn't CGuiEdit's to note
bool g_found = false;
int g_lines = 0;

bool LogSlot() { return g_lines < 100 && ++g_lines; }

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  const size_t slash = directory.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : directory.substr(0, slash);
}

bool InImage(uintptr_t address, size_t bytes) { return address >= g_image && address + bytes <= g_image + g_image_size; }

// The RTTI class name of one of the game's polymorphic objects ("....?AVCGuiInGameChatPrompt@@"), or nullptr: its
// vtable, the complete object locator before it and the type descriptor it names must all lie in the game's image.
const char* ClassName(const void* object) {
  if (!object) return nullptr;
  const uintptr_t vtable = *static_cast<const uintptr_t*>(object);
  if (!InImage(vtable - sizeof(void*), sizeof(void*))) return nullptr;
  const uintptr_t locator = *reinterpret_cast<const uintptr_t*>(vtable - sizeof(void*));
  if (!InImage(locator, 24)) return nullptr;
  const uint32_t descriptor = *reinterpret_cast<const uint32_t*>(locator + 12);  // its type descriptor (an RVA)
  const uintptr_t name = g_image + descriptor + 16;
  if (!InImage(name, sizeof kChatClassPrefix)) return nullptr;
  return reinterpret_cast<const char*>(name);
}

bool IsChatBox(void* edit) {
  void* owner = *reinterpret_cast<void**>(static_cast<uint8_t*>(edit) + kCallbackOwner);
  const char* name = ClassName(owner);
  const bool chat = name && strncmp(name, kChatClassPrefix, sizeof kChatClassPrefix - 1) == 0;
  if (chat && !g_found) {
    g_found = true;
    Log("chat: the chat bar's box found (its owner %.48s) - Up / Down recall its lines", name);
  }
  return chat;
}

std::string TextOf(void* edit) {
  alignas(16) uint8_t exo[32] = {};  // CExoString: the engine owns its buffer
  g_get_text(edit, exo);
  const char* text = *reinterpret_cast<const char**>(exo);
  std::string result = text ? text : "";
  g_destroy_string(exo);
  return result;
}

void SetTextOf(void* edit, const std::string& text) {
  alignas(16) uint8_t exo[32] = {};
  g_make_string(exo, text.c_str());
  g_set_text(static_cast<uint8_t*>(edit) + kStringWrapper, exo);
  g_destroy_string(exo);
  void* text_object = *reinterpret_cast<void**>(static_cast<uint8_t*>(edit) + kTextObject);
  if (text_object) {  // the caret after the last character, as typing it would leave it
    void** vtable = *static_cast<void***>(text_object);
    reinterpret_cast<SetCaretFn>(vtable[kSetCaretSlot])(text_object, static_cast<int>(text.size()));
  }
}

void SaveHistory() {
  FILE* file = nullptr;
  if (_wfopen_s(&file, g_file.c_str(), L"wb") != 0 || !file) {
    if (LogSlot()) Log("chat: nwn5e_chat_history.txt couldn't be written - the history lasts this session");
    return;
  }
  const std::string text = g_history.Save();
  fwrite(text.data(), 1, text.size(), file);
  fclose(file);
}

void LoadHistory() {
  FILE* file = nullptr;
  if (_wfopen_s(&file, g_file.c_str(), L"rb") != 0 || !file) return;  // none yet
  std::string text;
  char buffer[4096];
  size_t read = 0;
  while ((read = fread(buffer, 1, sizeof buffer, file)) > 0 && text.size() < (1u << 20)) text.append(buffer, read);
  fclose(file);
  g_history.Load(text);
}

// Up / Down: the box shows the next older / newer line. False: nothing to show - the key goes on to the box.
bool Walk(void* edit, int key) {
  std::string line;
  const bool moved = key == kKeyUp ? g_history.Older(TextOf(edit), &line) : g_history.Newer(&line);
  if (!moved) return false;
  SetTextOf(edit, line);
  return true;
}

void Remember(void* edit) {
  g_history.Add(TextOf(edit));
  SaveHistory();
}

int EditKeyPress(void* edit, int key) {
  const bool enter = (key == 13 || key == 10) && t_single == 0;
  if (edit && (key == kKeyUp || key == kKeyDown || key == kKeyEscape || enter) && IsChatBox(edit)) {
    if (key == kKeyUp || key == kKeyDown) {
      if (Walk(edit, key)) return 1;
    } else if (key == kKeyEscape) {
      g_history.ResetWalk();
    } else {
      Remember(edit);
    }
  }
  return g_edit_key(edit, key);
}

int SingleLineKeyPress(void* edit, int key) {
  if (edit && (key == 13 || key == 10) && IsChatBox(edit)) Remember(edit);  // before the box sends and empties it
  ++t_single;
  const int result = g_single_key(edit, key);
  --t_single;
  return result;
}

}  // namespace

void InstallChatInput() {
  g_image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_image);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(g_image + dos->e_lfanew);
  g_image_size = nt->OptionalHeader.SizeOfImage;
  g_get_text = reinterpret_cast<GetTextFn>(EngineExport(kEditGetText));
  g_set_text = reinterpret_cast<SetTextFn>(EngineExport(ee::kStringWrapperSetText));
  g_make_string = reinterpret_cast<ExoStringFn>(EngineExport(ee::kExoStringFromChars));
  g_destroy_string = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  if (!g_get_text || !g_set_text || !g_make_string || !g_destroy_string) {
    Log("SKIP chat history: a client function not found - the chat bar keeps no history");
    return;
  }
  g_file = DllDirectory() + L"\\nwn5e_chat_history.txt";
  LoadHistory();
  const bool hooked =
      HookExport("CGuiEdit::HandleKeyPress (chat history: Up / Down, Enter)", kEditKeyPress,
                 reinterpret_cast<void*>(&EditKeyPress), reinterpret_cast<void**>(&g_edit_key)) &&
      HookExport("CGuiSingleLineEdit::HandleKeyPress (chat history: Enter)", kSingleLineKeyPress,
                 reinterpret_cast<void*>(&SingleLineKeyPress), reinterpret_cast<void**>(&g_single_key));
  if (!hooked) {
    Log("SKIP chat history: the text boxes' keys aren't hooked - the chat bar keeps no history");
    return;
  }
  Log("chat: Up / Down in the chat bar recall the lines you sent (%zu kept from nwn5e_chat_history.txt, the newest %zu)",
      g_history.Lines().size(), data::ChatHistory::kMaxLines);
}
