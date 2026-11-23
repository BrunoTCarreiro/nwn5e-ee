// mdlmesh - the Floating Disk generator's mesh reader (#39; tools\data\floatdisk\build.js). A model's meshes as the VFX
// Bench's draft text, through this repo's own model reader (src\data\vfx_model.cpp): a compiled model through
// ReadVfxBinaryModel with any_mesh (plain, skin, animated and dangly meshes - never a walkmesh), an ASCII one through
// ReadVfxAsciiModel (its trimeshes; build.js hands it ASCII models with every mesh as a trimesh, its UVs one per vertex).
// After the draft: "# source binary|ascii", then a line per mesh: "# mesh <name> flags 0x<node flags> verts <n> faces <n>
// bitmap <name>". Built by build_mdlmesh.bat beside it into build\floatdisk\mdlmesh.exe (build.js runs that when the exe
// is missing or older than its sources).
//
//   mdlmesh <file.mdl>                    the model's meshes as a draft
//   mdlmesh --model <draft.vfx> <name>    a draft as the game's dev layer reads it, written as its ASCII model
//   mdlmesh --anims <file.mdl>...         (LD: tools\data\oneburst.js) each model's animations and emitters: "# model
//                                         <file> compiled|ascii", then a line per animation "anim <name> length <s>
//                                         header <offset> events <offset> <count>" (ReadModelAnimations: offsets in
//                                         the file, 0 for an ASCII model's), a line per event "event <its animation's
//                                         index> <time> <name>", a line per emitter "emitter <name> <update>"; one it
//                                         can't read "# model <file> unreadable <why>"
//   mdlmesh --drop-events <anim> <event> <in.mdl> <out.mdl>
//                                         (LD) the model with that animation's events of that name taken out
//                                         (DropAnimationEvents: every other byte as it was), written to out.mdl:
//                                         "dropped <n>"
//   mdlmesh --bounds <file.mdl>...        (#166 part 2: tools\data\placeable_bounds.js) each model's visible geometry's
//                                         bounds in model space - every mesh drawn (render on), its vertices moved
//                                         through its node and each parent's position and turn up to the root:
//                                         "bounds <file> <x0> <x1> <y0> <y1> <z0> <z1> <meshes>"; "# model <file>
//                                         nomesh" when nothing is drawn; "# model <file> unreadable <why>"
// A mode with the wrong number of arguments, or an option it doesn't know: the usage line, exit 2.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "data/vfx_model.h"

