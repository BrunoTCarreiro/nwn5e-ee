#include "effects.h"

#include <string.h>

#include "hooks.h"
#include "log.h"

namespace effects {

int Integer(const void* effect, int index) {
  if (!effect || index < 0 || index >= Field<int32_t>(effect, ee::kEffectIntegerCount)) return 0;
  const int32_t* integers = Field<const int32_t*>(effect, ee::kEffectIntegers);
  return integers ? integers[index] : 0;
}

bool HasType(const void* object, uint16_t type) {
  bool found = false;
  ForEachOfType(object, type, [&](const void*) { found = true; });
  return found;
}

bool HasState(const void* object, int state) {
  bool found = false;
  ForEachOfType(object, ee::kEffectTypeSetState, [&](const void* effect) {
    if (Integer(effect, 0) == state) found = true;
  });
  return found;
}

}  // namespace effects

namespace {

using RunScriptChunkFn = int (*)(void* vm, const void* code, uint32_t self, int self_valid, int wrap_into_main);
using ExoStringFromCharsFn = void* (*)(void* string, const char* text);
using ExoStringDestructorFn = void (*)(void* string);

struct Bridge {
  void** vm = reinterpret_cast<void**>(EngineExport(ee::kVirtualMachine));
  RunScriptChunkFn run = reinterpret_cast<RunScriptChunkFn>(EngineExport(ee::kRunScriptChunk));
  ExoStringFromCharsFn make = reinterpret_cast<ExoStringFromCharsFn>(EngineExport(ee::kExoStringFromChars));
  ExoStringDestructorFn destroy = reinterpret_cast<ExoStringDestructorFn>(EngineExport(ee::kExoStringDestructor));
};

const Bridge& B() {
  static const Bridge bridge;
  return bridge;
}

}  // namespace

int RunScriptChunk(const char* code, uint32_t self) {
  const Bridge& b = B();
  if (!b.vm || !*b.vm || !b.run || !b.make || !b.destroy) return -1;
  alignas(16) uint8_t text[32] = {};  // CExoString: char* + length; the engine owns its buffer
  b.make(text, code);
  const int result = b.run(*b.vm, text, self, 1, 1);
  b.destroy(text);
  static int logged = 0;
  if (logged < 3) {
    ++logged;
    Log("script bridge: RunScriptChunk returned %d (self 0x%08X, %zu chars)", result, self, strlen(code));
  }
  return result;
}
