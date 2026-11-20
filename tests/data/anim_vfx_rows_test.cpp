#include <stdio.h>
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "check.h"
#include "data/look_rows.h"

// visualeffects.2da's rows are shared by every rows file (data/rows/visualeffects*.txt) and the shipped looks
// (data/looks/visualeffects*.look.txt). A clash costs a whole file at launch: the looks' row server refuses a looks
// file naming a row nwn5e's rows already use (data/look_rows.h; 0.99.46's launch check lost the looks' file to Detect
// Magic's first row, 2100), and two rows files on one row overwrite each other's columns. So: no row in two files, and
// no rows file inside the looks' 2100-2199.

namespace {

// Each row number a rows file gives, with its file: "<row> ..." lines after the header (a "+" row has no number).
void CollectRows(const std::filesystem::path& folder, const std::string& suffix, bool rows_file,
                 std::map<int, std::string>* owner, int* clashes, int* in_looks_range, int* files) {
  if (!std::filesystem::exists(folder)) return;
  for (const auto& entry : std::filesystem::directory_iterator(folder)) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("visualeffects.", 0) != 0 || name.size() < suffix.size() ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    ++*files;
    std::ifstream file(entry.path(), std::ios::binary);
    std::string line;
    bool header = false;
    while (std::getline(file, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const size_t first = line.find_first_not_of(" \t");
      if (first == std::string::npos || line[first] == '#') continue;
      if (!header) {
        header = true;
        continue;
      }
      std::istringstream words(line);
      std::string word;
      words >> word;
      if (word.empty() || word[0] < '0' || word[0] > '9') continue;
      const int row = atoi(word.c_str());
      const auto it = owner->find(row);
      if (it != owner->end()) {
        printf("    visualeffects row %d: %s and %s\n", row, it->second.c_str(), name.c_str());
        ++*clashes;
      } else {
        owner->emplace(row, name);
      }
      if (rows_file && row >= data::kLookVisualEffectsFirst && row <= data::kLookVisualEffectsLast) {
        printf("    visualeffects row %d (%s) is the looks' (%d-%d)\n", row, name.c_str(), data::kLookVisualEffectsFirst,
               data::kLookVisualEffectsLast);
        ++*in_looks_range;
      }
    }
  }
}

}  // namespace

TEST(VisualEffectsRowFilesNeverShareARow) {
  std::map<int, std::string> owner;
  int clashes = 0, in_looks_range = 0, files = 0;
  CollectRows("data/rows", ".txt", true, &owner, &clashes, &in_looks_range, &files);  // visualeffects.txt, .<part>.txt
  const int rows_files = files;
  CollectRows("data/looks", ".look.txt", false, &owner, &clashes, &in_looks_range, &files);
  CHECK_EQ(clashes, 0);
  CHECK_EQ(in_looks_range, 0);
  CHECK_EQ(rows_files >= 9, true);  // visualeffects.txt and its 9 parts (bleed ... stealth, tally, debris)
  CHECK_EQ(files - rows_files >= 2, true);
  // #171's final marker (TF): the three spells' tallies, 33 rows from 2215, a file of their own (the first four-disc
  // marker at 2200-2210 and the TP probe at 2211-2214 were retired 2026-11-20: those rows are free)
  for (int row = 2215; row <= 2247; ++row) {
    CHECK_EQ(owner.count(row) == 1 && owner[row] == "visualeffects.tally.txt", true);
  }
  // #172's destruction effects (DE): the ten debris rows and the light's death, after the tallies, a file of their own
  for (int row = 2248; row <= 2258; ++row) {
    CHECK_EQ(owner.count(row) == 1 && owner[row] == "visualeffects.debris.txt", true);
  }
  CHECK_EQ(owner.count(2259), 0u);
}
