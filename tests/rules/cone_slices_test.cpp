#include <math.h>

#include "check.h"
#include "rules/cone_slices.h"

using namespace rules;

// #9 c. Breaks caught: a drawn cone other than the nearest model, a span off centre or of the wrong count, a slice's
// middle off by half a slice, a bearing put in the slice beside it, the facing's sign read backwards.
TEST(ConeSlicesSpanTheDrawnCone) {
  CHECK_EQ(DrawnConeDegrees(90), 90);
  CHECK_EQ(DrawnConeDegrees(100), 90);
  CHECK_EQ(DrawnConeDegrees(110), 120);
  CHECK_EQ(DrawnConeDegrees(10), 60);
  CHECK_EQ(DrawnConeDegrees(300), 150);
  CHECK_EQ(ConeSliceSpan(90).first, 2);
  CHECK_EQ(ConeSliceSpan(90).count, 6);
  CHECK_EQ(ConeSliceSpan(60).first, 3);
  CHECK_EQ(ConeSliceSpan(60).count, 4);
  CHECK_EQ(ConeSliceSpan(150).first, 0);
  CHECK_EQ(ConeSliceSpan(150).count, 10);
  CHECK_EQ(ConeSliceSpan(120).first, 1);
  CHECK_EQ(ConeSliceSpan(120).count, 8);
  CHECK_EQ(ConeSliceSpan(180).count, 10);  // never more than the positions
  CHECK_EQ(ConeSliceSpan(180).first, 0);
  CHECK_EQ(ConeSliceMiddle(0) == -67.5f, true);
  CHECK_EQ(ConeSliceMiddle(5) == 7.5f, true);
  CHECK_EQ(ConeSliceMiddle(9) == 67.5f, true);
  CHECK_EQ(ConeSliceAt(0.0f), 5);       // straight ahead: the slice just left of the facing's line
  CHECK_EQ(ConeSliceAt(-0.1f), 4);
  CHECK_EQ(ConeSliceAt(14.9f), 5);
  CHECK_EQ(ConeSliceAt(15.0f), 6);
  CHECK_EQ(ConeSliceAt(-75.0f), 0);
  CHECK_EQ(ConeSliceAt(74.9f), 9);
  CHECK_EQ(ConeSliceAt(75.0f), -1);     // past the widest cone
  CHECK_EQ(ConeSliceAt(-75.1f), -1);
  CHECK_EQ(ConeSliceAt(180.0f), -1);
  // Each slice's middle falls in it.
  for (int k = 0; k < kConeSlicePositions; ++k) CHECK_EQ(ConeSliceAt(ConeSliceMiddle(k)), k);
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
  CHECK_EQ(ConeSliceLengthAtMost(8.0f), 6);
  CHECK_EQ(ConeSliceLengthAtMost(20.0f), 10);
  CHECK_EQ(ConeSliceLengthAtMost(99.0f), kConeSliceLengthCount - 1);
  int tests = 0;
  float wall = 9.3f;
  const auto clear = [&](int i) {
    ++tests;
    return kConeSliceLengths[i] < wall;
  };
  // No hint: a search over 0 .. 20 m finds 8 m.
  CHECK_EQ(ConeSliceReach(10, -1, clear), 6);
  CHECK_EQ(tests <= 4, true);
  // The engine's block point (9.3 m: the hint is 8 m): one test.
  tests = 0;
  CHECK_EQ(ConeSliceReach(10, ConeSliceLengthAtMost(wall), clear), 6);
  CHECK_EQ(tests, 1);
  // A hint that is itself blocked (a bad block point): searched below it.
  tests = 0;
  wall = 3.5f;
  CHECK_EQ(ConeSliceReach(10, 6, clear), 2);
  // A hint past the top is ignored.
  CHECK_EQ(ConeSliceReach(4, 9, clear), 2);
  // A wall at the creature's feet: nothing drawn.
  wall = 0.5f;
  CHECK_EQ(ConeSliceReach(10, -1, clear), -1);
  CHECK_EQ(ConeSliceReach(10, 0, clear), -1);
  // Clear all the way (the caller found the whole line blocked only further out): the top.
  wall = 100.0f;
  CHECK_EQ(ConeSliceReach(10, -1, clear), 10);
  CHECK_EQ(ConeSliceReach(4, 9, clear), 4);  // never past the top, whatever the hint
  CHECK_EQ(ConeSliceReach(-1, -1, clear), -1);
  // Every wall distance: the longest drawn length short of it.
  for (int w = 15; w <= 600; w += 7) {
    wall = w / 10.0f;
    const int want = ConeSliceLengthAtMost(wall - 0.001f);
    CHECK_EQ(ConeSliceReach(kConeSliceLengthCount - 1, -1, clear), want);
  }
}

