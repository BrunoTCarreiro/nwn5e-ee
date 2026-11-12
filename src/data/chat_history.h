#pragma once

#include <stddef.h>

#include <string>
#include <vector>

// #158 (owner, 2026-11-12: "can we add a history to the chat input? ... when the chat input is active, pressing up
// would cycle through the previous commands"): the chat bar's history - the lines sent, oldest first - walked as a
// shell's: Up the next older line, Down the next newer one and, past the newest, the text that was being typed before
// the first Up. The client's part (the keys, the box, the file) is src/core/chat_input.cpp.
namespace data {

class ChatHistory {
 public:
  static constexpr size_t kMaxLines = 50;

  // A line sent. Its line breaks become spaces and its trailing spaces go; a blank one, or one the same as the newest,
  // isn't added; past kMaxLines the oldest goes. Any walk is over.
  void Add(const std::string& line);
  // Up, `current` the box's text: the next older line into `out` (the first Up keeps `current` as the draft). False,
  // nothing changed, when there's no older line.
  bool Older(const std::string& current, std::string* out);
  // Down: the next newer line into `out`, or past the newest the draft (the walk over). False when not walking.
  bool Newer(std::string* out);
  // The walk forgotten (the box closed with Escape).
  void ResetWalk();
  bool Walking() const { return position_ >= 0; }

  // The file's text: a line each, oldest first.
  std::string Save() const;
  // From a file's text (\r\n or \n line ends), each line as Add takes it: the newest kMaxLines kept. Replaces the
  // lines; no walk.
  void Load(const std::string& text);
  const std::vector<std::string>& Lines() const { return lines_; }

 private:
  std::vector<std::string> lines_;
  int position_ = -1;  // the line shown while walking, else -1
  std::string draft_;
};

}  // namespace data
