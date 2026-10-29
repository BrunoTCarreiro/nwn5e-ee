#include "rules/queue_strip.h"

#include <algorithm>

namespace rules {

namespace {

struct QueueAction {
  int action;
  const char* name;
  const char* icon;
};

// NWScript's ACTION_* values with a name; the engine's GetAQActionIDByID also
// gives 38, which NWScript doesn't name ("Action").
const QueueAction kActions[] = {
    {0, "Move", "ir_move"},
    {1, "Pick up", "ir_pickup"},
    {2, "Drop", "ir_drop"},
    {3, "Attack", "ir_attack"},
    {4, "Cast", "ir_cast"},
    {5, "Open", "ir_open"},
    {6, "Close", "ir_close"},
    {7, "Talk", "ir_dialog"},
    {8, "Disarm trap", "ir_disarmtrap"},
    {9, "Recover trap", "ir_rec_trap"},
    {10, "Flag trap", "ir_flag_trap"},
    {11, "Examine trap", "ir_examine"},
    {12, "Set trap", "ir_trap"},
    {13, "Pick lock", "ir_picklock"},
    {14, "Lock", "ir_lock"},
    {15, "Use", "ir_use"},
    {16, "Animal Empathy", "ir_animalemp"},
    {17, "Rest", "ir_rest"},
    {18, "Taunt", "ir_taunt"},
    {19, "Use item", "ir_use"},
    {31, "Counterspell", "ir_cntrspell"},
    {33, "Heal", "ir_heal"},
    {34, "Pick pocket", "ir_pickpocket"},
    {35, "Follow", "ir_follow"},
    {36, "Wait", "ir_action"},
    {37, "Sit", "ir_sit"},
    {40, "Smite", "ir_smiteevil"},
    {41, "Ki Damage", "ir_action"},
    {43, "Wander", "ir_move"},
};

const QueueAction* Find(int action) {
  for (const QueueAction& a : kActions) {
    if (a.action == action) return &a;
  }
  return nullptr;
}

}  // namespace

const char* QueueActionName(int action) {
  const QueueAction* a = Find(action);
  return a ? a->name : "Action";
}

const char* QueueActionIcon(int action) {
  const QueueAction* a = Find(action);
  return a ? a->icon : "ir_action";
}

QueueStripCount QueueStripSlots(int queued) {
  if (queued < 0) queued = 0;
  const int shown = queued < kQueueStripSlots ? queued : kQueueStripSlots;
  return {shown, queued - shown};
}

float QueueExitDrop(float progress, bool panel_under_strip) {
  if (panel_under_strip) return 0.0f;
  const float p = progress < 0.0f ? 0.0f : progress > 1.0f ? 1.0f : progress;
  return kQueueExitDrop * p * (2.0f - p);
}

float QueueTileWidth(int name_chars, int target_chars) {
  const int chars = name_chars > target_chars ? name_chars : target_chars;
  float width = kQueueTilePad + chars * kQueueCharWidth;
  if (width < kQueueTileMin) width = kQueueTileMin;
  if (width > kQueueTileMax) width = kQueueTileMax;
  return width;
}

std::vector<std::string> WrapQueueText(const std::string& text, int line_chars, int max_lines) {
  std::vector<std::string> words;
  std::string word;
  for (char c : text) {
    if (c == ' ') {
      if (!word.empty()) words.push_back(word);
      word.clear();
    } else {
      word += c;
    }
  }
  if (!word.empty()) words.push_back(word);
  auto join = [&](size_t from, size_t to) {
    std::string out;
    for (size_t i = from; i < to; ++i) out += (i > from ? " " : "") + words[i];
    return out;
  };
  if (words.empty()) return {};
  const std::string whole = join(0, words.size());
  if (static_cast<int>(whole.size()) <= line_chars || max_lines <= 1 || words.size() == 1) return {whole};
  if (max_lines == 2) {
    // Balanced: the split whose longer line is the shortest ("Potion of / Cure Wounds").
    size_t best = 1;
    size_t best_cost = whole.size();
    for (size_t k = 1; k < words.size(); ++k) {
      const size_t cost = (std::max)(join(0, k).size(), join(k, words.size()).size());
      if (cost < best_cost) {
        best_cost = cost;
        best = k;
      }
    }
    return {join(0, best), join(best, words.size())};
  }
  std::vector<std::string> lines;  // more lines: greedy, the last takes the rest
  for (const std::string& w : words) {
    const bool last_line = static_cast<int>(lines.size()) >= max_lines;
    if (!lines.empty() && (last_line || static_cast<int>(lines.back().size() + 1 + w.size()) <= line_chars)) {
      lines.back() += " " + w;
    } else {
      lines.push_back(w);
    }
  }
  return lines;
}

std::vector<float> QueueTileXs(const std::vector<float>& widths, float gui_width, int* fit) {
  const int count = static_cast<int>(widths.size());
  int shown = count;
  auto total = [&](int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += widths[i] + (i ? kQueueTileGap : 0.0f);
    return sum;
  };
  // Drop from the right until they fit (with the "+N"'s room when any are dropped).
  while (shown > 1 && total(shown) + (shown < count ? kQueueMoreWidth : 0.0f) > gui_width) --shown;
  if (fit) *fit = shown;
  std::vector<float> xs;
  float x = (gui_width - total(shown)) / 2.0f;
  if (x < 0.0f) x = 0.0f;
  for (int i = 0; i < shown; ++i) {
    xs.push_back(x);
    x += widths[i] + kQueueTileGap;
  }
  return xs;
}

int ItemPowerUsesLeft(int number_of_uses, int stack_size, int charges, int uses_left_today) {
  if (number_of_uses == 1) return stack_size > 0 ? stack_size : 0;  // single use
  if (number_of_uses >= 2 && number_of_uses <= 6) {                  // 5 / 4 / 3 / 2 / 1 charges per use
    const int per_use = 7 - number_of_uses;
    return charges > 0 ? charges / per_use : 0;
  }
  if (number_of_uses >= 8 && number_of_uses <= 12) return uses_left_today > 0 ? uses_left_today : 0;  // per day
  return -1;  // 0 charges per use, unlimited, unknown
}

float QueueStripWidth(int shown, int more) {
  constexpr float kFrame = 28.0f;  // the window's own padding and the group's
  return kFrame + shown * kQueueSlotWidth + (more > 0 ? kQueueMoreWidth : 0.0f);
}

}  // namespace rules
