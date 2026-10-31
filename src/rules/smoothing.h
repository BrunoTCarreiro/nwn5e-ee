#pragma once

// The client's smoothing of a creature nwn5e moves on the server
// (core/client_smoothing.cpp; #11, 0.58.7; owner, 2026-10-31, G2-11: "the enemy
// sometimes teleports to the final location, instead of sliding"; decided "go
// with your recommendation"). The client gets the server's positions at its
// network rate and smooths a move the server makes for a creature that isn't
// walking only under 4 m (a longer one snaps) and up to 512 ms late
// (HandleServerToPlayerCreatureUpdate_Update, read 89.8193.37). A push's slide
// is drawn by the client itself, each frame, along the server's line; this is
// its arithmetic.

namespace rules {

struct SmoothPoint {
  float x = 0.0f, y = 0.0f;
};

// The slide's point after `elapsed_ms` of `duration_ms`: straight along the
// line, as the server's (rules_push.cpp: SlideAll); its start before it begins,
// its end once the time is up (or for a slide of no length of time).
SmoothPoint SlidePoint(SmoothPoint from, SmoothPoint to, double elapsed_ms, double duration_ms);

// After the slide's time is up the model stays at its end until the client's
// own target for it (the last network position) has caught up - within
// kSmoothHandBackMetres of the end - or kSmoothHandBackAfterMs have passed:
// letting go sooner would have the client walk it back toward a mid-slide
// position for up to 512 ms. True: let go.
constexpr float kSmoothHandBackMetres = 0.05f;
constexpr double kSmoothHandBackAfterMs = 1000.0;
bool SmoothHandBack(SmoothPoint to, SmoothPoint network_target, double elapsed_ms, double duration_ms);

}  // namespace rules
