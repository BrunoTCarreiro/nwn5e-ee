#include <math.h>

#include "check.h"
#include "rules/cone_slices.h"

using namespace rules;

// #9 c. Breaks caught: a drawn cone other than the nearest multiple of 30, a span off centre or of the wrong count, a
// slice's middle off by half a slice, a bearing put in the slice beside it or not wrapped round, the facing's sign
// read backwards.
TEST(ConeSlicesSpanTheDrawnCone) {
  CHECK_EQ(DrawnConeDegrees(90), 90);
  CHECK_EQ(DrawnConeDegrees(100), 90);
  CHECK_EQ(DrawnConeDegrees(110), 120);
  CHECK_EQ(DrawnConeDegrees(120), 120);
  CHECK_EQ(DrawnConeDegrees(10), 30);
  CHECK_EQ(DrawnConeDegrees(300), 300);
  CHECK_EQ(DrawnConeDegrees(400), 330);
  CHECK_EQ(ConeSliceSpan(90).first, 9);   // the calm cone: positions 9-14, -45 .. +45
  CHECK_EQ(ConeSliceSpan(90).count, 6);
  CHECK_EQ(ConeSliceSpan(60).first, 10);
  CHECK_EQ(ConeSliceSpan(60).count, 4);
  CHECK_EQ(ConeSliceSpan(150).first, 7);
  CHECK_EQ(ConeSliceSpan(150).count, 10);
  CHECK_EQ(ConeSliceSpan(120).first, 8);
  CHECK_EQ(ConeSliceSpan(120).count, 8);
  CHECK_EQ(ConeSliceSpan(400).count, 24);  // never more than the circle
  CHECK_EQ(ConeSliceSpan(120, 4).first, 12);  // swung 60 degrees left
  CHECK_EQ(ConeSliceSpan(120, -4).first, 4);
  CHECK_EQ(ConeSliceSpan(120, -12).first, 20);  // wrapped round
  CHECK_EQ(InSliceSpan(ConeSliceSpan(90), 9), true);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(90), 14), true);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(90), 15), false);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(90), 8), false);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(120, -12), 23), true);  // 20-23 and 0-3
  CHECK_EQ(InSliceSpan(ConeSliceSpan(120, -12), 3), true);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(120, -12), 4), false);
  CHECK_EQ(InSliceSpan(ConeSliceSpan(120, -12), 19), false);
  CHECK_EQ(ConeSliceMiddle(0) == -172.5f, true);
  CHECK_EQ(ConeSliceMiddle(12) == 7.5f, true);
  CHECK_EQ(ConeSliceMiddle(23) == 172.5f, true);
  CHECK_EQ(ConeSliceAt(0.0f), 12);      // straight ahead: the slice just left of the facing's line
  CHECK_EQ(ConeSliceAt(-0.1f), 11);
  CHECK_EQ(ConeSliceAt(14.9f), 12);
  CHECK_EQ(ConeSliceAt(15.0f), 13);
  CHECK_EQ(ConeSliceAt(-75.0f), 7);
  CHECK_EQ(ConeSliceAt(75.0f), 17);
  CHECK_EQ(ConeSliceAt(179.9f), 23);
  CHECK_EQ(ConeSliceAt(180.0f), 0);     // behind
  CHECK_EQ(ConeSliceAt(-180.0f), 0);
  CHECK_EQ(ConeSliceAt(-179.9f), 0);
  CHECK_EQ(ConeSliceAt(370.0f), 12);    // wrapped
  CHECK_EQ(ConeSliceAt(-190.0f), 23);   // wrapped the other way
  CHECK_EQ(ConeSliceAt(-550.0f), 23);   // more than a turn round
  // Each slice's middle falls in it.
  int off_middle = 0;
  for (int k = 0; k < kConeSlicePositions; ++k) off_middle += ConeSliceAt(ConeSliceMiddle(k)) != k;
  CHECK_EQ(off_middle, 0);
  CHECK_EQ(fabsf(DegreesOffFacing(0.0f, 1.0f, 1.0f, 0.0f) - 90.0f) < 0.01f, true);    // left of an east facing
  CHECK_EQ(fabsf(DegreesOffFacing(0.0f, -1.0f, 1.0f, 0.0f) + 90.0f) < 0.01f, true);   // right of it
  CHECK_EQ(fabsf(DegreesOffFacing(-1.0f, -0.01f, 1.0f, 0.0f) + 179.4f) < 0.1f, true);  // behind, wrapped
  CHECK_EQ(fabsf(DegreesOffFacing(1.0f, 1.0f, 0.0f, 1.0f) + 45.0f) < 0.01f, true);    // facing north, north-east
  CHECK_EQ(DegreesOffFacing(0.0f, 0.0f, 1.0f, 0.0f) == 0.0f, true);
  // Across the west: facing 170 degrees, looking at -170 is 20 to the left; the other way round, 20 to the right.
  const float r = 3.14159265f / 180.0f;
  CHECK_EQ(fabsf(DegreesOffFacing(cosf(-170 * r), sinf(-170 * r), cosf(170 * r), sinf(170 * r)) - 20.0f) < 0.01f, true);
  CHECK_EQ(fabsf(DegreesOffFacing(cosf(170 * r), sinf(170 * r), cosf(-170 * r), sinf(-170 * r)) + 20.0f) < 0.01f, true);
}

