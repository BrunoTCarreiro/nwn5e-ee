#pragma once

#include <stdint.h>

#include <string>
#include <vector>

// Chat commands (PLAN.md C8, D3; port of 1.69's rules_chat.cpp): a player
// character typing "!word args" in any chat channel gets the command's
// answer as server messages; the line itself still goes through as normal
// chat. In EE a typed line reaches CNWSMessage::SendServerToPlayerChatMessage
// from HandlePlayerToServerChatMessage, so only calls from there count.
// Answers go out through the script bridge (SendMessageToPC).

using ChatCommand = std::vector<std::string> (*)(void* creature, const std::string& args);

// `word` with its "!", lower case (e.g. "!mastery"). Hooks the chat on first use.
bool AddChatCommand(const char* word, ChatCommand command);

// A server message to the player character `object_id`.
void TellPlayer(uint32_t object_id, const std::string& line);
