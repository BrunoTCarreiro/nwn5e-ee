#include "data/chat_history.h"

namespace data {

void ChatHistory::Add(const std::string& line) {
  ResetWalk();
  std::string clean = line;
  for (char& c : clean) {
    if (c == '\r' || c == '\n') c = ' ';
  }
  const size_t end = clean.find_last_not_of(" \t");
  if (end == std::string::npos) return;  // blank
  clean.erase(end + 1);
  if (!lines_.empty() && lines_.back() == clean) return;
  lines_.push_back(clean);
  if (lines_.size() > kMaxLines) lines_.erase(lines_.begin(), lines_.begin() + (lines_.size() - kMaxLines));
}

bool ChatHistory::Older(const std::string& current, std::string* out) {
  if (lines_.empty()) return false;
  if (position_ < 0) {
    draft_ = current;
    position_ = static_cast<int>(lines_.size());
  }
  if (position_ == 0) return false;  // the oldest already
  --position_;
  *out = lines_[static_cast<size_t>(position_)];
  return true;
}

bool ChatHistory::Newer(std::string* out) {
  if (position_ < 0) return false;
  ++position_;
  if (position_ >= static_cast<int>(lines_.size())) {
    *out = draft_;
    ResetWalk();
    return true;
  }
  *out = lines_[static_cast<size_t>(position_)];
  return true;
}

void ChatHistory::ResetWalk() {
  position_ = -1;
  draft_.clear();
}

std::string ChatHistory::Save() const {
  std::string text;
  for (const std::string& line : lines_) text += line + "\n";
  return text;
}

void ChatHistory::Load(const std::string& text) {
  lines_.clear();
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    Add(text.substr(start, end - start));  // a \r left at the end becomes a space and is trimmed
    start = end + 1;
  }
  ResetWalk();
}

}  // namespace data
