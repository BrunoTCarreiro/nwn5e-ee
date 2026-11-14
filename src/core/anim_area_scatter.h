#pragma once

// #161 VA-5 (owner, 2026-11-14: "A"): every area's group 1 copy 0 placed like its siblings (data/anim_area_copies.h):
// a hook after the client's CNWCAreaOfEffectObject::LoadEffect - not exported; found by its own warning text, its copy
// arrays checked by four byte patterns (the loader's and the update's), so a game whose area code differs keeps its own
// (logged "SKIP area scatter"). Logs "area scatter: ...".
void InstallAreaScatter();  // from InstallAnimLayers
