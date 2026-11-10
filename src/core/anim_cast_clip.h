#pragma once

// A cast's animation swapped on one caster for a clip of nwn5e's while the cast plays (#150; owner: "go with your
// recommendation for #150" - every cantrip casts n5_cantrip, castout's arms on the combat-ready legs, #149).
//
// RequestCastClip swaps `base` for `clip` on `creature` at once - EE's ReplaceObjectAnimation, as the flourishes and
// stance_look do - only when `clip`'s recipe was built and the body is part-based (appearance.2da MODELTYPE P: the
// skeletons the animation packs serve); else nothing changes and `base` plays. It maps `base` back to itself after
// `seconds` (data::CastClipMs: 0.1 - 10 s), or at once when a new request for the same creature replaces it or
// EndCastClip ends it. A cast's held loop (`base`lp - castout, then castoutlp while the cast's time runs) is swapped
// for `clip`lp the same way when that clip is built, so the body doesn't change between the two. It plays nothing
// itself: no PlayAnimation, so nothing is cut and no action is cleared - the engine plays `base` as it would, and that
// is the clip while the swap stands.

#include <string>
#include <vector>

void RequestCastClip(void* creature, const char* base, const std::string& clip, float seconds);

// Several at once - a look's conjure and cast (owner, 2026-11-10: "can I create other conjure setups?"): every pair
// swapped for the same `seconds`, as one swap (RequestCastClip is this with one pair). A pair whose clip isn't built
// keeps its base; nothing is swapped when none is.
struct CastClipSwap {
  std::string base, clip;
};
void RequestCastClips(void* creature, const std::vector<CastClipSwap>& swaps, float seconds);

// Maps the creature's swap back at once (a spell that isn't a cantrip cast inside the swap's time); nothing if none.
void EndCastClip(void* creature);

// Registers the creature-update listener that maps swaps back (InstallAnimLayers calls it); logged if it can't.
void InstallCastClip();
