#include "check.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

// #168: chat messages still written in the code (a TellPlayer call, or a file's Tell helper's, with a string literal
// and no PlayerText; a SendMessageToPC in a script; a literal starting "nwn5e: " - a command's line, a helper's
// message) - each file's count only goes down (tests/data/fixtures/text_literals.txt; lower it when a file's
// messages move to data/text/en.txt). New text goes through the catalogue.

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream bytes;
  bytes << file.rdbuf();
  return bytes.str();
}

// Calls of `name` (a whole word) whose arguments hold a string literal and no PlayerText(...); a definition skipped.
int LiteralCalls(const std::string& text, const std::string& name) {
  int count = 0;
  const std::string open = name + "(";
  for (size_t at = text.find(open); at != std::string::npos; at = text.find(open, at + 1)) {
    if (at > 0 && (std::isalnum(static_cast<unsigned char>(text[at - 1])) || text[at - 1] == '_')) continue;
    if (at >= 5 && text.compare(at - 5, 5, "void ") == 0) continue;
    size_t i = at + open.size();
    int depth = 1;
    bool in_string = false;
    std::string args;
    for (; i < text.size() && depth > 0; ++i) {
      const char c = text[i];
      if (in_string) {
        if (c == '\\') {
          args += c;
          ++i;
          if (i < text.size()) args += text[i];
          continue;
        }
        if (c == '"') in_string = false;
      } else if (c == '"') {
        in_string = true;
      } else if (c == '(') {
        ++depth;
      } else if (c == ')') {
        --depth;
      }
      args += c;
    }
    if (args.find('"') != std::string::npos && args.find("PlayerText(") == std::string::npos) ++count;
  }
  return count;
}

// String literals in code (not in a comment, nor escaped inside another string) whose text starts "nwn5e: ".
int ChatLineLiterals(const std::string& text) {
  int count = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      i = text.find('\n', i);
      if (i == std::string::npos) break;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      i = text.find("*/", i + 2);
      if (i == std::string::npos) break;
      ++i;
    } else if (c == '\'' || c == '"') {
      if (c == '"' && text.compare(i + 1, 7, "nwn5e: ") == 0) ++count;
      for (++i; i < text.size() && text[i] != c; ++i) {
        if (text[i] == '\\') ++i;
      }
    }
  }
  return count;
}

int Count(const std::string& text, const std::string& what) {
  int count = 0;
  for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++count;
  return count;
}

}  // namespace

TEST(ChatLineLiteralsCounted) {
  // two lines in code; one in a comment, one escaped in a script, one after a quote character - not counted
  const std::string text =
      "return {\"nwn5e: a\"}; // \"nwn5e: b\"\n"
      "/* \"nwn5e: c\" */ Run(\"SendMessageToPC(m, \\\"nwn5e: d\\\")\");\n"
      "char q = '\"'; push_back(\"nwn5e: e\" + x);";
  CHECK_EQ(ChatLineLiterals(text), 2);
  CHECK_EQ(ChatLineLiterals("\"nwn5e:x\" \"nwn5e : y\""), 0);
}

TEST(NoNewLiteralPlayerText) {
  struct Limits {
    int tells = 0, sends = 0, lines = 0;
  };
  std::map<std::string, Limits> baseline;
  std::istringstream file(ReadFile("tests/data/fixtures/text_literals.txt"));
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string name;
    int tells = 0, helpers = 0, sends = 0, lines = 0;
    fields >> name >> tells >> helpers >> sends >> lines;
    baseline[name] = {tells + helpers, sends, lines};
  }
  CHECK_EQ(baseline.empty(), false);
  int over = 0, files = 0;
  for (const auto& entry : std::filesystem::directory_iterator("src/core")) {
    if (entry.path().extension() != ".cpp") continue;
    ++files;
    const std::string name = entry.path().filename().string();
    const std::string text = ReadFile(entry.path().string());
    const int tells = LiteralCalls(text, "TellPlayer") + LiteralCalls(text, "Tell");
    const int sends = Count(text, "SendMessageToPC(");
    const int lines = ChatLineLiterals(text);
    const auto known = baseline.find(name);
    const Limits limit = known == baseline.end() ? Limits{} : known->second;
    if (tells > limit.tells || sends > limit.sends || lines > limit.lines) {
      ++over;
      printf("  %s: %d TellPlayer / Tell literals (at most %d), %d SendMessageToPC (at most %d), %d \"nwn5e: \" "
             "lines (at most %d) - new player text goes in data/text/en.txt\n",
             name.c_str(), tells, limit.tells, sends, limit.sends, lines, limit.lines);
    }
  }
  CHECK_EQ(over, 0);
  CHECK_EQ(files > 50, true);
}
