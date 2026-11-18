#include "check.h"
#include "data/text_catalog.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

// #168 (owner, 2026-11-16: "any new text is either written by me, or copied verbatim from the SRD. If you have to
// generate new text we'll need a document to keep track of it, maybe in an i18n style").

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream bytes;
  bytes << file.rdbuf();
  return bytes.str();
}

}  // namespace

TEST(TextCatalogParses) {
  data::TextCatalog catalog;
  std::string error;
  CHECK_EQ(data::ParseTextCatalog("# a comment\n\na.b | generated | Hello {name}.\\nAgain\r\n"
                                  "c_d | srd:Floating Disk | Floating Disk\n",
                                  &catalog, &error),
           true);
  CHECK_EQ(catalog.size(), 2u);
  CHECK_EQ(catalog["a.b"].text == "Hello {name}.\nAgain", true);
  CHECK_EQ(catalog["c_d"].source == "srd:Floating Disk", true);
  CHECK_EQ(data::ParseTextCatalog("A.b | generated | x\n", nullptr, &error), false);  // a capital in the key
  CHECK_EQ(data::ParseTextCatalog("a..b | generated | x\n", nullptr, &error), false);
  CHECK_EQ(data::ParseTextCatalog("a.b | mine | x\n", nullptr, &error), false);  // not a source
  CHECK_EQ(data::ParseTextCatalog("a.b | generated\n", nullptr, &error), false);  // no text part
  data::TextCatalog twice;
  CHECK_EQ(data::ParseTextCatalog("a | owner | x\na | owner | y\n", &twice, &error), false);
  CHECK_EQ(data::ValidTextSource("srd:Find Familiar+note") && data::ValidTextSource("symbol") &&
               data::ValidTextSource("owner") && !data::ValidTextSource("srd:") && !data::ValidTextSource("srd:+note") &&
               !data::ValidTextSource("srdmine") && !data::ValidTextSource("srd Floating Disk"),
           true);
  CHECK_EQ(data::FormatText("{a} and {b}, {a}; {c}", {{"a", "1"}, {"b", "{a}"}}) == "1 and {a}, 1; {c}", true);
}

// data/text/en.txt parses (a key once, a source each).
TEST(TextCatalogFile) {
  data::TextCatalog catalog;
  std::string error;
  const bool ok = data::ParseTextCatalog(ReadFile("data/text/en.txt"), &catalog, &error);
  if (!ok) printf("  data/text/en.txt: %s\n", error.c_str());
  CHECK_EQ(ok, true);
  CHECK_EQ(catalog.size() >= 30u, true);
}

// Every catalogue key the code names (PlayerText("..."), or a key literal of a catalogue part: "disk.why.hour") is in
// data/text/en.txt.
TEST(TextKeysInCodeExist) {
  data::TextCatalog catalog;
  std::string error;
  CHECK_EQ(data::ParseTextCatalog(ReadFile("data/text/en.txt"), &catalog, &error), true);
  std::set<std::string> parts;
  for (const auto& entry : catalog) parts.insert(entry.first.substr(0, entry.first.find('.')));
  const std::regex literal("\"([a-z][a-z0-9_]*\\.[a-z0-9_.]+)\"");
  int named = 0, missing = 0;
  for (const char* folder : {"src/core", "src/rules"}) {
    for (const auto& file : std::filesystem::directory_iterator(folder)) {
      if (file.path().extension() != ".cpp") continue;
      const std::string text = ReadFile(file.path().string());
      for (auto it = std::sregex_iterator(text.begin(), text.end(), literal); it != std::sregex_iterator(); ++it) {
        const std::string key = (*it)[1];
        if (!parts.count(key.substr(0, key.find('.')))) continue;
        static const std::set<std::string> kFiles = {"h", "cpp", "txt", "2da", "mdl", "utp", "tga", "nss", "ini", "log"};
        if (kFiles.count(key.substr(key.rfind('.') + 1))) continue;  // a file's name (#include "chat.h")
        ++named;
        if (!catalog.count(key)) {
          ++missing;
          printf("  %s names %s - not in data/text/en.txt\n", file.path().string().c_str(), key.c_str());
        }
      }
    }
  }
  CHECK_EQ(missing, 0);
  CHECK_EQ(named >= 30, true);
}

// data/text/strings_sources.txt gives every data/strings.txt entry one source, and names no other.
TEST(StringsHaveSources) {
  std::set<std::string> ids;
  std::istringstream strings(ReadFile("data/strings.txt"));
  std::string line;
  while (std::getline(strings, line)) {
    if (line.size() > 9 && line[8] == ' ' && std::isdigit(static_cast<unsigned char>(line[0]))) ids.insert(line.substr(0, 8));
  }
  std::map<std::string, std::string> sources;
  std::istringstream file(ReadFile("data/text/strings_sources.txt"));
  int bad = 0, twice = 0;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    const std::string id = line.substr(0, 8);
    const std::string source = line.size() > 9 ? line.substr(9) : "";
    if (line.size() < 10 || line[8] != ' ' || !data::ValidTextSource(source)) {
      ++bad;
      printf("  data/text/strings_sources.txt: bad line %.60s\n", line.c_str());
      continue;
    }
    if (!sources.emplace(id, source).second) ++twice;
  }
  int without = 0, extra = 0;
  for (const std::string& id : ids) {
    if (!sources.count(id)) {
      ++without;
      printf("  data/strings.txt %s has no source in data/text/strings_sources.txt\n", id.c_str());
    }
  }
  for (const auto& source : sources) extra += ids.count(source.first) ? 0 : 1;
  CHECK_EQ(bad, 0);
  CHECK_EQ(twice, 0);
  CHECK_EQ(without, 0);
  CHECK_EQ(extra, 0);
  CHECK_EQ(ids.size() >= 600u, true);
}
