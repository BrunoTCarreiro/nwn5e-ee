#include "check.h"
#include "rules/grapple.h"

using namespace rules;

namespace {
constexpr int kTiny = 1, kSmall = 2, kMedium = 3, kLarge = 4, kHuge = 5;
int Milli(float metres) { return static_cast<int>(metres * 1000.0f + 0.5f); }
}  // namespace

// SRD 5.2.1 p. 190: the Grapple's DC (and its escape DC) is the Shove's, 8 +
// Str + PB; the target at most one size larger. Breaks caught: the DC missing
// PB or Strength, a size limit of two (NWN's Improved Knockdown) or zero.
TEST(GrappleDcAndSizeAre2024) {
  CHECK_EQ(GrappleDc(3, 2), 13);
  CHECK_EQ(GrappleDc(-1, 4), 11);
  CHECK_EQ(GrappleSizeAllowed(kMedium, kLarge), true);
  CHECK_EQ(GrappleSizeAllowed(kMedium, kHuge), false);
  CHECK_EQ(GrappleSizeAllowed(kSmall, kMedium), true);
  CHECK_EQ(GrappleSizeAllowed(kSmall, kLarge), false);
  CHECK_EQ(GrappleSizeAllowed(kMedium, kTiny), true);
  CHECK_EQ(Milli(kShovePushMetres), 1524);  // 5 ft
}

// p. 182: dragging costs 1 extra foot (half speed) unless it is Tiny or two or
// more sizes smaller; the Grappler feat (p. 87): no extra for your size or
// smaller. Breaks caught: one size smaller counted free, Tiny charged, the feat
// freeing a larger creature.
TEST(DraggingCostsHalfSpeed) {
  CHECK_EQ(DragCostsExtra(kMedium, kMedium, false), true);
  CHECK_EQ(DragCostsExtra(kMedium, kSmall, false), true);
  CHECK_EQ(DragCostsExtra(kMedium, kTiny, false), false);
  CHECK_EQ(DragCostsExtra(kLarge, kSmall, false), false);
  CHECK_EQ(DragCostsExtra(kSmall, kTiny, false), false);
  CHECK_EQ(DragCostsExtra(kMedium, kLarge, false), true);
  CHECK_EQ(DragCostsExtra(kMedium, kMedium, true), false);
  CHECK_EQ(DragCostsExtra(kMedium, kLarge, true), true);
  CHECK_EQ(Milli(DragSpeedFactor(kMedium, kMedium, false)), 500);
  CHECK_EQ(Milli(DragSpeedFactor(kLarge, kSmall, false)), 1000);
}

// p. 182: Disadvantage on attacks against anyone but the grappler; GT3: a
// creature the game moves escapes unless it was fighting its grappler; the
// escape is the better of Athletics and Acrobatics.
TEST(GrappledCreatureAttacksAndEscapes) {
  CHECK_EQ(GrappledAttackHasDisadvantage(true), false);
  CHECK_EQ(GrappledAttackHasDisadvantage(false), true);
  CHECK_EQ(GrappledCreatureTriesToEscape(true), false);
  CHECK_EQ(GrappledCreatureTriesToEscape(false), true);
  CHECK_EQ(EscapeCheckBonus(5, 2), 5);
  CHECK_EQ(EscapeCheckBonus(1, 7), 7);
}

// GT2 (owner, 2026-10-27): a creature at least one size smaller; 5 ft x Str
// modifier, at least 5 ft; 1d6 per 10 ft (at least 1d6), half on a success.
// Breaks caught: throwing your own size, a Str 10 throw of 0 ft, 15 ft rounding
// up to 2d6, a success taking full damage.
TEST(ThrowIsTheHouseRule) {
  CHECK_EQ(ThrowSizeAllowed(kMedium, kSmall), true);
  CHECK_EQ(ThrowSizeAllowed(kMedium, kTiny), true);
  CHECK_EQ(ThrowSizeAllowed(kMedium, kMedium), false);
  CHECK_EQ(ThrowSizeAllowed(kLarge, kMedium), true);
  CHECK_EQ(ThrowFeet(4), 20);  // Str 18
  CHECK_EQ(ThrowFeet(5), 25);
  CHECK_EQ(ThrowFeet(1), 5);
  CHECK_EQ(ThrowFeet(0), 5);
  CHECK_EQ(ThrowFeet(-2), 5);
  CHECK_EQ(Milli(FeetToMetres(20)), 6096);
  CHECK_EQ(ThrowDamageDice(5), 1);
  CHECK_EQ(ThrowDamageDice(15), 1);
  CHECK_EQ(ThrowDamageDice(20), 2);
  CHECK_EQ(ThrowDamageDice(25), 2);
  CHECK_EQ(ThrowDamageDice(50), 5);
  CHECK_EQ(ThrowDamageTaken(9, false), 9);
  CHECK_EQ(ThrowDamageTaken(9, true), 4);
}
