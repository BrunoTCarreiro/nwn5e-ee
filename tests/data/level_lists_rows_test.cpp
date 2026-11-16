#include "check.h"
#include "data/twoda.h"
#include "rules/level_lists.h"

#include <fstream>
#include <string>
#include <vector>

namespace {

struct FileRow {
  int row = -1;
  std::vector<std::string> cells;  // Bard, Cleric, Druid, Paladin, Ranger, Wiz_Sorc
};

std::vector<FileRow> ReadClassColumns(const char* path) {
  static const char* const kColumns[] = {"Bard", "Cleric", "Druid", "Paladin", "Ranger", "Wiz_Sorc"};
  std::vector<FileRow> rows;
  std::ifstream file(path, std::ios::binary);
  CHECK_EQ(file.good(), true);
  std::string line;
  std::vector<std::string> head;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string> parts = data::Tokenize(line);
    if (head.empty()) {
      head = parts;
      continue;
    }
    FileRow row;
    row.row = std::stoi(parts[0]);
    for (const char* column : kColumns) {
      std::string value = "(none)";
      for (size_t i = 0; i < head.size(); ++i) {
        if (head[i] == column && i + 1 < parts.size()) value = parts[i + 1];
      }
      row.cells.push_back(value);
    }
    rows.push_back(row);
  }
  return rows;
}

std::string Cell(int level) { return level == rules::kNoLevel ? "****" : std::to_string(level); }

void CheckMatches(const char* path, const std::vector<rules::ClassListRow>& table) {
  const std::vector<FileRow> rows = ReadClassColumns(path);
  CHECK_EQ(rows.size(), table.size());
  for (size_t i = 0; i < rows.size() && i < table.size(); ++i) {
    CHECK_EQ(rows[i].row, table[i].row);
    for (int column = 0; column < 6; ++column) {
      CHECK_EQ(rows[i].cells[column] == Cell(rules::ListLevel(table[i], column)), true);
    }
  }
}

}  // namespace

// #39 Part 2a's end: the served rows files are rules/level_lists.h's tables, row by row and class by class.
TEST(Level1ListSwitchRows) {
  CheckMatches("data/rows/spells.if_spells2024.lists1.txt", rules::Level1ListSwitch());
  CheckMatches("data/rows/spells.if_retiresummons.txt", rules::RetiredSummons());
}