// Breaks caught: a length rounded up (the drawing past the wall), the shortest length not drawn, a search that
// skips the hint, takes a blocked hint, or stops one short; a clear line searched again.
TEST(ConeSliceReachRoundsDownToTheWall) {
  // The models' lengths (tools/data/stealth_rings.py's SLICE_LENGTHS).
  const float lengths[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 50};
  int differ = 0;
  for (int i = 0; i < kConeSliceLengthCount; ++i) differ += kConeSliceLengths[i] != lengths[i];
  CHECK_EQ(differ, 0);
  CHECK_EQ(ConeSliceLengthAtMost(0.9f), -1);
  CHECK_EQ(ConeSliceLengthAtMost(1.0f), 0);
  CHECK_EQ(ConeSliceLengthAtMost(7.9f), 5);   // 6 m, not 8
  CHECK_EQ(ConeSliceLengthAtMost(6.5f), 5);
  CHECK_EQ(ConeSliceLengthAtMost(8.0f), 6);
  CHECK_EQ(ConeSliceLengthAtMost(20.0f), 10);
  CHECK_EQ(ConeSliceLengthAtMost(99.0f), kConeSliceLengthCount - 1);
  int tests = 0;
  float wall = 9.3f;
  const auto clear = [&](int i) {
    ++tests;
    return kConeSliceLengths[i] < wall;
  };
  CHECK_EQ(ConeSliceReach(10, -1, clear), 6);  // no hint: a search over 0 .. 20 m finds 8 m
  CHECK_EQ(tests <= 4, true);
  tests = 0;
  CHECK_EQ(ConeSliceReach(10, ConeSliceLengthAtMost(wall), clear), 6);  // the engine's block point: one test
  CHECK_EQ(tests, 1);
  tests = 0;
  wall = 3.5f;
  CHECK_EQ(ConeSliceReach(10, 6, clear), 2);  // a hint that is itself blocked: searched below it
  CHECK_EQ(ConeSliceReach(4, 9, clear), 2);   // a hint past the top is ignored
  wall = 0.5f;
  CHECK_EQ(ConeSliceReach(10, -1, clear), -1);  // a wall at its feet: nothing drawn
  CHECK_EQ(ConeSliceReach(10, 0, clear), -1);
  wall = 100.0f;
  CHECK_EQ(ConeSliceReach(10, -1, clear), 10);
  CHECK_EQ(ConeSliceReach(4, 9, clear), 4);  // never past the top, whatever the hint
  CHECK_EQ(ConeSliceReach(-1, -1, clear), -1);
  int wrong = 0;
  for (int w = 15; w <= 600; w += 7) {
    wall = w / 10.0f;
    wrong += ConeSliceReach(kConeSliceLengthCount - 1, -1, clear) != ConeSliceLengthAtMost(wall - 0.001f);
  }
  CHECK_EQ(wrong, 0);
}

