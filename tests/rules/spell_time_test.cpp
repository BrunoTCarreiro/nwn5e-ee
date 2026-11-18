#include "check.h"
#include "rules/spell_time.h"

using namespace rules;

// #39 (owner, 2026-11-17: "go with A. no real-time clock"): durations in real time; a rest ends what it outlasts.
TEST(SpellTimeRealSeconds) {
  CHECK_EQ(SpellSeconds(1), 60);       // a minute: 10 rounds of 6 seconds
  CHECK_EQ(SpellSeconds(10), 600);
  CHECK_EQ(SpellSeconds(60), 3600);    // an hour: Floating Disk, Disguise Self, Longstrider
  CHECK_EQ(SpellSeconds(480), 28800);  // 8 hours: Mage Armor, Alarm
  CHECK_EQ(SpellSeconds(0), 0);
  CHECK_EQ(SpellSeconds(-5), 0);
  CHECK_EQ(kShortRestSeconds, 3600);
  CHECK_EQ(kLongRestSeconds, 28800);
}

TEST(SpellTimeRestsEndWhatTheyOutlast) {
  CHECK_EQ(RestEnds(kShortRestSeconds, 3600), true);   // an hour left: the Short Rest's hour ends it
  CHECK_EQ(RestEnds(kShortRestSeconds, 3601), false);
  CHECK_EQ(RestEnds(kLongRestSeconds, 28800), true);   // Mage Armor cast in the morning
  CHECK_EQ(RestEnds(kLongRestSeconds, 28801), false);
  CHECK_EQ(RestEnds(kShortRestSeconds, 0), true);
}

TEST(SpellTimeRestsEndOurAreas) {
  CHECK_EQ(RestEndsSpellArea("NWN5E_ENTANGLE", false), true);
  CHECK_EQ(RestEndsSpellArea("NWN5E_GREASE", false), true);
  CHECK_EQ(RestEndsSpellArea("NWN5E_FOG_CLOUD_3", false), true);
  CHECK_EQ(RestEndsSpellArea("NWN5E_ALARM", false), false);  // 8 hours: a Short Rest doesn't end it
  CHECK_EQ(RestEndsSpellArea("NWN5E_ALARM", true), true);
  CHECK_EQ(RestEndsSpellArea("VFX_PER_AURA_FEAR", true), false);  // a creature's aura
  CHECK_EQ(RestEndsSpellArea("NWN5E_PUDDLE", true), false);       // a flask's puddle
  CHECK_EQ(RestEndsSpellArea("", true), false);
}
