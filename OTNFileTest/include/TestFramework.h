#pragma once
#include <iostream>
#include <string>
#include <filesystem>

// ============================================================
//  Global counters
// ============================================================

inline int g_passed = 0;
inline int g_failed = 0;

// ============================================================
//  Macros
// ============================================================

#define CHECK(cond) do { \
    if (cond) { \
        ++g_passed; \
    } else { \
        std::cerr << "  [FAIL] " << #cond \
                  << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        ++g_failed; \
    } \
} while(0)

#define CHECK_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { \
        ++g_passed; \
    } else { \
        std::cerr << "  [FAIL] " << #a << " == " << #b \
                  << "  (got: " << _a << " vs " << _b << ")" \
                  << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        ++g_failed; \
    } \
} while(0)

#define RUN(fn) do { \
    std::cout << "[TEST] " << #fn << "\n"; \
    fn(basePath); \
} while(0)

// ============================================================
//  Summary
// ============================================================

inline void PrintSummary()
{
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Results: " << g_passed << " passed, "
              << g_failed << " failed\n";
}