// Breaks caught: the front ten off #9 part 1's rows, the others overlapping them or each other, the last row past
// 4179, a row read back as another slice, a bad part given a row.
TEST(ConeSliceRowsRoundTrip) {
  CHECK_EQ(ConeSliceRow(0, 7, 0), 2260);    // part 1's position 0 (-75 .. -60)
  CHECK_EQ(ConeSliceRow(0, 7, 15), 2275);
  CHECK_EQ(ConeSliceRow(0, 8, 0), 2276);
  CHECK_EQ(ConeSliceRow(1, 7, 0), 2420);
  CHECK_EQ(ConeSliceRow(4, 16, 15), 3059);
  CHECK_EQ(ConeSliceRow(0, 0, 0), 3060);    // the sweep's
  CHECK_EQ(ConeSliceRow(0, 6, 15), 3171);
  CHECK_EQ(ConeSliceRow(0, 17, 0), 3172);
  CHECK_EQ(ConeSliceRow(1, 0, 0), 3284);
  CHECK_EQ(ConeSliceRow(4, 23, 15), 4179);
  CHECK_EQ(kConeSliceRearRowLast, 4179);
  CHECK_EQ(ConeSliceRow(5, 0, 0), 0);
  CHECK_EQ(ConeSliceRow(0, 24, 0), 0);
  CHECK_EQ(ConeSliceRow(0, 0, 16), 0);
  CHECK_EQ(ConeSliceRow(-1, 0, 0), 0);
  CHECK_EQ(ConeSliceRow(0, -1, 0), 0);
  int bad = 0, out = 0;
  for (int c = 0; c < kConeSliceColours; ++c) {
    for (int k = 0; k < kConeSlicePositions; ++k) {
      for (int i = 0; i < kConeSliceLengthCount; ++i) {
        const int row = ConeSliceRow(c, k, i);
        int cc = -1, kk = -1, ii = -1;
        if (!ConeSliceRowParts(row, &cc, &kk, &ii) || cc != c || kk != k || ii != i) ++bad;
        if (row < 2260 || row > 4179) ++out;
      }
    }
  }
  CHECK_EQ(bad, 0);
  CHECK_EQ(out, 0);
  int c = 0, k = 0, i = 0;
  CHECK_EQ(ConeSliceRowParts(2259, &c, &k, &i), false);
  CHECK_EQ(ConeSliceRowParts(4180, &c, &k, &i), false);
}

