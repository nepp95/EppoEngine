#pragma once

#include "pch.h"

#include <gtest/gtest.h>

namespace Eppo::Testing
{
    // Thrown by EP_REQUIRE to abort the current test from anywhere — including
    // value-returning helper functions, where Google Test's ASSERT_* (it expands to
    // `return;`) does not compile. Google Test reports the unwound exception as a failure.
    struct RequireFailure
    {
    };

    [[noreturn]] inline auto FailRequire() -> void
    {
        throw RequireFailure{};
    }
}

// UnitTest++'s `REQUIRE CHECK` translated 1:1: a fatal check usable in any function.
#define EP_REQUIRE(cond) do { if (!(cond)) { ADD_FAILURE() << "EP_REQUIRE failed: " #cond; ::Eppo::Testing::FailRequire(); } } while (false)
#define EP_REQUIRE_EQ(expected, actual) EP_REQUIRE((expected) == (actual))

// Replacement for UnitTest++'s CHECK_ARRAY_EQUAL; reports the first index that differs.
#define EP_EXPECT_ARRAY_EQ(expected, actual, count)                                                                                        \
    for (size_t epArrayIndex = 0; epArrayIndex < static_cast<size_t>(count); ++epArrayIndex)                                               \
    EXPECT_EQ((expected)[epArrayIndex], (actual)[epArrayIndex]) << "array index " << epArrayIndex
