#pragma once
// Tiny dependency-free test harness. Tests self-register; run with an optional
// substring filter: atmos_tests [filter]

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace atmos::test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

} // namespace atmos::test

#define ATMOS_CAT2(a, b) a##b
#define ATMOS_CAT(a, b) ATMOS_CAT2(a, b)
#define TEST_CASE(name)                                                                   \
    static void ATMOS_CAT(test_fn_, __LINE__)();                                          \
    static ::atmos::test::Registrar ATMOS_CAT(test_reg_, __LINE__)(name, ATMOS_CAT(test_fn_, __LINE__)); \
    static void ATMOS_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++::atmos::test::failures();                                                  \
        }                                                                                 \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                             \
    do {                                                                                  \
        const double va_ = (a), vb_ = (b);                                                \
        if (!(std::fabs(va_ - vb_) <= (tol))) {                                           \
            std::printf("    FAIL %s:%d: %s = %g, expected %g (tol %g)\n", __FILE__, __LINE__, #a, va_, vb_, (double)(tol)); \
            ++::atmos::test::failures();                                                  \
        }                                                                                 \
    } while (0)

#define REQUIRE(cond)                                                                     \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("    FAIL (fatal) %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++::atmos::test::failures();                                                  \
            return;                                                                       \
        }                                                                                 \
    } while (0)
