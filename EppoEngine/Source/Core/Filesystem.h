#pragma once

#include "Core/Buffer.h"

#include <filesystem>

namespace Eppo
{
	class Filesystem
	{
	public:
		static void Init();
		static void Shutdown();

		static const std::filesystem::path& GetAssetsDirectory();

		static bool Exists(const std::filesystem::path& path);

		static Buffer ReadBytes(const std::filesystem::path& filepath);
		static void WriteBytes(const std::filesystem::path& filepath, Buffer buffer, bool overwrite = true);
		// Specifically used for shaders which are 4 byte aligned
		static void WriteBytes(const std::filesystem::path& filepath, const std::vector<uint32_t>& buffer, bool overwrite = true);
	};
}
