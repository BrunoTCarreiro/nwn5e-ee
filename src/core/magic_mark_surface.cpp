// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on the surface of the door's or the placeable's model where
// the caster's eyes met it, fixed to the part it hit - a door's leaf carries its mark as it swings. After MO-1 ("the
// door swings open but the magic mark stays in place") and MO-2 ("on a table it gets placed somehow inside the model
// ... I would assume it would be projected on the surface of the model?"): 0.96.0's mark stood on an invisible object
// at the door's slab or the placeable's walkmesh.
//
// The cast (rules_utility_cantrips.cpp) puts the mark's visual effect on the object itself and a record on the object
// (the local string "n5m_<model>", rules/mark_surface.h): the caster's eyes. The client attaches the effect's model
// with Gob::AttachToObject, and vfx_nodes.cpp's hook asks here first:
// - The object: the target model's external data (Gob +0x490, vtable +0xf0) is its client object - a door or a
//   placeable, by their vtables - whose id (+0x8, less bit 31) is the server's object's.
// - The surface: lines from the eyes to the model's box (Gob::GetBoundingBox, its own frame; rules::MarkAimPoints),
//   tested on the model's triangles by the game's own hit test - Gob::HitCheck (vtable +0x128) with a CHitInfo built
//   as the game builds it, (0, 0xFFFFFFFFBFFFFFFF, 0); its third value 1 tests the parts' boxes instead (the last
//   try). A hit leaves the nearest part's name at +0x68 (Part::HitCheck: Part::Name). Two more lines 2 cm beside the
//   first give the surface's normal (the triangle test leaves the CHitInfo's +0x5c at 0).
// - The mark: attached to that part, turning with it (mode 0), its own node (<model>_mark) set in the part's frame
//   (Gob::SetPartLocalPosition, vtable +0x140; rules::MarkFixInPart).
// - The record then keeps the part and the place, so the mark comes back there whenever the client attaches it
//   again (a load, a return to the area), whatever the door's state.
// The game's turns are checked at the install with its own Quaternion * Vector: rules::Rotate's way, or its reverse.
// Not placed (logged; the game's attach then): an object that isn't a door or a placeable, a record that doesn't
// read, no line hitting. The server's object is read from the client: single player and a host's own client.
//
// 0.96.3 (owner, 2026-11-09: "A" - the mark where you click - "but we should try to find a way to deal with walls
// that have floors behind them so that the wall is marked and not the floor"): the cast order's hook (action_guard.h,
// a player's own click, a frame after it) takes the point under the cursor with the game's own pick - after its
// CGuiMan::GetMousePos, the camera's HitCheck at the mouse (vtable +0x60): the camera's ray through it against the
// scene, tiles and all; two picks 3 pixels beside it give the surface's normal. The camera: g_pAppManager's client
// app (+0), its internal (+8), its world (+0x30), its camera (+0xe8) - the game's own pick's path, checked by its
// code at the install. Cast at a door or a placeable, the record keeps that ray and the model is tested along it
// (the eyes' lines when it misses: a radial opened on the object has the cursor on the menu). Cast at a point the
// cursor wasn't on (the floor behind a wall the game sends), the order's point moves in front of the wall and the
// cast marks the wall (rules_utility_cantrips.cpp). Single player only (GetIsMultiPlayer): the host's mouse isn't a
// remote player's.

#include "magic_mark_surface.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <string.h>

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "action_guard.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "rules/cantrips.h"
#include "rules/mark_surface.h"

