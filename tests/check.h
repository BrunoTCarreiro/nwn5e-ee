#pragma once

// Minimal test harness: TEST(name) { CHECK_EQ(actual, expected); }
// rules_main.cpp runs every registered test and exits non-zero on failure.

#include <stdio.h>

#include <vector>

namespace check {

struct TestCase {
  const char* name;
  void (*body)();
};

inline std::vector<TestCase>& Registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& FailureCount() {
  static int failures = 0;
  return failures;
}

struct Registrar {
  Registrar(const char* name, void (*body)()) { Registry().push_back({name, body}); }
};

inline void Fail(const char* file, int line, const char* expression, long long actual, long long expected) {
  ++FailureCount();
  printf("    FAIL %s:%d  %s  -> got %lld, want %lld\n", file, line, expression, actual, expected);
}

inline void FailText(const char* file, int line, const char* message) {
  ++FailureCount();
  printf("    FAIL %s:%d  %s\n", file, line, message);
}

inline int RunAll() {
  int failed_tests = 0;
  for (const TestCase& test : Registry()) {
    const int before = FailureCount();
    test.body();
    const bool ok = FailureCount() == before;
    if (!ok) ++failed_tests;
    printf("%s %s\n", ok ? "ok  " : "FAIL", test.name);
  }
  printf("\n%d tests, %d failed\n", static_cast<int>(Registry().size()), failed_tests);
  return failed_tests == 0 ? 0 : 1;
}

}  // namespace check

#define TEST(name)                                         \
  static void name();                                      \
  static check::Registrar name##_registrar(#name, &name); \
  static void name()

#define CHECK_EQ(actual, expected)                                                              \
  do {                                                                                          \
    const auto check_actual_ = (actual);                                                        \
    const auto check_expected_ = (expected);                                                    \
    if (!(check_actual_ == check_expected_)) {                                                  \
      check::Fail(__FILE__, __LINE__, #actual, static_cast<long long>(check_actual_),           \
                  static_cast<long long>(check_expected_));                                     \
    }                                                                                           \
  } while (0)
