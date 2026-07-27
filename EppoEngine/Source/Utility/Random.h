#pragma once

#include <random>

namespace Eppo::Utils
{
    namespace
    {
        std::random_device s_RandomDevice;
        std::mt19937_64 s_Engine64(s_RandomDevice());
    }

    inline auto GenerateRandomInt64(const int64_t min = INT64_MIN, int64_t max = INT64_MAX) -> int64_t
    {
        std::uniform_int_distribution uniformDistribution(min, max);
        return uniformDistribution(s_Engine64);
    }

    inline auto GenerateRandomUInt64(const uint64_t min = 0, uint64_t max = UINT64_MAX) -> uint64_t
    {
        std::uniform_int_distribution uniformDistribution(min, max);
        return uniformDistribution(s_Engine64);
    }
}