// The rule and the drawing agree. Breaks caught: a hider past its slice's length seen, one in an undrawn slice seen,
// the length read from another slice, the edge of the length not seen.
TEST(ConeSlicesDecideWhatTheRuleSees) {
  int rows[kConeSlicePositions] = {};
  // A 90-degree cone (positions 9-14), red: 20 m but for slices 12 (cut at 3 m: a wall ahead-left) and 13 (8 m).
  for (int k = 9; k <= 14; ++k) rows[k] = ConeSliceRow(4, k, ConeSliceLengthAtMost(20.0f));
  rows[12] = ConeSliceRow(4, 12, ConeSliceLengthAtMost(3.4f));
  rows[13] = ConeSliceRow(4, 13, ConeSliceLengthAtMost(8.0f));
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 2.9f), true);    // in front of the wall
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 3.0f), true);    // at the slice's end
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 3.2f), false);   // past where its slice is cut
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 12.0f), false);
  CHECK_EQ(SeenInsideSlices(rows, 20.0f, 7.9f), true);   // slice 13
  CHECK_EQ(SeenInsideSlices(rows, 20.0f, 8.5f), false);
  CHECK_EQ(SeenInsideSlices(rows, -5.0f, 12.0f), true);  // slice 11: uncut
  CHECK_EQ(SeenInsideSlices(rows, -5.0f, 20.5f), false);  // past its sight
  CHECK_EQ(SeenInsideSlices(rows, 50.0f, 1.0f), false);  // slice 15: not drawn
  CHECK_EQ(SeenInsideSlices(rows, 179.0f, 1.0f), false);  // behind it
  rows[10] = ConeSliceRow(4, 9, ConeSliceLengthAtMost(20.0f));  // a row from another position is not this slice's
  CHECK_EQ(SeenInsideSlices(rows, -25.0f, 5.0f), false);
  rows[0] = ConeSliceRow(2, 0, ConeSliceLengthAtMost(5.0f));     // behind, a searching cone swung round
  CHECK_EQ(SeenInsideSlices(rows, -178.0f, 4.0f), true);
  CHECK_EQ(SeenInsideSlices(rows, 180.0f, 4.0f), true);
  CHECK_EQ(SeenInsideSlices(rows, 178.0f, 4.0f), false);  // position 23: not drawn
  int none[kConeSlicePositions] = {};
  CHECK_EQ(ConeSliceSignature(none), 0);
  const int before = ConeSliceSignature(rows);
  CHECK_EQ(before != 0, true);
  rows[13] = ConeSliceRow(4, 13, ConeSliceLengthAtMost(10.0f));
  CHECK_EQ(ConeSliceSignature(rows) != before, true);
  rows[13] = ConeSliceRow(3, 13, ConeSliceLengthAtMost(10.0f));  // the colour alone
  const int orange = ConeSliceSignature(rows);
  CHECK_EQ(orange != before && orange != 0, true);
}

// The sweeping cone (owner, 2026-11-22: "A, go with the sweeping cone"). Breaks caught: the defaults off the ini's,
// keys not made sane, the swing not starting on the facing, going right first, overshooting or not reaching its
// ends, no pause there, the period off, a negative time; a step rounded the wrong way.
TEST(SearchSweepSwingsAndPauses) {
  const SearchSweep def;
  CHECK_EQ(def.cone_degrees == 120 && def.sweep_degrees == 60 && def.period_ms == 4000 && def.pause_ms == 500, true);
  const SearchSweep same = SaneSearchSweep(def);
  CHECK_EQ(same.cone_degrees == 120 && same.sweep_degrees == 60 && same.period_ms == 4000 && same.pause_ms == 500,
           true);
  SearchSweep raw;
  raw.cone_degrees = 100;
  raw.sweep_degrees = 300;
  raw.period_ms = 200;
  raw.pause_ms = 5000;
  const SearchSweep sane = SaneSearchSweep(raw);
  CHECK_EQ(sane.cone_degrees, 90);
  CHECK_EQ(sane.sweep_degrees, 180);
  CHECK_EQ(sane.period_ms, 1000);
  CHECK_EQ(sane.pause_ms, 300);
  raw = SearchSweep();
  raw.sweep_degrees = -5;
  raw.pause_ms = -1;
  raw.period_ms = 999999;
  CHECK_EQ(SaneSearchSweep(raw).sweep_degrees, 0);
  CHECK_EQ(SaneSearchSweep(raw).pause_ms, 0);
  CHECK_EQ(SaneSearchSweep(raw).period_ms, 60000);
  const auto at = [&](int64_t ms) { return SweepAngle(def, ms); };
  const auto near = [](float a, float b) { return fabsf(a - b) < 0.01f; };
  // 4 s: on the facing, 750 ms up to +60 (left), held 500 ms, 1.5 s across to -60, held 500 ms, 750 ms back.
  CHECK_EQ(near(at(0), 0.0f), true);
  CHECK_EQ(near(at(375), 30.0f), true);
  CHECK_EQ(near(at(750), 60.0f), true);
  CHECK_EQ(near(at(1000), 60.0f), true);   // held at the end
  CHECK_EQ(near(at(1249), 60.0f), true);
  CHECK_EQ(near(at(2000), 0.0f), true);    // back across the facing
  CHECK_EQ(near(at(2750), -60.0f), true);
  CHECK_EQ(near(at(3000), -60.0f), true);  // held at the other end
  CHECK_EQ(near(at(3625), -30.0f), true);
  CHECK_EQ(near(at(4000), 0.0f), true);    // a whole swing
  CHECK_EQ(near(at(4375), 30.0f), true);
  CHECK_EQ(near(at(-500), 0.0f), true);
  float lowest = 0.0f, highest = 0.0f;
  for (int64_t ms = 0; ms < 8000; ms += 10) {
    const float a = at(ms);
    lowest = a < lowest ? a : lowest;
    highest = a > highest ? a : highest;
  }
  CHECK_EQ(near(lowest, -60.0f) && near(highest, 60.0f), true);
  SearchSweep still = def;
  still.sweep_degrees = 0;
  CHECK_EQ(SweepAngle(still, 1234) == 0.0f, true);
  CHECK_EQ(SweepStep(0.0f), 0);
  CHECK_EQ(SweepStep(7.4f), 0);
  CHECK_EQ(SweepStep(7.6f), 1);
  CHECK_EQ(SweepStep(-7.6f), -1);
  CHECK_EQ(SweepStep(60.0f), 4);
  CHECK_EQ(SweepStep(-60.0f), -4);
}

