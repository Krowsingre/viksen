#include <cstring>

#include "tests/TestHarness.h"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (const auto& t : atmos::test::registry()) {
        if (filter && !std::strstr(t.name, filter)) continue;
        const int before = atmos::test::failures();
        std::printf("[ RUN  ] %s\n", t.name);
        try {
            t.fn();
        } catch (const std::exception& e) {
            std::printf("    FAIL uncaught exception: %s\n", e.what());
            ++atmos::test::failures();
        }
        const bool ok = atmos::test::failures() == before;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", t.name);
        ++run;
        if (!ok) ++failedTests;
    }
    std::printf("\n%d test(s) run, %d failed\n", run, failedTests);
    return failedTests == 0 && run > 0 ? 0 : 1;
}
