// FloatTextFor (floaty_text.h). CNWSVirtualMachineCommands::ExecuteCommandFloatingText (FloatingTextStringOnCreature)
// builds a CNWCCMessageData - object 0 the creature to float over, integer 9 the type (0x5e: a string), string 0 the
// text, integer 0 the chat-window flag - and sends it with CNWSMessage::SendServerToPlayerCCMessage (minor 0x0b) to
// the creature's own player, or broadcasts it to its faction. Here the same message goes to the caster's player with
// the target as the object: CNWSCreature::BroadcastFloatyData sends a floaty about one creature to other players
// the same way, so the client floats it over whichever object it names.
#include <windows.h>

#include <stdint.h>

#include "creature.h"
#include "ee_layout.h"
#include "floaty_text.h"
#include "hooks.h"
#include "log.h"

namespace {

using GetMessageFn = void* (*)(void* server_app);
using PlayerIdFn = uint32_t (*)(void* server_app, uint32_t object_id);
using SetObjectIdFn = void (*)(void* data, int index, uint32_t id);
using SetIntegerFn = void (*)(void* data, int index, int value);
using SetStringFn = void (*)(void* data, int index, void* string);  // by value: the callee frees the copy's buffer
using DataDestructorFn = void (*)(void* data);
using SendFn = int (*)(void* message, uint32_t player, uint8_t minor, void* data, void* attack);
using ExoStringFromCharsFn = void* (*)(void* string, const char* text);

constexpr const char* kGetNWSMessage = "?GetNWSMessage@CServerExoApp@@QEAAPEAVCNWSMessage@@XZ";
constexpr const char* kGetPlayerId = "?GetPlayerIDByGameObjectID@CServerExoApp@@QEAAII@Z";
constexpr const char* kSetObjectId = "?SetObjectID@CNWCCMessageData@@QEAAXHI@Z";
constexpr const char* kSetInteger = "?SetInteger@CNWCCMessageData@@QEAAXHH@Z";
constexpr const char* kSetString = "?SetString@CNWCCMessageData@@QEAAXHVCExoString@@@Z";
constexpr const char* kDataDestructor = "??1CNWCCMessageData@@QEAA@XZ";
constexpr const char* kSend =
    "?SendServerToPlayerCCMessage@CNWSMessage@@QEAAHIEPEAVCNWCCMessageData@@PEAVCNWSCombatAttackData@@@Z";

constexpr size_t kMessageDataSize = 0x48;  // operator new(0x48), zeroed, in ExecuteCommandFloatingText
constexpr int kIndexType = 9;
constexpr int kTypeFloatyString = 0x5e;
constexpr uint8_t kMinorFloaty = 0x0b;
constexpr uint32_t kNoPlayer = 0xfffffffe;

struct Engine {
  GetMessageFn get_message = reinterpret_cast<GetMessageFn>(EngineExport(kGetNWSMessage));
  PlayerIdFn player_id = reinterpret_cast<PlayerIdFn>(EngineExport(kGetPlayerId));
  SetObjectIdFn set_object = reinterpret_cast<SetObjectIdFn>(EngineExport(kSetObjectId));
  SetIntegerFn set_integer = reinterpret_cast<SetIntegerFn>(EngineExport(kSetInteger));
  SetStringFn set_string = reinterpret_cast<SetStringFn>(EngineExport(kSetString));
  DataDestructorFn destroy = reinterpret_cast<DataDestructorFn>(EngineExport(kDataDestructor));
  SendFn send = reinterpret_cast<SendFn>(EngineExport(kSend));
  ExoStringFromCharsFn make = reinterpret_cast<ExoStringFromCharsFn>(EngineExport(ee::kExoStringFromChars));
  void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  bool complete() const {
    return get_message && player_id && set_object && set_integer && set_string && destroy && send && make &&
           app_manager;
  }
};

const Engine& E() {
  static const Engine engine;
  return engine;
}

}  // namespace

bool FloatTextFor(uint32_t player_creature, uint32_t over, const std::string& text) {
  const Engine& e = E();
  if (!e.complete()) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      Log("SKIP floaty text: a piece of the engine's floaty message is missing - no text floats over targets");
    }
    return false;
  }
  void* app = *e.app_manager ? Field<void*>(*e.app_manager, ee::kAppManagerServerApp) : nullptr;
  if (!app) return false;
  const uint32_t player = e.player_id(app, player_creature);
  if (player == kNoPlayer || player == ee::kObjectInvalid) return false;  // no client: an NPC
  void* message = e.get_message(app);
  if (!message) return false;
  alignas(16) uint8_t data[kMessageDataSize] = {};  // as the command's new + zeroing; its arrays are the engine's
  alignas(16) uint8_t string[32] = {};              // CExoString: char* + length, its buffer the engine's
  e.make(string, text.c_str());
  e.set_object(data, 0, over);
  e.set_integer(data, kIndexType, kTypeFloatyString);
  e.set_string(data, 0, string);  // takes the buffer
  e.set_integer(data, 0, 0);      // not in the chat window
  e.send(message, player, kMinorFloaty, data, nullptr);
  e.destroy(data);
  return true;
}