// Breaks caught: the swing's range short of its ends or past the circle, the rule's cone not where the drawing's is
// (its step), a hider behind the swing seen.
TEST(SearchSweepConeIsWhereItPoints) {
  const SearchSweep def;
  CHECK_EQ(SweepRange(def).first, 4);   // -120 .. +120: 16 positions
  CHECK_EQ(SweepRange(def).count, 16);
  SearchSweep wide = def;
  wide.sweep_degrees = 150;
  CHECK_EQ(SweepRange(wide).first, 0);
  CHECK_EQ(SweepRange(wide).count, 24);
  SearchSweep fixed = def;
  fixed.sweep_degrees = 0;
  CHECK_EQ(SweepRange(fixed).first, 8);
  CHECK_EQ(SweepRange(fixed).count, 8);
  // On the facing (0 ms): -60 .. +60.
  CHECK_EQ(InSweepCone(def, 0, 0.0f), true);
  CHECK_EQ(InSweepCone(def, 0, 59.0f), true);
  CHECK_EQ(InSweepCone(def, 0, 61.0f), false);
  CHECK_EQ(InSweepCone(def, 0, -59.0f), true);
  CHECK_EQ(InSweepCone(def, 0, -61.0f), false);
  // Swung left (1000 ms, +60): 0 .. +120 - a hider to its right is behind the sweep.
  CHECK_EQ(InSweepCone(def, 1000, 110.0f), true);
  CHECK_EQ(InSweepCone(def, 1000, 1.0f), true);
  CHECK_EQ(InSweepCone(def, 1000, -1.0f), false);
  CHECK_EQ(InSweepCone(def, 1000, -40.0f), false);
  // Swung right (3000 ms, -60): -120 .. 0.
  CHECK_EQ(InSweepCone(def, 3000, -110.0f), true);
  CHECK_EQ(InSweepCone(def, 3000, 40.0f), false);
  // Part way (375 ms, +30: step 2): -30 .. +90.
  CHECK_EQ(InSweepCone(def, 375, 89.0f), true);
  CHECK_EQ(InSweepCone(def, 375, -29.0f), true);
  CHECK_EQ(InSweepCone(def, 375, -31.0f), false);
  CHECK_EQ(InSweepCone(def, 375, 91.0f), false);
  // Never behind it with the defaults.
  int behind = 0;
  for (int64_t ms = 0; ms < 4000; ms += 50) behind += InSweepCone(def, ms, 179.0f) || InSweepCone(def, ms, -179.0f);
  CHECK_EQ(behind, 0);
}
