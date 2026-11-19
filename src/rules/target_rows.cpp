#include "rules/target_rows.h"

#include <algorithm>

#include "rules/spells2024.h"

namespace rules {

int TargetRowsFor(int spell_row, int slot_level) {
  if (spell_row == kMagicMissile) return MagicMissileDarts(std::max(1, slot_level));
  if (spell_row == kScorchingRay) return 3 + std::max(0, slot_level - 2);
  return 0;
}

TargetRows MakeTargetRows(int count, uint32_t first) {
  TargetRows made;
  made.rows.assign(static_cast<size_t>(std::max(1, count)), 0u);
  if (first != 0) {
    made.rows[0] = first;
    made.picked.push_back(0);
  }
  return made;
}

bool AddTarget(TargetRows* rows, uint32_t id) {
  if (!rows || id == 0) return false;
  for (size_t i = 0; i < rows->rows.size(); ++i) {
    if (rows->rows[i] != 0) continue;
    rows->rows[i] = id;
    rows->picked.push_back(static_cast<int>(i));
    return true;
  }
  return false;
}

bool ClearRow(TargetRows* rows, int row) {
  if (!rows || row < 0 || row >= static_cast<int>(rows->rows.size()) || rows->rows[static_cast<size_t>(row)] == 0) {
    return false;
  }
  rows->rows[static_cast<size_t>(row)] = 0;
  rows->picked.erase(std::remove(rows->picked.begin(), rows->picked.end(), row), rows->picked.end());
  return true;
}

int TakeBackLatest(TargetRows* rows) {
  if (!rows || rows->picked.empty()) return -1;
  const int row = rows->picked.back();
  rows->picked.pop_back();
  rows->rows[static_cast<size_t>(row)] = 0;
  return row;
}

bool RestoreRow(TargetRows* rows, int row, uint32_t id) {
  if (!rows || id == 0 || row < 0 || row >= static_cast<int>(rows->rows.size()) ||
      rows->rows[static_cast<size_t>(row)] != 0) {
    return false;
  }
  rows->rows[static_cast<size_t>(row)] = id;
  rows->picked.push_back(row);
  return true;
}

std::vector<uint32_t> ResolveTargets(const TargetRows& rows) {
  if (rows.picked.empty()) return {};
  const uint32_t last = rows.rows[static_cast<size_t>(rows.picked.back())];
  std::vector<uint32_t> targets;
  targets.reserve(rows.rows.size());
  for (uint32_t id : rows.rows) targets.push_back(id != 0 ? id : last);
  return targets;
}

int FilledRows(const TargetRows& rows) { return static_cast<int>(rows.picked.size()); }

}  // namespace rules
