#include <stdlib.h>

#include <fstream>
#include <set>
#include <string>

#include "check.h"
#include "rules/species.h"

// data/strings.txt (read from the repo root, where test.bat runs): one string
// per line, `\n` written as two characters. Breaks caught: a raw line break in
// a string (the loader keeps the first line and skips the rest as "string id
// 0" — the background descriptions were cut to their first line for two
// versions), an id outside nwn5e's range, an id given twice, a string the
// creation swaps (rules/species.h) point at that isn't in the file.
TEST(StringsFileHasOneStringPerLine) {
  std::ifstream file("data/strings.txt");
  CHECK_EQ(file.good(), true);
  std::set<unsigned long> ids;
  int bad_lines = 0, repeated = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    char* end = nullptr;
    const unsigned long id = strtoul(line.c_str(), &end, 10);
    if (end != line.c_str() + 8 || *end != ' ' || id < 16000000ul || id >= 16500000ul) {
      ++bad_lines;
      printf("  bad line in data/strings.txt: %.60s\n", line.c_str());
      continue;
    }
    if (!ids.insert(id).second) ++repeated;
  }
  CHECK_EQ(bad_lines, 0);
  CHECK_EQ(repeated, 0);
  for (uint32_t strref : {148u, 162u, 481u, 485u, 32u, 33u, 4922u, 251u, 252u, 253u, 254u, 255u, 256u, 257u, 143u,
                          146u, 483u, 159u, 160u, 487u}) {
    const auto swap = rules::CreationStringSwap2024(strref);
    CHECK_EQ(swap.has_value() && ids.count(swap->nwn5e) == 1, true);
  }
}
