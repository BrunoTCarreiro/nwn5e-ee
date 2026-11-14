#include "check.h"
#include "data/anim_area_copies.h"

using namespace data;

// #161 VA-5: group 1's copy 0, left at start 0 by the game's loader (never placed: it stays at the centre), gets a start
// of 1 ms so the update places it like the others - when group 1 has several copies.
TEST(AreaFirstCopyScattered) {
  CHECK_EQ(FirstCopyStart(5, 0, 0), kFirstCopyStartMs);  // Entangle's 5 copies: copy 0 placed too
  CHECK_EQ(FirstCopyStart(2, 0, 0), kFirstCopyStartMs);
  CHECK_EQ(kFirstCopyStartMs, 1);
}

// A lone copy keeps the centre (a flask's splash), and a copy the loader did give a time, or one already alive, is the
// game's own.
TEST(AreaFirstCopyOtherwiseUnchanged) {
  CHECK_EQ(FirstCopyStart(1, 0, 0), 0);
  CHECK_EQ(FirstCopyStart(0, 0, 0), 0);
  CHECK_EQ(FirstCopyStart(5, 300, 0), 300);
  CHECK_EQ(FirstCopyStart(5, 0, 2000), 0);
  CHECK_EQ(FirstCopyStart(-1, 0, 0), 0);
}