namespace {

bool ReadFile(const char* name, std::string* bytes) {
  std::ifstream f(name, std::ios::binary);
  if (!f) return false;
  std::stringstream ss;
  ss << f.rdbuf();
  *bytes = ss.str();
  return true;
}

// A model's emitters as the model readers read them (compiled, else ASCII); false when neither reads it.
bool Emitters(const std::string& bytes, data::VfxEffect* effect, std::string* error) {
  if (data::ReadVfxBinaryModel(bytes, effect, error, false)) return true;
  std::string ascii_error;
  *effect = data::VfxEffect();
  if (data::ReadVfxAsciiModel(bytes, effect, &ascii_error)) return true;
  *error += " / " + ascii_error;
  return false;
}

// A node's place under its parent: its position and turn (axis x y z, angle in radians).
struct Placed {
  std::string parent;
  float position[3];
  float orientation[4];
};

// `p` turned by an axis-angle turn (Rodrigues), then moved by `position`.
void Through(const Placed& node, float p[3]) {
  float ax = node.orientation[0], ay = node.orientation[1], az = node.orientation[2];
  const float angle = node.orientation[3];
  const float length = std::sqrt(ax * ax + ay * ay + az * az);
  if (length > 1e-6f && std::fabs(angle) > 1e-6f) {
    ax /= length;
    ay /= length;
    az /= length;
    const float c = std::cos(angle), s = std::sin(angle);
    const float dot = ax * p[0] + ay * p[1] + az * p[2];
    const float cx = ay * p[2] - az * p[1], cy = az * p[0] - ax * p[2], cz = ax * p[1] - ay * p[0];
    const float x = p[0] * c + cx * s + ax * dot * (1 - c);
    const float y = p[1] * c + cy * s + ay * dot * (1 - c);
    const float z = p[2] * c + cz * s + az * dot * (1 - c);
    p[0] = x;
    p[1] = y;
    p[2] = z;
  }
  p[0] += node.position[0];
  p[1] += node.position[1];
  p[2] += node.position[2];
}

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// #166 part 2: the bounds of a model's drawn meshes in model space; false when none is drawn.
bool VisibleBounds(const data::VfxEffect& effect, float lo[3], float hi[3], int* meshes) {
  std::map<std::string, Placed> placed;
  for (const data::VfxNode& n : effect.nodes) {
    placed[Lower(n.name)] = Placed{Lower(n.parent), {n.position[0], n.position[1], n.position[2]},
                                   {n.orientation[0], n.orientation[1], n.orientation[2], n.orientation[3]}};
  }
  for (const data::VfxPlane& m : effect.planes) {
    placed[Lower(m.name)] = Placed{Lower(m.parent), {m.position[0], m.position[1], m.position[2]},
                                   {m.orientation[0], m.orientation[1], m.orientation[2], m.orientation[3]}};
  }
  *meshes = 0;
  bool any = false;
  for (const data::VfxPlane& m : effect.planes) {
    if (!m.render || m.verts.size() < 3) continue;
    ++*meshes;
    for (size_t i = 0; i + 2 < m.verts.size(); i += 3) {
      float p[3] = {m.verts[i], m.verts[i + 1], m.verts[i + 2]};
      std::string at = Lower(m.name);
      for (int depth = 0; depth < 64 && !at.empty(); ++depth) {  // up the parents to the root
        const auto it = placed.find(at);
        if (it == placed.end()) break;
        Through(it->second, p);
        at = it->second.parent;
      }
      for (int k = 0; k < 3; ++k) {
        if (!any || p[k] < lo[k]) lo[k] = p[k];
        if (!any || p[k] > hi[k]) hi[k] = p[k];
      }
      any = true;
    }
  }
  return any;
}

}  // namespace

