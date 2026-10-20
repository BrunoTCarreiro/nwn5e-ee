#pragma once

// Phase 3 CM0 (PLAN.md): the game's 2da tables as text, and nwn5e's rows
// merged into them. No engine code; unit-tested.

#include <string>
#include <vector>

namespace data {

constexpr const char* kEmpty = "****";

struct TwoDA {
  std::vector<std::string> columns;            // the header, without the row-index column
  std::vector<std::vector<std::string>> rows;  // one value per column ("****" = empty)
  std::string default_value;                   // the optional "DEFAULT:" line
};

// Parses 2DA V2.0 text. Values may be "quoted" (spaces inside); missing
// trailing values are "****". Row order is file order (the index column is
// not trusted).
bool ParseTwoDA(const std::string& text, TwoDA* table, std::string* error);

// 2DA V2.0 text (values with spaces quoted, empty values "****").
std::string WriteTwoDA(const TwoDA& table);

// Applies a rows file: '#' comments; the first other line names columns of
// the table; every following line is "<row> <value per named column>". A row
// inside the table replaces those columns; a row past its end creates it
// (empty rows pad the gap; unnamed columns stay "****"); row "+" appends.
// `rows_changed` = rows touched.
bool ApplyRows(TwoDA* table, const std::string& rows_text, std::string* error, int* rows_changed);

// Splits a line into tokens: whitespace-separated, "double quotes" keep spaces.
std::vector<std::string> Tokenize(const std::string& line);

}  // namespace data
