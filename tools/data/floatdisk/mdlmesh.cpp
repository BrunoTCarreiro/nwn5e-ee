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
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "data/vfx_model.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: mdlmesh <file.mdl> | mdlmesh --model <draft.vfx> <name>\n");
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
