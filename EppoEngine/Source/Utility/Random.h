#pragma once

#include <random>

namespace Eppo::Utility
{
	static std::random_device s_RandomDevice;
	static std::mt19937 s_Engine32(s_RandomDevice());
	static std::mt19937_64 s_Engine64(s_RandomDevice());

	// Generate a random 32 bit unsigned integer
	inline uint32_t GenerateRandomUInt32(uint32_t min = 0, uint32_t max = UINT32_MAX)
	{
		std::uniform_int_distribution<uint32_t> uniformDistribution(min, max);
		return uniformDistribution(s_Engine32);
	}

	// Generate a random 32 bit signed integer
	inline int32_t GenerateRandomInt32(int32_t min = INT32_MIN, int32_t max = INT32_MAX)
	{
		std::uniform_int_distribution<int32_t> uniformDistribution(min, max);
		return uniformDistribution(s_Engine32);
	}

	// Generate a random 64 bit unsigned integer
	inline uint64_t GenerateRandomUInt64(uint64_t min = 0, uint64_t max = UINT64_MAX)
	{
		std::uniform_int_distribution<uint64_t> uniformDistribution(min, max);
		return uniformDistribution(s_Engine64);
	}

	// Generate a random 64 bit signed integer
	inline int64_t GenerateRandomInt64(int64_t min = INT64_MIN, int64_t max = INT64_MAX)
	{
		std::uniform_int_distribution<int64_t> uniformDistribution(min, max);
		return uniformDistribution(s_Engine64);
	}
}
