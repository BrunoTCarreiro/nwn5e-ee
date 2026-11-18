#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace data {

// #168 (owner, 2026-11-16: "I just want to make sure that any new text is either written by me, or copied verbatim
// from the SRD. If you have to generate new text we'll need a document to keep track of it, maybe in an i18n style";
// "go with your recommendations"): nwn5e's player text catalogue, data/text/en.txt. One entry a line,
// `<key> | <source> | <text>` ('#' starts a comment line):
// - the key: a-z, 0-9 and '_' in dotted parts (disk.end.pile);
// - its source: owner (the owner's words), srd:<SRD 5.2.1 entry> (its text word for word), generated (nwn5e's
//   wording, for the owner's review - docs/TEXT-REVIEW.md);
// - the text: `\n` a line break, `{name}` a value the code fills in.
// data/text/strings_sources.txt gives data/strings.txt's entries their sources the same way (srd:<entry>+note - SRD
// 5.2.1's text, then a generated "nwn5e:" note; symbol - no words, "+2").
struct TextEntry {
  std::string source;
  std::string text;
};
using TextCatalog = std::map<std::string, TextEntry>;

// The catalogue's entries; false (and *error: the line and why) on a malformed line, a bad key or source, a key twice.
bool ParseTextCatalog(const std::string& file, TextCatalog* out, std::string* error);
bool ValidTextKey(const std::string& key);
// owner, generated, srd:<entry>, srd:<entry>+note, symbol.
bool ValidTextSource(const std::string& source);
// The text with each {name} replaced by its value (a name not given stays as it is).
std::string FormatText(const std::string& text, const std::vector<std::pair<std::string, std::string>>& values);

}  // namespace data