// Breaks caught: rows overlapping between colours or positions, the last row past 3059, a row read back as another
// slice, a bad part given a row.
TEST(ConeSliceRowsRoundTrip) {
  CHECK_EQ(ConeSliceRow(0, 0, 0), 2260);
  CHECK_EQ(ConeSliceRow(0, 0, 15), 2275);
  CHECK_EQ(ConeSliceRow(0, 1, 0), 2276);
  CHECK_EQ(ConeSliceRow(1, 0, 0), 2420);
  CHECK_EQ(ConeSliceRow(4, 9, 15), 3059);
  CHECK_EQ(kConeSliceRowLast, 3059);
  CHECK_EQ(ConeSliceRow(5, 0, 0), 0);
  CHECK_EQ(ConeSliceRow(0, 10, 0), 0);
  CHECK_EQ(ConeSliceRow(0, 0, 16), 0);
  CHECK_EQ(ConeSliceRow(-1, 0, 0), 0);
  int bad = 0;
  for (int c = 0; c < kConeSliceColours; ++c) {
    for (int k = 0; k < kConeSlicePositions; ++k) {
      for (int i = 0; i < kConeSliceLengthCount; ++i) {
        int cc = -1, kk = -1, ii = -1;
        if (!ConeSliceRowParts(ConeSliceRow(c, k, i), &cc, &kk, &ii) || cc != c || kk != k || ii != i) ++bad;
      }
    }
  }
  CHECK_EQ(bad, 0);
  int c = 0, k = 0, i = 0;
  CHECK_EQ(ConeSliceRowParts(2259, &c, &k, &i), false);
  CHECK_EQ(ConeSliceRowParts(3060, &c, &k, &i), false);
}

// The rule and the drawing agree. Breaks caught: a hider past its slice's length seen, one in an undrawn slice (or
// outside the slices) seen, the length read from another slice, the edge of the length not seen.
TEST(ConeSlicesDecideWhatTheRuleSees) {
  int rows[kConeSlicePositions] = {};
  // A 90-degree cone (positions 2-7), red: 20 m but for slices 5 (cut at 3 m: a wall ahead-left) and 6 (8 m).
  for (int k = 2; k <= 7; ++k) rows[k] = ConeSliceRow(4, k, ConeSliceLengthAtMost(20.0f));
  rows[5] = ConeSliceRow(4, 5, ConeSliceLengthAtMost(3.4f));
  rows[6] = ConeSliceRow(4, 6, ConeSliceLengthAtMost(8.0f));
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 2.9f), true);    // in front of the wall
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 3.0f), true);    // at the slice's end
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 3.2f), false);   // past where its slice is cut
  CHECK_EQ(SeenInsideSlices(rows, 5.0f, 12.0f), false);
  CHECK_EQ(SeenInsideSlices(rows, 20.0f, 7.9f), true);   // slice 6
  CHECK_EQ(SeenInsideSlices(rows, 20.0f, 8.5f), false);
  CHECK_EQ(SeenInsideSlices(rows, -5.0f, 12.0f), true);  // slice 4: uncut
  CHECK_EQ(SeenInsideSlices(rows, -5.0f, 20.5f), false);  // past its sight
  CHECK_EQ(SeenInsideSlices(rows, 50.0f, 1.0f), false);  // slice 8: not drawn (outside the 90 degrees)
  CHECK_EQ(SeenInsideSlices(rows, 120.0f, 1.0f), false);  // beside it
  // A row from another position is not this slice's.
  rows[3] = ConeSliceRow(4, 2, ConeSliceLengthAtMost(20.0f));
  CHECK_EQ(SeenInsideSlices(rows, -25.0f, 5.0f), false);
  // Signatures: none for nothing drawn, another for any change.
  int none[kConeSlicePositions] = {};
  CHECK_EQ(ConeSliceSignature(none), 0);
  const int before = ConeSliceSignature(rows);
  CHECK_EQ(before != 0, true);
  rows[6] = ConeSliceRow(4, 6, ConeSliceLengthAtMost(10.0f));
  CHECK_EQ(ConeSliceSignature(rows) != before, true);
  rows[6] = ConeSliceRow(3, 6, ConeSliceLengthAtMost(10.0f));  // the colour alone
  const int orange = ConeSliceSignature(rows);
  CHECK_EQ(orange != before && orange != 0, true);
}
