#include "data/twoda.h"

#include <stdlib.h>
#include <string.h>

#include <sstream>

namespace data {

namespace {

std::vector<std::string> Lines(const std::string& text) {
  std::vector<std::string> lines;
  std::string line;
  std::istringstream in(text);
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

bool Blank(const std::string& line) { return line.find_first_not_of(" \t") == std::string::npos; }

std::string Quoted(const std::string& value) {
  if (value.empty()) return kEmpty;
  if (value.find_first_of(" \t") == std::string::npos) return value;
  return "\"" + value + "\"";
}

}  // namespace

std::vector<std::string> Tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) break;
    if (line[i] == '"') {
      const size_t end = line.find('"', i + 1);
      if (end == std::string::npos) {
        tokens.push_back(line.substr(i + 1));
        break;
      }
      tokens.push_back(line.substr(i + 1, end - i - 1));
      i = end + 1;
    } else {
      const size_t end = line.find_first_of(" \t", i);
      tokens.push_back(line.substr(i, end == std::string::npos ? std::string::npos : end - i));
      i = end == std::string::npos ? line.size() : end;
    }
  }
  return tokens;
}

bool ParseTwoDA(const std::string& text, TwoDA* table, std::string* error) {
  const std::vector<std::string> lines = Lines(text);
  if (lines.empty() || lines[0].compare(0, 8, "2DA V2.0") != 0) {
    if (error) *error = "not a 2DA V2.0 file";
    return false;
  }
  *table = TwoDA();
  size_t i = 1;
  for (; i < lines.size() && Blank(lines[i]); ++i) {
  }
  if (i < lines.size() && lines[i].compare(0, 8, "DEFAULT:") == 0) {
    const std::vector<std::string> tokens = Tokenize(lines[i].substr(8));
    if (!tokens.empty()) table->default_value = tokens[0];
    for (++i; i < lines.size() && Blank(lines[i]); ++i) {
    }
  }
  if (i >= lines.size()) {
    if (error) *error = "no column header";
    return false;
  }
  table->columns = Tokenize(lines[i]);
  for (++i; i < lines.size(); ++i) {
    if (Blank(lines[i])) continue;
    std::vector<std::string> tokens = Tokenize(lines[i]);
    std::vector<std::string> row(table->columns.size(), kEmpty);
    for (size_t c = 0; c < row.size() && c + 1 < tokens.size(); ++c) row[c] = tokens[c + 1];
    table->rows.push_back(row);
  }
  return true;
}

std::string WriteTwoDA(const TwoDA& table) {
  std::string out = "2DA V2.0\n";
  if (!table.default_value.empty()) out += "DEFAULT: " + Quoted(table.default_value) + "\n";
  out += "\n";
  for (const std::string& column : table.columns) out += " " + column;
  out += "\n";
  for (size_t r = 0; r < table.rows.size(); ++r) {
    out += std::to_string(r);
    for (const std::string& value : table.rows[r]) out += " " + Quoted(value);
    out += "\n";
  }
  return out;
}

bool ApplyRows(TwoDA* table, const std::string& rows_text, std::string* error, int* rows_changed) {
  if (rows_changed) *rows_changed = 0;
  std::vector<int> targets;  // table column per named column
  bool have_header = false;
  int line_number = 0;
  for (const std::string& line : Lines(rows_text)) {
    ++line_number;
    if (Blank(line) || line.find_first_not_of(" \t") == line.find('#')) continue;
    const std::vector<std::string> tokens = Tokenize(line);
    if (!have_header) {
      for (const std::string& name : tokens) {
        int found = -1;
        for (size_t c = 0; c < table->columns.size(); ++c) {
          if (_stricmp(table->columns[c].c_str(), name.c_str()) == 0) found = static_cast<int>(c);
        }
        if (found < 0) {
          if (error) *error = "line " + std::to_string(line_number) + ": no column " + name;
          return false;
        }
        targets.push_back(found);
      }
      have_header = true;
      continue;
    }
    long row = static_cast<long>(table->rows.size());  // "+": append
    bool number = true;
    if (tokens[0] != "+") {
      char* end = nullptr;
      row = strtol(tokens[0].c_str(), &end, 10);
      number = *end == '\0';
    }
    if (!number || row < 0 || row > 65535 || tokens.size() != targets.size() + 1) {
      if (error) *error = "line " + std::to_string(line_number) + ": expected a row number and " +
                          std::to_string(targets.size()) + " values";
      return false;
    }
    while (table->rows.size() <= static_cast<size_t>(row)) {
      table->rows.push_back(std::vector<std::string>(table->columns.size(), kEmpty));
    }
    for (size_t t = 0; t < targets.size(); ++t) table->rows[row][targets[t]] = tokens[t + 1];
    if (rows_changed) ++*rows_changed;
  }
  return true;
}

}  // namespace data
