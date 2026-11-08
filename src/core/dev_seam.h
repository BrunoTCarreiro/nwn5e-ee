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
  // #144 (the animation session for the VFX bench, 2026-11-08; the owner: "yes, go for it"): core fills this
  // one (vfx_nodes.cpp, once its attach hooks are in; null otherwise) and the dev layer calls it. `model` goes on
  // `node` of the target's model (upright: its position only, else turning with it) whatever its visualeffects
  // row says; an empty `node` removes that (back to the row's NWN5E_Node, or none). Applies to attaches after the
  // call. False for an empty model or a bad node name (1-31 letters, digits or _).
  bool (*set_effect_node)(const char* model, const char* node, bool upright) = nullptr;
};
extern DevSeam g_dev_seam;

#ifdef NWN5E_DEV
void InstallDev();  // nwn5e-tools\dev\dev_layer.cpp: installs the layer and fills g_dev_seam
#endif
