#pragma once

#include <stdint.h>

// The client's own smoothing of a creature nwn5e moves on the server (#11,
// 0.58.7; owner, 2026-10-31, G2-11: "the enemy sometimes teleports to the
// final location, instead of sliding"; decided "go with your recommendation" -
// client-side smoothing first). The server side says what it moves; each frame
// the client's position setter for that creature gets nwn5e's point instead of
// the network's (rules/smoothing.h). Single player: the client runs in the
// server's process, on its thread; another player's client keeps the engine's
// own smoothing.

// A slide (a push): the model goes from `from` to `to` over `ms`, as the
// server's slide does, then is handed back to the client (SmoothHandBack).
void SmoothSlide(uint32_t id, float from_x, float from_y, float to_x, float to_y, uint32_t ms);
// Held (a grapple's drag): the model is at the server creature's own position
// each frame.
void SmoothFollow(uint32_t id);
// The slide cut short or the hold over: held where it is until the client
// catches up, then handed back.
void SmoothStop(uint32_t id);

void InstallClientSmoothing();
