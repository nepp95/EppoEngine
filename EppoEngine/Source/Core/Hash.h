#pragma once

#include "Core/Buffer.h"

namespace Eppo
{
	class Hash
	{
	public:
		static uint64_t GenerateFnv(const std::string& contents);
		static uint64_t GenerateFnv(Buffer buffer);
	};
}
