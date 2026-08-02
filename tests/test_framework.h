/**
 * test_framework.h - Minimal test harness.
 *
 * Deliberately dependency-free: the project ships no package manager and
 * vendoring a full framework for a handful of protocol assertions would be more
 * build surface than it is worth.
 *
 *   TEST(suite, name) { CHECK(x == y); }
 *
 * A failing CHECK records the failure and continues; REQUIRE aborts the current
 * test. main() is provided by test_main.cpp.
 */

#pragma once
#include <cstdio>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

/// Thrown by REQUIRE to abort the current test only.
struct AbortTest {};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& CurrentFailures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> fn) {
        Registry().push_back({suite, name, std::move(fn)});
    }
};

inline void ReportFailure(const char* file, int line, const char* expr,
                          const std::string& detail) {
    // Leading newline closes the "  <name> ... " line printed by the runner.
    ++CurrentFailures();
    std::printf("\n    FAIL %s:%d\n      %s\n", file, line, expr);
    if (!detail.empty()) std::printf("      %s\n", detail.c_str());
}

/// Renders a byte range as hex, for readable diffs on protocol assertions.
inline std::string HexDump(const uint8_t* data, size_t len) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < len; ++i) {
        if (i) out += ' ';
        out += kHex[data[i] >> 4];
        out += kHex[data[i] & 0x0F];
    }
    return out;
}

/// Compares two byte ranges, reporting the first differing offset.
inline bool BytesEqual(const uint8_t* actual, const uint8_t* expected, size_t len,
                       std::string& detail) {
    for (size_t i = 0; i < len; ++i) {
        if (actual[i] != expected[i]) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "first difference at offset %zu: actual 0x%02X, expected 0x%02X",
                          i, actual[i], expected[i]);
            detail = buf;
            detail += "\n      actual:   " + HexDump(actual, len);
            detail += "\n      expected: " + HexDump(expected, len);
            return false;
        }
    }
    return true;
}

int RunAll();

}  // namespace testing

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                      \
    static void TEST_CONCAT(test_body_, __LINE__)();                           \
    static ::testing::Registrar TEST_CONCAT(test_reg_, __LINE__)(              \
        #suite, #name, TEST_CONCAT(test_body_, __LINE__));                     \
    static void TEST_CONCAT(test_body_, __LINE__)()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) ::testing::ReportFailure(__FILE__, __LINE__, #expr, "");   \
    } while (0)

#define CHECK_MSG(expr, detail)                                                \
    do {                                                                       \
        if (!(expr))                                                           \
            ::testing::ReportFailure(__FILE__, __LINE__, #expr, (detail));      \
    } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        auto _a = (actual);                                                    \
        auto _e = (expected);                                                  \
        if (!(_a == _e)) {                                                     \
            std::string _d = "actual: " + std::to_string(_a) +                 \
                             ", expected: " + std::to_string(_e);              \
            ::testing::ReportFailure(__FILE__, __LINE__,                       \
                                     #actual " == " #expected, _d);            \
        }                                                                      \
    } while (0)

#define CHECK_BYTES(actual, expected, len)                                     \
    do {                                                                       \
        std::string _d;                                                        \
        if (!::testing::BytesEqual((actual), (expected), (len), _d))           \
            ::testing::ReportFailure(__FILE__, __LINE__,                       \
                                     "bytes " #actual " == " #expected, _d);   \
    } while (0)

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) {                                                         \
            ::testing::ReportFailure(__FILE__, __LINE__, #expr,                \
                                     "(required - aborting this test)");       \
            throw ::testing::AbortTest{};                                      \
        }                                                                      \
    } while (0)
