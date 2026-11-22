#pragma once

// A searching enemy's glances (#9; owner, 2026-11-22: "the sweeping 120 cone is very very annoying" - "1.A"): instead
// of a cone sweeping on its own, a searching creature keeps the calm cone's width on its real facing and turns to
// look - glances of min_degrees .. max_degrees, the first toward where it heard the hider when that is away from it,
// then to either side in turn, each held min_ms .. max_ms - only while it stands (core/stealth_watch.cpp). Its cone
// moves only when it turns. nwn5e.ini: search_glance_min_degrees / _max_degrees, search_glance_min_ms / _max_ms. This
// file only decides; the engine side turns the creature.

namespace rules {

struct GlanceTuning {
  int min_degrees = 45;  // how far a glance turns from where its stand began
  int max_degrees = 90;
  int min_ms = 1500;     // how long it holds each glance
  int max_ms = 2500;
};

// The keys as read, made sane: degrees 5-180, ms 300-10000, a pair given high first swapped.
GlanceTuning SaneGlance(GlanceTuning tuning);

// The facing of a stand's `n`-th glance (0 first), degrees anticlockwise from east (0 up to 360). `base`: its facing
// when the stand began. `toward` (when `has_toward`): the facing toward where it heard the hider - the first glance
// turns there, and later ones swing round it. The others turn by min .. max degrees (`roll`, 0 up to 1) to alternate
// sides of the stand's line, the first of them to the left when `left_first`.
float GlanceFacing(const GlanceTuning& tuning, float base, int n, double roll, bool has_toward, float toward,
                   bool left_first);

// How long a glance is held, ms: min .. max by `roll` (0 up to 1).
int GlanceHoldMs(const GlanceTuning& tuning, double roll);

// Whether a creature that has stood still `still_ms` may glance now: settled (300 ms) and its last glance's hold over.
bool GlanceDue(int still_ms, bool hold_over);

}  // namespace rules
