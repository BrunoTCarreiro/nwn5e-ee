#include <ctype.h>
#include <stdlib.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "check.h"
#include "data/twoda.h"
#include "rules/nwn_conversion.h"
#include "rules/skills.h"

using namespace data;

// C5's data (tools/data/class_skills.py), read from the repo root where
// test.bat runs, against the rules. Breaks caught: a class table marking a
// skill off the 2024 class list as a class skill (or the reverse) — the page
// would show it as one; a page row missing from a class's table (the page
// wouldn't list that skill); nwn5e's rows 28-34 left out of a table; the page
// hiding a page row, or showing one of a skill's other rows.

namespace {

std::string ReadText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

int Column(const TwoDA& table, const char* name) {
  for (size_t i = 0; i < table.columns.size(); ++i) {
    if (table.columns[i] == name) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

TEST(ClassSkillTablesAreThe2024Lists) {
  static const char* const kSuffix[] = {"barb", "bard", "cler", "dru", "fight", "monk",
                                        "pal",  "rang", "rog",  "sorc", "wiz"};
  for (int nwn_class = 0; nwn_class <= 10; ++nwn_class) {
    const std::string path = std::string("data/files/cls_sk5e_") + kSuffix[nwn_class] + ".2da";
    TwoDA table;
    std::string error;
    CHECK_EQ(ParseTwoDA(ReadText(path), &table, &error), true);
    const int index_column = Column(table, "SkillIndex"), class_column = Column(table, "ClassSkill");
    CHECK_EQ(index_column >= 0 && class_column >= 0, true);
    std::map<int, int> class_skill;  // skills.2da row -> ClassSkill
    for (const auto& row : table.rows) class_skill[atoi(row[index_column].c_str())] = atoi(row[class_column].c_str());
    const auto character_class = rules::ClassFromNwnId(nwn_class);
    CHECK_EQ(character_class.has_value(), true);
    const rules::SkillSet list = rules::SkillsPageAllowance(*character_class, rules::Species2024::kOther).class_list;
    for (int i = 0; i < rules::kSkill2024Count; ++i) {
      const rules::Skill2024 skill = static_cast<rules::Skill2024>(i);
      const int row = rules::SkillsPageRow(skill);
      if (row < 0) continue;  // a tool
      if (row == rules::kNwnPerform && nwn_class != 1) {
        CHECK_EQ(class_skill.count(row), 0u);  // Perform: a Bard's only, as in NWN
        continue;
      }
      if (class_skill.count(row) != 1 || class_skill[row] != (rules::Has(list, skill) ? 1 : 0)) {
        printf("  %s: row %d (%s) ClassSkill %d\n", path.c_str(), row, rules::Skill2024Name(skill),
               class_skill.count(row) ? class_skill[row] : -1);
      }
      CHECK_EQ(class_skill.count(row), 1u);
      CHECK_EQ(class_skill[row], rules::Has(list, skill) ? 1 : 0);
    }
    for (int row = rules::kNwnOwnSkillCount; row < rules::kNwnSkillCount; ++row) CHECK_EQ(class_skill.count(row), 1u);
  }
}

TEST(SkillsPageHidesAllButOneRowPerSkill) {
  std::istringstream rows(ReadText("data/rows/skills.if_skills2024.txt"));
  std::string line;
  int checked = 0;
  while (std::getline(rows, line)) {
    if (line.empty() || line[0] == '#' || !isdigit(static_cast<unsigned char>(line[0]))) continue;
    std::istringstream fields(line);
    int row = -1, hide = -1;
    fields >> row >> hide;
    CHECK_EQ(hide, rules::IsSkillsPageRow(row) ? 0 : 1);
    ++checked;
  }
  CHECK_EQ(checked, rules::kNwnOwnSkillCount);
}
