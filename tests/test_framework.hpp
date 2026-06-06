#pragma once
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int run_all_tests() {
    int passed = 0, failed = 0;
    for (auto& tc : test_registry()) {
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << '\n';
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << tc.name << " : " << e.what() << '\n';
            ++failed;
        } catch (...) {
            std::cout << "[FAIL] " << tc.name << " : unknown exception\n";
            ++failed;
        }
    }
    std::cout << '\n' << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}

#define TEST(name)                                                       \
    static void test_##name();                                           \
    static struct _Reg_##name {                                          \
        _Reg_##name() {                                                  \
            test_registry().push_back({#name, test_##name});             \
        }                                                                \
    } _reg_##name;                                                       \
    static void test_##name()

#define REQUIRE(expr)                                                    \
    do {                                                                 \
        if (!(expr))                                                     \
            throw std::runtime_error("REQUIRE failed: " #expr           \
                " at line " + std::to_string(__LINE__));                 \
    } while (false)

#define REQUIRE_EQ(a, b)                                                 \
    do {                                                                 \
        auto _a = (a); auto _b = (b);                                   \
        if (!(_a == _b))                                                 \
            throw std::runtime_error(                                    \
                std::string("REQUIRE_EQ failed: ")                      \
                + std::to_string(_a) + " != " + std::to_string(_b)      \
                + " at line " + std::to_string(__LINE__));               \
    } while (false)

#define REQUIRE_STR_EQ(a, b)                                             \
    do {                                                                 \
        std::string _a = (a), _b = (b);                                 \
        if (_a != _b)                                                    \
            throw std::runtime_error(                                    \
                "REQUIRE_STR_EQ failed: \"" + _a + "\" != \"" + _b      \
                + "\" at line " + std::to_string(__LINE__));             \
    } while (false)

#define REQUIRE_THROWS(expr)                                             \
    do {                                                                 \
        bool threw = false;                                              \
        try { (void)(expr); } catch (...) { threw = true; }             \
        if (!threw)                                                      \
            throw std::runtime_error(                                    \
                "REQUIRE_THROWS: expression did not throw at line "      \
                + std::to_string(__LINE__));                             \
    } while (false)
