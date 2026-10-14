#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace TestHarness {

enum class Tier {
    TIER_1_FEATURE = 1,
    TIER_2_BOUNDARY = 2,
    TIER_3_COMBINATIONS = 3,
    TIER_4_REAL_WORLD = 4
};

struct TestCaseInfo {
    std::string name;
    Tier tier;
    std::function<void()> func;
    bool passed = false;
    std::string failureMessage;
    double durationMs = 0.0;
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner s_runner;
        return s_runner;
    }

    void registerTest(const std::string& name, Tier tier, std::function<void()> func) {
        m_tests.push_back({name, tier, func, false, "", 0.0});
    }

    int runAll(const std::string& suiteName) {
        std::cout << "\n======================================================================\n";
        std::cout << "  RUNNING SUITE: " << suiteName << "\n";
        std::cout << "======================================================================\n";

        size_t t1_pass = 0, t1_fail = 0;
        size_t t2_pass = 0, t2_fail = 0;
        size_t t3_pass = 0, t3_fail = 0;
        size_t t4_pass = 0, t4_fail = 0;

        for (auto& t : m_tests) {
            m_currentTestFailed = false;
            m_currentFailureMsg.clear();

            auto start = std::chrono::high_resolution_clock::now();
            try {
                t.func();
                t.passed = !m_currentTestFailed;
                t.failureMessage = m_currentFailureMsg;
            } catch (const std::exception& e) {
                t.passed = false;
                t.failureMessage = std::string("Unhandled exception: ") + e.what();
            } catch (...) {
                t.passed = false;
                t.failureMessage = "Unknown non-std exception thrown";
            }
            auto end = std::chrono::high_resolution_clock::now();
            t.durationMs = std::chrono::duration<double, std::milli>(end - start).count();

            std::string tierTag;
            switch (t.tier) {
                case Tier::TIER_1_FEATURE:      tierTag = "[Tier 1: Feature]    "; break;
                case Tier::TIER_2_BOUNDARY:     tierTag = "[Tier 2: Boundary]   "; break;
                case Tier::TIER_3_COMBINATIONS: tierTag = "[Tier 3: Combination]"; break;
                case Tier::TIER_4_REAL_WORLD:   tierTag = "[Tier 4: Real-World] "; break;
            }

            if (t.passed) {
                switch (t.tier) {
                    case Tier::TIER_1_FEATURE:      t1_pass++; break;
                    case Tier::TIER_2_BOUNDARY:     t2_pass++; break;
                    case Tier::TIER_3_COMBINATIONS: t3_pass++; break;
                    case Tier::TIER_4_REAL_WORLD:   t4_pass++; break;
                }
                std::cout << "  ✓ " << tierTag << " " << t.name 
                          << " (" << std::fixed << std::setprecision(2) << t.durationMs << " ms)\n";
            } else {
                switch (t.tier) {
                    case Tier::TIER_1_FEATURE:      t1_fail++; break;
                    case Tier::TIER_2_BOUNDARY:     t2_fail++; break;
                    case Tier::TIER_3_COMBINATIONS: t3_fail++; break;
                    case Tier::TIER_4_REAL_WORLD:   t4_fail++; break;
                }
                std::cout << "  ✗ " << tierTag << " " << t.name << " FAILED!\n";
                std::cout << "      --> " << t.failureMessage << "\n";
            }
        }

        size_t totalPass = t1_pass + t2_pass + t3_pass + t4_pass;
        size_t totalFail = t1_fail + t2_fail + t3_fail + t4_fail;

        std::cout << "\n----------------------------------------------------------------------\n";
        std::cout << "  SUITE SUMMARY: " << suiteName << "\n";
        std::cout << "  Tier 1 (Feature)     : " << t1_pass << " passed, " << t1_fail << " failed\n";
        std::cout << "  Tier 2 (Boundary)    : " << t2_pass << " passed, " << t2_fail << " failed\n";
        std::cout << "  Tier 3 (Combination) : " << t3_pass << " passed, " << t3_fail << " failed\n";
        std::cout << "  Tier 4 (Real-World)  : " << t4_pass << " passed, " << t4_fail << " failed\n";
        std::cout << "  TOTAL                : " << totalPass << " passed, " << totalFail << " failed (Total " << m_tests.size() << " tests)\n";
        std::cout << "======================================================================\n";

        return (totalFail == 0) ? 0 : 1;
    }

    void setFailure(const std::string& msg, const char* file, int line) {
        m_currentTestFailed = true;
        std::ostringstream ss;
        ss << file << ":" << line << " - " << msg;
        m_currentFailureMsg = ss.str();
    }

    bool hasCurrentTestFailed() const { return m_currentTestFailed; }

    void clear() { m_tests.clear(); }

    const std::vector<TestCaseInfo>& getTests() const { return m_tests; }

