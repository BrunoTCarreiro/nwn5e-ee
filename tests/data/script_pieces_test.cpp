#include <string>
#include <vector>

#include "check.h"
#include "data/script_pieces.h"

using namespace data;

namespace {

// The literals of generated code, unescaped, in order.
std::vector<std::string> Literals(const std::string& code) {
  std::vector<std::string> out;
  for (size_t i = 0; i < code.size(); ++i) {
    if (code[i] != '"') continue;
    std::string text;
    for (++i; i < code.size() && code[i] != '"'; ++i) {
      if (code[i] == '\\' && i + 1 < code.size()) ++i;
      text += code[i];
    }
    out.push_back(text);
  }
  return out;
}

}  // namespace

TEST(ScriptStringBuildShortIsOneLiteral) {
  CHECK_EQ(ScriptStringBuild("s", "abc") == "string s = \"abc\";", true);
  CHECK_EQ(ScriptStringBuild("s", "") == "string s = \"\";", true);
  CHECK_EQ(ScriptStringBuild("s", "a\"b\\c") == "string s = \"a\\\"b\\\\c\";", true);  // escaped
}

TEST(ScriptStringBuildLongInPieces) {
  // The !vfx list's size: 35,000 characters, quotes among them.
  std::string text;
  for (int i = 0; i < 5000; ++i) text += "\"" + std::to_string(i) + "\",";
  const std::string code = ScriptStringBuild("v", text, 4000);
  const std::vector<std::string> parts = Literals(code);
  CHECK_EQ(parts.size(), (text.size() + 3999) / 4000);
  std::string joined;
  for (const std::string& p : parts) {
    CHECK_EQ(p.size() <= 4000u, true);
    joined += p;
  }
  CHECK_EQ(joined == text, true);  // nothing lost or doubled at a cut
  CHECK_EQ(code.compare(0, 13, "string v = \"\\") == 0, true);
  CHECK_EQ(code.find("; v += \"") != std::string::npos, true);
  // Exactly one piece's worth: one literal.
  CHECK_EQ(Literals(ScriptStringBuild("v", std::string(4000, 'x'), 4000)).size(), 1u);
  CHECK_EQ(Literals(ScriptStringBuild("v", std::string(4001, 'x'), 4000)).size(), 2u);
}