int main(int argc, char** argv) {
  // A mode with the wrong number of arguments (or one it doesn't know) is a usage error, exit 2 - never a file to read
  // (LD fix round 1: "--drop-events" short of an argument read "--drop-events" as the model).
  const std::string mode = argc >= 2 ? argv[1] : "";
  const bool usage = argc < 2 || (mode == "--model" && argc != 4) || (mode == "--anims" && argc < 3) ||
                     (mode == "--drop-events" && argc != 6) || (mode == "--bounds" && argc < 3) ||
                     (mode.rfind("--", 0) == 0 && mode != "--model" && mode != "--anims" && mode != "--drop-events" &&
                      mode != "--bounds");
  if (usage) {
    std::fprintf(stderr,
                 "usage: mdlmesh <file.mdl> | mdlmesh --model <draft.vfx> <name> | mdlmesh --anims <file.mdl>... | "
                 "mdlmesh --drop-events <anim> <event> <in.mdl> <out.mdl> | mdlmesh --bounds <file.mdl>...\n");
    return 2;
  }
  if (mode == "--model") {
    std::ifstream d(argv[2], std::ios::binary);
    std::stringstream ds;
    ds << d.rdbuf();
    data::VfxEffect effect;
    std::string error;
    if (!data::ReadVfxDraft(ds.str(), &effect, &error)) {
      std::fprintf(stderr, "draft failed: %s\n", error.c_str());
      return 1;
    }
    std::cout << data::WriteVfxModel(effect, argv[3]);
    return 0;
  }
  if (mode == "--anims") {
    for (int i = 2; i < argc; ++i) {
      std::string bytes, error;
      std::vector<data::ModelAnimation> anims;
      if (!ReadFile(argv[i], &bytes)) {
        std::printf("# model %s unreadable can't open it\n", argv[i]);
        continue;
      }
      if (!data::ReadModelAnimations(bytes, &anims, &error)) {
        std::printf("# model %s unreadable %s\n", argv[i], error.c_str());
        continue;
      }
      const bool compiled = bytes.size() >= 4 && bytes.compare(0, 4, std::string(4, '\0')) == 0;  // as the reader tells them
      std::printf("# model %s %s\n", argv[i], compiled ? "compiled" : "ascii");
      for (size_t a = 0; a < anims.size(); ++a) {
        std::printf("anim %s length %g header %zu events %zu %zu\n", anims[a].name.c_str(), anims[a].length, anims[a].header,
                    anims[a].event_at, anims[a].events.size());
        for (const data::ModelEvent& e : anims[a].events) std::printf("event %zu %g %s\n", a, e.time, e.name.c_str());
      }
      data::VfxEffect effect;
      std::string emitter_error;
      if (Emitters(bytes, &effect, &emitter_error)) {
        for (const data::VfxEmitter& e : effect.emitters) std::printf("emitter %s %s\n", e.name.c_str(), e.update.c_str());
      } else {
        std::printf("# emitters unreadable %s\n", emitter_error.c_str());
      }
    }
    return 0;
  }
  if (mode == "--bounds") {
    for (int i = 2; i < argc; ++i) {
      std::string bytes, error;
      if (!ReadFile(argv[i], &bytes)) {
        std::printf("# model %s unreadable can't open it\n", argv[i]);
        continue;
      }
      data::VfxEffect effect;
      if (!data::ReadVfxBinaryModel(bytes, &effect, &error, true)) {
        std::string ascii_error;
        effect = data::VfxEffect();
        if (!data::ReadVfxAsciiModel(bytes, &effect, &ascii_error)) {
          std::printf("# model %s unreadable %s / %s\n", argv[i], error.c_str(), ascii_error.c_str());
          continue;
        }
      }
      float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
      int meshes = 0;
      if (!VisibleBounds(effect, lo, hi, &meshes)) {
        std::printf("# model %s nomesh\n", argv[i]);
        continue;
      }
      std::printf("bounds %s %.3f %.3f %.3f %.3f %.3f %.3f %d\n", argv[i], lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                  meshes);
    }
    return 0;
  }
  if (mode == "--drop-events") {
    std::string bytes;
    if (!ReadFile(argv[4], &bytes)) {
      std::fprintf(stderr, "can't open %s\n", argv[4]);
      return 1;
    }
    std::vector<data::ModelAnimation> anims;
    std::string error;
    if (!data::ReadModelAnimations(bytes, &anims, &error)) {
      std::fprintf(stderr, "%s: not a model it reads - %s\n", argv[4], error.c_str());
      return 1;
    }
    const int dropped = data::DropAnimationEvents(&bytes, argv[2], argv[3]);
    std::ofstream out(argv[5], std::ios::binary);
    if (!out || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
      std::fprintf(stderr, "can't write %s\n", argv[5]);
      return 1;
    }
    std::printf("dropped %d\n", dropped);
    return 0;
  }
  std::ifstream f(argv[1], std::ios::binary);
  if (!f) {
    std::fprintf(stderr, "can't open %s\n", argv[1]);
    return 1;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  data::VfxEffect effect;
  std::string error;
  const bool binary = data::ReadVfxBinaryModel(ss.str(), &effect, &error, true);
  if (!binary) {
    std::string ascii_error;
    effect = data::VfxEffect();
    if (!data::ReadVfxAsciiModel(ss.str(), &effect, &ascii_error)) {
      std::fprintf(stderr, "read failed: %s / %s\n", error.c_str(), ascii_error.c_str());
      return 1;
    }
  }
  std::cout << data::WriteVfxDraft(effect);
  std::cout << "# source " << (binary ? "binary" : "ascii") << "\n";
  for (const auto& p : effect.planes) {
    std::printf("# mesh %s flags 0x%X verts %zu faces %zu bitmap %s\n", p.name.c_str(), static_cast<unsigned>(p.flags),
                p.verts.size() / 3, p.faces.size() / 3, p.bitmap.c_str());
  }
  return 0;
}
