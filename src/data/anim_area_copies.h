#pragma once

// #161 VA-5 (owner, 2026-11-14: "it's better, but there's still a permanent spawning and despawning in the exact center
// of the area ingame"; then "A": the game patched): the client's area loader (CNWCAreaOfEffectObject::LoadEffect) gives
// each copy of group 1 (the row's MODEL01) the start time k x 300 ms, and its update skips a copy whose start is 0 as
// "nothing pending" - so group 1's copy 0 is never placed: it stays at the area's centre, cycling there, in every area
// (the game's own too). core/anim_area_scatter sets that start to kFirstCopyStartMs after the loader, so the update
// places it like its siblings. A group 1 of one copy keeps it at the centre (a flask's single splash lands there).
// Pure: unit-tested.

namespace data {

constexpr int kFirstCopyStartMs = 1;

// The start time group 1's copy 0 gets after the game's loader: kFirstCopyStartMs when the loader left it never placed
// (start 0, not alive) and group 1 has several copies; else `start0` unchanged.
int FirstCopyStart(int group1_count, int start0, int alive0);

}  // namespace data
