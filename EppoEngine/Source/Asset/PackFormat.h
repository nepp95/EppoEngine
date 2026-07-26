#pragma once

#include <cstdint>

namespace Eppo::PackFormat
{
	struct FormatId
	{
		uint32_t Magic = 0;
		uint32_t Version = 0;
	};

	// Four-character code laid out little-endian, so 'E','P','A','K' reads as "EPAK" on disk.
	constexpr auto MakeMagic(const char a, const char b, const char c, const char d) -> uint32_t
	{
		return static_cast<uint32_t>(a) | static_cast<uint32_t>(b) << 8 | static_cast<uint32_t>(c) << 16 | static_cast<uint32_t>(d) << 24;
	}

	inline constexpr FormatId Package{ .Magic = MakeMagic('E', 'P', 'A', 'K'), .Version = 1 };

    // Global payloads
    inline constexpr FormatId Shader{ .Magic = MakeMagic('E', 'S', 'H', 'D'), .Version = 1 };

	// Per-asset payloads
    inline constexpr FormatId Mesh{ .Magic = MakeMagic('E', 'M', 'S', 'H'), .Version = 1 };
	inline constexpr FormatId Scene{ .Magic = MakeMagic('E', 'S', 'C', 'N'), .Version = 1 };
}
