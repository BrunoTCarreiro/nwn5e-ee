#include "rules/target_rows.h"

#include <algorithm>
#include <iterator>

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

int TakeBackTarget(TargetRows* rows, uint32_t id) {
  if (!rows) return -1;  // (`picked` lists filled rows only: an id 0 matches none)
  for (auto it = rows->picked.rbegin(); it != rows->picked.rend(); ++it) {
    const int row = *it;
    if (rows->rows[static_cast<size_t>(row)] != id) continue;
    rows->rows[static_cast<size_t>(row)] = 0;
    rows->picked.erase(std::next(it).base());
    return row;
  }
  return -1;
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

std::vector<TargetCount> CountsByTarget(const TargetRows& rows) {
  std::vector<TargetCount> counts;
  for (uint32_t id : rows.rows) {
    if (id == 0) continue;  // not assigned: the cast gives it the latest pick, the count doesn't
    bool found = false;
    for (TargetCount& one : counts) {
      if (one.id == id) {
        ++one.count;
        found = true;
        break;
      }
    }
    if (!found) counts.push_back(TargetCount{id, 1});
  }
  return counts;
}

namespace {

const TargetCount* FindCount(const std::vector<TargetCount>& counts, uint32_t id) {
  for (const TargetCount& one : counts) {
    if (one.id == id) return &one;
  }
  return nullptr;
}

}  // namespace

CountMarkerChanges CountMarkerChangesFor(const std::vector<TargetCount>& shown, const std::vector<TargetCount>& now) {
  CountMarkerChanges changes;
  for (const TargetCount& old : shown) {
    const TargetCount* still = FindCount(now, old.id);
    if (!still || still->count != old.count) changes.remove.push_back(old.id);
  }
  for (const TargetCount& one : now) {
    const TargetCount* was = FindCount(shown, one.id);
    if (!was || was->count != one.count) changes.apply.push_back(one);
  }
  return changes;
}

}  // namespace rules
