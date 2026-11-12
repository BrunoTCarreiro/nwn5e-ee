#include <string>

#include "check.h"
#include "data/chat_history.h"

using data::ChatHistory;

// Breaks caught (#158): a blank line or a repeat of the newest added, trailing spaces kept, the walk starting at the
// oldest, the draft not kept or not given back past the newest, Up past the oldest moving or changing `out`, Newer
// working when not walking, sending not ending the walk.
TEST(ChatHistoryWalk) {
  ChatHistory history;
  std::string out = "x";
  CHECK_EQ(history.Older("typing", &out), false);  // nothing sent yet
  CHECK_EQ(out == "x", true);
  CHECK_EQ(history.Walking(), false);
  history.Add("!test level1cleric");
  history.Add("!spawn nw_orcb");
  history.Add("!spawn nw_orcb");  // a repeat of the newest: once
  history.Add("   ");             // blank: not added
  history.Add("hello  ");         // its trailing spaces go
  CHECK_EQ(static_cast<int>(history.Lines().size()), 3);
  CHECK_EQ(history.Lines().back() == "hello", true);
  CHECK_EQ(history.Older("!gol", &out), true);  // the newest first
  CHECK_EQ(out == "hello", true);
  CHECK_EQ(history.Walking(), true);
  CHECK_EQ(history.Older("hello", &out), true);
  CHECK_EQ(out == "!spawn nw_orcb", true);
  CHECK_EQ(history.Older(out, &out), true);
  CHECK_EQ(out == "!test level1cleric", true);
  out = "y";
  CHECK_EQ(history.Older("!test level1cleric", &out), false);  // the oldest: it stays
  CHECK_EQ(out == "y", true);
  CHECK_EQ(history.Newer(&out), true);
  CHECK_EQ(out == "!spawn nw_orcb", true);
  CHECK_EQ(history.Newer(&out), true);
  CHECK_EQ(out == "hello", true);
  CHECK_EQ(history.Newer(&out), true);
  CHECK_EQ(out == "!gol", true);  // past the newest: the draft back
  CHECK_EQ(history.Walking(), false);
  out = "z";
  CHECK_EQ(history.Newer(&out), false);  // not walking
  CHECK_EQ(out == "z", true);
  CHECK_EQ(history.Older("", &out), true);  // a new walk starts at the newest again
  CHECK_EQ(out == "hello", true);
  history.ResetWalk();
  CHECK_EQ(history.Walking(), false);
  CHECK_EQ(history.Older("abc", &out), true);
  history.Add("!time");  // sending ends the walk
  CHECK_EQ(history.Walking(), false);
  CHECK_EQ(history.Older("", &out), true);
  CHECK_EQ(out == "!time", true);
}

// Breaks caught (#158): no cap or the newest dropped instead of the oldest, the file losing or reordering lines,
// \r\n left in, blank lines kept, line breaks inside a line kept.
TEST(ChatHistoryCapAndFile) {
  ChatHistory history;
  for (int i = 0; i < 60; ++i) history.Add("!line " + std::to_string(i));
  CHECK_EQ(static_cast<int>(history.Lines().size()), 50);
  CHECK_EQ(history.Lines().front() == "!line 10", true);
  CHECK_EQ(history.Lines().back() == "!line 59", true);
  ChatHistory back;
  back.Load(history.Save());
  CHECK_EQ(back.Lines() == history.Lines(), true);
  CHECK_EQ(back.Walking(), false);
  ChatHistory windows;
  windows.Load("!a\r\n\r\n!b\r\n!b\n  \n!c");
  CHECK_EQ(static_cast<int>(windows.Lines().size()), 3);
  CHECK_EQ(windows.Lines()[0] == "!a", true);
  CHECK_EQ(windows.Lines()[1] == "!b", true);
  CHECK_EQ(windows.Lines()[2] == "!c", true);
  ChatHistory big;
  std::string text;
  for (int i = 0; i < 70; ++i) text += "!x " + std::to_string(i) + "\n";
  big.Load(text);
  CHECK_EQ(static_cast<int>(big.Lines().size()), 50);
  CHECK_EQ(big.Lines().front() == "!x 20", true);
  ChatHistory broken;
  broken.Add("!a\nb");
  CHECK_EQ(broken.Lines().back() == "!a b", true);
}
