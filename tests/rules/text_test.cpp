#include <string>

#include "check.h"
#include "rules/text.h"

using namespace rules;

// Breaks caught: UTF-8 sent as is (the owner saw "â€"" for an em dash in
// chat, 2026-10-19); Latin-1 letters lost; unknown characters dropped instead
// of marked.
TEST(GameTextIsWindows1252) {
  CHECK_EQ(ToWindows1252("plain ASCII: +2") == "plain ASCII: +2", true);
  CHECK_EQ(ToWindows1252("a \xE2\x80\x94 b") == std::string("a \x97 b"), true);       // em dash
  CHECK_EQ(ToWindows1252("Thieves\xE2\x80\x99") == std::string("Thieves\x92"), true);  // right quote
  CHECK_EQ(ToWindows1252("caf\xC3\xA9") == std::string("caf\xE9"), true);             // Latin-1 e acute
  CHECK_EQ(ToWindows1252("\xE2\x88\x92" "1") == "-1", true);                          // minus sign
  CHECK_EQ(ToWindows1252("\xE4\xB8\xAD") == "?", true);                               // not in Windows-1252
  CHECK_EQ(ToWindows1252("bad \x80 byte") == "bad ? byte", true);
}
