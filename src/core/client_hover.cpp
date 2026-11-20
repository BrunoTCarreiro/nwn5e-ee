// client_hover.h. Read from the win64 nwmain.exe (EE 8193.37-17): CNWCCreature's vtable slot 33 (MouseOver(int)) opens
// mov [rsp+8],rbx / push rdi / sub rsp,20h / mov ebx,edx / mov rdi,rcx and calls CNWCObject::MouseOver; slot 34
// (MouseOff()) opens push rbx / sub rsp,20h / mov rbx,rcx and calls CNWCObject::MouseOff. Both base functions take
// the object (rcx) and read its id at +8 (CNWCObject::SetId, slot 1, writes it there) and its type at +0xc. A client
// object's id is the server's with the top bit set (the radial menu's target, light_sources.cpp).
#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <atomic>

#include "client_hover.h"
#include "creature.h"
#include "hooks.h"
#include "log.h"

namespace {

using MouseOverFn = void (*)(void* object, int mode);
using MouseOffFn = void (*)(void* object);

constexpr const char* kCreatureRtti = ".?AVCNWCCreature@@";
constexpr int kMouseOverSlot = 33;
constexpr int kMouseOffSlot = 34;
constexpr uint8_t kMouseOverHead[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
                                      0xEC, 0x20, 0x8B, 0xDA, 0x48, 0x8B, 0xF9, 0xE8};
constexpr uint8_t kMouseOffHead[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0xE8};
constexpr size_t kClientObjectId = 0x8;
constexpr uint32_t kClientIdBit = 0x80000000u;

MouseOverFn g_mouse_over = nullptr;
MouseOffFn g_mouse_off = nullptr;
std::atomic<uint32_t> g_hovered{0};    // the client id under the cursor, 0 none
std::atomic<uint32_t> g_left{0};       // the client id the cursor left last
std::atomic<uint64_t> g_left_at{0};    // when (GetTickCount64)

void MouseOver(void* object, int mode) {
  if (object) g_hovered = Field<uint32_t>(object, kClientObjectId);
  g_mouse_over(object, mode);
}

void MouseOff(void* object) {
  if (object) {
    const uint32_t id = Field<uint32_t>(object, kClientObjectId);
    if (g_hovered == id) g_hovered = 0;
    g_left = id;
    g_left_at = GetTickCount64();
  }
  g_mouse_off(object);
}

bool StartsWith(uintptr_t code, const uint8_t* head, size_t size) {
  return code && memcmp(reinterpret_cast<const void*>(code), head, size) == 0;
}

}  // namespace

bool InstallClientHover() {
  const uintptr_t vtable = FindVtable("hover (#171's right-click)", kCreatureRtti);
  const uintptr_t over = VtableSlot(vtable, kMouseOverSlot);
  const uintptr_t off = VtableSlot(vtable, kMouseOffSlot);
  if (!StartsWith(over, kMouseOverHead, sizeof kMouseOverHead) || !StartsWith(off, kMouseOffHead, sizeof kMouseOffHead)) {
    Log("SKIP hover: CNWCCreature's MouseOver / MouseOff (vtable slots %d, %d) not as read - a right-click in the target "
        "window takes nothing back",
        kMouseOverSlot, kMouseOffSlot);
    return false;
  }
  const uintptr_t base_over = CallTarget(over + sizeof kMouseOverHead - 1);
  const uintptr_t base_off = CallTarget(off + sizeof kMouseOffHead - 1);
  if (!HookAddress("CNWCObject::MouseOver (the object under the cursor, #171)", base_over,
                   reinterpret_cast<void*>(&MouseOver), reinterpret_cast<void**>(&g_mouse_over)) ||
      !HookAddress("CNWCObject::MouseOff (the cursor off it, #171)", base_off, reinterpret_cast<void*>(&MouseOff),
                   reinterpret_cast<void**>(&g_mouse_off))) {
    return false;
  }
  Log("hover: the object under the cursor followed (CNWCObject::MouseOver / MouseOff) - a right-click in the target "
      "window takes back its dart (#171)");
  return true;
}

uint32_t HoveredObject(uint64_t left_within_ms) {
  uint32_t id = g_hovered;
  if (!id && g_left && GetTickCount64() - g_left_at <= left_within_ms) id = g_left;
  return id ? (id & ~kClientIdBit) : 0;
}
