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

// #9 (owner, 2026-11-22: "2. Mostly A, but if there's an illuminated patch further out, there should not be an island
// of vision, it should form a contiguous slice of detectability"). Breaks caught: a lit patch beyond a dark stretch
// not reached (an island dropped, or the slice cut at the dark), the farthest lit point not the one taken, darkvision
// ignored or reaching past the wall, samples past the cut, a slice drawn where nothing is lit, sampling not from far
// to near (more samples than needed).
TEST(LightShortensASliceContiguously) {
  const auto near = [](float a, float b) { return fabsf(a - b) < 0.001f; };
  int samples = 0;
  // A torch at the guard's feet (lit 0-4 m) and a lit patch at 12-14 m, dark between: the slice reaches 14 m.
  const auto torch_and_patch = [&](float d) {
    ++samples;
    return d <= 4.0f || (d >= 12.0f && d <= 14.0f);
  };
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 0.0f, torch_and_patch), 14.0f), true);
  CHECK_EQ(samples, 7);  // 20, 19 ... 14: from far to near, stopping at the first lit
  // Only the torch: 4 m.
  const auto torch = [](float d) { return d <= 4.0f; };
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 0.0f, torch), 4.0f), true);
  // The patch past the wall (cut at 10 m): only the torch counts.
  CHECK_EQ(near(SeeableReach(10.0f, 1.0f, 0.0f, torch_and_patch), 4.0f), true);
  // All dark: nothing; with darkvision, its 18 m.
  const auto dark = [](float) { return false; };
  CHECK_EQ(SeeableReach(20.0f, 1.0f, 0.0f, dark) == 0.0f, true);
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 18.0f, dark), 18.0f), true);
  CHECK_EQ(near(SeeableReach(12.0f, 1.0f, 18.0f, dark), 12.0f), true);  // darkvision past the wall: the wall
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 18.0f, torch), 18.0f), true);  // a torch inside its darkvision: 18 m
  // Darkvision and a lit patch beyond it: the patch.
  const auto far_patch = [](float d) { return d >= 19.5f; };
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 18.0f, far_patch), 20.0f), true);
  // Lit all along (daylight): the cut, one sample.
  samples = 0;
  const auto day = [&](float) {
    ++samples;
    return true;
  };
  CHECK_EQ(near(SeeableReach(20.0f, 1.0f, 0.0f, day), 20.0f), true);
  CHECK_EQ(samples, 1);
  // No samples past the cut, none at or under 0.
  float farthest = 0.0f, nearest = 100.0f;
  const auto record = [&](float d) {
    farthest = d > farthest ? d : farthest;
    nearest = d < nearest ? d : nearest;
    return false;
  };
  SeeableReach(7.5f, 1.0f, 0.0f, record);
  CHECK_EQ(near(farthest, 7.5f) && nearest > 0.0f, true);
  CHECK_EQ(SeeableReach(0.0f, 1.0f, 18.0f, day) == 0.0f, true);  // a wall at its feet
}
