#pragma once

// Remembers which decisions were already written to nwn5e.log, so each
// distinct one (e.g. "this creature: 3e +3 -> 5e +4") appears once, and again
// only when its numbers change. A fixed cap bounds memory and file size.

#include <windows.h>

#include <stdint.h>

#include <unordered_set>

class LogOnce {
 public:
  explicit LogOnce(size_t max_entries) : max_entries_(max_entries) {}

  // True the first time `key` is seen (and while under the cap).
  bool First(uint64_t key) {
    AcquireSRWLockExclusive(&lock_);
    bool first = false;
    if (seen_.size() < max_entries_) first = seen_.insert(key).second;
    ReleaseSRWLockExclusive(&lock_);
    return first;
  }

  // Packs an object id and two small numbers into one key.
  static uint64_t Key(uint32_t object_id, int a, int b) {
    return (static_cast<uint64_t>(object_id) << 32) | (static_cast<uint64_t>(a & 0xFFFF) << 16) |
           static_cast<uint64_t>(b & 0xFFFF);
  }

 private:
  SRWLOCK lock_ = SRWLOCK_INIT;
  std::unordered_set<uint64_t> seen_;
  size_t max_entries_;
};
