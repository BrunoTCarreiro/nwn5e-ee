#pragma once

// #161 VA-5 (owner, 2026-11-14: "A"): every area's group 1 copy 0 placed like its siblings (data/anim_area_copies.h):
// a hook after the client's CNWCAreaOfEffectObject::LoadEffect - not exported; found by its own warning text, its copy
// arrays checked by four byte patterns (the loader's and the update's), so a game whose area code differs keeps its own
// (logged "SKIP area scatter"). Logs "area scatter: ...".
// AH (#39; owner, 2026-11-19: "go on the area fix"), in the same hook, for copies of nwn5e's own models only (an "n5"
// name; the game's models, in any row, untouched): each loses the white highlight the loader gives every copy
// (CNWCAnimBase::UnHilite(3)), so its tint and Glow draw as on a creature play; a lone group 1 copy (never placed) gets
// the area's own impact-then-duration sequence where it stands (CNWCAreaOfEffectObject::SetAnimation(1, 0)), so it fades
// in once and stays. Their functions and offsets (data/anim_area_engine.h) are checked by byte patterns first; any
// missing: logged "SKIP area copies", VA-5 alone. Logs "area copies: ...".
void InstallAreaScatter();  // from InstallAnimLayers
