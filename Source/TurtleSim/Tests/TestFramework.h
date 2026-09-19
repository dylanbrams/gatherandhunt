#pragma once

// Tiny in-tree test framework. No deps. Header-only.

// UBT compiles everything under the module directory, including Tests/. The test
// sources compile to nothing under __UNREAL__; do the same here so the framework's
// try/catch doesn't require exceptions in non-editor targets.
#if !defined(__UNREAL__)

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace turtlesim_test {

struct TestCase {
    const char* Suite;
    const char* Name;
    std::function<void()> Fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> R;
    return R;
}

struct Registrar {
    Registrar(const char* Suite, const char* Name, std::function<void()> Fn) {
        Registry().push_back({Suite, Name, std::move(Fn)});
    }
};

struct AssertionFailure {
    std::string Message;
};

inline void RequireImpl(bool Cond, const char* Expr, const char* File, int Line) {
    if (Cond) return;
    std::ostringstream os;
    os << File << ":" << Line << " REQUIRE(" << Expr << ") failed";
    throw AssertionFailure{os.str()};
}

inline int RunAll() {
    int Pass = 0, Fail = 0;
    for (const auto& T : Registry()) {
        try {
            T.Fn();
            ++Pass;
            std::printf("  ok   %s :: %s\n", T.Suite, T.Name);
        } catch (const AssertionFailure& e) {
            ++Fail;
            std::printf("  FAIL %s :: %s\n       %s\n", T.Suite, T.Name, e.Message.c_str());
        } catch (const std::exception& e) {
            ++Fail;
            std::printf("  FAIL %s :: %s (exception: %s)\n", T.Suite, T.Name, e.what());
        } catch (...) {
            ++Fail;
            std::printf("  FAIL %s :: %s (unknown exception)\n", T.Suite, T.Name);
        }
    }
    std::printf("\n%d passed, %d failed, %d total\n", Pass, Fail, Pass + Fail);
    return Fail == 0 ? 0 : 1;
}

} // namespace turtlesim_test

#define TURTLESIM_CONCAT_INNER(a, b) a##b
#define TURTLESIM_CONCAT(a, b) TURTLESIM_CONCAT_INNER(a, b)

// Single-argument macros to avoid preprocessor comma-splitting on brace-init.
// Use REQUIRE(a == b) instead of REQUIRE_EQ(a, b).
#define TEST_CASE(Suite, Name)                                                                \
    static void TURTLESIM_CONCAT(Suite##_##Name##_fn_, __LINE__)();                            \
    static ::turtlesim_test::Registrar TURTLESIM_CONCAT(Suite##_##Name##_reg_, __LINE__){      \
        #Suite, #Name, &TURTLESIM_CONCAT(Suite##_##Name##_fn_, __LINE__)};                     \
    static void TURTLESIM_CONCAT(Suite##_##Name##_fn_, __LINE__)()

#define REQUIRE(Cond) ::turtlesim_test::RequireImpl((Cond), #Cond, __FILE__, __LINE__)

#endif // !defined(__UNREAL__)
