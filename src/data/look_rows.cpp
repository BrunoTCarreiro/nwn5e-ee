#include "data/look_rows.h"

#include <stdlib.h>
#include <string.h>

#include <sstream>
#include <vector>

namespace data {

namespace {

const char* const kSpellColumns[] = {
    "ConjAnim",       "ConjHeadVisual", "ConjHandVisual", "ConjGrndVisual", "ConjSoundVFX",  "ConjSoundMale",
    "ConjSoundFemale", "CastAnim",      "CastHeadVisual", "CastHandVisual", "CastGrndVisual", "CastSound",
    "Proj",           "ProjModel",      "ProjType",       "ProjSpwnPoint",  "ProjSound",     "ProjOrientation",
};

bool Fail(std::string* error, int line, const std::string& why) {
  if (error) *error = "line " + std::to_string(line) + ": " + why;
  return false;
}

}  // namespace

bool IsLookSpellColumn(const std::string& column) {
  for (const char* name : kSpellColumns) {
    if (_stricmp(name, column.c_str()) == 0) return true;
  }
  return false;
}

bool CheckLookRows(const std::string& table, const TwoDA& base, const std::string& rows_text, std::string* error) {
  const bool spells = _stricmp(table.c_str(), "spells") == 0;
  int first = -1;
  int last = -1;
  if (_stricmp(table.c_str(), "visualeffects") == 0) {
    first = kLookVisualEffectsFirst;
    last = kLookVisualEffectsLast;
  } else if (_stricmp(table.c_str(), "progfx") == 0) {
    first = kLookProgFxFirst;
    last = kLookProgFxLast;
  } else if (!spells) {
    if (error) *error = "a look changes no " + table + ".2da";
    return false;
  }
  std::istringstream in(rows_text);
  std::string line;
  int line_number = 0;
  bool header = false;
  while (std::getline(in, line)) {
    ++line_number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line[start] == '#') continue;
    const std::vector<std::string> tokens = Tokenize(line);
    if (tokens.empty()) continue;
    if (!header) {
      if (spells) {
        for (const std::string& column : tokens) {
          if (!IsLookSpellColumn(column)) return Fail(error, line_number, "spells.2da's " + column + " isn't a look's");
        }
      }
      header = true;
      continue;
    }
    if (tokens[0] == "+") return Fail(error, line_number, "a look adds no rows");
    char* end = nullptr;
    const long row = strtol(tokens[0].c_str(), &end, 10);
    if (!end || *end != '\0' || row < 0) return Fail(error, line_number, "no row number");
    if (spells) {
      if (static_cast<size_t>(row) >= base.rows.size()) {
        return Fail(error, line_number, "spells.2da has no row " + std::to_string(row));
      }
      continue;
    }
    if (row < first || row > last) {
      return Fail(error, line_number, "row " + std::to_string(row) + " is outside " + std::to_string(first) + "-" +
                                          std::to_string(last));
    }
    if (static_cast<size_t>(row) < base.rows.size()) {
      for (const std::string& value : base.rows[row]) {
        if (value != kEmpty) return Fail(error, line_number, "row " + std::to_string(row) + " is in use");
      }
    }
  }
  return true;
}

}  // namespace data
