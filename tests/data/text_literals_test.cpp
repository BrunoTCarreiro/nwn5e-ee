#include "check.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

// #168: chat messages still written in the code (a TellPlayer call, or a file's Tell helper's, with a string literal
// and no PlayerText; a SendMessageToPC in a script) - each file's count only goes down (tests/data/fixtures/text_literals.txt; lower it
// when a file's messages move to data/text/en.txt). New text goes through the catalogue.

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

int Count(const std::string& text, const std::string& what) {
  int count = 0;
  for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++count;
  return count;
}

}  // namespace

TEST(NoNewLiteralPlayerText) {
  std::map<std::string, std::pair<int, int>> baseline;
  std::istringstream file(ReadFile("tests/data/fixtures/text_literals.txt"));
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string name;
    int tells = 0, helpers = 0, sends = 0;
    fields >> name >> tells >> helpers >> sends;
    baseline[name] = {tells + helpers, sends};
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
    const auto known = baseline.find(name);
    const int max_tells = known == baseline.end() ? 0 : known->second.first;
    const int max_sends = known == baseline.end() ? 0 : known->second.second;
    if (tells > max_tells || sends > max_sends) {
      ++over;
      printf("  %s: %d TellPlayer / Tell literals (at most %d), %d SendMessageToPC (at most %d) - new player text goes in "
             "data/text/en.txt\n",
             name.c_str(), tells, max_tells, sends, max_sends);
    }
  }
  CHECK_EQ(over, 0);
  CHECK_EQ(files > 50, true);
}
