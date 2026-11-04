#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "data/twoda.h"
#include "rules/spell_slots.h"

using namespace data;

// #36 Part 1: the served slot tables (data/files/cls_spgn_5e*.2da, classes.2da's SpellGainTable with spellcasting
// faithful) against rules::SpellGain2024 - SRD 5.2's tables. Breaks caught: a table's slot or cantrip count off the
// rules, a level missing, NumSpellLevels not the highest spell level + 1.

namespace {

std::string ReadText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

int Column(const TwoDA& table, const std::string& name) {
  for (size_t i = 0; i < table.columns.size(); ++i) {
    if (table.columns[i] == name) return static_cast<int>(i);
  }
  return -1;
}

int Value(const std::string& cell) { return cell == kEmpty ? 0 : atoi(cell.c_str()); }

}  // namespace

TEST(SpellGainTablesAreSrd52) {
  struct Table {
    const char* suffix;
    int class_row;
  };
  static const Table kTables[] = {{"bard", 1}, {"cler", 2}, {"dru", 3}, {"pal", 6}, {"rang", 7}, {"sorc", 9}, {"wiz", 10}};
  for (const Table& t : kTables) {
    const std::string path = std::string("data/files/cls_spgn_5e") + t.suffix + ".2da";
    TwoDA table;
    std::string error;
    CHECK_EQ(ParseTwoDA(ReadText(path), &table, &error), true);
    CHECK_EQ(table.rows.size(), 20u);
    const int level_column = Column(table, "Level"), count_column = Column(table, "NumSpellLevels");
    CHECK_EQ(level_column >= 0 && count_column >= 0, true);
    const rules::CasterKind kind = rules::CasterKindOfClass(t.class_row);
    for (size_t r = 0; r < table.rows.size(); ++r) {
      const int level = Value(table.rows[r][level_column]);
      CHECK_EQ(level, static_cast<int>(r) + 1);
      CHECK_EQ(Value(table.rows[r][count_column]), rules::MaxSpellLevel2024(kind, level) + 1);
      for (int spell_level = 0; spell_level <= 9; ++spell_level) {
        const int column = Column(table, "SpellLevel" + std::to_string(spell_level));
        CHECK_EQ(column >= 0, true);
        const int got = Value(table.rows[r][column]), want = rules::SpellGain2024(t.class_row, level, spell_level);
        if (got != want) printf("  %s level %d spell level %d: %d, the rules %d\n", path.c_str(), level, spell_level, got, want);
        CHECK_EQ(got, want);
      }
    }
  }
}
