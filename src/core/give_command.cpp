// Test helper (`[debug] give_command=1`, off by default): "!give <blueprint>
// [stack]" creates an item from its blueprint (resref) in the speaker's
// inventory — e.g. a Tower Shield (nw_ashto001) to try on a Small character.
// The blueprint is checked to be a resref (letters, digits, '_', at most 16)
// before it goes into the script.

#include <ctype.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "effects.h"
#include "log.h"

namespace {

bool IsResRef(const std::string& text) {
  if (text.empty() || text.size() > 16) return false;
  for (char c : text) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  return true;
}

std::vector<std::string> GiveCommand(void* creature, const std::string& args) {
  std::string blueprint = args, count_text;
  const size_t space = args.find(' ');
  if (space != std::string::npos) {
    blueprint = args.substr(0, space);
    count_text = args.substr(space + 1);
  }
  for (char& c : blueprint) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (!IsResRef(blueprint)) {
    return {"nwn5e: !give <item blueprint> [stack] - e.g. !give nw_ashto001 (Tower Shield), nw_wswgs001 "
            "(Greatsword), nw_wplss001 (Spear), nw_wamar001 20 (20 arrows)."};
  }
  int count = count_text.empty() ? 1 : atoi(count_text.c_str());
  if (count < 1) count = 1;
  if (count > 99) count = 99;
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  const std::string code = "object o = CreateItemOnObject(\"" + blueprint + "\", OBJECT_SELF, " +
                           std::to_string(count) +
                           "); if (GetIsObjectValid(o)) SendMessageToPC(OBJECT_SELF, \"nwn5e: given \" + GetName(o) + "
                           "\".\"); else SendMessageToPC(OBJECT_SELF, \"nwn5e: no item blueprint \\\"" +
                           blueprint + "\\\".\");";
  RunScriptChunk(code.c_str(), id);
  Log("give: 0x%08X asked for %s x%d", id, blueprint.c_str(), count);
  return {};
}

}  // namespace

void InstallGiveCommand() {
  if (!g_config.debug_give_command) return;
  AddChatCommand("!give", &GiveCommand);
  Log("config: [debug] give_command=1 - !give <blueprint> creates items (testing)");
}
