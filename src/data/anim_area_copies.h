#pragma once

// #161 VA-5 (owner, 2026-11-14: "it's better, but there's still a permanent spawning and despawning in the exact center
// of the area ingame"; then "A": the game patched): the client's area loader (CNWCAreaOfEffectObject::LoadEffect) gives
// each copy of group 1 (the row's MODEL01) the start time k x 300 ms, and its update skips a copy whose start is 0 as
// "nothing pending" - so group 1's copy 0 is never placed: it stays at the area's centre, cycling there, in every area
// (the game's own too). core/anim_area_scatter sets that start to kFirstCopyStartMs after the loader, so the update
// places it like its siblings. A group 1 of one copy keeps it at the centre (a flask's single splash lands there).
//
// AH (#39; owner, 2026-11-19: "go on the area fix"): the same hook, for nwn5e's own models only (the game's areas
// untouched) - (1) the loader gives every copy a white highlight (Hilite slot 3, colour (1, 1, 1): added to the
// material's self-illumination, so a tint is lost - clamped to white - or washed out with enhanced lighting); the hook
// takes it off again, so tints and Glow draw as on a creature play; (2) a lone group 1 copy is never placed, so it never
// gets the area's impact-then-duration sequence (it blinked); the hook starts that sequence on it where it stands.
// Pure: unit-tested.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "data/anim_area_engine.h"

namespace data {

constexpr int kFirstCopyStartMs = 1;

// The start time group 1's copy 0 gets after the game's loader: kFirstCopyStartMs when the loader left it never placed
// (start 0, not alive) and group 1 has several copies; else `start0` unchanged.
int FirstCopyStart(int group1_count, int start0, int alive0);

// AH, which copies are ours: a copy's model name (its CResRef - `size` chars at most, not always zero-terminated)
// starts "n5", in any case. nwn5e names every model it ships or plays so (n5_..., n5v_..., n5t_..., the VFX Bench's
// n5vx / n5va plays) and none of the game's 113,538 resources (its keys and ovr\) starts so. Not the row: nwn5e's own
// area rows also show the game's models (Entangle's, the fogs', the flasks'), made for the highlight.
bool IsOwnAreaModel(const char* name, size_t size);

// AH, a lone copy: whether the hook starts group 1's copy 0 itself, in place - the copy the update never places (its
// group's only one: VA-5 places the first of several), left by the loader at start 0, not alive and never animated
// (state 0), its model ours.
bool StartsLoneCopy(int group1_count, int start0, int alive0, int state0, bool own_model);

// AH's layout guard: an area's counts as the loader leaves them - 1..kMaxAreaCopies copies in all (the area's update and
// SetAnimation index a copy by a byte), group 1's 0..all of them. Anything else: the hook leaves the area alone.
constexpr int kMaxAreaCopies = 255;
bool AreaCopiesLaidOut(int total, int group1_count);

// A byte pattern in core/hooks' syntax ("48 8B ?? 78": hex bytes, ?? any) as its bytes, -1 for any.
std::vector<int> BytePattern(const char* text);
// Whether `pattern` matches the bytes at `code` (as many as the pattern is long, all readable).
bool BytesMatch(const uint8_t* code, const std::vector<int>& pattern);

// AH fix round 1 (the review's I1): whether `anchor`'s value is its pattern's bytes at its place - `size` (1-4)
// little-endian bytes from `at`, inside the pattern, none of them ??. Every one of area_engine::kOffsetAnchors must be:
// a pattern found in the game then proves the offset the hook reads with.
bool OffsetAnchored(const area_engine::OffsetAnchor& anchor);

// AH fix round 1 (the review's M1): LoadEffect's direct calls to Hilite - each the address of its E8, as FindCallsTo gives
// them - as the loader makes them: exactly area_engine::kHiliteSlot3Calls, each the end of kHiliteSlot3CallPattern
// (glow 0, slot kAreaHiliteSlot); reads the kHiliteSlot3CallAt bytes before each. The hook and the game test share it.
bool AreaHiliteCallsAsLoaded(const std::vector<const uint8_t*>& calls);

}  // namespace data
