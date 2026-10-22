#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "rules/feats2024.h"

// C7a's data (tools/data/asi_feats.py, tools/data/packft_rows.py), read from
// the repo root where test.bat runs, against the rules. Breaks caught: a row
// whose label (its abilities) isn't the choice rules::AsiFeatFromId reads
// from its id — the Feats page would show one increase and nwn5e apply
// another; a choice under the wrong ASI's master; a package's ASI entry
// pointing at another choice than its label says.

namespace {

std::string ExpectedLabel(int feat) {
  static const char* const kShort[] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
  const auto choice = rules::AsiFeatFromId(feat);
  if (!choice) return "";
  std::string label = "NWN5E_ASI_" + std::to_string(choice->slot) + "_" + kShort[static_cast<int>(choice->increase.first)];
  if (choice->increase.second) label += std::string("_") + kShort[static_cast<int>(*choice->increase.second)];
  return label;
}

std::vector<std::vector<std::string>> DataRows(const std::string& path) {
  std::ifstream file(path);
  std::vector<std::vector<std::string>> rows;
  std::string line;
  bool header = false;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    if (!header) {  // the column names
      header = true;
      continue;
    }
    std::istringstream words(line);
    std::vector<std::string> row;
    std::string word;
    while (words >> word) row.push_back(word);
    rows.push_back(row);
  }
  return rows;
}

}  // namespace

TEST(AsiChoiceRowsMatchTheRules) {
  const auto rows = DataRows("data/rows/feat.asi2024.txt");
  CHECK_EQ(rows.size(), static_cast<size_t>(rules::kAsiSplits * rules::kAsiFeatCount));
  for (const auto& row : rows) {
    const int feat = atoi(row[0].c_str());
    const auto choice = rules::AsiFeatFromId(feat);
    CHECK_EQ(choice.has_value(), true);
    if (row[1] != ExpectedLabel(feat)) printf("  feat %d: label %s, rules %s\n", feat, row[1].c_str(), ExpectedLabel(feat).c_str());
    CHECK_EQ(row[1] == ExpectedLabel(feat), true);
    CHECK_EQ(atoi(row[9].c_str()), rules::kAsiMasterFirst + choice->slot - 1);  // MASTERFEAT
  }
  int entries = 0;
  for (const char* table : {"packftfight1", "packftfight2", "packftwiz1", "packftrog1", "packftbard1"}) {
    for (const auto& row : DataRows(std::string("data/rows/") + table + ".txt")) {
      const int feat = atoi(row[1].c_str());
      if (!rules::AsiFeatFromId(feat)) continue;
      CHECK_EQ(row[2] == ExpectedLabel(feat), true);
      ++entries;
    }
  }
  CHECK_EQ(entries, 5 * 4 * rules::kAsiFeatCount);
}
