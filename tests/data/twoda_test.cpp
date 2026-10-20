#include "check.h"
#include "data/twoda.h"

using namespace data;

namespace {
const char* kFeat =
    "2DA V2.0\r\n"
    "\r\n"
    "    LABEL          FEAT   DESCRIPTION  ICON\r\n"
    "0   Alertness      289    424          ife_alertness\r\n"
    "1   \"Two Words\"    290    425\r\n";
}

// Breaks caught: quotes splitting a value, a short row losing its "****",
// CRLF kept in the last value, the index column read as a value.
TEST(TwoDAParsesGameTables) {
  TwoDA t;
  std::string error;
  CHECK_EQ(ParseTwoDA(kFeat, &t, &error), true);
  CHECK_EQ(t.columns.size(), 4u);
  CHECK_EQ(t.rows.size(), 2u);
  CHECK_EQ(t.rows[0][3] == "ife_alertness", true);
  CHECK_EQ(t.rows[1][0] == "Two Words", true);
  CHECK_EQ(t.rows[1][3] == "****", true);
  TwoDA again;
  CHECK_EQ(ParseTwoDA(WriteTwoDA(t), &again, &error), true);
  CHECK_EQ(again.rows[1][0] == "Two Words", true);
  CHECK_EQ(again.rows.size(), 2u);
  CHECK_EQ(ParseTwoDA("not a table", &t, &error), false);
}

// Breaks caught: a new row past the end not padding the gap, a replaced row
// losing its other columns, an unknown column accepted, comments read.
TEST(TwoDARowsMergeIntoTheTable) {
  TwoDA t;
  std::string error;
  ParseTwoDA(kFeat, &t, &error);
  int changed = 0;
  const char* rows =
      "# nwn5e rows\n"
      "LABEL FEAT\n"
      "1 Renamed 999\n"
      "4 NWN5E_NEW 16000000\n";
  CHECK_EQ(ApplyRows(&t, rows, &error, &changed), true);
  CHECK_EQ(changed, 2);
  CHECK_EQ(t.rows.size(), 5u);
  CHECK_EQ(t.rows[1][0] == "Renamed", true);
  CHECK_EQ(t.rows[1][2] == "425", true);  // DESCRIPTION untouched
  CHECK_EQ(t.rows[2][0] == "****", true);  // padding
  CHECK_EQ(t.rows[4][1] == "16000000", true);
  CHECK_EQ(t.rows[4][3] == "****", true);
  CHECK_EQ(ApplyRows(&t, "LABEL\n+ Appended\n+ Second\n", &error, &changed), true);
  CHECK_EQ(t.rows.size(), 7u);
  CHECK_EQ(t.rows[5][0] == "Appended", true);
  CHECK_EQ(t.rows[6][0] == "Second", true);
  CHECK_EQ(ApplyRows(&t, "NOPE\n1 x\n", &error, &changed), false);
  CHECK_EQ(ApplyRows(&t, "LABEL FEAT\n1 only-one\n", &error, &changed), false);
}
