#include "check.h"
#include "data/twoda.h"

#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Every served rows file parses as the DLL reads it (0.88.1: a second header line in spells.if_cantrips2024.txt
// made the game skip all of spells.2da): its first line not blank or a comment names columns, every other line is
// a row number (or +) and one value per named column.
TEST(RowsFilesParse) {
  int files = 0;
  for (const auto& entry : std::filesystem::directory_iterator("data/rows")) {
    if (entry.path().extension() != ".txt") continue;
    std::ifstream file(entry.path(), std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    std::istringstream lines(text.str());
    std::string line, header;
    while (std::getline(lines, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const size_t first = line.find_first_not_of(" \t");
      if (first == std::string::npos || line[first] == '#') continue;
      header = line;
      break;
    }
    data::TwoDA table;
    table.columns = data::Tokenize(header);
    std::string error;
    int changed = 0;
    const bool parsed = data::ApplyRows(&table, text.str(), &error, &changed);
    if (!parsed) printf("  %s: %s\n", entry.path().filename().string().c_str(), error.c_str());
    CHECK_EQ(parsed, true);
    ++files;
  }
  CHECK_EQ(files > 50, true);
}
