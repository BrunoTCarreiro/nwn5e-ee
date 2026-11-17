#include "data/text_catalog.h"

namespace data {

namespace {

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t");
  if (first == std::string::npos) return "";
  const size_t last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

}  // namespace

bool ValidTextKey(const std::string& key) {
  if (key.empty() || key.front() == '.' || key.back() == '.' || key.find("..") != std::string::npos) return false;
  for (char c : key) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
  }
  return true;
}

bool ValidTextSource(const std::string& source) {
  if (source == "owner" || source == "generated" || source == "symbol") return true;
  if (source.rfind("srd:", 0) != 0) return false;
  std::string entry = source.substr(4);
  const std::string note = "+note";
  if (entry.size() >= note.size() && entry.compare(entry.size() - note.size(), note.size(), note) == 0) {
    entry.erase(entry.size() - note.size());
  }
  return !Trim(entry).empty();
}

bool ParseTextCatalog(const std::string& file, TextCatalog* out, std::string* error) {
  int number = 0;
  size_t start = 0;
  while (start < file.size()) {
    size_t end = file.find('\n', start);
    if (end == std::string::npos) end = file.size();
    std::string line = file.substr(start, end - start);
    start = end + 1;
    ++number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;
    const size_t bar1 = line.find(" | ");
    const size_t bar2 = bar1 == std::string::npos ? std::string::npos : line.find(" | ", bar1 + 3);
    auto fail = [&](const char* why) {
      if (error) *error = "line " + std::to_string(number) + ": " + why;
      return false;
    };
    if (bar2 == std::string::npos) return fail("not `<key> | <source> | <text>`");
    const std::string key = Trim(line.substr(0, bar1));
    const std::string source = Trim(line.substr(bar1 + 3, bar2 - bar1 - 3));
    std::string text = line.substr(bar2 + 3);
    if (!ValidTextKey(key)) return fail("a key is a-z, 0-9 and '_' in dotted parts");
    if (!ValidTextSource(source)) return fail("a source is owner, generated or srd:<entry>");
    if (text.empty()) return fail("no text");
    for (size_t at = text.find("\\n"); at != std::string::npos; at = text.find("\\n", at + 1)) text.replace(at, 2, "\n");
    if (out && !out->emplace(key, TextEntry{source, text}).second) return fail("a key given twice");
  }
  return true;
}

std::string FormatText(const std::string& text, const std::vector<std::pair<std::string, std::string>>& values) {
  std::string result = text;
  for (const auto& value : values) {
    const std::string mark = "{" + value.first + "}";
    for (size_t at = result.find(mark); at != std::string::npos; at = result.find(mark, at + value.second.size())) {
      result.replace(at, mark.size(), value.second);
    }
  }
  return result;
}

}  // namespace data
