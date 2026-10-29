#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "rules/test_profile.h"

using namespace rules;

// Breaks caught (#82): a profile in data\tests that !test would refuse in game
// (a bad key, value, item or switch), one with no name for the !test list, or
// a file name !test <profile> can't reach.
TEST(TestProfilesInDataParse) {
  int profiles = 0;
  for (const auto& entry : std::filesystem::directory_iterator("data/tests")) {
    if (entry.path().extension() != ".txt") continue;
    ++profiles;
    std::ifstream file(entry.path(), std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    const TestProfile profile = ParseTestProfile(text.str());
    if (!profile.errors.empty()) {
      printf("  %s: %s\n", entry.path().filename().string().c_str(), profile.errors[0].c_str());
    }
    CHECK_EQ(profile.errors.empty(), true);
    CHECK_EQ(profile.name.empty(), false);
    CHECK_EQ(IsTestProfileName(entry.path().stem().string()), true);
  }
  CHECK_EQ(profiles >= 9, true);
  CHECK_EQ(std::filesystem::exists("data/tests/monk.txt"), true);
}
