#include "chat.h"

#include <ctype.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "hooks.h"
#include "log.h"
#include "rules/text.h"

namespace {

// CExoString as the engine lays it out; a by-value argument arrives as a
// pointer to the caller's copy (x64), which the callee frees.
struct ExoString {
  char* text;
  uint32_t length;
};

using SendChatFn = int (*)(void* message, uint8_t channel, uint32_t sender, const ExoString* text, uint32_t a,
                           const void* b);
SendChatFn g_send_chat = nullptr;
CodeRange g_typed_chat_code;  // HandlePlayerToServerChatMessage
std::vector<std::pair<std::string, ChatCommand>> g_commands;
bool g_hooked = false;
bool g_hook_failed = false;

// "!word args" (word in any case, leading spaces ignored), read before the
// engine frees the text.
ChatCommand ParseCommand(const ExoString* text, std::string* word_out, std::string* args) {
  if (!text || !text->text || text->length == 0) return nullptr;
  std::string line(text->text, strnlen(text->text, text->length));
  const size_t start = line.find_first_not_of(' ');
  if (start == std::string::npos || line[start] != '!') return nullptr;
  line = line.substr(start);
  const size_t word_end = line.find(' ');
  std::string word = line.substr(0, word_end);
  for (char& c : word) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (word_end != std::string::npos) {
    const size_t args_start = line.find_first_not_of(' ', word_end);
    if (args_start != std::string::npos) *args = line.substr(args_start);
  }
  for (const auto& entry : g_commands) {
    if (entry.first == word) {
      *word_out = word;
      return entry.second;
    }
  }
  return nullptr;
}

int SendServerToPlayerChatMessage(void* message, uint8_t channel, uint32_t sender, const ExoString* text, uint32_t a,
                                  const void* b) {
  ChatCommand command = nullptr;
  std::string word, args;
  if (g_typed_chat_code.Contains(reinterpret_cast<uintptr_t>(_ReturnAddress()))) {
    command = ParseCommand(text, &word, &args);
  }
  const int result = g_send_chat(message, channel, sender, text, a, b);
  if (!command) return result;
  void* creature = CreatureById(sender);
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).is_player) return result;
  const std::vector<std::string> lines = command(creature, args);
  for (const std::string& line : lines) TellPlayer(sender, line);
  if (g_config.log_attacks && !lines.empty()) {
    Log("chat: %s for 0x%08X: %s%s", word.c_str(), sender, lines[0].c_str(), lines.size() > 1 ? " ..." : "");
  }
  return result;
}

// An NWScript string literal (quotes and backslashes escaped).
std::string ScriptString(const std::string& text) {
  std::string out = "\"";
  for (char c : rules::ToWindows1252(text)) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

}  // namespace

bool AddChatCommand(const char* word, ChatCommand command) {
  if (!g_hooked && !g_hook_failed) {
    g_typed_chat_code = FunctionAt(EngineExport(ee::kHandlePlayerToServerChatMessage));
    g_hooked = g_typed_chat_code.Valid() &&
               HookExport("CNWSMessage::SendServerToPlayerChatMessage (chat commands)", ee::kSendServerToPlayerChatMessage,
                          reinterpret_cast<void*>(&SendServerToPlayerChatMessage), reinterpret_cast<void**>(&g_send_chat));
    g_hook_failed = !g_hooked;
    if (g_hook_failed) Log("SKIP chat commands: the chat functions were not found");
  }
  if (!g_hooked) return false;
  g_commands.emplace_back(word, command);
  return true;
}

void TellPlayer(uint32_t object_id, const std::string& line) {
  char head[64];
  sprintf_s(head, "SendMessageToPC(StringToObject(\"%x\"), ", object_id);
  RunScriptChunk((std::string(head) + ScriptString(line) + ");").c_str(), object_id);
}
