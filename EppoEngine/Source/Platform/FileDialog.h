#pragma once

#include <filesystem>

namespace Eppo
{
	class FileDialog
	{
	public:
		static std::filesystem::path OpenFile(const char* filter, const std::filesystem::path& initialDir);
		static std::filesystem::path SaveFile(const char* filter);
	};
}
