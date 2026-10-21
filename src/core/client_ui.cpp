// nwn5e's UI system on the client (docs/CHARGEN.md §10): EE's own client NUI
// windows (Nui::Window, Nui::JSON::JsonWindow) and the classic GUI panels,
// reached through anchors (hooks.h) since the client exports none of them.
// Spike B1 step 1: the anchors themselves, checked against what the research
// read (logged at start-up; nothing is hooked yet).

#include <windows.h>

#include <stdint.h>

#include "hooks.h"
#include "log.h"

namespace {

uintptr_t Rva(uintptr_t address) {
  return address ? address - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) : 0;
}

struct VtableAnchor {
  const char* what;
  const char* rtti;
  uintptr_t expected_rva;  // what the read-only research found (0: not read)
};

}  // namespace

void InstallClientUi() {
  const uintptr_t can_choose = FunctionByString("client: CNWCLevelUpStats::CanChooseFeat",
                                                "CNWCLevelUpStats::CanChooseFeat() Invalid Feat");
  Log("client ui: CanChooseFeat at rva 0x%zx (research: 0x7ed040)", Rva(can_choose));
  const VtableAnchor vtables[] = {
      {"client: creation hub", ".?AVCCharacterGenerationPanel@@", 0xe64678},
      {"client: creation feats page", ".?AVCCharacterFeatsPanel@@", 0xe7b040},
      {"client: NUI window", ".?AVWindow@Nui@@", 0},
      {"client: NUI JSON window", ".?AVJsonWindow@JSON@Nui@@", 0},
      {"client: NUI config window", ".?AVConfigWindow@Nui@@", 0},
  };
  for (const VtableAnchor& anchor : vtables) {
    const uintptr_t vtable = FindVtable(anchor.what, anchor.rtti);
    Log("client ui: %s vtable at rva 0x%zx%s", anchor.what, Rva(vtable),
        anchor.expected_rva ? (Rva(vtable) == anchor.expected_rva ? " (matches the research)" : " (DIFFERS from the research)")
                            : "");
  }
}
