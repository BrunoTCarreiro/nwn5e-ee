#pragma once
// The dev layer's way in (nwn5e-tools\dev, docs/BENCH-REDESIGN.md): the tools' in-game parts,
// compiled into this DLL only when that checkout sits beside this one and the build isn't "public".
// Shipped code reaches them only through these pointers, null in every DLL without the layer.

struct DevSeam {
  // !animtrace: a model's PlayAnimation (anim_layers.cpp), AddDriveAction (drive_fix.cpp), the
  // client creature update (stealth_ghost.cpp).
  void (*anim_play)(void* gob, const char* name, float speed, int flag, float value) = nullptr;
  void (*anim_drive)(void* creature, const float* target, int facing, int a, int keys, int c) = nullptr;
  void (*anim_client)(void* client_creature) = nullptr;
};
extern DevSeam g_dev_seam;

#ifdef NWN5E_DEV
void InstallDev();  // nwn5e-tools\dev\dev_layer.cpp: installs the layer and fills g_dev_seam
#endif
