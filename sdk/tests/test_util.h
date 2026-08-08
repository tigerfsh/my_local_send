#pragma once

#include <cstdio>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                      \
  do {                                                                   \
    ++g_checks;                                                          \
    if (!(cond)) {                                                       \
      ++g_failures;                                                      \
      std::printf("CHECK failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    }                                                                    \
  } while (0)

#define CHECK_EQ(a, b)                                                        \
  do {                                                                        \
    ++g_checks;                                                               \
    if (!((a) == (b))) {                                                      \
      ++g_failures;                                                           \
      std::printf("CHECK_EQ failed: %s == %s at %s:%d\n", #a, #b, __FILE__, __LINE__); \
    }                                                                         \
  } while (0)

#define TEST_MAIN(fn)                                                          \
  int main() {                                                                 \
    fn();                                                                      \
    if (g_failures) {                                                          \
      std::printf("FAILED: %d/%d checks failed\n", g_failures, g_checks);      \
      return 1;                                                                \
    }                                                                          \
    std::printf("OK: %d checks passed\n", g_checks);                           \
    return 0;                                                                  \
  }
