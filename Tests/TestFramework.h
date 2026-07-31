#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

/**
 * Minimal, dependency-free unit test harness. The DSP core (Source/DSP,
 * Source/Utility) is deliberately kept free of JUCE includes so it can
 * be tested with a plain C++20 compiler - no framework download required.
 */
namespace shifterfx::test
{
    struct TestCase
    {
        std::string name;
        std::function<void()> fn;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> instance;
        return instance;
    }

    struct Registrar
    {
        Registrar(std::string name, std::function<void()> fn)
        {
            registry().push_back({ std::move(name), std::move(fn) });
        }
    };

    inline int failureCount = 0;
}

#define SHIFTERFX_CONCAT_(a, b) a##b
#define SHIFTERFX_CONCAT(a, b) SHIFTERFX_CONCAT_(a, b)

#define TEST_CASE(name)                                                                                              \
    static void SHIFTERFX_CONCAT(shifterfx_test_fn_, __LINE__)();                                                    \
    static const shifterfx::test::Registrar SHIFTERFX_CONCAT(shifterfx_test_reg_, __LINE__)(                         \
        name, SHIFTERFX_CONCAT(shifterfx_test_fn_, __LINE__));                                                        \
    static void SHIFTERFX_CONCAT(shifterfx_test_fn_, __LINE__)()

#define EXPECT_TRUE(cond)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            std::cerr << "    FAIL: " << #cond << "  (" << __FILE__ << ":" << __LINE__ << ")\n";                       \
            ++shifterfx::test::failureCount;                                                                           \
        }                                                                                                               \
    } while (false)

#define EXPECT_NEAR(a, b, eps)                                                                                         \
    do                                                                                                                  \
    {                                                                                                                   \
        const auto shifterfx_a_ = (a);                                                                                 \
        const auto shifterfx_b_ = (b);                                                                                  \
        if (std::abs(static_cast<double>(shifterfx_a_) - static_cast<double>(shifterfx_b_)) > (eps))                    \
        {                                                                                                               \
            std::cerr << "    FAIL: " << #a << " ~= " << #b << "  (got " << shifterfx_a_ << ", expected "               \
                       << shifterfx_b_ << ")  (" << __FILE__ << ":" << __LINE__ << ")\n";                                \
            ++shifterfx::test::failureCount;                                                                            \
        }                                                                                                               \
    } while (false)
