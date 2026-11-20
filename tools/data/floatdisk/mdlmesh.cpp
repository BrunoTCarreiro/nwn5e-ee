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
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
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

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: mdlmesh <file.mdl> | mdlmesh --model <draft.vfx> <name> | mdlmesh --anims <file.mdl>... | "
                 "mdlmesh --drop-events <anim> <event> <in.mdl> <out.mdl>\n");
    return 2;
  }
  if (std::string(argv[1]) == "--model" && argc == 4) {
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
  if (std::string(argv[1]) == "--anims") {
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
  if (std::string(argv[1]) == "--drop-events" && argc == 6) {
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
