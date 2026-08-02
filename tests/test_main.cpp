#include "test_framework.h"

namespace testing {

int RunAll() {
    int total = 0;
    int failed_tests = 0;
    std::string current_suite;

    for (const auto& test : Registry()) {
        if (test.suite != current_suite) {
            current_suite = test.suite;
            std::printf("\n[%s]\n", current_suite.c_str());
        }

        const int before = CurrentFailures();
        std::printf("  %s ... ", test.name.c_str());
        std::fflush(stdout);

        try {
            test.fn();
        } catch (const AbortTest&) {
            // REQUIRE already reported the failure.
        } catch (const std::exception& e) {
            ++CurrentFailures();
            std::printf("\n    FAIL unexpected exception: %s\n", e.what());
        } catch (...) {
            ++CurrentFailures();
            std::printf("\n    FAIL unexpected non-standard exception\n");
        }

        ++total;
        if (CurrentFailures() > before) {
            ++failed_tests;
        } else {
            std::printf("ok\n");
        }
    }

    std::printf("\n----------------------------------------\n");
    if (failed_tests == 0) {
        std::printf("PASSED: %d test(s)\n", total);
        return 0;
    }
    std::printf("FAILED: %d of %d test(s), %d assertion(s)\n", failed_tests, total,
                CurrentFailures());
    return 1;
}

}  // namespace testing

int main() {
    return testing::RunAll();
}
