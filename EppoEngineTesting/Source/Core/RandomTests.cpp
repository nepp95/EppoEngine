#include "Support/EppoTest.h"

#include "Utility/Random.h"

#include <unordered_set>

using namespace Eppo;

SUITE(Core)
{
    TEST(RandomUInt64RespectsBounds)
    {
        constexpr uint64_t min = 100;
        constexpr uint64_t max = 200;

        for (int i = 0; i < 10000; ++i)
        {
            const uint64_t value = Utils::GenerateRandomUInt64(min, max);
            CHECK(value >= min);
            CHECK(value <= max);
        }
    }

    TEST(RandomInt64RespectsBounds)
    {
        constexpr int64_t min = -50;
        constexpr int64_t max = 50;

        for (int i = 0; i < 10000; ++i)
        {
            const int64_t value = Utils::GenerateRandomInt64(min, max);
            CHECK(value >= min);
            CHECK(value <= max);
        }
    }

    TEST(RandomUInt64SingletonRangeIsExact)
    {
        // A degenerate [n, n] range must yield exactly n, not wrap or throw.
        CHECK_EQUAL(500u, Utils::GenerateRandomUInt64(500, 500));
    }

    TEST(RandomDefaultRangeIsNotConstant)
    {
        // A stuck or unseeded generator returns the same value every call. Over
        // many full-range draws we must observe more than one distinct result;
        // collapsing to a single value signals a misconfigured distribution.
        std::unordered_set<uint64_t> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(Utils::GenerateRandomUInt64());

        CHECK(seen.size() > 1);
    }
}