namespace {

using rules::Quatf;
using rules::Vec3f;

constexpr const char* kHitCheck = "?HitCheck@Gob@@UEAAPEAVCAurObject@@VVector@@0PEAV3@PEAVCHitInfo@@@Z";
constexpr const char* kPartHitCheck = "?HitCheck@Part@@UEAAPEAVCAurPart@@VVector@@0PEAV3@PEAVCHitInfo@@M@Z";
constexpr const char* kTriMeshHitCheckGeom =
    "?HitCheckGeom@PartTriMesh@@UEAAHAEBVVector@@AEAV2@PEAV2@PEAVCHitInfo@@M@Z";
constexpr const char* kHitInfoMake = "??0CHitInfo@@QEAA@H_JH@Z";
constexpr const char* kHitInfoFree = "??1CHitInfo@@QEAA@XZ";
constexpr const char* kGetExternalData = "?GetExternalData@Gob@@UEAAPEAXXZ";
constexpr const char* kGetBoundingBox = "?GetBoundingBox@Gob@@UEAAXAEAVVector@@0@Z";
constexpr const char* kAdjustedScale = "?GetAdjustedScale@Gob@@UEBAMXZ";
constexpr const char* kAdjustedPosition = "?GetAdjustedPosition@Gob@@UEBA?AVVector@@XZ";
constexpr const char* kAdjustedOrientation = "?GetAdjustedOrientation@Gob@@UEBA?AVQuaternion@@XZ";
constexpr const char* kSetPartLocalPosition = "?SetPartLocalPosition@Gob@@UEAAHPEBDVVector@@VQuaternion@@@Z";
constexpr const char* kPartGetWorld = "?GetWorld@Part@@UEAAXAEAVVector@@AEAVQuaternion@@@Z";
constexpr const char* kGetModelName = "?GetModelName@Gob@@UEAAPEBDXZ";
constexpr const char* kGetPart = "?GetPart@Gob@@UEAAPEAVCAurPart@@PEBD@Z";
constexpr const char* kTurnVector = "??D@YA?AVVector@@AEBVQuaternion@@AEBV0@@Z";  // Quaternion * Vector
constexpr const char* kRttiGob = ".?AVGob@@";
constexpr const char* kRttiClientDoor = ".?AVCNWCDoor@@";
constexpr const char* kRttiClientPlaceable = ".?AVCNWCPlaceable@@";

// Gob's vtable (bytes / 8).
constexpr int kModelNameSlot = 0x18 / 8;
constexpr int kExternalDataSlot = 0xf0 / 8;
constexpr int kHitCheckSlot = 0x128 / 8;
constexpr int kSetPartLocalSlot = 0x140 / 8;
constexpr int kBoundingBoxSlot = 0x1f0 / 8;
constexpr int kGetPartSlot = 0x230 / 8;
constexpr int kAdjustedScaleSlot = 0x2d0 / 8;
constexpr int kAdjustedPositionSlot = 0x2d8 / 8;
constexpr int kAdjustedOrientationSlot = 0x2e0 / 8;
constexpr int kPartWorldSlot = 0x18 / 8;  // a part's vtable: Part::GetWorld

// CHitInfo: its constructor stores the third value at +0x20 (boxes only), Part::HitCheck the nearest part's name at
// +0x68. The constructor's last store is +0x7c; room to spare.
constexpr size_t kHitInfoSize = 0x100;
constexpr size_t kHitInfoPartName = 0x68;
constexpr int64_t kHitEverything = static_cast<int64_t>(0xFFFFFFFFBFFFFFFFull);  // the game's own mask
constexpr uint8_t kHitInfoBoxesStore[] = {0x44, 0x89, 0x49, 0x20};  // CHitInfo(): mov [rcx+20h], r9d
constexpr uint8_t kPartNameStore[] = {0xff, 0x50, 0x08, 0x49, 0x89, 0x46, 0x68};  // call [rax+8]; mov [r14+68h], rax
constexpr uint8_t kTriangleTest[] = {0x45, 0x39, 0x6f, 0x20};  // PartTriMesh::HitCheckGeom: cmp [r15+20h], r13d
constexpr uint32_t kClientIdBit = 0x80000000u;
constexpr int kAttachTurning = 0;
constexpr const char* kRecordPrefix = "n5m_";
constexpr const char* kMarkModelPrefix = "n5v_mark";
constexpr float kLargestBox = 60.0f;  // metres across: larger, the box isn't the model's own
constexpr int kLogCap = 48;

// 0.96.3: the point under the cursor at the click - the game's own pick right after its CGuiMan::GetMousePos call:
// mov rax, [g_pAppManager]; mov rcx, [rax]; call (mov rcx, [rcx+8]; jmp (mov rax, [rcx+30h]; ret)); the mouse's x, y
// and no CHitInfo as arguments; mov rcx, [rax+0e8h]; mov rax, [rcx]; call [rax+60h] (Camera::HitCheck).
constexpr const char* kGetMousePos = "?GetMousePos@CGuiMan@@QEAAXPEAH0@Z";
constexpr const char* kGuiManVariable = "?g_pGuiMan@@3PEAVCGuiMan@@EA";
constexpr const char* kCameraHitCheck = "?HitCheck@Camera@@UEAAHHHPEAVVector@@PEAVCHitInfo@@@Z";
constexpr const char* kCameraPosition = "?GetPosition@Camera@@UEAA?AVVector@@XZ";
constexpr const char* kIsMultiPlayer = "?GetIsMultiPlayer@CServerExoApp@@UEAAHXZ";
constexpr int kCameraPositionSlot = 0x40 / 8;
constexpr int kCameraHitCheckSlot = 0x60 / 8;
constexpr size_t kClientInternal = 0x8;
constexpr size_t kInternalWorld = 0x30;
constexpr size_t kWorldCamera = 0xe8;
constexpr uint8_t kLoadRaxRip[] = {0x48, 0x8b, 0x05};
constexpr uint8_t kLoadRcxRax[] = {0x48, 0x8b, 0x08};
constexpr uint8_t kInternalHop[] = {0x48, 0x8b, 0x49, 0x08, 0xe9};
constexpr uint8_t kWorldHop[] = {0x48, 0x8b, 0x41, 0x30, 0xc3};
constexpr uint8_t kPickArguments[] = {0x44, 0x8b, 0x44, 0x24, 0x50, 0x4c, 0x8d, 0x4c, 0x24, 0x40, 0x8b, 0x54,
                                      0x24, 0x54, 0x48, 0xc7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kCameraPickCall[] = {0x48, 0x8b, 0x88, 0xe8, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x01, 0xff, 0x50, 0x60};
constexpr int kNormalPixels = 3;           // the two picks beside the cursor's
constexpr int64_t kClickFreshMs = 120000;  // a click waits this long for its cast (the walk there included)
constexpr int kClickLogCap = 40;

using HitCheckFn = void* (*)(void* gob, float* start, float* end, float* hit, void* info);
using HitInfoMakeFn = void* (*)(void* info, int kind, int64_t mask, int boxes);
using HitInfoFreeFn = void (*)(void* info);
using ExternalDataFn = void* (*)(void* gob);
using BoundingBoxFn = void (*)(void* gob, float* box_min, float* box_max);
using ScaleFn = float (*)(const void* gob);
using ResultOutFn = float* (*)(const void* gob, float* out);  // a Vector or a Quaternion by value: `out` after `this`
using SetPartLocalFn = int (*)(void* gob, const char* part, const float* position, const float* turn);
using PartWorldFn = void (*)(void* part, float* position, float* turn);
using ModelNameFn = const char* (*)(void* gob);
using GetPartFn = void* (*)(void* object, const char* name);
using TurnVectorFn = float* (*)(float* out, const float* turn, const float* vector);
using MousePosFn = void (*)(void* gui, int* x, int* y);
using CameraPickFn = int (*)(void* camera, int x, int y, float* point, void* info);
using IsMultiPlayerFn = int (*)(void* server);

struct Api {
  HitInfoMakeFn make_info = nullptr;
  HitInfoFreeFn free_info = nullptr;
  uintptr_t model_name = 0, external_data = 0, hit_check = 0, set_part_local = 0, bounding_box = 0, get_part = 0;
  uintptr_t adjusted_scale = 0, adjusted_position = 0, adjusted_orientation = 0, part_world = 0;
  uintptr_t door_vtable = 0, placeable_vtable = 0;
};
Api g_api;
std::atomic<bool> g_ready{false};
bool g_reversed = false;  // the game turns rules::Rotate's reverse way: its turns conjugated in and out
std::atomic<int> g_logs{0};

struct ClickApi {
  MousePosFn mouse_pos = nullptr;
  IsMultiPlayerFn multiplayer = nullptr;
  void** gui = nullptr;          // g_pGuiMan
  void** app_manager = nullptr;  // g_pAppManager
  uintptr_t camera_hit_check = 0;
  uintptr_t camera_position = 0;
};
ClickApi g_click_api;
std::mutex g_clicks_mutex;
std::map<uint32_t, MarkClick> g_clicks;  // each caster's last Magic Mark click
std::atomic<int> g_click_logs{0};

bool ClickLogSlot() { return g_click_logs.fetch_add(1) < kClickLogCap; }

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

bool LogSlot() { return g_logs.fetch_add(1) < kLogCap; }

uintptr_t SlotOf(void* object, int slot) {
  const uintptr_t* vtable = *static_cast<const uintptr_t* const*>(object);
  return vtable ? vtable[slot] : 0;
}

template <typename Fn>
Fn Slot(void* object, int slot) {
  return reinterpret_cast<Fn>(SlotOf(object, slot));
}

void* PartOf(void* gob, const char* name) { return Slot<GetPartFn>(gob, kGetPartSlot)(gob, name); }

Quatf FromGame(const float turn[4]) {
  const Quatf q{turn[0], turn[1], turn[2], turn[3]};
  return g_reversed ? rules::Conjugate(q) : q;
}

void ToGame(const Quatf& q, float turn[4]) {
  const Quatf game = g_reversed ? rules::Conjugate(q) : q;
  turn[0] = game.w;
  turn[1] = game.x;
  turn[2] = game.y;
  turn[3] = game.z;
}

std::string Lower(const char* text) {
  std::string out = text ? text : "";
  for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}

// An upright Magic Mark's model: n5v_markNN[colour]u.
bool UprightMarkModel(const std::string& model) {
  return model.size() >= 11 && model.size() <= 13 && model.compare(0, 8, kMarkModelPrefix) == 0 &&
         model.back() == 'u';
}

// The target is a model whose vtable keeps the slots read here (a Gob's).
bool TargetAsRead(void* gob) {
  return SlotOf(gob, kExternalDataSlot) == g_api.external_data && SlotOf(gob, kHitCheckSlot) == g_api.hit_check &&
         SlotOf(gob, kBoundingBoxSlot) == g_api.bounding_box && SlotOf(gob, kGetPartSlot) == g_api.get_part &&
         SlotOf(gob, kAdjustedScaleSlot) == g_api.adjusted_scale &&
         SlotOf(gob, kAdjustedPositionSlot) == g_api.adjusted_position &&
         SlotOf(gob, kAdjustedOrientationSlot) == g_api.adjusted_orientation;
}

// The server's door or placeable whose model `target` is (*kind its name), or null.
void* ServerObjectOf(void* target, const char** kind, uint32_t* id) {
  void* client = Slot<ExternalDataFn>(target, kExternalDataSlot)(target);
  if (!client) return nullptr;
  const uintptr_t vtable = *static_cast<const uintptr_t*>(client);
  if (vtable == g_api.door_vtable) {
    *kind = "door";
  } else if (vtable == g_api.placeable_vtable) {
    *kind = "placeable";
  } else {
    return nullptr;
  }
  *id = Field<uint32_t>(client, ee::kClientObjectId) & ~kClientIdBit;
  return GameObjectById(*id);
}

struct Hit {
  Vec3f point;
  std::string part;
};

// The game's hit test of `gob` along a line: the nearest point and the part it's on.
std::optional<Hit> HitLine(void* gob, const Vec3f& from, const Vec3f& to, bool boxes) {
  alignas(16) uint8_t info[kHitInfoSize] = {};
  g_api.make_info(info, 0, kHitEverything, boxes ? 1 : 0);
  float start[3] = {from.x, from.y, from.z};
  float end[3] = {to.x, to.y, to.z};
  float at[3] = {};
  void* found = Slot<HitCheckFn>(gob, kHitCheckSlot)(gob, start, end, at, info);
  const char* name = Field<const char*>(info, kHitInfoPartName);
  std::optional<Hit> hit;
  if (found && name && *name) hit = Hit{{at[0], at[1], at[2]}, name};
  g_api.free_info(info);
  return hit;
}

// The model's place in the world.
struct Frame {
  Vec3f position;
  Quatf turn;
  float scale = 1.0f;
};

Frame FrameOf(void* gob) {
  float position[3] = {};
  float turn[4] = {1, 0, 0, 0};
  Slot<ResultOutFn>(gob, kAdjustedPositionSlot)(gob, position);
  Slot<ResultOutFn>(gob, kAdjustedOrientationSlot)(gob, turn);
  Frame frame;
  frame.position = {position[0], position[1], position[2]};
  frame.turn = FromGame(turn);
  const float scale = Slot<ScaleFn>(gob, kAdjustedScaleSlot)(gob);
  frame.scale = scale > 0.01f ? scale : 1.0f;
  return frame;
}

Vec3f ToWorld(const Frame& frame, const Vec3f& local) {
  return rules::Add(frame.position, rules::Rotate(frame.turn, rules::Scale(local, frame.scale)));
}

Vec3f ToLocal(const Frame& frame, const Vec3f& world) {
  return rules::Scale(rules::Rotate(rules::Conjugate(frame.turn), rules::Sub(world, frame.position)), 1.0f / frame.scale);
}

struct Projection {
  Hit hit;
  Vec3f normal;
  Vec3f direction;
  int lines = 0;
  bool boxes = false;
};

// A hit along the line from `from` to `to`, with the surface's normal there (two lines beside it).
Projection SurfaceAt(void* target, const Hit& hit, const Vec3f& from, const Vec3f& to, bool boxes, int lines) {
  const Vec3f direction = rules::Normalized(rules::Sub(to, from));
  Vec3f u, v;
  rules::MarkRayOffsets(direction, &u, &v);
  std::optional<Vec3f> beside_u, beside_v;
  if (const std::optional<Hit> h = HitLine(target, rules::Add(from, u), rules::Add(to, u), boxes)) beside_u = h->point;
  if (const std::optional<Hit> h = HitLine(target, rules::Add(from, v), rules::Add(to, v), boxes)) beside_v = h->point;
  Projection found;
  found.hit = hit;
  found.normal = rules::MarkSurfaceNormal(direction, hit.point, beside_u, beside_v);
  found.direction = direction;
  found.lines = lines;
  found.boxes = boxes;
  return found;
}

// The click's ray (0.96.3; lines 0), else lines from the eyes to the model; the first that hits, with the surface's
// normal there.
std::optional<Projection> Project(void* target, const rules::MarkRecord& record, std::string* why) {
  if (record.click) {
    const Vec3f end = rules::MarkClickEnd(*record.click);
    if (const std::optional<Hit> hit = HitLine(target, record.click->from, end, false)) {
      return SurfaceAt(target, *hit, record.click->from, end, false, 0);
    }
  }
  const Vec3f& eye_world = record.eye;
  float low[3] = {}, high[3] = {};
  Slot<BoundingBoxFn>(target, kBoundingBoxSlot)(target, low, high);
  const Vec3f box_min{low[0], low[1], low[2]}, box_max{high[0], high[1], high[2]};
  const float across = rules::Length(rules::Sub(box_max, box_min));
  if (!(box_min.x <= box_max.x && box_min.y <= box_max.y && box_min.z <= box_max.z) || !(across > 0.01f) ||
      across > kLargestBox) {
    char text[160];
    sprintf_s(text, "its box doesn't read (%.2f %.2f %.2f to %.2f %.2f %.2f)", box_min.x, box_min.y, box_min.z,
              box_max.x, box_max.y, box_max.z);
    *why = text;
    return std::nullopt;
  }
  const Frame frame = FrameOf(target);
  const Vec3f eye = ToLocal(frame, eye_world);
  int lines = 0;
  for (const bool boxes : {false, true}) {
    for (const Vec3f& aim : rules::MarkAimPoints(eye, box_min, box_max)) {
      const Vec3f to = ToWorld(frame, rules::MarkRayEnd(eye, aim, box_min, box_max));
      ++lines;
      const std::optional<Hit> hit = HitLine(target, eye_world, to, boxes);
      if (hit) return SurfaceAt(target, *hit, eye_world, to, boxes, lines);
    }
  }
  char text[200];
  sprintf_s(text, "no line from the eyes (%.2f %.2f %.2f) hits it - its box %.2f %.2f %.2f to %.2f %.2f %.2f",
            eye_world.x, eye_world.y, eye_world.z, box_min.x, box_min.y, box_min.z, box_max.x, box_max.y, box_max.z);
  *why = text;
  return std::nullopt;
}

// The mark on `part`, turning with it, its node at `fix` in the part's frame.
bool Place(void* self, void* target, void* part, const std::string& model, const rules::MarkFix& fix,
           MarkAttachPartFn attach_to_part, void** result) {
  const std::string node = model + "_mark";
  if (!PartOf(self, node.c_str())) return false;
  *result = attach_to_part(self, target, part, kAttachTurning);
  const float position[3] = {fix.position.x, fix.position.y, fix.position.z};
  float turn[4];
  ToGame(fix.turn, turn);
  Slot<SetPartLocalFn>(self, kSetPartLocalSlot)(self, node.c_str(), position, turn);
  return true;
}

bool PartWorld(void* part, Vec3f* position, Quatf* turn) {
  if (!part || SlotOf(part, kPartWorldSlot) != g_api.part_world) return false;
  float at[3] = {};
  float game_turn[4] = {1, 0, 0, 0};
  Slot<PartWorldFn>(part, kPartWorldSlot)(part, at, game_turn);
  *position = {at[0], at[1], at[2]};
  *turn = FromGame(game_turn);
  return true;
}

bool CodeIs(uintptr_t at, const uint8_t* bytes, size_t size) {
  return memcmp(reinterpret_cast<const void*>(at), bytes, size) == 0;
}

uintptr_t Rel32Target(uintptr_t field, uintptr_t next) { return next + *reinterpret_cast<const int32_t*>(field); }

// The game's own pick under the cursor is as read (see kGetMousePos), its camera through `app_manager`.
bool GamePickAsRead(uintptr_t mouse_pos, uintptr_t app_manager) {
  for (const uintptr_t at : FindCallsAnywhere(mouse_pos)) {
    if (!CodeIs(at + 5, kLoadRaxRip, sizeof(kLoadRaxRip)) || Rel32Target(at + 8, at + 12) != app_manager) continue;
    if (!CodeIs(at + 12, kLoadRcxRax, sizeof(kLoadRcxRax)) || *reinterpret_cast<const uint8_t*>(at + 15) != 0xe8) {
      continue;
    }
    const uintptr_t internal = Rel32Target(at + 16, at + 20);
    if (!CodeIs(internal, kInternalHop, sizeof(kInternalHop))) continue;
    if (!CodeIs(Rel32Target(internal + 5, internal + 9), kWorldHop, sizeof(kWorldHop))) continue;
    if (CodeIs(at + 20, kPickArguments, sizeof(kPickArguments)) &&
        CodeIs(at + 43, kCameraPickCall, sizeof(kCameraPickCall))) {
      return true;
    }
  }
  return false;
}

void* WorldCamera() {
  void* manager = *g_click_api.app_manager;
  void* client = manager ? Field<void*>(manager, 0) : nullptr;
  void* internal = client ? Field<void*>(client, kClientInternal) : nullptr;
  void* world = internal ? Field<void*>(internal, kInternalWorld) : nullptr;
  void* camera = world ? Field<void*>(world, kWorldCamera) : nullptr;
  if (!camera || SlotOf(camera, kCameraHitCheckSlot) != g_click_api.camera_hit_check ||
      SlotOf(camera, kCameraPositionSlot) != g_click_api.camera_position) {
    return nullptr;
  }
  return camera;
}

bool MultiPlayer() {
  void* manager = *g_click_api.app_manager;
  void* server = manager ? Field<void*>(manager, ee::kAppManagerServerApp) : nullptr;
  return !server || g_click_api.multiplayer(server) != 0;
}

std::optional<Vec3f> PickAt(void* camera, int x, int y) {
  alignas(16) uint8_t info[kHitInfoSize] = {};
  g_api.make_info(info, 0, kHitEverything, 0);
  float at[3] = {};
  const int hit = Slot<CameraPickFn>(camera, kCameraHitCheckSlot)(camera, x, y, at, info);
  g_api.free_info(info);
  if (!hit) return std::nullopt;
  return Vec3f{at[0], at[1], at[2]};
}

struct CursorPick {
  Vec3f camera;
  Vec3f point;
  Vec3f normal;
  int x = 0, y = 0;
};

// The point under the cursor now, the camera it's seen from and its surface's normal.
std::optional<CursorPick> PickUnderCursor(std::string* why) {
  void* camera = WorldCamera();
  void* gui = *g_click_api.gui;
  if (!camera || !gui) {
    *why = camera ? "no GUI" : "no world camera as read";
    return std::nullopt;
  }
  CursorPick pick;
  g_click_api.mouse_pos(gui, &pick.x, &pick.y);
  const std::optional<Vec3f> point = PickAt(camera, pick.x, pick.y);
  if (!point) {
    *why = "nothing under the cursor";
    return std::nullopt;
  }
  float from[3] = {};
  Slot<ResultOutFn>(camera, kCameraPositionSlot)(camera, from);
  pick.camera = {from[0], from[1], from[2]};
  pick.point = *point;
  const Vec3f direction = rules::Normalized(rules::Sub(pick.point, pick.camera));
  pick.normal = rules::MarkSurfaceNormal(direction, pick.point, PickAt(camera, pick.x + kNormalPixels, pick.y),
                                         PickAt(camera, pick.x, pick.y + kNormalPixels));
  return pick;
}

// The cast order's adjuster (action_guard.h): a player's own Magic Mark click in single player - the point under the
// cursor kept for the cast; cast at a point the cursor wasn't on (a wall with floor behind it), the order's point
// moved in front of the wall, at the caster's feet.
void NoteMarkOrder(void* creature, SpellOrder* order, bool from_input) {
  if (!from_input || !creature || !order ||
      rules::UtilityEffectOf(static_cast<int>(order->spell)) != rules::UtilityEffect::kMagicMark) {
    return;
  }
  const uint32_t caster = Field<uint32_t>(creature, ee::kObjectId);
  if (MultiPlayer()) {
    if (ClickLogSlot()) Log("magic mark clicks: 0x%08X - a multiplayer game: aimed from the eyes, no walls", caster);
    return;
  }
  std::string why;
  const std::optional<CursorPick> pick = PickUnderCursor(&why);
  if (!pick) {
    if (ClickLogSlot()) Log("magic mark clicks: 0x%08X spell %u - %s; aimed from the eyes", caster, order->spell, why.c_str());
    return;
  }
  MarkClick click;
  click.spell = order->spell;
  click.target = order->target;
  click.camera = pick->camera;
  click.point = pick->point;
  click.normal = pick->normal;
  click.when_ms = NowMs();
  const Vec3f feet{Field<float>(creature, ee::kObjectPositionX), Field<float>(creature, ee::kObjectPositionY),
                   Field<float>(creature, ee::kObjectPositionZ)};
  const Vec3f sent{order->x, order->y, order->z};
  const bool at_point = order->target == ee::kObjectInvalid;
  if (at_point && !rules::MarkClickOnFloor(pick->point, sent) && !rules::MarkCursorOnCaster(pick->point, feet)) {
    click.wall = true;
    const Vec3f before = rules::MarkCastPointBefore(pick->point, pick->camera, feet.z);
    order->x = before.x;
    order->y = before.y;
    order->z = before.z;
    if (ClickLogSlot()) {
      Log("magic mark clicks: 0x%08X spell %u - the cursor at %.2f %.2f %.2f (facing %.2f %.2f %.2f) isn't the floor "
          "sent (%.2f %.2f %.2f): a wall's mark; the cast moved to %.2f %.2f %.2f, in front of it (mouse %d %d)",
          caster, order->spell, pick->point.x, pick->point.y, pick->point.z, pick->normal.x, pick->normal.y,
          pick->normal.z, sent.x, sent.y, sent.z, before.x, before.y, before.z, pick->x, pick->y);
    }
  } else if (ClickLogSlot()) {
    Log("magic mark clicks: 0x%08X spell %u at 0x%08X - the cursor at %.2f %.2f %.2f, seen from %.2f %.2f %.2f "
        "(the order's point %.2f %.2f %.2f; mouse %d %d)",
        caster, order->spell, order->target, pick->point.x, pick->point.y, pick->point.z, pick->camera.x,
        pick->camera.y, pick->camera.z, sent.x, sent.y, sent.z, pick->x, pick->y);
  }
  std::lock_guard<std::mutex> lock(g_clicks_mutex);
  g_clicks[caster] = click;
}

void InstallMarkClicks() {
  g_click_api.mouse_pos = reinterpret_cast<MousePosFn>(EngineExport(kGetMousePos));
  g_click_api.multiplayer = reinterpret_cast<IsMultiPlayerFn>(EngineExport(kIsMultiPlayer));
  g_click_api.gui = reinterpret_cast<void**>(EngineExport(kGuiManVariable));
  g_click_api.app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  g_click_api.camera_hit_check = EngineExport(kCameraHitCheck);
  g_click_api.camera_position = EngineExport(kCameraPosition);
  if (!g_click_api.mouse_pos || !g_click_api.multiplayer || !g_click_api.gui || !g_click_api.app_manager ||
      !g_click_api.camera_hit_check || !g_click_api.camera_position ||
      !GamePickAsRead(reinterpret_cast<uintptr_t>(g_click_api.mouse_pos),
                      reinterpret_cast<uintptr_t>(g_click_api.app_manager))) {
    Log("SKIP magic mark clicks: the game's pick under the cursor isn't as read - marks on objects are aimed from "
        "the caster's eyes, and walls aren't marked");
    return;
  }
  if (!AddSpellOrderAdjuster(&NoteMarkOrder)) {
    Log("SKIP magic mark clicks: the cast order isn't hooked - marks on objects are aimed from the caster's eyes");
    return;
  }
  Log("magic mark clicks: on - a player's Magic Mark goes where the cursor was at the click (single player); a click "
      "on a wall marks the wall");
}

}  // namespace

std::optional<MarkClick> TakeMarkClick(uint32_t caster, uint32_t spell) {
  std::lock_guard<std::mutex> lock(g_clicks_mutex);
  const auto found = g_clicks.find(caster);
  if (found == g_clicks.end()) return std::nullopt;
  const MarkClick click = found->second;
  g_clicks.erase(found);
  if (click.spell != spell || NowMs() - click.when_ms > kClickFreshMs) return std::nullopt;
  return click;
}

bool AttachMagicMark(void* self, void* target, int game_mode, MarkAttachPartFn attach_to_part, void** result) {
  if (!g_ready.load(std::memory_order_acquire) || !self || !target || !attach_to_part || !result) return false;
  if (SlotOf(self, kModelNameSlot) != g_api.model_name) return false;
  const std::string model = Lower(Slot<ModelNameFn>(self, kModelNameSlot)(self));
  if (!UprightMarkModel(model)) return false;
  if (SlotOf(self, kSetPartLocalSlot) != g_api.set_part_local || SlotOf(self, kGetPartSlot) != g_api.get_part ||
      !TargetAsRead(target)) {
    if (LogSlot()) Log("magic mark surface: %s - its model or its target's isn't a Gob as read; the game's attach", model.c_str());
    return false;
  }
  const char* kind = nullptr;
  uint32_t id = 0;
  void* object = ServerObjectOf(target, &kind, &id);
  if (!object) return false;  // not a door or a placeable (0.96.0's invisible object is one, without a record)
  const std::string key = kRecordPrefix + model;
  const std::string text = engine::GetLocalString(object, key.c_str());
  if (text.empty()) return false;
  rules::MarkRecord record;
  if (!rules::ParseMarkRecord(text, &record)) {
    if (LogSlot()) Log("magic mark surface: %s on %s 0x%08X - its record \"%s\" doesn't read; the game's attach",
                       model.c_str(), kind, id, text.c_str());
    return false;
  }
  if (record.fix) {
    void* part = PartOf(target, record.fix->part.c_str());
    if (part && Place(self, target, part, model, *record.fix, attach_to_part, result)) {
      if (LogSlot()) {
        Log("magic mark surface: %s back on %s 0x%08X, its part \"%s\" (%.2f %.2f %.2f in it; the game's mode %d)",
            model.c_str(), kind, id, record.fix->part.c_str(), record.fix->position.x, record.fix->position.y,
            record.fix->position.z, game_mode);
      }
      return true;
    }
    if (LogSlot()) Log("magic mark surface: %s on %s 0x%08X - its part \"%s\" isn't there; placed again", model.c_str(),
                       kind, id, record.fix->part.c_str());
  }
  std::string why;
  const std::optional<Projection> found = Project(target, record, &why);
  if (!found) {
    if (LogSlot()) Log("magic mark surface: %s on %s 0x%08X - %s; the game's attach", model.c_str(), kind, id, why.c_str());
    return false;
  }
  void* part = PartOf(target, found->hit.part.c_str());
  Vec3f part_position;
  Quatf part_turn;
  if (!PartWorld(part, &part_position, &part_turn)) {
    if (LogSlot()) Log("magic mark surface: %s on %s 0x%08X - the part hit, \"%s\", isn't found as read; the game's attach",
                       model.c_str(), kind, id, found->hit.part.c_str());
    return false;
  }
  const rules::MarkFix fix = rules::MarkFixInPart(found->hit.part, found->hit.point, found->normal, found->direction,
                                                  part_position, part_turn);
  if (!Place(self, target, part, model, fix, attach_to_part, result)) {
    if (LogSlot()) Log("magic mark surface: %s has no node \"%s_mark\"; the game's attach", model.c_str(), model.c_str());
    return false;
  }
  record.fix = fix;
  engine::SetLocalString(object, key.c_str(), rules::MarkRecordText(record).c_str());
  if (LogSlot()) {
    const std::string how = found->lines == 0 ? std::string("the click's ray")
                                              : (record.click ? "the click's ray missed; line " : "line ") +
                                                    std::to_string(found->lines);
    Log("magic mark surface: %s on %s 0x%08X - its part \"%s\" at %.2f %.2f %.2f, facing %.2f %.2f %.2f (%s%s; "
        "the eyes at %.2f %.2f %.2f; the game's mode %d)",
        model.c_str(), kind, id, found->hit.part.c_str(), found->hit.point.x, found->hit.point.y, found->hit.point.z,
        found->normal.x, found->normal.y, found->normal.z, how.c_str(), found->boxes ? ", its parts' boxes" : "",
        record.eye.x, record.eye.y, record.eye.z, game_mode);
  }
  return true;
}

bool InstallMagicMarkSurface() {
  g_api.make_info = reinterpret_cast<HitInfoMakeFn>(EngineExport(kHitInfoMake));
  g_api.free_info = reinterpret_cast<HitInfoFreeFn>(EngineExport(kHitInfoFree));
  g_api.model_name = EngineExport(kGetModelName);
  g_api.external_data = EngineExport(kGetExternalData);
  g_api.hit_check = EngineExport(kHitCheck);
  g_api.set_part_local = EngineExport(kSetPartLocalPosition);
  g_api.bounding_box = EngineExport(kGetBoundingBox);
  g_api.get_part = EngineExport(kGetPart);
  g_api.adjusted_scale = EngineExport(kAdjustedScale);
  g_api.adjusted_position = EngineExport(kAdjustedPosition);
  g_api.adjusted_orientation = EngineExport(kAdjustedOrientation);
  g_api.part_world = EngineExport(kPartGetWorld);
  const uintptr_t gob = FindVtable("magic mark surface: Gob's vtable", kRttiGob);
  const bool slots = gob && g_api.make_info && g_api.free_info && g_api.part_world &&
                     VtableSlot(gob, kModelNameSlot) == g_api.model_name &&
                     VtableSlot(gob, kExternalDataSlot) == g_api.external_data &&
                     VtableSlot(gob, kHitCheckSlot) == g_api.hit_check &&
                     VtableSlot(gob, kSetPartLocalSlot) == g_api.set_part_local &&
                     VtableSlot(gob, kBoundingBoxSlot) == g_api.bounding_box &&
                     VtableSlot(gob, kGetPartSlot) == g_api.get_part &&
                     VtableSlot(gob, kAdjustedScaleSlot) == g_api.adjusted_scale &&
                     VtableSlot(gob, kAdjustedPositionSlot) == g_api.adjusted_position &&
                     VtableSlot(gob, kAdjustedOrientationSlot) == g_api.adjusted_orientation;
  if (!slots || !g_api.model_name) {
    Log("SKIP magic mark surface: Gob's slots aren't as read (0x18 0xf0 0x128 0x140 0x1f0 0x230 0x2d0-0x2e0) - marks on "
        "objects stand on an invisible object");
    return false;
  }
  const CodeRange make = FunctionAt(reinterpret_cast<uintptr_t>(g_api.make_info));
  const CodeRange part_hit = FunctionAt(EngineExport(kPartHitCheck));
  const CodeRange triangles = FunctionAt(EngineExport(kTriMeshHitCheckGeom));
  if (!make.Valid() || !part_hit.Valid() || !triangles.Valid() ||
      !ContainsBytes(make.begin, make.end, kHitInfoBoxesStore, sizeof(kHitInfoBoxesStore)) ||
      !ContainsBytes(part_hit.begin, part_hit.end, kPartNameStore, sizeof(kPartNameStore)) ||
      !ContainsBytes(triangles.begin, triangles.end, kTriangleTest, sizeof(kTriangleTest))) {
    Log("SKIP magic mark surface: the hit test isn't as read (CHitInfo +0x20, +0x68; the triangle test) - marks on "
        "objects stand on an invisible object");
    return false;
  }
  g_api.door_vtable = FindVtable("magic mark surface: the client's door", kRttiClientDoor);
  g_api.placeable_vtable = FindVtable("magic mark surface: the client's placeable", kRttiClientPlaceable);
  if (!g_api.door_vtable || !g_api.placeable_vtable) {
    Log("SKIP magic mark surface: the client's door or placeable isn't found - marks on objects stand on an invisible "
        "object");
    return false;
  }
  // The game's turns: a quarter turn about z takes east to north (rules::Rotate), or to south (the reverse).
  const TurnVectorFn turn_vector = reinterpret_cast<TurnVectorFn>(EngineExport(kTurnVector));
  const float quarter[4] = {0.70710678f, 0, 0, 0.70710678f};
  const float east[3] = {1, 0, 0};
  float turned[3] = {};
  if (turn_vector) turn_vector(turned, quarter, east);
  if (turn_vector && fabsf(turned[0]) < 0.01f && fabsf(turned[1] - 1.0f) < 0.01f && fabsf(turned[2]) < 0.01f) {
    g_reversed = false;
  } else if (turn_vector && fabsf(turned[0]) < 0.01f && fabsf(turned[1] + 1.0f) < 0.01f && fabsf(turned[2]) < 0.01f) {
    g_reversed = true;
  } else {
    Log("SKIP magic mark surface: the game's Quaternion * Vector isn't as read (east turned to %.2f %.2f %.2f) - marks "
        "on objects stand on an invisible object",
        turned[0], turned[1], turned[2]);
    return false;
  }
  g_ready.store(true, std::memory_order_release);
  Log("magic mark surface: on - a mark cast at a door or a placeable goes on its model's surface, on the part the "
      "caster's eyes meet (the game's turns %s)",
      g_reversed ? "the reverse way" : "as rules::Rotate");
  InstallMarkClicks();
  return true;
}

bool MagicMarkSurfaceReady() { return g_ready.load(std::memory_order_acquire); }
