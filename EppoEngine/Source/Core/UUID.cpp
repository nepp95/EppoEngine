#include "pch.h"
#include "UUID.h"

#include "Utility/Random.h"

namespace Eppo
{
	UUID::UUID()
		: m_UUID(Utility::GenerateRandomUInt64())
	{}

	UUID::UUID(const uint64_t uuid)
		: m_UUID(uuid)
	{}
}