private:
    TestRunner() = default;
    std::vector<TestCaseInfo> m_tests;
    bool m_currentTestFailed = false;
    std::string m_currentFailureMsg;
};

struct TestRegistrar {
    TestRegistrar(const std::string& name, Tier tier, std::function<void()> func) {
        TestRunner::instance().registerTest(name, tier, func);
    }
};

} // namespace TestHarness

#define TEST_CONCAT_INNER(a, b) a ## b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define REGISTER_TEST(tier_enum, test_name) \
    static void test_name(); \
    static ::TestHarness::TestRegistrar TEST_CONCAT(reg_, __LINE__)(#test_name, tier_enum, test_name); \
    static void test_name()

#define TEST_TIER1(name) REGISTER_TEST(::TestHarness::Tier::TIER_1_FEATURE, name)
#define TEST_TIER2(name) REGISTER_TEST(::TestHarness::Tier::TIER_2_BOUNDARY, name)
#define TEST_TIER3(name) REGISTER_TEST(::TestHarness::Tier::TIER_3_COMBINATIONS, name)
#define TEST_TIER4(name) REGISTER_TEST(::TestHarness::Tier::TIER_4_REAL_WORLD, name)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            ::TestHarness::TestRunner::instance().setFailure("Assertion failed: " #cond " is false", __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(cond) \
    do { \
        if (cond) { \
            ::TestHarness::TestRunner::instance().setFailure("Assertion failed: " #cond " is true", __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(actual, expected) \
    do { \
        auto a_val = (actual); \
        auto e_val = (expected); \
        if (a_val != e_val) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #actual " (" << a_val << ") == " #expected " (" << e_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(actual, expected) \
    do { \
        auto a_val = (actual); \
        auto e_val = (expected); \
        if (a_val == e_val) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #actual " (" << a_val << ") != " #expected " (" << e_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_LT(a, b) \
    do { \
        auto a_val = (a); \
        auto b_val = (b); \
        if (!(a_val < b_val)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " (" << a_val << ") < " #b " (" << b_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_LE(a, b) \
    do { \
        auto a_val = (a); \
        auto b_val = (b); \
        if (!(a_val <= b_val)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " (" << a_val << ") <= " #b " (" << b_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_GT(a, b) \
    do { \
        auto a_val = (a); \
        auto b_val = (b); \
        if (!(a_val > b_val)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " (" << a_val << ") > " #b " (" << b_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        auto a_val = (a); \
        auto b_val = (b); \
        if (!(a_val >= b_val)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " (" << a_val << ") >= " #b " (" << b_val << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NEAR(actual, expected, tol) \
    do { \
        auto a_val = (actual); \
        auto e_val = (expected); \
        auto diff = std::abs(a_val - e_val); \
        if (diff > (tol)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: |" #actual " (" << a_val << ") - " #expected " (" << e_val << ")| = " << diff << " > tolerance (" << (tol) << ")"; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_STR_EQ(actual, expected) \
    do { \
        std::string a_str = (actual); \
        std::string e_str = (expected); \
        if (a_str != e_str) { \
            std::ostringstream ss; \
            ss << "Assertion failed: \"" << a_str << "\" == \"" << e_str << "\""; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { \
        std::string h_str = (haystack); \
        std::string n_str = (needle); \
        if (h_str.find(n_str) == std::string::npos) { \
            std::ostringstream ss; \
            ss << "Assertion failed: \"" << h_str << "\" contains \"" << n_str << "\""; \
            ::TestHarness::TestRunner::instance().setFailure(ss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)
